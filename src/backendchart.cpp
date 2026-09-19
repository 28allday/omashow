#include "backend.h"
#include "core/chart.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/table.h"
QStringList Backend::chartNames() const { return Chart::names(); }
bool Backend::addChart(int kind) {
  if (kind < 0 || kind >= Chart::names().size() || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return false;
  SceneObject o;
  o.type = ObjectType::Chart;
  o.id = Edit::newId("chart");
  o.chart.kind = kind;
  o.groups = m_groupScope;
  o.rect = {m_document.size.width() * .12, m_document.size.height() * .14,
            m_document.size.width() * .76, m_document.size.height() * .72};
  o.fontSize = m_document.size.height() * .027;
  o.fillToken = "background";
  o.textColorToken = "foreground";
  o.fontToken = "body";
  const int columns = (kind == 7 || kind == 8) ? 2 : 3;
  o.table = Table::create(4, columns);
  o.table.headerColumns = 1;
  o.table.cells[0].text = kind == 9 ? "X" : "Category";
  o.table.cells[1].text = "2025";
  if (columns > 2)
    o.table.cells[2].text = "2026";
  for (int r = 1; r < 4; ++r) {
    o.table.cells[r * columns].text =
        kind == 9 ? QString::number(r) : QString("Q%1").arg(r);
    o.table.cells[r * columns + 1].text = QString::number(20 + r * 8);
    if (columns > 2)
      o.table.cells[r * columns + 2].text = QString::number(24 + r * 10);
  }
  m_history.begin(m_document, tr("Insert chart"));
  m_document.slides[m_currentSlide].objects.append(o);
  m_history.commit();
  m_selectedId = o.id;
  m_selectedIds = {o.id};
  touch();
  return true;
}
bool Backend::setChartProperty(const QString &key, const QVariant &value) {
  const auto *selected = static_cast<const Backend *>(this)->selectedObject();
  if (!selected || selectionCount() != 1 || selected->locked ||
      selected->type != ObjectType::Chart)
    return false;
  auto changed = *selected;
  auto properties = Chart::encode(changed.chart);
  if (!properties.contains(key) || key == "seriesColors")
    return false;
  properties[key] = value;
  if (key == "kind") {
    const int kind = value.toInt();
    if (kind != 4 && kind != 9)
      properties["logarithmic"] = false;
    if ((kind <= 3 || kind == 5 || kind == 6) &&
        (changed.chart.minimumY > 0 || changed.chart.maximumY < 0))
      properties["manualY"] = false;
  }
  if (key == "logarithmic" && value.toBool()) {
    properties["includeZero"] = false;
    if (changed.chart.minimumY <= 0)
      properties["manualY"] = false;
  }
  ChartData parsed;
  if (!Chart::decode(properties, parsed))
    return false;
  changed.chart = parsed;
  if (!Chart::validate(changed))
    return false;
  if (Chart::encode(selected->chart) == properties)
    return true;
  m_history.begin(m_document, tr("Format chart"));
  auto *o = selectedObject();
  o->chart = parsed;
  Design::markOverride(*o, "chart");
  m_history.commit();
  touch();
  return true;
}
bool Backend::setChartSeriesColor(const QString &id, const QString &color) {
  const auto *selected = static_cast<const Backend *>(this)->selectedObject();
  if (!selected || selectionCount() != 1 || selected->locked ||
      selected->type != ObjectType::Chart)
    return false;
  bool found = false;
  for (const auto &cell : selected->table.cells)
    if (cell.id == id)
      found = true;
  if (!found)
    return false;
  QColor parsed(color);
  if (!color.isEmpty() && (!parsed.isValid() || parsed.alpha() != 255))
    return false;
  m_history.begin(m_document, tr("Set chart series colour"));
  auto *o = selectedObject();
  if (color.isEmpty())
    o->chart.seriesColors.remove(id);
  else
    o->chart.seriesColors[id] = parsed;
  Design::markOverride(*o, "chart");
  m_history.commit();
  touch();
  return true;
}

bool Backend::setChartThemeColor(int slot, const QString &color) {
  const QColor parsed(color);
  if (slot < 0 || slot >= 8 || !parsed.isValid() || parsed.alpha() != 255)
    return false;
  const QString token = QString("chart%1").arg(slot + 1);
  if (m_document.theme.colors.value(token) == parsed)
    return true;
  m_history.begin(m_document, tr("Change deck chart palette"));
  m_document.theme.colors[token] = parsed;
  m_history.commit();
  touch();
  return true;
}
