#include "core/table.h"
#include "core/design.h"
#include "core/edit.h"
#include "render/textlayout.h"
#include <QPainter>
#include <QSet>
#include <cmath>
#include <numeric>

namespace {
const QSet<QString> styleKeys = {
    "fill",    "textColor",   "fontFamily", "fontSize",      "fontWeight",
    "italic",  "underline",   "textAlign",  "verticalAlign", "textFit",
    "padding", "borderWidth", "borderColor"};
bool styleValue(const QString &key, const QVariant &value) {
  if (!styleKeys.contains(key))
    return false;
  if (key == "fill" || key == "textColor" || key == "borderColor")
    return QColor(value.toString()).isValid();
  if (key == "fontFamily")
    return !value.toString().isEmpty() && value.toString().size() <= 256;
  if (key == "italic" || key == "underline")
    return value.metaType().id() == QMetaType::Bool;
  bool ok = false;
  const qreal n = value.toDouble(&ok);
  if (!ok || !std::isfinite(n))
    return false;
  if (key == "fontWeight")
    return n >= 100 && n <= 900 && n == int(n);
  if (key == "textAlign")
    return n >= 0 && n <= 3 && n == int(n);
  if (key == "verticalAlign")
    return n >= 0 && n <= 2 && n == int(n);
  if (key == "textFit")
    return n == 0 || n == 1;
  return n >= 0 && n <= 1000000 && (key != "fontSize" || n > 0);
}
bool region(const TableData &t, int top, int left, int bottom, int right) {
  return top >= 0 && left >= 0 && bottom >= top && right >= left &&
         bottom < t.rows.size() && right < t.columns.size();
}
QColor blend(QColor a, QColor b, qreal amount) {
  return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount,
                          a.greenF() * (1 - amount) + b.greenF() * amount,
                          a.blueF() * (1 - amount) + b.blueF() * amount);
}
QColor contrast(QColor c) {
  auto linear = [](qreal v) {
    return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4);
  };
  const qreal l = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) +
                  .0722 * linear(c.blueF());
  return l > .179 ? QColor(Qt::black) : QColor(Qt::white);
}
} // namespace
TableData Table::create(int rowCount, int columnCount) {
  TableData t;
  if (rowCount < 1 || rowCount > maxRows || columnCount < 1 ||
      columnCount > maxColumns)
    return t;
  t.rows.fill(1, rowCount);
  t.columns.fill(1, columnCount);
  for (int i = 0; i < rowCount * columnCount; ++i) {
    TableCell c;
    c.id = Edit::newId("cell");
    t.cells.append(c);
  }
  return t;
}
QVariantMap Table::encode(const TableData &t) {
  QVariantList rows, columns, cells;
  for (auto v : t.rows)
    rows.append(v);
  for (auto v : t.columns)
    columns.append(v);
  for (const auto &c : t.cells)
    cells.append(QVariantMap{{"id", c.id},
                             {"text", c.text},
                             {"rows", c.rowSpan},
                             {"columns", c.columnSpan},
                             {"style", c.style}});
  return {{"rows", rows},
          {"columns", columns},
          {"cells", cells},
          {"headerRows", t.headerRows},
          {"headerColumns", t.headerColumns},
          {"banded", t.banded},
          {"padding", t.padding},
          {"borderWidth", t.borderWidth},
          {"borderColor", t.borderColor.name(QColor::HexArgb)},
          {"headerFill", t.headerFill.name(QColor::HexArgb)},
          {"headerFillToken", t.headerFillToken},
          {"borderColorToken", t.borderColorToken}};
}
bool Table::decode(const QVariant &v, TableData &out) {
  if (v.metaType().id() != QMetaType::QVariantMap)
    return false;
  const auto m = v.toMap();
  TableData t;
  const auto known = encode(t);
  for (auto it = m.cbegin(); it != m.cend(); ++it)
    if (!known.contains(it.key()))
      return false;
  if (m.size() != known.size())
    return false;
  for (const auto &key : {"rows", "columns", "cells"})
    if (m.value(key).metaType().id() != QMetaType::QVariantList)
      return false;
  auto sizes = [](QVariantList list, QVector<qreal> &values, int limit) {
    if (list.size() > limit)
      return false;
    for (const auto &v : list) {
      bool ok = false;
      qreal n = v.toDouble(&ok);
      if (!ok || !std::isfinite(n) || n <= 0 || n > 1000000)
        return false;
      values.append(n);
    }
    return true;
  };
  if (!sizes(m.value("rows").toList(), t.rows, maxRows) ||
      !sizes(m.value("columns").toList(), t.columns, maxColumns))
    return false;
  const auto list = m.value("cells").toList();
  if (list.size() != t.rows.size() * t.columns.size())
    return false;
  for (const auto &v : list) {
    if (v.metaType().id() != QMetaType::QVariantMap)
      return false;
    const auto c = v.toMap();
    if (c.size() != 5 || !c.contains("id") || !c.contains("text") ||
        !c.contains("rows") || !c.contains("columns") || !c.contains("style"))
      return false;
    if (c.value("id").metaType().id() != QMetaType::QString ||
        c.value("text").metaType().id() != QMetaType::QString ||
        c.value("style").metaType().id() != QMetaType::QVariantMap)
      return false;
    bool ok = false;
    const qreal r = c.value("rows").toDouble(&ok);
    if (!ok || !std::isfinite(r) || r < 0 || r > maxRows || r != int(r))
      return false;
    const qreal col = c.value("columns").toDouble(&ok);
    if (!ok || !std::isfinite(col) || col < 0 || col > maxColumns ||
        col != int(col))
      return false;
    t.cells.append({c.value("id").toString(), c.value("text").toString(),
                    int(r), int(col), c.value("style").toMap()});
  }
  for (const auto &key : {"headerRows", "headerColumns"}) {
    bool ok = false;
    qreal n = m.value(key).toDouble(&ok);
    if (!ok || !std::isfinite(n) || n < 0 || n > 1 || n != int(n))
      return false;
  }
  t.headerRows = m.value("headerRows").toInt();
  t.headerColumns = m.value("headerColumns").toInt();
  if (m.value("banded").metaType().id() != QMetaType::Bool)
    return false;
  t.banded = m.value("banded").toBool();
  for (const auto &key : {"padding", "borderWidth"}) {
    bool ok = false;
    qreal n = m.value(key).toDouble(&ok);
    if (!ok || !std::isfinite(n) || n < 0 || n > 1000000)
      return false;
  }
  t.padding = m.value("padding").toDouble();
  t.borderWidth = m.value("borderWidth").toDouble();
  t.borderColor = QColor(m.value("borderColor").toString());
  t.headerFill = QColor(m.value("headerFill").toString());
  t.headerFillToken = m.value("headerFillToken").toString();
  t.borderColorToken = m.value("borderColorToken").toString();
  if (!validate(t))
    return false;
  out = t;
  return true;
}
bool Table::validate(const TableData &t) {
  if (t.rows.isEmpty() || t.columns.isEmpty())
    return t.rows.isEmpty() && t.columns.isEmpty() && t.cells.isEmpty();
  if (t.rows.size() > maxRows || t.columns.size() > maxColumns ||
      t.cells.size() != t.rows.size() * t.columns.size() ||
      !t.borderColor.isValid() || !t.headerFill.isValid())
    return false;
  if (t.headerRows < 0 || t.headerRows > 1 || t.headerColumns < 0 ||
      t.headerColumns > 1 || !std::isfinite(t.padding) || t.padding < 0 ||
      !std::isfinite(t.borderWidth) || t.borderWidth < 0)
    return false;
  for (auto n : t.rows + t.columns)
    if (!std::isfinite(n) || n <= 0 || n > 1000000)
      return false;
  QSet<QString> ids;
  QVector<bool> occupied(t.cells.size(), false);
  int characters = 0, cols = t.columns.size();
  for (int i = 0; i < t.cells.size(); ++i) {
    const auto &c = t.cells[i];
    characters += c.text.size();
    if (c.id.isEmpty() || c.id.size() > 128 || ids.contains(c.id) ||
        c.text.size() > 65536 || characters > maxText)
      return false;
    ids.insert(c.id);
    for (auto it = c.style.cbegin(); it != c.style.cend(); ++it)
      if (!styleValue(it.key(), it.value()))
        return false;
    if (c.rowSpan == 0 && c.columnSpan == 0) {
      if (!occupied[i] || !c.text.isEmpty())
        return false;
      continue;
    }
    const int r = i / cols, col = i % cols;
    if (c.rowSpan < 1 || c.columnSpan < 1 || r + c.rowSpan > t.rows.size() ||
        col + c.columnSpan > cols)
      return false;
    for (int y = r; y < r + c.rowSpan; ++y)
      for (int x = col; x < col + c.columnSpan; ++x) {
        const int k = y * cols + x;
        if (occupied[k])
          return false;
        occupied[k] = true;
        if (k != i && (t.cells[k].rowSpan != 0 || t.cells[k].columnSpan != 0))
          return false;
      }
  }
  return true;
}
int Table::anchor(const TableData &t, int row, int column) {
  if (!region(t, row, column, row, column))
    return -1;
  const int index = row * t.columns.size() + column;
  if (t.cells[index].rowSpan > 0)
    return index;
  for (int r = row; r >= 0; --r)
    for (int c = column; c >= 0; --c) {
      int k = r * t.columns.size() + c;
      const auto &cell = t.cells[k];
      if (r + cell.rowSpan > row && c + cell.columnSpan > column)
        return k;
    }
  return -1;
}
QString Table::address(int row, int column) {
  QString name;
  for (int n = column + 1; n > 0; n = (n - 1) / 26)
    name.prepend(QChar('A' + (n - 1) % 26));
  return name + QString::number(row + 1);
}
QRectF Table::cellRect(const SceneObject &o, int row, int column) {
  const auto &t = o.table;
  const int k = anchor(t, row, column);
  if (k < 0)
    return {};
  row = k / t.columns.size();
  column = k % t.columns.size();
  const auto &cell = t.cells[k];
  const qreal rw = std::accumulate(t.rows.cbegin(), t.rows.cend(), 0.0),
              cw = std::accumulate(t.columns.cbegin(), t.columns.cend(), 0.0);
  const qreal x = std::accumulate(t.columns.cbegin(),
                                  t.columns.cbegin() + column, 0.0),
              y = std::accumulate(t.rows.cbegin(), t.rows.cbegin() + row, 0.0);
  const qreal w = std::accumulate(t.columns.cbegin() + column,
                                  t.columns.cbegin() + column + cell.columnSpan,
                                  0.0),
              h = std::accumulate(t.rows.cbegin() + row,
                                  t.rows.cbegin() + row + cell.rowSpan, 0.0);
  return {o.rect.x() + x / cw * o.rect.width(),
          o.rect.y() + y / rw * o.rect.height(), w / cw * o.rect.width(),
          h / rw * o.rect.height()};
}
SceneObject Table::textObject(const SceneObject &o, int row, int column) {
  const auto &t = o.table;
  int k = anchor(t, row, column);
  if (k < 0)
    return {};
  const auto &cell = t.cells[k];
  row = k / t.columns.size();
  column = k % t.columns.size();
  SceneObject text = o;
  text.table = {};
  text.type = ObjectType::Text;
  text.rotation = 0;
  text.text = cell.text;
  if (row < t.headerRows || column < t.headerColumns) {
    text.fill = t.headerFill;
    text.textColor = contrast(text.fill);
    text.fontWeight = 600;
  } else if (t.banded && (row - t.headerRows) % 2 == 1)
    text.fill = blend(o.fill, o.textColor, .08);
  for (auto it = cell.style.cbegin(); it != cell.style.cend(); ++it)
    if (it.key() != "padding" && it.key() != "borderWidth" &&
        it.key() != "borderColor")
      Design::setProperty(text, it.key(), it.value(), false);
  const qreal padding = cell.style.value("padding", t.padding).toDouble();
  const auto r = cellRect(o, row, column);
  const qreal px = qMin(padding, r.width() / 2),
              py = qMin(padding, r.height() / 2);
  text.rect = r.adjusted(px, py, -px, -py);
  return text;
}
void Table::paint(QPainter &p, const SceneObject &o) {
  const auto &t = o.table;
  if (t.rows.isEmpty() || t.columns.isEmpty())
    return;
  // Fill/text first, then a deduplicated border per elementary grid edge.
  struct Edge {
    QPointF a, b;
    QColor color;
    qreal width = 0;
  };
  QMap<QString, Edge> edges;
  auto edge = [&](QString key, QPointF a, QPointF b, QColor color,
                  qreal width) {
    if (!edges.contains(key) || width >= edges[key].width)
      edges[key] = {a, b, color, width};
  };
  QVector<qreal> xs{o.rect.left()}, ys{o.rect.top()};
  qreal cw = std::accumulate(t.columns.cbegin(), t.columns.cend(), 0.0),
        rh = std::accumulate(t.rows.cbegin(), t.rows.cend(), 0.0);
  for (auto n : t.columns)
    xs.append(xs.last() + n / cw * o.rect.width());
  for (auto n : t.rows)
    ys.append(ys.last() + n / rh * o.rect.height());
  for (int r = 0; r < t.rows.size(); ++r)
    for (int c = 0; c < t.columns.size(); ++c) {
      const auto &cell = t.cells[r * t.columns.size() + c];
      if (!cell.rowSpan)
        continue;
      auto text = textObject(o, r, c);
      p.fillRect(cellRect(o, r, c), text.fill);
      if (!text.text.isEmpty() && !text.rect.isEmpty())
        TextLayout::paint(p, text);
      const QColor color =
          QColor(cell.style.value("borderColor", t.borderColor).toString());
      const qreal width =
          cell.style.value("borderWidth", t.borderWidth).toDouble();
      for (int x = c; x < c + cell.columnSpan; ++x)
        for (int y : {r, r + cell.rowSpan})
          edge(QString("h%1,%2").arg(y).arg(x), {xs[x], ys[y]},
               {xs[x + 1], ys[y]}, color, width);
      for (int y = r; y < r + cell.rowSpan; ++y)
        for (int x : {c, c + cell.columnSpan})
          edge(QString("v%1,%2").arg(y).arg(x), {xs[x], ys[y]},
               {xs[x], ys[y + 1]}, color, width);
    }
  p.save();
  p.setBrush(Qt::NoBrush);
  for (const auto &e : edges)
    if (e.width > 0) {
      p.setPen(QPen(e.color, e.width, Qt::SolidLine, Qt::SquareCap));
      p.drawLine(e.a, e.b);
    }
  p.restore();
}
int Table::overflowCount(const SceneObject &o) {
  int count = 0;
  for (int i = 0; i < o.table.cells.size(); ++i)
    if (o.table.cells[i].rowSpan && !o.table.cells[i].text.isEmpty() &&
        TextLayout::measure(textObject(o, i / o.table.columns.size(),
                                       i % o.table.columns.size()))
            .overflow)
      ++count;
  return count;
}
bool Table::merge(TableData &t, int top, int left, int bottom, int right) {
  if (!region(t, top, left, bottom, right) || (top == bottom && left == right))
    return false;
  const int cols = t.columns.size();
  QStringList texts;
  for (int r = top; r <= bottom; ++r)
    for (int c = left; c <= right; ++c) {
      const int k = anchor(t, r, c);
      if (k < 0 || k / cols < top || k % cols < left ||
          k / cols + t.cells[k].rowSpan - 1 > bottom ||
          k % cols + t.cells[k].columnSpan - 1 > right)
        return false;
      if (t.cells[r * cols + c].rowSpan &&
          !t.cells[r * cols + c].text.isEmpty())
        texts.append(t.cells[r * cols + c].text);
    }
  if (texts.join('\n').size() > 65536)
    return false;
  for (int r = top; r <= bottom; ++r)
    for (int c = left; c <= right; ++c) {
      auto &cell = t.cells[r * cols + c];
      cell.rowSpan = cell.columnSpan = 0;
      cell.text.clear();
    }
  auto &a = t.cells[top * cols + left];
  a.rowSpan = bottom - top + 1;
  a.columnSpan = right - left + 1;
  a.text = texts.join('\n');
  return true;
}
bool Table::split(TableData &t, int row, int column) {
  const int k = anchor(t, row, column);
  if (k < 0)
    return false;
  const auto a = t.cells[k];
  if (a.rowSpan == 1 && a.columnSpan == 1)
    return false;
  row = k / t.columns.size();
  column = k % t.columns.size();
  for (int r = row; r < row + a.rowSpan; ++r)
    for (int c = column; c < column + a.columnSpan; ++c) {
      auto &cell = t.cells[r * t.columns.size() + c];
      cell.rowSpan = cell.columnSpan = 1;
    }
  return true;
}
bool Table::changeAxis(TableData &t, bool rows, int index, bool remove) {
  auto old = t;
  const int length = rows ? t.rows.size() : t.columns.size(),
            limit = rows ? maxRows : maxColumns;
  if (index < 0 || index > (remove ? length - 1 : length) ||
      (remove ? length <= 1 : length >= limit))
    return false;
  auto &axis = rows ? t.rows : t.columns;
  if (remove)
    axis.removeAt(index);
  else
    axis.insert(index, axis.value(qMin(index, length - 1), 1));
  t.cells.clear();
  for (int r = 0; r < t.rows.size(); ++r)
    for (int c = 0; c < t.columns.size(); ++c) {
      int priorR = r, priorC = c;
      int &p = rows ? priorR : priorC;
      const int pos = rows ? r : c;
      if (!remove && pos == index) {
        TableCell fresh;
        fresh.id = Edit::newId("cell");
        t.cells.append(fresh);
        continue;
      }
      if (pos >= index)
        p += remove ? 1 : -1;
      auto cell = old.cells[priorR * old.columns.size() + priorC];
      cell.rowSpan = cell.columnSpan = 1;
      cell.text.clear();
      t.cells.append(cell);
    }
  // Re-map each anchor's rectangle, expanding a merge only when insertion is
  // inside it.
  for (int r = 0; r < old.rows.size(); ++r)
    for (int c = 0; c < old.columns.size(); ++c) {
      auto cell = old.cells[r * old.columns.size() + c];
      if (!cell.rowSpan)
        continue;
      int nr = r, nc = c, rs = cell.rowSpan, cs = cell.columnSpan;
      int &start = rows ? nr : nc, &span = rows ? rs : cs;
      if (remove) {
        if (index < start)
          --start;
        else if (index < start + span)
          --span;
      } else {
        if (index <= start)
          ++start;
        else if (index < start + span)
          ++span;
      }
      if (span <= 0)
        continue;
      cell.rowSpan = rs;
      cell.columnSpan = cs;
      // Preserve the anchor identity even when its original row/column was
      // removed.
      t.cells[nr * t.columns.size() + nc] = cell;
      for (int y = nr; y < nr + rs; ++y)
        for (int x = nc; x < nc + cs; ++x)
          if (y != nr || x != nc) {
            auto &covered = t.cells[y * t.columns.size() + x];
            covered.rowSpan = covered.columnSpan = 0;
            covered.text.clear();
          }
    }
  if (!validate(t)) {
    t = old;
    return false;
  }
  return true;
}
bool Table::setStyle(TableData &t, int top, int left, int bottom, int right,
                     const QString &key, const QVariant &value) {
  if (!region(t, top, left, bottom, right) ||
      (!value.isNull() && !styleValue(key, value)) || !styleKeys.contains(key))
    return false;
  QSet<int> anchors;
  for (int r = top; r <= bottom; ++r)
    for (int c = left; c <= right; ++c)
      anchors.insert(anchor(t, r, c));
  for (int k : anchors)
    if (k >= 0) {
      if (value.isNull())
        t.cells[k].style.remove(key);
      else
        t.cells[k].style[key] = value;
    }
  return true;
}
QStringList Table::styleKeyNames() {
  QStringList keys(styleKeys.cbegin(), styleKeys.cend());
  keys.sort();
  return keys;
}
void Table::scale(TableData &t, qreal factor) {
  t.padding *= factor;
  t.borderWidth *= factor;
  for (auto &cell : t.cells)
    for (const auto &key : {"fontSize", "padding", "borderWidth"})
      if (cell.style.contains(key))
        cell.style[key] = cell.style[key].toDouble() * factor;
}
