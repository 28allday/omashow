#include "render/scenerenderer.h"

#include "core/scene.h"
#include "render/textlayout.h"
#include "core/imageasset.h"
#include "core/mediaasset.h"
#include "core/shape.h"
#include "core/table.h"
#include "core/chart.h"

#include <QFont>
#include <QPainter>

namespace {

void paintObject(QPainter &painter, const SceneObject &object) {
    if (object.hidden || object.opacity <= 0.0)
        return;

    painter.save();
    painter.setOpacity(object.opacity);

    if (!qFuzzyIsNull(object.rotation) || !qFuzzyCompare(object.paintScale, 1.0)) {
        const QPointF centre = object.rect.center();
        painter.translate(centre);
        painter.rotate(object.rotation);
        painter.scale(object.paintScale, object.paintScale);
        painter.translate(-centre);
    }

    switch (object.type) {
    case ObjectType::Chart: Chart::paint(painter,object); break;
    case ObjectType::Table: Table::paint(painter,object); break;
    case ObjectType::Rect:
        Shape::paint(painter,object);
        break;

    case ObjectType::Media: {
        auto picture=object;
        picture.image=MediaAsset::frameAt(object,object.mediaPosition);
        ImageAsset::paint(painter,picture);
        break;
    }
    case ObjectType::Image: ImageAsset::paint(painter,object); break;
    case ObjectType::Text: {
        TextLayout::paint(painter,object);
        break;
    }
    }

    painter.restore();
}

} // namespace

void SceneRenderer::paint(QPainter &painter, const QVector<SceneObject> &states) {
    for (const SceneObject &state : states)
        paintObject(painter, state);
}

QImage SceneRenderer::render(const QVector<SceneObject> &states, const QSizeF &documentSize,
                             const QSize &pixelSize, const QColor &background) {
    QImage image(pixelSize, QImage::Format_RGBA8888);
    image.fill(background);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (documentSize.width() > 0.0 && documentSize.height() > 0.0) {
        painter.scale(pixelSize.width() / documentSize.width(),
                      pixelSize.height() / documentSize.height());
    }

    paint(painter, states);
    painter.end();
    return image;
}
