#include "core/design.h"
#include "io/pdf.h"

#include <QFileInfo>
#include <QFontMetricsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QMutexLocker>

#include <algorithm>

#include "anim/evaluator.h"
#include "render/scenerenderer.h"

namespace {

constexpr qreal kA4Width = 595.0, kA4Height = 842.0, kMargin = 48.0;

QVector<int> eligibleSlides(const Document &document, const Pdf::Options &options) {
    if (!options.indices.isEmpty()) {
        QVector<int> chosen;
        for (int i : options.indices) if (i >= 0 && i < document.slides.size()) chosen.append(i);
        return chosen;
    }
    const int from = qBound(0, options.from, int(document.slides.size()) - 1);
    const int to = options.to < 0 ? document.slides.size() - 1
                                  : qBound(from, options.to, int(document.slides.size()) - 1);
    QVector<int> eligible;
    for (int i = from; i <= to; ++i)
        if (options.includeSkipped || !document.slides.at(i).skipped) eligible.append(i);
    return eligible;
}

// The slide, drawn to fit a box on the page, with its own background.
void paintSlide(QPainter &painter, const Document &document, const Slide &slide,
                const QVector<SceneObject> &objects, const QRectF &box) {
    const qreal scale = qMin(box.width() / document.size.width(),
                             box.height() / document.size.height());
    const QSizeF drawn(document.size.width() * scale, document.size.height() * scale);
    const QRectF target(box.x() + (box.width() - drawn.width()) / 2,
                        box.y() + (box.height() - drawn.height()) / 2,
                        drawn.width(), drawn.height());
    painter.save();
    painter.fillRect(target, slide.background);
    painter.translate(target.topLeft());
    painter.scale(scale, scale);
    painter.setClipRect(QRectF(QPointF(), document.size));
    SceneRenderer::paint(painter, objects);
    painter.restore();
}

// Text poured down a column, starting a new page when it runs out of room.
// Returns false only when a new page could not be started.
bool flowText(QPainter &painter, const QString &text, const QFont &font, qreal indent,
              qreal &cursor, const QSizeF &page, const std::function<bool()> &newPage) {
    if (text.trimmed().isEmpty()) return true;
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    const qreal width = page.width() - kMargin * 2 - indent;
    for (const auto &paragraph : text.split('\n')) {
        QString line;
        QStringList lines;
        for (const auto &word : paragraph.split(' ')) {
            const QString candidate = line.isEmpty() ? word : line + ' ' + word;
            if (metrics.horizontalAdvance(candidate) > width && !line.isEmpty()) {
                lines.append(line);
                line = word;
            } else {
                line = candidate;
            }
        }
        lines.append(line);
        for (const auto &row : lines) {
            if (cursor + metrics.height() > page.height() - kMargin) {
                if (!newPage()) return false;
                cursor = kMargin;
            }
            painter.drawText(QRectF(kMargin + indent, cursor, width, metrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, row);
            cursor += metrics.height();
        }
    }
    return true;
}

QVector<QRectF> handoutBoxes(const QSizeF &page, int perPage) {
    const int columns = perPage >= 6 ? (perPage == 9 ? 3 : 2) : (perPage == 4 ? 2 : 1);
    const int rows = (perPage + columns - 1) / columns;
    const qreal gap = 18.0;
    const qreal width = (page.width() - kMargin * 2 - gap * (columns - 1)) / columns;
    const qreal height = (page.height() - kMargin * 2 - gap * (rows - 1)) / rows;
    QVector<QRectF> boxes;
    for (int i = 0; i < perPage; ++i)
        boxes.append(QRectF(kMargin + (i % columns) * (width + gap),
                            kMargin + (i / columns) * (height + gap), width, height));
    return boxes;
}

} // namespace

QVector<qreal> Pdf::stageTimes(const Slide &slide, bool pagePerBuildStage) {
    const qreal settled = slide.timeline.duration();
    if (!pagePerBuildStage || slide.timeline.steps.isEmpty())
        return {settled};

    // One page per moment a build finishes, so a handout shows the slide
    // building up the way the room saw it.
    QVector<qreal> times;
    for (const BuildStep &step : slide.timeline.resolvedSteps())
        times.append(step.end());
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(),
                            [](qreal a, qreal b) { return qFuzzyCompare(a + 1.0, b + 1.0); }),
                times.end());
    if (times.isEmpty() || !qFuzzyCompare(times.last() + 1.0, settled + 1.0))
        times.append(settled);
    return times;
}

QSizeF Pdf::pageSize(const Document &document, const Options &options) {
    if (options.layout != Slides || document.size.isEmpty())
        return QSizeF(kA4Width, kA4Height);
    const qreal width = options.pageWidthPoints;
    return QSizeF(width, width * document.size.height() / document.size.width());
}

int Pdf::pageCount(const Document &document, const Options &options) {
    const auto eligible = eligibleSlides(document, options);
    if (eligible.isEmpty()) return 0;
    switch (options.layout) {
    case Notes: return eligible.size();
    case Outline: return 0;   // it flows; the writer decides
    case Handout: {
        const int perPage = qBound(1, options.perPage, 9);
        return (eligible.size() + perPage - 1) / perPage;
    }
    default: break;
    }
    int pages = 0;
    for (int index : eligible)
        pages += stageTimes(Design::resolve(document, index), options.pagePerBuildStage).size();
    return pages;
}

