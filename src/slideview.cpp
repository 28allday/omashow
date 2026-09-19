#include "core/design.h"
#include "slideview.h"
#include "core/imagecrop.h"

#include "anim/evaluator.h"
#include "anim/presentation.h"
#include "core/scene.h"
#include "render/scenerenderer.h"

#include <QPainter>
#include <cmath>

SlideView::SlideView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    // Image, not FramebufferObject: this routes painting through the same
    // raster engine the export path uses, so the live view and a rendered frame
    // are the same pixels rather than merely similar ones.
    setRenderTarget(QQuickPaintedItem::Image);
}

void SlideView::setDeck(Backend *deck) {
    if (m_deck == deck)
        return;
    if (m_deck) disconnect(m_deck, nullptr, this, nullptr);
    m_deck = deck;
    if (m_deck) connect(m_deck, &Backend::documentChanged, this, [this] { updateLayout(); update(); });
    if(m_deck) connect(m_deck,&Backend::deckChanged,this,[this]{update();});
    emit deckChanged();
    updateLayout(); update();
}

void SlideView::setEditSlide(int index) {
    if (m_editSlide == index)
        return;
    m_editSlide = index;
    emit editSlideChanged();
    update();
}

QPointF SlideView::toDocument(qreal x, qreal y) const {
    if (qFuzzyIsNull(m_scale))
        return QPointF();
    return QPointF((x - m_origin.x()) / m_scale, (y - m_origin.y()) / m_scale);
}

void SlideView::setTime(qreal time) {
    if (qFuzzyCompare(m_time, time))
        return;
    m_time = time;
    emit timeChanged();
    update();
}

void SlideView::paint(QPainter *painter) {
    if (!m_deck)
        return;

    const Document &document = m_deck->document();
    const QSizeF documentSize = document.size;
    if (documentSize.isEmpty() || width() <= 0 || height() <= 0)
        return;

    updateLayout();
    const qreal scale = m_scale;
    const QRectF slideRect(m_origin, QSizeF(documentSize.width()*scale,documentSize.height()*scale));

    const bool editing = m_editSlide >= 0 && m_editSlide < document.slides.size();
    const int shown = editing ? m_editSlide : Presentation::frameAt(document, m_time, m_deck->includeSkipped()).slideIndex;
    const QColor background = document.slides.isEmpty()
                                  ? QColor(0, 0, 0)
                                  : editing ? Design::resolve(document, shown).background : Presentation::backgroundAt(document, m_time, m_deck->includeSkipped());

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->fillRect(slideRect, background);

    painter->save();
    painter->setClipRect(slideRect);
    painter->translate(slideRect.topLeft());
    painter->scale(scale, scale);
    if (editing) {
        Slide slide = Design::resolve(document, m_editSlide);
        if (!m_hiddenObject.isEmpty()) for(auto &o:slide.objects) if(o.id==m_hiddenObject) o.hidden=true;
        if(m_cropObject.isEmpty()) SceneRenderer::paint(*painter,slide.objects);
        else for(const auto &o:slide.objects) { if(o.id==m_cropObject && o.type==ObjectType::Image) SceneRenderer::paint(*painter,{ImageCrop::preview(o)}); SceneRenderer::paint(*painter,{o}); }
    } else {
        SceneRenderer::paint(*painter, m_deck->statesAt(m_time, m_deck->includeSkipped()));
    }
    painter->restore();
}

void SlideView::geometryChange(const QRectF &next, const QRectF &previous) {
    QQuickPaintedItem::geometryChange(next,previous); updateLayout();
}
void SlideView::updateLayout() {
    if (!m_deck || m_deck->slideSize().isEmpty() || width() <= 0 || height() <= 0) return;
    const auto size = m_deck->slideSize();
    const qreal scale = m_zoom > 0 ? m_zoom : qMin(width()/size.width(),height()/size.height());
    const QPointF origin((width()-size.width()*scale)/2 + m_pan.x(),
                         (height()-size.height()*scale)/2 + m_pan.y());
    if (!qFuzzyCompare(m_scale,scale) || origin != m_origin) {
        m_scale = scale; m_origin = origin; emit layoutChanged();
    }
}
void SlideView::zoomAt(qreal scale, qreal x, qreal y) {
    if (!m_deck || !std::isfinite(scale) || !std::isfinite(x) || !std::isfinite(y)) return;
    updateLayout();
    const auto anchor = toDocument(x,y);
    m_zoom = qBound(.1,scale,8.0);
    const auto size = m_deck->slideSize();
    m_pan = QPointF(x-anchor.x()*m_zoom-(width()-size.width()*m_zoom)/2,
                   y-anchor.y()*m_zoom-(height()-size.height()*m_zoom)/2);
    updateLayout(); emit layoutChanged(); update();
}
void SlideView::fit() { m_zoom = 0; m_pan = {}; updateLayout(); emit layoutChanged(); update(); }
void SlideView::panBy(qreal dx, qreal dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    m_pan += QPointF(dx,dy); updateLayout(); update();
}

void SlideView::fitRect(const QRectF &rect) {
    if(!m_deck || rect.isEmpty() || !std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
    m_zoom=qBound(.1,qMin(width()*.9/rect.width(),height()*.9/rect.height()),8.0);
    const auto size=m_deck->slideSize();
    m_pan=QPointF((size.width()/2-rect.center().x())*m_zoom,(size.height()/2-rect.center().y())*m_zoom);
    updateLayout(); emit layoutChanged(); update();
}
