#include "render/textlayout.h"
#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QCache>
#include "core/textruns.h"
#include "render/mathlayout.h"
#include "core/shape.h"
#include <QDataStream>
#include <cmath>
#include <memory>
namespace {
QFont fontFor(const SceneObject &o, qreal size) {
    QFont font(o.fontFamily);
    font.setPixelSize(qMax(1, int(size)));
    font.setWeight(QFont::Weight(o.fontWeight));
    font.setItalic(o.italic);
    font.setUnderline(o.underline);
    font.setLetterSpacing(QFont::AbsoluteSpacing, o.letterSpacing);
    // Capitals as a transform, not a different string: run offsets are against
    // what was typed, and a few letters change length when uppercased.
    font.setCapitalization(o.uppercase ? QFont::AllUppercase : QFont::MixedCase);
    return font;
}
Qt::Alignment alignment(int value) {
    return value == 1   ? Qt::AlignHCenter
           : value == 2 ? Qt::AlignRight
           : value == 3 ? Qt::AlignJustify
                        : Qt::AlignLeft;
}
// 0 none, 1 bullets, 2 numbers, then the rest of the markers people ask for.
// Bullets nest the way they do on paper: disc, then circle, then square.
QTextListFormat::Style listStyleFor(int style, int level) {
    switch (style) {
    case 1: return level <= 1   ? QTextListFormat::ListDisc
                   : level == 2 ? QTextListFormat::ListCircle
                                : QTextListFormat::ListSquare;
    case 2: return QTextListFormat::ListDecimal;
    case 3: return QTextListFormat::ListCircle;
    case 4: return QTextListFormat::ListSquare;
    case 5: return QTextListFormat::ListLowerAlpha;
    case 6: return QTextListFormat::ListUpperAlpha;
    case 7: return QTextListFormat::ListLowerRoman;
    case 8: return QTextListFormat::ListUpperRoman;
    }
    return QTextListFormat::ListDisc;
}
bool numbered(int style) { return style == 2 || style >= 5; }
bool maths(const SceneObject &o) { return o.textKind == 1; }
bool onPath(const SceneObject &o) { return o.textKind == 2 && !o.pathData.isEmpty(); }

// The words, laid along a line that bends. Each letter sits on the path at its
// own angle, so the shape of the line is what the reader follows.
struct PathRun {
    QPointF where;
    qreal angle = 0;
    QString letter;
};
struct PathText {
    QVector<PathRun> letters;
    qreal wanted = 0;    // how much room the words asked for
    qreal length = 0;    // how much the path has
    qreal height = 0;
    qreal lift = 0;      // across the line, not down the page
};
// A closed line has no beginning, so give it one people expect: the top, read
// left to right. An open line is read the way it was drawn.
qreal startOfClosedPath(QPainterPath &path, bool *closed) {
    *closed = false;
    if (path.elementCount() < 2) return 0;
    const auto first = path.pointAtPercent(0), last = path.pointAtPercent(1);
    if (QLineF(first, last).length() > qMax(1.0, path.boundingRect().width() * .01)) return 0;
    *closed = true;
    const int samples = 360;
    const auto topmost = [&](const QPainterPath &candidate) {
        int best = 0;
        for (int i = 1; i < samples; ++i)
            if (candidate.pointAtPercent(qreal(i) / samples).y() <
                candidate.pointAtPercent(qreal(best) / samples).y())
                best = i;
        return best;
    };
    int top = topmost(path);
    // Going forward from the top, the words should travel to the right.
    const auto after = path.pointAtPercent(qreal((top + samples / 36) % samples) / samples);
    if (after.x() < path.pointAtPercent(qreal(top) / samples).x()) {
        path = path.toReversed();
        top = topmost(path);
    }
    return path.length() * qreal(top) / samples;
}
PathText alongPath(const SceneObject &o, qreal size) {
    PathText laid;
    QPainterPath path = Shape::path(o);
    laid.length = path.length();
    if (laid.length <= 0) return laid;
    bool closed = false;
    const qreal begin = startOfClosedPath(path, &closed);
    const QFont font = fontFor(o, size);
    const QFontMetricsF metrics(font);
    laid.height = metrics.height();
    const QString words = (o.uppercase ? o.text.toUpper() : o.text).simplified();
    for (const QChar &letter : words) laid.wanted += metrics.horizontalAdvance(letter);
    // Where the words start is the box's own alignment, read along the line. A
    // closed line is aligned about its top rather than about an end it has not
    // got, so centred words sit at the top the way they are asked to.
    const qreal spare = laid.length - laid.wanted;
    qreal at = closed ? (o.textAlign == 1   ? -laid.wanted / 2
                         : o.textAlign == 2 ? -laid.wanted
                                            : 0)
                      : (o.textAlign == 1   ? qMax(0.0, spare / 2)
                         : o.textAlign == 2 ? qMax(0.0, spare)
                                            : 0);
    // And which side of the line they sit on is its vertical alignment.
    laid.lift = o.verticalAlign == 0 ? -metrics.descent()
              : o.verticalAlign == 2 ? metrics.ascent() : 0;
    for (const QChar &letter : words) {
        const qreal advance = metrics.horizontalAdvance(letter);
        const qreal centre = at + advance / 2;
        at += advance;
        if (!closed && centre > laid.length) break;
        const qreal along = std::fmod(std::fmod(centre + begin, laid.length) + laid.length,
                                      laid.length);
        const qreal percent = path.percentAtLength(along);
        laid.letters.append({path.pointAtPercent(percent), path.angleAtPercent(percent),
                             QString(letter)});
    }
    return laid;
}
// An equation drawn from the words that made it, at the box's own type size.
MathLayout::Rendered equation(const SceneObject &o, qreal size) {
    return MathLayout::build(o.text, fontFor(o, size), size, o.textAlign);
}
bool legacy(const SceneObject &o) {
    return !maths(o) && !onPath(o) && o.runs.isEmpty() && o.textAlign == 0 && o.verticalAlign == 1 && o.lineHeight == 100 &&
           o.paragraphSpacing == 0 && o.textIndent == 0 && o.listStyle == 0 && o.textFit == 0 &&
           o.tabStop == 0 && o.columns <= 1 && o.direction == 0;
}

// How wide one column is, and how far apart the columns sit.
qreal columnGap(const SceneObject &o, qreal size) {
    return o.columnGap > 0 ? o.columnGap : size * 0.8;
}
qreal columnWidth(const SceneObject &o, qreal size) {
    const int columns = qBound(1, o.columns, 6);
    if (columns <= 1) return qMax(1.0, o.rect.width());
    const qreal gaps = columnGap(o, size) * (columns - 1);
    return qMax(1.0, (o.rect.width() - gaps) / columns);
}
std::shared_ptr<QTextDocument> layout(const SceneObject &o, qreal size) {
    // QTextDocument belongs to its creating thread. Cache per thread, with a
    // memory budget, and exclude position/opacity so animation reuses layout.
    static thread_local QCache<QByteArray, std::shared_ptr<QTextDocument>> cache(16*1024);
    QByteArray key;
    QDataStream stream(&key, QIODevice::WriteOnly);
    stream << o.text << o.fontFamily << size << o.rect.width() << o.rect.height()
           << o.tabStop << o.columns << o.columnGap << o.direction << o.fontWeight
           << o.italic << o.underline << o.letterSpacing << o.uppercase
           << o.textAlign << o.lineHeight << o.paragraphSpacing << o.textIndent
           << o.listStyle << o.listStart << o.textColor;
    for (const auto &run : o.runs)
        stream << run.start << run.length << run.weight << run.italic << run.underline
               << run.strike << run.baseline << run.fontSize << run.fontFamily << run.color;
    if (auto *cached = cache.object(key)) return *cached;
    auto doc = std::make_shared<QTextDocument>();
    doc->setUndoRedoEnabled(false);
    doc->setDocumentMargin(0);
    doc->setDefaultFont(fontFor(o, size));
    doc->setIndentWidth(size * 1.2);
    doc->setTextWidth(columnWidth(o, size));
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Tabs line up on a stop rather than nesting, unless the box is a list,
    // where a leading tab is what makes a sub-item.
    if (o.listStyle == 0)
        option.setTabStopDistance(o.tabStop > 0 ? o.tabStop : size * 4);
    if (o.direction == 1) option.setTextDirection(Qt::LeftToRight);
    if (o.direction == 2) option.setTextDirection(Qt::RightToLeft);
    doc->setDefaultTextOption(option);
    // Columns are pages: the text flows down one and into the next.
    if (o.columns > 1)
        doc->setPageSize(QSizeF(columnWidth(o, size), qMax(1.0, o.rect.height())));
    QTextCursor cursor(doc.get());
    const auto paragraphs = o.text.split('\n');
    QMap<int, QTextList *> lists;
    // Where each paragraph's text starts, in the string and in the document, so
    // runs can be placed after the whole thing is built.
    QVector<int> authoredStart, documentStart, strippedTabs;
    int authored = 0;
    for (int i = 0; i < paragraphs.size(); ++i) {
        if (i)
            cursor.insertBlock();
        QString text = paragraphs.at(i);
        int level = 1;
        int stripped = 0;
        if (o.listStyle)
            while (text.startsWith('\t')) {
                ++level;
                ++stripped;
                text.remove(0, 1);
            }
        level = qMin(8, level);
        QTextBlockFormat block;
        block.setAlignment(alignment(o.textAlign));
        if (o.direction != 0)
            block.setLayoutDirection(o.direction == 2 ? Qt::RightToLeft : Qt::LeftToRight);
        block.setLineHeight(o.lineHeight, QTextBlockFormat::ProportionalHeight);
        block.setBottomMargin(i + 1 < paragraphs.size() ? o.paragraphSpacing : 0);
        block.setLeftMargin(o.textIndent);
        cursor.setBlockFormat(block);
        QTextCharFormat chars;
        chars.setFont(fontFor(o, size));
        chars.setForeground(o.textColor);
        cursor.setCharFormat(chars);
        authoredStart.append(authored);
        documentStart.append(cursor.position());
        strippedTabs.append(stripped);
        authored += paragraphs.at(i).size() + 1;   // the newline that split it
        cursor.insertText(text);
        if (o.listStyle && !text.isEmpty()) {
            if (!lists.contains(level)) {
                QTextListFormat list;
                list.setStyle(listStyleFor(o.listStyle, level));
                list.setIndent(level);
                if (numbered(o.listStyle)) list.setStart(level == 1 ? o.listStart : 1);
                lists[level] = cursor.createList(list);
            } else
                lists[level]->add(cursor.block());
            for (auto it = lists.begin(); it != lists.end();)
                if (it.key() > level)
                    it = lists.erase(it);
                else
                    ++it;
        }
    }
    // Stretches that look different: mapped back onto the built document.
    for (const auto &run : o.runs) {
        for (int i = 0; i < paragraphs.size(); ++i) {
            const int bodyStart = authoredStart.at(i) + strippedTabs.at(i);
            const int bodyEnd = authoredStart.at(i) + paragraphs.at(i).size();
            const int from = qMax(run.start, bodyStart);
            const int to = qMin(run.start + run.length, bodyEnd);
            if (from >= to) continue;
            QTextCursor scope(doc.get());
            scope.setPosition(documentStart.at(i) + (from - bodyStart));
            scope.setPosition(documentStart.at(i) + (to - bodyStart), QTextCursor::KeepAnchor);
            QTextCharFormat format;
            QFont font = fontFor(o, run.fontSize > 0 ? run.fontSize : size);
            if (!run.fontFamily.isEmpty()) font.setFamilies({run.fontFamily});
            if (run.weight != 0) font.setWeight(QFont::Weight(run.weight));
            if (run.italic != 0) font.setItalic(run.italic == 1);
            if (run.underline != 0) font.setUnderline(run.underline == 1);
            format.setFont(font);
            if (run.strike != 0) format.setFontStrikeOut(run.strike == 1);
            if (run.color.isValid()) format.setForeground(run.color);
            if (run.baseline == 1) format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            if (run.baseline == 2) format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
            scope.mergeCharFormat(format);
        }
    }
    cache.insert(key, new std::shared_ptr<QTextDocument>(doc), qMax(1, int((o.text.size()*10+4096)/1024)));
    return doc;
}
QSizeF contentSize(const SceneObject &o, qreal size) {
    if (onPath(o)) {
        const auto laid = alongPath(o, size);
        // Along a path, running out of room is running off the end of the line.
        return QSizeF(laid.wanted > laid.length ? o.rect.width() + 1 : o.rect.width(),
                      laid.height);
    }
    if (maths(o)) {
        const auto rendered = equation(o, size);
        // What could not be read is drawn as the words it was typed as, so the
        // box is measured that way too.
        if (!rendered.ok) {
            auto words = o;
            words.textKind = 0;
            return contentSize(words, size);
        }
        return rendered.size;
    }
    if (o.columns > 1) {
        // What matters is how many columns the text needs, expressed as height.
        auto doc = layout(o, size);
        const int pages = qMax(1, doc->pageCount());
        const qreal height = qMax(1.0, o.rect.height());
        const int columns = qBound(1, o.columns, 6);
        return QSizeF(o.rect.width(), pages <= columns ? height * pages / columns
                                                       : height * pages / columns);
    }
    if (legacy(o)) {
        const QFontMetricsF metrics(fontFor(o, size));
        return metrics
            .boundingRect(QRectF(0, 0, qMax(1.0, o.rect.width()), 1000000), Qt::TextWordWrap,
                          o.uppercase ? o.text.toUpper() : o.text)
            .size();
    }
    return layout(o, size)->documentLayout()->documentSize();
}
} // namespace
TextLayout::Metrics TextLayout::measure(const SceneObject &o) {
    Metrics m;
    if (o.type != ObjectType::Text)
        return m;
    m.effectiveSize = o.fontSize;
    auto dimensions = contentSize(o, o.fontSize);
    m.naturalHeight = dimensions.height();
    m.renderedHeight = m.naturalHeight;
    // Words only ever run out of height, because they wrap. An equation does not
    // wrap, so shrinking it has to watch its width too.
    const auto fits = [&o](qreal size) {
        const auto room = contentSize(o, size);
        return room.height() <= o.rect.height() &&
               (!maths(o) || room.width() <= o.rect.width());
    };
    if (o.textFit == 1 && !fits(o.fontSize)) {
        int lo = 4, hi = qMax(4, int(o.fontSize));
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (fits(mid))
                lo = mid;
            else
                hi = mid - 1;
        }
        m.effectiveSize = lo;
        dimensions = contentSize(o, lo);
        m.renderedHeight = dimensions.height();
    }
    m.overflow = m.renderedHeight > o.rect.height() + .5 ||
                 dimensions.width() > o.rect.width() + .5;
    return m;
}
void TextLayout::paint(QPainter &painter, const SceneObject &o) {
    if (onPath(o)) {
        const auto metrics = measure(o);
        const auto laid = alongPath(o, metrics.effectiveSize);
        painter.save();
        painter.setPen(o.textColor);
        painter.setFont(fontFor(o, metrics.effectiveSize));
        for (const auto &letter : laid.letters) {
            painter.save();
            painter.translate(letter.where);
            painter.rotate(-letter.angle);
            painter.drawText(QPointF(-QFontMetricsF(painter.font())
                                          .horizontalAdvance(letter.letter) / 2,
                                      laid.lift),
                             letter.letter);
            painter.restore();
        }
        painter.restore();
        return;
    }
    if (maths(o)) {
        const auto metrics = measure(o);
        const auto rendered = equation(o, metrics.effectiveSize);
        if (!rendered.ok) {
            // The fallback is honest: the source, as ordinary text, and the
            // review says what stopped it.
            auto words = o;
            words.textKind = 0;
            paint(painter, words);
            return;
        }
        const qreal spare = qMax(0.0, o.rect.height() - rendered.size.height());
        const qreal dy = o.verticalAlign == 1 ? spare / 2 : o.verticalAlign == 2 ? spare : 0;
        const qreal room = qMax(0.0, o.rect.width() - rendered.size.width());
        const qreal dx = o.textAlign == 1 ? room / 2 : o.textAlign == 2 ? room : 0;
        painter.save();
        painter.setClipRect(o.rect, Qt::IntersectClip);
        MathLayout::paint(painter, rendered, o.rect.topLeft() + QPointF(dx, dy),
                          fontFor(o, metrics.effectiveSize), o.textColor);
        painter.restore();
        return;
    }
    if (legacy(o)) {
        painter.setFont(fontFor(o, o.fontSize));
        painter.setPen(o.textColor);
        painter.drawText(o.rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                         o.uppercase ? o.text.toUpper() : o.text);
        return;
    }
    const auto metrics = measure(o);
    auto doc = layout(o, metrics.effectiveSize);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, o.textColor);
    if (o.columns > 1) {
        // Each column is one page of the same document, side by side.
        const int columns = qBound(1, o.columns, 6);
        const qreal width = columnWidth(o, metrics.effectiveSize);
        const qreal gap = columnGap(o, metrics.effectiveSize);
        const qreal height = qMax(1.0, o.rect.height());
        painter.save();
        painter.setClipRect(o.rect, Qt::IntersectClip);
        for (int column = 0; column < qMin(columns, qMax(1, doc->pageCount())); ++column) {
            painter.save();
            painter.translate(o.rect.topLeft() + QPointF(column * (width + gap), 0));
            painter.setClipRect(QRectF(0, 0, width, height), Qt::IntersectClip);
            painter.translate(0, -column * height);
            doc->documentLayout()->draw(&painter, context);
            painter.restore();
        }
        painter.restore();
        return;
    }
    const qreal spare = qMax(0.0, o.rect.height() - metrics.renderedHeight);
    const qreal dy = o.verticalAlign == 1 ? spare / 2 : o.verticalAlign == 2 ? spare : 0;
    painter.save();
    painter.setClipRect(o.rect, Qt::IntersectClip);
    painter.translate(o.rect.topLeft() + QPointF(0, dy));
    doc->documentLayout()->draw(&painter, context);
    painter.restore();
}

QString TextLayout::editorHtml(const SceneObject &object) {
    auto content = object;
    content.uppercase = false;
    // An equation, or words on a path, are edited as what was typed.
    content.textKind = 0;
    return layout(content, measure(object).effectiveSize)->toHtml();
}
