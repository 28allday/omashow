#include "io/exports.h"
#include "anim/evaluator.h"
#include "anim/presentation.h"
#include "anim/presentationcache.h"
#include "core/design.h"
#include "io/pdf.h"
#include "render/scenerenderer.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageWriter>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

namespace {

QSize pixels(const QSizeF &slide, int width) {
    const int w = qBound(160, width, 7680);
    const int h = qMax(90, qRound(w * slide.height() / qMax(1.0, slide.width())));
    // Encoders want even dimensions; so does anyone scaling the result later.
    return QSize(w - (w % 2), h - (h % 2));
}

QVector<int> slidesIn(const Document &document, const Exports::Request &request) {
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

} // namespace

QVariantMap Exports::Request::toMap() const {
    return {{"kind", kind}, {"path", path}, {"includeSkipped", includeSkipped},
            {"stages", stages}, {"from", from}, {"to", to}, {"format", format},
            {"width", width}, {"transparent", transparent}, {"fps", fps},
            {"quality", quality}};
}

Exports::Request Exports::Request::fromMap(const QVariantMap &map) {
    Request request;
    request.kind = qBound(int(Pdf), map.value("kind", Pdf).toInt(), int(Video));
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
    return request;
}

QString Exports::Request::describe() const {
    switch (kind) {
    case Images:
        return QStringLiteral("%1 pictures, %2 wide%3")
            .arg(format == 0 ? "PNG" : "JPEG").arg(width)
            .arg(transparent && format == 0 ? ", no background" : "");
    case Video:
        return QStringLiteral("H.264 film, %1 wide at %2 frames a second")
            .arg(width).arg(fps);
    default: break;
    }
    return stages ? QStringLiteral("PDF, a page for every build stage")
                  : QStringLiteral("PDF, a page a slide");
}

QString Exports::Request::suggestedName(const QString &deckName) const {
    const QString base = deckName.isEmpty() ? QStringLiteral("Untitled")
                                            : QFileInfo(deckName).completeBaseName();
    switch (kind) {
    case Images: return base + (format == 0 ? ".png" : ".jpg");
    case Video: return base + ".mp4";
    default: break;
    }
    return base + ".pdf";
}

bool Exports::encoderAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
}

Exports::Outcome Exports::run(const Document &document, const Request &request,
                              const std::shared_ptr<Workers::Job> &job,
                              const std::function<void(int)> &progress) {
    Outcome outcome;
    const auto report = [&progress](int percent) { if (progress) progress(percent); };
    const auto fail = [&outcome](const QString &message) {
        outcome.error = message;
        return outcome;
    };
    const auto canceled = [&job] { return job && job->canceled; };
    if (request.path.isEmpty()) return fail(QStringLiteral("Choose where to write it."));
    const auto indices = slidesIn(document, request);
    if (indices.isEmpty())
        return fail(QStringLiteral("That range has no slides to export."));

    if (request.kind == Pdf) {
        Pdf::Options options;
        options.from = indices.first();
        options.to = indices.last();
        options.pagePerBuildStage = request.stages;
        options.includeSkipped = request.includeSkipped;
        QString error;
        report(5);
        if (!Pdf::write(document, request.path, options, &error, job))
            return fail(error.isEmpty() ? QStringLiteral("The PDF could not be written.") : error);
        outcome.files.append(request.path);
        outcome.log.append(QStringLiteral("%1 slides").arg(indices.size()));
        report(100);
        outcome.ok = true;
        return outcome;
    }

    const QSize size = pixels(document.size, request.width);

    if (request.kind == Images) {
        const QString suffix = request.format == 0 ? QStringLiteral("png") : QStringLiteral("jpg");
        for (int n = 0; n < indices.size(); ++n) {
            if (canceled()) return fail(QStringLiteral("Export canceled."));
            const int index = indices.at(n);
            const Slide resolved = Design::resolve(document, index);
            const auto states = Evaluator::stateAt(resolved, resolved.timeline.duration());
            const QColor background = request.transparent && request.format == 0
                                          ? QColor(Qt::transparent) : resolved.background;
            auto image = SceneRenderer::render(states, document.size, size, background);
            const QFileInfo chosen(request.path);
            const QString file =
                indices.size() > 1
                    ? numbered(request.path, index + 1, document.slides.size(), suffix)
                    : chosen.suffix().compare(suffix, Qt::CaseInsensitive) == 0
                          ? request.path
                          : chosen.absolutePath() + '/' + chosen.completeBaseName() + '.' + suffix;
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
                        "-f", "mp4", request.path};
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
