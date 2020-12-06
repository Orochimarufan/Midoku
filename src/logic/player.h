#pragma once

#include "library/book.h"
#include "library/chapter.h"
#include "library/progress.h"
#include "mpv/mpv.h"
#include "error.h"

#include <QObject>
#include <QTimer>


namespace Midoku {

class Player : public QObject
{
    Q_OBJECT

    Mpv mpv;

    QTimer update_timer;

    Library::BookPtr mp_book;
    Library::ChapterPtr mp_chapter;
    Library::ProgressPtr mp_progress;

    qint64 chapter_time_offset;
    qint64 chapter_media_offset;
    qint64 book_duration;
    qint64 book_time;
    qint64 chapter_time;
    qint64 chapter_duration;

    Q_PROPERTY(Library::Book *book READ book NOTIFY bookChanged)
    Q_PROPERTY(Library::Chapter *chapter READ chapter NOTIFY chapterChanged)

    Q_PROPERTY(qint64 time_book READ timeBook NOTIFY timeChanged)
    Q_PROPERTY(qint64 time_chapter READ timeChapter NOTIFY timeChanged)
    Q_PROPERTY(qint64 duration_book READ durationBook NOTIFY bookChanged)
    Q_PROPERTY(qint64 duration_chapter READ durationChapter NOTIFY chapterDurationChanged)

    void updateTime();
    void updateProgress();
    void onPause(bool);

    Q_PROPERTY(Mpv *mpv READ getMpv)
    //Q_PROPERTY(bool pause READ pause WRITE pause NOTIFY pauseChanged)

public:
    Player(QObject *parent);

    // Properties
    inline Mpv *getMpv() {
        return &mpv;
    }

    inline Library::Book *book() {
        return mp_book.get();
    }

    inline Library::Chapter *chapter() {
        return mp_chapter.get();
    }

    inline qint64 timeChapter() {
        return chapter_time;
    }

    inline qint64 durationChapter() {
        return chapter_duration;
    }

    inline qint64 timeBook() {
        return book_time;
    }

    inline qint64 durationBook() {
        return book_duration;
    }

    /*inline bool pause() {
        return mpv.pause();
    }

    inline void pause(bool pause) {
        mpv.set_pause(pause);
    }*/

    // Actions
    Result<void> setBook(Library::BookPtr &&book);
    Result<void> playBook(Library::BookPtr &&book);
    Result<void> seekBook(qint64 total_time);

    Result<void> playChapter(Library::ChapterPtr &&chap, long start_time=0, bool start=true);
    Result<void> playResume(Library::ProgressPtr &&prog, bool start=true);
    void seekChapter(qint64 time);
    Result<void> seekRelative(qint64 time);
    Result<bool> nextChapter();
    Result<bool> previousChapter();

signals:
    void timeChanged();
    void chapterDurationChanged();
    void bookChanged(Library::Book *);
    void chapterChanged(Library::Chapter *);
    //void pauseChanged();
};

}
