#pragma once
#include <QStringList>
#include <QVector>
#include <functional>

namespace HardwareDecode {
struct Candidate {
    QString backend; // vaapi, cuda, or software; never a GPU vendor name
    QString device;
    QString key() const { return backend + ':' + (device.isEmpty() ? QStringLiteral("default") : device); }
};

// Pure selection policy, also exercised without physical GPU dependencies.
// Device creation AND decoding must succeed before a candidate is accepted.
QVector<Candidate> candidates(const QStringList &codecBackends, const QStringList &renderNodes,
                             int cudaDevices, const QString &preference = QStringLiteral("auto"),
                             const QString &device = {});
int firstWorking(const QVector<Candidate> &candidates,
                 const std::function<bool(const Candidate &)> &attempt);
QStringList renderNodes();
int cudaDeviceCount();
}
