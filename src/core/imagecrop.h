#pragma once
#include "core/scene.h"
namespace ImageCrop {
QRectF frame(const SceneObject &object);
QRectF fullRect(const SceneObject &object);
SceneObject preview(const SceneObject &object);
// The axis along which Fill box hides part of the crop: the only one its focal
// point can move. None once the crop already has the box's shape.
Qt::Orientations overflow(const SceneObject &object);
bool resize(SceneObject &result, const SceneObject &basis, int handle,
            const QPointF &documentPoint, bool lockAspect = false);
// Reposition the picture inside a box that keeps its size: pan slides it by a
// document-space distance, zoom scales it by factor (>1 shows less) about a
// document point. Both stop at the picture's edges.
bool pan(SceneObject &result, const SceneObject &basis,
         const QPointF &documentDelta);
bool zoom(SceneObject &result, const SceneObject &basis, qreal factor,
          const QPointF &documentAnchor);
} // namespace ImageCrop
