#include "backend.h"
#include "mediaplayback.h"
#include "core/design.h"
#include "core/review.h"
#include "io/decklock.h"
#include "io/exports.h"
#include <QDateTime>
#include <QProcess>
#include <QFileInfo>

#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileInfo>
#include <QImage>

#include "anim/presentation.h"
#include "core/edit.h"
#include "core/snap.h"
#include "filepicker.h"
#include "fixture.h"
#include "io/bundle.h"
#include "io/pdf.h"
#include "io/recovery.h"
#include "render/scenerenderer.h"

Backend::Backend(QObject *parent)
    : QObject(parent), m_chooser(new PortalFileChooser(this)) {
  connect(this,&Backend::documentChanged,this,&Backend::slideSelectionChanged);
  connect(this,&Backend::documentChanged,this,&Backend::fileStateChanged);
  // A pending import is described against the document it would land in.
  connect(this,&Backend::documentChanged,this,[this]{ if(!m_importSource.slides.isEmpty()) refreshImport(); });
  connect(this,&Backend::currentSlideChanged,this,[this]{ if(!m_importSource.slides.isEmpty()) refreshImport(); });
  connect(this,&Backend::documentChanged,this,&Backend::browserChanged);
  connect(this,&Backend::currentSlideChanged,this,&Backend::slideSelectionChanged);
  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Backend::clipboardChanged);
  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this] { ++m_clipboardVersion; });
  connect(m_chooser, &PortalFileChooser::selected, this,
          [this](const QUrl &url) {
            const Pending pending = m_pending;
            m_pending = Pending::None;
            QString path = url.toLocalFile();

            const auto ensureSuffix = [&path](const QString &suffix) {
              if (!path.endsWith(suffix, Qt::CaseInsensitive))
                path += suffix;
            };

            switch (pending) {
            case Pending::SaveDeck:
              ensureSuffix(QStringLiteral(".omashow"));
              saveAsync(path);
              break;
            case Pending::ExportPdf:
              ensureSuffix(QStringLiteral(".pdf"));
              exportPdfAsync(path, m_pendingPdfStages, m_pendingPdfSkipped);
              break;
            case Pending::InsertMedia:
            case Pending::ReplaceMedia:
              if(m_mediaPickerGeneration==m_documentGeneration)
                loadMedia(url,m_mediaPickerEmbed,m_mediaPickerSlide,pending==Pending::ReplaceMedia ? m_mediaPickerTarget : QString());
              break;
            case Pending::InsertImage:
            case Pending::ReplaceImage: {
              int index=-1;
              for(int i=0;i<m_document.slides.size();++i) if(m_document.slides.at(i).id==m_imageSlideId) index=i;
              loadImageAsync(url,pending==Pending::ReplaceImage,index,m_imageTargetId);
              break;
            }
            case Pending::ImportDeck:
              importFromDeck(url);
              break;
            case Pending::ExportReview:
              ensureSuffix(QStringLiteral(".txt"));
              exportReview(path);
              break;
            case Pending::ExportFile: {
              auto options = m_pendingExport;
              m_pendingExport.clear();
              const auto request = Exports::Request::fromMap(options);
              ensureSuffix(QFileInfo(request.suggestedName(QStringLiteral("x"))).suffix().isEmpty()
                               ? QString()
                               : '.' + QFileInfo(request.suggestedName(QStringLiteral("x"))).suffix());
              options["path"] = path;
              queueExport(options);
              break;
            }
            case Pending::InstallTemplate:
              installTemplate(url);
              break;
            case Pending::None:
              openAsync(url);
              break;
            }
          });
  connect(m_chooser, &PortalFileChooser::canceled, this,
          [this] {
            const bool saving = m_pending == Pending::SaveDeck;
            m_pending = Pending::None;
            if (saving) emit saveCanceled();
          });
  connect(m_chooser, &PortalFileChooser::failed, this,
          [this](const QString &message) {
            setStatus(message);
            emit failed(message);
          });

  m_comparisonAudio=new MediaPlayback(this);
  connect(m_comparisonAudio,&MediaPlayback::failed,this,&Backend::failed);
  m_exportTicker.setInterval(200);
  connect(&m_exportTicker, &QTimer::timeout, this, [this] {
    for (const auto &entry : m_exports)
      if (entry.state == QLatin1String("running")) { emit exportQueueChanged(); return; }
    m_exportTicker.stop();
  });
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
    if (path != m_fileUrl.toLocalFile()) return;
    const QFileInfo info(path);
    // Our own atomic save renames a new file into place; re-arm and ignore it.
    if (info.exists() && info.lastModified() == m_fileStamp && info.size() == m_fileBytes) {
      if (!m_watcher.files().contains(path)) m_watcher.addPath(path);
      return;
    }
    m_fileChangedOnDisk = true;
    if (info.exists() && !m_watcher.files().contains(path)) m_watcher.addPath(path);
    setStatus(info.exists() ? tr("%1 has changed on disk.").arg(fileName())
                            : tr("%1 is no longer on disk.").arg(fileName()));
    emit fileStateChanged();
  });
  m_comparisonTicker.setInterval(40);
  connect(&m_comparisonTicker,&QTimer::timeout,this,&Backend::syncMediaComparison);
  m_mediaPlayback=new MediaPlayback(this);
  connect(m_mediaPlayback,&MediaPlayback::failed,this,&Backend::failed);
  connect(this,&Backend::timeChanged,this,&Backend::syncMedia);
  connect(this,&Backend::playingChanged,this,&Backend::syncMedia);
  connect(this,&Backend::playbackRateChanged,this,&Backend::syncMedia);
  connect(this,&Backend::documentChanged,this,&Backend::syncMedia);
  m_mediaProgressTimer.setInterval(100);
  connect(&m_mediaProgressTimer,&QTimer::timeout,this,&Backend::mediaJobChanged);
  connect(this,&Backend::documentChanged,this,&Backend::mediaJobChanged);
  const auto invalidateLayoutPreview = [this] {
    if (!m_layoutPreview.isEmpty()) emit layoutPreviewChanged();
  };
  connect(this, &Backend::documentChanged, this, invalidateLayoutPreview);
  connect(this, &Backend::currentSlideChanged, this, invalidateLayoutPreview);
  connect(this, &Backend::slideSelectionChanged, this, invalidateLayoutPreview);

  // The CLI fixture remains available; the GUI opens the Start centre.
  resetMediaSession();
  m_document = Fixture::twoSlideMorph();
  m_history.reset(m_document);
  pause(); m_gestureActive = false; m_guides.clear(); emit guidesChanged();

  m_ticker.setInterval(8); // ask often; the monotonic clock decides the time
  connect(&m_ticker, &QTimer::timeout, this, &Backend::tick);

  // Autosave is debounced rather than periodic: it lands a few seconds after
  // you stop changing things, so a drag does not write the journal sixty
  // times and a pause in typing is never more than this far from safe.
  m_autosave.setSingleShot(true);
  m_autosave.setInterval(4000);
  connect(&m_autosave, &QTimer::timeout, this, &Backend::writeJournal);
  connect(this, &Backend::documentChanged, this, &Backend::scheduleAutosave);
}

