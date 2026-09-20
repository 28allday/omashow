#include "render/mathlayout.h"

#include <QCache>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace {

// How a piece of notation sits next to its neighbours. Relations breathe more
// than sums, and a comma only pushes what follows it.
enum Class { Ord, Bin, Rel, Punct, Fun, Open, Close };

struct Symbol {
    QString glyph;
    int cls = Ord;
    bool big = false;       // drawn larger and centred on the axis
    bool limits = false;    // its scripts go above and below, not beside
};

const QHash<QString, Symbol> &symbols() {
    static const QHash<QString, Symbol> table = {
        // Greek, as written in every textbook.
        {"alpha", {QString::fromUtf8("α")}}, {"beta", {QString::fromUtf8("β")}},
        {"gamma", {QString::fromUtf8("γ")}}, {"delta", {QString::fromUtf8("δ")}},
        {"epsilon", {QString::fromUtf8("ε")}}, {"varepsilon", {QString::fromUtf8("ε")}},
        {"zeta", {QString::fromUtf8("ζ")}}, {"eta", {QString::fromUtf8("η")}},
        {"theta", {QString::fromUtf8("θ")}}, {"vartheta", {QString::fromUtf8("ϑ")}},
        {"iota", {QString::fromUtf8("ι")}}, {"kappa", {QString::fromUtf8("κ")}},
        {"lambda", {QString::fromUtf8("λ")}}, {"mu", {QString::fromUtf8("μ")}},
        {"nu", {QString::fromUtf8("ν")}}, {"xi", {QString::fromUtf8("ξ")}},
        {"pi", {QString::fromUtf8("π")}}, {"rho", {QString::fromUtf8("ρ")}},
        {"sigma", {QString::fromUtf8("σ")}}, {"tau", {QString::fromUtf8("τ")}},
        {"upsilon", {QString::fromUtf8("υ")}}, {"phi", {QString::fromUtf8("φ")}},
        {"varphi", {QString::fromUtf8("ϕ")}}, {"chi", {QString::fromUtf8("χ")}},
        {"psi", {QString::fromUtf8("ψ")}}, {"omega", {QString::fromUtf8("ω")}},
        {"Gamma", {QString::fromUtf8("Γ")}}, {"Delta", {QString::fromUtf8("Δ")}},
        {"Theta", {QString::fromUtf8("Θ")}}, {"Lambda", {QString::fromUtf8("Λ")}},
        {"Xi", {QString::fromUtf8("Ξ")}}, {"Pi", {QString::fromUtf8("Π")}},
        {"Sigma", {QString::fromUtf8("Σ")}}, {"Upsilon", {QString::fromUtf8("Υ")}},
        {"Phi", {QString::fromUtf8("Φ")}}, {"Psi", {QString::fromUtf8("Ψ")}},
        {"Omega", {QString::fromUtf8("Ω")}},
        // Things done to numbers.
        {"times", {QString::fromUtf8("×"), Bin}}, {"div", {QString::fromUtf8("÷"), Bin}},
        {"pm", {QString::fromUtf8("±"), Bin}}, {"mp", {QString::fromUtf8("∓"), Bin}},
        {"cdot", {QString::fromUtf8("⋅"), Bin}}, {"ast", {QString::fromUtf8("∗"), Bin}},
        {"star", {QString::fromUtf8("⋆"), Bin}}, {"circ", {QString::fromUtf8("∘"), Bin}},
        {"bullet", {QString::fromUtf8("∙"), Bin}}, {"oplus", {QString::fromUtf8("⊕"), Bin}},
        {"otimes", {QString::fromUtf8("⊗"), Bin}}, {"cap", {QString::fromUtf8("∩"), Bin}},
        {"cup", {QString::fromUtf8("∪"), Bin}}, {"setminus", {QString::fromUtf8("∖"), Bin}},
        // Things said about them.
        {"leq", {QString::fromUtf8("≤"), Rel}}, {"le", {QString::fromUtf8("≤"), Rel}},
        {"geq", {QString::fromUtf8("≥"), Rel}}, {"ge", {QString::fromUtf8("≥"), Rel}},
        {"neq", {QString::fromUtf8("≠"), Rel}}, {"ne", {QString::fromUtf8("≠"), Rel}},
        {"approx", {QString::fromUtf8("≈"), Rel}}, {"equiv", {QString::fromUtf8("≡"), Rel}},
        {"sim", {QString::fromUtf8("∼"), Rel}}, {"propto", {QString::fromUtf8("∝"), Rel}},
        {"ll", {QString::fromUtf8("≪"), Rel}}, {"gg", {QString::fromUtf8("≫"), Rel}},
        {"subset", {QString::fromUtf8("⊂"), Rel}}, {"supset", {QString::fromUtf8("⊃"), Rel}},
        {"subseteq", {QString::fromUtf8("⊆"), Rel}}, {"supseteq", {QString::fromUtf8("⊇"), Rel}},
        {"in", {QString::fromUtf8("∈"), Rel}}, {"notin", {QString::fromUtf8("∉"), Rel}},
        {"to", {QString::fromUtf8("→"), Rel}}, {"rightarrow", {QString::fromUtf8("→"), Rel}},
        {"leftarrow", {QString::fromUtf8("←"), Rel}}, {"gets", {QString::fromUtf8("←"), Rel}},
        {"Rightarrow", {QString::fromUtf8("⇒"), Rel}}, {"Leftarrow", {QString::fromUtf8("⇐"), Rel}},
        {"leftrightarrow", {QString::fromUtf8("↔"), Rel}}, {"mapsto", {QString::fromUtf8("↦"), Rel}},
        // The rest of the furniture.
        {"infty", {QString::fromUtf8("∞")}}, {"partial", {QString::fromUtf8("∂")}},
        {"nabla", {QString::fromUtf8("∇")}}, {"angle", {QString::fromUtf8("∠")}},
        {"degree", {QString::fromUtf8("°")}}, {"prime", {QString::fromUtf8("′")}},
        {"hbar", {QString::fromUtf8("ℏ")}}, {"ell", {QString::fromUtf8("ℓ")}},
        {"emptyset", {QString::fromUtf8("∅")}}, {"aleph", {QString::fromUtf8("ℵ")}},
        {"forall", {QString::fromUtf8("∀")}}, {"exists", {QString::fromUtf8("∃")}},
        {"neg", {QString::fromUtf8("¬")}}, {"ldots", {QString::fromUtf8("…")}},
        {"cdots", {QString::fromUtf8("⋯")}}, {"dots", {QString::fromUtf8("…")}},
        {"perp", {QString::fromUtf8("⊥"), Rel}}, {"parallel", {QString::fromUtf8("∥"), Rel}},
        // Sums and the like: bigger, and their scripts sit above and below.
        {"sum", {QString::fromUtf8("∑"), Ord, true, true}},
        {"prod", {QString::fromUtf8("∏"), Ord, true, true}},
        {"coprod", {QString::fromUtf8("∐"), Ord, true, true}},
        {"bigcup", {QString::fromUtf8("⋃"), Ord, true, true}},
        {"bigcap", {QString::fromUtf8("⋂"), Ord, true, true}},
        {"int", {QString::fromUtf8("∫"), Ord, true, false}},
        {"iint", {QString::fromUtf8("∬"), Ord, true, false}},
        {"oint", {QString::fromUtf8("∮"), Ord, true, false}},
    };
    return table;
}

