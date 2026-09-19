#include "anim/build.h"

#include <algorithm>

qreal BuildStep::progressAt(qreal t) const {
    if (duration <= 0.0)
        return t >= start ? 1.0 : 0.0;
    if (t <= start)
        return 0.0;
    if (t >= end())
        return 1.0;
    return QEasingCurve(easing).valueForProgress((t - start) / duration);
}

qreal Timeline::duration() const {
    qreal last = 0.0;
    for (const BuildStep &step : resolvedSteps())
        last = std::max(last, step.end());
    return last;
}

QVector<const BuildStep *> Timeline::stepsFor(const QString &targetId) const {
    QVector<const BuildStep *> found;
    for (const BuildStep &step : steps) {
        if (step.targetId == targetId)
            found.append(&step);
    }
    return found;
}

QVector<BuildStep> Timeline::resolvedSteps() const {
    QVector<BuildStep> resolved;
    qreal previousStart = 0, previousEnd = 0, groupEnd = 0;
    for (auto step : steps) {
        switch (step.trigger) {
        case BuildTrigger::Absolute: break;
        case BuildTrigger::WithPrevious: step.start = previousStart + step.delay; break;
        case BuildTrigger::AfterPrevious: step.start = previousEnd + step.delay; break;
        case BuildTrigger::OnClick:
            // Deterministic preview/export schedule. The presenter pauses at
            // this boundary until the speaker advances the next click group.
            step.start = groupEnd + step.delay; break;
        }
        previousStart = step.start; previousEnd = step.end();
        groupEnd = std::max(groupEnd, previousEnd);
        resolved.append(step);
    }
    return resolved;
}
