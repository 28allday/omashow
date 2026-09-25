#pragma once

// Opening decks that were not written by OmaShow.
//
// A `.pptx` (PowerPoint, Google Slides, LibreOffice) or a `.key` (Keynote)
// opens like any other deck, but what arrives is a new, unsaved OmaShow deck:
// it never writes back to the foreign file, and what it could not bring across
// is said, not hidden. The readers live in `pptx.*` and `keynote.*`; this is
// the one place that decides which of them a path needs.

#include <QStringList>

#include "core/scene.h"

namespace Interchange {

enum Kind { Native, PowerPoint, Keynote, Unknown };

Kind kindOf(const QString &path);
QString kindName(Kind kind);
inline bool isForeign(Kind kind) { return kind == PowerPoint || kind == Keynote; }

struct Result {
    bool ok = false;
    QString error;
    Document document;
    Kind kind = Unknown;
    // What was left out or approximated, one line each, deduplicated.
    QStringList warnings;
};

// Native decks come back exactly as Bundle::load would give them. Foreign
// decks come back converted; `kind` says which reader answered.
Result load(const QString &path);
Result fromBytes(const QByteArray &raw, Kind kind);

// For file choosers: every pattern OmaShow can open, native first.
QStringList openPatterns();

// The name a converted deck should be saved under: the source name with the
// native suffix.
QString suggestedName(const QString &path);

} // namespace Interchange