// Names that are set upright, because they are words rather than quantities.
const QStringList &functions() {
    static const QStringList names = {"sin", "cos", "tan", "cot", "sec", "csc",
                                      "arcsin", "arccos", "arctan", "sinh", "cosh",
                                      "tanh", "log", "ln", "lg", "exp", "det", "dim",
                                      "gcd", "deg", "ker", "arg", "Pr", "mod"};
    return names;
}
// The same, but their scripts go underneath: lim, max, min.
const QStringList &limitFunctions() {
    static const QStringList names = {"lim", "max", "min", "sup", "inf", "limsup", "liminf"};
    return names;
}

struct Node {
    enum Kind { Row, Letter, Frac, Root, Scripts, Big, Fence, Space, Accent } kind = Row;
    QString text;            // Letter: the glyph; Accent: which mark
    int cls = Ord;
    bool italic = false;
    bool big = false;        // Letter: drawn larger, centred on the axis
    bool limits = false;     // Big: scripts above and below
    bool hasSup = false, hasSub = false, hasIndex = false;
    qreal space = 0;         // Space: in multiples of the type size
    QString open, close;     // Fence
    QVector<Node> kids;
};

int classOf(const Node &node) {
    if (node.kind == Node::Scripts || node.kind == Node::Accent)
        return node.kids.isEmpty() ? Ord : classOf(node.kids.first());
    return node.cls;
}

