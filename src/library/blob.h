#pragma once

#include "database.h"

#include <QImage>


namespace Midoku::Library {

class Blob : public Object<Blob>
{
public:
    ORM_COLUMN(QByteArray, data, Util::ORM::NotNull);

    DB_OBJECT(Blob, "Blob", data);


    explicit Blob(Database &db, QByteArray data) :
        Object(db),
        row({std::nullopt, data})
    {}

    explicit Blob(Database &db, const QSqlRecord &r) :
        Object(db),
        row(qsql_unpack_record<Table>(r))
    {}

    QImage toImage();
};

DB_OBJECT_POST(Blob);

}
