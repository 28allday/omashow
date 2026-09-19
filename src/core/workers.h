#pragma once
#include <QMutex>
#include <QThreadPool>
#include <QString>
#include <atomic>
#include <memory>

namespace Workers {
// Compression/import/export have their own small pool so that a large deck
// cannot occupy all cores or queue behind live video work.
QThreadPool *io();
QThreadPool *preview();
QThreadPool *video();
void shutdown();
// Declare after QGuiApplication, before workers/engines. This joins workers
// and destroys their thread-local decoder caches before CUDA/Qt teardown.
struct Session { ~Session() { shutdown(); } };
struct Job {
    std::atomic_bool canceled{false};
    QMutex commitMutex;
    void cancel();
};
bool write(const QString &path, const QByteArray &bytes, QString *error,
           const std::shared_ptr<Job> &job = {});
}
