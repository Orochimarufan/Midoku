#include "mpris.h"

#include <QCoreApplication>
#include <QStringLiteral>

using Midoku::Library::Chapter;
using Midoku::Library::Book;

namespace Midoku::Mpris {

// org.mpris.MediaPlayer2 -----------------------------------------------------
MediaPlayer2Adaptor::MediaPlayer2Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

// Properties
bool MediaPlayer2Adaptor::CanRaise() {
    return false;
}

bool MediaPlayer2Adaptor::CanQuit() {
    return true;
}

bool MediaPlayer2Adaptor::HasTrackList() {
    return false;
}

QString MediaPlayer2Adaptor::Identity() {
    return QStringLiteral("未読 Midoku Audiobook Player");
}

QStringList MediaPlayer2Adaptor::SupportedUriSchemes() {
    return {};
}

QStringList MediaPlayer2Adaptor::SupportedMimeTypes() {
    return {};
}

// Methods
void MediaPlayer2Adaptor::Raise() {
    // pass
}

void MediaPlayer2Adaptor::Quit() {
    QCoreApplication::instance()->quit();
}

// org.mpris.MediaPlayer2.Player ----------------------------------------------
PlayerAdaptor::PlayerAdaptor(Player *player)
    : QDBusAbstractAdaptor(player)
    , player(player)
    , has_next_chapter(false)
    , has_prev_chapter(false)
{
    connect(player, &Player::seeked, this, &PlayerAdaptor::onSeeked);
    connect(player, &Player::chapterChanged, this, &PlayerAdaptor::onChapterChanged);

    auto mpv = player->getMpv();
    connect(mpv, &Mpv::pauseChanged, this, &PlayerAdaptor::onPauseChanged);
    connect(mpv, &Mpv::volumeChanged, this, &PlayerAdaptor::onVolumeChanged);
    connect(mpv, &Mpv::speedChanged, this, &PlayerAdaptor::RateChanged);
}

// Properties
QString PlayerAdaptor::PlaybackStatus() {
    if (player->chapter() == nullptr) {
        return QStringLiteral("Stopped");
    } else if (player->getMpv()->pause()) {
        return QStringLiteral("Paused");
    } else {
        return QStringLiteral("Playing");
    }
}

double PlayerAdaptor::Rate() {
    return player->getMpv()->speed();
}

void PlayerAdaptor::setRate(double speed) {
    player->getMpv()->set_speed(speed);
}

QVariantMap PlayerAdaptor::Metadata() {
    auto book = player->book();
    auto chapter = player->chapter();
    if (book == nullptr || chapter == nullptr) {
        return {};
    }
    static const auto id_fmt = QStringLiteral("/me/sodimm/oro/Midoku/chapter/%1");
    auto result = QVariantMap{
    {QLatin1String("mpris:trackid"), QDBusObjectPath{id_fmt.arg(chapter->getId())}},
    {QLatin1String("mpris:length"), (qint64)(chapter->get(Chapter::length)*1000)},
    {QLatin1String("xesam:album"), book->get(Book::title)},
    {QLatin1String("xesam:artist"), QStringList{book->get(Book::reader)}},
    {QLatin1String("xesam:composer"), QStringList{book->get(Book::author)}},
    {QLatin1String("xesam:trackNumber"), (qint32)chapter->get(Chapter::chapter)},
    {QLatin1String("xesam:title"), chapter->get(Chapter::title)},
    };
    return result;
}

double PlayerAdaptor::Volume() {
    return player->getMpv()->volume() / 100.0;
}

void PlayerAdaptor::setVolume(double volume) {
    return player->getMpv()->set_volume(volume * 100.0);
}

qint64 PlayerAdaptor::Position() {
    return player->timeChapter() * 1000;
}

double PlayerAdaptor::MinimumRate() {
    return 0.1;
}

double PlayerAdaptor::MaximumRate() {
    return 3.0;
}

bool PlayerAdaptor::CanGoNext() {
    return has_next_chapter;
}

bool PlayerAdaptor::CanGoPrevious() {
    return has_prev_chapter;
}

bool PlayerAdaptor::CanPlay() {
    return player->chapter() != nullptr;
}

bool PlayerAdaptor::CanPause() {
    return player->chapter() != nullptr;
}

bool PlayerAdaptor::CanSeek() {
    return player->chapter() != nullptr;
}

bool PlayerAdaptor::CanControl() {
    return true;
}

// Methods
void PlayerAdaptor::Next() {
    player->nextChapter();
}

void PlayerAdaptor::Previous() {
    player->previousChapter();
}

void PlayerAdaptor::Pause() {
    player->getMpv()->set_pause(true);
}

void PlayerAdaptor::PlayPause() {
    auto mpv = player->getMpv();
    mpv->set_pause(!mpv->pause());
}

void PlayerAdaptor::Stop() {
    player->getMpv()->set_pause(true);
}

void PlayerAdaptor::Play() {
    player->getMpv()->set_pause(false);
}

void PlayerAdaptor::Seek(qint64 pos) {
    player->seekChapter(pos / 1000);
}

void PlayerAdaptor::SetPosition(QDBusObjectPath, qint64) {
    // TODO
}

void PlayerAdaptor::OpenUri(QString) {
    // Not Supported
}

// Notify logic
void PlayerAdaptor::onSeeked() {
    emit Seeked(player->timeChapter() * 1000);
}

void PlayerAdaptor::onChapterChanged(Library::Chapter *chap) {
    emit MetadataChanged(Metadata());
    // TODO: move to Player
    using C = std::optional<std::unique_ptr<Chapter>>;
    has_next_chapter = chap->getNextChapter().map(&C::has_value).value_or(false);
    has_prev_chapter = chap->getPreviousChapter().map(&C::has_value).value_or(false);
    emit CanGoNextChanged(this->has_next_chapter);
    emit CanGoPreviousChanged(this->has_prev_chapter);
    emit CanPauseChanged(true);
    emit CanPlayChanged(true);
    emit CanSeekChanged(true);
}

void PlayerAdaptor::onPauseChanged(bool pause) {
    if (pause)
        emit PlaybackStatusChanged(QStringLiteral("Paused"));
    else
        emit PlaybackStatusChanged(QStringLiteral("Playing"));
}

void PlayerAdaptor::onVolumeChanged(double volume) {
    emit VolumeChanged(volume / 100.0);
}

}
