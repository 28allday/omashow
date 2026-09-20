#include "core/workers.h"
#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

QThreadPool *Workers::io() {
    static QThreadPool pool;
    static const bool configured = [] { pool.setMaxThreadCount(2); return true; }();
    Q_UNUSED(configured);
    return &pool;
}
QThreadPool *Workers::preview() {
    static QThreadPool pool;
    static const bool configured = [] { pool.setMaxThreadCount(1); return true; }();
    Q_UNUSED(configured);
    return &pool;
}
QThreadPool *Workers::video() {
    static QThreadPool pool;
    static const bool configured = [] { pool.setMaxThreadCount(2); return true; }();
    Q_UNUSED(configured);
    return &pool;
}
void Workers::shutdown() {
    video()->waitForDone();
    preview()->waitForDone();
    io()->waitForDone();
    QThreadPool::globalInstance()->waitForDone();
}
void Workers::Job::cancel() {
    QMutexLocker lock(&commitMutex);
    canceled = true;
}
bool Workers::write(const QString &path, const QByteArray &bytes, QString *error,
                    const std::shared_ptr<Job> &job, bool keepPrevious) {
    if (job && job->canceled) return false;
    // The version being replaced is kept beside it, so one mistaken save is
    // always recoverable by hand. Failing to keep it never fails the save.
    if (keepPrevious && QFileInfo::exists(path)) {
        const QString backup = path + QStringLiteral(".bak");
        QFile::remove(backup);
        QFile::copy(path, backup);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    // Cancellation and rename are ordered. A discarded journal can never
    // reappear after the caller removes it, even while compression finishes.
    QMutexLocker lock(job ? &job->commitMutex : nullptr);
    if (job && job->canceled) { file.cancelWriting(); return false; }
    if (!file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
