#include "core/hardwaredecode.h"
#include <QDir>
#include <QRegularExpression>

QVector<HardwareDecode::Candidate> HardwareDecode::candidates(
        const QStringList &codecBackends, const QStringList &nodes, int cudaDevices,
        const QString &preference, const QString &device) {
    QVector<Candidate> result;
    auto mode = preference.trimmed().toLower();
    if (mode.isEmpty()) mode = "auto";
    const bool renderNode = QRegularExpression("^/dev/dri/renderD[0-9]+$").match(device).hasMatch();
    const bool cudaIndex = QRegularExpression("^[0-9]+$").match(device).hasMatch();
    if (mode == "auto" && !device.isEmpty()) mode = renderNode ? "vaapi" : cudaIndex ? "cuda" : "software";
    if ((mode == "auto" || mode == "vaapi") && codecBackends.contains("vaapi")) {
        if (!device.isEmpty()) {
            if (renderNode) result.append(Candidate{"vaapi", device});
        } else {
            // Probe every render node: renderD128 is not necessarily the GPU
            // with a working video driver, particularly on hybrid laptops.
            for (const auto &node : nodes) result.append(Candidate{"vaapi", node});
            if (nodes.isEmpty()) result.append(Candidate{"vaapi", {}});
        }
    }
    if ((mode == "auto" || mode == "cuda") && codecBackends.contains("cuda")) {
        if (!device.isEmpty()) {
            if (cudaIndex) result.append(Candidate{"cuda", device});
        } else {
            result.append(Candidate{"cuda", {}});
            for (int index = 1; index < cudaDevices; ++index) result.append(Candidate{"cuda", QString::number(index)});
        }
    }
    result.append(Candidate{"software", {}});
    return result;
}

int HardwareDecode::firstWorking(const QVector<Candidate> &plan,
                                 const std::function<bool(const Candidate &)> &attempt) {
    for (int i = 0; i < plan.size(); ++i) if (attempt(plan[i])) return i;
    return -1;
}

QStringList HardwareDecode::renderNodes() {
    QStringList nodes;
    const QDir directory("/dev/dri");
    const QRegularExpression name("^renderD[0-9]+$");
    for (const auto &entry : directory.entryList({"renderD*"}, QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name))
        if (name.match(entry).hasMatch()) nodes.append(directory.filePath(entry));
    return nodes;
}
int HardwareDecode::cudaDeviceCount() {
    int count = 0;
    const QRegularExpression name("^nvidia[0-9]+$");
    for (const auto &entry : QDir("/dev").entryList({"nvidia*"}, QDir::Files | QDir::System | QDir::NoDotAndDotDot))
        if (name.match(entry).hasMatch()) ++count;
    return count;
}
