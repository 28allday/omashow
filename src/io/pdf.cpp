#include "core/design.h"
#include "io/pdf.h"

#include <QFileInfo>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#include <algorithm>

#include "anim/evaluator.h"
#include "render/scenerenderer.h"

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

bool Pdf::write(const Document &document, const QString &path,
                const Options &options, QString *error) {
    if (document.slides.isEmpty()) {
        if (error) *error = QStringLiteral("The deck has no slides.");
        return false;
    }
    if (document.size.isEmpty()) {
        if (error) *error = QStringLiteral("The deck has no size.");
        return false;
    }

    const int from = qBound(0, options.from, document.slides.size() - 1);
    const int to = options.to < 0 ? document.slides.size() - 1
                                  : qBound(from, options.to, document.slides.size() - 1);

    QVector<int> eligible;
    for(int i=from;i<=to;++i) if(options.includeSkipped || !document.slides.at(i).skipped) eligible.append(i);
    if(eligible.isEmpty()) { if(error) *error=QStringLiteral("No included slides in this range."); return false; }
    const qreal pageWidth = options.pageWidthPoints;
    const qreal pageHeight = pageWidth * document.size.height() / document.size.width();

    QPdfWriter writer(path);
    writer.setResolution(72);   // 1 unit == 1 point, so the maths below is plain
    writer.setPageSize(QPageSize(QSizeF(pageWidth, pageHeight), QPageSize::Point,
                                 QStringLiteral("Slide")));
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

    const qreal scale = pageWidth / document.size.width();
    bool firstPage = true;

    for (int index : eligible) {
        const Slide slide = Design::resolve(document, index);
        const QVector<qreal> times = stageTimes(slide, options.pagePerBuildStage);

        for (const qreal time : times) {
            if (!firstPage)
                writer.newPage();
            firstPage = false;

            painter.save();
            // The slide's own background, edge to edge — a deck printed on
            // white when it was authored dark is not the same deck.
            painter.fillRect(QRectF(0, 0, pageWidth, pageHeight), slide.background);
            painter.scale(scale, scale);
            SceneRenderer::paint(painter, options.pagePerBuildStage ? Evaluator::stateAt(slide, time) : slide.objects);
            painter.restore();
        }
    }

    painter.end();
    return true;
}
