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
#include <QDataStream>
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
bool legacy(const SceneObject &o) {
    return o.runs.isEmpty() && o.textAlign == 0 && o.verticalAlign == 1 && o.lineHeight == 100 &&
           o.paragraphSpacing == 0 && o.textIndent == 0 && o.listStyle == 0 && o.textFit == 0;
}
std::shared_ptr<QTextDocument> layout(const SceneObject &o, qreal size) {
    // QTextDocument belongs to its creating thread. Cache per thread, with a
    // memory budget, and exclude position/opacity so animation reuses layout.
    static thread_local QCache<QByteArray, std::shared_ptr<QTextDocument>> cache(16*1024);
    QByteArray key;
    QDataStream stream(&key, QIODevice::WriteOnly);
    stream << o.text << o.fontFamily << size << o.rect.width() << o.fontWeight
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
    doc->setTextWidth(qMax(1.0, o.rect.width()));
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc->setDefaultTextOption(option);
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
                list.setStyle(o.listStyle == 1 ? QTextListFormat::ListDisc
                                               : QTextListFormat::ListDecimal);
                list.setIndent(level);
                list.setStart(level == 1 ? o.listStart : 1);
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
    if (legacy(o)) {
        const QFontMetricsF metrics(fontFor(o, size));
        return metrics
            .boundingRect(QRectF(0, 0, qMax(1.0, o.rect.width()), 1000000), Qt::TextWordWrap,
                          o.uppercase ? o.text.toUpper() : o.text)
            .size();
    }
    return layout(o, size)->documentLayout()->documentSize();
}
qreal height(const SceneObject &o, qreal size) {
    return contentSize(o, size).height();
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
    if (o.textFit == 1 && m.renderedHeight > o.rect.height()) {
        int lo = 4, hi = qMax(4, int(o.fontSize));
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (height(o, mid) <= o.rect.height())
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
    if (legacy(o)) {
        painter.setFont(fontFor(o, o.fontSize));
        painter.setPen(o.textColor);
        painter.drawText(o.rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                         o.uppercase ? o.text.toUpper() : o.text);
        return;
    }
    const auto metrics = measure(o);
    auto doc = layout(o, metrics.effectiveSize);
    const qreal spare = qMax(0.0, o.rect.height() - metrics.renderedHeight);
    const qreal dy = o.verticalAlign == 1 ? spare / 2 : o.verticalAlign == 2 ? spare : 0;
    painter.save();
    painter.setClipRect(o.rect, Qt::IntersectClip);
    painter.translate(o.rect.topLeft() + QPointF(0, dy));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, o.textColor);
    doc->documentLayout()->draw(&painter, context);
    painter.restore();
}

QString TextLayout::editorHtml(const SceneObject &object) {
    auto content = object;
    content.uppercase = false;
    return layout(content, measure(object).effectiveSize)->toHtml();
}
