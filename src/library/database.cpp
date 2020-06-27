#include "database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QDebug>

namespace Midoku::Library {

Database::Database(QString path) :
    name(path)
{
    QSqlDatabase db;

    // Open DB
    if (QSqlDatabase::contains(name)) {
        db = QSqlDatabase::database(name);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        db.open();
    }

    if (!db.isValid() || !db.isOpen()) {
        qCritical() << "Could not open database:" << db.lastError().text();
    }

    // setup stuff
    QSqlQuery q = QSqlQuery(db);
    q.exec("PRAGMA FOREIGN_KEYS = ON;");
}

Database::~Database()
{
}

// ---------------
QSqlDatabase Database::qsqldb() {
    return QSqlDatabase::database(name);
}

// ---------------
QSqlQuery Database::prepare(QString sql)
{
    //qDebug() << "SQL prepare:" << sql;
    QSqlQuery q(QSqlDatabase::database(name));
    q.prepare(sql);
    return q;
}

DBResult<void> Database::exec(QSqlQuery &q)
{
    qDebug() << "SQL:" << q.executedQuery() << q.boundValues();
    if (!q.exec())
        return Err(q.lastError());
    else
        return Ok();
}

} // namespace Midoku::Library
