#pragma once

#include "database.h"
#include "book.h"


namespace Midoku::Library {

class Chapter : public Object<Chapter>
{
    Q_OBJECT

    Q_PROPERTY(QVariant book READ getBookV)
    Q_PROPERTY(QVariant nextChapter READ getNextChapterV)
    Q_PROPERTY(QVariant previousChapter READ getPreviousChapterV)
public:
    ORM_COLUMN(long, book_id, Util::ORM::NotNull);
    ORM_COLUMN(long, chapter, Util::ORM::NotNull);
    ORM_COLUMN(long, length, Util::ORM::NotNull);
    ORM_COLUMN(QString, media, Util::ORM::NotNull);
    ORM_COLUMN(long, media_offset);
    ORM_COLUMN(long, media_chapter);
    ORM_COLUMN(QString, title);
    ORM_COLUMN(long, cover_blob_id);

    DB_OBJECT(Chapter, "Chapter", book_id, chapter, length, media, media_offset, media_chapter, title, cover_blob_id,
              Util::ORM::ForeignKey(book_id, Util::ORM::References(Book::table, Book::id)),
              Util::ORM::ForeignKey(cover_blob_id, Util::ORM::References(Blob::table, Blob::id)),
              Util::ORM::Unique(book_id, chapter));


    explicit Chapter(Database &db, const Book &b, long chap, long length,
                     QString media, long media_offset, std::optional<long> media_chapter,
                     QString title=QString(), Blob *cover = nullptr);

    explicit Chapter(Database &db, const QSqlRecord &r);


    DBResult<std::unique_ptr<Book>> getBook();
    Q_INVOKABLE QVariant getBookV();

    DBResult<std::optional<std::unique_ptr<Chapter>>> getNextChapter();
    Q_INVOKABLE QVariant getNextChapterV();

    DBResult<std::optional<std::unique_ptr<Chapter>>> getPreviousChapter();
    Q_INVOKABLE QVariant getPreviousChapterV();

    DBResult<long> getTotalOffset();
    Q_INVOKABLE QVariant getTotalOffsetV();

};

DB_OBJECT_POST(Chapter);

using ChapterPtr = std::unique_ptr<Chapter>;

}
