#include "anim/evaluator.h"

#include "core/scene.h"
#include "core/mediaasset.h"
#include "core/connector.h"

namespace {

// How far a Rise build travels, in document units, before settling.
constexpr qreal kRiseDistance = 48.0;

void applyBuild(SceneObject &object, const BuildStep &step, qreal t) {
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
            if (step.targetId == source.id) applyBuild(state, step, t);
        states.append(state);
    }
    Connector::resolve(states);
    return states;
}
