#include "render/textlayout.h"
#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <memory>
namespace {
QFont fontFor(const SceneObject &o, qreal size) {
    QFont font(o.fontFamily);
    font.setPixelSize(qMax(1, int(size)));
    font.setWeight(QFont::Weight(o.fontWeight));
    font.setItalic(o.italic);
    font.setUnderline(o.underline);
    font.setLetterSpacing(QFont::AbsoluteSpacing, o.letterSpacing);
    return font;
}
Qt::Alignment alignment(int value) {
    return value == 1   ? Qt::AlignHCenter
           : value == 2 ? Qt::AlignRight
           : value == 3 ? Qt::AlignJustify
                        : Qt::AlignLeft;
}
bool legacy(const SceneObject &o) {
    return o.textAlign == 0 && o.verticalAlign == 1 && o.lineHeight == 100 &&
           o.paragraphSpacing == 0 && o.textIndent == 0 && o.listStyle == 0 && o.textFit == 0;
}
std::unique_ptr<QTextDocument> layout(const SceneObject &o, qreal size) {
    auto doc = std::make_unique<QTextDocument>();
    doc->setUndoRedoEnabled(false);
    doc->setDocumentMargin(0);
    doc->setDefaultFont(fontFor(o, size));
    doc->setIndentWidth(size * 1.2);
    doc->setTextWidth(qMax(1.0, o.rect.width()));
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc->setDefaultTextOption(option);
    QTextCursor cursor(doc.get());
    const auto paragraphs = (o.uppercase ? o.text.toUpper() : o.text).split('\n');
    QMap<int, QTextList *> lists;
    for (int i = 0; i < paragraphs.size(); ++i) {
        if (i)
            cursor.insertBlock();
        QString text = paragraphs.at(i);
        int level = 1;
        if (o.listStyle)
            while (text.startsWith('\t')) {
                ++level;
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
    return doc;
}
qreal height(const SceneObject &o, qreal size) {
    if (legacy(o)) {
        const QFontMetricsF metrics(fontFor(o, size));
        return metrics
            .boundingRect(QRectF(0, 0, qMax(1.0, o.rect.width()), 1000000), Qt::TextWordWrap,
                          o.uppercase ? o.text.toUpper() : o.text)
            .height();
    }
    return layout(o, size)->documentLayout()->documentSize().height();
}
} // namespace
TextLayout::Metrics TextLayout::measure(const SceneObject &o) {
    Metrics m;
    if (o.type != ObjectType::Text)
        return m;
    m.effectiveSize = o.fontSize;
    m.naturalHeight = height(o, o.fontSize);
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
        m.renderedHeight = height(o, lo);
    }
    m.overflow = m.renderedHeight > o.rect.height() + .5;
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
