#include "io/printing.h"

#include <QPainter>
#include <QPageSize>
#include <QPrinter>
#include <QPrinterInfo>

QVariantList Printing::printers() {
    QVariantList rows;
    const auto fallback = QPrinterInfo::defaultPrinterName();
    for (const auto &info : QPrinterInfo::availablePrinters())
        rows.append(QVariantMap{{"name", info.printerName()},
                                {"description", info.description()},
                                {"location", info.location()},
                                {"default", info.printerName() == fallback}});
    return rows;
}

QString Printing::defaultPrinter() {
    const auto name = QPrinterInfo::defaultPrinterName();
    if (!name.isEmpty()) return name;
    const auto available = QPrinterInfo::availablePrinters();
    return available.isEmpty() ? QString() : available.first().printerName();
}

bool Printing::print(const Document &document, const QString &printer, int copies,
                     const Pdf::Options &options, QString *error,
                     const std::shared_ptr<Workers::Job> &job) {
    const auto fail = [&error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (document.slides.isEmpty()) return fail(QStringLiteral("The deck has no slides."));
    const QPrinterInfo info = QPrinterInfo::printerInfo(printer);
    if (info.isNull())
        return fail(printer.isEmpty() ? QStringLiteral("No printer is set up on this computer.")
                                      : QStringLiteral("%1 is not available.").arg(printer));
    QPrinter device(info, QPrinter::HighResolution);
    device.setCopyCount(qBound(1, copies, 99));
    device.setDocName(QStringLiteral("OmaShow"));
    device.setFullPage(true);
    const QSizeF page = Pdf::pageSize(document, options);
    device.setPageSize(QPageSize(page, QPageSize::Point,
                                 options.layout == Pdf::Slides ? QStringLiteral("Slide")
                                                               : QStringLiteral("A4")));
    device.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter;
    if (!painter.begin(&device))
        return fail(QStringLiteral("The printer would not take the job."));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    // The printer's own resolution, in the points the pages are laid out in.
    const qreal scale = device.resolution() / 72.0;
    painter.scale(scale, scale);
    const bool ok = Pdf::paint(painter, document, options, page,
                               [&device] { return device.newPage(); }, job, error);
    painter.end();
    if (!ok && error && error->isEmpty()) *error = QStringLiteral("Printing failed.");
    if (ok && job && job->canceled) {
        device.abort();
        return fail(QStringLiteral("Printing canceled."));
    }
    return ok;
}
