#include "player.h"

#include <QQmlEngine>


using namespace Midoku::Util;
using namespace Midoku::Library;


namespace Midoku {

Player::Player(QObject *parent) :
    QObject(parent),
    mpv(this)
{
    book_time = chapter_time = book_duration = 0;
    connect(&mpv, &Mpv::timeChanged, this, &Player::updateTime);
    //connect(&mpv, &Mpv::pauseChanged, this, &Player::pauseChanged);
    connect(&mpv, &Mpv::pauseChanged, this, &Player::updateProgress);
    connect(this, &Player::chapterChanged, this, &Player::chapterDurationChanged);
    connect(&mpv, &Mpv::endOfFile, this, &Player::nextChapter);
}

void Player::updateTime() {
    chapter_time = mpv.time() - chapter_media_offset;
    book_time = chapter_time_offset + chapter_time;

    if (chapter_time > chapter_duration) {
        // update chapter info
        mp_chapter->getNextChapter().map([this](auto chp) -> void {
            if (!chp)
                return;

            chapter_time_offset += chapter_duration;
            chapter_time -= chapter_duration;
            mp_chapter = std::move(*chp);
            chapter_media_offset = mp_chapter->get(Chapter::media_offset).value_or(0);
            chapter_duration = mp_chapter->get(Chapter::length);
            emit chapterChanged(mp_chapter.get());
        });
    }
    emit timeChanged();
}

void Player::updateProgress() {
    if (mp_progress && !mpv.pause()) {
        auto r = mp_progress->update(*mp_chapter, chapter_time);
        if (!r)
            qDebug() << "Progress: Failed" << r.error();
    }
}

Result<void> Player::setBook(BookPtr &&book) {
    mp_book = std::move(book);
    QQmlEngine::setObjectOwnership(mp_book.get(), QQmlEngine::CppOwnership); // 念の為
    return mp_book->getTotalTime().map([this] (long l) {
        book_duration = l;
        emit bookChanged(mp_book.get());
    });
}

Result<void> Player::playBook(BookPtr &&book) {
    return setBook(std::move(book)).bind([this]{
        return mp_book->getProgress().bind([this](ProgressPtr p) {
            return playResume(std::move(p));
        });
    });
}

Result<void> Player::seekBook(qint64 total_time) {
    return mp_book->getChapterAt2(total_time)
            .bind([this](std::pair<ChapterPtr, int64_t> p) -> Result<void> {
        auto [c, time] = std::move(p);
        if (c)
            return playChapter(std::move(c), time);
        return Err(Error("Seek out of range"));
    });
}

Result<void> Player::playChapter(ChapterPtr &&c, long start_time, bool start) {
    mp_chapter = std::move(c);
    QQmlEngine::setObjectOwnership(mp_chapter.get(), QQmlEngine::CppOwnership); // 念の為
    return mp_chapter->getTotalOffset().bind([this, start_time, start](long o) {
        chapter_time_offset = o;
        chapter_media_offset = mp_chapter->get(Chapter::media_offset).value_or(0);
        chapter_duration = mp_chapter->get(Chapter::length);
        chapter_time = start_time;
        auto start_offset = chapter_media_offset + start_time;
        emit chapterChanged(mp_chapter.get());
        QStringList opts;
        if (start_offset > 0)
            opts << QStringLiteral("start=%1").arg(start_offset);
        //if (!start) // this doesn't seem to work properly.
        //    opts << "pause";
        mpv.command(QVariantList() << "loadfile" << mp_chapter->get(Chapter::media) << "replace" << opts.join(","));
        mpv.set_pause(!start);
        return Ok();
    });
}

Result<void> Player::playResume(Library::ProgressPtr &&prog, bool start) {
    mp_progress = std::move(prog);
    QQmlEngine::setObjectOwnership(mp_progress.get(), QQmlEngine::CppOwnership);
    return [this]() -> Result<void> {
        if (!mp_book || mp_book && mp_book->getId() != mp_progress->get(Progress::book_id))
            return mp_progress->getBook().bind([this](auto b) {
                return setBook(std::move(b));
            });
        return Ok();
    }().bind([this, start] () {
        return mp_progress->getChapter().bind([this,start](auto chap) {
            return playChapter(std::move(chap), mp_progress->get(Progress::time), start);
        });
    });
}

void Player::seekChapter(qint64 time) {
    mpv.set_time(chapter_media_offset + time);
}

Result<bool> Player::nextChapter() {
    return mp_chapter->getNextChapter().bind([this](std::optional<ChapterPtr> c) -> Result<bool> {
        if (c)
            return playChapter(std::move(c.value())).map([]{return true;});
        return Ok(false);
    });
}

Result<bool> Player::previousChapter() {
    return mp_chapter->getPreviousChapter().bind([this](std::optional<ChapterPtr> c) -> Result<bool> {
        if (c)
            return playChapter(std::move(c.value())).map([]{return true;});
        return Ok(false);
    });
}

}
