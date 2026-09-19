#pragma once

// The evaluator: (slide, t) -> what you see.
//
// A pure function. No clock, no state, no side effects. Present mode calls it
// from a monotonic frame clock, the scrubber calls it with a dragged value, and
// video export calls it at exact frame intervals — and because it is the same
// function, the exported file matches what was rehearsed.

#include <QVector>

struct Slide;
struct SceneObject;

namespace Evaluator {

// Objects with no build on them are visible from t = 0. Objects with builds are
// hidden until their first build starts. Returned in draw order.
QVector<SceneObject> stateAt(const Slide &slide, qreal t);

} // namespace Evaluator
