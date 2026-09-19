#pragma once
#include "core/scene.h"
#include <QAbstractTableModel>
#include <QUrl>
class Backend;
class PortalFileChooser;
class TableModel : public QAbstractTableModel {
  Q_OBJECT
  Q_PROPERTY(QVariantMap selection READ selection NOTIFY selectionChanged)
  Q_PROPERTY(QVariantMap summary READ summary NOTIFY selectionChanged)
public:
  explicit TableModel(Backend *backend);
  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  QVariantMap selection() const;
  QVariantMap summary() const;
  const SceneObject &object() const { return m_object; }
  QRect selectedRange() const { return range(); }
  void refresh();
  Q_INVOKABLE void selectCell(int row, int column, bool extend = false);
  Q_INVOKABLE void moveCell(int horizontal, int vertical, bool extend = false);
  Q_INVOKABLE bool commitText(const QString &cellId, const QString &text,
                              const QString &expected);
  Q_INVOKABLE QVariantMap previewSort(int column, bool ascending, bool numeric,
                                      const QString &locale) const;
  Q_INVOKABLE bool sortRows(int column, bool ascending, bool numeric,
                            const QString &locale);
  Q_INVOKABLE void importDataDialog();
  Q_INVOKABLE bool loadDataFile(const QUrl &url);
  Q_INVOKABLE bool refreshDataFile();
  Q_INVOKABLE void cancelDataFile();
  Q_INVOKABLE bool disconnectDataFile();
  Q_INVOKABLE QVariantMap previewDataFile(const QString &text, int delimiter,
                                          bool linked) const;
  Q_INVOKABLE bool applyDataFile(const QString &text, int delimiter,
                                 bool linked);
  Q_INVOKABLE bool clearRange();
  Q_INVOKABLE bool distribute(bool rows);
  Q_INVOKABLE bool mergeCells();
  Q_INVOKABLE bool splitCell();
  Q_INVOKABLE bool moveAxis(bool rows, int from, int to);
  Q_INVOKABLE bool changeAxis(bool rows, int index, bool remove);
  Q_INVOKABLE bool setAxisSize(bool rows, int index, qreal size);
  Q_INVOKABLE bool formatCells(const QString &key, const QVariant &value);
  Q_INVOKABLE bool setTableOption(const QString &key, const QVariant &value);
  Q_INVOKABLE QVariantMap previewPaste(const QString &text,
                                       int delimiter = 0) const;
  Q_INVOKABLE bool pasteText(const QString &text, int delimiter = 0);
  Q_INVOKABLE QString copyText() const;
  Q_INVOKABLE void copyRange() const;
  Q_INVOKABLE QString clipboardText() const;
signals:
  void dataFileReady(const QString &text, const QString &name);
  void dataFileFailed(const QString &message);
  void selectionChanged();

private:
  enum Role {
    CellId = Qt::UserRole + 1,
    CellText,
    CellAddress,
    CoveredBy,
    SpanRows,
    SpanColumns,
    Header,
    Selected
  };
  QVector<int> sortedRows(int column, bool ascending, bool numeric,
                          const QString &locale, QString &error) const;
  PortalFileChooser *m_dataChooser = nullptr;
  int m_pickerRevision = -1;
  QString m_pickerObject, m_pickerSlide;
  bool m_loadingData = false;
  int m_dataRequest = 0, m_candidateRevision = -1;
  QString m_candidateObject, m_candidateSlide;
  LinkedData::File m_candidate;
  bool m_candidateRefresh = false;
  bool readDataFile(const QUrl &url, bool refresh);
  bool candidateCurrent() const;
  QStringList m_chartIssues, m_chartWarnings;
  int m_overflow = 0;
  Backend *m_backend;
  SceneObject m_object;
  QString m_slideId;
  int m_row = 0, m_column = 0, m_originRow = 0, m_originColumn = 0;
  bool apply(const TableData &table, const QString &label);
  QRect range() const;
};
