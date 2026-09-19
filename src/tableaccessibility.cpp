#include "tableaccessibility.h"
#include "core/table.h"
#include <QAccessible>
#include <QAccessibleObject>
#include <QQuickWindow>
#include <QSet>

namespace {
QRect globalRect(QQuickItem *item) {
  if (!item || !item->isVisible() || !item->window())
    return {};
  const auto r = item->mapRectToScene(item->boundingRect());
  return QRect(item->window()->mapToGlobal(r.topLeft().toPoint()),
               r.size().toSize());
}
class CellAccessible : public QAccessibleInterface,
                       public QAccessibleTableCellInterface,
                       public QAccessibleActionInterface {
public:
  CellAccessible(TableAccessibility *item, QString id)
      : m_item(item), m_id(std::move(id)) {}
  int index() const {
    if (!m_item || !m_item->model())
      return -1;
    const auto &cells = m_item->model()->object().table.cells;
    for (int i = 0; i < cells.size(); ++i)
      if (cells[i].id == m_id && cells[i].rowSpan > 0)
        return i;
    return -1;
  }
  bool isValid() const override { return index() >= 0; }
  QObject *object() const override { return nullptr; }
  QWindow *window() const override {
    return m_item ? m_item->window() : nullptr;
  }
  QAccessibleInterface *parent() const override {
    return m_item ? QAccessible::queryAccessibleInterface(m_item) : nullptr;
  }
  QAccessibleInterface *child(int) const override { return nullptr; }
  QAccessibleInterface *childAt(int, int) const override { return nullptr; }
  int childCount() const override { return 0; }
  int indexOfChild(const QAccessibleInterface *) const override { return -1; }
  QString text(QAccessible::Text kind) const override {
    if (!isValid())
      return {};
    const auto &c = m_item->model()->object().table.cells[index()];
    if (kind == QAccessible::Name)
      return Table::address(rowIndex(), columnIndex()) + ", " + c.text;
    if (kind == QAccessible::Value)
      return c.text;
    if (kind == QAccessible::Description)
      return QString("Row %1, column %2; spans %3 rows and %4 columns")
          .arg(rowIndex() + 1)
          .arg(columnIndex() + 1)
          .arg(rowExtent())
          .arg(columnExtent());
    return {};
  }
  void setText(QAccessible::Text kind, const QString &value) override {
    if (kind == QAccessible::Value && isValid())
      m_item->model()->commitText(
          m_id, value, m_item->model()->object().table.cells[index()].text);
  }
  QRect rect() const override {
    if (!isValid() || !m_item->view())
      return {};
    QQuickItem *visual = nullptr;
    const QPoint cell(columnIndex(), rowIndex());
    QMetaObject::invokeMethod(m_item->view(), "itemAtCell",
                              Q_RETURN_ARG(QQuickItem *, visual),
                              Q_ARG(QPoint, cell));
    return globalRect(visual).intersected(globalRect(m_item->view()));
  }
  QAccessible::Role role() const override {
    if (!isValid())
      return QAccessible::Cell;
    const auto &t = m_item->model()->object().table;
    return rowIndex() < t.headerRows         ? QAccessible::ColumnHeader
           : columnIndex() < t.headerColumns ? QAccessible::RowHeader
                                             : QAccessible::Cell;
  }
  QAccessible::State state() const override {
    QAccessible::State s;
    s.invalid = !isValid();
    if (s.invalid)
      return s;
    s.focusable = true;
    s.selectable = true;
    s.selected = isSelected();
    s.editable = !m_item->model()->object().locked;
    s.readOnly = !s.editable;
    s.invisible = !m_item->isVisible();
    s.offscreen = rect().isEmpty();
    const auto selection = m_item->model()->selection();
    s.focused = m_item->view() && m_item->view()->hasActiveFocus() &&
                selection.value("id").toString() == m_id;
    return s;
  }
  void *interface_cast(QAccessible::InterfaceType type) override {
    if (type == QAccessible::TableCellInterface)
      return static_cast<QAccessibleTableCellInterface *>(this);
    if (type == QAccessible::ActionInterface)
      return static_cast<QAccessibleActionInterface *>(this);
    return nullptr;
  }
  bool isSelected() const override {
    if (!isValid())
      return false;
    return m_item->model()->selectedRange().intersects(
        QRect(columnIndex(), rowIndex(), columnExtent(), rowExtent()));
  }
  int columnIndex() const override {
    int i = index();
    return i < 0 ? -1 : i % m_item->model()->columnCount();
  }
  int rowIndex() const override {
    int i = index();
    return i < 0 ? -1 : i / m_item->model()->columnCount();
  }
  int columnExtent() const override {
    int i = index();
    return i < 0 ? 0 : m_item->model()->object().table.cells[i].columnSpan;
  }
  int rowExtent() const override {
    int i = index();
    return i < 0 ? 0 : m_item->model()->object().table.cells[i].rowSpan;
  }
  QAccessibleInterface *table() const override { return parent(); }
  QList<QAccessibleInterface *> columnHeaderCells() const override {
    QList<QAccessibleInterface *> cells;
    if (!isValid() || !parent() || !parent()->tableInterface())
      return cells;
    for (int r = 0; r < m_item->model()->object().table.headerRows; ++r)
      for (int c = columnIndex(); c < columnIndex() + columnExtent(); ++c) {
        auto *header = parent()->tableInterface()->cellAt(r, c);
        if (header && header != this && !cells.contains(header))
          cells.append(header);
      }
    return cells;
  }
  QList<QAccessibleInterface *> rowHeaderCells() const override {
    QList<QAccessibleInterface *> cells;
    if (!isValid() || !parent() || !parent()->tableInterface())
      return cells;
    for (int c = 0; c < m_item->model()->object().table.headerColumns; ++c)
      for (int r = rowIndex(); r < rowIndex() + rowExtent(); ++r) {
        auto *header = parent()->tableInterface()->cellAt(r, c);
        if (header && header != this && !cells.contains(header))
          cells.append(header);
      }
    return cells;
  }
  QStringList actionNames() const override {
    return {setFocusAction(), pressAction()};
  }
  QStringList keyBindingsForAction(const QString &action) const override {
    return action == pressAction() ? QStringList{"Enter"} : QStringList();
  }
  void doAction(const QString &action) override {
    if (isValid() && (action == setFocusAction() || action == pressAction()))
      emit m_item->selectRequested(rowIndex(), columnIndex(), false);
  }

private:
  QPointer<TableAccessibility> m_item;
  QString m_id;
};
class TableAccessible : public QAccessibleObject,
                        public QAccessibleTableInterface {
public:
  explicit TableAccessible(TableAccessibility *item)
      : QAccessibleObject(item), m_item(item) {}
  ~TableAccessible() override {
    for (auto id : m_children)
      QAccessible::deleteAccessibleInterface(id);
  }
  TableModel *model() const { return m_item ? m_item->model() : nullptr; }
  QList<int> anchors() const {
    QList<int> values;
    if (!model())
      return values;
    const auto &cells = model()->object().table.cells;
    for (int i = 0; i < cells.size(); ++i)
      if (cells[i].rowSpan > 0)
        values.append(i);
    return values;
  }
  void prune() const {
    QSet<QString> ids;
    if (model())
      for (const auto &cell : model()->object().table.cells)
        if (cell.rowSpan)
          ids.insert(cell.id);
    for (auto it = m_children.begin(); it != m_children.end();)
      if (!ids.contains(it.key())) {
        QAccessible::deleteAccessibleInterface(it.value());
        it = m_children.erase(it);
      } else
        ++it;
  }
  int childCount() const override { return anchors().size(); }
  QAccessibleInterface *child(int index) const override {
    const auto list = anchors();
    if (index < 0 || index >= list.size() || columnCount() == 0)
      return nullptr;
    return cellAt(list[index] / columnCount(), list[index] % columnCount());
  }
  int indexOfChild(const QAccessibleInterface *child) const override {
    const auto *cell = dynamic_cast<const CellAccessible *>(child);
    if (!cell || cell->parent() != this)
      return -1;
    return anchors().indexOf(cell->index());
  }
  QAccessibleInterface *parent() const override {
    return m_item && m_item->parentItem()
               ? QAccessible::queryAccessibleInterface(m_item->parentItem())
               : nullptr;
  }
  QAccessibleInterface *focusChild() const override {
    if (!model())
      return nullptr;
    const auto s = model()->selection();
    return cellAt(s.value("row").toInt(), s.value("column").toInt());
  }
  QWindow *window() const override {
    return m_item ? m_item->window() : nullptr;
  }
  QRect rect() const override { return globalRect(m_item); }
  QAccessibleInterface *childAt(int x, int y) const override {
    for (int i = 0; i < childCount(); ++i) {
      auto *cell = child(i);
      if (cell && cell->rect().contains(x, y))
        return cell;
    }
    return nullptr;
  }
  QAccessible::Role role() const override { return QAccessible::Table; }
  QAccessible::State state() const override {
    QAccessible::State s;
    s.focusable = true;
    s.multiSelectable = true;
    s.invisible = !m_item || !m_item->isVisible();
    s.focused = m_item && m_item->view() && m_item->view()->hasActiveFocus();
    return s;
  }
  QString text(QAccessible::Text type) const override {
    if (type == QAccessible::Name)
      return QString("Table cells, %1 rows and %2 columns")
          .arg(rowCount())
          .arg(columnCount());
    if (type == QAccessible::Description)
      return "Arrows move between cells; Shift selects a range. Enter edits "
             "cell text. Tab advances. Control V previews pasted data.";
    return {};
  }
  void *interface_cast(QAccessible::InterfaceType type) override {
    return type == QAccessible::TableInterface
               ? static_cast<QAccessibleTableInterface *>(this)
               : nullptr;
  }
  QAccessibleInterface *caption() const override { return nullptr; }
  QAccessibleInterface *summary() const override { return nullptr; }
  QAccessibleInterface *cellAt(int row, int column) const override {
    if (!model())
      return nullptr;
    int k = Table::anchor(model()->object().table, row, column);
    if (k < 0)
      return nullptr;
    const auto id = model()->object().table.cells[k].id;
    if (!m_children.contains(id)) {
      prune();
      m_children[id] = QAccessible::registerAccessibleInterface(
          new CellAccessible(m_item, id));
    }
    return QAccessible::accessibleInterface(m_children[id]);
  }
  QList<QAccessibleInterface *> selectedCells() const override {
    QList<QAccessibleInterface *> cells;
    for (int i = 0; i < childCount(); ++i) {
      auto *c = child(i);
      if (c && c->tableCellInterface()->isSelected())
        cells.append(c);
    }
    return cells;
  }
  int selectedCellCount() const override { return selectedCells().size(); }
  QString columnDescription(int column) const override {
    auto *c = cellAt(0, column);
    return c ? c->text(QAccessible::Name) : QString();
  }
  QString rowDescription(int row) const override {
    auto *c = cellAt(row, 0);
    return c ? c->text(QAccessible::Name) : QString();
  }
  int rowCount() const override { return model() ? model()->rowCount() : 0; }
  int columnCount() const override {
    return model() ? model()->columnCount() : 0;
  }
  bool isColumnSelected(int column) const override {
    if (!model() || column < 0 || column >= columnCount())
      return false;
    const auto r = model()->selectedRange();
    return r.left() <= column && r.right() >= column && r.top() == 0 &&
           r.bottom() == rowCount() - 1;
  }
  bool isRowSelected(int row) const override {
    if (!model() || row < 0 || row >= rowCount())
      return false;
    const auto r = model()->selectedRange();
    return r.top() <= row && r.bottom() >= row && r.left() == 0 &&
           r.right() == columnCount() - 1;
  }
  QList<int> selectedRows() const override {
    QList<int> rows;
    for (int r = 0; r < rowCount(); ++r)
      if (isRowSelected(r))
        rows.append(r);
    return rows;
  }
  QList<int> selectedColumns() const override {
    QList<int> columns;
    for (int c = 0; c < columnCount(); ++c)
      if (isColumnSelected(c))
        columns.append(c);
    return columns;
  }
  int selectedRowCount() const override { return selectedRows().size(); }
  int selectedColumnCount() const override { return selectedColumns().size(); }
  bool selectRow(int row) override {
    if (row < 0 || row >= rowCount())
      return false;
    emit m_item->selectRequested(row, 0, false);
    emit m_item->selectRequested(row, columnCount() - 1, true);
    return isRowSelected(row);
  }
  bool selectColumn(int column) override {
    if (column < 0 || column >= columnCount())
      return false;
    emit m_item->selectRequested(0, column, false);
    emit m_item->selectRequested(rowCount() - 1, column, true);
    return isColumnSelected(column);
  }
  bool unselectRow(int row) override { return !isRowSelected(row); }
  bool unselectColumn(int column) override { return !isColumnSelected(column); }
  void modelChange(QAccessibleTableModelChangeEvent *) override { prune(); }

private:
  QPointer<TableAccessibility> m_item;
  mutable QMap<QString, QAccessible::Id> m_children;
};
QAccessibleInterface *factory(const QString &, QObject *object) {
  if (auto *item = qobject_cast<TableAccessibility *>(object))
    return new TableAccessible(item);
  return nullptr;
}
} // namespace
TableAccessibility::TableAccessibility(QQuickItem *parent)
    : QQuickItem(parent) {
  static bool installed = false;
  if (!installed) {
    QAccessible::installFactory(factory);
    installed = true;
  }
}
void TableAccessibility::setModel(TableModel *model) {
  if (m_model == model)
    return;
  if (m_model)
    disconnect(m_model, nullptr, this, nullptr);
  m_model = model;
  if (model) {
    connect(model, &TableModel::selectionChanged, this,
            &TableAccessibility::notifySelection);
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
      QAccessibleTableModelChangeEvent event(
          this, QAccessibleTableModelChangeEvent::ModelReset);
      QAccessible::updateAccessibility(&event);
    });
    connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &a, const QModelIndex &b,
                   const QList<int> &roles) {
              if (roles.size() == 1)
                return; // visual selection is announced separately
              QAccessibleTableModelChangeEvent event(
                  this, QAccessibleTableModelChangeEvent::DataChanged);
              event.setFirstRow(a.row());
              event.setLastRow(b.row());
              event.setFirstColumn(a.column());
              event.setLastColumn(b.column());
              QAccessible::updateAccessibility(&event);
            });
  }
  emit modelChanged();
}
void TableAccessibility::notifySelection() {
  if (!isVisible() || !m_model || !QAccessible::isActive())
    return;
  auto *table = QAccessible::queryAccessibleInterface(this);
  if (!table || !table->tableInterface())
    return;
  const auto selection = m_model->selection();
  auto *cell = table->tableInterface()->cellAt(
      selection.value("row").toInt(), selection.value("column").toInt());
  if (!cell)
    return;
  QAccessibleEvent event(cell, QAccessible::Focus);
  QAccessible::updateAccessibility(&event);
}
