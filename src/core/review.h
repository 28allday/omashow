#pragma once
#include "core/scene.h"
#include <QVariantMap>

// Reviewing a deck: comments, what the deck actually contains, and what would
// stop someone reading it.
//
// Findings are keyed by what they are about rather than by position, so
// dismissing one ("that is not a problem") survives editing, reordering and
// reopening — and comes back if the thing itself changes.
namespace Review {

// --- comments ------------------------------------------------------------
// Threads in document order: each row is a comment with its replies under it.
QVariantList threads(const Document &document, const QString &slideId = QString(),
                     bool includeResolved = true);
QString add(Document &document, const QString &slideId, const QString &objectId,
            const QString &author, const QString &text, const QString &parentId = QString(),
            const QString &created = QString());
bool setText(Document &document, const QString &id, const QString &text);
bool setResolved(Document &document, const QString &id, bool resolved);
// Removing a comment removes its replies; removing a reply removes only it.
bool remove(Document &document, const QString &id);
// Drops comments whose slide or object has gone, and reading-order entries for
// objects that are no longer there. Called after every edit, so the document in
// hand always describes itself truthfully.
bool prune(Document &document);
QString validate(const Document &document);

// --- what the deck contains ----------------------------------------------
QVariantMap statistics(const Document &document);

// --- reading order --------------------------------------------------------
// Object ids in reading order: the slide's own order first, then anything it
// does not mention, in stacking order.
QStringList readingOrder(const Document &document, int index);
bool moveReading(Document &document, int index, const QString &objectId, int delta);
bool resetReading(Document &document, int index);

// --- findings -------------------------------------------------------------
// Rows of {key, slide, slideId, objectId, severity, check, title, detail,
// dismissed}. Severity is "must", "should" or "info".
QVariantList issues(const Document &document);
bool dismiss(Document &document, const QString &key, bool dismissed);
// A plain-text report of the findings given, for sending to someone else.
QByteArray report(const Document &document, const QVariantList &issues,
                  const QString &deckName);
} // namespace Review
