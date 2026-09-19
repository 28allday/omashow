#pragma once
#include "core/scene.h"
namespace ImageCrop {
QRectF frame(const SceneObject &object);
QRectF fullRect(const SceneObject &object);
SceneObject preview(const SceneObject &object);
bool resize(SceneObject &result, const SceneObject &basis, int handle,
            const QPointF &documentPoint, bool lockAspect = false);
} // namespace ImageCrop
