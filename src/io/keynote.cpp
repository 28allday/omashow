#include "io/keynote.h"

#include "core/edit.h"
#include "core/imageasset.h"
#include "core/shape.h"
#include "core/svgasset.h"
#include "core/table.h"
#include "core/textruns.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPainterPath>
#include <QSet>
#include <QtMath>

#include <libetonyek/libetonyek.h>
#include <librevenge-stream/librevenge-stream.h>
#include <librevenge/librevenge.h>

namespace {

using librevenge::RVNGProperty;
using librevenge::RVNGPropertyList;
using librevenge::RVNGPropertyListVector;
using librevenge::RVNGString;

QString str(const RVNGPropertyList &props, const char *name) {
    const RVNGProperty *p = props[name];
    return p ? QString::fromUtf8(p->getStr().cstr()) : QString();
}

bool has(const RVNGPropertyList &props, const char *name) { return props[name] != nullptr; }

// A length in inches, whatever unit it was given in.
double inches(const RVNGPropertyList &props, const char *name, double fallback = 0) {
    const RVNGProperty *p = props[name];
    if (!p) return fallback;
    switch (p->getUnit()) {
    case librevenge::RVNG_INCH: return p->getDouble();
    case librevenge::RVNG_POINT: return p->getDouble() / 72.0;
    case librevenge::RVNG_TWIP: return p->getDouble() / 1440.0;
    default: return p->getDouble();
    }
}

double number(const RVNGPropertyList &props, const char *name, double fallback = 0) {
    const RVNGProperty *p = props[name];
    return p ? p->getDouble() : fallback;
}

QColor color(const RVNGPropertyList &props, const char *name) {
    const auto text = str(props, name);
    return text.isEmpty() ? QColor() : QColor(text);
}

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

// What a span looks like, and what a paragraph does.
struct SpanLook {
    qreal sizePt = 0;
    int bold = -1, italic = -1, underline = -1, strike = -1;
    QColor color;
    QString family, language;
    int baseline = 0;
    bool caps = false, capitalize = false;
    qreal letterSpacingPt = 0;
};

struct ParaLook {
    int align = -1;
    qreal lineHeight = 0;     // percent
    qreal spaceBeforePt = -1;
    bool bullet = false, numbered = false;
    int level = 0;
};

// The words of one text object, table cell or notes page while they arrive.
struct TextBuffer {
    QString text;
    struct Piece { int start, length; SpanLook look; };
    QVector<Piece> pieces;
    SpanLook box;
    ParaLook para;
    bool haveBox = false, firstParagraph = true;
    int bulleted = 0, numbered = 0, plain = 0;
    int listDepth = 0;
    QString link;

    void beginParagraph(const ParaLook &look) {
        if (!firstParagraph) text += QLatin1Char('\n');
        firstParagraph = false;
        para = look;
        if (look.bullet || look.numbered) text += QString(qMax(0, look.level - 1), QLatin1Char('\t'));
        if (!haveBox) { /* filled by the first span */ }
        anyInParagraph = false;
    }
    void endParagraph() {
        if (anyInParagraph) { if (para.numbered) ++numbered; else if (para.bullet) ++bulleted; else ++plain; }
    }
    void insert(QString words, const SpanLook &look) {
        if (words.isEmpty()) return;
        if (look.capitalize) {
            // Every word begins with a capital, as Keynote shows it.
            bool boundary = text.isEmpty() || text.back().isSpace();
            for (auto &ch : words) { if (boundary) ch = ch.toUpper(); boundary = ch.isSpace(); }
        }
        pieces.append({text.size(), words.size(), look});
        text += words;
        anyInParagraph = true;
        if (!haveBox) { box = look; boxPara = para; haveBox = true; }
    }
    bool anyInParagraph = false;
    ParaLook boxPara;
    bool empty() const { return text.trimmed().isEmpty(); }
};

class Generator : public librevenge::RVNGPresentationInterface {
public:
    Document doc;
    Warnings warnings;
    QString error;

    // ---- geometry
    qreal k = 1;      // slide units per inch
    qreal pt = 1;     // slide units per point
    bool sized = false;

    void sizeFrom(const RVNGPropertyList &props) {
        if (sized) return;
        const double w = inches(props, "svg:width"), h = inches(props, "svg:height");
        if (w <= 0 || h <= 0) return;
        doc.size = QSizeF(1920, qRound(1920 * h / w));
        k = 1920 / w;
        pt = k / 72.0;
        sized = true;
    }
    QRectF frame(const RVNGPropertyList &props) const {
        return QRectF(inches(props, "svg:x") * k, inches(props, "svg:y") * k,
                      inches(props, "svg:width") * k, inches(props, "svg:height") * k);
    }

