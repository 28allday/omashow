#include "anim/presentation.h"
#include "backend.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/imageasset.h"
#include "filepicker.h"
#include "mediaplayback.h"
#include "io/recovery.h"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <cmath>

void Backend::resetMediaSession() {
  cancelJournal();
  Recovery::discard();
  if (m_mediaPlayback)
    m_mediaPlayback->clear();
  discardMediaOptimisation();
  ++m_documentGeneration;
  m_imageQueue.clear();
  cancelMediaJob();
  m_mediaPermissions.clear();
}
void Backend::cancelMediaJob() {
  if (m_mediaJob)
    m_mediaJob->canceled = true;
}
void Backend::insertMediaDialog(bool embed) {
  if (busy() || m_document.slides.isEmpty())
    return;
  m_mediaPickerEmbed = embed;
  m_mediaPickerGeneration = m_documentGeneration;
  m_mediaPickerSlide = m_document.slides.at(m_currentSlide).id;
  m_pending = Pending::InsertMedia;
  m_chooser->openFile(tr("Insert audio or video"), tr("Audio and video"),
                      {"*.mp4", "*.mov", "*.mkv", "*.webm", "*.mp3", "*.wav",
                       "*.flac", "*.ogg", "*.m4a"});
}
void Backend::relinkMediaDialog() {
  const auto *o = selectedObject();
  if (!o || o->type != ObjectType::Media || busy())
    return;
  m_mediaPickerEmbed = o->mediaPath.isEmpty();
  m_mediaPickerGeneration = m_documentGeneration;
  m_mediaPickerSlide = m_document.slides.at(m_currentSlide).id;
  m_mediaPickerTarget = o->id;
  m_pending = Pending::ReplaceMedia;
  m_chooser->openFile(tr("Relink or replace media"), tr("Audio and video"),
                      {"*.mp4", "*.mov", "*.mkv", "*.webm", "*.mp3", "*.wav",
                       "*.flac", "*.ogg", "*.m4a"});
}
bool Backend::insertMedia(const QUrl &url, bool embed) {
  return !m_document.slides.isEmpty() &&
         loadMedia(url, embed, m_document.slides.at(m_currentSlide).id);
}
bool Backend::replaceMedia(const QUrl &url, bool embed) {
  const auto *o = selectedObject();
  return o && o->type == ObjectType::Media &&
         loadMedia(url, embed, m_document.slides.at(m_currentSlide).id, o->id);
}
bool Backend::loadMedia(const QUrl &url, bool embed, const QString &slideId,
                        const QString &target, bool approveOnly) {
  if (busy() || !url.isLocalFile())
    return false;
  QString previousHash;
  if (!target.isEmpty()) {
    for (const auto &slide : m_document.slides)
      if (slide.id == slideId)
        if (const auto *o = slide.find(target)) {
          if (o->type != ObjectType::Media || (o->locked && !approveOnly))
            return false;
          previousHash = o->mediaId;
        }
    if (previousHash.isEmpty())
      return false;
  }
  const int generation = m_documentGeneration;
  const auto job = std::make_shared<MediaAsset::Job>();
  m_mediaJob = job;
  m_mediaJobLabel =
      approveOnly ? tr("Checking linked media") : tr("Reading media");
  setBusy(true);
  m_mediaProgressTimer.start();
  emit mediaJobChanged();
  auto *watcher = new QFutureWatcher<MediaAsset::Result>(this);
  connect(watcher, &QFutureWatcher<MediaAsset::Result>::finished, this,
          [this, watcher, job, generation, slideId, target, previousHash,
           approveOnly] {
            const auto result = watcher->result();
            watcher->deleteLater();
            m_mediaProgressTimer.stop();
            m_mediaJob.reset();
            setBusy(false);
            emit mediaJobChanged();
            if (job->canceled || generation != m_documentGeneration) {
              emit mediaImportFinished(false);
              return;
            }
            if (!result.ok()) {
              emit failed(result.error);
              emit mediaImportFinished(false);
              return;
            }
            int index = -1;
            for (int i = 0; i < m_document.slides.size(); ++i)
              if (m_document.slides.at(i).id == slideId)
                index = i;
            const auto *existing =
                index >= 0 ? m_document.slides.at(index).find(target) : nullptr;
            if (index < 0 || (!target.isEmpty() &&
                              (!existing || existing->mediaId != previousHash ||
                               (existing->locked && !approveOnly)))) {
              emit failed(tr("The media target changed during import."));
              emit mediaImportFinished(false);
              return;
            }
            auto asset = result.object;
            if (approveOnly) {
              if (asset.mediaId != previousHash) {
                emit failed(tr("The linked file has changed. Use Relink to "
                               "review its replacement."));
                emit mediaImportFinished(false);
                return;
              }
              // Permission requires matching bytes. Files copied to another
              // machine may have a new modification time; keep that session
              // fact separately.
              if (existing->mediaModified != asset.mediaModified ||
                  existing->mediaBytes != asset.mediaBytes) {
                emit failed(tr("The linked file's metadata changed. Relink it "
                               "to update the saved reference."));
                emit mediaImportFinished(false);
                return;
              }
              m_mediaPermissions[asset.mediaPath] = asset.mediaId;
              emit mediaJobChanged();
              emit selectionChanged();
              emit timeChanged();
              emit mediaImportFinished(true);
              return;
            }
            auto capacity = m_document;
            capacity.slides[index].objects.append(asset);
            const qint64 bytes = MediaAsset::embeddedBytes(capacity);
            if (bytes > 480LL * 1024 * 1024) {
              emit failed(tr("Embedding this clip would exceed the deck's "
                             "asset limit. Choose Link instead."));
              emit mediaImportFinished(false);
              return;
            }
            m_history.begin(m_document, target.isEmpty() ? tr("Insert media")
                                                         : tr("Replace media"));
            auto &slide = m_document.slides[index];
            if (auto *o = slide.find(target)) {
              // Preserve identity, layout, style and existing animation steps.
              const auto retained = *o;
              if (o->mediaId != asset.mediaId)
                o->mediaOriginal.reset();
              o->mediaData = asset.mediaData;
              o->mediaId = asset.mediaId;
              o->mediaPath = asset.mediaPath;
              o->mediaName = asset.mediaName;
              o->mediaContainer = asset.mediaContainer;
              o->mediaCodec = asset.mediaCodec;
              o->mediaBytes = asset.mediaBytes;
              o->mediaModified = asset.mediaModified;
              o->mediaDuration = asset.mediaDuration;
              o->mediaVideo = asset.mediaVideo;
              o->mediaAudio = asset.mediaAudio;
              o->mediaTrimStart = qMin(retained.mediaTrimStart,
                                       qMax(0.0, asset.mediaDuration - .01));
              o->mediaTrimEnd =
                  qBound(o->mediaTrimStart + .000001, retained.mediaTrimEnd,
                         asset.mediaDuration);
              ImageAsset::copyData(*o, asset);
              for (auto &step : slide.timeline.steps)
                if (step.targetId == o->id && step.effect == Effect::Media)
                  step.duration = MediaAsset::playbackDuration(*o);
            } else {
              asset.id = Edit::newId("media");
              asset.groups = m_groupScope;
              QSizeF size = asset.image.size();
              size.scale(m_document.size * .65, Qt::KeepAspectRatio);
              asset.rect = QRectF(
                  QPointF((m_document.size.width() - size.width()) / 2,
                          (m_document.size.height() - size.height()) / 2),
                  size);
              slide.objects.append(asset);
              BuildStep step;
              step.targetId = asset.id;
              step.effect = Effect::Media;
              step.easing = QEasingCurve::Linear;
              step.duration = MediaAsset::playbackDuration(asset);
              slide.timeline.steps.append(step);
            }
            if (!asset.mediaPath.isEmpty())
              m_mediaPermissions[asset.mediaPath] = asset.mediaId;
            m_history.commit();
            if (index == m_currentSlide)
              select(target.isEmpty() ? asset.id : target);
            touch();
            emit mediaImportFinished(true);
          });
  watcher->setFuture(QtConcurrent::run([path = url.toLocalFile(), embed, job] {
    return MediaAsset::fromFile(path, embed, job);
  }));
  return true;
}
QVariantList Backend::mediaPreflight() const {
  QVariantList list;
  for (int i = 0; i < m_document.slides.size(); ++i)
    for (const auto &o : m_document.slides.at(i).objects)
      if (o.type == ObjectType::Media) {
        const bool allowed = m_mediaPermissions.value(o.mediaPath) == o.mediaId;
        list.append(QVariantMap{{"slide", i},
                                {"slideId", m_document.slides.at(i).id},
                                {"objectId", o.id},
                                {"name", o.mediaName},
                                {"path", o.mediaPath},
                                {"state", MediaAsset::linkState(o, allowed)},
                                {"embedded", o.mediaPath.isEmpty()},
                                {"approved", allowed},
                                {"bytes", o.mediaBytes},
                                {"codec", o.mediaCodec},
                                {"duration", o.mediaDuration}});
      }
  return list;
}
void Backend::approveMedia(const QString &slideId, const QString &objectId) {
  for (const auto &slide : m_document.slides)
    if (slide.id == slideId)
      if (const auto *o = slide.find(objectId)) {
        if (o->type == ObjectType::Media && !o->mediaPath.isEmpty())
          loadMedia(QUrl::fromLocalFile(o->mediaPath), false, slideId, objectId,
                    true);
        return;
      }
}
void Backend::embedSelectedMedia() {
  const auto *o = selectedObject();
  if (!o || o->type != ObjectType::Media || o->mediaPath.isEmpty())
    return;
  if (m_mediaPermissions.value(o->mediaPath) != o->mediaId) {
    emit failed(
        tr("Approve this linked file in Media preflight before embedding it."));
    return;
  }
  loadMedia(QUrl::fromLocalFile(o->mediaPath), true,
            m_document.slides.at(m_currentSlide).id, o->id);
}
QVector<SceneObject> Backend::statesAt(qreal time, bool includeSkipped) const {
  auto states = presentation(includeSkipped).statesAt(time);
  for (auto &o : states)
    if (o.type == ObjectType::Media)
      o.mediaReadAllowed = !o.mediaPath.isEmpty() &&
                           m_mediaPermissions.value(o.mediaPath) == o.mediaId;
  return states;
}
void Backend::setMediaPlayback(qreal start, qreal end, int loops, qreal volume,
                               bool onClick) {
  const auto *source = selectedObject();
  if (!source || source->locked || source->type != ObjectType::Media)
    return;
  if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(volume) ||
      start < 0 || end <= start || end > source->mediaDuration || loops < 1 ||
      loops > 100 || volume < 0 || volume > 1)
    return;
  pause();
  m_history.begin(m_document, tr("Edit media playback"));
  auto &slide = m_document.slides[m_currentSlide];
  auto *o = slide.find(selectedId());
  o->mediaTrimStart = start;
  o->mediaTrimEnd = end;
  o->mediaLoops = loops;
  o->mediaVolume = volume;
  BuildStep *cue = nullptr;
  for (auto &step : slide.timeline.steps)
    if (step.targetId == o->id && step.effect == Effect::Media) {
      cue = &step;
      break;
    }
  if (!cue) {
    BuildStep step;
    step.targetId = o->id;
    step.effect = Effect::Media;
    slide.timeline.steps.append(step);
    cue = &slide.timeline.steps.last();
  }
  cue->duration = MediaAsset::playbackDuration(*o);
  cue->phase = BuildPhase::In;
  cue->easing = QEasingCurve::Linear;
  cue->trigger = onClick ? BuildTrigger::OnClick : BuildTrigger::Absolute;
  cue->start = 0;
  cue->delay = 0;
  m_history.commit();
  touch();
}
void Backend::previewMedia() {
  emit mediaPreviewRequested();
  const auto rows = builds();
  for (int i = 0; i < rows.size(); ++i)
    if (rows.at(i).toMap().value("targetId").toString() == selectedId() &&
        rows.at(i).toMap().value("effect").toInt() == int(Effect::Media)) {
      previewBuild(i);
      return;
    }
}
void Backend::stopMedia() {
  pause();
  for (const auto &row : builds())
    if (row.toMap().value("targetId").toString() == selectedId() &&
        row.toMap().value("effect").toInt() == int(Effect::Media)) {
      setLocalTime(row.toMap().value("start").toDouble());
      return;
    }
}

void Backend::syncMedia() {
  if (m_mediaPlayback)
    m_mediaPlayback->sync(statesAt(m_time, m_includeSkipped),
                          m_playing && !m_mediaSuppressed, m_playbackRate);
}
QVariantList Backend::mediaTransport() const {
  return m_mediaPlayback ? m_mediaPlayback->transport() : QVariantList();
}
void Backend::setMediaSuppressed(bool value) {
  m_mediaSuppressed = value;
  syncMedia();
}

void Backend::refreshMediaPreflight() {
  emit mediaJobChanged();
  emit selectionChanged();
  emit timeChanged();
}
