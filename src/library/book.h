#pragma once

#include "database.h"
#include "blob.h"


namespace Midoku::Library {

template <typename T>
QVariant result_to_variant(DBResult<std::unique_ptr<T>> r) {
    if (r)
        return QVariant::fromValue(r.value().release());
    qDebug() << "qml error:" << r.error();
    return QVariant();
}

class Chapter;
class Progress;

class Book : public Object<Book>
{
    Q_OBJECT

    friend class Object<Book>;

    Q_PROPERTY(QVariantList chapters READ getChaptersV)
    Q_PROPERTY(QVariant chapterCount READ getChapterCountV)
    Q_PROPERTY(QVariant firstChapter READ getFirstChapterV)
    Q_PROPERTY(QVariant mostRecentProgress READ getMostRecentProgressV)

public:
    ORM_COLUMN(QString, title, Util::ORM::NotNull);
    ORM_COLUMN(QString, author);
    ORM_COLUMN(QString, reader);
    ORM_COLUMN(long, cover_blob_id);

    DB_OBJECT(Book, "Book", title, author, reader, cover_blob_id,
              Util::ORM::ForeignKey(cover_blob_id, Util::ORM::References(Blob::table, Blob::id)));


    explicit Book(Database &db, QString title, QString author = QString(), QString reader = QString(),
                  Blob *cover = nullptr) :
        Object(db),
        row({std::nullopt, title, author, reader, cover? std::optional(cover->getId()) : std::nullopt})
    {}

    explicit Book(Database &db, const QSqlRecord &r) :
        Object(db),
        row(qsql_unpack_record<Table>(r))
    {}

    DBResult<std::vector<std::unique_ptr<Chapter>>> getChapters();
    Q_INVOKABLE QVariantList getChaptersV();

    DBResult<long> getChapterCount();
    Q_INVOKABLE QVariant getChapterCountV();

    DBResult<long> getTotalTime();
    Q_INVOKABLE QVariant getTotalTimeV();

    DBResult<std::unique_ptr<Chapter>> getFirstChapter();
    Q_INVOKABLE QVariant getFirstChapterV();

    DBResult<std::unique_ptr<Chapter>> getChapterAt(long time);
    DBResult<std::pair<std::unique_ptr<Chapter>, int64_t>> getChapterAt2(long time);
    Q_INVOKABLE QVariant getChapterAtV(long time);

    DBResult<QImage> getCover();

    // See progress.cpp
    Q_INVOKABLE QVariant getMostRecentProgressV();

    DBResult<std::unique_ptr<Progress>> getProgress();
    Q_INVOKABLE QVariant getProgressV();
};

DB_OBJECT_POST(Book);

using BookPtr = std::unique_ptr<Book>;

}
