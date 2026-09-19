#pragma once
#include "core/scene.h"
namespace Arrange {
struct Unit {
  QString key;
  QStringList ids;
  QRectF rect;
};
bool inScope(const SceneObject &object, const QStringList &scope);
QString unitKey(const SceneObject &object, const QStringList &scope);
QVector<Unit> units(const Slide &slide, const QStringList &ids,
                    const QStringList &scope);
QRectF visualBounds(const Slide &slide, const QStringList &ids);
QRectF bounds(const Slide &slide, const QStringList &ids);
void setRect(SceneObject &object, const QRectF &before, const QRectF &after);
} // namespace Arrange
