#include "models.h"

namespace Midoku::Library {

QHash<int, QByteArray> TableModel::roleNames() const {
    return m_roleNames;
}

QVariant TableModel::data(const QModelIndex &idx, int role) const {
    if (role < Qt::UserRole)
        return QSqlTableModel::data(idx, role);

    return QSqlQueryModel::data(this->index(idx.row(), role - Qt::UserRole));
}

}
