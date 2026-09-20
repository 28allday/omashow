#pragma once

// Templates are decks.
//
// A template is an ordinary `.omashow` file kept in the templates folder, and
// starting from one is opening it as a new, unsaved deck. There is no second
// format to go wrong, nothing to convert, and a template can be edited by
// opening it like anything else. The built-in ones are generated rather than
// stored, so they always match the layouts this build makes.

#include <QString>
#include <QVariantList>

#include "core/scene.h"

namespace Templates {

QString folder();
// Where example decks live: installed beside the application, or in the source
// tree when running from it (OMASHOW_EXAMPLES points at another copy).
QStringList exampleFolders();

// Built-in themes first, then whatever is installed, each with enough detail to
// choose from: masters, layouts, typefaces (and which are missing here), the
// shape of the slides and where it came from.
QVariantList all();

// `id` is "builtin:<theme>:<layout>" or the path of an installed template.
bool describe(const QString &id, QVariantMap *row);
// The deck a template makes: a new document, with no file behind it.
bool open(const QString &id, Document *document, QString *error);

// Copies a deck into the templates folder after checking it opens, that it is
// not enormous, and that nothing in it depends on files that would not travel.
QString install(const QString &path, QString *error, QStringList *warnings = nullptr);
QString save(const Document &document, const QString &name, QString *error,
             QStringList *warnings = nullptr);
bool remove(const QString &id, QString *error);
} // namespace Templates
