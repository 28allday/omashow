#include "io/exports.h"
#include "anim/evaluator.h"
#include "anim/presentation.h"
#include "anim/presentationcache.h"
#include "core/design.h"
#include "io/packagedeck.h"
#include "io/pptxwriter.h"
#include "io/pdf.h"
#include "io/printing.h"
#include "render/scenerenderer.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageWriter>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {

QSize pixels(const QSizeF &slide, int width) {
    const qreal ratio = slide.height() / qMax(1.0, slide.width());
    int w = qBound(160, width, 7680);
    // A tall slide keeps its width only while its height stays sensible.
    if (w * ratio > 16384) w = qMax(160, int(16384 / ratio));
    const int h = qBound(90, qRound(w * ratio), 16384);
    // Encoders want even dimensions; so does anyone scaling the result later.
    return QSize(w - (w % 2), h - (h % 2));
}

QVector<int> slidesIn(const Document &document, const Exports::Request &request) {
    // A custom show is already a choice of slides, in an order: exporting one
    // exports that, whole, rather than a range cut out of the deck.
    if (!document.activeShow.isEmpty())
        return Presentation::slideIndices(document, request.includeSkipped);
    QVector<int> indices;
    const int from = request.from < 0 ? 0 : request.from;
    const int to = request.to < 0 ? document.slides.size() - 1 : request.to;
    for (int i = qMax(0, from); i <= qMin(to, int(document.slides.size()) - 1); ++i)
        if (request.includeSkipped || !document.slides.at(i).skipped) indices.append(i);
    return indices;
}

QString numbered(const QString &path, int n, int of, const QString &suffix) {
    const QFileInfo info(path);
    const int width = QString::number(of).size();
    return info.absolutePath() + '/' + info.completeBaseName() + '-' +
           QString::number(n).rightJustified(width, QLatin1Char('0')) + '.' + suffix;
}

// The one place a picture export's format becomes a file suffix.
QString pictureSuffix(const Exports::Request &request) {
    return request.format == 0 ? QStringLiteral("png") : QStringLiteral("jpg");
}

// The file one slide's picture goes to: numbered when there are several, and
// always with the suffix of the format actually written.
QString pictureFile(const Exports::Request &request, int index, int count, int total) {
    const QString suffix = pictureSuffix(request);
    const QFileInfo chosen(request.path);
    if (count > 1) return numbered(request.path, index + 1, total, suffix);
    return chosen.suffix().compare(suffix, Qt::CaseInsensitive) == 0
               ? request.path
               : chosen.absolutePath() + '/' + chosen.completeBaseName() + '.' + suffix;
}

} // namespace

QStringList Exports::targets(const Document &document, const Request &request) {
    if (request.path.isEmpty()) return {};
    if (request.kind != Images) return {request.path};
    const auto indices = slidesIn(document, request);
    QStringList files;
    for (int index : indices) files << pictureFile(request, index, indices.size(), document.slides.size());
    return files;
}

QString Exports::freePath(const Document &document, const Request &request) {
    const auto anyExist = [&document](const Request &r) {
        const auto files = targets(document, r);
        if (files.size() < 2) return false;
        for (const auto &file : files) if (QFileInfo::exists(file)) return true;
        return false;
    };
    if (request.kind != Images || !anyExist(request)) return request.path;
    const QFileInfo chosen(request.path);
    const QString suffix = chosen.suffix().isEmpty() ? QString() : QLatin1Char('.') + chosen.suffix();
    Request next = request;
    for (int n = 2; n < 1000; ++n) {
        next.path = chosen.absolutePath() + QLatin1Char('/') + chosen.completeBaseName() +
                    QStringLiteral(" (%1)").arg(n) + suffix;
        if (!anyExist(next)) return next.path;
    }
    return request.path;
}

