#pragma once
#include "core/scene.h"
#include <QPainterPath>
class QPainter;
namespace Chart {
QStringList names();
QVariantMap encode(const ChartData &chart);
bool decode(const QVariant &value, ChartData &chart);
bool validate(const SceneObject &object, QString *error = nullptr);
QString number(qreal value, const ChartData &chart);
QVector<QColor> palette(const DeckTheme &theme, int variant);
struct Tick {
  qreal value = 0, position = 0;
  QString label;
};
struct Mark {
  enum Kind { Bar, Line, Area, Slice, Point } kind = Bar;
  int series = 0, category = 0;
  qreal value = 0, startAngle = 0, sweepAngle = 0;
  QRectF rect;
  QPainterPath path;
  QColor color;
};
struct Layout {
  QRectF plot;
  QVector<Mark> marks;
  QVector<Tick> xTicks, yTicks;
  QStringList categories, series, issues, warnings;
  qreal minY = 0, maxY = 1, minX = 0, maxX = 1;
  int missing = 0, invalid = 0;
  bool horizontal = false, circular = false;
};
Layout layout(const SceneObject &object);
void paint(QPainter &painter, const SceneObject &object);
} // namespace Chart
