#pragma once

// Packaging a deck to hand to someone else.
//
// A zip holding the deck, copies of everything it links to, and a manifest that
// says what is inside, what was left out and why. Originals are only ever read:
// nothing on the author's disk is moved, rewritten or relinked, and a linked
// file that has not been approved for reading stays out and is named in the
// manifest rather than quietly skipped.

#include <QHash>
#include <QString>
#include <QStringList>

#include "core/scene.h"
#include "core/workers.h"

namespace Package {

struct Report {
    bool ok = false;
    QString error;
    QStringList lines;     // the manifest, also shown in the export log
    qint64 bytes = 0;
};

// `approved` maps a linked media path to the asset id the author approved for
// reading, exactly as the media preflight records it.
Report write(const Document &document, const QString &path,
             const QHash<QString, QString> &approved,
             const std::shared_ptr<Workers::Job> &job = {});
} // namespace Package
