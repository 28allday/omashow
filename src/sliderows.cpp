#include "sliderows.h"

int SlideRows::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant SlideRows::data(const QModelIndex &index, int role) const {
    if (role != Qt::UserRole || !index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    return m_rows.at(index.row());
}

QHash<int, QByteArray> SlideRows::roleNames() const {
    return {{Qt::UserRole, QByteArrayLiteral("row")}};
}

void SlideRows::sync(const QVariantList &rows) {
    const int kept = qMin(m_rows.size(), rows.size());
    for (int i = 0; i < kept; ++i) {
        if (m_rows.at(i) == rows.at(i)) continue;
        m_rows[i] = rows.at(i);
        emit dataChanged(index(i), index(i), {Qt::UserRole});
    }
    if (rows.size() > m_rows.size()) {
        beginInsertRows(QModelIndex(), m_rows.size(), rows.size() - 1);
        for (int i = m_rows.size(); i < rows.size(); ++i) m_rows.append(rows.at(i));
        endInsertRows();
    } else if (rows.size() < m_rows.size()) {
        beginRemoveRows(QModelIndex(), rows.size(), m_rows.size() - 1);
        m_rows.resize(rows.size());
        endRemoveRows();
    }
}