// What one character means on its own.
Node letterFor(QChar c) {
    Node node;
    node.kind = Node::Letter;
    node.text = c;
    if (c.isLetter()) { node.italic = true; return node; }
    static const QHash<QChar, QPair<QString, int>> marks = {
        {'+', {QStringLiteral("+"), Bin}}, {'-', {QString::fromUtf8("−"), Bin}},
        {'*', {QString::fromUtf8("∗"), Bin}}, {'/', {QStringLiteral("/"), Bin}},
        {'=', {QStringLiteral("="), Rel}}, {'<', {QStringLiteral("<"), Rel}},
        {'>', {QStringLiteral(">"), Rel}}, {',', {QStringLiteral(","), Punct}},
        {';', {QStringLiteral(";"), Punct}}, {':', {QStringLiteral(":"), Rel}},
        {'(', {QStringLiteral("("), Open}}, {')', {QStringLiteral(")"), Close}},
        {'[', {QStringLiteral("["), Open}}, {']', {QStringLiteral("]"), Close}},
        {'|', {QStringLiteral("|"), Ord}}, {'!', {QStringLiteral("!"), Close}},
        {'\'', {QString::fromUtf8("′"), Ord}},
    };
    if (marks.contains(c)) { node.text = marks.value(c).first; node.cls = marks.value(c).second; }
    return node;
}

// Reading what was typed. Anything it does not know, it names.
struct Parser {
    QString source;
    int at = 0;
    QString error;

    bool done() const { return at >= source.size(); }
    QChar peek(int ahead = 0) const {
        return at + ahead < source.size() ? source.at(at + ahead) : QChar();
    }
    void skipSpace() { while (!done() && peek().isSpace()) ++at; }

    QString command() {
        ++at;   // the backslash
        if (done()) { error = QStringLiteral("The equation ends with a stray \\."); return {}; }
        if (!peek().isLetter()) { const QString one = peek(); ++at; return one; }
        QString name;
        while (!done() && peek().isLetter()) name += source.at(at++);
        return name;
    }
    // \text{...} and friends take their braces literally.
    QString rawGroup(const QString &owner) {
        skipSpace();
        if (peek() != '{') {
            error = QStringLiteral("\\%1 needs its words in braces, as \\%1{like this}.").arg(owner);
            return {};
        }
        ++at;
        QString out;
        int depth = 1;
        while (!done()) {
            const QChar c = source.at(at++);
            if (c == '{') ++depth;
            if (c == '}' && --depth == 0) return out;
            out += c;
        }
        error = QStringLiteral("A { in \\%1 is never closed.").arg(owner);
        return {};
    }
    Node group(const QString &owner) {
        skipSpace();
        if (peek() == '{') {
            ++at;
            Node row = parseRow(true);
            if (!error.isEmpty()) return row;
            if (peek() != '}') { error = QStringLiteral("A { is never closed."); return row; }
            ++at;
            return row;
        }
        if (done() || peek() == '}') {
            error = QStringLiteral("\\%1 is missing something to work on.").arg(owner);
            return {};
        }
        Node single = atom();
        Node row;
        row.kids.append(single);
        return row;
    }
    Node words(const QString &text) {
        Node row;
        for (const QChar &c : text) {
            if (c == ' ') { Node gap; gap.kind = Node::Space; gap.space = .28; row.kids.append(gap); continue; }
            Node letter = letterFor(c);
            letter.italic = false;
            row.kids.append(letter);
        }
        return row;
    }
    Node named(const QString &name, int cls, bool limits) {
        Node node = words(name);
        node.cls = cls;
        node.limits = limits;
        node.kind = Node::Row;
        if (limits) {
            Node big;
            big.kind = Node::Big;
            big.limits = true;
            big.kids = {node, {}, {}};
            return big;
        }
        return node;
    }

