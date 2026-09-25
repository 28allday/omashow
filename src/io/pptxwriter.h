#pragma once

// Writing a deck out as PowerPoint (`.pptx`, Office Open XML).
//
// The counterpart of `pptx.*`: the resolved deck — every slide as it would be
// drawn, with its design applied — becomes masters, layouts, slides, notes,
// pictures, films, tables, charts, comments, sections, transitions and builds
// that PowerPoint, Keynote, Google Slides and LibreOffice all open. What has
// no equivalent there (equations, words on a path, morph, picture fills)
// is written in the nearest form and named in the report.

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <memory>

#include "core/scene.h"
#include "core/workers.h"

namespace PptxWriter {

struct Report {
    bool ok = false;
    QString error;
    QStringList lines;   // what was approximated or left out; shown in the export log
    qint64 bytes = 0;
};

QByteArray bytes(const Document &document, QStringList *warnings = nullptr,
                 const std::shared_ptr<Workers::Job> &job = {});
Report write(const Document &document, const QString &path,
             const std::shared_ptr<Workers::Job> &job = {});

} // namespace PptxWriter
