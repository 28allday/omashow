#include "backend.h"
#include "core/imageasset.h"
#include "io/bundle.h"
#include "io/pdf.h"
#include "io/recovery.h"
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

bool Backend::beginOperation(const QString &label) {
    if (!operation().isEmpty()) return false;
    m_operationJob = std::make_shared<Workers::Job>();
    m_operation = label;
    emit operationChanged();
    return true;
}
void Backend::endOperation() {
    m_operation.clear(); m_operationJob.reset();
    emit operationChanged();
}
void Backend::cancelOperation() {
    if (m_operationJob) m_operationJob->cancel();
    if (m_imageImportRunning) { m_cancelImages = true; m_imageQueue.clear(); }
}
void Backend::cancelJournal() {
    if (m_journalJob) m_journalJob->cancel();
    m_journalAgain = false;
}
void Backend::writeJournal() {
    if (!m_modified) return;
    if (m_journalRunning) { m_journalAgain = true; return; }
    const auto document = m_document;
    const auto path = m_fileUrl.toLocalFile();
    const auto job = std::make_shared<Workers::Job>();
    m_journalJob = job; m_journalRunning = true;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job] {
        m_journalRunning = false;
        const auto error = watcher->result();
        if (!job->canceled && !error.isEmpty()) setStatus(tr("Autosave failed: %1").arg(error));
        watcher->deleteLater();
        if (m_journalAgain) { m_journalAgain = false; writeJournal(); }
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [document, path, job] {
        QString error;
        if (!job->canceled) Recovery::write(document, path, &error, job);
        return error;
    }));
}

void Backend::openAsync(const QUrl &url, bool recovery) {
    if (!url.isLocalFile()) { emit failed(tr("Only local files can be opened.")); return; }
    if (!beginOperation(recovery ? tr("Recovering deck…") : tr("Opening deck…"))) return;
    const auto job = m_operationJob;
    const int generation = m_documentGeneration, revision = m_revision;
    struct Result { Bundle::ReadResult read; QString original; };
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job, url, recovery, generation, revision] {
        const auto result = watcher->result(); watcher->deleteLater(); endOperation();
        if (job->canceled) return;
        if (generation != m_documentGeneration || revision != m_revision) {
            emit failed(tr("The deck changed while opening. Your current edits have been kept.")); return;
        }
        if (!result.read.ok) { setStatus(result.read.error); emit failed(result.read.error); return; }
        acceptOpen(result.read.document, recovery ? QUrl() : url);
        if (recovery) {
            m_modified = true;
            setFileUrl(result.original.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.original));
            emit documentChanged();
            Recovery::forget(url.toLocalFile());
            setStatus(tr("Recovered — save to keep your recovered work"));
        }
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [url, recovery, job] {
        Result result;
        if (job->canceled) return result;
        result.read = Bundle::load(url.toLocalFile());
        if (recovery && result.read.ok)
            for (const auto &journal : Recovery::orphans())
                if (journal.journalPath == url.toLocalFile()) result.original = journal.originalPath;
        return result;
    }));
}

void Backend::saveAsync(const QString &path) {
    if (!beginOperation(tr("Saving deck…"))) return;
    const auto document = m_document;
    const auto job = m_operationJob;
    const int generation = m_documentGeneration, revision = m_revision;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job, path, generation, revision] {
        const auto error = watcher->result(); watcher->deleteLater(); endOperation();
        if (job->canceled) { emit saveCanceled(); return; }
        if (!error.isEmpty()) { setStatus(tr("Could not save: %1").arg(error)); emit failed(error); return; }
        rememberRecent(path);
        if (generation != m_documentGeneration) { emit saveCanceled(); return; }
        setFileUrl(QUrl::fromLocalFile(path));
        if (revision != m_revision) {
            setStatus(tr("Saved an earlier version; your newer edits still need saving."));
            emit saveCanceled(); // A pending close/open must not discard newer edits.
            return;
        }
        m_modified = false; m_autosave.stop(); cancelJournal(); Recovery::discard();
        watchFile();
        emit fileStateChanged();
        emit documentChanged();
        setStatus(tr("Saved %1").arg(fileName()));
        emit saved();
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [document, path, job] {
        QString error;
        if (!job->canceled) Workers::write(path, Bundle::toBytes(document), &error, job, true);
        return error;
    }));
}

void Backend::exportPdfAsync(const QString &path, bool stages, bool skipped) {
    if (!beginOperation(tr("Exporting PDF…"))) return;
    auto document = m_document;
    for (auto &slide : document.slides) for (auto &object : slide.objects)
        if (object.type == ObjectType::Media)
            object.mediaReadAllowed = m_mediaPermissions.value(object.mediaPath) == object.mediaId;
    const auto job = m_operationJob;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job, path] {
        const auto error = watcher->result(); watcher->deleteLater(); endOperation();
        if (job->canceled) return;
        if (!error.isEmpty()) { setStatus(error); emit failed(error); }
        else setStatus(tr("Exported %1").arg(QFileInfo(path).fileName()));
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [document, path, stages, skipped, job] {
        Pdf::Options options; options.pagePerBuildStage = stages; options.includeSkipped = skipped;
        QString error;
        Pdf::write(document, path, options, &error, job);
        return error;
    }));
}

bool Backend::insertImageAsync(const QUrl &url) {
    return loadImageAsync(url, false, m_currentSlide, {});
}
bool Backend::loadImageAsync(const QUrl &url, bool replace, int index, const QString &target) {
    if (!m_operation.isEmpty() || m_cancelImages) return false;
    if (!url.isLocalFile() || index < 0 || index >= m_document.slides.size()) {
        emit failed(tr("Choose a local image for the current deck.")); return false;
    }
    // One import at a time preserves drop order and bounds decoded-image memory.
    if (m_imageQueue.size() >= 32) { emit failed(tr("Please wait for the queued pictures to finish.")); return false; }
    m_imageQueue.append({url, replace, m_document.slides.at(index).id, target, m_groupScope, m_documentGeneration});
    startImageImport();
    return true;
}
void Backend::startImageImport() {
    if (m_imageImportRunning || m_imageQueue.isEmpty()) return;
    const auto request = m_imageQueue.takeFirst();
    m_imageImportRunning = true;
    emit operationChanged();
    struct Result { SceneObject image; QString error; };
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, request] {
        const auto result = watcher->result(); watcher->deleteLater();
        commitImageImport(request, result.image, result.error);
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [request] {
        Result result;
        ImageAsset::fromFile(result.image, request.url.toLocalFile(), &result.error);
        return result;
    }));
}
void Backend::commitImageImport(const ImageRequest &request, const SceneObject &image, const QString &error) {
    if (!m_cancelImages && request.generation == m_documentGeneration && m_gestureActive) {
        // Do not mix an import into a drag or text edit's open undo transaction.
        QTimer::singleShot(25, this, [this, request, image, error] { commitImageImport(request, image, error); });
        return;
    }
    m_imageImportRunning = false;
    if (!m_cancelImages && request.generation == m_documentGeneration) {
        if (!error.isEmpty()) emit failed(error);
        else for (int i = 0; i < m_document.slides.size(); ++i)
            if (m_document.slides.at(i).id == request.slide) {
                applyImage(image, request.replace, i, request.target, request.groups);
                break;
            }
    }
    m_cancelImages = false;
    startImageImport();
    emit operationChanged();
}