    Node atom() {
        skipSpace();
        const QChar c = peek();
        if (c == '{') return group(QStringLiteral("{"));
        if (c == '\\') {
            const QString name = command();
            if (!error.isEmpty()) return {};
            if (name == QStringLiteral("frac") || name == QStringLiteral("dfrac") ||
                name == QStringLiteral("tfrac")) {
                Node node;
                node.kind = Node::Frac;
                const Node top = group(name);
                if (!error.isEmpty()) return node;
                const Node bottom = group(name);
                node.kids = {top, bottom};
                return node;
            }
            if (name == QStringLiteral("sqrt")) {
                Node node;
                node.kind = Node::Root;
                Node index;
                skipSpace();
                if (peek() == '[') {
                    ++at;
                    index = parseRow(false, ']');
                    if (!error.isEmpty()) return node;
                    if (peek() != ']') { error = QStringLiteral("A [ in \\sqrt is never closed."); return node; }
                    ++at;
                    node.hasIndex = true;
                }
                const Node body = group(name);
                node.kids = {body, index};
                return node;
            }
            if (name == QStringLiteral("text") || name == QStringLiteral("mathrm") ||
                name == QStringLiteral("operatorname") || name == QStringLiteral("mathit")) {
                const QString literal = rawGroup(name);
                if (!error.isEmpty()) return {};
                Node row = words(literal);
                if (name == QStringLiteral("mathit"))
                    for (auto &kid : row.kids) kid.italic = kid.text.at(0).isLetter();
                return row;
            }
            static const QHash<QString, QString> accents = {
                {"bar", QStringLiteral("bar")}, {"overline", QStringLiteral("bar")},
                {"hat", QString::fromUtf8("^")}, {"widehat", QString::fromUtf8("^")},
                {"tilde", QString::fromUtf8("~")}, {"vec", QString::fromUtf8("→")},
                {"dot", QString::fromUtf8("˙")}, {"ddot", QString::fromUtf8("¨")},
            };
            if (accents.contains(name)) {
                Node node;
                node.kind = Node::Accent;
                node.text = accents.value(name);
                const Node body = group(name);
                node.kids = {body};
                return node;
            }
            if (name == QStringLiteral("left")) {
                Node node;
                node.kind = Node::Fence;
                skipSpace();
                node.open = peek() == '.' ? QString() : QString(peek());
                if (done()) { error = QStringLiteral("\\left needs a bracket after it."); return node; }
                ++at;
                const Node body = parseRow(false);
                if (!error.isEmpty()) return node;
                skipSpace();
                if (!(peek() == '\\' && source.mid(at, 6) == QStringLiteral("\\right"))) {
                    error = QStringLiteral("\\left has no \\right to close it.");
                    return node;
                }
                at += 6;
                skipSpace();
                if (done()) { error = QStringLiteral("\\right needs a bracket after it."); return node; }
                node.close = peek() == '.' ? QString() : QString(peek());
                ++at;
                node.kids = {body};
                return node;
            }
            static const QHash<QString, qreal> spaces = {
                {",", .16}, {";", .28}, {":", .22}, {"!", -.16}, {" ", .33},
                {"quad", 1.0}, {"qquad", 2.0},
            };
            if (spaces.contains(name)) {
                Node node;
                node.kind = Node::Space;
                node.space = spaces.value(name);
                return node;
            }
            if (name == QStringLiteral("{") || name == QStringLiteral("}") ||
                name == QStringLiteral("%") || name == QStringLiteral("$") ||
                name == QStringLiteral("&") || name == QStringLiteral("#") ||
                name == QStringLiteral("_")) {
                Node node = letterFor(name.at(0));
                node.italic = false;
                node.cls = Ord;
                return node;
            }
            if (functions().contains(name)) return named(name, Fun, false);
            if (limitFunctions().contains(name)) return named(name, Fun, true);
            if (symbols().contains(name)) {
                const auto symbol = symbols().value(name);
                Node node;
                node.kind = Node::Letter;
                node.text = symbol.glyph;
                node.cls = symbol.cls;
                node.big = symbol.big;
                if (!symbol.big) return node;
                Node big;
                big.kind = Node::Big;
                big.limits = symbol.limits;
                big.kids = {node, {}, {}};
                return big;
            }
            if (name.startsWith(QStringLiteral("begin")) || name.startsWith(QStringLiteral("end"))) {
                error = QStringLiteral("Environments such as \\%1 are not understood. "
                                       "Write the lines separately instead.").arg(name);
                return {};
            }
            error = QStringLiteral("\\%1 is not one of the commands this understands.").arg(name);
            return {};
        }
        if (c.isDigit()) {
            Node node;
            node.kind = Node::Letter;
            while (!done() && (peek().isDigit() || (peek() == '.' && peek(1).isDigit())))
                node.text += source.at(at++);
            return node;
        }
        if (c == '^' || c == '_' || c == '}' || c == ']') {
            error = c == '}' ? QStringLiteral("There is a } with no { before it.")
                             : QStringLiteral("There is a %1 with nothing before it to apply to.").arg(c);
            return {};
        }
        ++at;
        return letterFor(c);
    }

