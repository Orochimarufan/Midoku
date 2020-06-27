#include "database.h"
#include "schema.h"
#include "models.h"
#include "progress.h"

#include <QVariant>
#include <QStringList>

using namespace Midoku::Util::ORM;

namespace Midoku::Library {


// ----------------------------------------------------------------------------
// Library Class
// ----------------------------------------------------------------------------
// Constructors
/*Library::Library(QObject *parent, Database *db) :
    QObject(parent),
    db(db),
    own_db(false)
{
    upgrade_schema();
}

Library::Library(QObject *parent, const QString &db_path) :
    QObject(parent),
    db(new Database(db_path)),
    own_db(true)
{
    upgrade_schema();
}

Library::Library(Database *db) :
    db(db),
    own_db(false)
{
    upgrade_schema();
}

Library::Library(const QString &db_path) :
    db(new Database(db_path)),
    own_db(true)
{
    upgrade_schema();
}

Library::~Library() {
    if (own_db)
        delete db;
}*/

// Schema
DBResult<void> upgrade_schema(Database *db) {
    static constexpr long top_version = 3;

    auto r = db->exec(SQL<>{"PRAGMA user_version;"}).map([](auto q) {
        return (q.next() ? q.value(0).toInt() : 0);
    }).bind([db](int version) -> DBResult<void> {
        if (version == 0) {
            // Create from scratch  q   
            return Util::tuple_fold_bind(
                DBResult<void>(Ok()),
                std::tuple{Blob::table, Book::table, Chapter::table, Progress::table},
                [db](auto table) {
                    return db->exec(table.sqlCreateTable(true)).discard();
                }
            );
        } else if (version == top_version)
            return Ok();
        else if (version > top_version)
            return Err(QSqlError("", "Unknown DB schema version"));

        // Migration code here.
        DBResult<void> result = Ok();
        if (version < 2) {
            // Add Progress table
            result = result.bind([db](){
                return db->exec(Progress::table.sqlCreateTable(true)).discard();
            });
        }
        if (version < 3) {
            // Add media_offset column to chapter table
            /*db->qsqldb().transaction();
            result = result.bind([db]() {
                return db->exec(QStringLiteral("ALTER TABLE Chapter RENAME TO Mig3_Chapter;")).discard();
            }).bind([db]() {
                return db->exec(Chapter::table.sqlCreateTable(false)).discard();
            }).bind([db]() {
                static constexpr auto old_cols = std::tuple{Chapter::id, Chapter::book_id, Chapter::chapter, Chapter::length,
                        Chapter::media, Chapter::media_chapter, Chapter::title, Chapter::cover_blob_id};
                static const auto cols_string = Util::tuple_map<QStringList>(old_cols, [] (auto &col) {return col.getName();}).join(", ");
                return db->exec(QStringLiteral("INSERT INTO Chapter (%1) SELECT %1 FROM Mig3_Chapter;").arg(cols_string)).discard();
            });
            if (result)
                db->qsqldb().commit();
            else
                db->qsqldb().rollback();*/
            result = result.bind([db]() {
                return db->exec(Util::ORM::sql_join(" ", Util::ORM::SQL<>("ALTER TABLE Chapter ADD COLUMN"),
                                                    Chapter::media_offset.sqlCreateTableColumn(),
                                                    Util::ORM::SQL<>(";"))).discard();
            });
        }

        return result;
    }).bind([db]() {
        return db->exec(SQL<>{QStringLiteral("PRAGMA user_version = %0;").arg(top_version)}).discard();
    });

    if (!r)
        qDebug() << r.error();

    return r;
}

// Get database object
/*Database &Library::database() {
    return *db;
}

// Models
QVariant Library::booksModel() {
    auto model = new TableModel(nullptr, db->qsqldb(), Book::table);
    if(!model->select())
        qDebug() << "Select error" << model->lastError();
    qDebug() << "Model:" << model->tableName() << model->rowCount();
    return QVariant::fromValue(model);
}

QVariant Library::getBook(long id) {
    auto s = db->load<Book>(id);
    if (s)
        return QVariant::fromValue(s.value().release());
    return QVariant();
}*/

}
