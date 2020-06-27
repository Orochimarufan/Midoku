#include "app.h"

#include "error.h"
#include "library/blob.h"
#include "library/progress.h"

#include <QQuickImageProvider>

namespace Midoku {

QmlBlobImageProvider::QmlBlobImageProvider(Library::Database *db) :
    QQuickImageProvider(QQuickImageProvider::Image),
    db(db)
{}

QImage QmlBlobImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    if (id.isEmpty())
        return QImage();
    long blob_id = id.toLong();

    qDebug() << "Image from Blob:" << blob_id;

    QImage img = db->load<Library::Blob>(blob_id).map([] (auto blob) {
        return blob->toImage();
    }).value_or(QImage());

    if (size)
        *size = img.size();
    if (requestedSize.width() > 0 || requestedSize.height() > 0 )
        return img.scaled(requestedSize, Qt::KeepAspectRatio);
    else
        return img;
}

App::App(Library::Database *db) :
    QObject(),
    m_player(this),
    mp_db(db),
    m_timer()
{
    m_timer.setSingleShot(true);
}

// QML
bool App::playBook(long bookid) {
    return mp_db->load<Library::Book>(bookid).bind([this] (Library::BookPtr book) -> Result<void> {
        return m_player.playBook(std::move(book));
    });
}

bool App::seekBook(long total_time) {
    return m_player.seekBook(total_time);
}

bool App::seekChapter(long time) {
    m_player.seekChapter(time);
    return true;
}

bool App::nextChapter() {
    return m_player.nextChapter().value_or(false);
}

bool App::previousChapter() {
    return m_player.previousChapter().value_or(false);
}

bool App::prepareLast() {
    m_timer.callOnTimeout([this] () {
        return Library::Progress::findMostRecent(*mp_db).bind([this] (auto p) {
            return m_player.playResume(std::move(p), false);
        });
    });
    //m_timer.start(1000);
    return false;
}


QVariant App::booksModel() {
    auto model = new Library::TableModel(nullptr, mp_db->qsqldb(), Library::Book::table);
    if(!model->select())
        qDebug() << "Select error" << model->lastError();
    qDebug() << "Model:" << model->tableName() << model->rowCount();
    return QVariant::fromValue(model);
}

}
