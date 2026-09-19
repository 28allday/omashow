#include "tablemodel.h"
#include "backend.h"
#include "core/chart.h"
#include "core/delimited.h"
#include "core/design.h"
#include "core/table.h"
#include "filepicker.h"
#include <QClipboard>
#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QStringDecoder>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
TableModel::TableModel(Backend *backend)
    : QAbstractTableModel(backend), m_backend(backend) {
  connect(backend, &Backend::selectionChanged, this, &TableModel::refresh);
  connect(backend, &Backend::currentSlideChanged, this, &TableModel::refresh);
  refresh();
}
int TableModel::rowCount(const QModelIndex &p) const {
  return p.isValid() ? 0 : m_object.table.rows.size();
}
int TableModel::columnCount(const QModelIndex &p) const {
  return p.isValid() ? 0 : m_object.table.columns.size();
}
QHash<int, QByteArray> TableModel::roleNames() const {
  return {{Qt::DisplayRole, "display"}, {CellId, "cellId"},
          {CellText, "cellText"},       {CellAddress, "cellAddress"},
          {CoveredBy, "coveredBy"},     {SpanRows, "spanRows"},
          {SpanColumns, "spanColumns"}, {Header, "isHeader"},
          {Selected, "inSelection"}};
}
QVariant TableModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() ||
      index.column() < 0 || index.column() >= columnCount())
    return {};
  const int k = index.row() * columnCount() + index.column();
  const auto &cell = m_object.table.cells[k];
  const int a = Table::anchor(m_object.table, index.row(), index.column());
  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
  case CellText:
    return cell.text;
  case CellId:
    return cell.id;
  case CellAddress:
    return Table::address(index.row(), index.column());
  case CoveredBy:
    return a == k ? QString()
                  : Table::address(a / columnCount(), a % columnCount());
  case SpanRows:
    return cell.rowSpan;
  case SpanColumns:
    return cell.columnSpan;
  case Header:
    return index.row() < m_object.table.headerRows ||
           index.column() < m_object.table.headerColumns;
  case Selected:
    return range().contains(index.column(), index.row());
  case Qt::AccessibleTextRole:
    return QString("%1, row %2, column %3. %4%5")
        .arg(Table::address(index.row(), index.column()))
        .arg(index.row() + 1)
        .arg(index.column() + 1)
        .arg(a == k ? cell.text
                    : QString("Merged with %1")
                          .arg(Table::address(a / columnCount(),
                                              a % columnCount())))
        .arg(index.row() < m_object.table.headerRows ? ". Column header" : "");
  default:
    return {};
  }
}
QVariant TableModel::headerData(int section, Qt::Orientation orientation,
                                int role) const {
  if (role != Qt::DisplayRole && role != Qt::AccessibleTextRole)
    return {};
  return orientation == Qt::Vertical ? QString::number(section + 1)
                                     : Table::address(0, section).chopped(1);
}
Qt::ItemFlags TableModel::flags(const QModelIndex &index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  auto f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if (!m_object.locked && data(index, CoveredBy).toString().isEmpty())
    f |= Qt::ItemIsEditable;
  return f;
}
bool TableModel::setData(const QModelIndex &index, const QVariant &value,
                         int role) {
  return role == Qt::EditRole && flags(index).testFlag(Qt::ItemIsEditable) &&
         commitText(data(index, CellId).toString(), value.toString(),
                    data(index, CellText).toString());
}
QRect TableModel::range() const {
  return QRect(
      QPoint(qMin(m_column, m_originColumn), qMin(m_row, m_originRow)),
      QPoint(qMax(m_column, m_originColumn), qMax(m_row, m_originRow)));
}
QVariantMap TableModel::selection() const {
  const int k = Table::anchor(m_object.table, m_row, m_column);
  if (k < 0)
    return {};
  const auto &cell = m_object.table.cells[k];
  auto map = cell.style;
  const auto r = range();
  const auto text =
      Table::textObject(m_object, k / columnCount(), k % columnCount());
  for (const auto &key :
       {"fill", "textColor", "fontFamily", "fontSize", "fontWeight", "italic",
        "underline", "textAlign", "verticalAlign", "textFit"})
    if (!map.contains(key))
      map[key] = Design::properties(text).value(key);
  map["padding"] = cell.style.value("padding", m_object.table.padding);
  map["borderWidth"] =
      cell.style.value("borderWidth", m_object.table.borderWidth);
  map["borderColor"] =
      cell.style.value("borderColor", m_object.table.borderColor.name());
  map["id"] = cell.id;
  map["objectId"] = m_object.id;
  map["text"] = cell.text;
  map["address"] = Table::address(k / columnCount(), k % columnCount());
  map["row"] = m_row;
  map["column"] = m_column;
  map["top"] = r.top();
  map["left"] = r.left();
  map["bottom"] = r.bottom();
  map["right"] = r.right();
  map["range"] = Table::address(r.top(), r.left()) + ":" +
                 Table::address(r.bottom(), r.right());
  map["merged"] = cell.rowSpan > 1 || cell.columnSpan > 1;
  map["rowSize"] = m_object.table.rows.value(m_row);
  map["columnSize"] = m_object.table.columns.value(m_column);
  return map;
}
QVariantMap TableModel::summary() const {
  return {{"id", m_object.id},
          {"chart", m_object.type == ObjectType::Chart},
          {"chartIssues", m_chartIssues},
          {"chartWarnings", m_chartWarnings},
          {"sourcePath", m_object.dataSource.path},
          {"sourceEdited", !m_object.dataSource.path.isEmpty() &&
                               LinkedData::digest(m_object.table) !=
                                   m_object.dataSource.dataHash},
          {"loadingData", m_loadingData},
          {"candidatePath", candidateCurrent() ? m_candidate.path : QString()},
          {"candidateRefresh", candidateCurrent() && m_candidateRefresh},
          {"sourceDelimiter", m_object.dataSource.delimiter},
          {"rows", rowCount()},
          {"columns", columnCount()},
          {"headerRows", m_object.table.headerRows},
          {"headerColumns", m_object.table.headerColumns},
          {"banded", m_object.table.banded},
          {"locked", m_object.locked},
          {"overflow", m_overflow}};
}
void TableModel::refresh() {
  const auto selected = m_backend->selectedId();
  const auto slide =
      Design::resolve(m_backend->document(), m_backend->currentSlide());
  const auto *o =
      m_backend->selectionCount() == 1 ? slide.find(selected) : nullptr;
  const SceneObject next =
      o && (o->type == ObjectType::Table || o->type == ObjectType::Chart)
          ? *o
          : SceneObject();
  const bool identity = next.id != m_object.id || slide.id != m_slideId;
  if (identity || (m_candidateRevision >= 0 &&
                   m_candidateRevision != m_backend->revision())) {
    ++m_dataRequest;
    m_candidate = {};
    m_candidateRevision = -1;
    m_candidateRefresh = false;
    m_loadingData = false;
  }
  bool reset = identity || next.table.rows.size() != rowCount() ||
               next.table.columns.size() != columnCount();
  if (!reset)
    for (int i = 0; i < next.table.cells.size(); ++i)
      if (next.table.cells[i].id != m_object.table.cells[i].id) {
        reset = true;
        break;
      }
  QString cellId;
  int k = Table::anchor(m_object.table, m_row, m_column);
  if (k >= 0)
    cellId = m_object.table.cells[k].id;
  if (reset)
    beginResetModel();
  m_object = next;
  m_chartIssues.clear();
  m_chartWarnings.clear();
  if (m_object.type == ObjectType::Chart) {
    const auto layout = Chart::layout(m_object);
    m_chartIssues = layout.issues;
    m_chartWarnings = layout.warnings;
  }
  m_overflow =
      m_object.type == ObjectType::Table ? Table::overflowCount(m_object) : 0;
  m_slideId = slide.id;
  if (identity)
    m_row = m_column = m_originRow = m_originColumn = 0;
  else if (reset) {
    for (int i = 0; i < m_object.table.cells.size(); ++i)
      if (m_object.table.cells[i].id == cellId) {
        m_row = i / columnCount();
        m_column = i % columnCount();
        break;
      }
    m_originRow = m_row;
    m_originColumn = m_column;
  }
  m_row = qBound(0, m_row, qMax(0, rowCount() - 1));
  m_column = qBound(0, m_column, qMax(0, columnCount() - 1));
  m_originRow = qBound(0, m_originRow, qMax(0, rowCount() - 1));
  m_originColumn = qBound(0, m_originColumn, qMax(0, columnCount() - 1));
  if (reset)
    endResetModel();
  else if (rowCount() && columnCount())
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
  emit selectionChanged();
}
void TableModel::selectCell(int row, int column, bool extend) {
  const int k = Table::anchor(m_object.table, row, column);
  if (k < 0)
    return;
  m_row = extend ? row : k / columnCount();
  m_column = extend ? column : k % columnCount();
  if (!extend) {
    m_originRow = m_row;
    m_originColumn = m_column;
  }
  if (rowCount() && columnCount())
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                     {Selected});
  emit selectionChanged();
}
void TableModel::moveCell(int horizontal, int vertical, bool extend) {
  if (!rowCount() || !columnCount())
    return;
  int r = m_row, c = m_column;
  const int current = Table::anchor(m_object.table, r, c);
  for (int count = 0; count < rowCount() * columnCount(); ++count) {
    c += horizontal;
    r += vertical;
    if (horizontal && c >= columnCount()) {
      c = 0;
      ++r;
    }
    if (horizontal && c < 0) {
      c = columnCount() - 1;
      --r;
    }
    if (r < 0 || r >= rowCount() || c < 0 || c >= columnCount())
      return;
    if (extend || Table::anchor(m_object.table, r, c) != current) {
      selectCell(r, c, extend);
      return;
    }
  }
}
bool TableModel::apply(const TableData &table, const QString &label) {
  if ((m_object.type != ObjectType::Table &&
       m_object.type != ObjectType::Chart) ||
      m_object.locked || !Table::validate(table) || table.rows.isEmpty())
    return false;
  return m_backend->applyTable(m_slideId, m_object.id, table, label);
}
bool TableModel::commitText(const QString &id, const QString &text,
                            const QString &expected) {
  auto table = m_object.table;
  for (auto &cell : table.cells)
    if (cell.id == id && cell.rowSpan > 0) {
      if (cell.text != expected || text.size() > 65536)
        return false;
      if (text == cell.text)
        return true;
      cell.text = text;
      return apply(table, tr("Edit table cell"));
    }
  return false;
}
bool TableModel::clearRange() {
  auto t = m_object.table;
  const auto region = range();
  for (int r = region.top(); r <= region.bottom(); ++r)
    for (int c = region.left(); c <= region.right(); ++c) {
      int k = Table::anchor(t, r, c);
      if (k >= 0)
        t.cells[k].text.clear();
    }
  return apply(t, tr("Clear table range"));
}
bool TableModel::distribute(bool rows) {
  auto t = m_object.table;
  auto &axis = rows ? t.rows : t.columns;
  for (auto &size : axis)
    size = 1;
  return apply(t, rows ? tr("Distribute table rows")
                       : tr("Distribute table columns"));
}
bool TableModel::mergeCells() {
  if (m_object.type == ObjectType::Chart)
    return false;
  auto t = m_object.table;
  const auto r = range();
  return Table::merge(t, r.top(), r.left(), r.bottom(), r.right()) &&
         apply(t, tr("Merge table cells"));
}
bool TableModel::splitCell() {
  auto t = m_object.table;
  return Table::split(t, m_row, m_column) && apply(t, tr("Split table cell"));
}
bool TableModel::changeAxis(bool rows, int index, bool remove) {
  if (m_object.type == ObjectType::Chart && index == 0)
    return false;
  auto t = m_object.table;
  return Table::changeAxis(t, rows, index, remove) &&
         apply(
             t,
             remove
                 ? tr("Delete table %1").arg(rows ? tr("row") : tr("column"))
                 : tr("Insert table %1").arg(rows ? tr("row") : tr("column")));
}
bool TableModel::setAxisSize(bool rows, int index, qreal size) {
  auto t = m_object.table;
  auto &axis = rows ? t.rows : t.columns;
  if (index < 0 || index >= axis.size() || !std::isfinite(size) || size < .01 ||
      size > 1000)
    return false;
  axis[index] = size;
  return apply(t, tr("Resize table %1").arg(rows ? tr("row") : tr("column")));
}
bool TableModel::formatCells(const QString &key, const QVariant &value) {
  auto t = m_object.table;
  const auto r = range();
  return Table::setStyle(t, r.top(), r.left(), r.bottom(), r.right(), key,
                         value) &&
         apply(t, tr("Format table cells"));
}
bool TableModel::setTableOption(const QString &key, const QVariant &value) {
  if (m_object.type == ObjectType::Chart)
    return false;
  auto t = m_object.table;
  if (key == "headerRows")
    t.headerRows = value.toBool() ? 1 : 0;
  else if (key == "headerColumns")
    t.headerColumns = value.toBool() ? 1 : 0;
  else if (key == "banded")
    t.banded = value.toBool();
  else
    return false;
  return apply(t, tr("Format table"));
}
QVariantMap TableModel::previewPaste(const QString &text, int delimiter) const {
  const auto parsed =
      Delimited::parse(text, delimiter ? QChar(delimiter) : QChar());
  QVariantList preview;
  for (int r = 0; r < qMin(6, int(parsed.rows.size())); ++r)
    preview.append(parsed.rows[r].join("  |  "));
  QString error = parsed.error;
  const auto region = range();
  if (parsed.ok()) {
    const int rows = parsed.rows.size(), cols = parsed.rows.first().size();
    if (region.top() + rows > Table::maxRows ||
        region.left() + cols > Table::maxColumns)
      error = tr("The pasted range exceeds 100 rows or 50 columns.");
    for (int r = region.top(); r < qMin(rowCount(), region.top() + rows); ++r)
      for (int c = region.left(); c < qMin(columnCount(), region.left() + cols);
           ++c) {
        const auto &cell = m_object.table.cells[r * columnCount() + c];
        if (cell.rowSpan != 1 || cell.columnSpan != 1)
          error = tr("Split merged cells in the destination before pasting.");
      }
  }
  return {{"ok", error.isEmpty() && parsed.ok()},
          {"error", error},
          {"rows", parsed.rows.size()},
          {"columns", parsed.rows.isEmpty() ? 0 : parsed.rows.first().size()},
          {"preview", preview},
          {"delimiter", int(parsed.delimiter.unicode())}};
}
bool TableModel::pasteText(const QString &text, int delimiter) {
  if (!previewPaste(text, delimiter).value("ok").toBool())
    return false;
  const auto parsed =
      Delimited::parse(text, delimiter ? QChar(delimiter) : QChar());
  auto t = m_object.table;
  const auto r = range();
  while (t.rows.size() < r.top() + parsed.rows.size())
    if (!Table::changeAxis(t, true, t.rows.size(), false))
      return false;
  while (t.columns.size() < r.left() + parsed.rows.first().size())
    if (!Table::changeAxis(t, false, t.columns.size(), false))
      return false;
  for (int y = 0; y < parsed.rows.size(); ++y)
    for (int x = 0; x < parsed.rows[y].size(); ++x)
      t.cells[(r.top() + y) * t.columns.size() + r.left() + x].text =
          parsed.rows[y][x];
  return apply(t, tr("Paste table range"));
}
QString TableModel::copyText() const {
  QVector<QStringList> rows;
  const auto r = range();
  if (!rowCount() || !columnCount())
    return {};
  for (int y = r.top(); y <= r.bottom(); ++y) {
    QStringList row;
    for (int x = r.left(); x <= r.right(); ++x)
      row.append(m_object.table.cells[y * columnCount() + x].text);
    rows.append(row);
  }
  return Delimited::write(rows);
}
void TableModel::copyRange() const {
  QGuiApplication::clipboard()->setText(copyText());
}
QString TableModel::clipboardText() const {
  return QGuiApplication::clipboard()->text();
}

