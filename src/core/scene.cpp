#include "core/scene.h"
#include <QTransform>
#include "core/shape.h"

const SceneObject *Slide::find(const QString &id) const {
    for (const SceneObject &object : objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

SceneObject *Slide::find(const QString &id) {
    for (SceneObject &object : objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

QString Slide::objectAt(const QPointF &point) const {
    for (int i = objects.size() - 1; i >= 0; --i) {
        const auto &object = objects.at(i);
        if (object.hidden || object.locked) continue;
        QTransform transform;
        const auto centre = object.rect.center();
        transform.translate(centre.x(), centre.y()); transform.rotate(object.rotation); transform.translate(-centre.x(), -centre.y());
        if (Shape::contains(object,transform.inverted().map(point))) return object.id;
    }
    return QString();
}
