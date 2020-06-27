#include "progress.h"

namespace Midoku::Library {
DB_OBJECT_IMPL(Progress);

QDateTime Progress::timestampDate() const {
    return QDateTime::fromString(get(timestamp), Qt::ISODate);
}

DBResult<std::unique_ptr<Book>> Progress::getBook() {
    return database().load<Book>(get(book_id));
}

QVariant Progress::getBookV() {
    return result_to_variant(getBook());
}

DBResult<std::unique_ptr<Chapter>> Progress::getChapter() {
    return database().load<Chapter>(get(chapter_id));
}

QVariant Progress::getChapterV() {
    return result_to_variant(getChapter());
}

DBResult<void> Progress::update(long time) {
    set(Progress::time, time);
    set(timestamp, QDateTime::currentDateTime().toString(Qt::ISODate));
    return save().discard();
}

DBResult<void> Progress::update(const Chapter &chap) {
    set(chapter_id, chap.getId());
    set(time, 0);
    set(timestamp, QDateTime::currentDateTime().toString(Qt::ISODate));
    return save().discard();
}

DBResult<void> Progress::update(const Chapter &chap, long time) {
    set(chapter_id, chap.getId());
    set(Progress::time, time);
    set(timestamp, QDateTime::currentDateTime().toString(Qt::ISODate));
    return save().discard();
}

bool Progress::update(QVariant v) {
    if (v.canConvert<Chapter*>()) {
        return update(*(v.value<Chapter*>()));
    } else if (v.canConvert<long>()) {
        return update(v.value<long>());
    } else {
        return false;
    }
}

DBResult<std::optional<std::unique_ptr<Progress>>> Progress::findMostRecentForBook(Database &db, const Book &b) {
    // TODO: make select<> be able to do this
    using namespace Util::ORM;
    return Progress::selectOne(db, Progress::book_id == b.getId(), Sel::OrderBy(Expr::datetime(Progress::timestamp), true))
            .map([] (std::unique_ptr<Progress> p) -> std::optional<std::unique_ptr<Progress>> {
        if (!p)
            return std::nullopt;
        else
            return p;
    });
}

DBResult<std::unique_ptr<Progress>> Progress::findMostRecent(Database &db) {
    using namespace Util::ORM;
    return Progress::selectOne(db, Sel::OrderBy(Expr::datetime(Progress::timestamp), true));
}

QVariant Book::getMostRecentProgressV() {
    return Progress::findMostRecentForBook(database(), *this)
            .map([] (auto opt) {return opt ? QVariant::fromValue(opt.value().release()) : QVariant();})
            .value_or(QVariant());
}

DBResult<std::unique_ptr<Progress>> Progress::forBook(Database &db, Book &book) {
    return findMostRecentForBook(db, book)
        .bind([&book,&db] (auto opt) -> DBResult<std::unique_ptr<Progress>> {
            if (opt)
                return Ok(std::move(opt.value()));
            else
                return book.getFirstChapter().bind([&db] (auto chap) {
                    return Ok(std::make_unique<Progress>(db, *chap, 0, QDateTime::currentDateTime()));
                });
        });
}

DBResult<std::unique_ptr<Progress>> Book::getProgress() {
    return Progress::forBook(database(), *this);
}

QVariant Book::getProgressV() {
    return result_to_variant(Progress::forBook(database(), *this));
}

}
