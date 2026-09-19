#pragma once
#include <QColor>
#include <QMap>
#include <QStringList>
#include <QVector>
struct ChartData {
  // Column, bar, stacked column/bar, line, area, stacked area, pie, donut,
  // scatter.
  int kind = 0;
  QString title = QStringLiteral("Chart title"), xTitle, yTitle;
  QString locale = QStringLiteral("en_GB");
  bool legend = true, grid = true, labels = false, includeZero = true;
  bool logarithmic = false, manualY = false, manualX = false;
  qreal minimumY = 0, maximumY = 100, minimumX = 0, maximumX = 100;
  int numberFormat = 0, decimals = 1, palette = 0;
  QString currency = QStringLiteral("£");
  QMap<QString, QColor> seriesColors; // stable header-cell identities
  QVector<QColor> resolvedColors;     // theme-derived, not serialised
};
