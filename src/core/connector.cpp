#include "core/connector.h"
#include "core/shape.h"
#include <QTransform>
#include <cmath>
QPointF Connector::anchor(const SceneObject &o, int side,
                          const QPointF &toward) {
  const auto c = o.rect.center();
  QTransform t;
  t.translate(c.x(), c.y());
  t.rotate(o.rotation);
  t.translate(-c.x(), -c.y());
  if (side == 0) {
    const auto delta = t.inverted().map(toward) - c;
    if (std::abs(delta.x()) / qMax(1.0, o.rect.width()) >
        std::abs(delta.y()) / qMax(1.0, o.rect.height()))
      side = delta.x() >= 0 ? 2 : 4;
    else
      side = delta.y() >= 0 ? 3 : 1;
  }
  return t.map(side == 1   ? QPointF(c.x(), o.rect.top())
               : side == 2 ? QPointF(o.rect.right(), c.y())
               : side == 3 ? QPointF(c.x(), o.rect.bottom())
                           : QPointF(o.rect.left(), c.y()));
}
void Connector::resolve(QVector<SceneObject> &objects) {
  QMap<QString, SceneObject> targets;
  for (const auto &o : objects)
    if (!o.connector)
      targets.insert(o.id, o);
  for (auto &o : objects)
    if (o.connector) {
      auto start = o.connectorStart, end = o.connectorEnd;
      const auto from = targets.constFind(o.connectorFrom),
                 to = targets.constFind(o.connectorTo);
      if (from != targets.cend())
        start = from->rect.center();
      if (to != targets.cend())
        end = to->rect.center();
      const auto centreStart = start, centreEnd = end;
      const bool coincident = centreStart == centreEnd;
      if (from != targets.cend())
        start = anchor(
            *from,
            coincident && o.connectorFromSide == 0 ? 2 : o.connectorFromSide,
            centreEnd);
      if (to != targets.cend())
        end = anchor(
            *to, coincident && o.connectorToSide == 0 ? 1 : o.connectorToSide,
            centreStart);
      QPainterPath path;
      path.moveTo(start);
      if (o.connectorRoute == 1) {
        if (coincident && from != targets.cend() && to != targets.cend()) {
          const qreal right = qMax(start.x(), end.x()) + 30,
                      top = qMin(start.y(), end.y()) - 30;
          path.lineTo(right, start.y());
          path.lineTo(right, top);
          path.lineTo(end.x(), top);
        } else if (std::abs(start.x() - centreStart.x()) <
                   std::abs(start.y() - centreStart.y())) {
          const qreal middle = (start.y() + end.y()) / 2;
          path.lineTo(start.x(), middle);
          path.lineTo(end.x(), middle);
        } else {
          const qreal middle = (start.x() + end.x()) / 2;
          path.lineTo(middle, start.y());
          path.lineTo(middle, end.y());
        }
        path.lineTo(end);
      } else if (o.connectorRoute == 2) {
        const qreal middle = (start.x() + end.x()) / 2;
        path.cubicTo(QPointF(middle, start.y()), QPointF(middle, end.y()), end);
      } else
        path.lineTo(end);
      // Keep fallback endpoints current in resolved copies; deletion and copy
      // use this geometry when detaching a target.
      o.connectorStart = start;
      o.connectorEnd = end;
      if (!Shape::assignPath(o, path)) {
        o.pathData.clear();
        o.rect = QRectF(start, QSizeF(1, 1));
        o.rotation = 0;
      }
    }
}
QPainterPath Connector::arrows(const SceneObject &o) {
  QPainterPath arrows;
  if (!o.connector || o.strokeWidth <= 0)
    return arrows;
  const auto path = Shape::path(o);
  if (path.isEmpty())
    return arrows;
  const auto add = [&](bool atStart) {
    const auto tip = path.pointAtPercent(atStart ? 0 : 1),
               inside = path.pointAtPercent(atStart ? .01 : .99);
    const auto delta = tip - inside;
    const auto distance = std::hypot(delta.x(), delta.y());
    if (distance < .0001)
      return;
    const auto direction = delta / distance;
    const auto length = qMax(9.0, o.strokeWidth * 3.5);
    const auto base = tip - direction * length;
    const QPointF normal(-direction.y() * length * .4,
                         direction.x() * length * .4);
    arrows.moveTo(tip);
    arrows.lineTo(base + normal);
    arrows.lineTo(base - normal);
    arrows.closeSubpath();
  };
  if (o.connectorArrowStart)
    add(true);
  if (o.connectorArrowEnd)
    add(false);
  return arrows;
}
void Connector::detachTarget(Slide &slide, const Slide &resolved,
                             const QString &id) {
  for (auto &o : slide.objects)
    if (o.connector && (o.connectorFrom == id || o.connectorTo == id)) {
      const auto *shown = resolved.find(o.id);
      if (shown) {
        o.connectorStart = shown->connectorStart;
        o.connectorEnd = shown->connectorEnd;
      }
      if (o.connectorFrom == id)
        o.connectorFrom.clear();
      if (o.connectorTo == id)
        o.connectorTo.clear();
    }
}
