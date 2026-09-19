#include "io/recents.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
namespace {
QString absolute(const QString &file) { return QFileInfo(file).absoluteFilePath(); }
QVariantList entries(const QString &path) {
    QSettings s(path, QSettings::IniFormat);
    return s.value("files").toList();
}
void write(const QString &path, QVariantList list) {
    std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("pinned").toBool() > b.toMap().value("pinned").toBool();
    });
    int unpinned = 0;
    for (int i = 0; i < list.size();) {
        if (!list.at(i).toMap().value("pinned").toBool() && ++unpinned > 12)
            list.removeAt(i);
        else
            ++i;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings s(path, QSettings::IniFormat);
    s.setValue("files", list);
    s.sync();
}
} // namespace
QString Recents::settingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/recent.ini";
}
QVariantList Recents::read(const QString &path) {
    QVariantList result;
    for (const auto &v : entries(path)) {
        auto m = v.toMap();
        const QFileInfo info(m.value("path").toString());
        m["name"] = info.fileName();
        m["missing"] = !info.isFile();
        result.append(m);
    }
    return result;
}
void Recents::remember(const QString &settings, const QString &file) {
    auto list = entries(settings);
    const QString path = absolute(file);
    bool pinned = false;
    for (int i = list.size() - 1; i >= 0; --i)
        if (list.at(i).toMap().value("path") == path) {
            pinned = list.at(i).toMap().value("pinned").toBool();
            list.removeAt(i);
        }
    list.prepend(QVariantMap{{"path", path}, {"pinned", pinned}});
    write(settings, list);
}
void Recents::pin(const QString &settings, const QString &file, bool pinned) {
    auto list = entries(settings);
    for (auto &v : list) {
        auto m = v.toMap();
        if (m.value("path") == absolute(file)) {
            m["pinned"] = pinned;
            v = m;
        }
    }
    write(settings, list);
}
void Recents::remove(const QString &settings, const QString &file) {
    auto list = entries(settings);
    for (int i = list.size() - 1; i >= 0; --i)
        if (list.at(i).toMap().value("path") == absolute(file))
            list.removeAt(i);
    write(settings, list);
}
