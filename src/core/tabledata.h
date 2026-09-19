#pragma once
#include <QColor>
#include <QVariantMap>
#include <QVector>

struct TableCell {
  QString id, text;
  int rowSpan = 1, columnSpan = 1; // both zero for a covered cell
  QVariantMap style; // explicit cell overrides; absent keys inherit the table
};
struct TableData {
  QVector<qreal> rows, columns; // relative sizes, normalised at layout time
  QVector<TableCell> cells;     // row major, including covered cells
  int headerRows = 1, headerColumns = 0;
  bool banded = true;
  qreal padding = 12, borderWidth = 1;
  QColor borderColor = QColor("#6b7280"), headerFill = QColor("#27c2ff");
  QString headerFillToken = QStringLiteral("accent"),
          borderColorToken = QStringLiteral("muted");
};
