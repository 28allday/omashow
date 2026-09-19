#pragma once
#include <QString>
#include <QVariantList>
namespace Recents {
QString settingsPath();
QVariantList read(const QString &path);
void remember(const QString &settings, const QString &file);
void pin(const QString &settings, const QString &file, bool pinned);
void remove(const QString &settings, const QString &file);
} // namespace Recents