QVariantMap Exports::Request::toMap() const {
    return {{"kind", kind}, {"path", path}, {"includeSkipped", includeSkipped},
            {"stages", stages}, {"layout", layout}, {"perPage", perPage},
            {"from", from}, {"to", to}, {"format", format},
            {"width", width}, {"transparent", transparent}, {"fps", fps},
            {"quality", quality}, {"printer", printer}, {"copies", copies}};
}

Exports::Request Exports::Request::fromMap(const QVariantMap &map) {
    Request request;
    request.kind = qBound(int(Pdf), map.value("kind", Pdf).toInt(), int(PowerPoint));
    request.path = map.value("path").toString();
    request.includeSkipped = map.value("includeSkipped").toBool();
    request.stages = map.value("stages").toBool();
    request.from = map.value("from", -1).toInt();
    request.to = map.value("to", -1).toInt();
    request.format = qBound(0, map.value("format").toInt(), 1);
    request.width = qBound(160, map.value("width", 1920).toInt(), 7680);
    request.transparent = map.value("transparent").toBool();
    request.fps = qBound(1, map.value("fps", 30).toInt(), 120);
    request.quality = qBound(0, map.value("quality").toInt(), 1);
    request.layout = qBound(0, map.value("layout").toInt(), 3);
    request.perPage = qBound(1, map.value("perPage", 2).toInt(), 9);
    request.printer = map.value("printer").toString();
    request.copies = qBound(1, map.value("copies", 1).toInt(), 99);
    return request;
}

QString Exports::Request::describe() const {
    const QStringList pages{QStringLiteral("a page a slide"),
                            QStringLiteral("slides with their notes"),
                            QStringLiteral("an outline"),
                            QStringLiteral("%1 slides a sheet")};
    const QString shape = layout == 3 ? pages.at(3).arg(perPage) : pages.at(qBound(0, layout, 2));
    switch (kind) {
    case Images:
        return QStringLiteral("%1 pictures, %2 wide%3")
            .arg(format == 0 ? "PNG" : "JPEG").arg(width)
            .arg(transparent && format == 0 ? ", no background" : "");
    case Video:
        return QStringLiteral("H.264 film, %1 wide at %2 frames a second")
            .arg(width).arg(fps);
    case Package:
        return QStringLiteral("The deck with copies of everything it links to");
    case PowerPoint:
        return QStringLiteral("A PowerPoint deck, for people who have that");
    case Print:
        return QStringLiteral("Printed on %1 · %2%3")
            .arg(printer.isEmpty() ? QStringLiteral("the default printer") : printer, shape)
            .arg(copies > 1 ? QStringLiteral(" · %1 copies").arg(copies) : QString());
    default: break;
    }
    return stages && layout == 0
               ? QStringLiteral("PDF, a page for every build stage")
               : QStringLiteral("PDF, ") + shape;
}

QString Exports::Request::suggestedName(const QString &deckName) const {
    const QString base = deckName.isEmpty() ? QStringLiteral("Untitled")
                                            : QFileInfo(deckName).completeBaseName();
    switch (kind) {
    case Images: return base + (format == 0 ? ".png" : ".jpg");
    case Video: return base + ".mp4";
    case Package: return base + "-package.zip";
    case PowerPoint: return base + ".pptx";
    default: break;
    }
    return base + ".pdf";
}

bool Exports::encoderAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
}

