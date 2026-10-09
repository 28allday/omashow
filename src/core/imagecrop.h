#pragma once
#include "core/scene.h"
class QPainterPath;
namespace ImageCrop {
// While cropping, the part the crop cuts off stays visible around the frame,
// 40% darker and 30% less saturated, so it reads as "outside". At 80% of the
// picture's own opacity, so what lies behind it shows through a little.
inline constexpr qreal cutDarken = .4, cutSaturation = .7, cutOpacity = .8;
QRectF frame(const SceneObject &object);
QRectF fullRect(const SceneObject &object);
SceneObject preview(const SceneObject &object);
// The cut-off part in document coordinates: the whole picture minus the kept frame.
QPainterPath cutArea(const SceneObject &object);
bool resize(SceneObject &result, const SceneObject &basis, int handle,
            const QPointF &documentPoint, bool lockAspect = false);
} // namespace ImageCrop
