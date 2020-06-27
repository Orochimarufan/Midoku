#pragma once

#include "book.h"
#include "chapter.h"

#include <QSqlTableModel>
#include <QSqlRelationalTableModel>


namespace Midoku::Library {

class TableModel : public QSqlTableModel {
    Q_OBJECT

    QHash<int, QByteArray> m_roleNames;
public:
    template <typename Table>
    TableModel(QObject *parent, QSqlDatabase db, Table) :
        QSqlTableModel(parent, db),
        m_roleNames(Util::tuple_enumerate_map<decltype (m_roleNames)>(Table::columns, [](int i, auto col) {
            return std::pair<int, QByteArray>(Qt::UserRole + i, col.name.value);
        }))
    {
        setTable(Table::name.value);
    }

public:
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE virtual QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
};

}
