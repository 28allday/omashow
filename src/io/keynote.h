#pragma once

// Reading Keynote decks (`.key`) into the native model, read-only.
//
// Keynote's file formats are undocumented and two generations apart (the
// APXL XML of Keynote '09 and the protobuf IWA of Keynote 6 and later), so
// this does not parse them itself: libetonyek, the Document Liberation
// Project's reader, walks the file and calls back with slides, shapes, text,
// pictures, tables and notes in the shape librevenge gives every reader. What
// arrives here is turned into native objects; builds, transitions and charts
// are not part of what libetonyek reports and are said to be missing.

#include <QByteArray>
#include <QStringList>

#include "core/scene.h"

namespace Keynote {

struct Result {
    bool ok = false;
    QString error;
    Document document;
    QStringList warnings;
};

Result read(const QByteArray &raw);
// A path: a `.key` file, or the folder that an older Keynote package is.
Result load(const QString &path);

} // namespace Keynote
