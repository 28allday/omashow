#pragma once

// The live slide canvas.
//
// It holds no animation of its own: it is handed a time and asks the evaluator
// what the slide looks like then. That is the whole point — the same call the
// shot harness makes. A QML NumberAnimation in here would quietly break the
// export guarantee, so there are none anywhere in this app.

#include <QQuickPaintedItem>

// Backend is a Q_PROPERTY type here, so moc needs the full definition.
#include "backend.h"
#include "render/liveframes.h"

class SlideView : public QQuickPaintedItem {
    Q_OBJECT
    // Named "deck", not "backend": a context property called `backend` already
    // exists, and inside this item QML resolves an identical property name to
    // the item's own before the context's — so `backend: backend` binds the
    // property to itself, silently stays null, and the canvas paints nothing.
    Q_PROPERTY(Backend *deck READ deck WRITE setDeck NOTIFY deckChanged)
    Q_PROPERTY(qreal time READ time WRITE setTime NOTIFY timeChanged)
    // -1 plays the deck at `time`; >= 0 shows that one slide settled, which is
    // what the Edit canvas wants — you arrange objects where they end up, not
    // where a build happens to have left them.
    Q_PROPERTY(int editSlide READ editSlide WRITE setEditSlide NOTIFY editSlideChanged)
    // The document -> item transform, published so the selection overlay can be
    // drawn in QML without duplicating the letterbox maths.
    Q_PROPERTY(QString cropObject READ cropObject WRITE setCropObject NOTIFY cropObjectChanged)
    Q_PROPERTY(QString hiddenObject READ hiddenObject WRITE setHiddenObject NOTIFY hiddenObjectChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY layoutChanged)
    Q_PROPERTY(bool fitMode READ fitMode NOTIFY layoutChanged)
    Q_PROPERTY(qreal contentScale READ contentScale NOTIFY layoutChanged)
    Q_PROPERTY(QPointF contentOrigin READ contentOrigin NOTIFY layoutChanged)

public:
    explicit SlideView(QQuickItem *parent = nullptr);
    bool hardwarePainting() const { return m_hardwarePainting.load(); }

    Backend *deck() const { return m_deck; }
    void setDeck(Backend *deck);

    qreal time() const { return m_time; }
    void setTime(qreal time);

    int editSlide() const { return m_editSlide; }
    void setEditSlide(int index);

    qreal contentScale() const { return m_scale; }
    QPointF contentOrigin() const { return m_origin; }

    // Item point -> document point, for hit testing and dragging.
    Q_INVOKABLE QPointF toDocument(qreal x, qreal y) const;
    // The other way: a place on the slide, in this view's pixels.
    Q_INVOKABLE QPointF fromDocument(qreal x, qreal y) const;
    Q_INVOKABLE qreal documentScale() const { return m_scale; }

    QString cropObject() const { return m_cropObject; }
    void setCropObject(const QString &id) { if(m_cropObject==id) return; m_cropObject=id; emit cropObjectChanged(); polish(); update(); }
    QString hiddenObject() const { return m_hiddenObject; }
    void setHiddenObject(const QString &id) { if(m_hiddenObject==id) return; m_hiddenObject=id; emit hiddenObjectChanged(); polish(); update(); }
    qreal zoom() const { return m_scale; }
    bool fitMode() const { return m_zoom <= 0; }
    Q_INVOKABLE void zoomAt(qreal scale, qreal x, qreal y);
    Q_INVOKABLE void fit();
    Q_INVOKABLE void fitRect(const QRectF &rect);
    Q_INVOKABLE void panBy(qreal dx, qreal dy);
    void paint(QPainter *painter) override;
protected:
    void updatePolish() override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void cropObjectChanged();
    void hiddenObjectChanged();
    void deckChanged();
    void timeChanged();
    void editSlideChanged();
    void layoutChanged();

private:
    std::atomic_bool m_hardwarePainting{false};
    LiveFrames m_frames;
    QVector<SceneObject> m_states;
    QColor m_background = Qt::black;
    QSizeF m_documentSize;
    QString m_hiddenObject, m_cropObject;
    void updateLayout();
    qreal m_zoom = 0;
    QPointF m_pan;
    Backend *m_deck = nullptr;
    QMetaObject::Connection m_frameClock;
    qreal m_time = 0.0;
    int m_editSlide = -1;
    qreal m_scale = 1.0;
    QPointF m_origin;
};
