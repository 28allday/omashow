#pragma once

// Document edits, as free functions.
//
// Kept out of Backend so the tests can drive real editing without a window, an
// engine or a desktop — the same reason the evaluator is a pure function.
// Backend's job is only to wrap these in undo and tell QML something changed.

#include <QString>

#include "core/scene.h"

namespace Edit {

QString newId(const QString &prefix);

int addSlide(Document &document, int afterIndex);

// Duplicating keeps every object id. That is not an oversight: identical ids
// across two slides are exactly what Morph matches on, and "duplicate the slide
// then move things" is the workflow the transition exists to serve.
int duplicateSlide(Document &document, int index);

bool deleteSlide(Document &document, int index);
bool moveSlide(Document &document, int from, int to);

QString addText(Document &document, int slideIndex, const QPointF &centre);
QString addRect(Document &document, int slideIndex, const QPointF &centre);
bool deleteObject(Document &document, int slideIndex, const QString &id);
bool raiseObject(Document &document, int slideIndex, const QString &id, int delta);

} // namespace Edit
