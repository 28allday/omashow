#pragma once

// The native document: a `.omashow` zip container of stable-ordered JSON.
//
//   document.json        version, slide size, transition, slide order, sections
//   slides/<id>.json     scene, builds, notes, layout links and local overrides
//   theme.json           palette and font tokens
//   masters/design.json  masters and named placeholder layouts
//   assets/<hash>.png/svg deduplicated embedded pictures and vectors
//   styles.json         reusable object appearances
//
// JSON with sorted keys, one member per slide, and a deterministic container,
// so a deck is diffable in git and a script can write one. Picture data is
// named by its content hash and is never an external file dependency.
//
// The version is written and checked. A file from a newer version is refused
// rather than half-read: losing a user's work to an optimistic parse is worse
// than telling them to update.

#include <QString>

#include "core/scene.h"

namespace Bundle {

// Format 6 adds attached connectors and validated object actions.
// Format 5 adds SVG, native shape/path styles and reusable appearances.
// Format 4 added skipped slides; format 3 added paragraphs and pictures.
// Versions 1/2 retain their original text rendering. Versions 1–3 include all slides.
// Format 7 adds embedded/linked media and timed playback cues.
// Format 8 retains the original media source after optimisation.
// Format 9 adds native tables and cell formatting.
// Format 10 adds native charts with embedded data and axis/series settings.
// Format 11 adds explicit local CSV links with cached data. Opening never reads them.
constexpr int kFormatVersion = 11;

QByteArray toBytes(const Document &document);

struct ReadResult {
    bool ok = false;
    QString error;
    Document document;
};

ReadResult fromBytes(const QByteArray &raw);

// Writes to a temporary file in the same directory, flushes it, then renames
// over the target — so an interrupted save leaves the previous deck intact
// rather than a truncated one.
bool save(const Document &document, const QString &path, QString *error = nullptr);
ReadResult load(const QString &path);

} // namespace Bundle