    // ---- where things go
    Master *master = nullptr;         // while inside a master slide
    Slide *slide = nullptr;           // while inside a slide
    int slideNumber = 0;
    QHash<QString, QString> masterIdByName, layoutIdByMaster;
    QVector<SceneObject> *objects() { return master ? &master->objects : slide ? &slide->objects : nullptr; }
    QStringList groups;

    // ---- the current graphic style, set before each drawing call
    RVNGPropertyList style;

    void applyStyle(SceneObject &o, const RVNGPropertyList &props) const {
        auto pick = [&](const char *name) -> const RVNGProperty * {
            if (const RVNGProperty *p = props[name]) return p;
            return style[name];
        };
        const RVNGProperty *fill = pick("draw:fill");
        const auto fillKind = fill ? QString::fromUtf8(fill->getStr().cstr()) : QStringLiteral("none");
        if (fillKind == QLatin1String("solid")) {
            o.fillStyle = 0;
            if (const RVNGProperty *c = pick("draw:fill-color")) o.fill = QColor(QString::fromUtf8(c->getStr().cstr()));
        } else if (fillKind == QLatin1String("gradient")) {
            o.fillStyle = 1;
            if (const RVNGProperty *c = pick("draw:start-color")) o.fill = QColor(QString::fromUtf8(c->getStr().cstr()));
            if (const RVNGProperty *c = pick("draw:end-color")) o.fillSecondary = QColor(QString::fromUtf8(c->getStr().cstr()));
            if (const RVNGProperty *a = pick("draw:angle")) o.fillAngle = a->getDouble();
        } else if (fillKind == QLatin1String("bitmap")) {
            o.fillStyle = 5;
        } else o.fillStyle = 5;
        if (const RVNGProperty *opacity = pick("draw:opacity")) {
            const double v = opacity->getDouble();
            o.opacity = qBound(0.0, v > 1 ? v / 100.0 : v, 1.0);
        }
        const RVNGProperty *stroke = pick("draw:stroke");
        const auto strokeKind = stroke ? QString::fromUtf8(stroke->getStr().cstr()) : QStringLiteral("none");
        if (strokeKind != QLatin1String("none")) {
            const RVNGProperty *width = pick("svg:stroke-width");
            double w = 0.0139; // one point
            if (width) w = width->getUnit() == librevenge::RVNG_POINT ? width->getDouble() / 72.0 : width->getDouble();
            o.strokeWidth = qMax(0.5, w * k);
            if (const RVNGProperty *c = pick("svg:stroke-color")) o.strokeColor = QColor(QString::fromUtf8(c->getStr().cstr()));
            o.strokeStyle = strokeKind == QLatin1String("dash") ? 1 : 0;
        } else o.strokeWidth = 0;
        if (const RVNGProperty *shadow = pick("draw:shadow")) {
            if (QString::fromUtf8(shadow->getStr().cstr()) == QLatin1String("visible")) {
                o.shadowEnabled = true;
                if (const RVNGProperty *c = pick("draw:shadow-color")) o.shadowColor = QColor(QString::fromUtf8(c->getStr().cstr()));
                if (const RVNGProperty *a = pick("draw:shadow-opacity")) o.shadowColor.setAlphaF(qBound(0.0, a->getDouble() > 1 ? a->getDouble() / 100.0 : a->getDouble(), 1.0));
                const RVNGProperty *x = pick("draw:shadow-offset-x"), *y = pick("draw:shadow-offset-y");
                if (x) o.shadowX = (x->getUnit() == librevenge::RVNG_POINT ? x->getDouble() / 72.0 : x->getDouble()) * k;
                if (y) o.shadowY = (y->getUnit() == librevenge::RVNG_POINT ? y->getDouble() / 72.0 : y->getDouble()) * k;
            }
        }
    }

    void place(SceneObject &o) {
        o.groups = groups;
        if (auto *list = objects()) list->append(o);
    }

    // ---- text
    TextBuffer *text = nullptr;          // the buffer words go to right now
    TextBuffer textObject;               // for a text box
    TextBuffer notes;
    TextBuffer cell;
    TextBuffer comment;
    SceneObject textFrame;               // geometry and style of the current text box
    bool inTextObject = false, inNotes = false, inComment = false, inTable = false, inCell = false;
    QVector<SpanLook> spans;
    QVector<int> listKinds;              // 1 bullets, 2 numbers, by depth

