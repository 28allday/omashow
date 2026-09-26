#include "core/datasource.h"
#include "core/table.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringDecoder>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
QVariantMap LinkedData::encode(const DataSource &s) {
  if (s.path.isEmpty())
    return {};
  return {{"path", s.path},
          {"fileHash", s.fileHash},
          {"dataHash", s.dataHash},
          {"delimiter", s.delimiter}};
}
bool LinkedData::validate(const DataSource &s) {
  if (s.path.isEmpty())
    return s.fileHash.isEmpty() && s.dataHash.isEmpty() && s.delimiter == 0;
  static const QRegularExpression hash("^[a-f0-9]{64}$");
  return s.path.size() <= 4096 && !s.path.contains(QChar(0)) &&
         QDir::isAbsolutePath(s.path) && QDir::cleanPath(s.path) == s.path &&
         hash.match(s.fileHash).hasMatch() &&
         hash.match(s.dataHash).hasMatch() &&
         (s.delimiter == 9 || s.delimiter == 44 || s.delimiter == 59);
}
bool LinkedData::decode(const QVariant &value, DataSource &source) {
  if (value.metaType().id() != QMetaType::QVariantMap)
    return false;
  const auto map = value.toMap();
  if (map.isEmpty()) {
    source = {};
    return true;
  }
  if (map.size() != 4)
    return false;
  for (const auto &key : {"path", "fileHash", "dataHash"})
    if (map.value(key).metaType().id() != QMetaType::QString)
      return false;
  const auto d = map.value("delimiter");
  if (d.metaType().id() != QMetaType::Int &&
      d.metaType().id() != QMetaType::Double &&
      d.metaType().id() != QMetaType::LongLong)
    return false;
  if (d.toDouble() != d.toInt())
    return false;
  DataSource result{map["path"].toString(), map["fileHash"].toString(),
                    map["dataHash"].toString(), d.toInt()};
  if (!validate(result))
    return false;
  source = result;
  return true;
}
QString LinkedData::digest(const TableData &table) {
  QJsonArray rows;
  for (int r = 0; r < table.rows.size(); ++r) {
    QJsonArray row;
    for (int c = 0; c < table.columns.size(); ++c)
      row.append(table.cells[r * table.columns.size() + c].text);
    rows.append(row);
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(
          QJsonDocument(rows).toJson(QJsonDocument::Compact),
          QCryptographicHash::Sha256)
          .toHex());
}
TableData LinkedData::replace(const TableData &table,
                              const QVector<QStringList> &rows, bool chart) {
  if (rows.isEmpty())
    return {};
  auto result = Table::create(rows.size(), rows.first().size());
  if (result.cells.isEmpty())
    return {};
  auto mapping = [&](bool rowAxis) {
    const int length = rowAxis ? rows.size() : rows.first().size(),
              oldLength = rowAxis ? table.rows.size() : table.columns.size();
    QVector<int> map(length, -1);
    auto oldName = [&](int i) {
      return table.cells[rowAxis ? i * table.columns.size() : i].text;
    };
    auto newName = [&](int i) { return rowAxis ? rows[i][0] : rows[0][i]; };
    for (int i = 0; i < length; ++i) {
      if (!chart || i == 0) {
        if (i < oldLength)
          map[i] = i;
        continue;
      }
      const auto name = newName(i);
      if (name.isEmpty())
        continue;
      int match = -1, oldCount = 0, newCount = 0;
      for (int j = 1; j < oldLength; ++j)
        if (oldName(j) == name) {
          match = j;
          ++oldCount;
        }
      for (int j = 1; j < length; ++j)
        if (newName(j) == name)
          ++newCount;
      if (oldCount == 1 && newCount == 1)
        map[i] = match;
      else if (i < oldLength && oldName(i) == name)
        map[i] = i;
    }
    return map;
  };
  const auto rr = mapping(true), cc = mapping(false);
  result.headerRows = table.headerRows;
  result.headerColumns = table.headerColumns;
  result.banded = table.banded;
  result.padding = table.padding;
  result.borderWidth = table.borderWidth;
  result.borderColor = table.borderColor;
  result.headerFill = table.headerFill;
  result.borderColorToken = table.borderColorToken;
  result.headerFillToken = table.headerFillToken;
  for (int r = 0; r < rows.size(); ++r) {
    if (rr[r] >= 0)
      result.rows[r] = table.rows[rr[r]];
    for (int c = 0; c < rows[r].size(); ++c) {
      if (cc[c] >= 0)
        result.columns[c] = table.columns[cc[c]];
      auto &cell = result.cells[r * result.columns.size() + c];
      if (rr[r] >= 0 && cc[c] >= 0)
        cell = table.cells[rr[r] * table.columns.size() + cc[c]];
      cell.rowSpan = cell.columnSpan = 1;
      cell.text = rows[r][c];
    }
  }
  return result;
}
LinkedData::File LinkedData::read(const QString &path) {
  File result;
  result.name = QFileInfo(path).fileName();
  // Open nonblocking without following the final symlink, then validate the
  // opened descriptor. A file swapped for a FIFO cannot block the worker.
  const int fd = ::open(QFile::encodeName(path).constData(),
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  struct stat before{};
  if (fd < 0 || ::fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) ||
      before.st_size > 4 * 1024 * 1024) {
    if (fd >= 0)
      ::close(fd);
    result.error =
        "Choose an available regular UTF-8 CSV/TSV file up to 4 MiB.";
    return result;
  }
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    result.error = "The data file could not be opened.";
    return result;
  }
  const auto bytes = file.read(4 * 1024 * 1024 + 1);
  struct stat after{}, named{};
  if (file.error() != QFileDevice::NoError || bytes.size() != before.st_size ||
      ::fstat(fd, &after) != 0 ||
      ::lstat(QFile::encodeName(path).constData(), &named) != 0 ||
      named.st_dev != before.st_dev || named.st_ino != before.st_ino ||
      after.st_size != before.st_size ||
      after.st_mtim.tv_sec != before.st_mtim.tv_sec ||
      after.st_mtim.tv_nsec != before.st_mtim.tv_nsec ||
      after.st_ctim.tv_sec != before.st_ctim.tv_sec ||
      after.st_ctim.tv_nsec != before.st_ctim.tv_nsec) {
    result.error = "The data file changed while reading. Try again.";
    return result;
  }
  QStringDecoder decoder(QStringDecoder::Utf8);
  result.text = decoder(bytes);
  if (decoder.hasError() || result.text.contains(QChar(0))) {
    result.error = "The file is not valid UTF-8 text. Save it as UTF-8 CSV or "
                   "TSV and try again.";
    return result;
  }
  result.path = QFileInfo(path).canonicalFilePath();
  result.bytes = bytes;
  result.hash = QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
  return result;
}