bool Pdf::paint(QPainter &painter, const Document &document, const Options &options,
                const QSizeF &page, const std::function<bool()> &newPage,
                const std::shared_ptr<Workers::Job> &job, QString *error) {
    const auto eligible = eligibleSlides(document, options);
    if (eligible.isEmpty()) {
        if (error) *error = QStringLiteral("No included slides in this range.");
        return false;
    }
    const auto stopped = [&job] { return job && job->canceled; };
    const auto fail = [&error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    QFont title = painter.font();
    title.setPointSizeF(13);
    title.setWeight(QFont::DemiBold);
    QFont body = painter.font();
    body.setPointSizeF(10.5);
    QFont label = painter.font();
    label.setPointSizeF(9);

    bool first = true;
    const auto next = [&] {
        if (first) { first = false; return true; }
        return newPage();
    };

    if (options.layout == Outline) {
        if (!next()) return fail(QStringLiteral("Could not start the page."));
        qreal cursor = kMargin;
        painter.setPen(Qt::black);
        for (int index : eligible) {
            if (stopped()) return false;
            const Slide slide = Design::resolve(document, index);
            QStringList lines;
            for (const auto &object : slide.objects) {
                if (object.hidden || object.type != ObjectType::Text) continue;
                if (object.id.startsWith(QStringLiteral("@field/"))) continue;
                if (!object.text.trimmed().isEmpty()) lines.append(object.text);
            }
            const QString heading = QStringLiteral("%1. %2")
                                        .arg(index + 1)
                                        .arg(lines.isEmpty() ? QStringLiteral("Untitled slide")
                                                             : lines.takeFirst().section('\n', 0, 0));
            if (!flowText(painter, heading, title, 0, cursor, page, newPage))
                return fail(QStringLiteral("Could not start the page."));
            for (const auto &line : lines)
                if (!flowText(painter, line, body, 24, cursor, page, newPage))
                    return fail(QStringLiteral("Could not start the page."));
            cursor += 8;
        }
        return true;
    }

    if (options.layout == Handout) {
        const int perPage = qBound(1, options.perPage, 9);
        const auto boxes = handoutBoxes(page, perPage);
        for (int n = 0; n < eligible.size(); ++n) {
            if (stopped()) return false;
            if (n % perPage == 0 && !next())
                return fail(QStringLiteral("Could not start the page."));
            const int index = eligible.at(n);
            const Slide slide = Design::resolve(document, index);
            const QRectF box = boxes.at(n % perPage);
            const QRectF frame(box.x(), box.y(), box.width(), box.height() - 16);
            paintSlide(painter, document, slide, slide.objects, frame);
            painter.setFont(label);
            painter.setPen(Qt::darkGray);
            painter.drawText(QRectF(box.x(), box.bottom() - 14, box.width(), 14),
                             Qt::AlignLeft, QStringLiteral("Slide %1").arg(index + 1));
        }
        return true;
    }

    for (int index : eligible) {
        if (stopped()) return false;
        const Slide slide = Design::resolve(document, index);
        if (options.layout == Notes) {
            if (!next()) return fail(QStringLiteral("Could not start the page."));
            const QRectF frame(kMargin, kMargin, page.width() - kMargin * 2,
                               (page.width() - kMargin * 2) * document.size.height() /
                                   document.size.width());
            paintSlide(painter, document, slide, slide.objects, frame);
            painter.setPen(Qt::darkGray);
            painter.setFont(label);
            painter.drawText(QRectF(kMargin, frame.bottom() + 6, frame.width(), 14),
                             Qt::AlignLeft, QStringLiteral("Slide %1").arg(index + 1));
            painter.setPen(Qt::black);
            qreal cursor = frame.bottom() + 28;
            const QString notes = document.slides.at(index).notes.trimmed();
            if (!flowText(painter, notes.isEmpty() ? QStringLiteral("—") : notes, body, 0,
                          cursor, page, newPage))
                return fail(QStringLiteral("Could not start the page."));
            continue;
        }
        for (const qreal time : stageTimes(slide, options.pagePerBuildStage)) {
            if (!next()) return fail(QStringLiteral("Could not write the next PDF page."));
            // The slide's own background, edge to edge — a deck printed on
            // white when it was authored dark is not the same deck.
            painter.fillRect(QRectF(QPointF(), page), slide.background);
            paintSlide(painter, document, slide,
                       options.pagePerBuildStage ? Evaluator::stateAt(slide, time) : slide.objects,
                       QRectF(QPointF(), page));
        }
    }
    return true;
}

bool Pdf::write(const Document &document, const QString &path,
                const Options &options, QString *error, const std::shared_ptr<Workers::Job> &job) {
    if (document.slides.isEmpty()) {
        if (error) *error = QStringLiteral("The deck has no slides.");
        return false;
    }
    if (document.size.isEmpty()) {
        if (error) *error = QStringLiteral("The deck has no size.");
        return false;
    }

    const QSizeF page = pageSize(document, options);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    {
    QPdfWriter writer(&file);
    writer.setResolution(72);   // 1 unit == 1 point, so the maths below is plain
    writer.setPageSize(QPageSize(page, QPageSize::Point,
                                 options.layout == Slides ? QStringLiteral("Slide")
                                                          : QStringLiteral("A4")));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle(QFileInfo(path).completeBaseName());
    writer.setCreator(QStringLiteral("OmaShow"));

    QPainter painter;
    if (!painter.begin(&writer)) {
        if (error) *error = QStringLiteral("Could not write to %1.").arg(path);
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::LosslessImageRendering, true);

    const bool ok = paint(painter, document, options, page,
                          [&writer] { return writer.newPage(); }, job, error);
    painter.end();
    if (!ok) { file.cancelWriting(); return false; }
    } // Finish the PDF trailer before committing the atomic output.
    QMutexLocker lock(job ? &job->commitMutex : nullptr);
    if (job && job->canceled) { file.cancelWriting(); return false; }
    if (!file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
