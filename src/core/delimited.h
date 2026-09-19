#pragma once
#include <QStringList>
#include <QVector>
namespace Delimited {
struct Result {
  QVector<QStringList> rows;
  QString error;
  QChar delimiter;
  bool ok() const { return error.isEmpty() && !rows.isEmpty(); }
};
Result parse(const QString &text, QChar delimiter = QChar());
QString write(const QVector<QStringList> &rows, QChar delimiter = '\t');
} // namespace Delimited
