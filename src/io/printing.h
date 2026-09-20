#pragma once

// Printing, with the same ink as the PDF.
//
// No widget print dialog: the printers are listed for the interface to show and
// the pages are drawn by `Pdf::paint`, so a printed handout and an exported one
// are the same pages.

#include <QString>
#include <QVariantList>

#include "core/scene.h"
#include "core/workers.h"
#include "io/pdf.h"

namespace Printing {

// {name, description, location, default} for every printer this computer knows.
QVariantList printers();
QString defaultPrinter();

bool print(const Document &document, const QString &printer, int copies,
           const Pdf::Options &options, QString *error = nullptr,
           const std::shared_ptr<Workers::Job> &job = {});
} // namespace Printing
