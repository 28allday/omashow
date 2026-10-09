#include "core/design.h"
#include "slideview.h"
#include "core/imagecrop.h"

#include "anim/evaluator.h"
#include "anim/presentation.h"
#include "core/scene.h"
#include "render/scenerenderer.h"

#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QPaintEngine>
#include <cmath>

SlideView::SlideView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    // Qt 6.9+ accelerates QPainter on OpenGL FBOs. Other scene-graph APIs
    // automatically retain the image path; exports always use CPU raster.
    if (qgetenv("OMASHOW_RENDERER") != "raster")
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
#endif
    connect(&m_frames, &LiveFrames::ready, this, [this] { polish(); update(); });
}

void SlideView::setDeck(Backend *deck) {
    if (m_deck == deck)
        return;
    if (m_deck) disconnect(m_deck, nullptr, this, nullptr);
    m_frames.reset();
    m_deck = deck;
    if (m_deck) connect(m_deck, &Backend::documentChanged, this, [this] { updateLayout(); polish(); update(); });
    if(m_deck) connect(m_deck,&Backend::deckChanged,this,[this]{polish(); update();});
    if(m_deck) connect(m_deck,&Backend::mediaJobChanged,this,[this]{polish(); update();});
    emit deckChanged();
    updateLayout(); polish(); update();
}

void SlideView::setEditSlide(int index) {
    if (m_editSlide == index)
        return;
    m_editSlide = index;
    emit editSlideChanged();
    polish(); update();
}

QPointF SlideView::toDocument(qreal x, qreal y) const {
    if (qFuzzyIsNull(m_scale))
        return QPointF();
    return QPointF((x - m_origin.x()) / m_scale, (y - m_origin.y()) / m_scale);
}

QPointF SlideView::fromDocument(qreal x, qreal y) const {
    return QPointF(x * m_scale + m_origin.x(), y * m_scale + m_origin.y());
}

void SlideView::setTime(qreal time) {
    if (qFuzzyCompare(m_time, time))
        return;
    m_time = time;
    emit timeChanged();
    polish(); update();
}

void SlideView::updatePolish() {
    if (!m_deck) { m_states.clear(); return; }
    updateLayout();
    m_documentSize = m_deck->slideSize();
    const auto &cache = m_deck->presentation(m_deck->includeSkipped());
    if (m_editSlide >= 0 && m_editSlide < m_deck->slideCount()) {
        const auto slide = cache.slide(m_editSlide);
        m_background = slide.background;
        m_states = slide.objects;
        for (auto &o : m_states) if (o.id == m_hiddenObject) o.hidden = true;
    } else {
        m_background = cache.backgroundAt(m_time);
        m_states = m_deck->statesAt(m_time, m_deck->includeSkipped());
    }
    m_states = m_frames.prepare(m_states);
}

void SlideView::paint(QPainter *painter) {
    m_hardwarePainting = painter->paintEngine()->type() == QPaintEngine::OpenGL2;
    if (m_documentSize.isEmpty() || width() <= 0 || height() <= 0) return;
    const QRectF slideRect(m_origin, m_documentSize * m_scale);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->fillRect(slideRect, m_background);
    painter->save();
    painter->setClipRect(slideRect);
    painter->translate(slideRect.topLeft());
    painter->scale(m_scale, m_scale);
    if (m_cropObject.isEmpty()) SceneRenderer::paint(*painter, m_states);
    else for (const auto &o : m_states) {
        if (o.id == m_cropObject && o.type == ObjectType::Image) {
            // Only around the kept frame, so pictures with transparency stay
            // clean inside; the kept part is then drawn normally on top.
            const auto cut = ImageCrop::cutArea(o);
            painter->save();
            painter->setClipPath(cut, Qt::IntersectClip);
            SceneRenderer::paint(*painter, {ImageCrop::preview(o)});
            if (o.imageFormat == QLatin1String("svg")) // vectors skip the colour adjustment
                painter->fillPath(cut, QColor::fromRgbF(0, 0, 0, ImageCrop::cutDarken * ImageCrop::cutOpacity * o.opacity));
            painter->restore();
        }
        SceneRenderer::paint(*painter, {o});
    }
    painter->restore();
}

void SlideView::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickPaintedItem::itemChange(change, value);
    if (change != ItemSceneChange) return;
    disconnect(m_frameClock);
    // While the deck plays, read the clock as each frame begins and ask for
    // the next one, so every frame advances by exactly one refresh.
    if (value.window) m_frameClock = connect(value.window, &QQuickWindow::afterAnimating, this, [this] {
        if (!m_deck || !m_deck->playing() || m_editSlide >= 0 || !isVisible()) return;
        m_deck->frameTick();
        update();
    });
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
    m_pan += QPointF(dx,dy); updateLayout(); polish(); update();
}

void SlideView::fitRect(const QRectF &rect) {
    if(!m_deck || rect.isEmpty() || !std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
    m_zoom=qBound(.1,qMin(width()*.9/rect.width(),height()*.9/rect.height()),8.0);
    const auto size=m_deck->slideSize();
    m_pan=QPointF((size.width()/2-rect.center().x())*m_zoom,(size.height()/2-rect.center().y())*m_zoom);
    updateLayout(); emit layoutChanged(); update();
}
