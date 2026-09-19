#pragma once
#include "core/scene.h"
#include <QPainterPath>
namespace Connector {
QPointF anchor(const SceneObject &object, int side, const QPointF &toward);
void resolve(QVector<SceneObject> &objects);
QPainterPath arrows(const SceneObject &object);
void detachTarget(Slide &slide, const Slide &resolved, const QString &targetId);
} // namespace Connector