    SpanLook spanLook(const RVNGPropertyList &props) const {
        SpanLook look = spans.isEmpty() ? SpanLook() : spans.last();
        if (has(props, "fo:font-size")) look.sizePt = inches(props, "fo:font-size") * 72.0;
        if (has(props, "fo:font-weight")) look.bold = str(props, "fo:font-weight") == QLatin1String("bold");
        if (has(props, "fo:font-style")) look.italic = str(props, "fo:font-style") == QLatin1String("italic");
        if (has(props, "style:text-underline-type")) look.underline = str(props, "style:text-underline-type") != QLatin1String("none");
        if (has(props, "style:text-line-through-type")) look.strike = str(props, "style:text-line-through-type") != QLatin1String("none");
        if (has(props, "fo:color")) look.color = color(props, "fo:color");
        if (has(props, "style:font-name")) look.family = str(props, "style:font-name");
        if (has(props, "fo:text-transform")) {
            const auto transform = str(props, "fo:text-transform");
            look.caps = transform == QLatin1String("uppercase");
            look.capitalize = transform == QLatin1String("capitalize");
        }
        if (has(props, "style:text-position")) {
            const auto position = str(props, "style:text-position");
            look.baseline = position.startsWith(QLatin1String("super")) || position.startsWith(QLatin1Char('+')) ? 1
                          : position.startsWith(QLatin1String("sub")) || position.startsWith(QLatin1Char('-')) ? 2 : 0;
        }
        if (const RVNGProperty *spacing = props["fo:letter-spacing"]) {
            if (spacing->getUnit() == librevenge::RVNG_POINT) look.letterSpacingPt = spacing->getDouble();
            else if (spacing->getUnit() == librevenge::RVNG_INCH) look.letterSpacingPt = spacing->getDouble() * 72.0;
        }
        if (has(props, "fo:language")) {
            look.language = str(props, "fo:language");
            if (has(props, "fo:country")) look.language += QLatin1Char('_') + str(props, "fo:country");
        }
        return look;
    }

    ParaLook paraLook(const RVNGPropertyList &props) const {
        ParaLook look;
        const auto align = str(props, "fo:text-align");
        if (align == QLatin1String("left") || align == QLatin1String("start")) look.align = 0;
        else if (align == QLatin1String("center")) look.align = 1;
        else if (align == QLatin1String("right") || align == QLatin1String("end")) look.align = 2;
        else if (align == QLatin1String("justify")) look.align = 3;
        if (const RVNGProperty *lh = props["fo:line-height"]) {
            if (lh->getUnit() == librevenge::RVNG_PERCENT) look.lineHeight = lh->getDouble() * (lh->getDouble() <= 5 ? 100 : 1);
        }
        if (has(props, "fo:margin-top")) look.spaceBeforePt = inches(props, "fo:margin-top") * 72.0;
        return look;
    }

    // Turns a buffer into a text object's properties.
    void applyText(SceneObject &o, const TextBuffer &buffer) const {
        o.type = ObjectType::Text;
        o.text = buffer.text;
        const auto &b = buffer.box;
        o.fontSize = (b.sizePt > 0 ? b.sizePt : 18) * pt;
        o.fontWeight = b.bold > 0 ? 700 : 400;
        o.italic = b.italic > 0;
        o.underline = b.underline > 0;
        o.textColor = b.color.isValid() ? b.color : QColor(Qt::black);
        o.fontFamily = b.family.isEmpty() ? QStringLiteral("Inter") : b.family;
        o.uppercase = b.caps;
        o.letterSpacing = b.letterSpacingPt * pt;
        o.language = b.language;
        o.textAlign = buffer.boxPara.align < 0 ? 0 : buffer.boxPara.align;
        o.lineHeight = buffer.boxPara.lineHeight > 0 ? buffer.boxPara.lineHeight : 100;
        o.paragraphSpacing = buffer.boxPara.spaceBeforePt > 0 ? buffer.boxPara.spaceBeforePt * pt : 0;
        o.listStyle = buffer.bulleted + buffer.numbered > buffer.plain && buffer.bulleted + buffer.numbered > 0
                      ? (buffer.numbered > buffer.bulleted ? 2 : 1) : 0;
        o.fillStyle = 5;
        o.strokeWidth = 0;
        o.fillToken.clear(); o.textColorToken.clear(); o.fontToken.clear();
        QVector<TextRun> runs;
        for (const auto &piece : buffer.pieces) {
            const auto &l = piece.look;
            TextRun run;
            run.start = piece.start; run.length = piece.length;
            bool differs = false;
            if (l.bold != b.bold && l.bold >= 0) { run.weight = l.bold ? 700 : 400; differs = true; }
            if (l.italic != b.italic && l.italic >= 0) { run.italic = l.italic ? 1 : 2; differs = true; }
            if (l.underline != b.underline && l.underline >= 0) { run.underline = l.underline ? 1 : 2; differs = true; }
            if (l.strike != b.strike && l.strike >= 0) { run.strike = l.strike ? 1 : 2; differs = true; }
            if (l.baseline != b.baseline) { run.baseline = l.baseline; differs = true; }
            if (l.sizePt > 0 && !qFuzzyCompare(l.sizePt + 1, b.sizePt + 1)) { run.fontSize = l.sizePt * pt; differs = true; }
            if (!l.family.isEmpty() && l.family != b.family) { run.fontFamily = l.family; differs = true; }
            if (l.color.isValid() && l.color != b.color) { run.color = l.color; differs = true; }
            if (!l.language.isEmpty() && l.language != b.language) { run.language = l.language; differs = true; }
            if (differs) runs.append(run);
        }
        o.runs = TextRuns::tidy(runs, o.text.size());
        if (!buffer.link.isEmpty()) {
            if (buffer.link.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive)) { o.linkKind = 2; o.linkTarget = buffer.link.mid(7); }
            else if (buffer.link.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) { o.linkKind = 1; o.linkTarget = buffer.link; }
        }
    }

