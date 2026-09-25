#include "backend.h"
#include "filepicker.h"
#include "io/exports.h"
#include "io/printing.h"
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

QVariantList Backend::exportQueue() const {
  QVariantList rows;
  for (const auto &entry : m_exports) {
    auto row = entry.request.toMap();
    row["id"] = entry.id;
    row["state"] = entry.state;
    row["progress"] = entry.percent ? entry.percent->load() : entry.progress;
    row["message"] = entry.message;
    row["log"] = entry.log;
    row["name"] = entry.request.kind == Exports::Print
                      ? (entry.request.printer.isEmpty() ? tr("Printer")
                                                         : entry.request.printer)
                      : QFileInfo(entry.request.path).fileName();
    row["folder"] = QFileInfo(entry.request.path).absolutePath();
    row["detail"] = entry.request.describe();
    row["running"] = entry.state == QLatin1String("running");
    row["finished"] = entry.state == QLatin1String("done") ||
                      entry.state == QLatin1String("failed") ||
                      entry.state == QLatin1String("canceled");
    rows.append(row);
  }
  return rows;
}

bool Backend::encoderAvailable() const { return Exports::encoderAvailable(); }

// Asking CUPS what printers exist can take many seconds, so the answer is
// fetched once on a worker and the interface fills in when it arrives.
QVariantList Backend::printers() const { return m_printers; }
bool Backend::printersKnown() const { return m_printersKnown; }

void Backend::refreshPrinters() {
  if (m_printersRunning) return;
  m_printersRunning = true;
  struct Found { QVariantList printers; QString byDefault; };
  auto *watcher = new QFutureWatcher<Found>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
    const auto found = watcher->result();
    watcher->deleteLater();
    m_printersRunning = false;
    m_printersKnown = true;
    m_printers = found.printers;
    m_defaultPrinter = found.byDefault;
    emit printersChanged();
  });
  watcher->setFuture(QtConcurrent::run(Workers::io(), [] {
    return Found{Printing::printers(), Printing::defaultPrinter()};
  }));
}

void Backend::exportDialog(const QVariantMap &options) {
  // Printing has no file to name; it goes straight into the queue.
  if (Exports::Request::fromMap(options).kind == Exports::Print) {
    queueExport(options);
    return;
  }
  m_pendingExport = options;
  m_pending = Pending::ExportFile;
  const auto request = Exports::Request::fromMap(options);
  const QString filter = request.kind == Exports::Video   ? tr("MP4 film")
                         : request.kind == Exports::PowerPoint ? tr("PowerPoint decks")
                         : request.kind == Exports::Package ? tr("Deck packages")
                         : request.kind == Exports::Images
                             ? (request.format == 0 ? tr("PNG pictures") : tr("JPEG pictures"))
                             : tr("PDF documents");
  const QString pattern = request.kind == Exports::Video   ? QStringLiteral("*.mp4")
                          : request.kind == Exports::PowerPoint ? QStringLiteral("*.pptx")
                          : request.kind == Exports::Package ? QStringLiteral("*.zip")
                          : request.kind == Exports::Images
                              ? (request.format == 0 ? QStringLiteral("*.png")
                                                     : QStringLiteral("*.jpg"))
                              : QStringLiteral("*.pdf");
  m_chooser->saveFile(tr("Export"), request.suggestedName(fileName()), filter, {pattern});
}

int Backend::queueExport(const QVariantMap &options) {
  auto request = Exports::Request::fromMap(options);
  if (m_document.slides.isEmpty()) return -1;
  if (request.path.isEmpty() && request.kind != Exports::Print) return -1;
  if (request.kind == Exports::Print && m_printersKnown && m_defaultPrinter.isEmpty() &&
      request.printer.isEmpty()) {
    setStatus(tr("No printer is set up on this computer."));
    emit failed(status());
    return -1;
  }
  if (request.kind == Exports::Video && !Exports::encoderAvailable()) {
    setStatus(tr("FFmpeg is not installed, so film cannot be encoded here."));
    emit failed(status());
    return -1;
  }
  if (m_exports.size() >= 32) {
    setStatus(tr("The export queue is full. Clear what has finished."));
    return -1;
  }
  ExportEntry entry;
  entry.id = ++m_exportSerial;
  entry.request = request;
  entry.state = QStringLiteral("queued");
  // The deck is taken as it is now, so editing while it exports is safe.
  entry.document = m_document;
  for (auto &slide : entry.document.slides)
    for (auto &object : slide.objects)
      if (object.type == ObjectType::Media)
        object.mediaReadAllowed = m_mediaPermissions.value(object.mediaPath) == object.mediaId;
  m_exports.append(entry);
  emit exportQueueChanged();
  startNextExport();
  return entry.id;
}

