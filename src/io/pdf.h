#pragma once

// PDF export.
//
// Real text, not outlines: QPainter on a QPdfWriter emits glyphs, so the result
// is searchable, selectable and copyable. Flattening a deck to images is the
// easy way out and produces a file nobody can quote from.
//
// The slide is drawn by the same renderer as the canvas and the show, at the
// same settled time — so the handout matches what the audience saw.

#include <QString>

#include "core/scene.h"

namespace Pdf {

struct Options {
    int from = 0;              // slide index, inclusive
    int to = -1;               // -1 means to the end
    // A page per build stage instead of one per slide: the handout mode the
    // competing products do badly, and it is nearly free here because the
    // evaluator can be asked for any moment.
    bool pagePerBuildStage = false;
    bool includeSkipped = false;
    qreal pageWidthPoints = 960.0;   // 13.33in — the usual widescreen deck
};

bool write(const Document &document, const QString &path,
           const Options &options = {}, QString *error = nullptr);

// The times each slide contributes, in order: one settled time normally, or
// one per build stage when asked.
QVector<qreal> stageTimes(const Slide &slide, bool pagePerBuildStage);

} // namespace Pdf