Backend::~Backend() {
  cancelOperation();
  cancelJournal();
  cancelMediaJob();
  // A clean exit leaves no journal behind — otherwise every launch would
  // offer to recover from the last ordinary quit.
  Recovery::discard();
}

void Backend::scheduleAutosave() {
  if (m_modified)
    m_autosave.start();
}


QVariantList Backend::recoveryCandidates() const {
  QVariantList list;
  const QVector<Recovery::Journal> journals = Recovery::orphans();
  for (const Recovery::Journal &journal : journals) {
    QVariantMap map;
    map[QStringLiteral("journalPath")] = journal.journalPath;
    map[QStringLiteral("originalPath")] = journal.originalPath;
    map[QStringLiteral("name")] = journal.displayName();
    map[QStringLiteral("savedAt")] =
        journal.savedAt.toLocalTime().toString(QStringLiteral("d MMM, HH:mm"));
    list.append(map);
  }
  return list;
}

void Backend::recoverFrom(const QString &journalPath) {
  const Bundle::ReadResult result = Bundle::load(journalPath);
  if (!result.ok) {
    setStatus(result.error);
    emit failed(result.error);
    return;
  }

  const QVector<Recovery::Journal> journals = Recovery::orphans();
  QString originalPath;
  for (const Recovery::Journal &journal : journals) {
    if (journal.journalPath == journalPath)
      originalPath = journal.originalPath;
  }

  resetMediaSession();
  m_document = result.document;
  m_history.reset(m_document);
  pause(); m_gestureActive = false; m_guides.clear(); emit guidesChanged();
  m_currentSlide = 0;
  m_collapsedSections.clear();
  resetSlideSelection();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_time = 0.0;
  // Recovered work comes back UNSAVED. The last explicit save is still on
  // disk untouched until the user decides to replace it.
  m_modified = true;
  setFileUrl(originalPath.isEmpty() ? QUrl()
                                    : QUrl::fromLocalFile(originalPath));
  ++m_revision;
  emit currentSlideChanged();
  emit selectionChanged();
  emit timeChanged();
  emit documentChanged();
  emit deckChanged();

  activateDocument();
  Recovery::forget(journalPath);
  setStatus(originalPath.isEmpty()
                ? tr("Recovered unsaved work — not saved anywhere yet")
                : tr("Recovered — not yet written over %1").arg(fileName()));
}

