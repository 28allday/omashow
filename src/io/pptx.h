#pragma once

// Reading PowerPoint decks (`.pptx`, Office Open XML) into the native model.
//
// Written in-house on the zip reader and QXmlStreamReader: the format is XML
// parts in a zip, and what a slide needs from it — shapes, text with its
// inheritance from layout and master, pictures, tables, charts, notes,
// transitions, builds and sections — is a bounded amount of it. Everything
// resolved here is baked into the deck: an imported slide carries its own
// geometry and typography rather than a translation of PowerPoint's
// inheritance rules, so it reads the same whichever layout it is later given.
//
// What cannot be carried across is reported, never silently dropped.

#include <QByteArray>
#include <QStringList>

#include "core/scene.h"

namespace Pptx {

struct Result {
    bool ok = false;
    QString error;
    Document document;
    QStringList warnings;
};

Result read(const QByteArray &raw);
Result load(const QString &path);

} // namespace Pptx
