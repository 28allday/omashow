#pragma once

// Stretches of text that look different from the box around them.
//
// Runs are stored against the authored string, never against the laid-out one,
// so wrapping, list indents and uppercase display cannot move them. They are
// kept sorted, non-overlapping and inside the text; anything that would break
// that is refused rather than half-applied.

#include "core/scene.h"
#include <QVariantList>

namespace TextRuns {

// Sorted, clipped to the text, merged where they touch and identical, with
// empty ones dropped. What every path stores.
QVector<TextRun> tidy(QVector<TextRun> runs, int length);

// The run covering a position, or nothing.
const TextRun *at(const QVector<TextRun> &runs, int position);

// Applies one property over [start, end) — "weight", "italic", "underline",
// "strike", "baseline", "fontSize", "fontFamily" or "color" — splitting and
// merging as needed. An empty/zero/invalid value hands the stretch back to the
// box. Returns false if the range or the value is no good.
bool apply(SceneObject &object, int start, int end, const QString &key, const QVariant &value);

// Text edited by hand: runs follow the characters that survived.
QVector<TextRun> afterEdit(const QVector<TextRun> &runs, const QString &before,
                           const QString &after);

QVariantList encode(const QVector<TextRun> &runs);
bool decode(const QVariant &value, QVector<TextRun> &runs);
} // namespace TextRuns