    // `stop` is true inside braces, where a } ends the row.
    Node parseRow(bool stop, QChar until = QChar()) {
        Node row;
        while (true) {
            skipSpace();
            if (done()) break;
            if (stop && peek() == '}') break;
            if (!until.isNull() && peek() == until) break;
            if (peek() == '\\' && source.mid(at, 6) == QStringLiteral("\\right")) break;
            Node item = atom();
            if (!error.isEmpty()) return row;
            // Scripts belong to what they follow.
            Node sup, sub;
            bool hasSup = false, hasSub = false;
            while (true) {
                skipSpace();
                if (peek() != '^' && peek() != '_') break;
                const bool up = peek() == '^';
                ++at;
                const Node script = group(up ? QStringLiteral("^") : QStringLiteral("_"));
                if (!error.isEmpty()) return row;
                if (up) { sup = script; hasSup = true; } else { sub = script; hasSub = true; }
            }
            if (hasSup || hasSub) {
                if (item.kind == Node::Big && item.limits) {
                    item.kids[1] = sub;   // below
                    item.kids[2] = sup;   // above
                    item.hasSub = hasSub;
                    item.hasSup = hasSup;
                } else {
                    Node scripts;
                    scripts.kind = Node::Scripts;
                    scripts.kids = {item, sup, sub};
                    scripts.hasSup = hasSup;
                    scripts.hasSub = hasSub;
                    item = scripts;
                }
            }
            row.kids.append(item);
        }
        return row;
    }
};

// Laying it out. Every box is measured from its own baseline, so boxes can be
// stacked, raised and dropped without knowing what is inside them.
struct Box {
    qreal width = 0, ascent = 0, descent = 0;
    QVector<MathLayout::Glyph> glyphs;
    QVector<QRectF> rules;
};

QFont fontAt(const QFont &base, qreal size, bool italic) {
    QFont font = base;
    font.setPixelSize(qMax(1, int(std::lround(size))));
    font.setItalic(italic);
    font.setUnderline(false);
    font.setCapitalization(QFont::MixedCase);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    return font;
}
qreal axisFor(const QFont &base, qreal size) {
    const QFontMetricsF metrics(fontAt(base, size, false));
    return metrics.xHeight() / 2;
}
qreal ruleFor(qreal size) { return qMax(1.0, size * .055); }

void shift(Box &box, qreal dx, qreal dy) {
    for (auto &glyph : box.glyphs) glyph.origin += QPointF(dx, dy);
    for (auto &rule : box.rules) rule.translate(dx, dy);
}
void merge(Box &into, const Box &from) {
    into.glyphs += from.glyphs;
    into.rules += from.rules;
}
qreal gapBetween(int left, int right, qreal size) {
    if (left == Punct) return size * .16;
    if (left == Open || right == Close || right == Punct) return 0;
    if (left == Rel || right == Rel) return size * .24;
    if (left == Bin || right == Bin) return size * .18;
    if (left == Fun) return size * .12;
    return 0;
}