void Backend::discardRecovery(const QString &journalPath) {
  Recovery::forget(journalPath);
}

QString Backend::displayNameFor(const QUrl &url) {
  if (url.isEmpty())
    return QStringLiteral("Untitled");
  if (url.isLocalFile()) {
    const QString name = QFileInfo(url.toLocalFile()).fileName();
    if (!name.isEmpty())
      return name;
  }
  const QString name = url.fileName();
  return name.isEmpty() ? QStringLiteral("Untitled") : name;
}

QString Backend::fileName() const { return displayNameFor(m_fileUrl); }

void Backend::openDialog() {
  m_pending = Pending::None;
  m_chooser->openFile(tr("Open deck"), tr("OmaShow decks"),
                      {QStringLiteral("*.omashow")});
}

void Backend::open(const QUrl &url) {
  if (!url.isLocalFile()) {
    setStatus(tr("Only local files can be opened."));
    return;
  }
  const QString path = url.toLocalFile();
  if (!QFileInfo::exists(path)) {
    setStatus(tr("%1 is gone.").arg(displayNameFor(url)));
    emit recentsChanged(); emit failed(status());
    return;
  }

  const Bundle::ReadResult result = Bundle::load(path);
  if (!result.ok) {
    // Opening a deck never damages it, and a failure never replaces what is
    // already on screen.
    setStatus(result.error);
    emit failed(result.error);
    return;
  }

  acceptOpen(result.document, url);
}

void Backend::acceptOpen(const Document &document, const QUrl &url) {
  resetMediaSession();
  m_document = document;
  Review::prune(m_document);
  m_activeShow.clear();
  applyActiveShow();
  m_history.reset(m_document);
  pause(); m_gestureActive = false; m_guides.clear(); emit guidesChanged();
  m_currentSlide = 0;
  m_collapsedSections.clear();
  resetSlideSelection();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_time = 0.0;
  m_modified = false;
  setFileUrl(url);
  ++m_revision;
  emit currentSlideChanged();
  emit selectionChanged();
  emit timeChanged();
  emit documentChanged();
  emit deckChanged();
  activateDocument();
  if (url.isLocalFile()) rememberRecent(url.toLocalFile());
  m_autosave.stop(); Recovery::discard();
  setStatus(tr("Opened %1").arg(fileName()));
  emit opened();
}

void Backend::newDeck() { createDeck(0, 1920, 1080); }

// Another deck at the same time is another copy of the application: separate
// selection, undo, playback, notes and export queue, with nothing shared.
bool Backend::openInNewWindow(const QUrl &url) {
  const auto program = QCoreApplication::applicationFilePath();
  if (program.isEmpty()) return false;
  QStringList arguments;
  if (url.isLocalFile()) arguments << url.toLocalFile();
  if (!QProcess::startDetached(program, arguments)) {
    setStatus(tr("Could not open another window."));
    emit failed(status());
    return false;
  }
  setStatus(arguments.isEmpty() ? tr("Opened another window")
                                : tr("Opened %1 in another window")
                                      .arg(displayNameFor(url)));
  return true;
}

void Backend::save() {
  const QFileInfo info(m_fileUrl.toLocalFile());
  // A file that cannot be written is not an error to discover halfway through
  // saving: ask where it should go instead.
  if (m_fileUrl.isLocalFile() && (!info.exists() || info.isWritable()))
    saveAsync(m_fileUrl.toLocalFile());
  else {
    if (m_fileUrl.isLocalFile())
      setStatus(tr("%1 is read-only — choose where to save instead.").arg(fileName()));
    saveAsDialog();
  }
}

