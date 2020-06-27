#include "chapter.h"

namespace Midoku::Library {

DB_OBJECT_IMPL(Chapter)


Chapter::Chapter(Database &db, const Book &b, long chap, long length,
                 QString media, long media_offset, std::optional<long> media_chapter, QString title,
                 Blob *cover) :
    Object(db),
    row({std::nullopt, b.getId(), chap, length, media, media_offset, media_chapter, title, cover? std::optional(cover->getId()) : std::nullopt})
{}

Chapter::Chapter(Database &db, const QSqlRecord &r) :
    Object(db),
    row(qsql_unpack_record<Table>(r))
{}


DBResult<std::unique_ptr<Book>> Chapter::getBook() {
    return database().load<Book>(get(book_id));
}

QVariant Chapter::getBookV() {
    return result_to_variant(getBook());
}

DBResult<std::optional<std::unique_ptr<Chapter>>> Chapter::getNextChapter() {
    return database().select<Chapter>(book_id == get(book_id) && chapter == get(chapter) + 1)
            .map([] (std::vector<std::unique_ptr<Chapter>> &&cs) -> std::optional<std::unique_ptr<Chapter>> {
        if (cs.size() > 0)
            return std::move(cs[0]);
        else
            return std::nullopt;
    });
}

QVariant Chapter::getNextChapterV() {
    auto r = getNextChapter();
    if (!r)
        return QVariant();
    auto opt = std::move(r.value());
    if (!opt)
        return QVariant();
    return QVariant::fromValue(opt.value().release());
}

DBResult<std::optional<std::unique_ptr<Chapter>>> Chapter::getPreviousChapter() {
    return database().select<Chapter>(book_id == get(book_id) && chapter == get(chapter) - 1)
            .map([] (std::vector<std::unique_ptr<Chapter>> &&cs) -> std::optional<std::unique_ptr<Chapter>> {
        if (cs.size() > 0)
            return std::move(cs[0]);
        else
            return std::nullopt;
    });
}

QVariant Chapter::getPreviousChapterV() {
    auto r = getPreviousChapter();
    if (!r)
        return QVariant();
    auto opt = std::move(r.value());
    if (!opt)
        return QVariant();
    return QVariant::fromValue(opt.value().release());
}

DBResult<long> Chapter::getTotalOffset() {
    using namespace Util::ORM;
    return database().exec(Util::ORM::select(Sel::From(Chapter::table, Expr::sum(Chapter::length)),
                                             Chapter::book_id == get(book_id) && Chapter::chapter < get(chapter)))
            .bind([] (QSqlQuery &&q) -> DBResult<long> {
        if (q.next())
            return Ok(q.value(0).toInt());
        else
            return Err(QSqlError("Sum didn't return anything"));
    });
}

QVariant Chapter::getTotalOffsetV() {
    return getTotalOffset().map([] (long x) {return QVariant::fromValue(x);}).value_or(QVariant());
}

}
