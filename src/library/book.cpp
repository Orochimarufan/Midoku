#include "book.h"
#include "chapter.h"

namespace Midoku::Library {

DB_OBJECT_IMPL(Book)

DBResult<std::vector<std::unique_ptr<Chapter>>> Book::getChapters() {
    return database().select<Chapter>(Chapter::book_id == getId());
}

QVariantList Book::getChaptersV() {
   return getChapters().map([] (std::vector<std::unique_ptr<Chapter>> &&r) {
       QVariantList l;
       l.reserve(r.size());
       std::for_each(std::move_iterator(r.begin()), std::move_iterator(r.end()), [&l](auto &&chap) {
           l.append(QVariant::fromValue(chap.release()));
       });
       return l;
   }).value_or(QVariantList());
}

DBResult<long> Book::getTotalTime() {
    using namespace Util::ORM;
    return database().exec(Util::ORM::select(Sel::From(Chapter::table, Expr::sum(Chapter::length)), Chapter::book_id == getId()))
            .bind([] (QSqlQuery &&q) -> DBResult<long> {
        if (q.next())
            return Ok(q.value(0).toInt());
        else
            return Err(QSqlError("Sum didn't return anything"));
    });
}

QVariant Book::getTotalTimeV() {
    return getTotalTime().map([] (long x) {return QVariant::fromValue(x);}).value_or(QVariant());
}

DBResult<std::unique_ptr<Chapter>> Book::getFirstChapter() {
    using namespace Util::ORM;
    return Chapter::selectOne(database(), Chapter::book_id == get(id).value() &&
                 Chapter::chapter == Util::ORM::select(Sel::From(Chapter::table, Expr::min(Chapter::chapter)), Chapter::book_id == getId()));
}

QVariant Book::getFirstChapterV() {
    return result_to_variant(getFirstChapter());
}

DBResult<std::unique_ptr<Chapter>> Book::getChapterAt(long time) {
    using namespace Util::ORM;
    return database().exec(Util::ORM::select(
                               Sel::From(Chapter::table, Chapter::id, Chapter::chapter, Chapter::length),
                               Chapter::book_id == getId(),
                               Sel::OrderBy(Chapter::chapter)))
            .bind([this, &time] (QSqlQuery &&q) -> DBResult<std::unique_ptr<Chapter>> {
        while (q.next()) {
            long len = q.value(2).toInt();
            qDebug() << len;
            if (time < len) {
                return database().load<Chapter>(q.value(0).toInt());
            }
            time -= len;
        }

        return Err(QSqlError("Timestamp past end of book"));
    });
}

DBResult<std::pair<std::unique_ptr<Chapter>, int64_t>> Book::getChapterAt2(long time) {
    using namespace Util::ORM;
    return database().exec(Util::ORM::select(Chapter::table,
                                             Chapter::book_id == getId(),
                                             Sel::OrderBy(Chapter::chapter)))
            .bind([this, &time] (QSqlQuery &&q) -> DBResult<std::pair<std::unique_ptr<Chapter>, int64_t>> {
        while (q.next()) {
            long len = q.value(Chapter::Table::Columns::index<decltype(Chapter::chapter)>).toInt();
            qDebug() << len;
            if (time < len) {
                return Ok(std::pair{std::make_unique<Chapter>(database(), q.record()), time});
            }
            time -= len;
        }

        return Err(QSqlError("Timestamp past end of book"));
    });
}

QVariant Book::getChapterAtV(long time) {
    return result_to_variant(getChapterAt(time));
}

DBResult<QImage> Book::getCover() {
    auto id = get(cover_blob_id);
    if (!id)
        return Ok(QImage());

    return database().load<Blob>(id.value()).map([] (auto blob) {
        return blob->toImage();
    });
}

}
