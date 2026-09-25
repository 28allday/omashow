#include "io/pptxwriter.h"

#include "anim/presentation.h"
#include "core/chart.h"
#include "core/design.h"
#include "core/mediaasset.h"
#include "core/shape.h"
#include "core/table.h"
#include "core/textruns.h"
#include "io/zip.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QPainter>
#include <QSaveFile>
#include <QUrl>
#include <QSet>
#include <QUuid>
#include <QXmlStreamWriter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

const QString nsA = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main");
const QString nsP = QStringLiteral("http://schemas.openxmlformats.org/presentationml/2006/main");
const QString nsR = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString nsC = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/chart");
const QString nsP14 = QStringLiteral("http://schemas.microsoft.com/office/powerpoint/2010/main");
const QString relBase = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");
const QString relMedia = QStringLiteral("http://schemas.microsoft.com/office/2007/relationships/media");

// ---------------------------------------------------------------------------
// A thin hand on QXmlStreamWriter: prefixed names written as plain names, so
// the markup reads like the specification does.

class Xml {
public:
    Xml() : m_writer(&m_bytes) { m_writer.writeStartDocument(QStringLiteral("1.0"), true); }
    Xml &start(const char *tag) { m_writer.writeStartElement(QString::fromLatin1(tag)); return *this; }
    Xml &attr(const char *name, const QString &value) { m_writer.writeAttribute(QString::fromLatin1(name), value); return *this; }
    Xml &attr(const char *name, const char *value) { return attr(name, QString::fromLatin1(value)); }
    Xml &attr(const char *name, qint64 value) { return attr(name, QString::number(value)); }
    Xml &attr(const char *name, int value) { return attr(name, QString::number(value)); }
    Xml &text(const QString &words) { m_writer.writeCharacters(words); return *this; }
    Xml &end() { m_writer.writeEndElement(); return *this; }
    Xml &empty(const char *tag) { m_writer.writeEmptyElement(QString::fromLatin1(tag)); return *this; }
    QByteArray finish() { m_writer.writeEndDocument(); return m_bytes; }
private:
    QByteArray m_bytes;
    QXmlStreamWriter m_writer;
};

// A relationships part being assembled beside its owner.
struct Rels {
    struct Rel { QString id, type, target; bool external = false; };
    QVector<Rel> rels;
    QString add(const QString &type, const QString &target, bool external = false) {
        for (const auto &r : rels) if (r.type == type && r.target == target && r.external == external) return r.id;
        Rel rel;
        rel.id = QStringLiteral("rId%1").arg(rels.size() + 1);
        rel.type = type; rel.target = target; rel.external = external;
        rels.append(rel);
        return rel.id;
    }
    QByteArray bytes() const {
        Xml x;
        x.start("Relationships").attr("xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        for (const auto &r : rels) {
            x.start("Relationship").attr("Id", r.id).attr("Type", r.type).attr("Target", r.target);
            if (r.external) x.attr("TargetMode", "External");
            x.end();
        }
        x.end();
        return x.finish();
    }
};

QString hex(const QColor &c) {
    return QStringLiteral("%1%2%3").arg(c.red(), 2, 16, QLatin1Char('0')).arg(c.green(), 2, 16, QLatin1Char('0')).arg(c.blue(), 2, 16, QLatin1Char('0')).toUpper();
}

// <a:srgbClr val="…"><a:alpha val="…"/></a:srgbClr>
void writeColor(Xml &x, const QColor &c, qreal opacity = 1.0) {
    x.start("a:srgbClr").attr("val", hex(c));
    const qreal alpha = c.alphaF() * opacity;
    if (alpha < 0.999) x.start("a:alpha").attr("val", qRound(alpha * 100000)).end();
    x.end();
}

void writeSolidFill(Xml &x, const QColor &c, qreal opacity = 1.0) {
    x.start("a:solidFill");
    writeColor(x, c, opacity);
    x.end();
}

// ---------------------------------------------------------------------------

struct Warnings {
    QMap<QString, QList<int>> where;
    void add(const QString &message, int slide) {
        auto &list = where[message];
        if (!list.contains(slide)) list.append(slide);
    }
    QStringList lines() const {
        QStringList out;
        for (auto it = where.cbegin(); it != where.cend(); ++it) {
            QStringList numbers;
            for (int n : it.value()) if (n > 0) numbers.append(QString::number(n));
            if (numbers.isEmpty()) out.append(it.key());
            else out.append(QStringLiteral("%1 (slide%2 %3)").arg(it.key(), numbers.size() > 1 ? QStringLiteral("s") : QString(), numbers.join(QStringLiteral(", "))));
        }
        return out;
    }
};

const char *presetFor(int kind) {
    static const char *names[] = {"rect", "ellipse", "triangle", "diamond", "pentagon", "hexagon", "star5", "heart",
                                  "rightArrow", "chevron", "wedgeRectCallout", "parallelogram", "can", "line", "mathPlus",
                                  "mathMultiply", "leftArrow", "leftRightArrow", "flowChartDocument", "trapezoid",
                                  "flowChartTerminator", "cloud", "arc", "donut"};
    return kind >= 0 && kind < 24 ? names[kind] : nullptr;
}

const char *dashFor(int style) {
    switch (style) {
    case 1: return "dash";
    case 2: return "sysDot";
    case 3: return "dashDot";
    case 4: return "lgDashDotDot";
    default: return "solid";
    }
}

const char *patternFor(int style) {
    // Qt::Dense1Pattern … Qt::DiagCrossPattern, in the order the model keeps.
    switch (style) {
    case 2: return "pct90"; case 3: return "pct75"; case 4: return "pct60"; case 5: return "pct50";
    case 6: return "pct30"; case 7: return "pct20"; case 8: return "pct10";
    case 9: return "horz"; case 10: return "vert"; case 11: return "smGrid";
    case 12: return "wdUpDiag"; case 13: return "wdDnDiag"; case 14: return "diagCross";
    default: return "pct50";
    }
}

struct Writer {
    const Document &doc;
    Warnings warnings;
    QVector<Zip::Entry> entries;
    std::shared_ptr<Workers::Job> job;

    qreal e = 1;      // EMU per slide unit
    qint64 cx = 12192000, cy = 6858000;

    QStringList overrides;   // content-type overrides, one line each
    QSet<QString> defaults;  // extensions with a default content type

    // Media are shared: one part per picture identity.
    QHash<QString, QString> mediaByIdentity;
    int mediaCount = 0, chartCount = 0, commentCount = 0;

    explicit Writer(const Document &document) : doc(document) {
        e = 12192000.0 / qMax(1.0, doc.size.width());
        cx = 12192000;
        cy = qRound64(doc.size.height() * e);
    }

    bool canceled() const { return job && job->canceled; }
    qint64 emu(qreal units) const { return qRound64(units * e); }
    int hundredthsPt(qreal units) const { return qMax(100, qRound(units * e / 12700.0 * 100)); }

    void part(const QString &path, const QByteArray &data, const char *contentType) {
        entries.append({path, data, true});
        if (contentType) overrides.append(QStringLiteral("<Override PartName=\"/%1\" ContentType=\"%2\"/>").arg(path, QString::fromLatin1(contentType)));
    }
    void binary(const QString &path, const QByteArray &data, const QString &extension, const QString &mime) {
        entries.append({path, data, !(extension == QLatin1String("png") || extension == QLatin1String("jpeg") || extension == QLatin1String("mp4"))});
        if (!defaults.contains(extension)) {
            defaults.insert(extension);
            overrides.append(QStringLiteral("<Default Extension=\"%1\" ContentType=\"%2\"/>").arg(extension, mime));
        }
    }
    static QString relsPath(const QString &partPath) {
        const int slash = partPath.lastIndexOf(QLatin1Char('/'));
        return partPath.left(slash + 1) + QStringLiteral("_rels/") + partPath.mid(slash + 1) + QStringLiteral(".rels");
    }

    // ---- theme

    QByteArray themeXml(const QString &name) const {
        Xml x;
        x.start("a:theme").attr("xmlns:a", nsA).attr("name", name);
        x.start("a:themeElements");
        x.start("a:clrScheme").attr("name", name);
        const auto c = [&](const char *tag, const QColor &color) { x.start(tag); x.start("a:srgbClr").attr("val", hex(color)).end(); x.end(); };
        const auto fg = doc.theme.colors.value(QStringLiteral("foreground"), QColor(Qt::white));
        const auto bg = doc.theme.colors.value(QStringLiteral("background"), QColor(Qt::black));
        const auto muted = doc.theme.colors.value(QStringLiteral("muted"), fg);
        const auto accent = doc.theme.colors.value(QStringLiteral("accent"), QColor(0x27, 0xc2, 0xff));
        c("a:dk1", fg); c("a:lt1", bg); c("a:dk2", muted); c("a:lt2", bg.lighter(110));
        const auto palette = Chart::palette(doc.theme, 0);
        for (int i = 0; i < 6; ++i) c(QByteArray("a:accent" + QByteArray::number(i + 1)).constData(), i < palette.size() ? palette[i] : accent);
        c("a:hlink", accent); c("a:folHlink", muted);
        x.end();
        x.start("a:fontScheme").attr("name", name);
        for (const auto *tag : {"a:majorFont", "a:minorFont"}) {
            x.start(tag);
            x.start("a:latin").attr("typeface", doc.theme.fonts.value(QString::fromLatin1(tag) == QLatin1String("a:majorFont") ? QStringLiteral("heading") : QStringLiteral("body"), QStringLiteral("Inter"))).end();
            x.start("a:ea").attr("typeface", "").end();
            x.start("a:cs").attr("typeface", "").end();
            x.end();
        }
        x.end();
        // The format scheme every theme must carry, in its plainest form.
        x.start("a:fmtScheme").attr("name", name);
        x.start("a:fillStyleLst");
        for (int i = 0; i < 3; ++i) { x.start("a:solidFill"); x.start("a:schemeClr").attr("val", "phClr").end(); x.end(); }
        x.end();
        x.start("a:lnStyleLst");
        for (int w : {6350, 12700, 19050}) {
            x.start("a:ln").attr("w", w).attr("cap", "flat").attr("cmpd", "sng").attr("algn", "ctr");
            x.start("a:solidFill"); x.start("a:schemeClr").attr("val", "phClr").end(); x.end();
            x.start("a:prstDash").attr("val", "solid").end();
            x.end();
        }
        x.end();
        x.start("a:effectStyleLst");
        for (int i = 0; i < 3; ++i) { x.start("a:effectStyle"); x.empty("a:effectLst"); x.end(); }
        x.end();
        x.start("a:bgFillStyleLst");
        for (int i = 0; i < 3; ++i) { x.start("a:solidFill"); x.start("a:schemeClr").attr("val", "phClr").end(); x.end(); }
        x.end();
        x.end(); // fmtScheme
        x.end(); // themeElements
        x.end();
        return x.finish();
    }

