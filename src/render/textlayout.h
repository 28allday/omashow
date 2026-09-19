#pragma once
#include "core/scene.h"
class QPainter;
namespace TextLayout {
struct Metrics {
    qreal naturalHeight = 0, renderedHeight = 0, effectiveSize = 0;
    bool overflow = false;
};
QString editorHtml(const SceneObject &object);
Metrics measure(const SceneObject &object);
void paint(QPainter &painter, const SceneObject &object);
} // namespace TextLayout
