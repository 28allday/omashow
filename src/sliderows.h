#pragma once

// The navigator's rows as a model that is brought up to date in place. A plain
// JavaScript array given to a ListView is replaced whole on every edit, which
// destroys every delegate and reloads every thumbnail; here a changed row is
// updated and the rest are left alone, so the delegates (and the pictures they
// hold) survive an edit to one slide.

#include <QAbstractListModel>
#include <QVariantList>

class SlideRows : public QAbstractListModel {
    Q_OBJECT
public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    // One role, so a delegate's modelData is the row itself.
    QHash<int, QByteArray> roleNames() const override;

    void sync(const QVariantList &rows);
    QVariantList rows() const { return m_rows; }

private:
    QVariantList m_rows;
};
