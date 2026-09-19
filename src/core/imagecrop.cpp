#include "core/imagecrop.h"
#include "core/imageasset.h"
#include <QTransform>
#include <cmath>
namespace {
QTransform rotation(const SceneObject &o) {
  QTransform t;
  const auto c = o.rect.center();
  t.translate(c.x(), c.y());
  t.rotate(o.rotation);
  t.translate(-c.x(), -c.y());
  return t;
}
} // namespace
QRectF ImageCrop::frame(const SceneObject &o) {
  auto target = o.rect;
  const auto source = ImageAsset::sourceRect(o);
  if (o.imageMode == 0 && !source.isEmpty()) {
    auto size = source.size();
    size.scale(o.rect.size(), Qt::KeepAspectRatio);
    target = QRectF(
        o.rect.center() - QPointF(size.width() / 2, size.height() / 2), size);
  }
  return target;
}
QRectF ImageCrop::fullRect(const SceneObject &o) {
  const auto source = ImageAsset::sourceRect(o), target = frame(o);
  if (source.isEmpty() || target.isEmpty())
    return {};
  const qreal sx = target.width() / source.width(),
              sy = target.height() / source.height();
  const auto size = ImageAsset::size(o);
  return QRectF(target.x() - source.x() * sx, target.y() - source.y() * sy,
                size.width() * sx, size.height() * sy);
}
SceneObject ImageCrop::preview(const SceneObject &o) {
  auto result = o;
  const auto full = fullRect(o);
  const auto centre = rotation(o).map(full.center());
  result.id = "@crop-preview/" + o.id;
  result.rect = QRectF(centre - QPointF(full.width() / 2, full.height() / 2),
                       full.size());
  result.imageCrop = QRectF(0, 0, 1, 1);
  result.imageMode = 2;
  result.imageMask = 0;
  result.opacity *= .25;
  return result;
}
bool ImageCrop::resize(SceneObject &result, const SceneObject &basis,
                       int handle, const QPointF &documentPoint,
                       bool lockAspect) {
  if (basis.type != ObjectType::Image || handle < 0 || handle > 7 ||
      !std::isfinite(documentPoint.x()) || !std::isfinite(documentPoint.y()))
    return false;
  auto box = frame(basis);
  const auto full = fullRect(basis);
  if (box.isEmpty() || full.isEmpty())
    return false;
  const auto point = rotation(basis).inverted().map(documentPoint);
  const bool left = handle == 0 || handle == 6 || handle == 7,
             right = handle == 2 || handle == 3 || handle == 4;
  const bool top = handle == 0 || handle == 1 || handle == 2,
             bottom = handle == 4 || handle == 5 || handle == 6;
  const qreal minW = qMin(full.width(), qMax(1.0, full.width() * .01)),
              minH = qMin(full.height(), qMax(1.0, full.height() * .01));
  if (left)
    box.setLeft(qBound(full.left(), point.x(), box.right() - minW));
  if (right)
    box.setRight(qBound(box.left() + minW, point.x(), full.right()));
  if (top)
    box.setTop(qBound(full.top(), point.y(), box.bottom() - minH));
  if (bottom)
    box.setBottom(qBound(box.top() + minH, point.y(), full.bottom()));
  if (lockAspect && handle % 2 == 0) {
    const auto original = frame(basis);
    const QPointF anchor(left ? original.right() : original.left(),
                         top ? original.bottom() : original.top());
    const qreal minimum =
        qMax(minW / original.width(), minH / original.height());
    const qreal maximum =
        qMin((left ? anchor.x() - full.left() : full.right() - anchor.x()) /
                 original.width(),
             (top ? anchor.y() - full.top() : full.bottom() - anchor.y()) /
                 original.height());
    const qreal factor = qBound(
        minimum,
        qMin(box.width() / original.width(), box.height() / original.height()),
        maximum);
    const auto size = original.size() * factor;
    box = QRectF(QPointF(left ? anchor.x() - size.width() : anchor.x(),
                         top ? anchor.y() - size.height() : anchor.y()),
                 size);
  }
  const auto centre = rotation(basis).map(box.center());
  result.rect =
      QRectF(centre - QPointF(box.width() / 2, box.height() / 2), box.size());
  result.rotation = basis.rotation;
  const qreal x = qBound(0.0, (box.x() - full.x()) / full.width(), .99),
              y = qBound(0.0, (box.y() - full.y()) / full.height(), .99);
  result.imageCrop =
      QRectF(x, y, qBound(.01, box.width() / full.width(), 1 - x),
             qBound(.01, box.height() / full.height(), 1 - y));
  return true;
}
