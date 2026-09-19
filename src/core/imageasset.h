#pragma once
#include "core/scene.h"
class QPainter;
namespace ImageAsset {
constexpr qint64 maxPixels = 32 * 1024 * 1024;
constexpr qint64 maxBytes = 64 * 1024 * 1024;
bool fromFile(SceneObject &object, const QString &path,
              QString *error = nullptr);
bool fromImage(SceneObject &object, const QImage &image,
               QString *error = nullptr);
bool decode(SceneObject &object, const QByteArray &bytes,
            QString *error = nullptr);
QSizeF size(const SceneObject &object);
void copyData(SceneObject &target, const SceneObject &source);
QString identity(const QByteArray &bytes);
void paint(QPainter &painter, const SceneObject &object);
QRectF sourceRect(const SceneObject &object);
} // namespace ImageAsset
