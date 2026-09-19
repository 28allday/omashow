#include "core/snap.h"

#include "core/scene.h"

#include <cmath>

namespace {

struct Candidate {
    qreal position = 0.0;
    qreal from = 0.0;   // the other object's extent, for drawing the guide
    qreal to = 0.0;
};

void collect(QVector<Candidate> &into, qreal position, qreal from, qreal to) {
    into.append({position, from, to});
}

// Picks the candidate nearest one of the moving edges, and reports which edge
// won so the caller knows how far to shift.
bool bestFor(const QVector<Candidate> &candidates, const QVector<qreal> &edges,
             qreal tolerance, qreal *shift, Candidate *winner) {
    qreal bestDistance = tolerance;
    bool found = false;

    for (const Candidate &candidate : candidates) {
        for (const qreal edge : edges) {
            const qreal distance = std::abs(candidate.position - edge);
            if (distance < bestDistance) {
                bestDistance = distance;
                *shift = candidate.position - edge;
                *winner = candidate;
                found = true;
            }
        }
    }
    return found;
}

} // namespace

Snap::Result Snap::adjust(const Slide &slide, const QSizeF &documentSize,
                          const QString &movingId, const QRectF &proposed,
                          qreal tolerance, bool keepSize) {
    Result result;
    result.rect = proposed;
    if (tolerance <= 0.0)
        return result;

    QVector<Candidate> verticals;    // x positions
    QVector<Candidate> horizontals;  // y positions

    // The slide itself: edges and centres. Centring on the slide is the single
    // most common thing anyone does, so it has to be the easiest.
    collect(verticals, 0.0, 0.0, documentSize.height());
    collect(verticals, documentSize.width() / 2.0, 0.0, documentSize.height());
    collect(verticals, documentSize.width(), 0.0, documentSize.height());
    collect(horizontals, 0.0, 0.0, documentSize.width());
    collect(horizontals, documentSize.height() / 2.0, 0.0, documentSize.width());
    collect(horizontals, documentSize.height(), 0.0, documentSize.width());

    for (const SceneObject &object : slide.objects) {
        if (object.id == movingId)
            continue;
        const QRectF other = object.rect;
        collect(verticals, other.left(), other.top(), other.bottom());
        collect(verticals, other.center().x(), other.top(), other.bottom());
        collect(verticals, other.right(), other.top(), other.bottom());
        collect(horizontals, other.top(), other.left(), other.right());
        collect(horizontals, other.center().y(), other.left(), other.right());
        collect(horizontals, other.bottom(), other.left(), other.right());
    }

    qreal shift = 0.0;
    Candidate winner;

    if (bestFor(verticals, {proposed.left(), proposed.center().x(), proposed.right()},
                tolerance, &shift, &winner)) {
        if (keepSize)
            result.rect.translate(shift, 0.0);
        else
            result.rect.setRight(result.rect.right() + shift);

        Guide guide;
        guide.vertical = true;
        guide.position = winner.position;
        // The guide spans both the object it aligns to and the one being moved,
        // so it reads as a relationship rather than a stray line.
        guide.from = qMin(winner.from, result.rect.top());
        guide.to = qMax(winner.to, result.rect.bottom());
        result.guides.append(guide);
    }

    if (bestFor(horizontals, {proposed.top(), proposed.center().y(), proposed.bottom()},
                tolerance, &shift, &winner)) {
        if (keepSize)
            result.rect.translate(0.0, shift);
        else
            result.rect.setBottom(result.rect.bottom() + shift);

        Guide guide;
        guide.vertical = false;
        guide.position = winner.position;
        guide.from = qMin(winner.from, result.rect.left());
        guide.to = qMax(winner.to, result.rect.right());
        result.guides.append(guide);
    }

    return result;
}
