#pragma once

// PDF export.
//
// Real text, not outlines: QPainter on a QPdfWriter emits glyphs, so the result
// is searchable, selectable and copyable. Flattening a deck to images is the
// easy way out and produces a file nobody can quote from.
//
// The slide is drawn by the same renderer as the canvas and the show, at the
// same settled time — so the handout matches what the audience saw.

#include <QSizeF>
#include <QString>
#include <QVector>
#include <functional>

#include "core/scene.h"
#include "core/workers.h"

class QPainter;

namespace Pdf {

// What the pages are: the slides themselves, a slide with its notes under it,
// the deck as an outline, or several slides to a sheet.
enum Layout { Slides = 0, Notes = 1, Outline = 2, Handout = 3 };

struct Options {
    int from = 0;              // slide index, inclusive
    int to = -1;               // -1 means to the end
    QVector<int> indices;      // when given, exactly these slides in this order (a custom show)
    // A page per build stage instead of one per slide: the handout mode the
    // competing products do badly, and it is nearly free here because the
    // evaluator can be asked for any moment.
    bool pagePerBuildStage = false;
    bool includeSkipped = false;
    qreal pageWidthPoints = 960.0;   // 13.33in — the usual widescreen deck
    int layout = Slides;
    int perPage = 2;                 // handouts: 2, 3, 4, 6 or 9 slides a sheet
};

bool write(const Document &document, const QString &path,
           const Options &options = {}, QString *error = nullptr,
           const std::shared_ptr<Workers::Job> &job = {});

// The times each slide contributes, in order: one settled time normally, or
// one per build stage when asked.
QVector<qreal> stageTimes(const Slide &slide, bool pagePerBuildStage);

// The page, in points: the slide's own shape, or A4 for notes, outlines and
// handouts.
QSizeF pageSize(const Document &document, const Options &options);
// How many pages this would be, or 0 for an outline, which flows.
int pageCount(const Document &document, const Options &options);

// Draws every page onto an open painter, asking for the next page as it goes.
// Shared by the PDF writer and the printer so both put down the same ink.
bool paint(QPainter &painter, const Document &document, const Options &options,
           const QSizeF &page, const std::function<bool()> &newPage,
           const std::shared_ptr<Workers::Job> &job = {}, QString *error = nullptr);

} // namespace Pdf
