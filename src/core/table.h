#pragma once
#include "core/scene.h"
class QPainter;
namespace Table {
constexpr int maxRows = 100, maxColumns = 50, maxText = 1000000;
TableData create(int rows, int columns);
QVariantMap encode(const TableData &table);
bool decode(const QVariant &value, TableData &table);
bool validate(const TableData &table);
int anchor(const TableData &table, int row, int column);
QString address(int row, int column);
QRectF cellRect(const SceneObject &object, int row, int column);
SceneObject textObject(const SceneObject &object, int row, int column);
void paint(QPainter &painter, const SceneObject &object);
int overflowCount(const SceneObject &object);
bool merge(TableData &table, int top, int left, int bottom, int right);
bool split(TableData &table, int row, int column);
bool changeAxis(TableData &table, bool rows, int index, bool remove);
bool setStyle(TableData &table, int top, int left, int bottom, int right,
              const QString &key, const QVariant &value);
void scale(TableData &table, qreal factor);
// The per-cell style keys setStyle accepts, for anything describing them.
QStringList styleKeyNames();
} // namespace Table
