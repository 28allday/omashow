#include "io/pptx.h"

#include "anim/build.h"
#include "anim/presentation.h"
#include "core/edit.h"
#include "core/imageasset.h"
#include "core/link.h"
#include "core/mediaasset.h"
#include "core/shape.h"
#include "core/svgasset.h"
#include "core/table.h"
#include "core/textruns.h"
#include "io/zip.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QPainterPath>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTransform>
#include <QXmlStreamReader>
#include <QtMath>

#include <memory>

namespace {

// ---------------------------------------------------------------------------
// A small tree of the XML. The format is read by reference — a slide's text
// looks up its layout, which looks up its master, which looks up the theme —
// so a streaming read would have to be repeated; the parts are small and a
// tree is simpler to ask questions of.

struct Node {
    QString name;   // local name; the prefix is conventional and the URI fixed
    QHash<QString, QString> attrs;
    QVector<Node> kids;
    QString text;

    const Node *child(const QString &n) const {
        for (const auto &k : kids) if (k.name == n) return &k;
        return nullptr;
    }
    QVector<const Node *> all(const QString &n) const {
        QVector<const Node *> out;
        for (const auto &k : kids) if (k.name == n) out.append(&k);
        return out;
    }
    QString attr(const QString &n, const QString &fallback = QString()) const {
        return attrs.value(n, fallback);
    }
    bool has(const QString &n) const { return attrs.contains(n); }
    // Descends by local names; null when any step is missing.
    const Node *path(std::initializer_list<const char *> names) const {
        const Node *node = this;
        for (const auto *n : names) {
            node = node->child(QString::fromLatin1(n));
            if (!node) return nullptr;
        }
        return node;
    }
    // Every descendant with this name, in document order.
    void collect(const QString &n, QVector<const Node *> &out) const {
        for (const auto &k : kids) {
            if (k.name == n) out.append(&k);
            k.collect(n, out);
        }
    }
    // The words in every a:t below this node, paragraphs apart.
    QString words() const {
        QString out;
        QVector<const Node *> paragraphs;
        collect(QStringLiteral("p"), paragraphs);
        if (paragraphs.isEmpty()) {
            QVector<const Node *> ts;
            collect(QStringLiteral("t"), ts);
            for (const auto *t : ts) out += t->text;
            return out;
        }
        for (const auto *p : paragraphs) {
            QVector<const Node *> ts;
            p->collect(QStringLiteral("t"), ts);
            QString line;
            for (const auto *t : ts) line += t->text;
            if (!out.isEmpty()) out += QLatin1Char('\n');
            out += line;
        }
        return out;
    }
};

bool parseXml(const QByteArray &bytes, Node *root, QString *error) {
    QXmlStreamReader xml(bytes);
    QVector<Node *> stack;
    *root = Node();
    bool started = false;
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement: {
            // A tree of any depth is a stack overflow waiting to happen; no
            // real deck nests anywhere near this.
            if (stack.size() > 256) { if (error) *error = QStringLiteral("nested too deeply"); return false; }
            Node *node;
            if (!started) { node = root; started = true; }
            else { stack.last()->kids.append(Node()); node = &stack.last()->kids.last(); }
            node->name = xml.name().toString();
            for (const auto &a : xml.attributes()) {
                // Keyed by local name, and by prefix:name where there is a
                // prefix; an unprefixed attribute keeps the local key when a
                // prefixed one shares it (p:sldId has both id and r:id).
                const auto local = a.name().toString();
                if (a.prefix().isEmpty() || !node->attrs.contains(local)) node->attrs.insert(local, a.value().toString());
                if (!a.prefix().isEmpty()) node->attrs.insert(a.qualifiedName().toString(), a.value().toString());
            }
            stack.append(node);
            break;
        }
        case QXmlStreamReader::EndElement:
            if (!stack.isEmpty()) stack.removeLast();
            break;
        case QXmlStreamReader::Characters:
            if (!stack.isEmpty() && (!xml.isWhitespace() || stack.last()->name == QLatin1String("t")))
                stack.last()->text += xml.text().toString();
            break;
        default: break;
        }
    }
    if (xml.hasError()) {
        if (error) *error = xml.errorString();
        return false;
    }
    return started;
}

// Markup written for a newer PowerPoint carries an older reading beside it.
// Shapes take the fallback, which every reader understands; the transition
// reader looks at both, because the newer one may be the interesting one.
void flattened(const Node &parent, QVector<const Node *> &out) {
    for (const auto &k : parent.kids) {
        if (k.name == QLatin1String("AlternateContent")) {
            const Node *branch = k.child(QStringLiteral("Fallback"));
            if (!branch) branch = k.child(QStringLiteral("Choice"));
            if (branch) flattened(*branch, out);
        } else out.append(&k);
    }
}

// ---------------------------------------------------------------------------
// The package: parts by path, relationships by part.

struct Rel { QString type, target; bool external = false; };

QString dirOf(const QString &path) {
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : path.left(slash);
}

QString resolvePath(const QString &from, const QString &target) {
    if (target.startsWith(QLatin1Char('/'))) return QDir::cleanPath(target.mid(1));
    const auto dir = dirOf(from);
    return QDir::cleanPath(dir.isEmpty() ? target : dir + QLatin1Char('/') + target);
}

struct Package {
    Zip::Reader zip;
    mutable QHash<QString, std::shared_ptr<Node>> parts;
    mutable QHash<QString, QHash<QString, Rel>> relations;

    explicit Package(const QByteArray &raw) : zip(raw) {}

    QByteArray bytes(const QString &path) const { return zip.read(path); }
    bool has(const QString &path) const { return zip.contains(path); }

    const Node *part(const QString &path) const {
        if (path.isEmpty()) return nullptr;
        auto it = parts.find(path);
        if (it != parts.end()) return it->get();
        std::shared_ptr<Node> node;
        if (zip.contains(path)) {
            node = std::make_shared<Node>();
            QString error;
            if (!parseXml(zip.read(path), node.get(), &error)) node.reset();
        }
        parts.insert(path, node);
        return node.get();
    }

    const QHash<QString, Rel> &rels(const QString &path) const {
        auto it = relations.find(path);
        if (it != relations.end()) return *it;
        QHash<QString, Rel> out;
        const auto dir = dirOf(path);
        const auto file = path.mid(dir.isEmpty() ? 0 : dir.size() + 1);
        const auto relsPath = (dir.isEmpty() ? QString() : dir + QLatin1Char('/'))
                              + QStringLiteral("_rels/") + file + QStringLiteral(".rels");
        Node root;
        QString error;
        if (zip.contains(relsPath) && parseXml(zip.read(relsPath), &root, &error)) {
            for (const auto &r : root.all(QStringLiteral("Relationship"))) {
                Rel rel;
                rel.type = r->attr(QStringLiteral("Type"));
                rel.external = r->attr(QStringLiteral("TargetMode")) == QLatin1String("External");
                rel.target = rel.external ? r->attr(QStringLiteral("Target"))
                                          : resolvePath(path, r->attr(QStringLiteral("Target")));
                out.insert(r->attr(QStringLiteral("Id")), rel);
            }
        }
        return *relations.insert(path, out);
    }

    // The part a relationship id points at, or empty when it is external or absent.
    QString target(const QString &from, const QString &rid) const {
        const auto rel = rels(from).value(rid);
        return rel.external ? QString() : rel.target;
    }
    QString external(const QString &from, const QString &rid) const {
        const auto rel = rels(from).value(rid);
        return rel.external ? rel.target : QString();
    }
    // The first related part of a type ("slideLayout", "theme", …).
    QString related(const QString &from, const QString &typeSuffix) const {
        const auto all = rels(from);
        for (auto it = all.cbegin(); it != all.cend(); ++it)
            if (!it->external && it->type.endsWith(QLatin1Char('/') + typeSuffix)) return it->target;
        return QString();
    }
    QStringList relatedAll(const QString &from, const QString &typeSuffix) const {
        QStringList out;
        const auto all = rels(from);
        for (auto it = all.cbegin(); it != all.cend(); ++it)
            if (!it->external && it->type.endsWith(QLatin1Char('/') + typeSuffix)) out.append(it->target);
        return out;
    }
};

// ---------------------------------------------------------------------------
// Theme colours and fonts.

struct Theme {
    QString name;
    QHash<QString, QColor> colors;   // dk1, lt1, dk2, lt2, accent1…6, hlink, folHlink
    QString major, minor;            // latin typefaces
};

QColor plainColor(const Node *clr) {
    if (!clr) return QColor();
    if (clr->name == QLatin1String("srgbClr")) return QColor(QLatin1Char('#') + clr->attr(QStringLiteral("val")));
    if (clr->name == QLatin1String("sysClr")) return QColor(QLatin1Char('#') + clr->attr(QStringLiteral("lastClr"), QStringLiteral("000000")));
    if (clr->name == QLatin1String("prstClr")) { QColor c(clr->attr(QStringLiteral("val"))); return c.isValid() ? c : QColor(Qt::black); }
    if (clr->name == QLatin1String("scrgbClr")) {
        return QColor::fromRgbF(clr->attr(QStringLiteral("r")).toDouble() / 100000.0,
                                clr->attr(QStringLiteral("g")).toDouble() / 100000.0,
                                clr->attr(QStringLiteral("b")).toDouble() / 100000.0);
    }
    if (clr->name == QLatin1String("hslClr")) {
        return QColor::fromHslF(clr->attr(QStringLiteral("hue")).toDouble() / 21600000.0,
                                clr->attr(QStringLiteral("sat")).toDouble() / 100000.0,
                                clr->attr(QStringLiteral("lum")).toDouble() / 100000.0);
    }
    return QColor();
}

Theme readTheme(const Package &pkg, const QString &path) {
    Theme theme;
    const Node *root = pkg.part(path);
    if (!root) return theme;
    theme.name = root->attr(QStringLiteral("name"));
    if (const Node *scheme = root->path({"themeElements", "clrScheme"})) {
        for (const auto &entry : scheme->kids) {
            for (const auto &clr : entry.kids) {
                const auto color = plainColor(&clr);
                if (color.isValid()) { theme.colors.insert(entry.name, color); break; }
            }
        }
    }
    if (const Node *fonts = root->path({"themeElements", "fontScheme"})) {
        if (const Node *major = fonts->path({"majorFont", "latin"})) theme.major = major->attr(QStringLiteral("typeface"));
        if (const Node *minor = fonts->path({"minorFont", "latin"})) theme.minor = minor->attr(QStringLiteral("typeface"));
    }
    return theme;
}

struct ColorContext {
    const Theme *theme = nullptr;
    QHash<QString, QString> clrMap;   // bg1 → lt1 …
    QColor phColor;                   // a placeholder colour, for "phClr"
};

QColor readColor(const Node *clr, const ColorContext &ctx) {
    if (!clr) return QColor();
    QColor color;
    if (clr->name == QLatin1String("schemeClr")) {
        auto key = clr->attr(QStringLiteral("val"));
        if (key == QLatin1String("phClr")) color = ctx.phColor;
        else {
            key = ctx.clrMap.value(key, key);
            if (key == QLatin1String("bg1")) key = QStringLiteral("lt1");
            if (key == QLatin1String("tx1")) key = QStringLiteral("dk1");
            if (key == QLatin1String("bg2")) key = QStringLiteral("lt2");
            if (key == QLatin1String("tx2")) key = QStringLiteral("dk2");
            color = ctx.theme ? ctx.theme->colors.value(key) : QColor();
        }
    } else color = plainColor(clr);
    if (!color.isValid()) return color;
    // Modifiers, in the order PowerPoint applies the common ones.
    for (const auto &mod : clr->kids) {
        const qreal v = mod.attr(QStringLiteral("val")).toDouble() / 100000.0;
        if (mod.name == QLatin1String("lumMod") || mod.name == QLatin1String("lumOff")) {
            float h, s, l, a;
            color.getHslF(&h, &s, &l, &a);
            l = mod.name == QLatin1String("lumMod") ? l * float(v) : l + float(v);
            color = QColor::fromHslF(h < 0 ? 0 : h, s, qBound(0.0f, l, 1.0f), a);
        } else if (mod.name == QLatin1String("alpha")) {
            color.setAlphaF(qBound(0.0, v, 1.0));
        } else if (mod.name == QLatin1String("tint")) {
            color = QColor::fromRgbF(qBound(0.0, 1 - (1 - color.redF()) * v, 1.0),
                                     qBound(0.0, 1 - (1 - color.greenF()) * v, 1.0),
                                     qBound(0.0, 1 - (1 - color.blueF()) * v, 1.0), color.alphaF());
        } else if (mod.name == QLatin1String("shade")) {
            color = QColor::fromRgbF(color.redF() * v, color.greenF() * v, color.blueF() * v, color.alphaF());
        }
    }
    return color;
}

// The colour inside a solidFill (or any parent with one colour child).
QColor colorIn(const Node *parent, const ColorContext &ctx) {
    if (!parent) return QColor();
    for (const auto &k : parent->kids) {
        const auto c = readColor(&k, ctx);
        if (c.isValid()) return c;
    }
    return QColor();
}

// ---------------------------------------------------------------------------
// Fill, line and shadow of a shape.

struct Look {
    bool fillSet = false;
    int fillStyle = 5;   // none
    QColor fill, fill2;
    qreal angle = 0;
    QString blipRid;
    bool lineSet = false, line = false;
    QColor lineColor;
    qreal lineWidthEmu = 0;
    int dash = 0;
    bool shadow = false;
    QColor shadowColor;
    qreal shadowDistEmu = 0, shadowDir = 0;
};

