#pragma once
#include "core/scene.h"
#include <QVariantMap>

// Finding words across a deck, and putting different ones in their place.
//
// A replacement never touches anything but the characters it matched: the box
// keeps its formatting, a placeholder keeps its link to the layout while
// recording that its words are the author's own, and a stale match — one whose
// text has changed since it was found — is refused rather than applied blind.
namespace FindReplace {

// `options`: caseSensitive (bool), wholeWords (bool), notes (bool),
// slides (QStringList of slide ids; empty means the whole deck).
// Rows: {slide, slideId, objectId, cellId, notes, start, length, context, label}.
QVariantList find(const Document &document, const QString &needle, const QVariantMap &options);

bool replaceOne(Document &document, const QVariantMap &match, const QString &needle,
                const QString &replacement, const QVariantMap &options);
int replaceAll(Document &document, const QString &needle, const QString &replacement,
               const QVariantMap &options);
} // namespace FindReplace
