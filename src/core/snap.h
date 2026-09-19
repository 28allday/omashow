#pragma once

// Smart guides: the difference between moving a box and laying out a slide.
//
// A pure function, like everything else that decides what things look like —
// given the other objects on the slide and where the dragged one wants to be,
// it returns where it should actually go and which alignments to draw. That
// keeps it testable without a window, and keeps the canvas code to "ask, then
// draw".

#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

struct Slide;

namespace Snap {

struct Guide {
    bool vertical = true;   // a vertical line, i.e. an x alignment
    qreal position = 0.0;   // in document coordinates
    qreal from = 0.0;       // the span to draw, so a guide reaches both objects
    qreal to = 0.0;
};

struct Result {
    QRectF rect;
    QVector<Guide> guides;
};

// `tolerance` is in document units; the canvas passes a constant number of
// screen pixels converted through the current zoom, so snapping feels the same
// however far in you are.
Result adjust(const Slide &slide, const QSizeF &documentSize, const QString &movingId,
              const QRectF &proposed, qreal tolerance, bool keepSize = true);

} // namespace Snap