    // ---- tables
    SceneObject table;
    QVector<qreal> tableColumns;
    QVector<QVector<TableCell>> tableRows;
    QVector<qreal> tableRowHeights;
    int cellRow = -1, cellColumn = -1;
    TableCell currentCell;

    // ---- RVNGPresentationInterface

    void startDocument(const RVNGPropertyList &) override {}
    void endDocument() override {}
    void setDocumentMetaData(const RVNGPropertyList &) override {}
    void defineEmbeddedFont(const RVNGPropertyList &props) override {
        warnings.add(QStringLiteral("A typeface embedded in the deck is not carried across: %1").arg(str(props, "librevenge:name")), 0);
    }

    void startMasterSlide(const RVNGPropertyList &props) override {
        sizeFrom(props);
        Master m;
        m.id = Edit::newId(QStringLiteral("master"));
        m.name = str(props, "librevenge:master-page-name");
        if (m.name.isEmpty()) m.name = QStringLiteral("Master %1").arg(doc.masters.size() + 1);
        m.backgroundToken.clear();
        if (str(props, "draw:fill") == QLatin1String("solid")) {
            const auto c = color(props, "draw:fill-color");
            if (c.isValid()) m.background = c;
        } else m.background = QColor(Qt::white);
        doc.masters.append(m);
        master = &doc.masters.last();
        masterIdByName.insert(m.name, m.id);
        SlideLayout layout;
        layout.id = Edit::newId(QStringLiteral("layout"));
        layout.name = m.name;
        layout.masterId = m.id;
        doc.layouts.append(layout);
        layoutIdByMaster.insert(m.id, layout.id);
    }
    void endMasterSlide() override { master = nullptr; }

    void startSlide(const RVNGPropertyList &props) override {
        sizeFrom(props);
        ++slideNumber;
        Slide s;
        s.id = Edit::newId(QStringLiteral("slide"));
        const auto masterName = str(props, "librevenge:master-page-name");
        const auto masterId = masterIdByName.value(masterName);
        s.layoutId = layoutIdByMaster.value(masterId);
        s.transition = 0;
        if (str(props, "draw:fill") == QLatin1String("solid")) {
            const auto c = color(props, "draw:fill-color");
            if (c.isValid()) { s.background = c; s.backgroundOverride = true; }
        } else if (str(props, "draw:fill") == QLatin1String("bitmap")) {
            const auto image = str(props, "draw:fill-image");
            if (!image.isEmpty()) {
                SceneObject o;
                o.id = Edit::newId(QStringLiteral("image"));
                o.type = ObjectType::Image;
                QString trouble;
                if (ImageAsset::decode(o, QByteArray::fromBase64(image.toLatin1()), &trouble)) {
                    o.rect = QRectF(QPointF(0, 0), doc.size); o.imageMode = 1; o.fillStyle = 5; o.locked = true;
                    s.objects.append(o);
                }
            }
            s.background = QColor(Qt::white); s.backgroundOverride = true;
        } else if (has(props, "draw:fill")) { s.background = QColor(Qt::white); s.backgroundOverride = true; }
        doc.slides.append(s);
        slide = &doc.slides.last();
    }
    void endSlide() override { slide = nullptr; }

    void setStyle(const RVNGPropertyList &props) override { style = props; }
    void setSlideTransition(const RVNGPropertyList &) override {
        warnings.add(QStringLiteral("Keynote transitions are not carried across"), slideNumber);
    }
    void startLayer(const RVNGPropertyList &) override {}
    void endLayer() override {}
    void startEmbeddedGraphics(const RVNGPropertyList &) override {
        warnings.add(QStringLiteral("An embedded drawing was left out"), slideNumber);
    }
    void endEmbeddedGraphics() override {}
    void openGroup(const RVNGPropertyList &) override { groups.append(Edit::newId(QStringLiteral("group"))); }
    void closeGroup() override { if (!groups.isEmpty()) groups.removeLast(); }

