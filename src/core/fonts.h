#pragma once

// Typefaces a deck asks for, and what to do when this computer lacks them.
//
// A deck from elsewhere names its fonts; it cannot bring them. Rather than
// leave the system to substitute silently, the deck's families are listed,
// the ones not installed are named, and for each a replacement is suggested:
// the same family without a weight in its name when that is installed
// ("Inter SemiBold" → "Inter"), else the installed face nearest in kind —
// serif for a serif, monospace for a monospace, sans for the rest.
// Substitution rewrites every place a family is named: text boxes and the
// stretches within them, masters, layouts, named styles, table cells and the
// theme's own heading and body fonts.

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "core/scene.h"

namespace Fonts {

// Family → how many places name it, over the whole deck.
QMap<QString, int> families(const Document &document);

// The families the deck names that are not installed here.
QStringList missing(const Document &document);

// An installed family to stand in for one that is not, never empty.
QString suggested(const QString &family);

// For the sheet: {family, uses, suggested} for every missing family.
QVariantList report(const Document &document);

// family → replacement, applied everywhere. Returns how many names changed.
int substitute(Document &document, const QMap<QString, QString> &replacements);

} // namespace Fonts
