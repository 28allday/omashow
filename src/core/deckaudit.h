#pragma once
#include "core/scene.h"
#include <QVariantMap>

// Template hygiene: what a deck is carrying that nothing uses.
//
// A deck that has been through a few themes and layout changes collects masters
// no layout points at, layouts no slide points at, empty sections and originals
// kept from optimising a picture or a film. The audit names each one, says what
// removing it would give back, and refuses to remove anything that is still in
// use — the report is a claim about the current document, so a stale selection
// takes nothing with it.
namespace DeckAudit {

// "rows" of {id, kind, name, detail, bytes}, plus totals.
QVariantMap report(const Document &document);

// Removes exactly the rows named, in dependency order, and only while each one
// is genuinely unused. Anything else leaves the document untouched.
bool remove(Document &document, const QStringList &ids);
} // namespace DeckAudit