    void drawShape(const QPainterPath &path, const RVNGPropertyList &props) {
        if (path.isEmpty() || !objects()) return;
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("shape"));
        o.type = ObjectType::Rect;
        applyStyle(o, props);
        if (o.fillStyle == 5 && o.strokeWidth <= 0) return;
        Shape::assignPath(o, path);
        place(o);
    }
    void drawRectangle(const RVNGPropertyList &props) override {
        if (!objects()) return;
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("shape"));
        o.type = ObjectType::Rect;
        o.rect = frame(props);
        applyStyle(o, props);
        if (o.fillStyle == 5 && o.strokeWidth <= 0) return;
        const double rx = inches(props, "svg:rx");
        if (rx > 0) o.cornerRadius = rx * k;
        place(o);
    }
    void drawEllipse(const RVNGPropertyList &props) override {
        if (!objects()) return;
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("shape"));
        o.type = ObjectType::Rect;
        o.shapeKind = 1;
        const double cx = inches(props, "svg:cx") * k, cy = inches(props, "svg:cy") * k;
        const double rx = inches(props, "svg:rx") * k, ry = inches(props, "svg:ry") * k;
        o.rect = QRectF(cx - rx, cy - ry, 2 * rx, 2 * ry);
        applyStyle(o, props);
        if (o.fillStyle == 5 && o.strokeWidth <= 0) return;
        place(o);
    }
    QPainterPath points(const RVNGPropertyList &props, bool close) const {
        QPainterPath path;
        const RVNGPropertyListVector *vec = props.child("svg:points");
        if (!vec) return path;
        RVNGPropertyListVector::Iter it(*vec);
        bool first = true;
        while (it.next()) {
            const QPointF p(inches(it(), "svg:x") * k, inches(it(), "svg:y") * k);
            if (first) { path.moveTo(p); first = false; } else path.lineTo(p);
        }
        if (close && !first) path.closeSubpath();
        return path;
    }
    void drawPolygon(const RVNGPropertyList &props) override { drawShape(points(props, true), props); }
    void drawPolyline(const RVNGPropertyList &props) override { drawShape(points(props, false), props); }
    void drawConnector(const RVNGPropertyList &props) override {
        if (props.child("svg:d")) drawPath(props); else drawPolyline(props);
    }
    void drawPath(const RVNGPropertyList &props) override {
        const RVNGPropertyListVector *vec = props.child("svg:d");
        if (!vec) return;
        QPainterPath path;
        RVNGPropertyListVector::Iter it(*vec);
        while (it.next()) {
            const auto &cmd = it();
            const auto action = str(cmd, "librevenge:path-action");
            const QPointF p(inches(cmd, "svg:x") * k, inches(cmd, "svg:y") * k);
            if (action == QLatin1String("M")) path.moveTo(p);
            else if (action == QLatin1String("L")) path.lineTo(p);
            else if (action == QLatin1String("C")) path.cubicTo(QPointF(inches(cmd, "svg:x1") * k, inches(cmd, "svg:y1") * k),
                                                                QPointF(inches(cmd, "svg:x2") * k, inches(cmd, "svg:y2") * k), p);
            else if (action == QLatin1String("Q")) path.quadTo(QPointF(inches(cmd, "svg:x1") * k, inches(cmd, "svg:y1") * k), p);
            else if (action == QLatin1String("Z")) path.closeSubpath();
        }
        drawShape(path, props);
    }

    void drawGraphicObject(const RVNGPropertyList &props) override {
        if (!objects() && !inCell) return;
        const auto mime = str(props, "librevenge:mime-type");
        const RVNGProperty *data = props["office:binary-data"];
        if (!data) return;
        if (inCell) { warnings.add(QStringLiteral("A picture inside a table cell was left out"), slideNumber); return; }
        const librevenge::RVNGBinaryData binary(data->getStr());
        const QByteArray bytes(reinterpret_cast<const char *>(binary.getDataBuffer()), int(binary.size()));
        if (mime.startsWith(QLatin1String("video")) || mime.startsWith(QLatin1String("audio"))) {
            warnings.add(QStringLiteral("A film or sound was left out"), slideNumber);
            return;
        }
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("image"));
        o.type = ObjectType::Image;
        QString trouble;
        bool ok = false;
        if (mime.contains(QLatin1String("svg")) || bytes.startsWith("<?xml") || bytes.startsWith("<svg")) ok = SvgAsset::decode(o, bytes, &trouble);
        else if (mime.contains(QLatin1String("pdf"))) { warnings.add(QStringLiteral("A PDF picture was left out"), slideNumber); return; }
        else ok = ImageAsset::decode(o, bytes, &trouble);
        if (!ok) {
            warnings.add(QStringLiteral("A picture could not be read (%1)").arg(mime.isEmpty() ? QStringLiteral("unknown format") : mime), slideNumber);
            return;
        }
        o.rect = frame(props);
        if (o.rect.width() <= 0 || o.rect.height() <= 0) {
            const auto size = ImageAsset::size(o);
            o.rect.setSize(QSizeF(size.width() * k / 96.0, size.height() * k / 96.0));
        }
        o.imageMode = 2;
        o.fillStyle = 5;
        if (number(props, "librevenge:rotate") == 180) o.rotation = 180;
        if (const RVNGProperty *opacity = props["draw:image-opacity"]) o.opacity = qBound(0.0, opacity->getDouble() > 1 ? opacity->getDouble() / 100.0 : opacity->getDouble(), 1.0);
        place(o);
    }

    void startTextObject(const RVNGPropertyList &props) override {
        inTextObject = true;
        textObject = TextBuffer();
        text = &textObject;
        textFrame = SceneObject();
        textFrame.rect = frame(props);
        applyStyle(textFrame, props);
        const auto anchor = str(props, "draw:textarea-vertical-align");
        textFrame.verticalAlign = anchor == QLatin1String("middle") ? 1 : anchor == QLatin1String("bottom") ? 2 : 0;
        if (textFrame.rect.width() <= 0) textFrame.rect.setWidth(doc.size.width() * .5);
        if (textFrame.rect.height() <= 0) textFrame.rect.setHeight(doc.size.height() * .1);
    }
    void endTextObject() override {
        inTextObject = false;
        text = nullptr;
        if (!objects()) return;
        const bool visibleBox = textFrame.fillStyle != 5 || textFrame.strokeWidth > 0;
        QStringList own = groups;
        if (visibleBox && !textObject.empty()) own.append(Edit::newId(QStringLiteral("group")));
        if (visibleBox) {
            SceneObject box = textFrame;
            box.id = Edit::newId(QStringLiteral("shape"));
            box.type = ObjectType::Rect;
            box.groups = own;
            objects()->append(box);
        }
        if (!textObject.empty()) {
            SceneObject words;
            words.id = Edit::newId(QStringLiteral("text"));
            words.rect = textFrame.rect;
            applyText(words, textObject);
            words.verticalAlign = textFrame.verticalAlign;
            words.groups = own;
            objects()->append(words);
        }
    }

    void insertTab() override { if (text) text->insert(QStringLiteral("\t"), spans.isEmpty() ? SpanLook() : spans.last()); }
    void insertSpace() override { if (text) text->insert(QStringLiteral(" "), spans.isEmpty() ? SpanLook() : spans.last()); }
    void insertText(const RVNGString &words) override {
        if (text) text->insert(QString::fromUtf8(words.cstr()), spans.isEmpty() ? SpanLook() : spans.last());
    }
    void insertLineBreak() override { if (text) text->insert(QStringLiteral("\n"), spans.isEmpty() ? SpanLook() : spans.last()); }
    void insertField(const RVNGPropertyList &props) override {
        if (!text) return;
        const auto type = str(props, "librevenge:field-type");
        QString words;
        if (type.contains(QLatin1String("page-number")) || type.contains(QLatin1String("slide"))) words = QString::number(slideNumber);
        else words = str(props, "librevenge:text");
        if (!words.isEmpty()) text->insert(words, spans.isEmpty() ? SpanLook() : spans.last());
    }

    void openOrderedListLevel(const RVNGPropertyList &) override { listKinds.append(2); }
    void openUnorderedListLevel(const RVNGPropertyList &props) override {
        listKinds.append(str(props, "text:bullet-char").trimmed().isEmpty() ? 0 : 1);
    }
    void closeOrderedListLevel() override { if (!listKinds.isEmpty()) listKinds.removeLast(); }
    void closeUnorderedListLevel() override { if (!listKinds.isEmpty()) listKinds.removeLast(); }
    void openListElement(const RVNGPropertyList &props) override {
        if (!text) return;
        ParaLook look = paraLook(props);
        const int kind = listKinds.isEmpty() ? 1 : listKinds.last();
        look.bullet = kind == 1; look.numbered = kind == 2;
        look.level = listKinds.size();
        text->beginParagraph(look);
        spans.append(spanLook(props));
    }
    void closeListElement() override { if (text) text->endParagraph(); if (!spans.isEmpty()) spans.removeLast(); }
    void defineParagraphStyle(const RVNGPropertyList &) override {}
    void openParagraph(const RVNGPropertyList &props) override {
        if (!text) return;
        text->beginParagraph(paraLook(props));
        spans.append(spanLook(props));
    }
    void closeParagraph() override { if (text) text->endParagraph(); if (!spans.isEmpty()) spans.removeLast(); }
    void defineCharacterStyle(const RVNGPropertyList &) override {}
    void openSpan(const RVNGPropertyList &props) override { spans.append(spanLook(props)); }
    void closeSpan() override { if (!spans.isEmpty()) spans.removeLast(); }
    void openLink(const RVNGPropertyList &props) override {
        if (text && text->link.isEmpty()) text->link = str(props, "xlink:href");
    }
    void closeLink() override {}

    void startTableObject(const RVNGPropertyList &props) override {
        if (!objects()) return;
        inTable = true;
        table = SceneObject();
        table.id = Edit::newId(QStringLiteral("table"));
        table.type = ObjectType::Table;
        table.rect = frame(props);
        tableColumns.clear(); tableRows.clear(); tableRowHeights.clear();
        const RVNGPropertyListVector *columns = props.child("librevenge:table-columns");
        if (!columns) columns = props.child("librevenge:columns");
        if (columns) {
            RVNGPropertyListVector::Iter it(*columns);
            while (it.next()) tableColumns.append(qMax(0.01, inches(it(), "style:column-width", 1)));
        }
    }
    void openTableRow(const RVNGPropertyList &props) override {
        if (!inTable) return;
        tableRows.append(QVector<TableCell>());
        tableRowHeights.append(qMax(0.01, inches(props, "style:row-height", 0.3)));
    }
    void closeTableRow() override {}
    void openTableCell(const RVNGPropertyList &props) override {
        if (!inTable || tableRows.isEmpty()) return;
        inCell = true;
        cell = TextBuffer();
        text = &cell;
        currentCell = TableCell();
        currentCell.columnSpan = qMax(1, int(number(props, "table:number-columns-spanned", 1)));
        currentCell.rowSpan = qMax(1, int(number(props, "table:number-rows-spanned", 1)));
        const auto fill = color(props, "fo:background-color");
        if (fill.isValid()) currentCell.style.insert(QStringLiteral("fill"), fill.name(QColor::HexArgb));
        const auto valign = str(props, "style:vertical-align");
        if (valign == QLatin1String("middle")) currentCell.style.insert(QStringLiteral("verticalAlign"), 1);
        else if (valign == QLatin1String("bottom")) currentCell.style.insert(QStringLiteral("verticalAlign"), 2);
    }
    void closeTableCell() override {
        if (!inCell) return;
        inCell = false;
        text = inTextObject ? &textObject : inNotes ? &notes : nullptr;
        currentCell.text = cell.text;
        const auto &b = cell.box;
        if (b.sizePt > 0) currentCell.style.insert(QStringLiteral("fontSize"), b.sizePt * pt);
        if (b.bold > 0) currentCell.style.insert(QStringLiteral("fontWeight"), 700);
        if (b.italic > 0) currentCell.style.insert(QStringLiteral("italic"), true);
        if (b.color.isValid()) currentCell.style.insert(QStringLiteral("textColor"), b.color.name(QColor::HexArgb));
        if (!b.family.isEmpty()) currentCell.style.insert(QStringLiteral("fontFamily"), b.family);
        if (cell.boxPara.align >= 0) currentCell.style.insert(QStringLiteral("textAlign"), cell.boxPara.align);
        tableRows.last().append(currentCell);
    }
    void insertCoveredTableCell(const RVNGPropertyList &) override {
        if (!inTable || tableRows.isEmpty()) return;
        TableCell covered;
        covered.rowSpan = 0; covered.columnSpan = 0;
        tableRows.last().append(covered);
    }
    void endTableObject() override {
        if (!inTable) return;
        inTable = false;
        if (!objects()) return;
        int columns = tableColumns.size();
        for (const auto &row : tableRows) columns = qMax(columns, row.size());
        const int rows = tableRows.size();
        if (rows < 1 || columns < 1 || rows > Table::maxRows || columns > Table::maxColumns) {
            warnings.add(QStringLiteral("A table was too large to bring across"), slideNumber);
            return;
        }
        table.table = Table::create(rows, columns);
        auto relative = [](QVector<qreal> &sizes) {
            qreal total = 0;
            for (qreal s : sizes) total += s;
            const qreal mean = sizes.isEmpty() || total <= 0 ? 1 : total / sizes.size();
            for (auto &s : sizes) s = qBound(0.05, s / mean, 20.0);
        };
        for (int c = 0; c < columns; ++c) table.table.columns[c] = tableColumns.value(c, 1.0);
        for (int r = 0; r < rows; ++r) table.table.rows[r] = tableRowHeights.value(r, 0.3);
        relative(table.table.columns);
        relative(table.table.rows);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < columns; ++c) {
                auto &target = table.table.cells[r * columns + c];
                if (c >= tableRows[r].size()) continue;
                const auto &source = tableRows[r][c];
                target.text = source.text;
                target.style = source.style;
                target.rowSpan = qMin(source.rowSpan, rows - r);
                target.columnSpan = qMin(source.columnSpan, columns - c);
            }
        table.table.headerRows = 1;
        table.table.headerColumns = 0;
        table.fontSize = 14 * pt;
        table.verticalAlign = 0;
        table.table.padding = 3 * pt;
        table.table.borderWidth = 0.75 * pt;
        table.fillToken = QStringLiteral("background"); table.textColorToken = QStringLiteral("foreground"); table.fontToken = QStringLiteral("body");
        if (table.rect.width() <= 0) table.rect.setWidth(doc.size.width() * .6);
        if (table.rect.height() <= 0) table.rect.setHeight(doc.size.height() * .4);
        place(table);
    }

    void startComment(const RVNGPropertyList &) override { inComment = true; comment = TextBuffer(); text = &comment; }
    void endComment() override {
        inComment = false;
        text = inTextObject ? &textObject : inNotes ? &notes : nullptr;
        if (!slide || comment.empty()) return;
        Comment c;
        c.id = Edit::newId(QStringLiteral("comment"));
        c.slideId = slide->id;
        c.author = QStringLiteral("Keynote");
        c.text = comment.text;
        doc.comments.append(c);
    }
    void startNotes(const RVNGPropertyList &) override { inNotes = true; notes = TextBuffer(); text = &notes; }
    void endNotes() override {
        inNotes = false;
        text = nullptr;
        if (slide) slide->notes = notes.text.trimmed();
    }

    void defineChartStyle(const RVNGPropertyList &) override {}
    void openChart(const RVNGPropertyList &) override { warnings.add(QStringLiteral("A chart was left out"), slideNumber); }
    void closeChart() override {}
    void openChartTextObject(const RVNGPropertyList &) override {}
    void closeChartTextObject() override {}
    void openChartPlotArea(const RVNGPropertyList &) override {}
    void closeChartPlotArea() override {}
    void insertChartAxis(const RVNGPropertyList &) override {}
    void openChartSeries(const RVNGPropertyList &) override {}
    void closeChartSeries() override {}
    void openAnimationSequence(const RVNGPropertyList &) override {}
    void closeAnimationSequence() override {}
    void openAnimationGroup(const RVNGPropertyList &) override {}
    void closeAnimationGroup() override {}
    void openAnimationIteration(const RVNGPropertyList &) override {}
    void closeAnimationIteration() override {}
    void insertMotionAnimation(const RVNGPropertyList &) override {}
    void insertColorAnimation(const RVNGPropertyList &) override {}
    void insertAnimation(const RVNGPropertyList &) override {}
    void insertEffect(const RVNGPropertyList &) override {}
};