Box layout(const Node &node, const QFont &base, qreal size);

// A glyph, measured by its ink so fractions and roots fit it snugly.
Box glyphBox(const QString &text, const QFont &base, qreal size, bool italic, qreal stretchY = 1) {
    Box box;
    if (text.isEmpty()) return box;
    const QFontMetricsF metrics(fontAt(base, size, italic));
    const QRectF ink = metrics.tightBoundingRect(text);
    box.width = metrics.horizontalAdvance(text);
    box.ascent = qMax(0.0, -ink.top() * stretchY);
    box.descent = qMax(0.0, ink.bottom() * stretchY);
    box.glyphs.append({text, QPointF(0, 0), size, italic, stretchY});
    return box;
}

Box rowBox(const QVector<Node> &kids, const QFont &base, qreal size) {
    Box row;
    qreal x = 0;
    for (int i = 0; i < kids.size(); ++i) {
        Box box = layout(kids.at(i), base, size);
        if (i > 0) x += gapBetween(classOf(kids.at(i - 1)), classOf(kids.at(i)), size);
        shift(box, x, 0);
        merge(row, box);
        row.ascent = qMax(row.ascent, box.ascent);
        row.descent = qMax(row.descent, box.descent);
        x += box.width;
    }
    row.width = qMax(0.0, x);
    return row;
}

