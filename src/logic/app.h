#pragma once

#include "player.h"
#include "../library/book.h"
#include "../library/models.h"

#include <QObject>
#include <QQuickImageProvider>
#include <QTimer>

int main(int, char**);

namespace Midoku {

class QmlBlobImageProvider : public QQuickImageProvider {
    Library::Database *db;

public:
    QmlBlobImageProvider(Library::Database *db);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

class App : public QObject
{
    Q_OBJECT

    Player m_player;
    Library::Database *mp_db;
    QTimer m_timer;

    Q_PROPERTY(Player *player READ player);

public:
    explicit App(Library::Database *db);

    inline Player *player() {
        return &m_player;
    }

    Q_INVOKABLE bool playBook(long book);
    Q_INVOKABLE bool seekBook(long total_time);
    Q_INVOKABLE bool seekChapter(long time);
    Q_INVOKABLE bool nextChapter();
    Q_INVOKABLE bool previousChapter();
    Q_INVOKABLE bool prepareLast();

    Q_INVOKABLE QVariant booksModel();

signals:
    void bookChanged();
    void chapterChanged();
};

}
