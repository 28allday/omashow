#pragma once
#include "core/scene.h"
#include <QVariantMap>

// Bringing design and slides in from another native deck.
//
// Like a layout change, an import answers "what exactly would this do" before
// it touches anything: one call produces both the resulting document and the
// report the dialog shows, so what is applied is what was previewed. Masters,
// fonts and media are resolved here — nothing can be inserted while a typeface
// is absent or a linked film has gone missing without saying what to do about
// it.
namespace DeckImport {

// Design modes: 0 reuse target masters and layouts whose names and placeholder
// roles match, importing only what has no counterpart; 1 always import copies;
// 2 bring no design across and keep each slide's own appearance instead.
enum Mode { MatchByName = 0, Copies = 1, KeepAppearance = 2 };

struct Result {
    Document document;   // the target with the import applied
    QVariantMap plan;    // what the dialog shows: slides, conflicts, fonts, media
    QStringList slideIds;
    QString error;
    bool ok() const { return error.isEmpty() && !plan.value("slides").toList().isEmpty(); }
};

// `options`: design (int mode), theme (bool), dropMissingMedia (bool),
// fonts (family -> replacement family, or "keep"), after (insert index, -1 first).
Result build(const Document &target, const Document &source,
             const QList<int> &slides, const QVariantMap &options);
} // namespace DeckImport
