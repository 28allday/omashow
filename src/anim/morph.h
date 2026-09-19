#pragma once

// Morph — the match transition.
//
// Two slides, and the objects that are "the same thing" on both, interpolated
// instead of cross-faded. Matching runs in three tiers, in this order:
//
//   1. Identity   — the same object id appears on both slides. Duplicating a
//                   slide preserves ids, and "duplicate the slide and move
//                   things" is how the transition is actually used, so this
//                   tier carries the overwhelming majority of real pairings.
//   2. Scored     — for what identity missed: same type, same text, similar
//                   size and fill. Greedy above a threshold.
//   3. Unmatched  — everything left fades out or in.
//
// Tier 3 is not a failure case, it is the honest one. What matters is that a
// user can see which tier produced a pair and override it — that part is the
// Animate workspace's job, and this file gives it the data to draw.

#include <QString>
#include <QVector>

struct Slide;
struct SceneObject;

enum class MatchKind {
    Identity,
    Scored,
    FadeOut,   // present on the outgoing slide only
    FadeIn,    // present on the incoming slide only
};

struct MorphPair {
    QString fromId;
    QString toId;
    MatchKind kind = MatchKind::Identity;
    qreal score = 1.0;
};

namespace Morph {

// Below this, a scored candidate is not a pair — it fades instead. A wrong
// match looks far worse than an honest cross-fade.
constexpr qreal kScoreThreshold = 0.55;

qreal scorePair(const SceneObject &from, const SceneObject &to);

QVector<MorphPair> match(const Slide &from, const Slide &to);

// u runs 0..1 across the transition. Same contract as the evaluator: pure.
QVector<SceneObject> stateAt(const Slide &from, const Slide &to,
                             const QVector<MorphPair> &pairs, qreal u);

} // namespace Morph
