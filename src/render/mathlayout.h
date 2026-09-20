#pragma once

// Equations, drawn with the same painter as everything else.
//
// A text box set to maths keeps what was typed — the notation people already
// write by hand: ^ and _ for scripts, \frac, \sqrt, \sum, Greek letters and the
// usual operators. Nothing is rasterised: an equation is glyphs and rules, so
// it is as sharp in an exported PDF as it is on the canvas, it follows the
// box's typeface and colour, and it can still be edited as the letters that
// made it.
//
// What cannot be understood is said in words, and the box falls back to drawing
// the source as ordinary text rather than showing nothing.

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

class QPainter;

namespace MathLayout {

struct Glyph {
    QString text;
    QPointF origin;       // the left of the glyph, on its own baseline
    qreal size = 0;       // pixels, so scripts are smaller than what they follow
    bool italic = false;  // single letters lean; numbers and operators do not
    qreal stretchY = 1;   // grown brackets and radical signs
};

struct Rendered {
    bool ok = false;
    QString error;            // what stopped it, in words, when ok is false
    QSizeF size;              // the ink, measured from its own top left
    QVector<Glyph> glyphs;
    QVector<QRectF> rules;    // fraction bars, radical tops, overlines
};

// `base` carries the box's typeface and weight; `size` is the type size the box
// is being drawn at. `align` is the box's own alignment, which decides where
// shorter lines sit.
Rendered build(const QString &source, const QFont &base, qreal size, int align = 0);
void paint(QPainter &painter, const Rendered &rendered, const QPointF &topLeft,
           const QFont &base, const QColor &color);
} // namespace MathLayout
