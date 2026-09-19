#pragma once

// The one place scene objects become pixels.
//
// The live canvas and the offscreen shot harness both come through here, which
// is what makes "what you rehearsed is what you export" testable rather than
// hopeful. This is also the seam a Skia painter slots behind later — the
// signature takes a QPainter today because QPainter is the reference
// implementation, not because it is the only one.

#include <QColor>
#include <QImage>
#include <QSizeF>
#include <QVector>

class QPainter;
struct SceneObject;

namespace SceneRenderer {

// Paints into a painter already scaled so that one unit == one document unit.
void paint(QPainter &painter, const QVector<SceneObject> &states);

QImage render(const QVector<SceneObject> &states, const QSizeF &documentSize,
              const QSize &pixelSize, const QColor &background);

} // namespace SceneRenderer
