#pragma once

// Autosave, and what happens after a crash.
//
// Three rules, all from the brief and all easy to get subtly wrong:
//
//  1. Autosave is SEPARATE from user save. It never writes over the user's
//     file. A journal lives in the state directory and the deck on disk is
//     only ever touched when the user asks.
//  2. Recovery offers a COPY. The recovered deck comes back dirty and
//     unsaved, so the last explicit save survives until the user decides.
//  3. A journal belonging to a live process is not a crash. Each journal
//     records the pid that owns it; only journals whose owner is gone are
//     offered back.
//
// The journal is an ordinary `.omashow` bundle with one extra member,
// `recovery.json`, naming the original file and when it was written — so a
// journal can be opened by hand, or renamed, and it is simply a deck.

#include <QDateTime>
#include <QString>
#include <QVector>

#include "core/scene.h"

namespace Recovery {

struct Journal {
    QString journalPath;
    QString originalPath;   // empty when the deck had never been saved
    QDateTime savedAt;
    qint64 pid = 0;

    QString displayName() const;
};

QString directory();

// Writes (or rewrites) this process's journal. Cheap enough to call on a timer.
bool write(const Document &document, const QString &originalPath, QString *error = nullptr);

// Removes this process's journal — on a clean save and on a clean exit.
void discard();

// Journals left behind by processes that are no longer running.
QVector<Journal> orphans();

void forget(const QString &journalPath);

} // namespace Recovery
