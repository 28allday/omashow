#include "backend.h"
#include "core/chart.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/table.h"
#include "tablemodel.h"
TableModel *Backend::tableModel() {
  if (!m_tableModel)
    m_tableModel = new TableModel(this);
  return m_tableModel;
}
bool Backend::addTable(int rows, int columns) {
  const auto data = Table::create(rows, columns);
  if (data.rows.isEmpty() || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return false;
  SceneObject object;
  object.id = Edit::newId("table");
  object.type = ObjectType::Table;
  object.table = data;
  object.groups = m_groupScope;
  const auto size = m_document.size;
  object.rect = {size.width() * .15, size.height() * .25, size.width() * .7,
                 size.height() * .5};
  object.fillToken = "background";
  object.textColorToken = "foreground";
  object.fontToken = "body";
  object.fontSize = size.height() * .032;
  object.verticalAlign = 1;
  object.table.padding = size.height() * .012;
  object.table.borderWidth = size.height() / 1080;
  m_history.begin(m_document, tr("Insert table"));
  m_document.slides[m_currentSlide].objects.append(object);
  m_history.commit();
  m_selectedIds = {object.id};
  m_selectedId = object.id;
  touch();
  return true;
}
bool Backend::applyTable(const QString &slideId, const QString &objectId,
                         const TableData &table, const QString &label,
                         const DataSource *source) {
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
      m_document.slides.at(m_currentSlide).id != slideId ||
      selectedId() != objectId || selectionCount() != 1 ||
      !Table::validate(table))
    return false;
  const auto *before = selectedObject();
  if (!before || before->locked ||
      (before->type != ObjectType::Table && before->type != ObjectType::Chart))
    return false;
  auto candidate = *before;
  candidate.table = table;
  if (source)
    candidate.dataSource = *source;
  if (!LinkedData::validate(candidate.dataSource))
    return false;
  QSet<QString> ids;
  for (const auto &cell : table.cells)
    ids.insert(cell.id);
  for (auto it = candidate.chart.seriesColors.begin();
       it != candidate.chart.seriesColors.end();)
    if (!ids.contains(it.key()))
      it = candidate.chart.seriesColors.erase(it);
    else
      ++it;
  if (candidate.type == ObjectType::Chart && !Chart::validate(candidate))
    return false;
  if (Table::encode(before->table) == Table::encode(table) &&
      before->dataSource == candidate.dataSource)
    return true;
  m_history.begin(m_document, label);
  auto *o = m_document.slides[m_currentSlide].find(objectId);
  o->table = table;
  o->chart = candidate.chart;
  o->dataSource = candidate.dataSource;
  if (source)
    Design::markOverride(*o, "dataSource");
  Design::markOverride(*o, "table");
  m_history.commit();
  touch();
  return true;
}
void Backend::editSelectedTable() {
  if (selectionCount() == 1 && selectedObject() &&
      (selectedObject()->type == ObjectType::Table ||
       selectedObject()->type == ObjectType::Chart))
    emit tableEditorRequested();
}