QVector<int> TableModel::sortedRows(int column, bool ascending, bool numeric,
                                    const QString &locale,
                                    QString &error) const {
  QVector<int> order;
  if (column < 0 || column >= columnCount()) {
    error = tr("Select a sort column.");
    return order;
  }
  const auto &t = m_object.table;
  for (const auto &cell : t.cells)
    if (cell.rowSpan != 1 || cell.columnSpan != 1) {
      error = tr("Split merged cells before sorting rows.");
      return order;
    }
  if (locale != "en_GB" && locale != "de_DE" && locale != "fr_FR") {
    error = tr("Choose a supported number locale.");
    return order;
  }
  QLocale numbers(locale);
  QCollator collator(numbers);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(false);
  QMap<int, qreal> values;
  for (int r = t.headerRows; r < rowCount(); ++r) {
    const auto text = t.cells[r * columnCount() + column].text.trimmed();
    order.append(r);
    if (numeric && !text.isEmpty()) {
      bool ok = false;
      qreal value = numbers.toDouble(text, &ok);
      if (!ok || !std::isfinite(value)) {
        error = tr("%1 is not a number in the chosen locale.")
                    .arg(Table::address(r, column));
        return {};
      }
      values[r] = value;
    }
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const auto left = t.cells[a * columnCount() + column].text.trimmed(),
               right = t.cells[b * columnCount() + column].text.trimmed();
    if (left.isEmpty() || right.isEmpty())
      return !left.isEmpty() && right.isEmpty();
    const int comparison = numeric ? (values[a] < values[b]   ? -1
                                      : values[a] > values[b] ? 1
                                                              : 0)
                                   : collator.compare(left, right);
    return ascending ? comparison < 0 : comparison > 0;
  });
  return order;
}
QVariantMap TableModel::previewSort(int column, bool ascending, bool numeric,
                                    const QString &locale) const {
  QString error;
  const auto order = sortedRows(column, ascending, numeric, locale, error);
  QStringList labels;
  for (int i = 0; i < qMin(8, int(order.size())); ++i) {
    const auto &t = m_object.table;
    labels.append(QString("%1 · %2")
                      .arg(order[i] + 1)
                      .arg(t.cells[order[i] * columnCount() + column].text));
  }
  return {{"ok", error.isEmpty()},
          {"error", error},
          {"preview", labels},
          {"rows", order.size()}};
}
bool TableModel::sortRows(int column, bool ascending, bool numeric,
                          const QString &locale) {
  QString error;
  const auto order = sortedRows(column, ascending, numeric, locale, error);
  if (!error.isEmpty())
    return false;
  auto t = m_object.table;
  auto old = t;
  for (int i = 0; i < order.size(); ++i) {
    const int row = t.headerRows + i;
    t.rows[row] = old.rows[order[i]];
    for (int c = 0; c < columnCount(); ++c)
      t.cells[row * columnCount() + c] =
          old.cells[order[i] * columnCount() + c];
  }
  return apply(t, tr("Sort table rows"));
}
void TableModel::importDataDialog() {
  if ((m_object.type != ObjectType::Table &&
       m_object.type != ObjectType::Chart) ||
      m_loadingData)
    return;
  if (!m_dataChooser) {
    m_dataChooser = new PortalFileChooser(this);
    connect(m_dataChooser, &PortalFileChooser::selected, this,
            [this](const QUrl &url) {
              if (m_pickerRevision == m_backend->revision() &&
                  m_pickerObject == m_object.id && m_pickerSlide == m_slideId)
                loadDataFile(url);
            });
    connect(m_dataChooser, &PortalFileChooser::failed, this,
            &TableModel::dataFileFailed);
  }
  m_pickerRevision = m_backend->revision();
  m_pickerObject = m_object.id;
  m_pickerSlide = m_slideId;
  m_dataChooser->openFile(tr("Import table data"), tr("CSV / TSV data"),
                          {"*.csv", "*.tsv", "*.txt", "*.CSV", "*.TSV"});
}
bool TableModel::loadDataFile(const QUrl &url) {
  return readDataFile(url, false);
}
bool TableModel::refreshDataFile() {
  return !m_object.dataSource.path.isEmpty() &&
         readDataFile(QUrl::fromLocalFile(m_object.dataSource.path), true);
}
void TableModel::cancelDataFile() {
  ++m_dataRequest;
  m_loadingData = false;
  m_candidate = {};
  m_candidateRevision = -1;
  m_candidateRefresh = false;
  emit selectionChanged();
}
bool TableModel::candidateCurrent() const {
  return !m_candidate.path.isEmpty() &&
         m_candidateRevision == m_backend->revision() &&
         m_candidateObject == m_object.id && m_candidateSlide == m_slideId;
}
bool TableModel::readDataFile(const QUrl &url, bool refresh) {
  if (!url.isLocalFile() || m_loadingData || m_object.locked ||
      (m_object.type != ObjectType::Table &&
       m_object.type != ObjectType::Chart))
    return false;
  cancelDataFile();
  const int revision = m_backend->revision(), request = ++m_dataRequest;
  const auto id = m_object.id, slideId = m_slideId;
  m_loadingData = true;
  emit selectionChanged();
  auto *watcher = new QFutureWatcher<LinkedData::File>(this);
  connect(watcher, &QFutureWatcher<LinkedData::File>::finished, this,
          [this, watcher, revision, request, id, slideId, refresh] {
            const auto result = watcher->result();
            watcher->deleteLater();
            if (request != m_dataRequest)
              return;
            m_loadingData = false;
            if (revision != m_backend->revision() || id != m_object.id ||
                slideId != m_slideId) {
              emit selectionChanged();
              return;
            }
            if (!result.error.isEmpty()) {
              emit selectionChanged();
              emit dataFileFailed(result.error);
              return;
            }
            m_candidate = result;
            m_candidateRevision = revision;
            m_candidateObject = id;
            m_candidateSlide = slideId;
            m_candidateRefresh = refresh;
            emit selectionChanged();
            emit dataFileReady(result.text, result.name);
          });
  watcher->setFuture(QtConcurrent::run(
      [path = url.toLocalFile()] { return LinkedData::read(path); }));
  return true;
}
QVariantMap TableModel::previewDataFile(const QString &text, int delimiter,
                                        bool linked) const {
  if (!candidateCurrent())
    return {
        {"ok", false},
        {"error", tr("This file preview has expired. Read the file again.")}};
  if (!linked)
    return previewPaste(text, delimiter);
  QString error;
  if (text != m_candidate.text)
    error = tr("Linked data must match the file. Revert the preview text or "
               "import without linking.");
  const auto parsed =
      Delimited::parse(text, delimiter ? QChar(delimiter) : QChar());
  if (error.isEmpty())
    error = parsed.error;
  if (error.isEmpty() && m_object.type == ObjectType::Chart &&
      (parsed.rows.size() < 2 || parsed.rows.first().size() < 2))
    error = tr("A chart needs a header row, a category column and at least one "
               "value.");
  for (const auto &cell : m_object.table.cells)
    if (error.isEmpty() && (cell.rowSpan != 1 || cell.columnSpan != 1))
      error = tr("Split merged cells before replacing the linked grid.");
  QVariantList preview;
  for (int r = 0; r < qMin(6, int(parsed.rows.size())); ++r)
    preview.append(parsed.rows[r].join("  |  "));
  int changed = 0;
  const int columns = parsed.rows.isEmpty() ? 0 : parsed.rows.first().size();
  for (int r = 0; r < parsed.rows.size(); ++r)
    for (int c = 0; c < columns; ++c)
      if (r >= rowCount() || c >= columnCount() ||
          m_object.table.cells[r * columnCount() + c].text != parsed.rows[r][c])
        ++changed;
  return {{"ok", error.isEmpty() && parsed.ok() && !m_object.locked},
          {"error", error},
          {"rows", parsed.rows.size()},
          {"columns", columns},
          {"preview", preview},
          {"changed", changed},
          {"oldRows", rowCount()},
          {"oldColumns", columnCount()},
          {"fileChanged", m_candidate.hash != m_object.dataSource.fileHash}};
}
bool TableModel::applyDataFile(const QString &text, int delimiter,
                               bool linked) {
  if (!candidateCurrent())
    return false;
  if (!linked)
    return pasteText(text, delimiter);
  if (!previewDataFile(text, delimiter, true).value("ok").toBool())
    return false;
  const auto parsed =
      Delimited::parse(text, delimiter ? QChar(delimiter) : QChar());
  const auto table = LinkedData::replace(m_object.table, parsed.rows,
                                         m_object.type == ObjectType::Chart);
  const DataSource source{m_candidate.path, m_candidate.hash,
                          LinkedData::digest(table),
                          parsed.delimiter.unicode()};
  const auto label =
      m_candidateRefresh ? tr("Refresh linked data") : tr("Link CSV data");
  if (!m_backend->applyTable(m_slideId, m_object.id, table, label, &source))
    return false;
  cancelDataFile();
  return true;
}
bool TableModel::disconnectDataFile() {
  if (m_object.dataSource.path.isEmpty())
    return false;
  const DataSource source;
  const bool result =
      m_backend->applyTable(m_slideId, m_object.id, m_object.table,
                            tr("Disconnect linked data"), &source);
  if (result)
    cancelDataFile();
  return result;
}

bool TableModel::moveAxis(bool rows, int from, int to) {
  const int length = rows ? rowCount() : columnCount();
  if (from < 0 || to < 0 || from >= length || to >= length || from == to)
    return false;
  if (m_object.type == ObjectType::Chart && (from == 0 || to == 0))
    return false;
  auto t = m_object.table;
  for (const auto &cell : t.cells)
    if (cell.rowSpan != 1 || cell.columnSpan != 1)
      return false;
  QVector<int> order;
  for (int i = 0; i < length; ++i)
    order.append(i);
  order.move(from, to);
  const auto old = t;
  auto &axis = rows ? t.rows : t.columns;
  const auto sizes = axis;
  for (int i = 0; i < length; ++i)
    axis[i] = sizes[order[i]];
  for (int r = 0; r < rowCount(); ++r)
    for (int c = 0; c < columnCount(); ++c)
      t.cells[r * columnCount() + c] =
          old.cells[(rows ? order[r] : r) * columnCount() +
                    (rows ? c : order[c])];
  return apply(t, rows ? tr("Move table row") : tr("Move table column"));
}
