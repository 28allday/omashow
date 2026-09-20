#pragma once

// Who else has this deck open.
//
// An advisory lock, not a mandatory one: a `.lock` file beside the deck naming
// the process that opened it. A lock whose process is gone is stale and ignored
// — the same rule the recovery journal uses, for the same reason. Nothing is
// ever prevented by it; the point is to say so before two people overwrite each
// other's afternoon.

#include <QString>

namespace DeckLock {

struct Holder {
    bool held = false;      // someone has it
    bool mine = false;      // and that someone is this process
    qint64 pid = 0;
    QString host, since;
};

// The lock beside a deck, if any process still holds it.
Holder check(const QString &deckPath);
// Takes the lock for this process, releasing whatever this process held before.
bool take(const QString &deckPath);
void release();
QString pathFor(const QString &deckPath);
} // namespace DeckLock
