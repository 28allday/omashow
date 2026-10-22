#pragma once

// Office typefaces on PowerPoint import, and back again on PowerPoint save.
//
// A deck made in PowerPoint names Windows and Office fonts (Calibri, Aptos,
// Cambria…) that a Linux computer does not have. On import each one that is
// not installed is swapped for its measured free match: the table in
// :/fonts/office-font-map.json (office-font-map, CC BY-SA 4.0)
// lists for every Office family up to three free families, metric-compatible
// ones first, with the size, width and line factors measured against the
// Office font. The first installed option is used. When it is not
// metric-compatible its size factor is applied, so it looks as big as the
// Office font did; a metric-compatible match keeps the size, so lines break
// where PowerPoint breaks them.
//
// Nothing is lost: each box, stretch, table cell and theme slot keeps the
// Office family it named, the size it had, the factor applied and any weight
// set from the family's name (origFont, origFontSize, fontScale, origWeight),
// and saving as PowerPoint writes those back exactly (revert). A free family
// chosen in OmaShow that is metric-compatible with an Office family is saved
// under the Office name (Carlito as Calibri), and is then not embedded.

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "core/scene.h"

namespace FontSwap {

struct Option {
    QString family;
    bool metric = false;           // same advance widths at the same size
    qreal size = 1, width = 1, line = 1;
    QString package, googleFonts, github;   // where it can be installed from
    bool fallback = false;         // not in the table: an installed font measured here
};

// When none of an Office family's free matches is installed: the installed
// font drawn instead (fontconfig's choice for the family's generic class),
// with size and line factors measured here, at runtime, against the Office
// family's stored metrics: size = the Office font's kerned advance over an
// English sample (map "aw", per style) ÷ the fallback's over the same text,
// snapped to 1 within 0.3%; line factor from the GDI+ line height. Null for a
// family the table has no width for.
// style: 0 regular, 1 bold, 2 italic (widths are measured per style).
const Option *fallback(const QString &officeFamily, int style = 0);
int styleOf(int weight, bool italic);

// The free matches of an Office family, best first; empty when the table does
// not know the family.
QVector<Option> options(const QString &officeFamily);
// The weight an Office family carries in its name ("Segoe UI Semibold" 600),
// 0 when it carries none.
int namedWeight(const QString &officeFamily);
// Whether the table knows this Office family.
bool isOffice(const QString &family);
// The Office family a free family is metric-compatible with, for saving a
// family chosen in OmaShow; empty when there is none.
QString officeFor(const QString &freeFamily);

struct Report {
    int changed = 0;               // places a family name was swapped
    QVariantList swaps;            // {office, free, metric, size, uses}
};
// Swap every Office family the deck names that is not installed here for its
// first installed free match. Families already swapped are left alone.
Report apply(Document &document);

// Undo apply() for a copy of the deck about to be written as PowerPoint.
void revert(Document &document);

// How a box drawn in its free stand-in must be laid out so its lines sit where
// the Office family's would: the line height multiplied by `line`, and the
// text moved up by `shift` (pixels at `size`, the size it is drawn at). From
// the Office family's ascent/descent/gap in the table and the free family's as
// Qt measures it; metric-compatible matches only. Drawing only: the deck keeps its own line height, so nothing
// needs reversing on save. {1, 0} for a box that is not swapped.
struct VerticalFix { qreal line = 1; qreal shift = 0; };
VerticalFix verticalFix(const SceneObject &o, qreal size);

// What has been swapped in this deck: {office, free, metric, size, uses}.
QVariantList swapped(const Document &document);

} // namespace FontSwap