void Backend::startNextExport() {
  for (const auto &entry : m_exports)
    if (entry.state == QLatin1String("running")) return;
  int next = -1;
  for (int i = 0; i < m_exports.size(); ++i)
    if (m_exports.at(i).state == QLatin1String("queued")) { next = i; break; }
  if (next < 0) {
    if (m_exportTicker.isActive()) m_exportTicker.stop();
    return;
  }
  auto &entry = m_exports[next];
  entry.state = QStringLiteral("running");
  entry.message = tr("Working…");
  entry.job = std::make_shared<Workers::Job>();
  entry.percent = std::make_shared<std::atomic_int>(0);
  const int id = entry.id;
  const auto request = entry.request;
  const auto document = entry.document;
  const auto job = entry.job;
  const auto percent = entry.percent;
  const auto approved = m_mediaPermissions;
  if (!m_exportTicker.isActive()) m_exportTicker.start(200);
  emit exportQueueChanged();
  auto *watcher = new QFutureWatcher<Exports::Outcome>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
    const auto outcome = watcher->result();
    watcher->deleteLater();
    for (auto &entry : m_exports) {
      if (entry.id != id) continue;
      const bool canceled = entry.job && entry.job->canceled;
      entry.state = outcome.ok ? QStringLiteral("done")
                    : canceled ? QStringLiteral("canceled")
                               : QStringLiteral("failed");
      entry.progress = outcome.ok ? 100 : entry.percent ? entry.percent->load() : 0;
      entry.message = outcome.ok
                          ? (outcome.files.size() == 1
                                 ? tr("Wrote %1").arg(QFileInfo(outcome.files.first()).fileName())
                                 : tr("Wrote %1 files").arg(outcome.files.size()))
                          : outcome.error;
      entry.log = outcome.log;
      entry.job.reset();
      entry.percent.reset();
      entry.document = Document();   // the export is over; let the copy go
      if (outcome.ok) setStatus(entry.message);
      else if (!canceled) { setStatus(outcome.error); emit failed(outcome.error); }
      break;
    }
    emit exportQueueChanged();
    startNextExport();
  });
  watcher->setFuture(QtConcurrent::run(Workers::io(), [document, request, job, percent, approved] {
    return Exports::run(document, request, job,
                        [percent](int value) { percent->store(value); }, approved);
  }));
}

void Backend::cancelExport(int id) {
  for (auto &entry : m_exports) {
    if (entry.id != id) continue;
    if (entry.state == QLatin1String("queued")) {
      entry.state = QStringLiteral("canceled");
      entry.message = tr("Canceled before it started");
      emit exportQueueChanged();
      startNextExport();
    } else if (entry.job) {
      entry.job->cancel();
      entry.message = tr("Stopping…");
      emit exportQueueChanged();
    }
    return;
  }
}

bool Backend::retryExport(int id) {
  for (auto &entry : m_exports) {
    if (entry.id != id || entry.state == QLatin1String("running") ||
        entry.state == QLatin1String("queued"))
      continue;
    entry.state = QStringLiteral("queued");
    entry.message.clear();
    entry.log.clear();
    entry.progress = 0;
    entry.document = m_document;
    for (auto &slide : entry.document.slides)
      for (auto &object : slide.objects)
        if (object.type == ObjectType::Media)
          object.mediaReadAllowed = m_mediaPermissions.value(object.mediaPath) == object.mediaId;
    emit exportQueueChanged();
    startNextExport();
    return true;
  }
  return false;
}

void Backend::clearFinishedExports() {
  const int before = m_exports.size();
  for (int i = m_exports.size() - 1; i >= 0; --i) {
    const auto &state = m_exports.at(i).state;
    if (state == QLatin1String("done") || state == QLatin1String("failed") ||
        state == QLatin1String("canceled"))
      m_exports.removeAt(i);
  }
  if (before != m_exports.size()) emit exportQueueChanged();
}
