#include "core/shape.h"
#include "core/imageasset.h"
#include "core/connector.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPathStroker>
#include <QRadialGradient>
#include <cmath>

QStringList Shape::names() {
  return {"Rectangle",   "Ellipse",      "Triangle", "Diamond",
          "Pentagon",    "Hexagon",      "Star",     "Heart",
          "Right arrow", "Chevron",      "Callout",  "Parallelogram",
          "Cylinder",    "Line",         "Plus",     "Multiply",
          "Left arrow",  "Double arrow", "Document", "Trapezoid",
          "Terminator",  "Cloud",        "Arc",      "Ring"};
}
namespace {
QPainterPath polygon(std::initializer_list<QPointF> points) {
  QPainterPath p;
  bool first = true;
  for (const auto &point : points) {
    if (first)
      p.moveTo(point);
    else
      p.lineTo(point);
    first = false;
  }
  p.closeSubpath();
  return p;
}
QPainterPath regular(int n, bool star = false) {
  QPainterPath p;
  for (int i = 0; i < n; ++i) {
    const qreal a = -M_PI / 2 + i * 2 * M_PI / n, r = star && i % 2 ? .23 : .5;
    const QPointF v(.5 + std::cos(a) * r, .5 + std::sin(a) * r);
    if (i == 0)
      p.moveTo(v);
    else
      p.lineTo(v);
  }
  p.closeSubpath();
  return p;
}
QPen pen(const SceneObject &o) {
  QPen p(o.strokeColor, o.strokeWidth, Qt::PenStyle(o.strokeStyle + 1),
         Qt::PenCapStyle(o.strokeCap * 16),
         Qt::PenJoinStyle(o.strokeJoin * 64));
  return p;
}
} // namespace
QPainterPath Shape::path(const SceneObject &o) {
  QPainterPath p;
  switch (o.shapeKind) {
  case 0:
    p.addRoundedRect(o.rect, o.cornerRadius, o.cornerRadius);
    return p;
  case 1:
    p.addEllipse(QRectF(0, 0, 1, 1));
    break;
  case 2:
    p = polygon({{.5, 0}, {1, 1}, {0, 1}});
    break;
  case 3:
    p = polygon({{.5, 0}, {1, .5}, {.5, 1}, {0, .5}});
    break;
  case 4:
    p = regular(5);
    break;
  case 5:
    p = regular(6);
    break;
  case 6:
    p = regular(10, true);
    break;
  case 7:
    p.moveTo(.5, 1);
    p.cubicTo(-.4, .3, .1, -.35, .5, .2);
    p.cubicTo(.9, -.35, 1.4, .3, .5, 1);
    p.closeSubpath();
    break;
  case 8:
    p = polygon({{0, .28},
                 {.62, .28},
                 {.62, 0},
                 {1, .5},
                 {.62, 1},
                 {.62, .72},
                 {0, .72}});
    break;
  case 9:
    p = polygon({{0, 0}, {.6, 0}, {1, .5}, {.6, 1}, {0, 1}, {.4, .5}});
    break;
  case 10:
    p = polygon(
        {{0, 0}, {1, 0}, {1, .75}, {.4, .75}, {.15, 1}, {.22, .75}, {0, .75}});
    break;
  case 11:
    p = polygon({{.22, 0}, {1, 0}, {.78, 1}, {0, 1}});
    break;
  case 12:
    p.moveTo(0, .15);
    p.cubicTo(0, -.05, 1, -.05, 1, .15);
    p.lineTo(1, .85);
    p.cubicTo(1, 1.05, 0, 1.05, 0, .85);
    p.closeSubpath();
    p.moveTo(1, .15);
    p.cubicTo(1, .35, 0, .35, 0, .15);
    p.setFillRule(Qt::WindingFill);
    break;
  case 13:
    p.moveTo(0, .5);
    p.lineTo(1, .5);
    break;
  case 14:
    p = polygon({{.35, 0},
                 {.65, 0},
                 {.65, .35},
                 {1, .35},
                 {1, .65},
                 {.65, .65},
                 {.65, 1},
                 {.35, 1},
                 {.35, .65},
                 {0, .65},
                 {0, .35},
                 {.35, .35}});
    break;
  case 15:
    p = polygon({{.15, 0},
                 {.5, .35},
                 {.85, 0},
                 {1, .15},
                 {.65, .5},
                 {1, .85},
                 {.85, 1},
                 {.5, .65},
                 {.15, 1},
                 {0, .85},
                 {.35, .5},
                 {0, .15}});
    break;
  case 16:
    p = polygon({{1, .28},
                 {.38, .28},
                 {.38, 0},
                 {0, .5},
                 {.38, 1},
                 {.38, .72},
                 {1, .72}});
    break;
  case 17:
    p = polygon({{0, .5},
                 {.28, 0},
                 {.28, .28},
                 {.72, .28},
                 {.72, 0},
                 {1, .5},
                 {.72, 1},
                 {.72, .72},
                 {.28, .72},
                 {.28, 1}});
    break;
  case 18:
    p.moveTo(0, 0);
    p.lineTo(1, 0);
    p.lineTo(1, .85);
    p.cubicTo(.6, .55, .4, 1.15, 0, .85);
    p.closeSubpath();
    break;
  case 19:
    p = polygon({{.2, 0}, {.8, 0}, {1, 1}, {0, 1}});
    break;
  case 20:
    p.addRoundedRect(o.rect, o.rect.height() / 2, o.rect.height() / 2);
    return p;
  case 21:
    p.moveTo(.2, .9);
    p.cubicTo(-.08, .95, -.08, .45, .16, .4);
    p.cubicTo(.08, .02, .55, -.1, .65, .23);
    p.cubicTo(1, .04, 1.18, .7, .87, .75);
    p.cubicTo(.85, 1.12, .52, 1.04, .46, .9);
    p.cubicTo(.4, 1.05, .23, 1.04, .2, .9);
    p.closeSubpath();
    break;
  case 22:
    p.arcMoveTo(QRectF(0, 0, 1, 1), 20);
    p.arcTo(QRectF(0, 0, 1, 1), 20, 280);
    break;
  case 23:
    p.addEllipse(QRectF(0, 0, 1, 1));
    p.addEllipse(QRectF(.23, .23, .54, .54));
    break;
  case custom:
    decode(o.pathData, &p);
    p.setFillRule(o.pathWinding ? Qt::WindingFill : Qt::OddEvenFill);
    break;
  default:
    return p;
  }
  QTransform t;
  t.translate(o.rect.x(), o.rect.y());
  t.scale(o.rect.width(), o.rect.height());
  return t.map(p);
}
QPainterPath Shape::worldPath(const SceneObject &o) {
  const auto c = o.rect.center();
  QTransform t;
  t.translate(c.x(), c.y());
  t.rotate(o.rotation);
  t.translate(-c.x(), -c.y());
  return t.map(path(o));
}
bool Shape::contains(const SceneObject &o, const QPointF &point) {
  if(o.type==ObjectType::Image && o.imageMask>0) {
    SceneObject mask; mask.rect=o.rect; mask.shapeKind=o.imageMask==1?1:o.imageMask==2?0:o.imageMask==3?5:7;
    mask.cornerRadius=o.cornerRadius>0?o.cornerRadius:qMin(o.rect.width(),o.rect.height())*.1;
    return path(mask).contains(point);
  }
  if (o.type != ObjectType::Rect)
    return o.rect.contains(point);
  if(o.connector && Connector::arrows(o).contains(point)) return true;
  auto p = path(o);
  if (p.contains(point))
    return true;
  QPainterPathStroker stroker;
  stroker.setWidth(qMax(6.0, o.strokeWidth));
  return stroker.createStroke(p).contains(point);
}
void Shape::paint(QPainter &painter, const SceneObject &o) {
  const auto p = path(o);
  if (o.shadowEnabled) {
    painter.save();
    painter.translate(o.shadowX, o.shadowY);
    painter.setPen(Qt::NoPen);
    painter.setBrush(o.shadowColor);
    painter.drawPath(p);
    painter.restore();
  }
  painter.setPen(o.strokeWidth > 0 ? pen(o) : QPen(Qt::NoPen));
  QBrush brush(o.fill);
  if (o.fillStyle == 1) {
    const qreal a = o.fillAngle * M_PI / 180;
    const QPointF v(std::cos(a) * o.rect.width() / 2,
                    std::sin(a) * o.rect.height() / 2);
    QLinearGradient g(o.rect.center() - v, o.rect.center() + v);
    g.setColorAt(0, o.fill);
    g.setColorAt(1, o.fillSecondary);
    brush = g;
  } else if (o.fillStyle == 2) {
    QRadialGradient g(o.rect.center(),
                      qMax(o.rect.width(), o.rect.height()) / 2);
    g.setColorAt(0, o.fill);
    g.setColorAt(1, o.fillSecondary);
    brush = g;
  } else if (o.fillStyle == 3) {
    painter.save();
    painter.fillPath(p, o.fillSecondary);
    painter.restore();
    brush = QBrush(o.fill, Qt::BrushStyle(Qt::Dense1Pattern + o.patternStyle));
  } else if (o.fillStyle == 4) {
    painter.save();
    painter.setClipPath(p, Qt::IntersectClip);
    ImageAsset::paint(painter, o);
    painter.restore();
    brush = Qt::NoBrush;
  } else if (o.fillStyle == 5)
    brush = Qt::NoBrush;
  painter.setBrush(brush);
  painter.drawPath(p);
  if(o.connector) painter.fillPath(Connector::arrows(o),o.strokeColor);
}
QVariantList Shape::encode(const QPainterPath &p) {
  QVariantList data;
  QPointF start;
  for (int i = 0; i < p.elementCount(); ++i) {
    const auto e = p.elementAt(i);
    if (e.isMoveTo()) {
      data.append(QVariant(QVariantList{"M", e.x, e.y}));
      start = QPointF(e.x, e.y);
    } else if (e.isLineTo()) {
      if (QPointF(e.x, e.y) == start &&
          (i + 1 == p.elementCount() || p.elementAt(i + 1).isMoveTo()))
        data.append(QVariant(QVariantList{"Z"}));
      else
        data.append(QVariant(QVariantList{"L", e.x, e.y}));
    } else if (e.isCurveTo() && i + 2 < p.elementCount()) {
      const auto b = p.elementAt(++i), c = p.elementAt(++i);
      data.append(QVariant(QVariantList{"C", e.x, e.y, b.x, b.y, c.x, c.y}));
    }
  }
  return data;
}
bool Shape::decode(const QVariantList &data, QPainterPath *out) {
  if (data.size() > 20000)
    return false;
  QPainterPath p;
  bool started = false;
  for (const auto &value : data) {
    const auto row = value.toList();
    const auto kind = row.value(0).toString();
    const int count = kind == "M" || kind == "L" ? 3
                      : kind == "C"              ? 7
                      : kind == "Z"              ? 1
                                                 : 0;
    if (!count || row.size() != count || (!started && kind != "M"))
      return false;
    for (int i = 1; i < row.size(); ++i) {
      bool ok = false;
      const auto n = row[i].toDouble(&ok);
      if (!ok || !std::isfinite(n) || std::abs(n) > 1000000)
        return false;
    }
    if (kind == "M") {
      p.moveTo(row[1].toDouble(), row[2].toDouble());
      started = true;
    } else if (kind == "L")
      p.lineTo(row[1].toDouble(), row[2].toDouble());
    else if (kind == "C")
      p.cubicTo(row[1].toDouble(), row[2].toDouble(), row[3].toDouble(),
                row[4].toDouble(), row[5].toDouble(), row[6].toDouble());
    else
      p.closeSubpath();
  }
  if (out)
    *out = p;
  return true;
}
bool Shape::assignPath(SceneObject &o, const QPainterPath &p) {
  if (p.isEmpty())
    return false;
  auto bounds = p.boundingRect();
  if (bounds.width() < .001)
    bounds.setWidth(1);
  if (bounds.height() < .001)
    bounds.setHeight(1);
  QTransform t;
  t.scale(1 / bounds.width(), 1 / bounds.height());
  t.translate(-bounds.x(), -bounds.y());
  o.pathWinding = p.fillRule() == Qt::WindingFill;
  o.pathData = encode(t.map(p));
  o.rect = bounds;
  o.rotation = 0;
  o.shapeKind = custom;
  o.cornerRadius = 0;
  return true;
}
QVector<SceneObject> Shape::combine(const QVector<SceneObject> &objects,
                                    int operation) {
  if (objects.size() < 2 || objects.size() > 100 || operation < 0 ||
      operation > 3)
    return {};
  for (const auto &o : objects)
    if (o.type != ObjectType::Rect || o.connector || o.locked || o.hidden)
      return {};
  QVector<QPainterPath> pieces{worldPath(objects.first())};
  for (int i = 1; i < objects.size(); ++i) {
    const auto next = worldPath(objects[i]);
    if (operation == 0)
      pieces[0] = pieces[0].united(next);
    else if (operation == 1)
      pieces[0] = pieces[0].intersected(next);
    else if (operation == 2)
      pieces[0] = pieces[0].subtracted(next);
    else {
      QPainterPath covered;
      QVector<QPainterPath> split;
      for (const auto &piece : pieces) {
        covered = covered.united(piece);
        const auto inside = piece.intersected(next),
                   outside = piece.subtracted(next);
        if (!outside.isEmpty())
          split.append(outside);
        if (!inside.isEmpty())
          split.append(inside);
      }
      const auto remainder = next.subtracted(covered);
      if (!remainder.isEmpty())
        split.append(remainder);
      pieces = split;
      if (pieces.size() > 512)
        return {};
    }
  }
  QVector<SceneObject> result;
  for (const auto &piece : pieces) {
    SceneObject o = objects.first();
    if (assignPath(o, piece))
      result.append(o);
  }
  return result;
}