Box layout(const Node &node, const QFont &base, qreal size) {
    const qreal axis = axisFor(base, size);
    const qreal thickness = ruleFor(size);
    switch (node.kind) {
    case Node::Row:
        return rowBox(node.kids, base, size);
    case Node::Space: {
        Box box;
        box.width = size * node.space;
        return box;
    }
    case Node::Letter:
        return glyphBox(node.text, base, size, node.italic);
    case Node::Frac: {
        Box top = layout(node.kids.at(0), base, size);
        Box bottom = layout(node.kids.at(1), base, size);
        const qreal gap = size * .18;
        const qreal width = qMax(top.width, bottom.width) + size * .2;
        Box box;
        box.width = width;
        shift(top, (width - top.width) / 2, -(axis + thickness / 2 + gap + top.descent));
        shift(bottom, (width - bottom.width) / 2, -axis + thickness / 2 + gap + bottom.ascent);
        merge(box, top);
        merge(box, bottom);
        box.rules.append(QRectF(0, -axis - thickness / 2, width, thickness));
        box.ascent = axis + thickness / 2 + gap + top.descent + top.ascent;
        box.descent = -axis + thickness / 2 + gap + bottom.ascent + bottom.descent;
        return box;
    }
    case Node::Root: {
        Box body = layout(node.kids.at(0), base, size);
        const qreal gap = thickness * 2.5;
        const QFontMetricsF metrics(fontAt(base, size, false));
        const QString sign = QString::fromUtf8("√");
        const QRectF ink = metrics.tightBoundingRect(sign);
        const qreal needed = body.ascent + body.descent + gap;
        const qreal stretch = ink.height() > 0 ? qMax(1.0, needed / ink.height()) : 1.0;
        const qreal signWidth = metrics.horizontalAdvance(sign);
        Box box;
        Box index;
        qreal dx = 0;
        if (node.hasIndex) {
            index = layout(node.kids.at(1), base, size * .55);
            dx = index.width + size * .05;
        }
        // The sign sits so its foot is level with the bottom of what is inside.
        const qreal originY = body.descent - ink.bottom() * stretch;
        box.glyphs.append({sign, QPointF(dx, originY), size, false, stretch});
        const qreal top = originY + ink.top() * stretch;
        shift(body, dx + signWidth, 0);
        merge(box, body);
        box.rules.append(QRectF(dx + signWidth, top, body.width + size * .08, thickness));
        box.width = dx + signWidth + body.width + size * .08;
        box.ascent = qMax(-top, body.ascent);
        box.descent = qMax(body.descent, 0.0);
        if (node.hasIndex) {
            shift(index, 0, top + index.descent + size * .15);
            merge(box, index);
            box.ascent = qMax(box.ascent, -(top + size * .15) + index.ascent + index.descent);
        }
        return box;
    }
    case Node::Scripts: {
        Box body = layout(node.kids.at(0), base, size);
        const qreal small = qMax(6.0, size * .68);
        Box box = body;
        box.width = body.width;
        qreal widest = 0;
        // Scripts are placed by their own baseline, so a letter with a dot on it
        // does not sit lower than one without.
        if (node.hasSup) {
            Box sup = layout(node.kids.at(1), base, small);
            const qreal raise = qMax(size * .45, body.ascent - size * .22);
            shift(sup, body.width + size * .04, -raise);
            merge(box, sup);
            box.ascent = qMax(box.ascent, raise + sup.ascent);
            box.descent = qMax(box.descent, sup.descent - raise);
            widest = qMax(widest, sup.width);
        }
        if (node.hasSub) {
            Box sub = layout(node.kids.at(2), base, small);
            const qreal drop = qMax(size * .2, body.descent + size * .05);
            shift(sub, body.width + size * .04, drop);
            merge(box, sub);
            box.descent = qMax(box.descent, drop + sub.descent);
            box.ascent = qMax(box.ascent, sub.ascent - drop);
            widest = qMax(widest, sub.width);
        }
        box.width = body.width + size * .04 + widest;
        return box;
    }
    case Node::Big: {
        const bool wordy = node.kids.at(0).kind == Node::Row;
        const qreal opSize = wordy ? size : size * 1.35;
        Box op = wordy ? layout(node.kids.at(0), base, size)
                       : glyphBox(node.kids.at(0).text, base, opSize, false);
        if (!wordy) {
            // Centre the sign on the axis, where a fraction bar would sit.
            const qreal dy = -axis - (op.descent - op.ascent) / 2;
            shift(op, 0, dy);
            op.ascent -= dy;
            op.descent += dy;
        }
        const qreal small = qMax(6.0, size * .66);
        Box box = op;
        if (!node.limits) {
            Node scripts;
            scripts.kind = Node::Scripts;
            Node plain = node;
            plain.limits = true;
            plain.hasSup = plain.hasSub = false;
            plain.kids = {node.kids.at(0), {}, {}};
            scripts.kids = {plain, node.kids.at(2), node.kids.at(1)};
            scripts.hasSup = node.hasSup;
            scripts.hasSub = node.hasSub;
            return layout(scripts, base, size);
        }
        if (node.hasSup) {
            Box above = layout(node.kids.at(2), base, small);
            shift(above, (op.width - above.width) / 2, -(op.ascent + size * .12 + above.descent));
            merge(box, above);
            box.ascent = qMax(box.ascent, op.ascent + size * .12 + above.ascent + above.descent);
            box.width = qMax(box.width, above.width);
        }
        if (node.hasSub) {
            Box below = layout(node.kids.at(1), base, small);
            shift(below, (op.width - below.width) / 2, op.descent + size * .12 + below.ascent);
            merge(box, below);
            box.descent = qMax(box.descent, op.descent + size * .12 + below.ascent + below.descent);
            box.width = qMax(box.width, below.width);
        }
        return box;
    }
    case Node::Fence: {
        Box body = layout(node.kids.at(0), base, size);
        const QFontMetricsF metrics(fontAt(base, size, false));
        Box box;
        qreal x = 0;
        const auto bracket = [&](const QString &text) {
            if (text.isEmpty()) return;
            const QRectF ink = metrics.tightBoundingRect(text);
            const qreal reach = qMax(body.ascent - axis, body.descent + axis);
            const qreal needed = 2 * reach + size * .1;
            const qreal stretch = ink.height() > 0 ? qMax(1.0, needed / ink.height()) : 1.0;
            const qreal centre = (ink.top() + ink.bottom()) / 2 * stretch;
            const qreal originY = -axis - centre;
            box.glyphs.append({text, QPointF(x, originY), size, false, stretch});
            box.ascent = qMax(box.ascent, -(originY + ink.top() * stretch));
            box.descent = qMax(box.descent, originY + ink.bottom() * stretch);
            x += metrics.horizontalAdvance(text);
        };
        bracket(node.open);
        shift(body, x, 0);
        merge(box, body);
        box.ascent = qMax(box.ascent, body.ascent);
        box.descent = qMax(box.descent, body.descent);
        x += body.width;
        bracket(node.close);
        box.width = x;
        return box;
    }
    case Node::Accent: {
        Box body = layout(node.kids.at(0), base, size);
        Box box = body;
        const qreal gap = size * .06;
        if (node.text == QStringLiteral("bar")) {
            box.rules.append(QRectF(0, -(body.ascent + gap + thickness), body.width, thickness));
            box.ascent = body.ascent + gap + thickness;
            return box;
        }
        Box mark = glyphBox(node.text, base, size * .8, false);
        shift(mark, (body.width - mark.width) / 2, -(body.ascent + gap) + mark.descent);
        merge(box, mark);
        box.ascent = qMax(box.ascent, body.ascent + gap + mark.ascent);
        return box;
    }
    }
    return {};
}