// Direct fill children of spPr, bgPr, tcPr, ln, rPr …
bool readFill(const Node *owner, Look &look, const ColorContext &ctx) {
    if (!owner) return false;
    for (const auto &k : owner->kids) {
        if (k.name == QLatin1String("solidFill")) {
            look.fillSet = true; look.fillStyle = 0; look.fill = colorIn(&k, ctx);
            if (!look.fill.isValid()) look.fillStyle = 5;
            return true;
        }
        if (k.name == QLatin1String("noFill")) { look.fillSet = true; look.fillStyle = 5; return true; }
        if (k.name == QLatin1String("gradFill")) {
            look.fillSet = true;
            QVector<QPair<qreal, QColor>> stops;
            if (const Node *list = k.child(QStringLiteral("gsLst")))
                for (const auto *gs : list->all(QStringLiteral("gs")))
                    stops.append({gs->attr(QStringLiteral("pos")).toDouble() / 100000.0, colorIn(gs, ctx)});
            if (stops.isEmpty()) { look.fillStyle = 5; return true; }
            look.fillStyle = k.child(QStringLiteral("path")) ? 2 : 1;
            look.fill = stops.first().second;
            look.fill2 = stops.last().second;
            if (const Node *lin = k.child(QStringLiteral("lin")))
                look.angle = lin->attr(QStringLiteral("ang")).toDouble() / 60000.0;
            return true;
        }
        if (k.name == QLatin1String("pattFill")) {
            look.fillSet = true; look.fillStyle = 3;
            look.fill = colorIn(k.child(QStringLiteral("fgClr")), ctx);
            look.fill2 = colorIn(k.child(QStringLiteral("bgClr")), ctx);
            return true;
        }
        if (k.name == QLatin1String("blipFill")) {
            look.fillSet = true; look.fillStyle = 5;
            if (const Node *blip = k.child(QStringLiteral("blip"))) look.blipRid = blip->attr(QStringLiteral("embed"));
            return true;
        }
        if (k.name == QLatin1String("grpFill")) { look.fillSet = true; look.fillStyle = 5; return true; }
    }
    return false;
}

void readLine(const Node *ln, Look &look, const ColorContext &ctx) {
    if (!ln) return;
    look.lineSet = true;
    look.lineWidthEmu = ln->attr(QStringLiteral("w"), QStringLiteral("9525")).toDouble();
    Look inner;
    readFill(ln, inner, ctx);
    if (inner.fillSet && inner.fillStyle == 5) { look.line = false; return; }
    look.lineColor = inner.fill.isValid() ? inner.fill : QColor(Qt::black);
    // A fully transparent line is no line.
    if (inner.fill.isValid() && inner.fill.alpha() == 0) { look.line = false; return; }
    look.line = true;
    if (const Node *dash = ln->child(QStringLiteral("prstDash"))) {
        const auto v = dash->attr(QStringLiteral("val"));
        if (v.contains(QLatin1String("dot")) && !v.contains(QLatin1String("dash"))) look.dash = 2;
        else if (v.contains(QLatin1String("dashDot"))) look.dash = 3;
        else if (v.contains(QLatin1String("dash"))) look.dash = 1;
    }
}

void readEffects(const Node *effectLst, Look &look, const ColorContext &ctx) {
    if (!effectLst) return;
    if (const Node *shadow = effectLst->child(QStringLiteral("outerShdw"))) {
        look.shadow = true;
        look.shadowColor = colorIn(shadow, ctx);
        if (!look.shadowColor.isValid()) look.shadowColor = QColor(0, 0, 0, 100);
        look.shadowDistEmu = shadow->attr(QStringLiteral("dist"), QStringLiteral("38100")).toDouble();
        look.shadowDir = shadow->attr(QStringLiteral("dir"), QStringLiteral("5400000")).toDouble() / 60000.0;
    }
}

