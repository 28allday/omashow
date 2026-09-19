#include "fixture.h"

#include "core/scene.h"

namespace {

const QColor kPaper(11, 22, 38);
const QColor kInk(231, 237, 247);
const QColor kAccent(39, 194, 255);
const QColor kCard(18, 38, 62);

SceneObject text(const QString &id, const QString &body, const QRectF &rect,
                 qreal size, int weight, const QColor &colour) {
    SceneObject object;
    object.id = id;
    object.type = ObjectType::Text;
    object.text = body;
    object.rect = rect;
    object.fontSize = size;
    object.fontWeight = weight;
    object.textColor = colour;
    return object;
}

SceneObject rect(const QString &id, const QRectF &bounds, const QColor &fill, qreal radius) {
    SceneObject object;
    object.id = id;
    object.type = ObjectType::Rect;
    object.rect = bounds;
    object.fill = fill;
    object.cornerRadius = radius;
    return object;
}

} // namespace

Document Fixture::twoSlideMorph() {
    Document document;
    document.size = QSizeF(1920, 1080);
    document.transitionDuration = 1.0;

    // --- Slide 1 -----------------------------------------------------------
    Slide one;
    one.id = QStringLiteral("slide-1");
    one.background = kPaper;
    one.objects = {
        rect(QStringLiteral("rule"), QRectF(160, 300, 220, 6), kAccent, 3),
        text(QStringLiteral("title"), QStringLiteral("Reimagining\nurban life"),
             QRectF(160, 340, 1100, 300), 112, 700, kInk),
        text(QStringLiteral("subtitle"), QStringLiteral("Designing healthier, more connected cities"),
             QRectF(160, 660, 1000, 70), 40, 400, kInk),
        rect(QStringLiteral("card"), QRectF(1320, 380, 420, 300), kCard, 16),
        text(QStringLiteral("metric"), QStringLiteral("42%"),
             QRectF(1370, 420, 340, 120), 96, 700, kAccent),
        text(QStringLiteral("metric-label"), QStringLiteral("less energy"),
             QRectF(1370, 550, 340, 60), 34, 500, kInk),
    };
    // Builds land in reading order, each picking up where the last settled.
    one.timeline.steps = {
        {QStringLiteral("rule"),         Effect::Fade, 0.00, 0.40, QEasingCurve::OutCubic},
        {QStringLiteral("title"),        Effect::Rise, 0.15, 0.60, QEasingCurve::OutCubic},
        {QStringLiteral("subtitle"),     Effect::Rise, 0.45, 0.60, QEasingCurve::OutCubic},
        {QStringLiteral("card"),         Effect::Rise, 0.80, 0.60, QEasingCurve::OutCubic},
        {QStringLiteral("metric"),       Effect::Fade, 1.10, 0.50, QEasingCurve::OutCubic},
        {QStringLiteral("metric-label"), Effect::Fade, 1.25, 0.50, QEasingCurve::OutCubic},
    };

    // --- Slide 2 -----------------------------------------------------------
    // Same ids, different geometry: this is the "duplicate the slide and move
    // things" workflow Morph exists to serve.
    Slide two;
    two.id = QStringLiteral("slide-2");
    two.background = kPaper;
    two.objects = {
        rect(QStringLiteral("rule"), QRectF(160, 180, 120, 6), kAccent, 3),
        text(QStringLiteral("title"), QStringLiteral("Reimagining\nurban life"),
             QRectF(160, 210, 700, 160), 56, 700, kInk),
        // Same words, new id — identity misses, scoring should catch it.
        text(QStringLiteral("caption-b"), QStringLiteral("Designing healthier, more connected cities"),
             QRectF(160, 380, 700, 70), 32, 400, kInk),
        // The card takes over the slide and the metric grows into it.
        rect(QStringLiteral("card"), QRectF(160, 520, 1600, 380), kCard, 24),
        text(QStringLiteral("metric"), QStringLiteral("42%"),
             QRectF(240, 590, 700, 200), 168, 700, kAccent),
        text(QStringLiteral("metric-label"), QStringLiteral("less energy than conventional cities"),
             QRectF(240, 790, 1200, 70), 40, 500, kInk),
        // On slide 2 only — must fade in, not snap.
        rect(QStringLiteral("badge"), QRectF(1480, 590, 200, 200), kAccent, 100),
    };
    two.timeline.steps = {
        {QStringLiteral("badge"), Effect::Fade, 0.20, 0.50, QEasingCurve::OutCubic},
    };

    document.slides = {one, two};
    return document;
}