Keynote::Result convert(librevenge::RVNGInputStream &stream, const QString &name) {
    Keynote::Result result;
    libetonyek::EtonyekDocument::Type type = libetonyek::EtonyekDocument::TYPE_UNKNOWN;
    const auto confidence = libetonyek::EtonyekDocument::isSupported(&stream, &type);
    if (confidence == libetonyek::EtonyekDocument::CONFIDENCE_NONE || type != libetonyek::EtonyekDocument::TYPE_KEYNOTE) {
        result.error = type == libetonyek::EtonyekDocument::TYPE_UNKNOWN
            ? QStringLiteral("%1 is not a Keynote deck.").arg(name)
            : QStringLiteral("%1 is a %2 document, not a Keynote deck.").arg(name, type == libetonyek::EtonyekDocument::TYPE_NUMBERS ? QStringLiteral("Numbers") : QStringLiteral("Pages"));
        return result;
    }
    Generator generator;
    if (!libetonyek::EtonyekDocument::parse(&stream, &generator)) {
        result.error = QStringLiteral("%1 could not be read as a Keynote deck.").arg(name);
        return result;
    }
    if (generator.doc.slides.isEmpty()) {
        result.error = QStringLiteral("%1 has no slides.").arg(name);
        return result;
    }
    result.document = generator.doc;
    result.document.transition = 0;
    result.document.transitionDuration = 0.5;
    if (!result.document.masters.isEmpty()) {
        result.document.theme.name = QStringLiteral("Keynote");
        result.document.theme.colors[QStringLiteral("background")] = result.document.masters.first().background;
        // Foreground is whatever most words are written in.
        QMap<QString, int> colours;
        for (const auto &s : result.document.slides)
            for (const auto &o : s.objects) if (o.type == ObjectType::Text) colours[o.textColor.name()] += o.text.size();
        if (!colours.isEmpty()) {
            QString best; int most = -1;
            for (auto it = colours.cbegin(); it != colours.cend(); ++it) if (it.value() > most) { most = it.value(); best = it.key(); }
            result.document.theme.colors[QStringLiteral("foreground")] = QColor(best);
        }
    }
    result.warnings = generator.warnings.lines();
    result.ok = true;
    return result;
}

} // namespace

namespace Keynote {

Result read(const QByteArray &raw) {
    librevenge::RVNGStringStream stream(reinterpret_cast<const unsigned char *>(raw.constData()), static_cast<unsigned>(raw.size()));
    return convert(stream, QStringLiteral("The file"));
}

Result load(const QString &path) {
    const QFileInfo info(path);
    const auto name = info.fileName();
    if (info.isDir()) {
        librevenge::RVNGDirectoryStream stream(QFile::encodeName(path).constData());
        return convert(stream, name);
    }
    if (!info.exists()) {
        Result result;
        result.error = QStringLiteral("%1 could not be read.").arg(name);
        return result;
    }
    librevenge::RVNGFileStream stream(QFile::encodeName(path).constData());
    return convert(stream, name);
}

} // namespace Keynote
