#pragma once

#include "database.h"
#include "book.h"
#include "chapter.h"

#include <QDateTime>

namespace Midoku::Library {

class Progress : public Object<Progress>
{
    Q_OBJECT

    Q_PROPERTY(QVariant book READ getBookV)
    Q_PROPERTY(QVariant chapter READ getChapterV)
    Q_PROPERTY(QDateTime date READ timestampDate)

public:
    ORM_COLUMN(long, book_id, Util::ORM::NotNull);
    ORM_COLUMN(long, chapter_id, Util::ORM::NotNull);
    ORM_COLUMN(long, time, Util::ORM::NotNull);
    ORM_COLUMN(QString, timestamp);

    DB_OBJECT(Progress, "Progress", book_id, chapter_id, time, timestamp,
              Util::ORM::ForeignKey(book_id, Util::ORM::References(Book::table, Book::id)),
              Util::ORM::ForeignKey(chapter_id, Util::ORM::References(Chapter::table, Chapter::id)));


    explicit Progress(Database &db, const Chapter &chap, long time, QDateTime when) :
        Object(db),
        row({std::nullopt, chap.get(Chapter::book_id), chap.getId(), time, when.toString(Qt::ISODate)})
    {}

    explicit Progress(Database &db, const QSqlRecord &r) :
        Object(db),
        row(qsql_unpack_record<Table>(r))
    {}

    QDateTime timestampDate() const;

    DBResult<std::unique_ptr<Book>> getBook();
    Q_INVOKABLE QVariant getBookV();

    DBResult<std::unique_ptr<Chapter>> getChapter();
    Q_INVOKABLE QVariant getChapterV();

    DBResult<void> update(long time);
    DBResult<void> update(const Chapter &chap);
    DBResult<void> update(const Chapter &chap, long time);
    Q_INVOKABLE bool update(QVariant v);

    static DBResult<std::optional<std::unique_ptr<Progress>>> findMostRecentForBook(Database &db, const Book &b);
    static DBResult<std::unique_ptr<Progress>> forBook(Database &db, Book &b);

    static DBResult<std::unique_ptr<Progress>> findMostRecent(Database &db);
};

using ProgressPtr = std::unique_ptr<Progress>;

DB_OBJECT_POST(Progress);

}