void Backend::saveAsDialog() {
  m_pending = Pending::SaveDeck;
  const QString suggested =
      m_fileUrl.isLocalFile() ? fileName() : QStringLiteral("Untitled.omashow");
  m_chooser->saveFile(tr("Save deck"), suggested, tr("OmaShow decks"),
                      {QStringLiteral("*.omashow")});
}

bool Backend::saveTo(const QString &path) {
  QString error;
  if (!Bundle::save(m_document, path, &error)) {
    setStatus(tr("Could not save: %1").arg(error));
    emit failed(error);
    return false;
  }
  m_modified = false;
  cancelJournal();
  m_autosave.stop();
  // The deck on disk is now the truth; the journal has nothing left to add.
  Recovery::discard();
  setFileUrl(QUrl::fromLocalFile(path));
  watchFile();
  emit fileStateChanged();
  emit documentChanged();
  rememberRecent(path);
  setStatus(tr("Saved %1").arg(fileName()));
  emit saved();
  return true;
}

void Backend::setStatus(const QString &status) {
  if (m_status == status)
    return;
  m_status = status;
  emit statusChanged();
}

void Backend::setBusy(bool busy) {
  if (m_busy == busy)
    return;
  m_busy = busy;
  emit busyChanged();
}

void Backend::setFileUrl(const QUrl &url) {
  if (m_fileUrl == url)
    return;
  m_fileUrl = url;
  watchFile();
  emit fileUrlChanged();
  emit fileStateChanged();
}

// The deck's file, as it stands on disk right now: whether it can be written
// to, whether it is still there, and whether someone else has changed it.
void Backend::watchFile() {
  if (!m_watcher.files().isEmpty()) m_watcher.removePaths(m_watcher.files());
  m_fileChangedOnDisk = false;
  const auto path = m_fileUrl.toLocalFile();
  if (path.isEmpty()) {
    m_fileStamp = QDateTime();
    m_fileBytes = -1;
    m_openedElsewhere = false;
    DeckLock::release();
    return;
  }
  // Advisory only: it says who else has this open, and prevents nothing.
  const auto holder = DeckLock::check(path);
  m_openedElsewhere = holder.held && !holder.mine;
  if (!m_openedElsewhere) DeckLock::take(path);
  const QFileInfo info(path);
  m_fileStamp = info.lastModified();
  m_fileBytes = info.exists() ? info.size() : -1;
  if (info.exists()) m_watcher.addPath(path);
}

QVariantMap Backend::fileState() const {
  const auto path = m_fileUrl.toLocalFile();
  const QFileInfo info(path);
  const bool saved = !path.isEmpty();
  return {{"path", path},
          {"saved", saved},
          {"openedElsewhere", m_openedElsewhere},
          {"missing", saved && !info.exists()},
          {"readOnly", saved && info.exists() && !info.isWritable()},
          {"changedOnDisk", m_fileChangedOnDisk},
          {"modified", m_modified}};
}

void Backend::keepMyVersion() {
  if (!m_fileChangedOnDisk) return;
  m_fileChangedOnDisk = false;
  const QFileInfo info(m_fileUrl.toLocalFile());
  m_fileStamp = info.lastModified();
  m_fileBytes = info.exists() ? info.size() : -1;
  setStatus(tr("Keeping your version — saving will replace what is on disk."));
  emit fileStateChanged();
}

void Backend::reloadFromDisk() {
  if (!m_fileUrl.isLocalFile()) return;
  const auto url = m_fileUrl;
  m_fileChangedOnDisk = false;
  emit fileStateChanged();
  openAsync(url);
}

void Backend::setDocument(const Document &document) {
  activateDocument();
  pause();
  resetMediaSession();
  m_document = document;
  Review::prune(m_document);
  m_activeShow.clear();
  applyActiveShow();
  m_history.reset(m_document);
  pause(); m_gestureActive = false; m_guides.clear(); emit guidesChanged();
  m_currentSlide = 0;
  m_collapsedSections.clear();
  resetSlideSelection();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_modified = false;
  m_time = 0;
  ++m_revision;
  emit currentSlideChanged();
  emit selectionChanged();
  emit timeChanged();
  emit documentChanged();
  emit deckChanged();
}

