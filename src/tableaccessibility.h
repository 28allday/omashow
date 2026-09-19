#pragma once
#include "tablemodel.h"
#include <QPointer>
#include <QQuickItem>
// Semantic parent for the virtualised visual grid. Cell accessibles exist for
// the entire table, including cells currently outside the scrolling viewport.
class TableAccessibility : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(TableModel *model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)
public:
  explicit TableAccessibility(QQuickItem *parent = nullptr);
  TableModel *model() const { return m_model; }
  void setModel(TableModel *model);
  QQuickItem *view() const { return m_view; }
  void setView(QQuickItem *view) {
    m_view = view;
    emit viewChanged();
  }
  void notifySelection();
signals:
  void modelChanged();
  void viewChanged();
  void selectRequested(int row, int column, bool extend);

private:
  QPointer<TableModel> m_model;
  QPointer<QQuickItem> m_view;
};