    // ---- shapes

    struct SlideCtx {
        QString partPath;
        Rels *rels = nullptr;
        int number = 0;               // 1-based slide number, 0 for masters and layouts
        int nextId = 2;
        QHash<QString, int> spidByObject;
    };

    void writeXfrm(Xml &x, const QRectF &rect, qreal rotation, const char *tag = "a:xfrm") const {
        x.start(tag);
        if (!qFuzzyIsNull(rotation)) x.attr("rot", qRound64(std::fmod(rotation, 360.0) * 60000));
        x.start("a:off").attr("x", emu(rect.x())).attr("y", emu(rect.y())).end();
        x.start("a:ext").attr("cx", qMax<qint64>(0, emu(rect.width()))).attr("cy", qMax<qint64>(0, emu(rect.height()))).end();
        x.end();
    }

    void writeCustomGeometry(Xml &x, const SceneObject &o) const {
        const auto path = Shape::path(o);
        const QRectF r = o.rect;
        const qreal w = qMax(1.0, r.width()), h = qMax(1.0, r.height());
        x.start("a:custGeom");
        x.empty("a:avLst"); x.empty("a:gdLst"); x.empty("a:ahLst"); x.empty("a:cxnLst");
        x.start("a:rect").attr("l", "0").attr("t", "0").attr("r", "r").attr("b", "b").end();
        x.start("a:pathLst");
        x.start("a:path").attr("w", emu(w)).attr("h", emu(h));
        if (o.fillStyle == 5) x.attr("fill", "none");
        const auto pt = [&](qreal px, qreal py) {
            x.start("a:pt").attr("x", emu(px - r.x())).attr("y", emu(py - r.y())).end();
        };
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto el = path.elementAt(i);
            if (el.isMoveTo()) {
                x.start("a:moveTo"); pt(el.x, el.y); x.end();
            } else if (el.isLineTo()) {
                x.start("a:lnTo"); pt(el.x, el.y); x.end();
            } else if (el.isCurveTo() && i + 2 < path.elementCount()) {
                const auto b = path.elementAt(i + 1), c = path.elementAt(i + 2);
                x.start("a:cubicBezTo"); pt(el.x, el.y); pt(b.x, b.y); pt(c.x, c.y); x.end();
                i += 2;
            }
        }
        if (o.fillStyle != 5 && !o.connector && path.elementCount() > 1) x.empty("a:close");
        x.end(); // path
        x.end(); // pathLst
        x.end(); // custGeom
    }

    void writeFill(Xml &x, const SceneObject &o, int slide) {
        switch (o.fillStyle) {
        case 0: writeSolidFill(x, o.fill, o.opacity); break;
        case 1: case 2: {
            x.start("a:gradFill").attr("rotWithShape", "1");
            x.start("a:gsLst");
            x.start("a:gs").attr("pos", 0); writeColor(x, o.fill, o.opacity); x.end();
            x.start("a:gs").attr("pos", 100000); writeColor(x, o.fillSecondary, o.opacity); x.end();
            x.end();
            if (o.fillStyle == 1) x.start("a:lin").attr("ang", qRound64(std::fmod(o.fillAngle + 360.0, 360.0) * 60000)).attr("scaled", "0").end();
            else { x.start("a:path").attr("path", "circle"); x.start("a:fillToRect").attr("l", 50000).attr("t", 50000).attr("r", 50000).attr("b", 50000).end(); x.end(); }
            x.end();
            break;
        }
        case 3:
            x.start("a:pattFill").attr("prst", patternFor(o.patternStyle));
            x.start("a:fgClr"); writeColor(x, o.fill, o.opacity); x.end();
            x.start("a:bgClr"); writeColor(x, o.fillSecondary, o.opacity); x.end();
            x.end();
            break;
        case 4:
            warnings.add(QStringLiteral("A shape filled with a picture was given a plain fill"), slide);
            writeSolidFill(x, o.fill, o.opacity);
            break;
        default: x.empty("a:noFill"); break;
        }
    }

    void writeLine(Xml &x, const SceneObject &o) const {
        if (o.strokeWidth <= 0) { x.start("a:ln"); x.empty("a:noFill"); x.end(); return; }
        x.start("a:ln").attr("w", qMax<qint64>(1, emu(o.strokeWidth)));
        x.attr("cap", o.strokeCap == 0 ? "flat" : o.strokeCap == 2 ? "sq" : "rnd");
        writeSolidFill(x, o.strokeColor, o.opacity);
        x.start("a:prstDash").attr("val", dashFor(o.strokeStyle)).end();
        if (o.strokeJoin == 0) x.empty("a:miter"); else if (o.strokeJoin == 2) x.empty("a:bevel"); else x.empty("a:round");
        if (o.connector) {
            if (o.connectorArrowStart) x.start("a:headEnd").attr("type", "triangle").end();
            if (o.connectorArrowEnd) x.start("a:tailEnd").attr("type", "triangle").end();
        }
        x.end();
    }

    void writeEffects(Xml &x, const SceneObject &o) const {
        if (!o.shadowEnabled) return;
        x.start("a:effectLst");
        x.start("a:outerShdw").attr("blurRad", 38100).attr("dist", emu(std::hypot(o.shadowX, o.shadowY)))
            .attr("dir", qRound64(std::fmod(qRadiansToDegrees(std::atan2(o.shadowY, o.shadowX)) + 360.0, 360.0) * 60000))
            .attr("algn", "tl").attr("rotWithShape", "0");
        writeColor(x, o.shadowColor);
        x.end();
        x.end();
    }

    void writeNonVisual(Xml &x, const char *tag, const char *cNvTag, const SceneObject &o, SlideCtx &ctx, const QString &fallbackName, bool textBox = false) {
        const int id = ctx.nextId++;
        ctx.spidByObject.insert(o.id, id);
        x.start(tag);
        x.start("p:cNvPr").attr("id", id).attr("name", o.altTitle.isEmpty() ? fallbackName : o.altTitle);
        if (!o.altText.isEmpty()) x.attr("descr", o.altText);
        if (o.hidden) x.attr("hidden", "1");
        x.end();
        x.start(cNvTag);
        if (textBox) x.attr("txBox", "1");
        x.end();
        x.empty("p:nvPr");
        x.end();
    }

    // ---- text

    void writeRunProperties(Xml &x, const char *tag, const SceneObject &o, const TextRun *run, const QString &linkRel) const {
        QString lang = run && !run->language.isEmpty() ? run->language : !o.language.isEmpty() ? o.language : !doc.language.isEmpty() ? doc.language : QStringLiteral("en_GB");
        lang.replace(QLatin1Char('_'), QLatin1Char('-'));
        x.start(tag).attr("lang", lang);
        const qreal size = run && run->fontSize > 0 ? run->fontSize : o.fontSize;
        x.attr("sz", hundredthsPt(size));
        const bool bold = run && run->weight ? run->weight >= 600 : o.fontWeight >= 600;
        const bool italic = run && run->italic ? run->italic == 1 : o.italic;
        const bool underline = run && run->underline ? run->underline == 1 : o.underline;
        x.attr("b", bold ? "1" : "0").attr("i", italic ? "1" : "0").attr("u", underline ? "sng" : "none");
        if (run && run->strike == 1) x.attr("strike", "sngStrike");
        if (run && run->baseline) x.attr("baseline", run->baseline == 1 ? 30000 : -25000);
        if (o.uppercase) x.attr("cap", "all");
        if (!qFuzzyIsNull(o.letterSpacing)) x.attr("spc", qRound(o.letterSpacing * e / 12700.0 * 100));
        const QColor color = run && run->color.isValid() ? run->color : o.textColor;
        writeSolidFill(x, color, o.opacity);
        const QString family = run && !run->fontFamily.isEmpty() ? run->fontFamily : o.fontFamily;
        x.start("a:latin").attr("typeface", family).end();
        if (!linkRel.isEmpty()) x.start("a:hlinkClick").attr("r:id", linkRel).end();
        x.end();
    }

    // The words of a text object, or of a table cell, as a txBody.
    void writeTextBody(Xml &x, const char *tag, const SceneObject &o, SlideCtx &ctx, bool inTable) {
        x.start(tag);
        x.start("a:bodyPr").attr("wrap", "square").attr("lIns", inTable ? 45720 : 0).attr("tIns", inTable ? 45720 : 0).attr("rIns", inTable ? 45720 : 0).attr("bIns", inTable ? 45720 : 0)
            .attr("rtlCol", o.direction == 2 ? "1" : "0")
            .attr("anchor", o.verticalAlign == 1 ? "ctr" : o.verticalAlign == 2 ? "b" : "t");
        if (o.columns > 1) x.attr("numCol", o.columns);
        if (o.textFit == 1) x.empty("a:normAutofit"); else x.empty("a:noAutofit");
        x.end();
        x.empty("a:lstStyle");

        QString linkRel;
        if (!inTable && o.linkKind >= 1 && o.linkKind <= 3 && !o.linkTarget.isEmpty()) {
            if (o.linkKind == 3) {
                int index = -1;
                for (int i = 0; i < doc.slides.size(); ++i) if (doc.slides[i].id == o.linkTarget) index = i;
                if (index >= 0) linkRel = ctx.rels->add(relBase + QStringLiteral("slide"), QStringLiteral("slide%1.xml").arg(index + 1));
            } else {
                const auto target = o.linkKind == 2 ? QStringLiteral("mailto:") + o.linkTarget : o.linkTarget;
                linkRel = ctx.rels->add(relBase + QStringLiteral("hyperlink"), target, true);
            }
        }
        if (o.textKind == 1) warnings.add(QStringLiteral("An equation was written as its source text"), ctx.number);
        if (o.textKind == 2) warnings.add(QStringLiteral("Words on a path were written as an ordinary text box"), ctx.number);

        const auto runs = TextRuns::tidy(o.runs, o.text.size());
        const auto paragraphs = o.text.split(QLatin1Char('\n'));
        int position = 0;
        int number = o.listStart;
        for (const auto &paragraph : paragraphs) {
            int level = 0;
            QString words = paragraph;
            if (o.listStyle > 0) { while (words.startsWith(QLatin1Char('\t'))) { words.remove(0, 1); ++level; } }
            const int skipped = paragraph.size() - words.size();
            x.start("a:p");
            x.start("a:pPr");
            if (o.listStyle > 0) {
                x.attr("marL", qint64(342900) * (level + 1)).attr("indent", -342900).attr("lvl", qMin(8, level));
            }
            x.attr("algn", o.textAlign == 1 ? "ctr" : o.textAlign == 2 ? "r" : o.textAlign == 3 ? "just" : "l");
            if (!qFuzzyCompare(o.lineHeight, 100.0) && o.lineHeight > 0) { x.start("a:lnSpc"); x.start("a:spcPct").attr("val", qRound(o.lineHeight * 1000)).end(); x.end(); }
            if (o.paragraphSpacing > 0) { x.start("a:spcBef"); x.start("a:spcPts").attr("val", qRound(o.paragraphSpacing * e / 12700.0 * 100)).end(); x.end(); }
            switch (o.listStyle) {
            case 0: x.empty("a:buNone"); break;
            case 1: x.start("a:buChar").attr("char", level == 0 ? QString(QChar(0x2022)) : level == 1 ? QString(QChar(0x25E6)) : QString(QChar(0x25AA))).end(); break;
            case 2: x.start("a:buAutoNum").attr("type", "arabicPeriod").attr("startAt", number).end(); break;
            case 3: x.start("a:buChar").attr("char", QString(QChar(0x25E6))).end(); break;
            case 4: x.start("a:buChar").attr("char", QString(QChar(0x25AA))).end(); break;
            case 5: x.start("a:buAutoNum").attr("type", "alphaLcPeriod").attr("startAt", number).end(); break;
            case 6: x.start("a:buAutoNum").attr("type", "alphaUcPeriod").attr("startAt", number).end(); break;
            case 7: x.start("a:buAutoNum").attr("type", "romanLcPeriod").attr("startAt", number).end(); break;
            case 8: x.start("a:buAutoNum").attr("type", "romanUcPeriod").attr("startAt", number).end(); break;
            default: x.empty("a:buNone"); break;
            }
            x.end(); // pPr
            // Runs: split the paragraph where the formatting changes.
            const int start = position + skipped, end = position + paragraph.size();
            int at = start;
            while (at < end) {
                const TextRun *run = TextRuns::at(runs, at);
                int stop = end;
                if (run) stop = qMin(end, run->start + run->length);
                for (const auto &r : runs) if (r.start > at && r.start < stop) stop = r.start;
                x.start("a:r");
                writeRunProperties(x, "a:rPr", o, run, linkRel);
                x.start("a:t").text(o.text.mid(at, stop - at)).end();
                x.end();
                at = stop;
            }
            writeRunProperties(x, "a:endParaRPr", o, nullptr, QString());
            x.end(); // p
            position += paragraph.size() + 1;
        }
        x.end();
    }

    void writeTextObject(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        x.start("p:sp");
        writeNonVisual(x, "p:nvSpPr", "p:cNvSpPr", o, ctx, QStringLiteral("Text %1").arg(ctx.nextId), true);
        x.start("p:spPr");
        writeXfrm(x, o.rect, o.rotation);
        x.start("a:prstGeom").attr("prst", "rect"); x.empty("a:avLst"); x.end();
        x.empty("a:noFill");
        x.end();
        writeTextBody(x, "p:txBody", o, ctx, false);
        x.end();
    }

    void writeShape(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        x.start("p:sp");
        writeNonVisual(x, "p:nvSpPr", "p:cNvSpPr", o, ctx, QStringLiteral("Shape %1").arg(ctx.nextId));
        x.start("p:spPr");
        writeXfrm(x, o.rect, o.rotation);
        const bool rounded = o.shapeKind == 0 && o.cornerRadius > 0;
        const char *preset = o.connector || o.shapeKind == 99 ? nullptr : rounded ? "roundRect" : presetFor(o.shapeKind);
        if (preset) {
            x.start("a:prstGeom").attr("prst", preset);
            x.start("a:avLst");
            if (rounded) {
                // The rounding is a fraction of the shorter side; a pill is 50000.
                const qreal shorter = qMax(1.0, qMin(o.rect.width(), o.rect.height()));
                x.start("a:gd").attr("name", "adj").attr("fmla", QStringLiteral("val %1").arg(qBound(0, qRound(o.cornerRadius / shorter * 100000), 50000))).end();
            }
            x.end();
            x.end();
        } else writeCustomGeometry(x, o);
        writeFill(x, o, ctx.number);
        writeLine(x, o);
        writeEffects(x, o);
        x.end();
        x.end();
    }

    // ---- pictures and films

    QString mediaPart(const QString &identity, const QByteArray &data, const QString &extension, const QString &mime) {
        const auto key = identity.isEmpty() ? QString::number(++mediaCount) + extension : identity + extension;
        auto it = mediaByIdentity.find(key);
        if (it != mediaByIdentity.end()) return *it;
        const auto path = QStringLiteral("ppt/media/media%1.%2").arg(++mediaCount).arg(extension);
        binary(path, data, extension, mime);
        mediaByIdentity.insert(key, path);
        return path;
    }

    static QByteArray pngOf(const QImage &image) {
        QByteArray out;
        QBuffer buffer(&out);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return out;
    }

    void writePictureCommon(Xml &x, const SceneObject &o, SlideCtx &ctx, const QString &blipRel, bool film) {
        x.start("p:blipFill");
        x.start("a:blip").attr("r:embed", blipRel);
        if (!film) {
            if (o.opacity < 0.999) x.start("a:alphaModFix").attr("amt", qRound(o.opacity * 100000)).end();
            if (qFuzzyIsNull(o.imageSaturation)) x.empty("a:grayscl");
            if (!qFuzzyIsNull(o.imageBrightness) || !qFuzzyCompare(o.imageContrast, 1.0))
                x.start("a:lum").attr("bright", qRound(qBound(-1.0, o.imageBrightness, 1.0) * 100000)).attr("contrast", qRound(qBound(-1.0, o.imageContrast - 1.0, 1.0) * 100000)).end();
            if (o.imageTintAmount > 0) warnings.add(QStringLiteral("A picture's tint was left off"), ctx.number);
        }
        x.end();
        const QRectF crop = o.imageCrop;
        if (!film && (crop.x() > 0 || crop.y() > 0 || crop.right() < 1 || crop.bottom() < 1)) {
            x.start("a:srcRect").attr("l", qRound(crop.x() * 100000)).attr("t", qRound(crop.y() * 100000))
                .attr("r", qRound((1 - crop.right()) * 100000)).attr("b", qRound((1 - crop.bottom()) * 100000)).end();
        }
        x.start("a:stretch"); x.empty("a:fillRect"); x.end();
        x.end();
        x.start("p:spPr");
        writeXfrm(x, o.rect, o.rotation);
        const char *mask = o.imageMask == 1 ? "ellipse" : o.imageMask == 2 ? "roundRect" : o.imageMask == 3 ? "hexagon" : o.imageMask == 4 ? "heart" : "rect";
        x.start("a:prstGeom").attr("prst", mask); x.empty("a:avLst"); x.end();
        if (o.strokeWidth > 0) writeLine(x, o);
        writeEffects(x, o);
        x.end();
    }

    void writePicture(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        QByteArray data = o.imageData;
        QString extension = o.imageFormat.toLower(), mime;
        if (extension == QLatin1String("jpg")) extension = QStringLiteral("jpeg");
        if (extension == QLatin1String("svg") || data.isEmpty()) {
            // A vector picture goes as a bitmap of itself at a generous size.
            QImage raster = o.image;
            if (!o.vectorPicture.isNull() && !o.vectorSize.isEmpty()) {
                const qreal scale = qMin(4.0, 2048.0 / qMax(1.0, qMax(o.vectorSize.width(), o.vectorSize.height())));
                raster = QImage((o.vectorSize * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
                raster.fill(Qt::transparent);
                QPainter painter(&raster);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.scale(scale, scale);
                painter.drawPicture(0, 0, o.vectorPicture);
            }
            if (raster.isNull()) { warnings.add(QStringLiteral("A picture had no data to write"), ctx.number); return; }
            data = pngOf(raster);
            extension = QStringLiteral("png");
            if (o.imageFormat.toLower() == QLatin1String("svg")) warnings.add(QStringLiteral("A vector picture was written as a bitmap"), ctx.number);
        }
        mime = extension == QLatin1String("png") ? QStringLiteral("image/png") : extension == QLatin1String("jpeg") ? QStringLiteral("image/jpeg")
             : extension == QLatin1String("gif") ? QStringLiteral("image/gif") : extension == QLatin1String("webp") ? QStringLiteral("image/webp") : QStringLiteral("application/octet-stream");
        const auto path = mediaPart(o.imageId + QLatin1Char('/') + extension, data, extension, mime);
        const auto rel = ctx.rels->add(relBase + QStringLiteral("image"), QStringLiteral("../") + path.mid(4));
        x.start("p:pic");
        writeNonVisual(x, "p:nvPicPr", "p:cNvPicPr", o, ctx, QStringLiteral("Picture %1").arg(ctx.nextId));
        writePictureCommon(x, o, ctx, rel, false);
        x.end();
    }

    void writeMedia(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        // The poster is the first frame; the film itself rides beside it.
        QImage poster = MediaAsset::frameAt(o, qMax(0.0, o.mediaTrimStart));
        if (poster.isNull()) { poster = QImage(64, 36, QImage::Format_RGB32); poster.fill(QColor(30, 30, 30)); }
        const auto posterPath = mediaPart(o.mediaId + QStringLiteral("/poster"), pngOf(poster), QStringLiteral("png"), QStringLiteral("image/png"));
        const auto posterRel = ctx.rels->add(relBase + QStringLiteral("image"), QStringLiteral("../") + posterPath.mid(4));
        QString linkRel, embedRel;
        if (!o.mediaData.isEmpty()) {
            auto extension = QFileInfo(o.mediaName).suffix().toLower();
            if (extension.isEmpty()) extension = o.mediaContainer.section(QLatin1Char(','), 0, 0).toLower();
            if (extension.isEmpty() || extension.size() > 5) extension = o.mediaVideo ? QStringLiteral("mp4") : QStringLiteral("m4a");
            const QString mime = extension == QLatin1String("mp4") || extension == QLatin1String("m4v") ? QStringLiteral("video/mp4")
                               : extension == QLatin1String("mov") ? QStringLiteral("video/quicktime")
                               : extension == QLatin1String("webm") ? QStringLiteral("video/webm")
                               : extension == QLatin1String("mkv") ? QStringLiteral("video/x-matroska")
                               : extension == QLatin1String("mp3") ? QStringLiteral("audio/mpeg")
                               : extension == QLatin1String("wav") ? QStringLiteral("audio/wav")
                               : extension == QLatin1String("m4a") ? QStringLiteral("audio/mp4")
                               : extension == QLatin1String("ogg") || extension == QLatin1String("oga") ? QStringLiteral("audio/ogg")
                               : extension == QLatin1String("flac") ? QStringLiteral("audio/flac")
                               : o.mediaVideo ? QStringLiteral("video/mp4") : QStringLiteral("audio/mpeg");
            const auto path = mediaPart(o.mediaId + QLatin1Char('/') + extension, o.mediaData, extension, mime);
            linkRel = ctx.rels->add(relBase + (o.mediaVideo ? QStringLiteral("video") : QStringLiteral("audio")), QStringLiteral("../") + path.mid(4));
            embedRel = ctx.rels->add(relMedia, QStringLiteral("../") + path.mid(4));
        } else if (!o.mediaPath.isEmpty()) {
            linkRel = ctx.rels->add(relBase + (o.mediaVideo ? QStringLiteral("video") : QStringLiteral("audio")), QUrl::fromLocalFile(o.mediaPath).toString(), true);
            warnings.add(QStringLiteral("A linked film points at its file on this computer; embed it to travel"), ctx.number);
        }
        if (o.mediaTrimStart > 0 || o.mediaTrimEnd > 0) warnings.add(QStringLiteral("A film's trim was left off; the whole file plays"), ctx.number);
        x.start("p:pic");
        const int id = ctx.nextId++;
        ctx.spidByObject.insert(o.id, id);
        x.start("p:nvPicPr");
        x.start("p:cNvPr").attr("id", id).attr("name", o.altTitle.isEmpty() ? (o.mediaName.isEmpty() ? QStringLiteral("Film %1").arg(id) : o.mediaName) : o.altTitle);
        if (!o.altText.isEmpty()) x.attr("descr", o.altText);
        if (!linkRel.isEmpty()) { x.start("a:hlinkClick").attr("r:id", "").attr("action", "ppaction://media").end(); }
        x.end();
        x.start("p:cNvPicPr"); x.start("a:picLocks").attr("noChangeAspect", "1").end(); x.end();
        x.start("p:nvPr");
        if (!linkRel.isEmpty()) {
            x.start(o.mediaVideo ? "a:videoFile" : "a:audioFile").attr("r:link", linkRel).end();
            if (!embedRel.isEmpty()) {
                x.start("p:extLst"); x.start("p:ext").attr("uri", "{DAA4B4D4-6D71-4841-9C94-3DE7FCFB9230}");
                x.start("p14:media").attr("xmlns:p14", nsP14).attr("r:embed", embedRel).end();
                x.end(); x.end();
            }
        }
        x.end();
        x.end();
        writePictureCommon(x, o, ctx, posterRel, true);
        x.end();
    }

    // ---- tables

    void writeTable(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        const auto &t = o.table;
        const int rows = t.rows.size(), columns = t.columns.size();
        if (rows < 1 || columns < 1) return;
        x.start("p:graphicFrame");
        writeNonVisual(x, "p:nvGraphicFramePr", "p:cNvGraphicFramePr", o, ctx, QStringLiteral("Table %1").arg(ctx.nextId));
        writeXfrm(x, o.rect, 0, "p:xfrm");
        if (!qFuzzyIsNull(o.rotation)) warnings.add(QStringLiteral("A turned table was written upright"), ctx.number);
        x.start("a:graphic");
        x.start("a:graphicData").attr("uri", "http://schemas.openxmlformats.org/drawingml/2006/table");
        x.start("a:tbl");
        x.start("a:tblPr");
        if (t.headerRows > 0) x.attr("firstRow", "1");
        if (t.headerColumns > 0) x.attr("firstCol", "1");
        if (t.banded) x.attr("bandRow", "1");
        x.end();
        qreal columnTotal = 0, rowTotal = 0;
        for (qreal c : t.columns) columnTotal += c;
        for (qreal r : t.rows) rowTotal += r;
        x.start("a:tblGrid");
        for (qreal c : t.columns) x.start("a:gridCol").attr("w", qMax<qint64>(1, emu(o.rect.width() * c / qMax(0.0001, columnTotal)))).end();
        x.end();
        for (int r = 0; r < rows; ++r) {
            x.start("a:tr").attr("h", qMax<qint64>(1, emu(o.rect.height() * t.rows[r] / qMax(0.0001, rowTotal))));
            for (int c = 0; c < columns; ++c) {
                const auto &cell = t.cells[r * columns + c];
                x.start("a:tc");
                if (cell.rowSpan == 0 && cell.columnSpan == 0) {
                    // Covered: say which way, by finding the anchor.
                    const int anchorIndex = Table::anchor(t, r, c);
                    const int ar = anchorIndex / columns, ac = anchorIndex % columns;
                    if (ac != c) x.attr("hMerge", "1");
                    if (ar != r) x.attr("vMerge", "1");
                } else {
                    if (cell.columnSpan > 1) x.attr("gridSpan", cell.columnSpan);
                    if (cell.rowSpan > 1) x.attr("rowSpan", cell.rowSpan);
                }
                SceneObject words = Table::textObject(o, r, c);
                words.id = cell.id;
                words.runs.clear();
                words.linkKind = 0;
                if (cell.rowSpan == 0 && cell.columnSpan == 0) words.text.clear();
                writeTextBody(x, "a:txBody", words, ctx, true);
                x.start("a:tcPr").attr("marL", emu(t.padding)).attr("marR", emu(t.padding)).attr("marT", emu(t.padding)).attr("marB", emu(t.padding))
                    .attr("anchor", words.verticalAlign == 1 ? "ctr" : words.verticalAlign == 2 ? "b" : "t");
                const QColor border = QColor(cell.style.value(QStringLiteral("borderColor"), t.borderColor.name(QColor::HexArgb)).toString());
                const qreal width = cell.style.value(QStringLiteral("borderWidth"), t.borderWidth).toDouble();
                for (const auto *side : {"a:lnL", "a:lnR", "a:lnT", "a:lnB"}) {
                    x.start(side).attr("w", qMax<qint64>(0, emu(width)));
                    if (width > 0 && border.isValid() && border.alpha() > 0) writeSolidFill(x, border); else x.empty("a:noFill");
                    x.end();
                }
                if (words.fill.isValid() && words.fill.alpha() > 0) writeSolidFill(x, words.fill); else x.empty("a:noFill");
                x.end();
                x.end(); // tc
            }
            x.end(); // tr
        }
        x.end(); x.end(); x.end();
        x.end();
    }

    // ---- charts

    static bool number(const QString &text, const QString &locale, double *value) {
        QString cleaned = text.trimmed();
        cleaned.remove(QLatin1Char('%')).remove(QStringLiteral("£")).remove(QStringLiteral("€")).remove(QLatin1Char('$'));
        bool ok = false;
        double v = QLocale(locale).toDouble(cleaned, &ok);
        if (!ok) { v = QLocale::c().toDouble(cleaned.remove(QLatin1Char(',')), &ok); }
        if (ok) *value = v;
        return ok;
    }

    void writeChart(Xml &x, const SceneObject &o, SlideCtx &ctx) {
        const auto layout = Chart::layout(o);
        const auto &t = o.table;
        const int columns = t.columns.size(), rows = t.rows.size();
        if (columns < 2 || rows < 2) { warnings.add(QStringLiteral("A chart without data was left out"), ctx.number); return; }
        const int kind = o.chart.kind;
        const bool scatter = kind == 9, pie = kind == 7 || kind == 8;
        const auto chartPath = QStringLiteral("ppt/charts/chart%1.xml").arg(++chartCount);
        const auto rel = ctx.rels->add(relBase + QStringLiteral("chart"), QStringLiteral("../") + chartPath.mid(4));

        Xml c;
        c.start("c:chartSpace").attr("xmlns:c", nsC).attr("xmlns:a", nsA).attr("xmlns:r", nsR);
        c.start("c:roundedCorners").attr("val", "0").end();
        c.start("c:chart");
        if (!o.chart.title.trimmed().isEmpty()) {
            c.start("c:title"); c.start("c:tx"); c.start("c:rich"); c.empty("a:bodyPr"); c.empty("a:lstStyle");
            c.start("a:p"); c.start("a:r"); c.start("a:rPr").attr("lang", "en-GB").attr("sz", hundredthsPt(o.fontSize * 1.2)).attr("b", "0"); writeSolidFill(c, o.textColor); c.end();
            c.start("a:t").text(o.chart.title).end(); c.end(); c.end();
            c.end(); c.end(); c.start("c:overlay").attr("val", "0").end(); c.end();
            c.start("c:autoTitleDeleted").attr("val", "0").end();
        } else c.start("c:autoTitleDeleted").attr("val", "1").end();
        c.start("c:plotArea");
        c.empty("c:layout");
        const char *element = scatter ? "c:scatterChart" : pie ? (kind == 8 ? "c:doughnutChart" : "c:pieChart")
                            : kind == 4 ? "c:lineChart" : kind == 5 || kind == 6 ? "c:areaChart" : "c:barChart";
        c.start(element);
        if (scatter) c.start("c:scatterStyle").attr("val", "lineMarker").end();
        if (kind <= 3) { c.start("c:barDir").attr("val", kind == 1 || kind == 3 ? "bar" : "col").end(); c.start("c:grouping").attr("val", kind >= 2 ? "stacked" : "clustered").end(); }
        else if (kind == 4 || kind == 5 || kind == 6) c.start("c:grouping").attr("val", kind == 6 ? "stacked" : "standard").end();
        c.start("c:varyColors").attr("val", pie ? "1" : "0").end();

        const auto colorFor = [&](int series, int category) {
            const auto &cell = t.cells[pie ? (category + 1) * columns : series + 1];
            if (o.chart.seriesColors.contains(cell.id)) return o.chart.seriesColors.value(cell.id);
            const auto palette = o.chart.resolvedColors.isEmpty() ? Chart::palette(doc.theme, o.chart.palette) : o.chart.resolvedColors;
            return palette.isEmpty() ? QColor(Qt::gray) : palette[(pie ? category : series) % palette.size()];
        };
        const int seriesCount = columns - 1, categoryCount = rows - 1;
        for (int s = 0; s < seriesCount; ++s) {
            c.start("c:ser");
            c.start("c:idx").attr("val", s).end(); c.start("c:order").attr("val", s).end();
            c.start("c:tx"); c.start("c:strRef"); c.start("c:f").text(QStringLiteral("Sheet1!$%1$1").arg(QChar('B' + s))).end();
            c.start("c:strCache"); c.start("c:ptCount").attr("val", 1).end(); c.start("c:pt").attr("idx", 0); c.start("c:v").text(layout.series.value(s)).end(); c.end(); c.end();
            c.end(); c.end();
            if (!pie) {
                c.start("c:spPr");
                if (kind == 4 || scatter) { c.start("a:ln").attr("w", 28575); writeSolidFill(c, colorFor(s, 0)); c.end(); }
                else writeSolidFill(c, colorFor(s, 0));
                c.end();
                if (kind == 4 || scatter) { c.start("c:marker"); c.start("c:symbol").attr("val", "circle").end(); c.start("c:size").attr("val", 6).end(); c.end(); }
            } else {
                for (int k = 0; k < categoryCount; ++k) {
                    c.start("c:dPt"); c.start("c:idx").attr("val", k).end(); c.start("c:bubble3D").attr("val", "0").end();
                    c.start("c:spPr"); writeSolidFill(c, colorFor(s, k)); c.end(); c.end();
                }
            }
            if (o.chart.labels) {
                c.start("c:dLbls");
                for (const auto *flag : {"c:showLegendKey", "c:showVal", "c:showCatName", "c:showSerName", "c:showPercent", "c:showBubbleSize"})
                    c.start(flag).attr("val", QLatin1String(flag) == QLatin1String("c:showVal") ? "1" : "0").end();
                c.end();
            }
            if (scatter) {
                for (const auto *axis : {"c:xVal", "c:yVal"}) {
                    const int column = QLatin1String(axis) == QLatin1String("c:xVal") ? 0 : s + 1;
                    c.start(axis); c.start("c:numRef"); c.start("c:f").text(QStringLiteral("Sheet1!$%1$2:$%1$%2").arg(QChar('A' + column)).arg(rows)).end();
                    c.start("c:numCache"); c.start("c:formatCode").text(QStringLiteral("General")).end(); c.start("c:ptCount").attr("val", categoryCount).end();
                    for (int k = 0; k < categoryCount; ++k) {
                        double v;
                        if (!number(t.cells[(k + 1) * columns + column].text, o.chart.locale, &v)) continue;
                        c.start("c:pt").attr("idx", k); c.start("c:v").text(QString::number(v, 'g', 15)).end(); c.end();
                    }
                    c.end(); c.end(); c.end();
                }
            } else {
                c.start("c:cat"); c.start("c:strRef"); c.start("c:f").text(QStringLiteral("Sheet1!$A$2:$A$%1").arg(rows)).end();
                c.start("c:strCache"); c.start("c:ptCount").attr("val", categoryCount).end();
                for (int k = 0; k < categoryCount; ++k) { c.start("c:pt").attr("idx", k); c.start("c:v").text(layout.categories.value(k)).end(); c.end(); }
                c.end(); c.end(); c.end();
                c.start("c:val"); c.start("c:numRef"); c.start("c:f").text(QStringLiteral("Sheet1!$%1$2:$%1$%2").arg(QChar('B' + s)).arg(rows)).end();
                c.start("c:numCache"); c.start("c:formatCode").text(QStringLiteral("General")).end(); c.start("c:ptCount").attr("val", categoryCount).end();
                for (int k = 0; k < categoryCount; ++k) {
                    double v;
                    if (!number(t.cells[(k + 1) * columns + s + 1].text, o.chart.locale, &v)) continue;
                    c.start("c:pt").attr("idx", k); c.start("c:v").text(QString::number(v, 'g', 15)).end(); c.end();
                }
                c.end(); c.end(); c.end();
            }
            if (kind == 4 || scatter) c.start("c:smooth").attr("val", "0").end();
            c.end(); // ser
        }
        if (kind >= 2 && kind <= 3) { c.start("c:gapWidth").attr("val", 150).end(); c.start("c:overlap").attr("val", 100).end(); }
        else if (kind <= 1) c.start("c:gapWidth").attr("val", 150).end();
        if (kind == 8) c.start("c:holeSize").attr("val", 50).end();
        if (!pie) { c.start("c:axId").attr("val", 111).end(); c.start("c:axId").attr("val", 222).end(); }
        c.end(); // kind element
        if (!pie) {
            const auto axisTitle = [&](const QString &words) {
                if (words.trimmed().isEmpty()) return;
                c.start("c:title"); c.start("c:tx"); c.start("c:rich"); c.empty("a:bodyPr"); c.empty("a:lstStyle");
                c.start("a:p"); c.start("a:r"); c.start("a:rPr").attr("lang", "en-GB").attr("sz", hundredthsPt(o.fontSize)); writeSolidFill(c, o.textColor); c.end();
                c.start("a:t").text(words).end(); c.end(); c.end(); c.end(); c.end();
                c.start("c:overlay").attr("val", "0").end(); c.end();
            };
            const bool horizontal = kind == 1 || kind == 3;
            c.start(scatter ? "c:valAx" : "c:catAx");
            c.start("c:axId").attr("val", 111).end();
            c.start("c:scaling"); c.start("c:orientation").attr("val", "minMax").end(); c.end();
            c.start("c:delete").attr("val", "0").end();
            c.start("c:axPos").attr("val", horizontal ? "l" : "b").end();
            axisTitle(o.chart.xTitle);
            c.start("c:numFmt").attr("formatCode", "General").attr("sourceLinked", "1").end();
            c.start("c:majorTickMark").attr("val", "out").end(); c.start("c:minorTickMark").attr("val", "none").end();
            c.start("c:tickLblPos").attr("val", "nextTo").end();
            c.start("c:crossAx").attr("val", 222).end();
            c.start("c:crosses").attr("val", "autoZero").end();
            if (!scatter) { c.start("c:auto").attr("val", "1").end(); c.start("c:lblAlgn").attr("val", "ctr").end(); c.start("c:lblOffset").attr("val", 100).end(); }
            else c.start("c:crossBetween").attr("val", "midCat").end();
            c.end();
            c.start("c:valAx");
            c.start("c:axId").attr("val", 222).end();
            c.start("c:scaling");
            if (o.chart.logarithmic) c.start("c:logBase").attr("val", 10).end();
            c.start("c:orientation").attr("val", "minMax").end();
            if (o.chart.manualY) { c.start("c:max").attr("val", QString::number(o.chart.maximumY)).end(); c.start("c:min").attr("val", QString::number(o.chart.minimumY)).end(); }
            c.end();
            c.start("c:delete").attr("val", "0").end();
            c.start("c:axPos").attr("val", horizontal ? "b" : "l").end();
            if (o.chart.grid) c.empty("c:majorGridlines");
            axisTitle(o.chart.yTitle);
            c.start("c:numFmt").attr("formatCode", "General").attr("sourceLinked", "1").end();
            c.start("c:majorTickMark").attr("val", "out").end(); c.start("c:minorTickMark").attr("val", "none").end();
            c.start("c:tickLblPos").attr("val", "nextTo").end();
            c.start("c:crossAx").attr("val", 111).end();
            c.start("c:crosses").attr("val", "autoZero").end();
            c.start("c:crossBetween").attr("val", scatter ? "midCat" : "between").end();
            c.end();
        }
        c.end(); // plotArea
        if (o.chart.legend) {
            c.start("c:legend"); c.start("c:legendPos").attr("val", "b").end(); c.start("c:overlay").attr("val", "0").end(); c.end();
        }
        c.start("c:plotVisOnly").attr("val", "1").end();
        c.start("c:dispBlanksAs").attr("val", "gap").end();
        c.end(); // chart
        c.start("c:txPr"); c.empty("a:bodyPr"); c.empty("a:lstStyle");
        c.start("a:p"); c.start("a:pPr"); c.start("a:defRPr").attr("sz", hundredthsPt(o.fontSize)); writeSolidFill(c, o.textColor); c.start("a:latin").attr("typeface", o.fontFamily).end(); c.end(); c.end();
        c.empty("a:endParaRPr"); c.end(); c.end();
        c.end(); // chartSpace
        part(chartPath, c.finish(), "application/vnd.openxmlformats-officedocument.drawingml.chart+xml");

        x.start("p:graphicFrame");
        writeNonVisual(x, "p:nvGraphicFramePr", "p:cNvGraphicFramePr", o, ctx, QStringLiteral("Chart %1").arg(ctx.nextId));
        writeXfrm(x, o.rect, 0, "p:xfrm");
        x.start("a:graphic");
        x.start("a:graphicData").attr("uri", nsC);
        x.start("c:chart").attr("xmlns:c", nsC).attr("r:id", rel).end();
        x.end(); x.end();
        x.end();
    }

    void writeObjects(Xml &x, const QVector<SceneObject> &objects, SlideCtx &ctx) {
        for (const auto &o : objects) {
            if (o.hidden) continue;
            if (canceled()) return;
            switch (o.type) {
            case ObjectType::Rect: writeShape(x, o, ctx); break;
            case ObjectType::Text: writeTextObject(x, o, ctx); break;
            case ObjectType::Image: writePicture(x, o, ctx); break;
            case ObjectType::Media: writeMedia(x, o, ctx); break;
            case ObjectType::Table: writeTable(x, o, ctx); break;
            case ObjectType::Chart: writeChart(x, o, ctx); break;
            }
        }
    }

    void startTree(Xml &x) const {
        x.start("p:spTree");
        x.start("p:nvGrpSpPr"); x.start("p:cNvPr").attr("id", 1).attr("name", "").end(); x.empty("p:cNvGrpSpPr"); x.empty("p:nvPr"); x.end();
        x.start("p:grpSpPr"); x.start("a:xfrm");
        x.start("a:off").attr("x", 0).attr("y", 0).end(); x.start("a:ext").attr("cx", 0).attr("cy", 0).end();
        x.start("a:chOff").attr("x", 0).attr("y", 0).end(); x.start("a:chExt").attr("cx", 0).attr("cy", 0).end();
        x.end(); x.end();
    }

    void writeBackground(Xml &x, const QColor &color) const {
        x.start("p:bg"); x.start("p:bgPr"); writeSolidFill(x, color); x.empty("a:effectLst"); x.end(); x.end();
    }

    // ---- masters and layouts

    struct MasterOut { QString path; int index; };
    QHash<QString, int> masterIndex;   // Master.id → 1-based
    QHash<QString, int> layoutIndex;   // SlideLayout.id → 1-based

    void writeMaster(const Master &m, int index, const QVector<int> &layoutIndices) {
        const auto path = QStringLiteral("ppt/slideMasters/slideMaster%1.xml").arg(index);
        Rels rels;
        Xml x;
        x.start("p:sldMaster").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        x.start("p:cSld").attr("name", m.name);
        writeBackground(x, doc.theme.colors.value(m.backgroundToken, m.background));
        startTree(x);
        SlideCtx ctx; ctx.partPath = path; ctx.rels = &rels;
        QVector<SceneObject> objects;
        for (const auto &o : m.objects) objects.append(Design::themed(doc.theme, o));
        writeObjects(x, objects, ctx);
        x.end(); x.end();
        x.start("p:clrMap").attr("bg1", "lt1").attr("tx1", "dk1").attr("bg2", "lt2").attr("tx2", "dk2").attr("accent1", "accent1").attr("accent2", "accent2")
            .attr("accent3", "accent3").attr("accent4", "accent4").attr("accent5", "accent5").attr("accent6", "accent6").attr("hlink", "hlink").attr("folHlink", "folHlink").end();
        x.start("p:sldLayoutIdLst");
        qint64 layoutId = 2147483649LL + index * 100;
        for (int li : layoutIndices) {
            const auto rel = rels.add(relBase + QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout%1.xml").arg(li));
            x.start("p:sldLayoutId").attr("id", layoutId++).attr("r:id", rel).end();
        }
        x.end();
        x.start("p:txStyles");
        for (const auto *tag : {"p:titleStyle", "p:bodyStyle", "p:otherStyle"}) {
            x.start(tag);
            x.start("a:lvl1pPr").attr("algn", "l");
            x.start("a:defRPr").attr("sz", QLatin1String(tag) == QLatin1String("p:titleStyle") ? 4400 : 2400);
            x.start("a:solidFill"); x.start("a:schemeClr").attr("val", "tx1").end(); x.end();
            x.start("a:latin").attr("typeface", QLatin1String(tag) == QLatin1String("p:titleStyle") ? "+mj-lt" : "+mn-lt").end();
            x.end(); x.end(); x.end();
        }
        x.end();
        x.end();
        rels.add(relBase + QStringLiteral("theme"), QStringLiteral("../theme/theme1.xml"));
        part(path, x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml");
        part(relsPath(path), rels.bytes(), nullptr);
    }

    void writeLayout(const SlideLayout &layout, int index, int master) {
        const auto path = QStringLiteral("ppt/slideLayouts/slideLayout%1.xml").arg(index);
        Rels rels;
        rels.add(relBase + QStringLiteral("slideMaster"), QStringLiteral("../slideMasters/slideMaster%1.xml").arg(master));
        Xml x;
        x.start("p:sldLayout").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP).attr("preserve", "1");
        x.start("p:cSld").attr("name", layout.name);
        startTree(x);
        SlideCtx ctx; ctx.partPath = path; ctx.rels = &rels;
        int bodies = 0;
        for (const auto &placeholder : layout.placeholders) {
            const bool title = placeholder.id == QLatin1String("title");
            if (!title && placeholder.id.startsWith(QLatin1String("body"))) ++bodies;
            SceneObject p = Design::themed(doc.theme, placeholder);
            const int id = ctx.nextId++;
            x.start("p:sp");
            x.start("p:nvSpPr");
            x.start("p:cNvPr").attr("id", id).attr("name", title ? QStringLiteral("Title") : QStringLiteral("Body %1").arg(bodies)).end();
            x.start("p:cNvSpPr"); x.start("a:spLocks").attr("noGrp", "1").end(); x.end();
            x.start("p:nvPr");
            x.start("p:ph");
            if (title) x.attr("type", "title"); else x.attr("idx", bodies);
            x.end();
            x.end(); x.end();
            x.start("p:spPr"); writeXfrm(x, p.rect, p.rotation); x.end();
            p.linkKind = 0;
            writeTextBody(x, "p:txBody", p, ctx, false);
            x.end();
        }
        x.end(); x.end();
        x.start("p:clrMapOvr"); x.empty("a:masterClrMapping"); x.end();
        x.end();
        part(path, x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml");
        part(relsPath(path), rels.bytes(), nullptr);
    }

    // ---- notes

    void writeNotesMaster() {
        Rels rels;
        rels.add(relBase + QStringLiteral("theme"), QStringLiteral("../theme/theme2.xml"));
        Xml x;
        x.start("p:notesMaster").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        x.start("p:cSld");
        writeBackground(x, QColor(Qt::white));
        startTree(x);
        x.start("p:sp"); x.start("p:nvSpPr"); x.start("p:cNvPr").attr("id", 2).attr("name", "Slide Image Placeholder 1").end();
        x.start("p:cNvSpPr"); x.start("a:spLocks").attr("noGrp", "1").attr("noRot", "1").attr("noChangeAspect", "1").end(); x.end();
        x.start("p:nvPr"); x.start("p:ph").attr("type", "sldImg").attr("idx", 2).end(); x.end(); x.end();
        x.start("p:spPr"); x.start("a:xfrm"); x.start("a:off").attr("x", 685800).attr("y", 1143000).end(); x.start("a:ext").attr("cx", 5486400).attr("cy", 3086100).end(); x.end();
        x.start("a:prstGeom").attr("prst", "rect"); x.empty("a:avLst"); x.end(); x.empty("a:noFill"); x.end(); x.end();
        x.start("p:sp"); x.start("p:nvSpPr"); x.start("p:cNvPr").attr("id", 3).attr("name", "Notes Placeholder 2").end();
        x.start("p:cNvSpPr"); x.start("a:spLocks").attr("noGrp", "1").end(); x.end();
        x.start("p:nvPr"); x.start("p:ph").attr("type", "body").attr("idx", 1).end(); x.end(); x.end();
        x.start("p:spPr"); x.start("a:xfrm"); x.start("a:off").attr("x", 685800).attr("y", 4343400).end(); x.start("a:ext").attr("cx", 5486400).attr("cy", 4114800).end(); x.end();
        x.start("a:prstGeom").attr("prst", "rect"); x.empty("a:avLst"); x.end(); x.end();
        x.start("p:txBody"); x.empty("a:bodyPr"); x.empty("a:lstStyle"); x.start("a:p"); x.start("a:r"); x.start("a:rPr").attr("lang", "en-GB").end(); x.start("a:t").text(QStringLiteral("Notes")).end(); x.end(); x.end(); x.end();
        x.end();
        x.end(); x.end();
        x.start("p:clrMap").attr("bg1", "lt1").attr("tx1", "dk1").attr("bg2", "lt2").attr("tx2", "dk2").attr("accent1", "accent1").attr("accent2", "accent2")
            .attr("accent3", "accent3").attr("accent4", "accent4").attr("accent5", "accent5").attr("accent6", "accent6").attr("hlink", "hlink").attr("folHlink", "folHlink").end();
        x.start("p:notesStyle"); x.start("a:lvl1pPr"); x.start("a:defRPr").attr("sz", 1200); x.start("a:solidFill"); x.start("a:schemeClr").attr("val", "tx1").end(); x.end(); x.start("a:latin").attr("typeface", "+mn-lt").end(); x.end(); x.end(); x.end();
        x.end();
        part(QStringLiteral("ppt/notesMasters/notesMaster1.xml"), x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.notesMaster+xml");
        part(relsPath(QStringLiteral("ppt/notesMasters/notesMaster1.xml")), rels.bytes(), nullptr);
    }

    void writeNotes(int number, const QString &notes) {
        const auto path = QStringLiteral("ppt/notesSlides/notesSlide%1.xml").arg(number);
        Rels rels;
        rels.add(relBase + QStringLiteral("notesMaster"), QStringLiteral("../notesMasters/notesMaster1.xml"));
        rels.add(relBase + QStringLiteral("slide"), QStringLiteral("../slides/slide%1.xml").arg(number));
        Xml x;
        x.start("p:notes").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        x.start("p:cSld");
        startTree(x);
        x.start("p:sp"); x.start("p:nvSpPr"); x.start("p:cNvPr").attr("id", 2).attr("name", "Slide Image Placeholder 1").end();
        x.start("p:cNvSpPr"); x.start("a:spLocks").attr("noGrp", "1").attr("noRot", "1").attr("noChangeAspect", "1").end(); x.end();
        x.start("p:nvPr"); x.start("p:ph").attr("type", "sldImg").end(); x.end(); x.end();
        x.empty("p:spPr"); x.end();
        x.start("p:sp"); x.start("p:nvSpPr"); x.start("p:cNvPr").attr("id", 3).attr("name", "Notes Placeholder 2").end();
        x.start("p:cNvSpPr"); x.start("a:spLocks").attr("noGrp", "1").end(); x.end();
        x.start("p:nvPr"); x.start("p:ph").attr("type", "body").attr("idx", 1).end(); x.end(); x.end();
        x.empty("p:spPr");
        x.start("p:txBody"); x.empty("a:bodyPr"); x.empty("a:lstStyle");
        for (const auto &paragraph : notes.split(QLatin1Char('\n'))) {
            x.start("a:p"); x.start("a:r"); x.start("a:rPr").attr("lang", "en-GB").end(); x.start("a:t").text(paragraph).end(); x.end(); x.end();
        }
        x.end(); x.end();
        x.end(); x.end();
        x.start("p:clrMapOvr"); x.empty("a:masterClrMapping"); x.end();
        x.end();
        part(path, x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml");
        part(relsPath(path), rels.bytes(), nullptr);
    }

    // ---- comments

    QStringList authors;
    int authorOf(const QString &name) {
        const auto who = name.isEmpty() ? QStringLiteral("Unknown") : name;
        if (!authors.contains(who)) authors.append(who);
        return authors.indexOf(who);
    }

    void writeComments(int number, const Slide &slide, Rels &rels) {
        QVector<const Comment *> mine;
        for (const auto &c : doc.comments) if (c.slideId == slide.id) mine.append(&c);
        if (mine.isEmpty()) return;
        const auto path = QStringLiteral("ppt/comments/comment%1.xml").arg(++commentCount);
        Xml x;
        x.start("p:cmLst").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        int idx = 0;
        for (const auto *c : mine) {
            x.start("p:cm").attr("authorId", authorOf(c->author)).attr("idx", ++idx);
            if (!c->created.isEmpty()) x.attr("dt", c->created);
            QPointF where(10, 10);
            if (const auto *o = slide.find(c->objectId)) where = QPointF(o->rect.x() * e / 12700.0, o->rect.y() * e / 12700.0);
            x.start("p:pos").attr("x", qRound64(where.x())).attr("y", qRound64(where.y())).end();
            QString words = c->text;
            if (!c->parentId.isEmpty()) words = QStringLiteral("Re: ") + words;
            if (c->resolved) words += QStringLiteral(" [resolved]");
            x.start("p:text").text(words).end();
            x.end();
        }
        x.end();
        part(path, x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.comments+xml");
        rels.add(relBase + QStringLiteral("comments"), QStringLiteral("../") + path.mid(4));
    }

    void writeAuthors() {
        if (authors.isEmpty()) return;
        Xml x;
        x.start("p:cmAuthorLst").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        for (int i = 0; i < authors.size(); ++i) {
            QString initials;
            for (const auto &word : authors[i].split(QLatin1Char(' '), Qt::SkipEmptyParts)) initials += word.at(0).toUpper();
            x.start("p:cmAuthor").attr("id", i).attr("name", authors[i]).attr("initials", initials.left(3)).attr("lastIdx", 1).attr("clrIdx", i % 8).end();
        }
        x.end();
        part(QStringLiteral("ppt/commentAuthors.xml"), x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.commentAuthors+xml");
    }

    // ---- transitions and builds

    void writeTransition(Xml &x, int index, const Slide &slide) {
        const int kind = Presentation::transitionKind(doc, index);
        const qreal seconds = Presentation::transitionSeconds(doc, index);
        const bool advance = slide.advanceAfter >= 0;
        if (kind == 0 && !advance) return;
        x.start("p:transition").attr("spd", seconds < 0.6 ? "fast" : seconds < 0.9 ? "med" : "slow");
        if (advance) x.attr("advTm", qRound64(slide.advanceAfter * 1000));
        if (kind == 1) x.empty("p:fade");
        else if (kind == 2) x.start("p:push").attr("dir", slide.transitionDirection == 1 ? "r" : slide.transitionDirection == 2 ? "u" : slide.transitionDirection == 3 ? "d" : "l").end();
        else if (kind == 3) { x.empty("p:fade"); warnings.add(QStringLiteral("The morph transition was written as a fade"), index + 1); }
        x.end();
    }

    void writeBuilds(Xml &x, int number, const Slide &slide, const SlideCtx &ctx) {
        struct Step { const BuildStep *step; int spid; };
        QVector<Step> steps;
        bool emphasisDropped = false, revealDropped = false;
        for (const auto &s : slide.timeline.steps) {
            if (s.effect == Effect::None || s.effect == Effect::Media || s.effect == Effect::Path) {
                if (s.effect == Effect::Path) warnings.add(QStringLiteral("A build along a path was left out"), number);
                continue;
            }
            const int spid = ctx.spidByObject.value(s.targetId, 0);
            if (!spid) continue;
            if (s.effect == Effect::Reveal) revealDropped = true;
            if (s.effect == Effect::Pulse) emphasisDropped = true;
            steps.append({&s, spid});
        }
        if (revealDropped) warnings.add(QStringLiteral("A text reveal was written as a fade of the whole box"), number);
        if (emphasisDropped) warnings.add(QStringLiteral("An emphasis build was written as a grow and shrink"), number);
        if (steps.isEmpty()) return;
        int id = 1;
        x.start("p:timing");
        x.start("p:tnLst"); x.start("p:par");
        x.start("p:cTn").attr("id", id++).attr("dur", "indefinite").attr("restart", "never").attr("nodeType", "tmRoot");
        x.start("p:childTnLst");
        x.start("p:seq").attr("concurrent", "1").attr("nextAc", "seek");
        x.start("p:cTn").attr("id", id++).attr("dur", "indefinite").attr("nodeType", "mainSeq");
        x.start("p:childTnLst");
        bool clickOpen = false;
        qreal previousEnd = 0;
        const auto cond = [&](const char *delay) { x.start("p:stCondLst"); x.start("p:cond").attr("delay", delay).end(); x.end(); };
        const auto target = [&](int spid) { x.start("p:tgtEl"); x.start("p:spTgt").attr("spid", spid).end(); x.end(); };
        for (int i = 0; i < steps.size(); ++i) {
            const auto &s = *steps[i].step;
            const int spid = steps[i].spid;
            const bool click = i == 0 || s.trigger == BuildTrigger::OnClick || s.trigger == BuildTrigger::Absolute;
            if (click) {
                if (clickOpen) { x.end(); x.end(); x.end(); x.end(); x.end(); x.end(); }
                x.start("p:par"); x.start("p:cTn").attr("id", id++).attr("fill", "hold"); cond("indefinite"); x.start("p:childTnLst");
                x.start("p:par"); x.start("p:cTn").attr("id", id++).attr("fill", "hold"); cond("0"); x.start("p:childTnLst");
                clickOpen = true;
                previousEnd = 0;
            }
            const qint64 ms = qMax<qint64>(1, qRound64(s.duration * 1000));
            const qint64 delay = qRound64(s.delay * 1000) + (s.trigger == BuildTrigger::AfterPrevious ? qRound64(previousEnd * 1000) : 0);
            const bool out = s.phase == BuildPhase::Out;
            const char *presetClass = s.effect == Effect::Pulse ? "emph" : out ? "exit" : "entr";
            const int presetId = s.effect == Effect::Pulse ? 6 : s.effect == Effect::Rise ? 2 : s.effect == Effect::Scale ? 53 : s.effect == Effect::Spin ? 49 : 10;
            x.start("p:par");
            x.start("p:cTn").attr("id", id++).attr("presetID", presetId).attr("presetClass", presetClass).attr("presetSubtype", s.effect == Effect::Rise ? 4 : 0).attr("fill", "hold")
                .attr("nodeType", click ? "clickEffect" : s.trigger == BuildTrigger::AfterPrevious ? "afterEffect" : "withEffect");
            cond(QByteArray::number(delay).constData());
            x.start("p:childTnLst");
            if (s.effect == Effect::Pulse) {
                x.start("p:animScale"); x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", ms / 2).attr("autoRev", "1").attr("fill", "hold").end(); target(spid); x.end();
                x.start("p:by").attr("x", qRound(100000 + qMax(0.05, s.amount > 0 ? s.amount : 0.2) * 100000)).attr("y", qRound(100000 + qMax(0.05, s.amount > 0 ? s.amount : 0.2) * 100000)).end();
                x.end();
            } else {
                if (!out) {
                    x.start("p:set"); x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", 1).attr("fill", "hold"); cond("0"); x.end(); target(spid);
                    x.start("p:attrNameLst"); x.start("p:attrName").text(QStringLiteral("style.visibility")).end(); x.end(); x.end();
                    x.start("p:to"); x.start("p:strVal").attr("val", "visible").end(); x.end(); x.end();
                }
                x.start("p:animEffect").attr("transition", out ? "out" : "in").attr("filter", "fade");
                x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", ms).end(); target(spid); x.end(); x.end();
                if (s.effect == Effect::Rise || s.effect == Effect::Move) {
                    // Arrives from below (or from where the move says), settling in place.
                    const qreal fromX = s.effect == Effect::Move ? s.amountX : 0, fromY = s.effect == Effect::Move ? s.amountY : doc.size.height() * 0.1;
                    for (const auto *axis : {"ppt_x", "ppt_y"}) {
                        const bool xAxis = QLatin1String(axis) == QLatin1String("ppt_x");
                        const qreal offset = (xAxis ? fromX : fromY) / (xAxis ? doc.size.width() : doc.size.height());
                        if (qFuzzyIsNull(offset)) continue;
                        x.start("p:anim").attr("calcmode", "lin").attr("valueType", "num");
                        x.start("p:cBhvr").attr("additive", "base"); x.start("p:cTn").attr("id", id++).attr("dur", ms).attr("fill", "hold").end(); target(spid);
                        x.start("p:attrNameLst"); x.start("p:attrName").text(QString::fromLatin1(axis)).end(); x.end(); x.end();
                        x.start("p:tavLst");
                        x.start("p:tav").attr("tm", 0); x.start("p:val"); x.start("p:strVal").attr("val", QStringLiteral("#%1%2%3").arg(QString::fromLatin1(axis), out ? "" : "+", QString::number(out ? 0 : offset, 'f', 4))).end(); x.end(); x.end();
                        x.start("p:tav").attr("tm", 100000); x.start("p:val"); x.start("p:strVal").attr("val", QStringLiteral("#%1%2").arg(QString::fromLatin1(axis), out ? QStringLiteral("+") + QString::number(offset, 'f', 4) : QString())).end(); x.end(); x.end();
                        x.end(); x.end();
                    }
                } else if (s.effect == Effect::Scale) {
                    x.start("p:animScale"); x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", ms).attr("fill", "hold").end(); target(spid); x.end();
                    x.start(out ? "p:to" : "p:from").attr("x", 10000).attr("y", 10000).end();
                    x.start(out ? "p:from" : "p:to").attr("x", 100000).attr("y", 100000).end();
                    x.end();
                } else if (s.effect == Effect::Spin) {
                    x.start("p:animRot").attr("by", qRound64((s.amount > 0 ? s.amount : 360) * 60000));
                    x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", ms).attr("fill", "hold").end(); target(spid);
                    x.start("p:attrNameLst"); x.start("p:attrName").text(QStringLiteral("r")).end(); x.end(); x.end(); x.end();
                }
                if (out) {
                    x.start("p:set"); x.start("p:cBhvr"); x.start("p:cTn").attr("id", id++).attr("dur", 1).attr("fill", "hold"); cond(QByteArray::number(qint64(ms - 1)).constData()); x.end(); target(spid);
                    x.start("p:attrNameLst"); x.start("p:attrName").text(QStringLiteral("style.visibility")).end(); x.end(); x.end();
                    x.start("p:to"); x.start("p:strVal").attr("val", "hidden").end(); x.end(); x.end();
                }
            }
            x.end(); // childTnLst
            x.end(); // cTn
            x.end(); // par (effect)
            previousEnd = (delay + ms) / 1000.0;
        }
        if (clickOpen) { x.end(); x.end(); x.end(); x.end(); x.end(); x.end(); }
        x.end(); // childTnLst of mainSeq
        x.end(); // cTn mainSeq
        x.start("p:prevCondLst"); x.start("p:cond").attr("evt", "onPrev").attr("delay", "0"); x.start("p:tgtEl"); x.empty("p:sldTgt"); x.end(); x.end(); x.end();
        x.start("p:nextCondLst"); x.start("p:cond").attr("evt", "onNext").attr("delay", "0"); x.start("p:tgtEl"); x.empty("p:sldTgt"); x.end(); x.end(); x.end();
        x.end(); // seq
        x.end(); // childTnLst root
        x.end(); // cTn root
        x.end(); // par
        x.end(); // tnLst
        x.start("p:bldLst");
        QSet<int> listed;
        for (const auto &s : steps) {
            if (listed.contains(s.spid)) continue;
            listed.insert(s.spid);
            x.start("p:bldP").attr("spid", s.spid).attr("grpId", 0).end();
        }
        x.end();
        x.end(); // timing
    }

    // ---- slides

    void writeSlide(int index) {
        const auto &slide = doc.slides[index];
        const int number = index + 1;
        const auto path = QStringLiteral("ppt/slides/slide%1.xml").arg(number);
        Rels rels;
        const auto *layout = Design::layout(doc, slide.layoutId);
        int layoutNumber = layoutIndex.value(slide.layoutId, 0);
        if (!layoutNumber) layoutNumber = layoutIndex.isEmpty() ? 1 : layoutIndex.cbegin().value();
        rels.add(relBase + QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout%1.xml").arg(layoutNumber));
        Q_UNUSED(layout);

        const Slide resolved = Design::resolve(doc, index);
        Xml x;
        x.start("p:sld").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP);
        if (slide.skipped) x.attr("show", "0");
        if (!slide.showMasterObjects) x.attr("showMasterSp", "0");
        x.start("p:cSld");
        writeBackground(x, resolved.background);
        startTree(x);
        SlideCtx ctx; ctx.partPath = path; ctx.rels = &rels; ctx.number = number;
        QVector<SceneObject> objects;
        for (const auto &o : resolved.objects) {
            // The master's own shapes are on the master; fields are written as words.
            if (o.id.startsWith(QLatin1String("@master/"))) continue;
            objects.append(o);
        }
        writeObjects(x, objects, ctx);
        x.end(); // spTree
        x.end(); // cSld
        x.start("p:clrMapOvr"); x.empty("a:masterClrMapping"); x.end();
        writeTransition(x, index, slide);
        writeBuilds(x, number, slide, ctx);
        x.end();
        if (!slide.notes.trimmed().isEmpty()) {
            writeNotes(number, slide.notes);
            rels.add(relBase + QStringLiteral("notesSlide"), QStringLiteral("../notesSlides/notesSlide%1.xml").arg(number));
        }
        writeComments(number, slide, rels);
        part(path, x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.slide+xml");
        part(relsPath(path), rels.bytes(), nullptr);
    }

    // ---- the deck

    QByteArray run() {
        Document copy = doc;
        Q_UNUSED(copy);
        // Masters and layouts, numbered; every layout belongs to a master.
        QVector<QVector<int>> layoutsOfMaster(doc.masters.size());
        int layoutNumber = 0;
        for (const auto &layout : doc.layouts) {
            int master = 0;
            for (int i = 0; i < doc.masters.size(); ++i) if (doc.masters[i].id == layout.masterId) master = i;
            ++layoutNumber;
            layoutIndex.insert(layout.id, layoutNumber);
            if (!doc.masters.isEmpty()) layoutsOfMaster[master].append(layoutNumber);
        }
        if (doc.masters.isEmpty()) {
            Master plain;
            plain.id = QStringLiteral("plain");
            plain.name = QStringLiteral("Plain");
            plain.background = doc.theme.colors.value(QStringLiteral("background"), QColor(Qt::white));
            plain.backgroundToken.clear();
            QVector<int> mine;
            if (doc.layouts.isEmpty()) {
                SlideLayout blank; blank.id = QStringLiteral("blank"); blank.name = QStringLiteral("Blank"); blank.masterId = plain.id;
                writeLayout(blank, 1, 1);
                layoutIndex.insert(blank.id, 1);
                mine.append(1);
            } else for (int i = 1; i <= doc.layouts.size(); ++i) mine.append(i);
            writeMaster(plain, 1, mine);
        } else {
            for (int i = 0; i < doc.masters.size(); ++i) {
                if (layoutsOfMaster[i].isEmpty()) {
                    // PowerPoint needs a layout under every master.
                    SlideLayout blank; blank.id = QStringLiteral("blank-") + doc.masters[i].id; blank.name = QStringLiteral("Blank"); blank.masterId = doc.masters[i].id;
                    writeLayout(blank, ++layoutNumber, i + 1);
                    layoutIndex.insert(blank.id, layoutNumber);
                    layoutsOfMaster[i].append(layoutNumber);
                }
                writeMaster(doc.masters[i], i + 1, layoutsOfMaster[i]);
            }
        }
        for (const auto &layout : doc.layouts) {
            int master = 0;
            for (int i = 0; i < doc.masters.size(); ++i) if (doc.masters[i].id == layout.masterId) master = i;
            writeLayout(layout, layoutIndex.value(layout.id), master + 1);
        }
        part(QStringLiteral("ppt/theme/theme1.xml"), themeXml(doc.theme.name.isEmpty() ? QStringLiteral("OmaShow") : doc.theme.name), "application/vnd.openxmlformats-officedocument.theme+xml");
        part(QStringLiteral("ppt/theme/theme2.xml"), themeXml(QStringLiteral("Notes")), "application/vnd.openxmlformats-officedocument.theme+xml");
        writeNotesMaster();

        for (int i = 0; i < doc.slides.size(); ++i) {
            if (canceled()) return QByteArray();
            writeSlide(i);
        }
        writeAuthors();

        // The presentation part ties it together.
        Rels rels;
        Xml x;
        x.start("p:presentation").attr("xmlns:a", nsA).attr("xmlns:r", nsR).attr("xmlns:p", nsP).attr("saveSubsetFonts", "1");
        x.start("p:sldMasterIdLst");
        const int masterCount = doc.masters.isEmpty() ? 1 : doc.masters.size();
        for (int i = 1; i <= masterCount; ++i) {
            const auto rel = rels.add(relBase + QStringLiteral("slideMaster"), QStringLiteral("slideMasters/slideMaster%1.xml").arg(i));
            x.start("p:sldMasterId").attr("id", 2147483648LL + i - 1).attr("r:id", rel).end();
        }
        x.end();
        x.start("p:notesMasterIdLst");
        x.start("p:notesMasterId").attr("r:id", rels.add(relBase + QStringLiteral("notesMaster"), QStringLiteral("notesMasters/notesMaster1.xml"))).end();
        x.end();
        QStringList slideRels;
        x.start("p:sldIdLst");
        for (int i = 0; i < doc.slides.size(); ++i) {
            const auto rel = rels.add(relBase + QStringLiteral("slide"), QStringLiteral("slides/slide%1.xml").arg(i + 1));
            slideRels.append(rel);
            x.start("p:sldId").attr("id", 256 + i).attr("r:id", rel).end();
        }
        x.end();
        x.start("p:sldSz").attr("cx", cx).attr("cy", cy).end();
        x.start("p:notesSz").attr("cx", 6858000).attr("cy", 9144000).end();
        if (!doc.shows.isEmpty()) {
            x.start("p:custShowLst");
            int showId = 0;
            for (const auto &show : doc.shows) {
                x.start("p:custShow").attr("name", show.name).attr("id", showId++);
                x.start("p:sldLst");
                for (const auto &slideId : show.slideIds)
                    for (int i = 0; i < doc.slides.size(); ++i)
                        if (doc.slides[i].id == slideId) x.start("p:sld").attr("r:id", slideRels[i]).end();
                x.end(); x.end();
            }
            x.end();
        }
        x.start("p:defaultTextStyle");
        x.start("a:defPPr"); x.start("a:defRPr").attr("lang", "en-GB").end(); x.end();
        x.start("a:lvl1pPr").attr("marL", 0).attr("algn", "l"); x.start("a:defRPr").attr("sz", 1800); x.start("a:latin").attr("typeface", "+mn-lt").end(); x.end(); x.end();
        x.end();
        if (!doc.sections.isEmpty()) {
            // Every slide belongs somewhere; the ones before any section share a nameless one.
            struct Group { QString name; QVector<int> slides; };
            QVector<Group> groups;
            for (int i = 0; i < doc.slides.size(); ++i) {
                QString name;
                for (const auto &s : doc.sections) if (s.id == doc.slides[i].sectionId) name = s.name;
                if (groups.isEmpty() || groups.last().name != name) groups.append({name, {}});
                groups.last().slides.append(i);
            }
            x.start("p:extLst"); x.start("p:ext").attr("uri", "{521415D9-36F7-43E2-AB2F-B90AF26B5E84}");
            x.start("p14:sectionLst").attr("xmlns:p14", nsP14);
            for (const auto &g : groups) {
                x.start("p14:section").attr("name", g.name.isEmpty() ? QStringLiteral("Untitled Section") : g.name).attr("id", QUuid::createUuid().toString(QUuid::WithBraces).toUpper());
                x.start("p14:sldIdLst");
                for (int i : g.slides) x.start("p14:sldId").attr("id", 256 + i).end();
                x.end(); x.end();
            }
            x.end(); x.end(); x.end();
        }
        x.end();
        if (!authors.isEmpty()) rels.add(relBase + QStringLiteral("commentAuthors"), QStringLiteral("commentAuthors.xml"));
        rels.add(relBase + QStringLiteral("theme"), QStringLiteral("theme/theme1.xml"));
        part(QStringLiteral("ppt/presentation.xml"), x.finish(), "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml");
        part(relsPath(QStringLiteral("ppt/presentation.xml")), rels.bytes(), nullptr);

        Rels root;
        root.add(relBase + QStringLiteral("officeDocument"), QStringLiteral("ppt/presentation.xml"));
        entries.prepend({QStringLiteral("_rels/.rels"), root.bytes(), true});

        QByteArray types = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                           "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                           "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                           "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
        for (const auto &line : overrides) types += line.toUtf8();
        types += "</Types>";
        entries.prepend({QStringLiteral("[Content_Types].xml"), types, true});
        return Zip::write(entries);
    }
};

} // namespace

namespace PptxWriter {

QByteArray bytes(const Document &document, QStringList *warnings, const std::shared_ptr<Workers::Job> &job) {
    Writer writer(document);
    writer.job = job;
    const auto out = writer.run();
    if (warnings) *warnings = writer.warnings.lines();
    return out;
}

Report write(const Document &document, const QString &path, const std::shared_ptr<Workers::Job> &job) {
    Report report;
    if (document.slides.isEmpty()) { report.error = QStringLiteral("The deck has no slides to write."); return report; }
    QStringList warnings;
    const auto data = bytes(document, &warnings, job);
    if (job && job->canceled) { report.error = QStringLiteral("Cancelled."); return report; }
    if (data.isEmpty()) { report.error = QStringLiteral("The deck could not be written as PowerPoint."); return report; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        report.error = QStringLiteral("%1 could not be written.").arg(QFileInfo(path).fileName());
        return report;
    }
    report.ok = true;
    report.bytes = data.size();
    report.lines = warnings;
    report.lines.prepend(QStringLiteral("Wrote %1 slides as PowerPoint.").arg(document.slides.size()));
    return report;
}

} // namespace PptxWriter
