#include "app.h"

#include "error.h"
#include "library/blob.h"
#include "library/progress.h"
#include "mpris.h"

#include <QDBusConnection>
#include <QDBusError>
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
    mp_db(db)
{
    // D-Bus
    new Mpris::MediaPlayer2Adaptor(&m_player);
    new Mpris::PlayerAdaptor(&m_player);
    auto bus = QDBusConnection::sessionBus();
    bus.registerObject("/org/mpris/MediaPlayer2", &m_player);
    if (!bus.registerService(QLatin1String("org.mpris.MediaPlayer2.midoku"))) {
        auto err = bus.lastError();
        if (err.type() == QDBusError::AddressInUse) {
            if (!bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.midoku.p%1").arg(getpid())))
                qWarning() << "Could not register MPRIS2 service" << bus.lastError().message();
        } else {
            qWarning() << "Could not register MPRIS2 service" << err.message();
        }
    }
}

// QML
bool App::playBook(long bookid) {
    return mp_db->load<Library::Book>(bookid).bind([this] (Library::BookPtr book) -> Result<void> {
        return m_player.playBook(std::move(book));
    });
}

bool App::seekBook(long total_time) {
    auto r = m_player.seekBook(total_time);
    if (!r)
        r.error().debugPrint();
    return r;
}

bool App::seekChapter(long time) {
    m_player.seekChapter(time);
    return true;
}

bool App::seekRelative(long diff) {
    return m_player.seekRelative(diff);
}

bool App::nextChapter() {
    return m_player.nextChapter().value_or(false);
}

bool App::previousChapter() {
    return m_player.previousChapter().value_or(false);
}

bool App::loadRecentBook() {
    return Library::Progress::findMostRecent(*mp_db).bind([this] (auto p) -> Result<void> {
        if (p)
            return m_player.playResume(std::move(p), false);
        return Util::Ok();
    });
}


QVariant App::booksModel() {
    auto model = new Library::TableModel(nullptr, mp_db->qsqldb(), Library::Book::table);
    if(!model->select())
        qDebug() << "Select error" << model->lastError();
    qDebug() << "Model:" << model->tableName() << model->rowCount();
    return QVariant::fromValue(model);
}

}
