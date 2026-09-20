#pragma once
#include "core/scene.h"
namespace DeckResize {
bool apply(Document &document, const QSizeF &size, bool scaleContent);
// One object scaled about a point, type size, strokes, shadows and table
// geometry included. Shared with the animation evaluator so a scaled object
// looks the same whether the deck was resized or the object is mid-build.
void scaleObject(SceneObject &object, qreal scale, const QPointF &origin = QPointF());
}