void Backend::setIncludeSkipped(bool value) {
  if(m_includeSkipped==value) return;
  pause(); m_includeSkipped=value; m_time=qMin(m_time,duration());
  emit deckChanged(); emit timeChanged(); emit selectionChanged();
}
const PresentationCache &Backend::presentation(bool includeSkipped) const {
  if (m_presentationRevision != m_revision || m_presentationSkipped != includeSkipped) {
    m_presentation.reset(m_document, includeSkipped);
    m_presentationRevision = m_revision;
    m_presentationSkipped = includeSkipped;
  }
  return m_presentation;
}
qreal Backend::duration() const { return presentation(m_includeSkipped).duration(); }

int Backend::slideIndex() const {
  return presentation(m_includeSkipped).frameAt(m_time).slideIndex;
}

bool Backend::inTransition() const {
  return presentation(m_includeSkipped).frameAt(m_time).inTransition;
}

QString Backend::timecode() const {
  return QStringLiteral("%1 / %2")
      .arg(m_time, 0, 'f', 2)
      .arg(duration(), 0, 'f', 2);
}

void Backend::setTime(qreal time) {
  time = qBound(0.0, time, duration());
  if (qFuzzyCompare(m_time + 1.0, time + 1.0))
    return;
  m_time = time;
  emit timeChanged();
}

void Backend::play() {
  m_playTo = -1.0;
  if (m_playing)
    return;
  if (m_time >= duration())
    m_time = 0.0;
  m_playFrom = m_time;
  m_playing = true;
  emit playingChanged();
  // Player creation may initialize an audio backend. That setup time is not
  // presentation time: start the monotonic clock after synchronous listeners.
  if(m_playing) { m_clock.restart(); m_ticker.start(); }
}

void Backend::pause() {
  if (!m_playing)
    return;
  m_ticker.stop();
  m_playing = false;
  emit playingChanged();
}

void Backend::togglePlay() { m_playing ? pause() : play(); }

void Backend::restart() {
  pause();
  setTime(0.0);
  play();
}

void Backend::step(qreal seconds) {
  pause();
  setTime(m_time + seconds);
}

void Backend::tick() {
  const qreal end = m_playTo >= 0 ? m_playTo : duration();
  setTime(qMin(end, m_playFrom + m_clock.elapsed() / 1000.0 * m_playbackRate));
  if (m_time >= end)
    pause();
}