QByteArray keyFor(const QString &source, const QFont &base, qreal size, int align) {
    return (source + '\x1f' + base.family() + '\x1f' + QString::number(base.weight()) + '\x1f' +
            QString::number(size, 'f', 2) + '\x1f' + QString::number(align))
        .toUtf8();
}

} // namespace

MathLayout::Rendered MathLayout::build(const QString &source, const QFont &base, qreal size,
                                       int align) {
    static thread_local QCache<QByteArray, Rendered> cache(256);
    const auto key = keyFor(source, base, size, align);
    if (const auto *hit = cache.object(key)) return *hit;

    Rendered rendered;
    const auto lines = source.split(QRegularExpression(QStringLiteral("\\\\\\\\|\n")));
    QVector<Box> boxes;
    for (const auto &line : lines) {
        Parser parser{line, 0, {}};
        const Node row = parser.parseRow(false);
        if (!parser.error.isEmpty()) {
            rendered.error = parser.error;
            cache.insert(key, new Rendered(rendered));
            return rendered;
        }
        if (!parser.done()) {
            rendered.error = QStringLiteral("There is a %1 with no opening to match it.")
                                 .arg(parser.peek());
            cache.insert(key, new Rendered(rendered));
            return rendered;
        }
        boxes.append(layout(row, base, size));
    }
    // Lines stack, each placed by the box's own alignment.
    const qreal lineGap = size * .35;
    qreal widest = 0;
    for (const auto &box : boxes) widest = qMax(widest, box.width);
    qreal y = 0;
    for (int i = 0; i < boxes.size(); ++i) {
        Box box = boxes.at(i);
        const qreal spare = widest - box.width;
        const qreal dx = align == 1 ? spare / 2 : align == 2 ? spare : 0;
        shift(box, dx, y + box.ascent);
        rendered.glyphs += box.glyphs;
        rendered.rules += box.rules;
        y += box.ascent + box.descent + (i + 1 < boxes.size() ? lineGap : 0);
    }
    rendered.ok = true;
    rendered.size = QSizeF(widest, y);
    cache.insert(key, new Rendered(rendered));
    return rendered;
}

void MathLayout::paint(QPainter &painter, const Rendered &rendered, const QPointF &topLeft,
                       const QFont &base, const QColor &color) {
    painter.save();
    painter.setPen(color);
    for (const auto &glyph : rendered.glyphs) {
        painter.setFont(fontAt(base, glyph.size, glyph.italic));
        if (qFuzzyCompare(glyph.stretchY, 1.0)) {
            painter.drawText(topLeft + glyph.origin, glyph.text);
            continue;
        }
        painter.save();
        painter.translate(topLeft + glyph.origin);
        painter.scale(1, glyph.stretchY);
        painter.drawText(QPointF(0, 0), glyph.text);
        painter.restore();
    }
    for (const auto &rule : rendered.rules) painter.fillRect(rule.translated(topLeft), color);
    painter.restore();
}
