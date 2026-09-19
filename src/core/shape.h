#pragma once
#include "core/scene.h"
#include <QPainterPath>
#include <QVariantList>
class QPainter;
namespace Shape {
constexpr int custom = 99;
QStringList names();
QPainterPath path(const SceneObject &object);
QPainterPath worldPath(const SceneObject &object);
bool contains(const SceneObject &object, const QPointF &localPoint);
void paint(QPainter &painter, const SceneObject &object);
QVariantList encode(const QPainterPath &path);
bool decode(const QVariantList &data, QPainterPath *path = nullptr);
bool assignPath(SceneObject &object, const QPainterPath &worldPath);
QVector<SceneObject> combine(const QVector<SceneObject> &objects,
                             int operation);
} // namespace Shape
