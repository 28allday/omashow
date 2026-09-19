#include "backend.h"
#include "core/design.h"
#include "core/imageasset.h"
#include "core/imageoptimisation.h"
#include "core/mediatranscode.h"
#include "mediaplayback.h"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <cmath>
namespace {
QString assetKey(const SceneObject &o) {
  return o.type == ObjectType::Media ? o.mediaId : o.imageId;
}
void fitTrim(Slide &slide, SceneObject &o) {
  o.mediaTrimStart = qMin(o.mediaTrimStart, qMax(0.0, o.mediaDuration - .01));
  o.mediaTrimEnd =
      qBound(o.mediaTrimStart + .000001, o.mediaTrimEnd, o.mediaDuration);
  for (auto &step : slide.timeline.steps)
    if (step.targetId == o.id && step.effect == Effect::Media)
      step.duration = MediaAsset::playbackDuration(o);
}
} // namespace
QVariantMap Backend::mediaOptimisation() const {
  return {{"ready", m_mediaPreviewReady},
          {"hasSource", !assetKey(m_mediaPreviewSource).isEmpty()},
          {"isImage", m_imageOptimisation},
          {"sourceId", assetKey(m_mediaPreviewSource)},
          {"previewId", assetKey(m_mediaPreview)},
          {"name", m_mediaPreviewSource.mediaName},
          {"beforeBytes", m_imageOptimisation
                              ? m_mediaPreviewSource.imageData.size()
                              : m_mediaPreviewSource.mediaBytes},
          {"afterBytes", m_imageOptimisation ? m_mediaPreview.imageData.size()
                                             : m_mediaPreview.mediaBytes},
          {"codec", m_imageOptimisation ? m_mediaPreview.imageFormat.toUpper()
                                        : m_mediaPreview.mediaCodec},
          {"width", m_mediaPreview.image.width()},
          {"height", m_mediaPreview.image.height()},
          {"duration", m_mediaPreviewSource.mediaDuration},
          {"time", m_comparisonTime},
          {"playing", m_comparisonPlaying},
          {"originalLinked",
           m_mediaPreviewSource.mediaOriginal
               ? !m_mediaPreviewSource.mediaOriginal->mediaPath.isEmpty()
               : !m_mediaPreviewSource.mediaPath.isEmpty()}};
}
bool Backend::previewMediaOptimisation(int preset) {
  const auto *source = selectedObject();
  if (busy() || !source ||
      (source->type != ObjectType::Media &&
       (source->image.isNull() || source->imageFormat == "svg")) ||
      source->locked || selectionCount() != 1)
    return false;
  pause();
  discardMediaOptimisation();
  m_imageOptimisation = source->type != ObjectType::Media;
  m_mediaPreviewSource = *source;
  m_mediaPreviewSource.mediaReadAllowed =
      m_mediaPermissions.value(source->mediaPath) == source->mediaId;
  if (!source->mediaPath.isEmpty() &&
      MediaAsset::linkState(m_mediaPreviewSource,
                            m_mediaPreviewSource.mediaReadAllowed) !=
          QStringLiteral("Linked · keep source file")) {
    emit failed(tr("Approve or relink this source in Media preflight first."));
    return false;
  }
  m_mediaPreviewSlide = m_document.slides.at(m_currentSlide).id;
  m_mediaPreviewGeneration = m_documentGeneration;
  const auto input = m_mediaPreviewSource;
  const int generation = m_documentGeneration;
  const auto job = std::make_shared<MediaAsset::Job>();
  m_mediaJob = job;
  m_mediaOptimising = true;
  m_mediaJobLabel = tr("Preparing compression preview");
  setBusy(true);
  m_mediaProgressTimer.start();
  emit mediaJobChanged();
  emit mediaOptimisationChanged();
  auto *watcher = new QFutureWatcher<MediaAsset::Result>(this);
  connect(watcher, &QFutureWatcher<MediaAsset::Result>::finished, this,
          [this, watcher, job, generation, input] {
            const auto result = watcher->result();
            watcher->deleteLater();
            m_mediaProgressTimer.stop();
            m_mediaJob.reset();
            m_mediaOptimising = false;
            setBusy(false);
            emit mediaJobChanged();
            if (job->canceled || generation != m_documentGeneration ||
                m_mediaPreviewGeneration != generation ||
                assetKey(m_mediaPreviewSource) != assetKey(input))
              return;
            if (!result.ok()) {
              emit failed(result.error);
              return;
            }
            m_mediaPreview = result.object;
            m_mediaPreviewReady = true;
            emit mediaOptimisationChanged();
          });
  watcher->setFuture(
      QtConcurrent::run([input, preset, job, picture = m_imageOptimisation] {
        return picture ? ImageOptimisation::preview(input, preset, job)
                       : MediaTranscode::preview(input, preset, job);
      }));
  return true;
}
bool Backend::applyMediaOptimisation(bool keepOriginal) {
  const auto *source = selectedObject();
  if (!m_mediaPreviewReady || busy() || !source || source->locked ||
      source->id != m_mediaPreviewSource.id ||
      assetKey(*source) != assetKey(m_mediaPreviewSource) ||
      m_mediaPreviewGeneration != m_documentGeneration ||
      m_document.slides.at(m_currentSlide).id != m_mediaPreviewSlide)
    return false;
  auto candidate = m_document;
  auto &slide = candidate.slides[m_currentSlide];
  auto *o = slide.find(source->id);
  if (m_imageOptimisation) {
    auto original = *source;
    original.type = ObjectType::Image;
    original.imageOriginal.reset();
    original.mediaOriginal.reset();
    const auto retained = source->imageOriginal
                              ? source->imageOriginal
                              : std::make_shared<const SceneObject>(original);
    ImageAsset::copyData(*o, m_mediaPreview);
    Design::markOverride(*o, "imageId");
    o->imageOriginal = keepOriginal ? retained : nullptr;
  } else {
    const auto original = source->mediaOriginal
                              ? source->mediaOriginal
                              : std::make_shared<const SceneObject>(*source);
    MediaAsset::copySource(*o, m_mediaPreview);
    o->mediaOriginal = keepOriginal ? original : nullptr;
    fitTrim(slide, *o);
  }
  if (MediaAsset::embeddedBytes(candidate) > 480LL * 1024 * 1024) {
    emit failed(tr("This preview would exceed the deck's asset limit."));
    return false;
  }
  pause();
  m_history.begin(m_document, m_imageOptimisation ? tr("Optimise picture")
                                                  : tr("Optimise media"));
  m_document = candidate;
  m_history.commit();
  discardMediaOptimisation();
  touch();
  return true;
}
void Backend::discardMediaOptimisation() {
  if (m_mediaOptimising)
    cancelMediaJob();
  m_comparisonTicker.stop();
  m_comparisonPlaying = false;
  if (m_comparisonAudio)
    m_comparisonAudio->clear();
  m_mediaPreviewReady = false;
  m_mediaPreviewGeneration = -1;
  m_mediaPreviewSource = {};
  m_mediaPreview = {};
  m_mediaPreviewSlide.clear();
  m_comparisonTime = 0;
  emit mediaOptimisationChanged();
}
void Backend::restoreOriginalMedia() {
  const auto *source = selectedObject();
  if (!source || source->locked ||
      (!source->mediaOriginal && !source->imageOriginal))
    return;
  const bool picture = bool(source->imageOriginal);
  const auto original = picture ? source->imageOriginal : source->mediaOriginal;
  pause();
  m_history.begin(m_document, picture ? tr("Restore original picture")
                                      : tr("Restore original media"));
  auto &slide = m_document.slides[m_currentSlide];
  auto *o = slide.find(selectedId());
  if (picture) {
    ImageAsset::copyData(*o, *original);
    o->imageOriginal.reset();
  } else {
    MediaAsset::copySource(*o, *original);
    o->mediaOriginal.reset();
    fitTrim(slide, *o);
  }
  m_history.commit();
  touch();
}
void Backend::discardOriginalMedia() {
  const auto *source = selectedObject();
  if (!source || source->locked ||
      (!source->mediaOriginal && !source->imageOriginal))
    return;
  m_history.begin(m_document, tr("Discard original media"));
  auto *object = m_document.slides[m_currentSlide].find(selectedId());
  object->mediaOriginal.reset();
  object->imageOriginal.reset();
  m_history.commit();
  touch();
}
QImage Backend::mediaComparisonFrame(bool compressed, qreal seconds) const {
  const auto &source = compressed ? m_mediaPreview : m_mediaPreviewSource;
  return assetKey(source).isEmpty() ? QImage()
         : m_imageOptimisation      ? source.image
                                    : MediaAsset::frameAt(source, seconds);
}
void Backend::playMediaComparison(bool compressed) {
  if (!m_mediaPreviewReady || busy() || m_imageOptimisation)
    return;
  pause();
  m_comparisonCompressed = compressed;
  m_comparisonPlaying = true;
  if (m_comparisonTime >= m_mediaPreviewSource.mediaDuration)
    m_comparisonTime = 0;
  m_comparisonFrom = m_comparisonTime;
  auto object = compressed ? m_mediaPreview : m_mediaPreviewSource;
  object.mediaPosition = m_comparisonTime;
  object.mediaActive = true;
  object.hidden = false;
  object.opacity = 1;
  object.mediaVolume = m_mediaPreviewSource.mediaVolume;
  m_comparisonAudio->sync({object}, true, 1);
  m_comparisonClock.restart();
  m_comparisonTicker.start();
  emit mediaOptimisationChanged();
}
void Backend::seekMediaComparison(qreal seconds) {
  if (!std::isfinite(seconds))
    return;
  m_comparisonPlaying = false;
  m_comparisonTicker.stop();
  m_comparisonTime = qBound(0.0, seconds, m_mediaPreviewSource.mediaDuration);
  if (m_comparisonAudio)
    m_comparisonAudio->clear();
  emit mediaOptimisationChanged();
}
void Backend::syncMediaComparison() {
  if (!m_comparisonPlaying || !m_mediaPreviewReady)
    return;
  m_comparisonTime =
      qMin(m_mediaPreviewSource.mediaDuration,
           m_comparisonFrom + m_comparisonClock.elapsed() / 1000.0);
  auto object = m_comparisonCompressed ? m_mediaPreview : m_mediaPreviewSource;
  object.mediaPosition = qMin(m_comparisonTime, object.mediaDuration);
  object.mediaActive = m_comparisonTime < m_mediaPreviewSource.mediaDuration;
  object.hidden = false;
  object.opacity = 1;
  object.mediaVolume = m_mediaPreviewSource.mediaVolume;
  m_comparisonAudio->sync({object}, object.mediaActive, 1);
  if (!object.mediaActive) {
    m_comparisonPlaying = false;
    m_comparisonTicker.stop();
  }
  emit mediaOptimisationChanged();
}
