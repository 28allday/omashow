#pragma once

// A build is one animation of one object, placed on a slide's timeline.
//
// The whole architecture rests on one rule: a build describes *what a property
// is at time t*, never *how to animate from now*. Nothing here starts, stops or
// ticks. That is what lets present mode, scrubbing and video export share a
// single evaluator, and what makes the result testable by asserting state at a
// given t.

#include <QEasingCurve>
#include <QString>
#include <QVector>

enum class Effect {
    None,
    Fade,   // opacity 0 -> 1
    Media = 3, // source playback on the same deterministic timeline
    Rise = 2,   // opacity 0 -> 1 while translating up into place
    Move = 4,   // travels in from, or out to, an offset
    Scale = 5,  // grows or shrinks into place
    Spin = 6,   // turns into place
    Pulse = 7,  // emphasis: swells and settles again, in place
    Reveal = 8, // text arriving by paragraph, word or character
};

enum class BuildPhase { In, Out };
enum class BuildTrigger { Absolute, OnClick, WithPrevious, AfterPrevious };

struct BuildStep {
    QString targetId;
    Effect effect = Effect::Fade;
    qreal start = 0.0;       // seconds from the start of the slide
    qreal duration = 0.6;
    QEasingCurve::Type easing = QEasingCurve::OutCubic;

    BuildPhase phase = BuildPhase::In;
    BuildTrigger trigger = BuildTrigger::Absolute;
    qreal delay = 0.0;

    // What the effect needs beyond its timing: how far a Move travels, what a
    // Scale starts at, how far a Spin turns, how much a Pulse swells, and what
    // a Reveal counts as one step.
    qreal amountX = 0, amountY = 0;
    qreal amount = 0;
    int unit = 0;   // Reveal: 0 paragraphs, 1 words, 2 characters

    qreal end() const { return start + duration; }

    // Whether the object is not there yet before this build runs. Media plays
    // in place, and an emphasis acts on something already on the slide.
    bool startsHidden() const {
        return phase == BuildPhase::In && effect != Effect::None &&
               effect != Effect::Media && effect != Effect::Pulse;
    }

    // 0 before the build, 1 after it, eased in between.
    qreal progressAt(qreal t) const;
};

struct Timeline {
    QVector<BuildStep> steps;

    QVector<BuildStep> resolvedSteps() const;
    qreal duration() const;
    QVector<const BuildStep *> stepsFor(const QString &targetId) const;
};
