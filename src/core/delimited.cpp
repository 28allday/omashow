#include "core/delimited.h"
#include "core/table.h"
Delimited::Result Delimited::parse(const QString &input, QChar delimiter) {
  Result result;
  QString text = input;
  if (text.startsWith(QChar(0xfeff)))
    text.remove(0, 1);
  if (text.size() > Table::maxText || text.contains(QChar(0))) {
    result.error = "Data is too large or contains a NUL character.";
    return result;
  }
  if (text.isEmpty()) {
    result.error = "The data is empty.";
    return result;
  }
  if (delimiter.isNull()) {
    bool quote = false;
    int tabs = 0, commas = 0, semicolons = 0;
    for (int i = 0; i < text.size(); ++i) {
      auto c = text[i];
      if (c == '"') {
        if (quote && i + 1 < text.size() && text[i + 1] == '"')
          ++i;
        else
          quote = !quote;
      } else if (!quote) {
        if (c == '\n' || c == '\r')
          break;
        if (c == '\t')
          ++tabs;
        if (c == ',')
          ++commas;
        if (c == ';')
          ++semicolons;
      }
    }
    delimiter = tabs > 0 ? '\t' : semicolons > commas ? ';' : ',';
  }
  if (delimiter != '\t' && delimiter != ',' && delimiter != ';') {
    result.error = "Choose comma, tab or semicolon.";
    return result;
  }
  result.delimiter = delimiter;
  QStringList row;
  QString field;
  bool quoted = false, closed = false, started = false;
  auto appendField = [&]() {
    row.append(field);
    field.clear();
    closed = started = false;
    return row.size() <= Table::maxColumns;
  };
  for (int i = 0; i < text.size(); ++i) {
    const auto c = text[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          quoted = false;
          closed = true;
        }
      } else
        field += c;
    } else if (c == delimiter || c == '\n' || c == '\r') {
      if (!appendField()) {
        result.error = "Data exceeds 50 columns.";
        return result;
      }
      if (c != delimiter) {
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
          ++i;
        result.rows.append(row);
        row.clear();
        if (result.rows.size() > Table::maxRows) {
          result.error = "Data exceeds 100 rows.";
          return result;
        }
      }
    } else if (c == '"') {
      if (started || closed) {
        result.error = "A quote must begin a field; quote literal quotes twice "
                       "inside quoted fields.";
        return result;
      }
      quoted = true;
      started = true;
    } else {
      if (closed) {
        result.error = "Unexpected text after a closing quote.";
        return result;
      }
      started = true;
      field += c;
    }
    if (field.size() > 65536) {
      result.error = "A cell exceeds 65,536 characters.";
      return result;
    }
  }
  if (quoted) {
    result.error = "A quoted field is not closed.";
    return result;
  }
  if (!row.isEmpty() || !field.isEmpty() || started || closed ||
      text.endsWith(delimiter)) {
    if (!appendField()) {
      result.error = "Data exceeds 50 columns.";
      return result;
    }
    result.rows.append(row);
  }
  if (result.rows.size() > Table::maxRows) {
    result.error = "Data exceeds 100 rows.";
    return result;
  }
  int width = 0;
  for (const auto &r : result.rows)
    width = qMax(width, int(r.size()));
  for (auto &r : result.rows)
    while (r.size() < width)
      r.append(QString());
  return result;
}
QString Delimited::write(const QVector<QStringList> &rows, QChar delimiter) {
  QStringList lines;
  for (const auto &row : rows) {
    QStringList fields;
    for (auto field : row) {
      if (field.contains(delimiter) || field.contains('"') ||
          field.contains('\n') || field.contains('\r')) {
        field.replace("\"", "\"\"");
        field = '"' + field + '"';
      }
      fields.append(field);
    }
    lines.append(fields.join(delimiter));
  }
  return lines.join('\n');
}