// p:style — theme-indexed fill and line for shapes that name none of their own.
void applyStyleRefs(const Node *style, Look &look, const ColorContext &ctx) {
    if (!style) return;
    if (!look.fillSet) {
        if (const Node *ref = style->child(QStringLiteral("fillRef"))) {
            const int idx = ref->attr(QStringLiteral("idx")).toInt();
            const auto color = colorIn(ref, ctx);
            if (idx > 0 && color.isValid()) { look.fillSet = true; look.fillStyle = 0; look.fill = color; }
        }
    }
    if (!look.lineSet) {
        if (const Node *ref = style->child(QStringLiteral("lnRef"))) {
            const int idx = ref->attr(QStringLiteral("idx")).toInt();
            const auto color = colorIn(ref, ctx);
            if (idx > 0 && color.isValid()) {
                look.lineSet = true; look.line = true; look.lineColor = color;
                look.lineWidthEmu = 9525.0 * idx;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Text: what a run and a paragraph look like once every layer has spoken.

struct RunLook {
    qreal sz = 0;          // hundredths of a point; 0 unknown
    int b = -1, i = -1, u = -1, strike = -1;
    QColor color;
    QString family;
    int baseline = 0;
    bool caps = false;
    qreal spc = 0;         // hundredths of a point
    QString lang, link;
};

struct ParaLook {
    int algn = -1;         // 0 left, 1 centre, 2 right, 3 justify
    int bullet = -1;       // 0 none, 1 marks, 2 numbers
    int startAt = 1;
    qreal lnSpc = 0;       // percent
    qreal spcBef = -1;     // points
    int lvl = 0;
};

QString mapFont(const QString &face, const Theme *theme) {
    if (face.startsWith(QLatin1String("+mj"))) return theme ? theme->major : QString();
    if (face.startsWith(QLatin1String("+mn"))) return theme ? theme->minor : QString();
    return face;
}

struct TextContext {
    const Package *pkg = nullptr;
    QString partPath;                 // for hyperlink relationships
    const ColorContext *color = nullptr;
    const QHash<QString, QString> *slideIdByPath = nullptr;
};

void applyRPr(RunLook &run, const Node *rPr, const TextContext &ctx) {
    if (!rPr) return;
    if (rPr->has(QStringLiteral("sz"))) run.sz = rPr->attr(QStringLiteral("sz")).toDouble();
    if (rPr->has(QStringLiteral("b"))) run.b = rPr->attr(QStringLiteral("b")) == QLatin1String("1") || rPr->attr(QStringLiteral("b")) == QLatin1String("true");
    if (rPr->has(QStringLiteral("i"))) run.i = rPr->attr(QStringLiteral("i")) == QLatin1String("1") || rPr->attr(QStringLiteral("i")) == QLatin1String("true");
    if (rPr->has(QStringLiteral("u"))) run.u = rPr->attr(QStringLiteral("u")) != QLatin1String("none");
    if (rPr->has(QStringLiteral("strike"))) run.strike = rPr->attr(QStringLiteral("strike")) != QLatin1String("noStrike");
    if (rPr->has(QStringLiteral("baseline"))) {
        const int v = rPr->attr(QStringLiteral("baseline")).toInt();
        run.baseline = v > 0 ? 1 : v < 0 ? 2 : 0;
    }
    if (rPr->has(QStringLiteral("cap"))) run.caps = rPr->attr(QStringLiteral("cap")) == QLatin1String("all");
    if (rPr->has(QStringLiteral("spc"))) run.spc = rPr->attr(QStringLiteral("spc")).toDouble();
    if (rPr->has(QStringLiteral("lang"))) run.lang = rPr->attr(QStringLiteral("lang"));
    if (const Node *fill = rPr->child(QStringLiteral("solidFill"))) {
        const auto c = colorIn(fill, *ctx.color);
        if (c.isValid()) run.color = c;
    }
    if (const Node *latin = rPr->child(QStringLiteral("latin"))) {
        const auto face = mapFont(latin->attr(QStringLiteral("typeface")), ctx.color->theme);
        if (!face.isEmpty()) run.family = face;
    }
    if (const Node *link = rPr->child(QStringLiteral("hlinkClick"))) {
        const auto rid = link->attr(QStringLiteral("r:id"));
        const auto action = link->attr(QStringLiteral("action"));
        if (action.contains(QLatin1String("hlinksldjump")) && ctx.pkg && ctx.slideIdByPath) {
            const auto slidePath = ctx.pkg->target(ctx.partPath, rid);
            const auto id = ctx.slideIdByPath->value(slidePath);
            if (!id.isEmpty()) run.link = QStringLiteral("slide:") + id;
        } else if (ctx.pkg && !rid.isEmpty()) {
            const auto url = ctx.pkg->external(ctx.partPath, rid);
            if (!url.isEmpty()) run.link = url;
        }
    }
}

void applyPPr(ParaLook &para, RunLook &run, const Node *pPr, const TextContext &ctx) {
    if (!pPr) return;
    const auto algn = pPr->attr(QStringLiteral("algn"));
    if (algn == QLatin1String("l")) para.algn = 0;
    else if (algn == QLatin1String("ctr")) para.algn = 1;
    else if (algn == QLatin1String("r")) para.algn = 2;
    else if (algn == QLatin1String("just") || algn == QLatin1String("dist")) para.algn = 3;
    if (const Node *ln = pPr->path({"lnSpc", "spcPct"})) para.lnSpc = ln->attr(QStringLiteral("val")).toDouble() / 1000.0;
    if (const Node *bef = pPr->path({"spcBef", "spcPts"})) para.spcBef = bef->attr(QStringLiteral("val")).toDouble() / 100.0;
    if (pPr->child(QStringLiteral("buNone"))) para.bullet = 0;
    else if (pPr->child(QStringLiteral("buChar")) || pPr->child(QStringLiteral("buBlip"))) para.bullet = 1;
    else if (const Node *num = pPr->child(QStringLiteral("buAutoNum"))) {
        para.bullet = 2;
        para.startAt = qMax(1, num->attr(QStringLiteral("startAt"), QStringLiteral("1")).toInt());
    }
    applyRPr(run, pPr->child(QStringLiteral("defRPr")), ctx);
}

// Layers holding lvl1pPr…lvl9pPr, lowest priority first; and the bodyPr
// layers the same way, for the anchor and the autofit.
struct TextChain {
    QVector<const Node *> levels;
    QVector<const Node *> bodies;
};

void resolveLevel(const TextChain &chain, int lvl, ParaLook &para, RunLook &run, const TextContext &ctx) {
    const auto name = QStringLiteral("lvl%1pPr").arg(qBound(0, lvl, 8) + 1);
    for (const auto *layer : chain.levels) {
        if (!layer) continue;
        applyPPr(para, run, layer->child(name), ctx);
    }
}

struct TextOut {
    QString text;
    QVector<TextRun> runs;
    RunLook box;
    ParaLook para;
    int listStyle = 0, listStart = 1;
    int anchor = 0;           // 0 top, 1 middle, 2 bottom
    bool shrink = false;
    qreal fontScale = 1;
    QString link;
    bool empty = true;
};

// Reads a txBody (or a table cell's) into words, a box look and the runs
// that differ from it.
TextOut convertText(const Node *txBody, const TextChain &chain, const TextContext &ctx) {
    TextOut out;
    if (!txBody) return out;
    // bodyPr: the nearest layer that says wins.
    for (const auto *body : chain.bodies) {
        if (!body) continue;
        const auto anchor = body->attr(QStringLiteral("anchor"));
        if (anchor == QLatin1String("t")) out.anchor = 0;
        else if (anchor == QLatin1String("ctr")) out.anchor = 1;
        else if (anchor == QLatin1String("b")) out.anchor = 2;
    }
    if (const Node *own = txBody->child(QStringLiteral("bodyPr"))) {
        const auto anchor = own->attr(QStringLiteral("anchor"));
        if (anchor == QLatin1String("t")) out.anchor = 0;
        else if (anchor == QLatin1String("ctr")) out.anchor = 1;
        else if (anchor == QLatin1String("b")) out.anchor = 2;
        if (const Node *fit = own->child(QStringLiteral("normAutofit"))) {
            out.shrink = true;
            if (fit->has(QStringLiteral("fontScale")))
                out.fontScale = qBound(0.1, fit->attr(QStringLiteral("fontScale")).toDouble() / 100000.0, 1.0);
        }
    }
    TextChain full = chain;
    full.levels.append(txBody->child(QStringLiteral("lstStyle")));

    struct Piece { int start, length; RunLook look; };
    QVector<Piece> pieces;
    bool firstParagraph = true, haveBox = false;
    int bulleted = 0, numbered = 0, plain = 0;
    for (const auto *p : txBody->all(QStringLiteral("p"))) {
        const Node *pPr = p->child(QStringLiteral("pPr"));
        const int lvl = qBound(0, pPr ? pPr->attr(QStringLiteral("lvl"), QStringLiteral("0")).toInt() : 0, 8);
        ParaLook para;
        RunLook base;
        resolveLevel(full, lvl, para, base, ctx);
        applyPPr(para, base, pPr, ctx);
        if (!firstParagraph) out.text += QLatin1Char('\n');
        QString line;
        bool any = false;
        for (const auto &k : p->kids) {
            if (k.name == QLatin1String("r") || k.name == QLatin1String("fld")) {
                const Node *t = k.child(QStringLiteral("t"));
                const QString words = t ? t->text : QString();
                RunLook look = base;
                applyRPr(look, k.child(QStringLiteral("rPr")), ctx);
                if (!look.link.isEmpty() && out.link.isEmpty()) out.link = look.link;
                if (words.isEmpty()) continue;
                pieces.append({out.text.size() + line.size() + (para.bullet > 0 ? lvl : 0), words.size(), look});
                line += words;
                any = true;
                if (!haveBox) { out.box = look; out.para = para; haveBox = true; }
            } else if (k.name == QLatin1String("br")) {
                line += QLatin1Char('\n');
            }
        }
        if (!any && !haveBox) {
            // An empty paragraph still says what the box would look like.
            RunLook look = base;
            applyRPr(look, p->child(QStringLiteral("endParaRPr")), ctx);
            out.box = look; out.para = para; haveBox = true;
        }
        if (any) {
            if (para.bullet == 1) ++bulleted; else if (para.bullet == 2) ++numbered; else ++plain;
            out.empty = false;
        }
        if (para.bullet > 0 && lvl > 0) out.text += QString(lvl, QLatin1Char('\t'));
        out.text += line;
        if (para.bullet == 2 && out.listStart == 1) out.listStart = para.startAt;
        firstParagraph = false;
    }
    if (bulleted + numbered > plain && bulleted + numbered > 0) out.listStyle = numbered > bulleted ? 2 : 1;
    // Runs that differ from the box.
    for (const auto &piece : pieces) {
        TextRun run;
        run.start = piece.start; run.length = piece.length;
        const auto &l = piece.look; const auto &b = out.box;
        bool differs = false;
        if ((l.b > 0) != (b.b > 0)) { run.weight = l.b > 0 ? 700 : 400; differs = true; }
        if ((l.i > 0) != (b.i > 0)) { run.italic = l.i > 0 ? 1 : 2; differs = true; }
        if ((l.u > 0) != (b.u > 0)) { run.underline = l.u > 0 ? 1 : 2; differs = true; }
        if ((l.strike > 0) != (b.strike > 0)) { run.strike = l.strike > 0 ? 1 : 2; differs = true; }
        if (l.baseline != b.baseline) { run.baseline = l.baseline; differs = true; }
        if (!qFuzzyCompare(l.sz + 1, b.sz + 1) && l.sz > 0) { run.fontSize = l.sz; differs = true; } // scaled later
        if (l.family != b.family && !l.family.isEmpty()) { run.fontFamily = l.family; differs = true; }
        if (l.color.isValid() && l.color != b.color) { run.color = l.color; differs = true; }
        if (l.lang != b.lang && !l.lang.isEmpty()) { run.language = l.lang; differs = true; }
        if (differs) out.runs.append(run);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Geometry.

struct Units {
    qreal k = 1;    // slide units per EMU
    qreal pt = 1;   // slide units per point
};

struct GroupSpace {
    QTransform points;   // child space → slide space
    qreal sx = 1, sy = 1;
    qreal rotation = 0;
};

QRectF xfrmRect(const Node *xfrm, const Units &u) {
    if (!xfrm) return QRectF();
    const Node *off = xfrm->child(QStringLiteral("off"));
    const Node *ext = xfrm->child(QStringLiteral("ext"));
    if (!off || !ext) return QRectF();
    return QRectF(off->attr(QStringLiteral("x")).toDouble() * u.k, off->attr(QStringLiteral("y")).toDouble() * u.k,
                  ext->attr(QStringLiteral("cx")).toDouble() * u.k, ext->attr(QStringLiteral("cy")).toDouble() * u.k);
}

struct Placement { QRectF rect; qreal rotation = 0; bool flipH = false, flipV = false; bool valid = false; };

Placement placementOf(const Node *xfrm, const Units &u, const GroupSpace &space) {
    Placement p;
    if (!xfrm) return p;
    const auto rect = xfrmRect(xfrm, u);
    if (rect.isEmpty() && rect.width() <= 0 && rect.height() <= 0) return p;
    const QPointF centre = space.points.map(rect.center());
    const qreal w = rect.width() * space.sx, h = rect.height() * space.sy;
    p.rect = QRectF(centre.x() - w / 2, centre.y() - h / 2, w, h);
    p.rotation = xfrm->attr(QStringLiteral("rot"), QStringLiteral("0")).toDouble() / 60000.0 + space.rotation;
    p.flipH = xfrm->attr(QStringLiteral("flipH")) == QLatin1String("1");
    p.flipV = xfrm->attr(QStringLiteral("flipV")) == QLatin1String("1");
    p.valid = true;
    return p;
}

int presetKind(const QString &prst, qreal *corner) {
    static const QHash<QString, int> kinds{
        {QStringLiteral("rect"), 0}, {QStringLiteral("roundRect"), 0}, {QStringLiteral("snipRoundRect"), 0},
        {QStringLiteral("round1Rect"), 0}, {QStringLiteral("round2SameRect"), 0}, {QStringLiteral("round2DiagRect"), 0},
        {QStringLiteral("ellipse"), 1}, {QStringLiteral("triangle"), 2}, {QStringLiteral("rtTriangle"), 2},
        {QStringLiteral("diamond"), 3}, {QStringLiteral("pentagon"), 4}, {QStringLiteral("hexagon"), 5},
        {QStringLiteral("star5"), 6}, {QStringLiteral("star4"), 6}, {QStringLiteral("star6"), 6}, {QStringLiteral("star8"), 6},
        {QStringLiteral("heart"), 7}, {QStringLiteral("rightArrow"), 8}, {QStringLiteral("homePlate"), 8},
        {QStringLiteral("chevron"), 9}, {QStringLiteral("wedgeRectCallout"), 10}, {QStringLiteral("wedgeRoundRectCallout"), 10},
        {QStringLiteral("parallelogram"), 11}, {QStringLiteral("can"), 12}, {QStringLiteral("line"), 13},
        {QStringLiteral("straightConnector1"), 13}, {QStringLiteral("mathPlus"), 14}, {QStringLiteral("plus"), 14},
        {QStringLiteral("mathMultiply"), 15}, {QStringLiteral("leftArrow"), 16}, {QStringLiteral("leftRightArrow"), 17},
        {QStringLiteral("flowChartDocument"), 18}, {QStringLiteral("trapezoid"), 19}, {QStringLiteral("flowChartTerminator"), 20},
        {QStringLiteral("cloud"), 21}, {QStringLiteral("cloudCallout"), 21}, {QStringLiteral("arc"), 22}, {QStringLiteral("donut"), 23},
        {QStringLiteral("flowChartProcess"), 0}, {QStringLiteral("flowChartConnector"), 1}, {QStringLiteral("flowChartDecision"), 3},
    };
    if (corner) *corner = prst.startsWith(QLatin1String("round")) ? 0.16667 : 0;
    return kinds.value(prst, -1);
}

// custGeom → a path in slide units inside `rect`.
QPainterPath customPath(const Node *custGeom, const QRectF &rect) {
    QPainterPath out;
    const Node *list = custGeom ? custGeom->child(QStringLiteral("pathLst")) : nullptr;
    if (!list) return out;
    for (const auto *path : list->all(QStringLiteral("path"))) {
        const qreal w = path->attr(QStringLiteral("w"), QStringLiteral("0")).toDouble();
        const qreal h = path->attr(QStringLiteral("h"), QStringLiteral("0")).toDouble();
        const qreal sx = w > 0 ? rect.width() / w : 1, sy = h > 0 ? rect.height() / h : 1;
        auto pt = [&](const Node *n) {
            return QPointF(rect.x() + n->attr(QStringLiteral("x")).toDouble() * sx,
                           rect.y() + n->attr(QStringLiteral("y")).toDouble() * sy);
        };
        for (const auto &cmd : path->kids) {
            const auto pts = cmd.all(QStringLiteral("pt"));
            if (cmd.name == QLatin1String("moveTo") && pts.size() >= 1) out.moveTo(pt(pts[0]));
            else if (cmd.name == QLatin1String("lnTo") && pts.size() >= 1) out.lineTo(pt(pts[0]));
            else if (cmd.name == QLatin1String("cubicBezTo") && pts.size() >= 3) out.cubicTo(pt(pts[0]), pt(pts[1]), pt(pts[2]));
            else if (cmd.name == QLatin1String("quadBezTo") && pts.size() >= 2) out.quadTo(pt(pts[0]), pt(pts[1]));
            else if (cmd.name == QLatin1String("close")) out.closeSubpath();
            else if (cmd.name == QLatin1String("arcTo")) {
                // An elliptical arc from the current point; approximated by its chord.
                const qreal wr = cmd.attr(QStringLiteral("wR")).toDouble() * sx, hr = cmd.attr(QStringLiteral("hR")).toDouble() * sy;
                const qreal st = cmd.attr(QStringLiteral("stAng")).toDouble() / 60000.0, sw = cmd.attr(QStringLiteral("swAng")).toDouble() / 60000.0;
                const QPointF cur = out.currentPosition();
                const QPointF centre(cur.x() - wr * qCos(qDegreesToRadians(st)), cur.y() - hr * qSin(qDegreesToRadians(st)));
                out.arcTo(QRectF(centre.x() - wr, centre.y() - hr, 2 * wr, 2 * hr), -st, -sw);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Warnings are counted per message with the slides they came from.

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

// ---------------------------------------------------------------------------
// Placeholders.

struct Ph { bool is = false; QString type, idx; };

Ph phOf(const Node *sp) {
    Ph ph;
    const Node *nv = sp->child(QStringLiteral("nvSpPr"));
    if (!nv) nv = sp->child(QStringLiteral("nvPicPr"));
    if (!nv) nv = sp->child(QStringLiteral("nvGraphicFramePr"));
    const Node *node = nv ? nv->path({"nvPr", "ph"}) : nullptr;
    if (!node) return ph;
    ph.is = true;
    ph.type = node->attr(QStringLiteral("type"), QStringLiteral("body"));
    ph.idx = node->attr(QStringLiteral("idx"));
    return ph;
}

QString phClass(const QString &type) {
    if (type == QLatin1String("title") || type == QLatin1String("ctrTitle")) return QStringLiteral("title");
    if (type == QLatin1String("body") || type == QLatin1String("subTitle") || type == QLatin1String("obj") || type.isEmpty())
        return QStringLiteral("body");
    return type;
}

const Node *spTreeOf(const Node *root) {
    return root ? root->path({"cSld", "spTree"}) : nullptr;
}

// The placeholder in another part that a placeholder inherits from.
const Node *findPh(const Node *root, const Ph &want, bool useIdx) {
    const Node *tree = spTreeOf(root);
    if (!tree) return nullptr;
    QVector<const Node *> shapes;
    flattened(*tree, shapes);
    const Node *best = nullptr;
    int bestScore = 0;
    for (const auto *sp : shapes) {
        if (sp->name != QLatin1String("sp")) continue;
        const Ph ph = phOf(sp);
        if (!ph.is) continue;
        int score = 0;
        if (useIdx && !want.idx.isEmpty() && ph.idx == want.idx) score = 3;
        else if (phClass(ph.type) == phClass(want.type)) score = ph.type == want.type ? 2 : 1;
        if (score > bestScore) { best = sp; bestScore = score; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// The conversion of one part's shapes.

struct MasterInfo {
    QString path;
    QString id;             // Master.id in the document
    ColorContext color;
    Theme theme;
    const Node *root = nullptr;
};

struct Reader {
    const Package &pkg;
    Units u;
    Document doc;
    Warnings warnings;
    const Node *presentation = nullptr;
    QString presentationPath;
    std::map<QString, MasterInfo> masters;  // by part path; a map so references stay put
    QHash<QString, QString> layoutIds;      // part path → SlideLayout.id
    QHash<QString, QString> slideIdByPath;
    QString scratchDir;

    explicit Reader(const Package &package) : pkg(package) {}

    const Node *defaultTextStyle() const {
        return presentation ? presentation->child(QStringLiteral("defaultTextStyle")) : nullptr;
    }

    // What one shape needs to know about where it is.
    struct Scope {
        QString partPath;              // the part the shapes are in (slide, layout or master)
        const Node *slide = nullptr;   // null when converting a layout or master
        const Node *layout = nullptr;
        const MasterInfo *master = nullptr;
        int slideNumber = 0;
        QString slideId;
        QVector<SceneObject> *objects = nullptr;
        QHash<QString, QString> *spidToObject = nullptr;
        MasterFields *fields = nullptr;
        bool fieldsSeen = false;
    };

    TextContext textContext(const Scope &scope) const {
        TextContext ctx;
        ctx.pkg = &pkg;
        ctx.partPath = scope.partPath;
        ctx.color = &scope.master->color;
        ctx.slideIdByPath = &slideIdByPath;
        return ctx;
    }

    // Layers for a shape's text, lowest first.
    TextChain chainFor(const Scope &scope, const Ph &ph, const Node *layoutSp, const Node *masterSp) const {
        TextChain chain;
        chain.levels.append(defaultTextStyle());
        const Node *styles = scope.master->root ? scope.master->root->child(QStringLiteral("txStyles")) : nullptr;
        if (styles) {
            const auto cls = ph.is ? phClass(ph.type) : QString();
            if (cls == QLatin1String("title")) chain.levels.append(styles->child(QStringLiteral("titleStyle")));
            else if (cls == QLatin1String("body")) chain.levels.append(styles->child(QStringLiteral("bodyStyle")));
            else chain.levels.append(styles->child(QStringLiteral("otherStyle")));
        }
        for (const auto *sp : {masterSp, layoutSp}) {
            if (!sp) continue;
            const Node *body = sp->child(QStringLiteral("txBody"));
            if (!body) continue;
            chain.levels.append(body->child(QStringLiteral("lstStyle")));
            chain.bodies.append(body->child(QStringLiteral("bodyPr")));
        }
        return chain;
    }

    void applyLook(SceneObject &o, const Look &look) const {
        o.fillStyle = look.fillSet ? look.fillStyle : 5;
        if (look.fill.isValid()) o.fill = look.fill;
        if (look.fill2.isValid()) o.fillSecondary = look.fill2;
        o.fillAngle = look.angle;
        if (look.line) {
            o.strokeColor = look.lineColor;
            o.strokeWidth = qMax(0.5, look.lineWidthEmu * u.k);
            o.strokeStyle = look.dash;
        } else o.strokeWidth = 0;
        if (look.shadow) {
            o.shadowEnabled = true;
            o.shadowColor = look.shadowColor;
            const qreal d = look.shadowDistEmu * u.k;
            o.shadowX = d * qCos(qDegreesToRadians(look.shadowDir));
            o.shadowY = d * qSin(qDegreesToRadians(look.shadowDir));
        }
    }

    void applyText(SceneObject &o, const TextOut &text, const RunLook &fallback) {
        o.type = ObjectType::Text;
        o.text = text.text;
        const auto &b = text.box;
        const qreal sz = (b.sz > 0 ? b.sz : fallback.sz > 0 ? fallback.sz : 1800) / 100.0 * text.fontScale;
        o.fontSize = sz * u.pt;
        o.fontWeight = b.b > 0 ? 700 : 400;
        o.italic = b.i > 0;
        o.underline = b.u > 0;
        o.textColor = b.color.isValid() ? b.color : fallback.color.isValid() ? fallback.color : QColor(Qt::black);
        o.fontFamily = !b.family.isEmpty() ? b.family : !fallback.family.isEmpty() ? fallback.family : QStringLiteral("Inter");
        o.uppercase = b.caps;
        o.letterSpacing = b.spc / 100.0 * u.pt;
        if (!b.lang.isEmpty()) { o.language = b.lang; o.language.replace(QLatin1Char('-'), QLatin1Char('_')); }
        o.textAlign = text.para.algn < 0 ? 0 : text.para.algn;
        o.verticalAlign = text.anchor;
        o.lineHeight = text.para.lnSpc > 0 ? text.para.lnSpc : 100;
        o.paragraphSpacing = text.para.spcBef > 0 ? text.para.spcBef * u.pt : 0;
        o.listStyle = text.listStyle;
        o.listStart = text.listStart;
        o.textFit = text.shrink ? 1 : 0;
        o.fillStyle = 5;
        o.strokeWidth = 0;
        o.fillToken.clear(); o.textColorToken.clear(); o.fontToken.clear();
        QVector<TextRun> runs = text.runs;
        for (auto &run : runs) {
            if (run.fontSize > 0) run.fontSize = run.fontSize / 100.0 * text.fontScale * u.pt;
            if (!run.language.isEmpty()) run.language.replace(QLatin1Char('-'), QLatin1Char('_'));
        }
        o.runs = TextRuns::tidy(runs, o.text.size());
        if (!text.link.isEmpty()) {
            if (text.link.startsWith(QLatin1String("slide:"))) { o.linkKind = 3; o.linkTarget = text.link.mid(6); }
            else if (text.link.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive)) { o.linkKind = 2; o.linkTarget = text.link.mid(7); }
            else if (text.link.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) { o.linkKind = 1; o.linkTarget = text.link; }
            // A link the deck would refuse to reopen with is no link.
            if (o.linkKind && !Links::validate(o.linkKind, o.linkTarget, doc, true).isEmpty()) {
                o.linkKind = 0; o.linkTarget.clear();
                warnings.add(QStringLiteral("A link OmaShow cannot follow was left off: %1").arg(text.link.left(80)), 0);
            }
        }
    }

    void finish(SceneObject &o, const Placement &p, const QStringList &groups) const {
        o.rect = p.rect;
        o.rotation = p.rotation;
        o.groups = groups;
    }

    // ---- shapes

    void convertShape(const Node *sp, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        const Ph ph = phOf(sp);
        const Node *layoutSp = nullptr, *masterSp = nullptr;
        if (ph.is) {
            const auto cls = phClass(ph.type);
            if (cls == QLatin1String("sldNum") || cls == QLatin1String("dt") || cls == QLatin1String("ftr") || cls == QLatin1String("hdr")) {
                if (scope.slide && scope.fields) {
                    const Node *body = sp->child(QStringLiteral("txBody"));
                    const auto words = body ? body->words().trimmed() : QString();
                    if (cls == QLatin1String("sldNum")) scope.fields->showNumber = true;
                    else if (cls == QLatin1String("ftr") && !words.isEmpty()) { scope.fields->showFooter = true; scope.fields->footer = words; }
                    else if (cls == QLatin1String("dt")) { scope.fields->showDate = true; if (!body || !body->path({"p", "fld"})) scope.fields->date = words; }
                    scope.fieldsSeen = true;
                }
                return;
            }
            if (scope.slide) {
                layoutSp = findPh(scope.layout, ph, true);
                const Ph layoutPh = layoutSp ? phOf(layoutSp) : ph;
                masterSp = findPh(scope.master->root, layoutPh, false);
            } else if (scope.layout && scope.partPath != scope.master->path) {
                masterSp = findPh(scope.master->root, ph, false);
            }
        }

        // Geometry: its own, else inherited.
        const Node *spPr = sp->child(QStringLiteral("spPr"));
        Placement place = placementOf(spPr ? spPr->child(QStringLiteral("xfrm")) : nullptr, u, space);
        for (const auto *from : {layoutSp, masterSp}) {
            if (place.valid || !from) continue;
            const Node *pr = from->child(QStringLiteral("spPr"));
            place = placementOf(pr ? pr->child(QStringLiteral("xfrm")) : nullptr, u, GroupSpace());
        }
        if (!place.valid) return;

        // Look: its own, else the placeholder it fills, else the theme reference.
        Look look;
        const auto &color = scope.master->color;
        readFill(spPr, look, color);
        if (spPr) readLine(spPr->child(QStringLiteral("ln")), look, color);
        if (spPr) readEffects(spPr->child(QStringLiteral("effectLst")), look, color);
        for (const auto *from : {layoutSp, masterSp}) {
            if (!from) continue;
            const Node *pr = from->child(QStringLiteral("spPr"));
            if (!look.fillSet) readFill(pr, look, color);
            if (!look.lineSet && pr) readLine(pr->child(QStringLiteral("ln")), look, color);
        }
        applyStyleRefs(sp->child(QStringLiteral("style")), look, color);
        if (!look.blipRid.isEmpty()) warnings.add(QStringLiteral("A shape filled with a picture was given no fill"), scope.slideNumber);

        // Words.
        const Node *txBody = sp->child(QStringLiteral("txBody"));
        TextChain chain = chainFor(scope, ph, layoutSp, masterSp);
        TextOut text = convertText(txBody, chain, textContext(scope));
        RunLook fallback;
        { ParaLook para; resolveLevel(chain, 0, para, fallback, textContext(scope)); }

        const bool visibleBox = (look.fillSet && look.fillStyle != 5) || look.line;
        const QString spid = [&] {
            const Node *nv = sp->child(QStringLiteral("nvSpPr"));
            const Node *c = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
            return c ? c->attr(QStringLiteral("id")) : QString();
        }();
        const QString name = [&] {
            const Node *nv = sp->child(QStringLiteral("nvSpPr"));
            const Node *c = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
            return c ? c->attr(QStringLiteral("name")) : QString();
        }();
        const bool hidden = [&] {
            const Node *nv = sp->child(QStringLiteral("nvSpPr"));
            const Node *c = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
            return c && c->attr(QStringLiteral("hidden")) == QLatin1String("1");
        }();

        QStringList own = groups;
        QString firstId;
        if (visibleBox) {
            SceneObject box;
            box.id = Edit::newId(QStringLiteral("shape"));
            box.type = ObjectType::Rect;
            finish(box, place, groups);
            applyLook(box, look);
            const Node *prst = spPr ? spPr->child(QStringLiteral("prstGeom")) : nullptr;
            const Node *cust = spPr ? spPr->child(QStringLiteral("custGeom")) : nullptr;
            qreal corner = 0;
            const int kind = prst ? presetKind(prst->attr(QStringLiteral("prst")), &corner) : cust ? -2 : 0;
            if (kind == -2) {
                const auto path = customPath(cust, place.rect);
                if (!path.isEmpty()) { const qreal rot = box.rotation; Shape::assignPath(box, path); box.rotation = rot; }
                else box.shapeKind = 0;
            } else if (kind < 0) {
                box.shapeKind = 0;
                warnings.add(QStringLiteral("The %1 shape was drawn as a rectangle").arg(prst->attr(QStringLiteral("prst"))), scope.slideNumber);
            } else {
                box.shapeKind = kind;
                if (corner > 0) {
                    // The rounding is a fraction of the shorter side; a pill is 50000.
                    if (const Node *adjustments = prst->child(QStringLiteral("avLst")))
                        for (const auto *gd : adjustments->all(QStringLiteral("gd")))
                            if (gd->attr(QStringLiteral("name")) == QLatin1String("adj")) {
                                const auto formula = gd->attr(QStringLiteral("fmla"));
                                if (formula.startsWith(QLatin1String("val "))) corner = qBound(0.0, formula.mid(4).toDouble() / 100000.0, 0.5);
                            }
                    box.cornerRadius = qMin(place.rect.width(), place.rect.height()) * corner;
                }
                if (kind == 13 && (place.flipV != place.flipH)) {
                    // A diagonal line: the rectangle's other diagonal.
                    QPainterPath path;
                    path.moveTo(place.rect.bottomLeft());
                    path.lineTo(place.rect.topRight());
                    const qreal rot = box.rotation; const auto keep = box.rect;
                    Shape::assignPath(box, path); box.rotation = rot; box.rect = keep;
                }
            }
            if (!text.empty && !text.text.trimmed().isEmpty()) {
                const auto groupId = Edit::newId(QStringLiteral("group"));
                own.append(groupId);
                box.groups = own;
            }
            if (!name.isEmpty()) box.altTitle = name;
            box.hidden = hidden;
            scope.objects->append(box);
            firstId = box.id;
        }
        if (!text.empty && !text.text.trimmed().isEmpty()) {
            SceneObject words;
            words.id = Edit::newId(QStringLiteral("text"));
            finish(words, place, own);
            applyText(words, text, fallback);
            if (!name.isEmpty() && firstId.isEmpty()) words.altTitle = name;
            words.hidden = hidden;
            scope.objects->append(words);
            if (firstId.isEmpty()) firstId = words.id;
        } else if (!visibleBox) {
            return;
        }
        if (scope.spidToObject && !spid.isEmpty() && !firstId.isEmpty()) scope.spidToObject->insert(spid, firstId);
    }

    // ---- pictures and films

    bool decodePicture(SceneObject &o, const QString &mediaPath, int slideNumber) {
        const auto bytes = pkg.bytes(mediaPath);
        const auto suffix = QFileInfo(mediaPath).suffix().toLower();
        if (bytes.isEmpty()) {
            warnings.add(QStringLiteral("A picture's file was missing from the deck"), slideNumber);
            return false;
        }
        QString error;
        if (suffix == QLatin1String("svg")) {
            if (SvgAsset::decode(o, bytes, &error)) return true;
        } else if (suffix == QLatin1String("emf") || suffix == QLatin1String("wmf")) {
            warnings.add(QStringLiteral("A picture in the %1 format was left out").arg(suffix.toUpper()), slideNumber);
            return false;
        } else if (ImageAsset::decode(o, bytes, &error)) return true;
        warnings.add(QStringLiteral("A picture could not be read: %1").arg(error.isEmpty() ? suffix : error), slideNumber);
        return false;
    }

    void convertPicture(const Node *pic, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        const Ph ph = phOf(pic);
        const Node *spPr = pic->child(QStringLiteral("spPr"));
        Placement place = placementOf(spPr ? spPr->child(QStringLiteral("xfrm")) : nullptr, u, space);
        if (!place.valid && ph.is && scope.slide) {
            const Node *layoutSp = findPh(scope.layout, ph, true);
            const Node *pr = layoutSp ? layoutSp->child(QStringLiteral("spPr")) : nullptr;
            place = placementOf(pr ? pr->child(QStringLiteral("xfrm")) : nullptr, u, GroupSpace());
        }
        if (!place.valid) return;

        const Node *nv = pic->child(QStringLiteral("nvPicPr"));
        const Node *cNvPr = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
        const QString spid = cNvPr ? cNvPr->attr(QStringLiteral("id")) : QString();
        const QString name = cNvPr ? cNvPr->attr(QStringLiteral("name")) : QString();
        const QString descr = cNvPr ? cNvPr->attr(QStringLiteral("descr")) : QString();

        // A film or a sound is a picture with a media link beside it.
        const Node *nvPr = nv ? nv->child(QStringLiteral("nvPr")) : nullptr;
        const Node *video = nvPr ? nvPr->child(QStringLiteral("videoFile")) : nullptr;
        const Node *audio = nvPr ? nvPr->child(QStringLiteral("audioFile")) : nullptr;
        if (video || audio) {
            const auto rid = (video ? video : audio)->attr(QStringLiteral("link"));
            const auto mediaPath = pkg.target(scope.partPath, rid);
            const auto external = pkg.external(scope.partPath, rid);
            if (!mediaPath.isEmpty() && pkg.has(mediaPath)) {
                SceneObject media = convertMedia(mediaPath, scope.slideNumber);
                if (!media.id.isEmpty()) {
                    finish(media, place, groups);
                    if (!name.isEmpty()) media.altTitle = name;
                    if (!descr.isEmpty()) media.altText = descr;
                    scope.objects->append(media);
                    if (scope.spidToObject && !spid.isEmpty()) scope.spidToObject->insert(spid, media.id);
                    return;
                }
            } else if (!external.isEmpty()) {
                warnings.add(QStringLiteral("A linked film outside the deck was shown as its poster picture"), scope.slideNumber);
            }
        }

        const Node *blipFill = pic->child(QStringLiteral("blipFill"));
        const Node *blip = blipFill ? blipFill->child(QStringLiteral("blip")) : nullptr;
        if (!blip) return;
        QString rid = blip->attr(QStringLiteral("embed"));
        // A vector original beside the bitmap is the better picture.
        QVector<const Node *> svgs;
        blip->collect(QStringLiteral("svgBlip"), svgs);
        QString mediaPath;
        if (!svgs.isEmpty()) {
            const auto svgPath = pkg.target(scope.partPath, svgs.first()->attr(QStringLiteral("embed")));
            if (pkg.has(svgPath)) mediaPath = svgPath;
        }
        if (mediaPath.isEmpty()) mediaPath = pkg.target(scope.partPath, rid);
        if (mediaPath.isEmpty()) {
            if (!pkg.external(scope.partPath, blip->attr(QStringLiteral("link"))).isEmpty())
                warnings.add(QStringLiteral("A picture linked from outside the deck was left out"), scope.slideNumber);
            return;
        }
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("image"));
        o.type = ObjectType::Image;
        if (!decodePicture(o, mediaPath, scope.slideNumber)) return;
        finish(o, place, groups);
        o.fillStyle = 5;
        o.imageMode = 2; // the frame is the picture, as PowerPoint places it
        if (const Node *src = blipFill->child(QStringLiteral("srcRect"))) {
            const qreal l = src->attr(QStringLiteral("l"), QStringLiteral("0")).toDouble() / 100000.0;
            const qreal t = src->attr(QStringLiteral("t"), QStringLiteral("0")).toDouble() / 100000.0;
            const qreal r = src->attr(QStringLiteral("r"), QStringLiteral("0")).toDouble() / 100000.0;
            const qreal b = src->attr(QStringLiteral("b"), QStringLiteral("0")).toDouble() / 100000.0;
            if (l + r < 1 && t + b < 1 && (l > 0 || t > 0 || r > 0 || b > 0)) o.imageCrop = QRectF(l, t, 1 - l - r, 1 - t - b);
        }
        if (spPr) {
            if (const Node *prst = spPr->child(QStringLiteral("prstGeom"))) {
                const auto p = prst->attr(QStringLiteral("prst"));
                if (p == QLatin1String("ellipse")) o.imageMask = 1;
                else if (p.startsWith(QLatin1String("round"))) o.imageMask = 2;
                else if (p == QLatin1String("hexagon")) o.imageMask = 3;
                else if (p == QLatin1String("heart")) o.imageMask = 4;
            }
            Look look;
            readLine(spPr->child(QStringLiteral("ln")), look, scope.master->color);
            readEffects(spPr->child(QStringLiteral("effectLst")), look, scope.master->color);
            look.fillSet = true; look.fillStyle = 5;
            applyLook(o, look);
            o.fillStyle = 5;
        }
        for (const auto &k : blip->kids) {
            if (k.name == QLatin1String("alphaModFix")) o.opacity = qBound(0.0, k.attr(QStringLiteral("amt"), QStringLiteral("100000")).toDouble() / 100000.0, 1.0);
            else if (k.name == QLatin1String("grayscl")) o.imageSaturation = 0;
            else if (k.name == QLatin1String("lum")) {
                o.imageBrightness = k.attr(QStringLiteral("bright"), QStringLiteral("0")).toDouble() / 100000.0;
                o.imageContrast = 1 + k.attr(QStringLiteral("contrast"), QStringLiteral("0")).toDouble() / 100000.0;
            }
        }
        if (!name.isEmpty()) o.altTitle = name;
        if (!descr.isEmpty()) o.altText = descr;
        o.hidden = cNvPr && cNvPr->attr(QStringLiteral("hidden")) == QLatin1String("1");
        scope.objects->append(o);
        if (scope.spidToObject && !spid.isEmpty()) scope.spidToObject->insert(spid, o.id);
    }

    SceneObject convertMedia(const QString &mediaPath, int slideNumber) {
        SceneObject none;
        if (scratchDir.isEmpty()) return none;
        QTemporaryFile file(scratchDir + QStringLiteral("/omashow-import-XXXXXX.") + QFileInfo(mediaPath).suffix());
        if (!file.open()) return none;
        file.write(pkg.bytes(mediaPath));
        file.flush();
        const auto result = MediaAsset::fromFile(file.fileName(), true, {}, QFileInfo(mediaPath).fileName());
        if (!result.ok()) {
            warnings.add(QStringLiteral("A film could not be embedded: %1").arg(result.error), slideNumber);
            return none;
        }
        SceneObject o = result.object;
        o.id = Edit::newId(QStringLiteral("media"));
        o.type = ObjectType::Media;
        o.mediaName = QFileInfo(mediaPath).fileName();
        return o;
    }

    // ---- tables

    void convertTable(const Node *tbl, const Placement &place, Scope &scope, const QStringList &groups, const QString &spid, const QString &name) {
        const Node *grid = tbl->child(QStringLiteral("tblGrid"));
        const auto rowsNodes = tbl->all(QStringLiteral("tr"));
        const auto colsNodes = grid ? grid->all(QStringLiteral("gridCol")) : QVector<const Node *>();
        const int rows = rowsNodes.size(), columns = colsNodes.size();
        if (rows < 1 || columns < 1 || rows > Table::maxRows || columns > Table::maxColumns) {
            warnings.add(QStringLiteral("A table was too large to bring across"), scope.slideNumber);
            return;
        }
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("table"));
        o.type = ObjectType::Table;
        o.table = Table::create(rows, columns);
        finish(o, place, groups);
        // Sizes are relative: each against the mean, so a wide column stays wide.
        auto relative = [](QVector<qreal> &sizes) {
            qreal total = 0;
            for (qreal s : sizes) total += s;
            const qreal mean = sizes.isEmpty() || total <= 0 ? 1 : total / sizes.size();
            for (auto &s : sizes) s = qBound(0.05, s / mean, 20.0);
        };
        for (int c = 0; c < columns; ++c) o.table.columns[c] = qMax(1.0, colsNodes[c]->attr(QStringLiteral("w")).toDouble());
        for (int r = 0; r < rows; ++r) o.table.rows[r] = qMax(1.0, rowsNodes[r]->attr(QStringLiteral("h")).toDouble());
        relative(o.table.columns);
        relative(o.table.rows);
        const Node *tblPr = tbl->child(QStringLiteral("tblPr"));
        const bool styled = tblPr && tblPr->child(QStringLiteral("tableStyleId"));
        o.fontSize = 18 * u.pt;
        o.verticalAlign = 0;
        o.table.padding = 3.6 * u.pt;
        // Cells are see-through unless they say otherwise; a named table style
        // gives the header its colour and the rows their bands.
        o.fill = QColor(0, 0, 0, 0);
        o.fillStyle = 0;
        o.fillToken.clear(); o.textColorToken = QStringLiteral("foreground"); o.fontToken = QStringLiteral("body");
        o.table.borderWidth = styled ? 0.75 * u.pt : 0;
        bool explicitFills = false;

        TextChain chain;
        chain.levels.append(defaultTextStyle());
        if (scope.master->root)
            if (const Node *styles = scope.master->root->child(QStringLiteral("txStyles")))
                chain.levels.append(styles->child(QStringLiteral("otherStyle")));
        const auto ctx = textContext(scope);
        QSet<int> covered;
        for (int r = 0; r < rows; ++r) {
            const auto cells = rowsNodes[r]->all(QStringLiteral("tc"));
            for (int c = 0; c < qMin(columns, cells.size()); ++c) {
                const Node *tc = cells[c];
                auto &cell = o.table.cells[r * columns + c];
                const int gridSpan = qMax(1, tc->attr(QStringLiteral("gridSpan"), QStringLiteral("1")).toInt());
                const int rowSpan = qMax(1, tc->attr(QStringLiteral("rowSpan"), QStringLiteral("1")).toInt());
                const bool hidden = tc->attr(QStringLiteral("hMerge")) == QLatin1String("1") || tc->attr(QStringLiteral("vMerge")) == QLatin1String("1");
                if (hidden) { cell.rowSpan = 0; cell.columnSpan = 0; continue; }
                cell.rowSpan = qMin(rowSpan, rows - r);
                cell.columnSpan = qMin(gridSpan, columns - c);
                for (int rr = r; rr < r + cell.rowSpan; ++rr)
                    for (int cc = c; cc < c + cell.columnSpan; ++cc)
                        if (rr != r || cc != c) { auto &under = o.table.cells[rr * columns + cc]; under.rowSpan = 0; under.columnSpan = 0; }
                const TextOut text = convertText(tc->child(QStringLiteral("txBody")), chain, ctx);
                cell.text = text.text;
                const auto &b = text.box;
                if (b.sz > 0) cell.style.insert(QStringLiteral("fontSize"), b.sz / 100.0 * u.pt);
                if (b.b > 0) cell.style.insert(QStringLiteral("fontWeight"), 700);
                if (b.i > 0) cell.style.insert(QStringLiteral("italic"), true);
                if (b.u > 0) cell.style.insert(QStringLiteral("underline"), true);
                if (b.color.isValid()) cell.style.insert(QStringLiteral("textColor"), b.color.name(QColor::HexArgb));
                if (!b.family.isEmpty()) cell.style.insert(QStringLiteral("fontFamily"), b.family);
                if (text.para.algn >= 0) cell.style.insert(QStringLiteral("textAlign"), text.para.algn);
                if (const Node *tcPr = tc->child(QStringLiteral("tcPr"))) {
                    Look look;
                    readFill(tcPr, look, scope.master->color);
                    if (look.fillSet) {
                        explicitFills = true;
                        cell.style.insert(QStringLiteral("fill"), look.fillStyle != 5 && look.fill.isValid() ? look.fill.name(QColor::HexArgb) : QStringLiteral("#00000000"));
                    }
                    const auto anchor = tcPr->attr(QStringLiteral("anchor"));
                    if (anchor == QLatin1String("ctr")) cell.style.insert(QStringLiteral("verticalAlign"), 1);
                    else if (anchor == QLatin1String("b")) cell.style.insert(QStringLiteral("verticalAlign"), 2);
                    // One width and colour per cell: the widest of its four edges.
                    bool anyLine = false; qreal widest = 0; QColor lineColor;
                    for (const auto *side : {"lnL", "lnR", "lnT", "lnB"}) {
                        const Node *ln = tcPr->child(QString::fromLatin1(side));
                        if (!ln) continue;
                        anyLine = true;
                        Look edge; readLine(ln, edge, scope.master->color);
                        if (edge.line && edge.lineWidthEmu * u.k > widest) { widest = edge.lineWidthEmu * u.k; lineColor = edge.lineColor; }
                    }
                    if (anyLine) {
                        cell.style.insert(QStringLiteral("borderWidth"), widest > 0 ? qMax(0.5, widest) : 0.0);
                        if (lineColor.isValid()) cell.style.insert(QStringLiteral("borderColor"), lineColor.name(QColor::HexArgb));
                    }
                }
            }
        }
        const bool header = styled && !explicitFills && tblPr->attr(QStringLiteral("firstRow")) == QLatin1String("1");
        o.table.headerRows = header ? 1 : 0;
        o.table.headerColumns = styled && !explicitFills && tblPr->attr(QStringLiteral("firstCol")) == QLatin1String("1") ? 1 : 0;
        o.table.banded = styled && !explicitFills && tblPr->attr(QStringLiteral("bandRow")) == QLatin1String("1");
        if (!name.isEmpty()) o.altTitle = name;
        scope.objects->append(o);
        if (scope.spidToObject && !spid.isEmpty()) scope.spidToObject->insert(spid, o.id);
    }

    // ---- charts

    static QStringList cachePoints(const Node *ref, int *count) {
        // strRef/numRef → strCache/numCache → pt idx / v
        QStringList out;
        if (!ref) return out;
        const Node *cache = nullptr;
        for (const auto &k : ref->kids) {
            if (k.name == QLatin1String("strRef") || k.name == QLatin1String("numRef")) {
                cache = k.child(QStringLiteral("strCache"));
                if (!cache) cache = k.child(QStringLiteral("numCache"));
            } else if (k.name == QLatin1String("strLit") || k.name == QLatin1String("numLit")) cache = &k;
        }
        if (!cache) return out;
        QMap<int, QString> points;
        int maxIdx = -1;
        for (const auto *pt : cache->all(QStringLiteral("pt"))) {
            const int idx = pt->attr(QStringLiteral("idx")).toInt();
            const Node *v = pt->child(QStringLiteral("v"));
            points.insert(idx, v ? v->text : QString());
            maxIdx = qMax(maxIdx, idx);
        }
        if (const Node *n = cache->child(QStringLiteral("ptCount"))) maxIdx = qMax(maxIdx, n->attr(QStringLiteral("val")).toInt() - 1);
        maxIdx = qMin(maxIdx, Table::maxRows * 4);   // a count is a claim, not a budget
        for (int i = 0; i <= maxIdx; ++i) out.append(points.value(i));
        if (count) *count = maxIdx + 1;
        return out;
    }

    void convertChart(const QString &chartPath, const Placement &place, Scope &scope, const QStringList &groups, const QString &spid, const QString &name) {
        const Node *root = pkg.part(chartPath);
        const Node *chart = root ? root->child(QStringLiteral("chart")) : nullptr;
        const Node *plot = chart ? chart->child(QStringLiteral("plotArea")) : nullptr;
        if (!plot) { warnings.add(QStringLiteral("A chart could not be read"), scope.slideNumber); return; }
        const Node *kindNode = nullptr;
        for (const auto &k : plot->kids) if (k.name.endsWith(QLatin1String("Chart"))) { kindNode = &k; break; }
        if (!kindNode) { warnings.add(QStringLiteral("A chart of an unknown kind was left out"), scope.slideNumber); return; }
        const auto kindName = kindNode->name;
        const auto grouping = kindNode->child(QStringLiteral("grouping")) ? kindNode->child(QStringLiteral("grouping"))->attr(QStringLiteral("val")) : QString();
        const bool stacked = grouping.contains(QLatin1String("tacked"));
        const bool horizontal = kindNode->child(QStringLiteral("barDir")) && kindNode->child(QStringLiteral("barDir"))->attr(QStringLiteral("val")) == QLatin1String("bar");
        int kind = -1;
        if (kindName.startsWith(QLatin1String("bar"))) kind = horizontal ? (stacked ? 3 : 1) : (stacked ? 2 : 0);
        else if (kindName.startsWith(QLatin1String("line")) || kindName == QLatin1String("stockChart")) kind = 4;
        else if (kindName.startsWith(QLatin1String("area"))) kind = stacked ? 6 : 5;
        else if (kindName.startsWith(QLatin1String("pie")) || kindName == QLatin1String("ofPieChart")) kind = 7;
        else if (kindName.startsWith(QLatin1String("doughnut"))) kind = 8;
        else if (kindName.startsWith(QLatin1String("scatter")) || kindName.startsWith(QLatin1String("bubble"))) kind = 9;
        else if (kindName.startsWith(QLatin1String("radar")) || kindName.startsWith(QLatin1String("surface"))) {
            kind = 4;
            warnings.add(QStringLiteral("A %1 was drawn as a line chart").arg(kindName), scope.slideNumber);
        } else { warnings.add(QStringLiteral("A %1 was left out").arg(kindName), scope.slideNumber); return; }

        struct Series { QString name; QStringList cats, vals, xs; QColor color; };
        QVector<Series> series;
        QVector<const Node *> serNodes;
        int kinds = 0;
        for (const auto &k : plot->kids) {
            if (!k.name.endsWith(QLatin1String("Chart"))) continue;
            ++kinds;
            for (const auto *ser : k.all(QStringLiteral("ser"))) serNodes.append(ser);
        }
        if (kinds > 1) warnings.add(QStringLiteral("A chart combining kinds was drawn as one kind"), scope.slideNumber);
        bool anyLabels = false;
        for (const auto *ser : serNodes) {
            if (const Node *labels = ser->child(QStringLiteral("dLbls"))) {
                const Node *show = labels->child(QStringLiteral("showVal"));
                if (show && show->attr(QStringLiteral("val")) == QLatin1String("1")) anyLabels = true;
            }
            Series s;
            if (const Node *tx = ser->child(QStringLiteral("tx"))) { const auto names = cachePoints(tx, nullptr); s.name = names.value(0); }
            if (s.name.isEmpty()) s.name = QStringLiteral("Series %1").arg(series.size() + 1);
            s.cats = cachePoints(ser->child(QStringLiteral("cat")), nullptr);
            s.vals = cachePoints(ser->child(QStringLiteral("val")), nullptr);
            if (kind == 9) { s.xs = cachePoints(ser->child(QStringLiteral("xVal")), nullptr); s.vals = cachePoints(ser->child(QStringLiteral("yVal")), nullptr); }
            if (const Node *spPr = ser->child(QStringLiteral("spPr"))) {
                Look look; readFill(spPr, look, scope.master->color);
                if (look.fill.isValid()) s.color = look.fill;
                else if (const Node *ln = spPr->child(QStringLiteral("ln"))) { Look line; readFill(ln, line, scope.master->color); if (line.fill.isValid()) s.color = line.fill; }
            }
            series.append(s);
        }
        if (series.isEmpty()) { warnings.add(QStringLiteral("A chart with no data was left out"), scope.slideNumber); return; }
        int longest = 0;
        for (const auto &s : series) longest = qMax(longest, kind == 9 ? s.xs.size() : s.vals.size());
        if (longest < 1) { warnings.add(QStringLiteral("A chart whose data was not cached in the deck was left out"), scope.slideNumber); return; }
        if (longest > Table::maxRows - 1 || series.size() > Table::maxColumns - 1)
            warnings.add(QStringLiteral("A chart was cut to %1 points and %2 series").arg(Table::maxRows - 1).arg(Table::maxColumns - 1), scope.slideNumber);

        SceneObject o;
        o.id = Edit::newId(QStringLiteral("chart"));
        o.type = ObjectType::Chart;
        o.chart.kind = kind;
        finish(o, place, groups);
        o.fontSize = 14 * u.pt;
        o.fillToken = QStringLiteral("background"); o.textColorToken = QStringLiteral("foreground"); o.fontToken = QStringLiteral("body");

        if (kind == 9) {
            // Rows are the x values every series shares; blanks where one has none.
            QStringList xs;
            bool repeated = false;
            for (const auto &s : series) { QSet<QString> seen; for (const auto &x : s.xs) { if (seen.contains(x)) repeated = true; seen.insert(x); if (!xs.contains(x)) xs.append(x); } }
            if (repeated) warnings.add(QStringLiteral("A scatter chart repeats an x value; only the first point at each x was kept"), scope.slideNumber);
            std::sort(xs.begin(), xs.end(), [](const QString &a, const QString &b) { return a.toDouble() < b.toDouble(); });
            const int rows = qMin(Table::maxRows, xs.size() + 1), columns = qMin(Table::maxColumns, series.size() + 1);
            o.table = Table::create(rows, columns);
            o.table.headerColumns = 1;
            o.table.cells[0].text = QStringLiteral("X");
            for (int c = 1; c < columns; ++c) o.table.cells[c].text = series[c - 1].name;
            for (int r = 1; r < rows; ++r) {
                o.table.cells[r * columns].text = xs[r - 1];
                for (int c = 1; c < columns; ++c) {
                    const auto &s = series[c - 1];
                    const int at = s.xs.indexOf(xs[r - 1]);
                    if (at >= 0) o.table.cells[r * columns + c].text = s.vals.value(at);
                }
            }
        } else {
            QStringList cats = series.first().cats;
            int points = 0;
            for (const auto &s : series) points = qMax(points, s.vals.size());
            while (cats.size() < points) cats.append(QStringLiteral("%1").arg(cats.size() + 1));
            const int rows = qMin(Table::maxRows, points + 1), columns = qMin(Table::maxColumns, series.size() + 1);
            o.table = Table::create(rows, columns);
            o.table.headerColumns = 1;
            o.table.cells[0].text = QStringLiteral("Category");
            for (int c = 1; c < columns; ++c) o.table.cells[c].text = series[c - 1].name;
            for (int r = 1; r < rows; ++r) {
                o.table.cells[r * columns].text = cats.value(r - 1);
                for (int c = 1; c < columns; ++c) o.table.cells[r * columns + c].text = series[c - 1].vals.value(r - 1);
            }
        }
        const int columns = o.table.columns.size();
        for (int c = 1; c < columns; ++c)
            if (series[c - 1].color.isValid()) o.chart.seriesColors.insert(o.table.cells[c].id, series[c - 1].color);

        const Node *title = chart->child(QStringLiteral("title"));
        const Node *deleted = chart->child(QStringLiteral("autoTitleDeleted"));
        o.chart.title = title ? title->words().trimmed() : QString();
        if (o.chart.title.isEmpty() && title && !(deleted && deleted->attr(QStringLiteral("val")) == QLatin1String("1")) && series.size() == 1) o.chart.title = series.first().name;
        o.chart.legend = chart->child(QStringLiteral("legend")) != nullptr;
        o.chart.labels = anyLabels;
        if (const Node *labels = kindNode->child(QStringLiteral("dLbls"))) {
            const Node *show = labels->child(QStringLiteral("showVal"));
            if (show && show->attr(QStringLiteral("val")) == QLatin1String("1")) o.chart.labels = true;
        }
        for (const auto *axis : plot->all(QStringLiteral("catAx")))
            if (const Node *t = axis->child(QStringLiteral("title"))) o.chart.xTitle = t->words().trimmed();
        for (const auto *axis : plot->all(QStringLiteral("valAx"))) {
            if (const Node *t = axis->child(QStringLiteral("title"))) {
                const bool vertical = !axis->child(QStringLiteral("axPos")) || axis->child(QStringLiteral("axPos"))->attr(QStringLiteral("val")) == QLatin1String("l") || axis->child(QStringLiteral("axPos"))->attr(QStringLiteral("val")) == QLatin1String("r");
                (vertical ? o.chart.yTitle : o.chart.xTitle) = t->words().trimmed();
            }
            if (const Node *grid = axis->child(QStringLiteral("majorGridlines"))) { Q_UNUSED(grid); o.chart.grid = true; }
            else o.chart.grid = false;
        }
        if (!name.isEmpty()) o.altTitle = name;
        scope.objects->append(o);
        if (scope.spidToObject && !spid.isEmpty()) scope.spidToObject->insert(spid, o.id);
    }

    void convertFrame(const Node *frame, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        const Placement place = placementOf(frame->child(QStringLiteral("xfrm")), u, space);
        if (!place.valid) return;
        const Node *nv = frame->child(QStringLiteral("nvGraphicFramePr"));
        const Node *cNvPr = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
        const QString spid = cNvPr ? cNvPr->attr(QStringLiteral("id")) : QString();
        const QString name = cNvPr ? cNvPr->attr(QStringLiteral("name")) : QString();
        const Node *data = frame->path({"graphic", "graphicData"});
        if (!data) return;
        const auto uri = data->attr(QStringLiteral("uri"));
        if (uri.endsWith(QLatin1String("/table"))) {
            if (const Node *tbl = data->child(QStringLiteral("tbl"))) convertTable(tbl, place, scope, groups, spid, name);
        } else if (uri.endsWith(QLatin1String("/chart"))) {
            const Node *chart = data->child(QStringLiteral("chart"));
            const auto path = chart ? pkg.target(scope.partPath, chart->attr(QStringLiteral("r:id"))) : QString();
            if (path.isEmpty()) warnings.add(QStringLiteral("A chart's data was missing from the deck"), scope.slideNumber);
            else convertChart(path, place, scope, groups, spid, name);
        } else if (uri.contains(QLatin1String("/diagram"))) {
            warnings.add(QStringLiteral("A SmartArt graphic was left out"), scope.slideNumber);
        } else if (uri.contains(QLatin1String("/ole"))) {
            warnings.add(QStringLiteral("An embedded object was left out"), scope.slideNumber);
        } else {
            warnings.add(QStringLiteral("A %1 frame was left out").arg(uri.section(QLatin1Char('/'), -1)), scope.slideNumber);
        }
    }

    void convertConnector(const Node *cxn, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        const Node *spPr = cxn->child(QStringLiteral("spPr"));
        Placement place = placementOf(spPr ? spPr->child(QStringLiteral("xfrm")) : nullptr, u, space);
        if (!place.valid) return;
        Look look;
        readFill(spPr, look, scope.master->color);
        if (spPr) readLine(spPr->child(QStringLiteral("ln")), look, scope.master->color);
        applyStyleRefs(cxn->child(QStringLiteral("style")), look, scope.master->color);
        if (!look.line) return;
        SceneObject o;
        o.id = Edit::newId(QStringLiteral("shape"));
        o.type = ObjectType::Rect;
        if (place.rect.height() < 1) place.rect.setHeight(1);
        if (place.rect.width() < 1) place.rect.setWidth(1);
        finish(o, place, groups);
        look.fillSet = true; look.fillStyle = 5;
        applyLook(o, look);
        o.shapeKind = 13;
        const Node *prst = spPr ? spPr->child(QStringLiteral("prstGeom")) : nullptr;
        const auto kind = prst ? prst->attr(QStringLiteral("prst")) : QString();
        if (!kind.isEmpty() && !kind.startsWith(QLatin1String("straight")) && kind != QLatin1String("line"))
            warnings.add(QStringLiteral("A %1 connector was drawn straight").arg(kind), scope.slideNumber);
        if (place.rect.height() > 2 && place.rect.width() > 2) {
            QPainterPath path;
            if (place.flipV != place.flipH) { path.moveTo(place.rect.bottomLeft()); path.lineTo(place.rect.topRight()); }
            else { path.moveTo(place.rect.topLeft()); path.lineTo(place.rect.bottomRight()); }
            const qreal rot = o.rotation; const auto keep = o.rect;
            Shape::assignPath(o, path); o.rotation = rot; o.rect = keep;
        } else if (place.rect.width() <= 2) {
            // Vertical: turn the horizontal line.
            const QPointF c = place.rect.center();
            const qreal len = place.rect.height();
            o.rect = QRectF(c.x() - len / 2, c.y() - 0.5, len, 1);
            o.rotation += 90;
        }
        scope.objects->append(o);
        const Node *nv = cxn->child(QStringLiteral("nvCxnSpPr"));
        const Node *cNvPr = nv ? nv->child(QStringLiteral("cNvPr")) : nullptr;
        if (scope.spidToObject && cNvPr) scope.spidToObject->insert(cNvPr->attr(QStringLiteral("id")), o.id);
    }

    void convertGroup(const Node *grp, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        const Node *pr = grp->child(QStringLiteral("grpSpPr"));
        const Node *xfrm = pr ? pr->child(QStringLiteral("xfrm")) : nullptr;
        GroupSpace inner = space;
        if (xfrm) {
            const Node *off = xfrm->child(QStringLiteral("off")), *ext = xfrm->child(QStringLiteral("ext"));
            const Node *chOff = xfrm->child(QStringLiteral("chOff")), *chExt = xfrm->child(QStringLiteral("chExt"));
            if (off && ext && chOff && chExt) {
                const qreal cw = chExt->attr(QStringLiteral("cx")).toDouble(), ch = chExt->attr(QStringLiteral("cy")).toDouble();
                const qreal sx = cw > 0 ? ext->attr(QStringLiteral("cx")).toDouble() / cw : 1;
                const qreal sy = ch > 0 ? ext->attr(QStringLiteral("cy")).toDouble() / ch : 1;
                QTransform local;
                local.translate(off->attr(QStringLiteral("x")).toDouble() * u.k, off->attr(QStringLiteral("y")).toDouble() * u.k);
                local.scale(sx, sy);
                local.translate(-chOff->attr(QStringLiteral("x")).toDouble() * u.k, -chOff->attr(QStringLiteral("y")).toDouble() * u.k);
                const qreal rot = xfrm->attr(QStringLiteral("rot"), QStringLiteral("0")).toDouble() / 60000.0;
                if (!qFuzzyIsNull(rot)) {
                    const QRectF frame = xfrmRect(xfrm, u);
                    QTransform spin;
                    spin.translate(frame.center().x(), frame.center().y());
                    spin.rotate(rot);
                    spin.translate(-frame.center().x(), -frame.center().y());
                    local = local * spin;
                    inner.rotation += rot;
                }
                inner.points = local * space.points;
                inner.sx *= sx; inner.sy *= sy;
            }
        }
        QStringList own = groups;
        own.append(Edit::newId(QStringLiteral("group")));
        convertChildren(grp, scope, inner, own);
    }

    void convertChildren(const Node *tree, Scope &scope, const GroupSpace &space, const QStringList &groups) {
        QVector<const Node *> kids;
        flattened(*tree, kids);
        for (const auto *k : kids) {
            if (k->name == QLatin1String("sp")) convertShape(k, scope, space, groups);
            else if (k->name == QLatin1String("pic")) convertPicture(k, scope, space, groups);
            else if (k->name == QLatin1String("grpSp")) convertGroup(k, scope, space, groups);
            else if (k->name == QLatin1String("graphicFrame")) convertFrame(k, scope, space, groups);
            else if (k->name == QLatin1String("cxnSp")) convertConnector(k, scope, space, groups);
            else if (k->name == QLatin1String("contentPart")) warnings.add(QStringLiteral("Ink or another drawing part was left out"), scope.slideNumber);
        }
    }

    // Shapes of a layout or master that are decoration, not placeholders.
    void convertDecoration(const Node *root, Scope &scope) {
        const Node *tree = spTreeOf(root);
        if (!tree) return;
        QVector<const Node *> kids;
        flattened(*tree, kids);
        for (const auto *k : kids) {
            if (k->name == QLatin1String("sp") || k->name == QLatin1String("pic") || k->name == QLatin1String("graphicFrame"))
                if (phOf(k).is) continue;
            if (k->name == QLatin1String("sp")) convertShape(k, scope, GroupSpace(), {});
            else if (k->name == QLatin1String("pic")) convertPicture(k, scope, GroupSpace(), {});
            else if (k->name == QLatin1String("grpSp")) convertGroup(k, scope, GroupSpace(), {});
            else if (k->name == QLatin1String("graphicFrame")) convertFrame(k, scope, GroupSpace(), {});
            else if (k->name == QLatin1String("cxnSp")) convertConnector(k, scope, GroupSpace(), {});
        }
    }

    // ---- backgrounds

    // The background a cSld names, or invalid when it names none. A picture
    // background comes back as its part path.
    QColor backgroundOf(const Node *root, const ColorContext &color, QString *picturePath, const QString &partPath) const {
        const Node *bg = root ? root->path({"cSld", "bg"}) : nullptr;
        if (!bg) return QColor();
        if (const Node *pr = bg->child(QStringLiteral("bgPr"))) {
            Look look;
            readFill(pr, look, color);
            if (!look.blipRid.isEmpty() && picturePath) *picturePath = pkg.target(partPath, look.blipRid);
            if (look.fillSet && look.fillStyle != 5 && look.fill.isValid()) return look.fill;
            if (!look.blipRid.isEmpty()) return QColor();
            return look.fillSet ? QColor(Qt::white) : QColor();
        }
        if (const Node *ref = bg->child(QStringLiteral("bgRef"))) {
            const auto c = colorIn(ref, color);
            if (c.isValid()) return c;
        }
        return QColor();
    }

    // ---- masters and layouts

    MasterInfo &masterFor(const QString &masterPath) {
        auto found = masters.find(masterPath);
        if (found != masters.end()) return found->second;
        MasterInfo info;
        info.path = masterPath;
        info.root = pkg.part(masterPath);
        const auto themePath = pkg.related(masterPath, QStringLiteral("theme"));
        info.theme = readTheme(pkg, themePath.isEmpty() ? pkg.related(presentationPath, QStringLiteral("theme")) : themePath);
        if (info.root)
            if (const Node *map = info.root->child(QStringLiteral("clrMap")))
                for (auto a = map->attrs.cbegin(); a != map->attrs.cend(); ++a) info.color.clrMap.insert(a.key(), a.value());
        MasterInfo &it = masters.emplace(masterPath, info).first->second;
        it.color.theme = &it.theme;

        Master master;
        master.id = Edit::newId(QStringLiteral("master"));
        const Node *cSld = info.root ? info.root->child(QStringLiteral("cSld")) : nullptr;
        master.name = cSld ? cSld->attr(QStringLiteral("name")) : QString();
        if (master.name.isEmpty()) master.name = QStringLiteral("Master %1").arg(doc.masters.size() + 1);
        QString picture;
        const auto bg = backgroundOf(info.root, it.color, &picture, masterPath);
        master.background = bg.isValid() ? bg : it.theme.colors.value(it.color.clrMap.value(QStringLiteral("bg1"), QStringLiteral("lt1")), QColor(Qt::white));
        master.backgroundToken.clear();
        it.id = master.id;
        Scope scope;
        scope.partPath = masterPath;
        scope.master = &it;
        scope.objects = &master.objects;
        if (!picture.isEmpty()) {
            SceneObject o;
            o.id = Edit::newId(QStringLiteral("image"));
            o.type = ObjectType::Image;
            if (decodePicture(o, picture, 0)) { o.rect = QRectF(QPointF(0, 0), doc.size); o.imageMode = 1; o.fillStyle = 5; master.objects.append(o); }
        }
        convertDecoration(info.root, scope);
        if (presentation && presentation->attr(QStringLiteral("showSpecialPlsOnTitleSld")) == QLatin1String("0")) master.fields.hideOnFirst = true;
        doc.masters.append(master);
        return it;
    }

    QString layoutFor(const QString &layoutPath, MasterInfo &master) {
        auto it = layoutIds.find(layoutPath);
        if (it != layoutIds.end()) return *it;
        const Node *root = pkg.part(layoutPath);
        SlideLayout layout;
        layout.id = Edit::newId(QStringLiteral("layout"));
        layout.masterId = master.id;
        const Node *cSld = root ? root->child(QStringLiteral("cSld")) : nullptr;
        layout.name = cSld ? cSld->attr(QStringLiteral("name")) : QString();
        if (layout.name.isEmpty()) layout.name = QStringLiteral("Layout %1").arg(doc.layouts.size() + 1);
        const Node *tree = spTreeOf(root);
        QVector<const Node *> kids;
        if (tree) flattened(*tree, kids);
        int bodies = 0;
        Scope scope;
        scope.partPath = layoutPath;
        scope.layout = root;
        scope.master = &master;
        for (const auto *sp : kids) {
            if (sp->name != QLatin1String("sp")) continue;
            const Ph ph = phOf(sp);
            if (!ph.is) continue;
            const auto cls = phClass(ph.type);
            if (cls != QLatin1String("title") && cls != QLatin1String("body")) continue;
            const Node *masterSp = findPh(master.root, ph, false);
            const Node *spPr = sp->child(QStringLiteral("spPr"));
            Placement place = placementOf(spPr ? spPr->child(QStringLiteral("xfrm")) : nullptr, u, GroupSpace());
            if (!place.valid && masterSp) {
                const Node *pr = masterSp->child(QStringLiteral("spPr"));
                place = placementOf(pr ? pr->child(QStringLiteral("xfrm")) : nullptr, u, GroupSpace());
            }
            if (!place.valid) continue;
            SceneObject o;
            o.id = cls == QLatin1String("title") ? QStringLiteral("title")
                 : bodies == 0 ? QStringLiteral("body") : QStringLiteral("body-%1").arg(bodies + 1);
            if (cls == QLatin1String("body")) ++bodies;
            bool taken = false;
            for (const auto &p : layout.placeholders) if (p.id == o.id) taken = true;
            if (taken) continue;
            TextChain chain = chainFor(scope, ph, nullptr, masterSp);
            const TextOut text = convertText(sp->child(QStringLiteral("txBody")), chain, textContext(scope));
            RunLook fallback; { ParaLook para; resolveLevel(chain, 0, para, fallback, textContext(scope)); }
            finish(o, place, {});
            applyText(o, text, fallback);
            auto prompt = text.text.trimmed();
            if (prompt.isEmpty() || prompt.startsWith(QLatin1String("Click to edit")) || prompt.startsWith(QLatin1String("Click to add")))
                prompt = cls == QLatin1String("title") ? QStringLiteral("Title") : ph.type == QLatin1String("subTitle") ? QStringLiteral("Subtitle") : QStringLiteral("Body");
            o.text = prompt;
            o.runs.clear();
            o.placeholderId.clear();
            layout.placeholders.append(o);
        }
        doc.layouts.append(layout);
        return *layoutIds.insert(layoutPath, layout.id);
    }

    // ---- transitions and builds

    void convertTransition(const Node *slideRoot, Slide &slide, int number) {
        const Node *transition = nullptr, *newerNode = nullptr;
        QString newer;
        const auto isEffect = [](const Node &k) { return k.name != QLatin1String("sndAc") && k.name != QLatin1String("extLst"); };
        for (const auto &k : slideRoot->kids) {
            if (k.name == QLatin1String("transition")) transition = &k;
            else if (k.name == QLatin1String("AlternateContent")) {
                if (const Node *choice = k.child(QStringLiteral("Choice")))
                    if (const Node *t = choice->child(QStringLiteral("transition"))) {
                        for (const auto &effect : t->kids) if (isEffect(effect)) newer = effect.name;
                        newerNode = t;
                        if (!transition) transition = t;
                    }
                if (const Node *fallback = k.child(QStringLiteral("Fallback")))
                    if (const Node *t = fallback->child(QStringLiteral("transition"))) transition = t;
            }
        }
        if (!transition) { slide.transition = 0; return; }
        QString effect = newer;
        if (effect.isEmpty()) for (const auto &k : transition->kids) if (isEffect(k)) effect = k.name;
        const Node *effectNode = nullptr;
        for (const auto &k : transition->kids) if (k.name == effect) effectNode = &k;
        if (effect.isEmpty() || effect == QLatin1String("cut")) slide.transition = 0;
        else if (effect == QLatin1String("fade")) slide.transition = 1;
        else if (effect == QLatin1String("push") || effect == QLatin1String("cover") || effect == QLatin1String("pull") || effect == QLatin1String("wipe")) {
            slide.transition = 2;
            const auto dir = effectNode ? effectNode->attr(QStringLiteral("dir"), QStringLiteral("l")) : QStringLiteral("l");
            slide.transitionDirection = dir == QLatin1String("r") ? 1 : dir == QLatin1String("u") ? 2 : dir == QLatin1String("d") ? 3 : 0;
            if (effect != QLatin1String("push")) warnings.add(QStringLiteral("The %1 transition was shown as a push").arg(effect), number);
        } else if (effect == QLatin1String("morph")) slide.transition = 3;
        else { slide.transition = 1; warnings.add(QStringLiteral("The %1 transition was shown as a fade").arg(effect), number); }
        const auto speed = transition->attr(QStringLiteral("spd"), QStringLiteral("fast"));
        slide.transitionSeconds = speed == QLatin1String("slow") ? 1.0 : speed == QLatin1String("med") ? 0.75 : 0.5;
        // The newer markup carries an exact duration the older one cannot.
        for (const auto *t : {transition, newerNode})
            if (t && t->has(QStringLiteral("dur"))) slide.transitionSeconds = qBound(0.05, t->attr(QStringLiteral("dur")).toDouble() / 1000.0, 30.0);
        if (transition->has(QStringLiteral("advTm"))) slide.advanceAfter = transition->attr(QStringLiteral("advTm")).toDouble() / 1000.0;
    }

    void convertBuilds(const Node *slideRoot, Slide &slide, const QHash<QString, QString> &spidToObject, int number) {
        const Node *timing = slideRoot->child(QStringLiteral("timing"));
        if (!timing) return;
        QVector<const Node *> nodes;
        timing->collect(QStringLiteral("cTn"), nodes);
        QSet<QString> done;
        int approximated = 0;
        for (const auto *cTn : nodes) {
            const auto cls = cTn->attr(QStringLiteral("presetClass"));
            if (cls.isEmpty()) continue;
            QVector<const Node *> targets;
            cTn->collect(QStringLiteral("spTgt"), targets);
            if (targets.isEmpty()) continue;
            const auto spid = targets.first()->attr(QStringLiteral("spid"));
            const auto objectId = spidToObject.value(spid);
            if (objectId.isEmpty()) continue;
            const bool paragraphs = targets.first()->child(QStringLiteral("txEl")) != nullptr;
            const QString key = objectId + QLatin1Char('/') + cls;
            if (done.contains(key)) continue;
            done.insert(key);
            BuildStep step;
            step.targetId = objectId;
            const int preset = cTn->attr(QStringLiteral("presetID")).toInt();
            if (cls == QLatin1String("entr") || cls == QLatin1String("exit")) {
                step.phase = cls == QLatin1String("entr") ? BuildPhase::In : BuildPhase::Out;
                if (paragraphs && step.phase == BuildPhase::In) { step.effect = Effect::Reveal; step.unit = 0; }
                else if (preset == 1) { step.effect = Effect::Fade; step.duration = 0.05; }
                else if (preset == 10) step.effect = Effect::Fade;
                else if (preset == 2 || preset == 4 || preset == 12 || preset == 13 || preset == 22 || preset == 3 || preset == 42 || preset == 47 || preset == 52) step.effect = Effect::Rise;
                else if (preset == 23 || preset == 53 || preset == 24 || preset == 15) step.effect = Effect::Scale;
                else if (preset == 49 || preset == 21) step.effect = Effect::Spin;
                else { step.effect = Effect::Fade; ++approximated; }
            } else if (cls == QLatin1String("emph")) {
                step.effect = Effect::Pulse;
            } else continue;
            const auto trigger = cTn->attr(QStringLiteral("nodeType"));
            step.trigger = trigger == QLatin1String("withEffect") ? BuildTrigger::WithPrevious
                         : trigger == QLatin1String("afterEffect") ? BuildTrigger::AfterPrevious
                         : BuildTrigger::OnClick;
            qreal longest = 0;
            QVector<const Node *> inner;
            cTn->collect(QStringLiteral("cTn"), inner);
            for (const auto *n : inner) {
                bool ok = false;
                const qreal ms = n->attr(QStringLiteral("dur")).toDouble(&ok);
                if (ok) longest = qMax(longest, ms);
            }
            if (longest > 0 && step.effect != Effect::Fade) step.duration = qBound(0.1, longest / 1000.0, 10.0);
            else if (longest > 0 && step.duration > 0.05) step.duration = qBound(0.1, longest / 1000.0, 10.0);
            QVector<const Node *> conditions;
            if (const Node *st = cTn->child(QStringLiteral("stCondLst"))) st->collect(QStringLiteral("cond"), conditions);
            for (const auto *cond : conditions) {
                bool ok = false;
                const qreal delay = cond->attr(QStringLiteral("delay")).toDouble(&ok);
                if (ok && delay > 0) step.delay = delay / 1000.0;
            }
            slide.timeline.steps.append(step);
        }
        if (approximated) warnings.add(QStringLiteral("An animation effect OmaShow does not have was shown as a fade"), number);
    }

    // ---- slides

    void convertSlide(const QString &slidePath, int number, const QString &sectionId) {
        const Node *root = pkg.part(slidePath);
        if (!root) { warnings.add(QStringLiteral("A slide could not be read"), number); return; }
        const auto layoutPath = pkg.related(slidePath, QStringLiteral("slideLayout"));
        auto masterPath = layoutPath.isEmpty() ? QString() : pkg.related(layoutPath, QStringLiteral("slideMaster"));
        if (masterPath.isEmpty()) masterPath = pkg.relatedAll(presentationPath, QStringLiteral("slideMaster")).value(0);
        MasterInfo &master = masterFor(masterPath);
        const Node *layoutRoot = pkg.part(layoutPath);

        Slide slide;
        slide.id = slideIdByPath.value(slidePath);
        slide.sectionId = sectionId;
        slide.layoutId = layoutPath.isEmpty() ? QString() : layoutFor(layoutPath, master);
        slide.skipped = root->attr(QStringLiteral("show")) == QLatin1String("0");
        slide.showMasterObjects = root->attr(QStringLiteral("showMasterSp")) != QLatin1String("0")
                                  && (!layoutRoot || layoutRoot->attr(QStringLiteral("showMasterSp")) != QLatin1String("0"));

        // Background: the slide's, else the layout's, else the master's.
        QString picture;
        QColor bg = backgroundOf(root, master.color, &picture, slidePath);
        if (!bg.isValid() && picture.isEmpty() && layoutRoot) bg = backgroundOf(layoutRoot, master.color, &picture, layoutPath);
        if (bg.isValid()) { slide.background = bg; slide.backgroundOverride = true; }
        else if (!picture.isEmpty()) {
            slide.background = QColor(Qt::white);
            for (const auto &m : doc.masters) if (m.id == master.id) slide.background = m.background;
            slide.backgroundOverride = true;
        }

        QHash<QString, QString> spidToObject;
        Scope scope;
        scope.partPath = slidePath;
        scope.slide = root;
        scope.layout = layoutRoot;
        scope.master = &master;
        scope.slideNumber = number;
        scope.slideId = slide.id;
        scope.objects = &slide.objects;
        scope.spidToObject = &spidToObject;
        Master *masterRecord = nullptr;
        for (auto &m : doc.masters) if (m.id == master.id) masterRecord = &m;
        MasterFields fields = masterRecord ? masterRecord->fields : MasterFields();
        scope.fields = &fields;

        if (!picture.isEmpty()) {
            SceneObject o;
            o.id = Edit::newId(QStringLiteral("image"));
            o.type = ObjectType::Image;
            if (decodePicture(o, picture, number)) { o.rect = QRectF(QPointF(0, 0), doc.size); o.imageMode = 1; o.fillStyle = 5; o.locked = true; slide.objects.append(o); }
        }
        // The layout's own decoration belongs to every slide that uses it.
        if (layoutRoot && slide.showMasterObjects) {
            Scope decoration = scope;
            decoration.partPath = layoutPath;
            decoration.slide = nullptr;
            decoration.spidToObject = nullptr;
            decoration.fields = nullptr;
            convertDecoration(layoutRoot, decoration);
            for (auto &o : slide.objects) if (o.groups.isEmpty() && !o.locked && o.type != ObjectType::Image) o.locked = true;
        }
        const Node *tree = spTreeOf(root);
        if (tree) convertChildren(tree, scope, GroupSpace(), {});
        if (masterRecord && scope.fieldsSeen) masterRecord->fields = fields;

        // Words for the speaker.
        const auto notesPath = pkg.related(slidePath, QStringLiteral("notesSlide"));
        if (const Node *notes = pkg.part(notesPath)) {
            const Node *notesTree = spTreeOf(notes);
            QVector<const Node *> kids;
            if (notesTree) flattened(*notesTree, kids);
            for (const auto *sp : kids) {
                if (sp->name != QLatin1String("sp")) continue;
                const Ph ph = phOf(sp);
                if (!ph.is || phClass(ph.type) != QLatin1String("body")) continue;
                const Node *body = sp->child(QStringLiteral("txBody"));
                if (body) slide.notes = body->words().trimmed();
            }
        }

        convertTransition(root, slide, number);
        convertBuilds(root, slide, spidToObject, number);
        convertComments(slidePath, slide, number);
        doc.slides.append(slide);
    }

    QHash<QString, QString> authors;
    int newerComments = 0;

    void convertComments(const QString &slidePath, Slide &slide, int number) {
        const auto all = pkg.rels(slidePath);
        for (auto it = all.cbegin(); it != all.cend(); ++it) {
            if (it->external) continue;
            if (it->type.endsWith(QLatin1String("/relationships/comments"))) {
                const Node *root = pkg.part(it->target);
                if (!root) continue;
                for (const auto *cm : root->all(QStringLiteral("cm"))) {
                    Comment comment;
                    comment.id = Edit::newId(QStringLiteral("comment"));
                    comment.slideId = slide.id;
                    comment.author = authors.value(cm->attr(QStringLiteral("authorId")), QStringLiteral("Unknown"));
                    comment.created = cm->attr(QStringLiteral("dt"));
                    const Node *text = cm->child(QStringLiteral("text"));
                    comment.text = text ? text->text : QString();
                    doc.comments.append(comment);
                }
            } else if (it->type.contains(QLatin1String("comments"), Qt::CaseInsensitive) && !it->type.endsWith(QLatin1String("Authors"))) {
                const Node *root = pkg.part(it->target);
                int count = 0;
                if (root) { QVector<const Node *> cms; root->collect(QStringLiteral("cm"), cms); count = cms.size(); }
                if (count) { newerComments += count; warnings.add(QStringLiteral("Comments in PowerPoint's newer format were left out"), number); }
            }
        }
    }

    // ---- the deck

    bool run(QString *error) {
        // The main part, by relationship rather than by the customary path.
        const auto rootRels = pkg.rels(QString());
        for (auto it = rootRels.cbegin(); it != rootRels.cend(); ++it)
            if (it->type.endsWith(QLatin1String("/officeDocument"))) presentationPath = it->target;
        if (presentationPath.isEmpty() && pkg.has(QStringLiteral("ppt/presentation.xml"))) presentationPath = QStringLiteral("ppt/presentation.xml");
        presentation = pkg.part(presentationPath);
        if (!presentation || presentation->name != QLatin1String("presentation")) {
            *error = QStringLiteral("this is not a PowerPoint deck: it has no presentation inside.");
            return false;
        }

        // Size.
        qreal cx = 12192000, cy = 6858000;
        if (const Node *size = presentation->child(QStringLiteral("sldSz"))) {
            cx = size->attr(QStringLiteral("cx")).toDouble();
            cy = size->attr(QStringLiteral("cy")).toDouble();
        }
        if (cx <= 0 || cy <= 0) { cx = 12192000; cy = 6858000; }
        // Any shape is allowed, within what a deck can hold.
        const qreal ratio = qBound(0.125, cy / cx, 8.0);
        doc.size = QSizeF(1920, qRound(1920 * ratio));
        cy = cx * ratio;
        u.k = doc.size.width() / cx;
        u.pt = doc.size.height() / (cy / 12700.0);

        // Slides in order, with stable ids before anything can link to them.
        QStringList slidePaths;
        QHash<QString, QString> slidePathByNumericId;
        if (const Node *list = presentation->child(QStringLiteral("sldIdLst"))) {
            for (const auto *id : list->all(QStringLiteral("sldId"))) {
                const auto path = pkg.target(presentationPath, id->attr(QStringLiteral("r:id")));
                if (path.isEmpty() || !pkg.has(path) || slidePaths.contains(path)) continue;
                slidePaths.append(path);
                slidePathByNumericId.insert(id->attr(QStringLiteral("id")), path);
                slideIdByPath.insert(path, Edit::newId(QStringLiteral("slide")));
            }
        }
        if (slidePaths.isEmpty()) {
            *error = QStringLiteral("the deck has no slides.");
            return false;
        }

        // Theme colours and fonts for the deck's own palette.
        const auto masterPaths = pkg.relatedAll(presentationPath, QStringLiteral("slideMaster"));
        for (const auto &m : masterPaths) masterFor(m);
        if (!masters.empty()) {
            const auto firstPath = masterPaths.value(0, masters.begin()->first);
            const MasterInfo &first = masters.count(firstPath) ? masters.at(firstPath) : masters.begin()->second;
            const auto &t = first.theme;
            auto mapped = [&](const QString &key, const QString &fallback) {
                return t.colors.value(first.color.clrMap.value(key, fallback), t.colors.value(fallback));
            };
            doc.theme.name = t.name.isEmpty() ? QStringLiteral("Imported") : t.name;
            const QColor bg = mapped(QStringLiteral("bg1"), QStringLiteral("lt1")), fg = mapped(QStringLiteral("tx1"), QStringLiteral("dk1"));
            const QColor muted = mapped(QStringLiteral("tx2"), QStringLiteral("dk2")), accent = t.colors.value(QStringLiteral("accent1"));
            if (bg.isValid()) doc.theme.colors[QStringLiteral("background")] = bg;
            if (fg.isValid()) doc.theme.colors[QStringLiteral("foreground")] = fg;
            if (muted.isValid()) doc.theme.colors[QStringLiteral("muted")] = muted;
            if (accent.isValid()) doc.theme.colors[QStringLiteral("accent")] = accent;
            if (!t.major.isEmpty()) doc.theme.fonts[QStringLiteral("heading")] = t.major;
            if (!t.minor.isEmpty()) doc.theme.fonts[QStringLiteral("body")] = t.minor;
        }

        // Authors of comments.
        const auto authorsPath = pkg.related(presentationPath, QStringLiteral("commentAuthors"));
        if (const Node *root = pkg.part(authorsPath))
            for (const auto *a : root->all(QStringLiteral("cmAuthor"))) authors.insert(a->attr(QStringLiteral("id")), a->attr(QStringLiteral("name")));

        // Sections.
        QHash<QString, QString> sectionBySlidePath;
        QVector<const Node *> sectionLists;
        presentation->collect(QStringLiteral("sectionLst"), sectionLists);
        if (!sectionLists.isEmpty()) {
            for (const auto *section : sectionLists.first()->all(QStringLiteral("section"))) {
                Section s;
                s.id = Edit::newId(QStringLiteral("section"));
                s.name = section->attr(QStringLiteral("name"));
                bool used = false;
                if (const Node *ids = section->child(QStringLiteral("sldIdLst")))
                    for (const auto *id : ids->all(QStringLiteral("sldId"))) {
                        const auto path = slidePathByNumericId.value(id->attr(QStringLiteral("id")));
                        if (!path.isEmpty()) { sectionBySlidePath.insert(path, s.id); used = true; }
                    }
                if (used) doc.sections.append(s);
            }
            if (doc.sections.size() == 1) { doc.sections.clear(); sectionBySlidePath.clear(); }
        }

        doc.transition = 0;
        doc.transitionDuration = 0.5;
        int number = 0;
        for (const auto &path : slidePaths) convertSlide(path, ++number, sectionBySlidePath.value(path));
        if (doc.slides.isEmpty()) { *error = QStringLiteral("none of the deck's slides could be read."); return false; }

        // Embedded typefaces are not carried: say which the deck asks for.
        if (const Node *fonts = presentation->child(QStringLiteral("embeddedFontLst"))) {
            QStringList names;
            for (const auto *f : fonts->all(QStringLiteral("embeddedFont")))
                if (const Node *font = f->child(QStringLiteral("font"))) names.append(font->attr(QStringLiteral("typeface")));
            if (!names.isEmpty()) warnings.add(QStringLiteral("Typefaces embedded in the deck are not carried across; it asks for %1").arg(names.join(QStringLiteral(", "))), 0);
        }
        return true;
    }
};

} // namespace

namespace Pptx {

Result read(const QByteArray &raw) {
    Result result;
    Package pkg(raw);
    if (!pkg.zip.isValid()) {
        result.error = QStringLiteral("this is not a PowerPoint deck: %1.").arg(pkg.zip.error());
        return result;
    }
    Reader reader(pkg);
    reader.scratchDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString error;
    if (!reader.run(&error)) { result.error = error; return result; }
    result.document = reader.doc;
    result.warnings = reader.warnings.lines();
    result.ok = true;
    return result;
}

Result load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Result result;
        result.error = QStringLiteral("%1 could not be read.").arg(QFileInfo(path).fileName());
        return result;
    }
    return read(file.readAll());
}

} // namespace Pptx
