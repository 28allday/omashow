#pragma once
#include "core/tabledata.h"
#include <QVariantMap>
struct DataSource {
  QString path, fileHash, dataHash;
  int delimiter = 0;
  bool operator==(const DataSource &other) const {
    return path == other.path && fileHash == other.fileHash &&
           dataHash == other.dataHash && delimiter == other.delimiter;
  }
};
namespace LinkedData {
QVariantMap encode(const DataSource &source);
bool decode(const QVariant &value, DataSource &source);
bool validate(const DataSource &source);
QString digest(const TableData &table);
TableData replace(const TableData &table, const QVector<QStringList> &rows,
                  bool chart);
struct File {
  QString text, path, name, hash, error;
  QByteArray bytes;
};
File read(const QString &path);
} // namespace LinkedData
