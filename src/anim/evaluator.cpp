#include "anim/evaluator.h"

#include "core/scene.h"
#include "core/mediaasset.h"
#include "core/connector.h"
#include "core/deckresize.h"
#include "core/shape.h"

#include <QTransform>

#include <cmath>

namespace {

// How far a Rise build travels, in document units, before settling.
constexpr qreal kRiseDistance = 48.0;

// Where each paragraph, word or character of a text ends. A Reveal shows the
// text up to one of these, so nothing is ever half a letter wide.
QVector<int> unitEnds(const QString &text, int unit) {
    QVector<int> ends;
    if (unit == 2) {
        for (int i = 1; i <= text.size(); ++i) ends.append(i);
        return ends;
    }
    bool inWord = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (unit == 0) {
            if (c == QLatin1Char('\n')) ends.append(i);
        } else if (c.isSpace()) {
            if (inWord) { ends.append(i); inWord = false; }
        } else {
            inWord = true;
        }
    }
    if (ends.isEmpty() || ends.last() != text.size()) ends.append(text.size());
    return ends;
}

void applyBuild(SceneObject &object, const BuildStep &step, qreal t, const Slide &slide) {
    const qreal progress = step.progressAt(t);
    const qreal p = step.phase == BuildPhase::Out ? 1.0 - progress : progress;
    switch (step.effect) {
    case Effect::Media:
        if(object.type==ObjectType::Media) MediaAsset::evaluate(object,t-step.start,step.duration);
        break;
    case Effect::None:
        break;
    case Effect::Fade:
        object.opacity *= p;
        break;
    case Effect::Rise:
        object.opacity *= p;
        object.rect.translate(0.0, kRiseDistance * (1.0 - p) * (step.phase == BuildPhase::Out ? -1.0 : 1.0));
        break;
    case Effect::Move:
        object.rect.translate(step.amountX * (1.0 - p), step.amountY * (1.0 - p));
        break;
    case Effect::Scale: {
        const qreal from = step.amount > 0 ? step.amount : 0.5;
        DeckResize::scaleObject(object, from + (1.0 - from) * p, object.rect.center());
        break;
    }
    case Effect::Spin:
        object.rotation += (step.amount != 0 ? step.amount : 180.0) * (1.0 - p);
        break;
    case Effect::Pulse: {
        // An emphasis is not an entrance: it swells and settles where it is.
        const qreal swell = step.amount != 0 ? step.amount : 0.15;
        const qreal q = std::sin(M_PI * qBound(0.0, progress, 1.0));
        DeckResize::scaleObject(object, 1.0 + swell * q, object.rect.center());
        break;
    }
    case Effect::Path: {
        const auto *guide = slide.find(step.pathId);
        if (!guide || guide->id == object.id) break;
        const auto path = Shape::worldPath(*guide);
        if (path.isEmpty() || path.length() <= 0) break;
        const qreal along = qBound(0.0, step.pathReverse ? 1.0 - p : p, 1.0);
        const qreal landing = step.pathReverse ? 0.0 : 1.0;
        // The object lands where it was authored: the path says how it gets
        // there, not where "there" is.
        const QPointF offset = object.rect.center() - path.pointAtPercent(landing);
        object.rect.moveCenter(path.pointAtPercent(along) + offset);
        if (step.orient) object.rotation -= path.angleAtPercent(along);
        break;
    }
    case Effect::Reveal: {
        if (object.type != ObjectType::Text) break;
        const auto ends = unitEnds(object.text, qBound(0, step.unit, 2));
        const int shown = int(std::ceil(p * ends.size() - 1e-9));
        object.text = shown <= 0 ? QString()
                                 : object.text.left(ends.at(qMin(shown, int(ends.size())) - 1));
        break;
    }
    }
}

} // namespace

QVector<SceneObject> Evaluator::stateAt(const Slide &slide, qreal t) {
    QVector<SceneObject> states;
    states.reserve(slide.objects.size());

    const auto builds = slide.timeline.resolvedSteps();
    for (const SceneObject &source : slide.objects) {
        SceneObject state = source;
        for (const auto &step : builds)
            if (step.targetId == source.id) applyBuild(state, step, t, slide);
        states.append(state);
    }
    Connector::resolve(states);
    return states;
}