bool Backend::renderFrame(qreal time, const QString &path, int width,bool includeSkipped) const {
  if (Presentation::slideIndices(m_document,includeSkipped).isEmpty() || m_document.size.isEmpty())
    return false;

  const int height =
      qRound(width * m_document.size.height() / m_document.size.width());
  const QColor background = Presentation::backgroundAt(m_document, time, includeSkipped);

  const QImage image =
      SceneRenderer::render(statesAt(time, includeSkipped),
                            m_document.size, QSize(width, height), background);

  QDir().mkpath(QFileInfo(path).absolutePath());
  return image.save(path, "PNG");
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void Backend::touch() {
  // Notes and reading orders never outlive what they point at.
  Review::prune(m_document);
  applyActiveShow();
  ++m_revision;
  m_modified = true;
  emit documentChanged();
  emit deckChanged();
  emit selectionChanged();
}

SceneObject *Backend::selectedObject() {
  if (m_selectedId.isEmpty() || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return nullptr;
  return m_document.slides[m_currentSlide].find(m_selectedId);
}

const SceneObject *Backend::selectedObject() const {
  if (m_selectedId.isEmpty() || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return nullptr;
  return m_document.slides.at(m_currentSlide).find(m_selectedId);
}

bool Backend::hasSelection() const { return !selectedIds().isEmpty(); }

qreal Backend::settledTime(int slideIndex) const {
  if (slideIndex < 0 || slideIndex >= m_document.slides.size())
    return 0.0;
  return m_document.slides.at(slideIndex).timeline.duration();
}

void Backend::setCurrentSlide(int index) {
  resetSlideSelection();
  index = qBound(0, index, qMax(0, m_document.slides.size() - 1));
  if (m_currentSlide == index)
    return;
  m_currentSlide = index;
  m_groupScope.clear();
  clearSelection();
  emit selectionChanged();
  emit currentSlideChanged();
}

void Backend::addSlide() {
  m_history.begin(m_document, QStringLiteral("Add slide"));
  const int at = Edit::addSlide(m_document, m_currentSlide);
  m_history.commit();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_currentSlide = at;
  resetSlideSelection();
  emit currentSlideChanged();
  touch();
}

void Backend::duplicateSlide() {
  m_history.begin(m_document, QStringLiteral("Duplicate slide"));
  const int at = Edit::duplicateSlide(m_document, m_currentSlide);
  m_history.commit();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_currentSlide = at;
  resetSlideSelection();
  emit currentSlideChanged();
  touch();
}

void Backend::deleteSlide() {
  resetSlideSelection();
  m_history.begin(m_document, QStringLiteral("Delete slide"));
  if (!Edit::deleteSlide(m_document, m_currentSlide)) {
    m_history.abandon();
    setStatus(QStringLiteral("A deck keeps at least one slide."));
    return;
  }
  m_history.commit();
  m_selectedId.clear();
  m_selectedIds.clear();
  m_groupScope.clear();
  m_currentSlide = qBound(0, m_currentSlide, m_document.slides.size() - 1);
  emit currentSlideChanged();
  touch();
}

void Backend::moveSlide(int from, int to) {
  const QString currentId = m_document.slides.isEmpty()
                                ? QString()
                                : m_document.slides.at(m_currentSlide).id;
  m_history.begin(m_document, QStringLiteral("Reorder slides"));
  if (!Edit::moveSlide(m_document, from, to)) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  for (int i = 0; i < m_document.slides.size(); ++i)
    if (m_document.slides.at(i).id == currentId)
      m_currentSlide = i;
  emit currentSlideChanged();
  touch();
}

void Backend::addText() {
  m_history.begin(m_document, QStringLiteral("Add text"));
  const QString id = Edit::addText(
      m_document, m_currentSlide,
      QPointF(m_document.size.width() / 2.0, m_document.size.height() / 2.0));
  m_history.commit();
  m_selectedId = id;
  m_selectedIds = {id};
  if (auto *o = selectedObject())
    o->groups = m_groupScope;
  touch();
}

void Backend::addRect() { addShape(0); }

void Backend::setSnapEnabled(bool enabled) {
  if (m_snapEnabled == enabled)
    return;
  m_snapEnabled = enabled;
  emit snapEnabledChanged();
}

void Backend::undo() {
  const auto currentId=m_document.slides.value(m_currentSlide).id;
  if (!m_history.undo(m_document))
    return;
  m_currentSlide =
      qBound(0, m_currentSlide, qMax(0, m_document.slides.size() - 1));
  restoreCurrentSlide(currentId);
  m_selectedIds = selectedIds();
  m_selectedId = m_selectedIds.isEmpty() ? QString() : m_selectedIds.last();
  if (m_selectedIds.isEmpty())
    m_groupScope.clear();
  touch();
}

void Backend::redo() {
  const auto currentId=m_document.slides.value(m_currentSlide).id;
  if (!m_history.redo(m_document))
    return;
  m_currentSlide =
      qBound(0, m_currentSlide, qMax(0, m_document.slides.size() - 1));
  restoreCurrentSlide(currentId);
  m_selectedIds = selectedIds();
  m_selectedId = m_selectedIds.isEmpty() ? QString() : m_selectedIds.last();
  if (m_selectedIds.isEmpty())
    m_groupScope.clear();
  touch();
}

void Backend::exportPdfDialog(bool pagePerBuildStage,bool includeSkipped) {
  m_pending = Pending::ExportPdf;
  m_pendingPdfStages = pagePerBuildStage;
  m_pendingPdfSkipped = includeSkipped;
  const QString base =
      m_fileUrl.isLocalFile()
          ? QFileInfo(m_fileUrl.toLocalFile()).completeBaseName()
          : QStringLiteral("Untitled");
  m_chooser->saveFile(tr("Export PDF"), base + QStringLiteral(".pdf"),
                      tr("PDF documents"), {QStringLiteral("*.pdf")});
}

bool Backend::exportPdf(const QString &path, bool pagePerBuildStage,bool includeSkipped) {
  Pdf::Options options;
  options.pagePerBuildStage = pagePerBuildStage;
  options.includeSkipped = includeSkipped;

  QString error;
  auto exportDocument=m_document;
  for(auto &slide:exportDocument.slides) for(auto &o:slide.objects)
    if(o.type==ObjectType::Media) o.mediaReadAllowed=m_mediaPermissions.value(o.mediaPath)==o.mediaId;
  if (!Pdf::write(exportDocument, path, options, &error)) {
    setStatus(tr("Could not export: %1").arg(error));
    emit failed(error);
    return false;
  }
  setStatus(tr("Exported %1").arg(QFileInfo(path).fileName()));
  return true;
}