Exports::Outcome Exports::run(const Document &document, const Request &request,
                              const std::shared_ptr<Workers::Job> &job,
                              const std::function<void(int)> &progress,
                              const QHash<QString, QString> &approved) {
    Outcome outcome;
    const auto report = [&progress](int percent) { if (progress) progress(percent); };
    const auto fail = [&outcome](const QString &message) {
        outcome.error = message;
        return outcome;
    };
    const auto canceled = [&job] { return job && job->canceled; };
    if (request.path.isEmpty() && request.kind != Print)
        return fail(QStringLiteral("Choose where to write it."));
    const auto indices = slidesIn(document, request);
    if (indices.isEmpty())
        return fail(QStringLiteral("That range has no slides to export."));

    if (request.kind == Package) {
        report(5);
        const auto packaged = Package::write(document, request.path, approved, job);
        if (!packaged.ok) return fail(packaged.error);
        outcome.files.append(request.path);
        outcome.log = packaged.lines;
        if (indices.size() != document.slides.size())
            outcome.log.append(QStringLiteral("A package is always the whole deck; the slide range was not applied."));
        report(100);
        outcome.ok = true;
        return outcome;
    }

    if (request.kind == PowerPoint) {
        report(5);
        // The whole deck goes across, hidden slides as hidden slides, unless a
        // range or a custom show asks for particular slides in an order.
        Document chosen = document;
        if (request.from >= 0 || request.to >= 0 || !document.activeShow.isEmpty()) {
            chosen.slides.clear();
            for (int i : indices) chosen.slides.append(document.slides.at(i));
        }
        chosen.activeShow.clear();
        const auto written = PptxWriter::write(chosen, request.path, job);
        if (!written.ok) return fail(written.error);
        outcome.files.append(request.path);
        outcome.log = written.lines;
        report(100);
        outcome.ok = true;
        return outcome;
    }

    if (request.kind == Pdf || request.kind == Print) {
        Pdf::Options options;
        options.from = *std::min_element(indices.cbegin(), indices.cend());
        options.to = *std::max_element(indices.cbegin(), indices.cend());
        options.indices = indices;
        options.pagePerBuildStage = request.stages;
        options.includeSkipped = request.includeSkipped;
        options.layout = request.layout;
        options.perPage = request.perPage;
        QString error;
        report(5);
        if (request.kind == Print) {
            if (!Printing::print(document, request.printer.isEmpty()
                                               ? Printing::defaultPrinter() : request.printer,
                                 request.copies, options, &error, job))
                return fail(error.isEmpty() ? QStringLiteral("Printing failed.") : error);
            outcome.log.append(QStringLiteral("%1 pages sent to %2")
                                   .arg(Pdf::pageCount(document, options))
                                   .arg(request.printer.isEmpty()
                                            ? Printing::defaultPrinter() : request.printer));
            report(100);
            outcome.ok = true;
            return outcome;
        }
        if (!Pdf::write(document, request.path, options, &error, job))
            return fail(error.isEmpty() ? QStringLiteral("The PDF could not be written.") : error);
        outcome.files.append(request.path);
        outcome.log.append(QStringLiteral("%1 slides, %2 pages")
                               .arg(indices.size())
                               .arg(Pdf::pageCount(document, options) > 0
                                        ? QString::number(Pdf::pageCount(document, options))
                                        : QStringLiteral("as many as the outline needs")));
        report(100);
        outcome.ok = true;
        return outcome;
    }

    const QSize size = pixels(document.size, request.width);

    if (request.kind == Images) {
        const QString suffix = pictureSuffix(request);
        for (int n = 0; n < indices.size(); ++n) {
            if (canceled()) return fail(QStringLiteral("Export canceled."));
            const int index = indices.at(n);
            const Slide resolved = Design::resolve(document, index);
            const auto states = Evaluator::stateAt(resolved, resolved.timeline.duration());
            const QColor background = request.transparent && request.format == 0
                                          ? QColor(Qt::transparent) : resolved.background;
            auto image = SceneRenderer::render(states, document.size, size, background);
            const QString file = pictureFile(request, index, indices.size(), document.slides.size());
            QImageWriter writer(file, suffix.toUtf8());
            if (request.format == 1) writer.setQuality(92);
            if (!writer.write(image))
                return fail(QStringLiteral("Could not write %1: %2")
                                .arg(QFileInfo(file).fileName(), writer.errorString()));
            outcome.files.append(file);
            report(qBound(1, (n + 1) * 100 / indices.size(), 100));
        }
        outcome.log.append(QStringLiteral("%1 × %2 pixels").arg(size.width()).arg(size.height()));
        outcome.ok = true;
        return outcome;
    }

    // --- film ---------------------------------------------------------------
    if (!encoderAvailable())
        return fail(QStringLiteral("FFmpeg is not installed, so film cannot be encoded here."));
    PresentationCache cache;
    Document ranged = document;
    // A range is exported by taking the deck down to those slides, so the
    // timing inside it is the deck's own and transitions still run between them.
    if (request.from >= 0 || request.to >= 0) {
        QVector<Slide> kept;
        for (int index : indices) kept.append(document.slides.at(index));
        ranged.slides = kept;
    }
    cache.reset(ranged, request.includeSkipped);
    const qreal duration = cache.duration();
    if (duration <= 0) return fail(QStringLiteral("There is nothing to film yet."));
    const int frames = qMax(1, int(std::ceil(duration * request.fps)));

    QStringList args = {"-v", "error", "-nostdin", "-y",
                        "-f", "rawvideo", "-pix_fmt", "rgba",
                        "-s", QStringLiteral("%1x%2").arg(size.width()).arg(size.height()),
                        "-r", QString::number(request.fps), "-i", "-",
                        "-an", "-c:v", "libx264", "-preset",
                        request.quality == 0 ? "veryfast" : "slow",
                        "-crf", request.quality == 0 ? "23" : "18",
                        "-pix_fmt", "yuv420p", "-movflags", "+faststart",
                        // ffmpeg reads "tcp://…", "pipe:1" or "-x" as other
                        // things than a file; say plainly that this is one.
                        "-f", "mp4",
                        QStringLiteral("file:") + QFileInfo(request.path).absoluteFilePath()};
    QProcess encoder;
    encoder.start(QStringLiteral("ffmpeg"), args);
    if (!encoder.waitForStarted(5000))
        return fail(QStringLiteral("FFmpeg would not start."));
    QElapsedTimer idle;
    idle.start();
    for (int frame = 0; frame < frames; ++frame) {
        if (canceled() || encoder.state() == QProcess::NotRunning) break;
        const qreal t = qMin(duration, frame / qreal(request.fps));
        const auto image = SceneRenderer::render(cache.statesAt(t), ranged.size, size,
                                                 cache.backgroundAt(t))
                               .convertToFormat(QImage::Format_RGBA8888);
        const char *bits = reinterpret_cast<const char *>(image.constBits());
        qint64 written = 0;
        const qint64 total = qint64(image.bytesPerLine()) * image.height();
        while (written < total) {
            const qint64 n = encoder.write(bits + written, total - written);
            if (n < 0) break;
            written += n;
            if (!encoder.waitForBytesWritten(30000)) break;
        }
        if (written < total) {
            encoder.kill();
            encoder.waitForFinished(5000);
            return fail(QStringLiteral("The encoder stopped taking frames."));
        }
        report(qBound(1, (frame + 1) * 98 / frames, 98));
        if (idle.elapsed() > 250) idle.restart();
    }
    const bool stopped = canceled();
    encoder.closeWriteChannel();
    if (!encoder.waitForFinished(120000)) {
        encoder.kill();
        encoder.waitForFinished(5000);
        return fail(QStringLiteral("The encoder stopped responding."));
    }
    const auto errors = QString::fromUtf8(encoder.readAllStandardError()).trimmed();
    if (stopped) {
        QFile::remove(request.path);
        return fail(QStringLiteral("Export canceled."));
    }
    if (encoder.exitStatus() != QProcess::NormalExit || encoder.exitCode() != 0)
        return fail(errors.isEmpty() ? QStringLiteral("Encoding failed.")
                                     : QStringLiteral("Encoding failed: ") + errors);
    outcome.files.append(request.path);
    outcome.log.append(QStringLiteral("%1 frames at %2 fps, %3 × %4 pixels")
                           .arg(frames).arg(request.fps).arg(size.width()).arg(size.height()));
    outcome.log.append(QStringLiteral("Sound from film and audio on the slides is not "
                                      "included; the picture is frame-exact."));
    report(100);
    outcome.ok = true;
    return outcome;
}
