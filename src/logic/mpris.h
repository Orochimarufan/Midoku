#pragma once

#include <QVariant>
#include <QMap>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

#include "player.h"

namespace Midoku::Mpris {

class MediaPlayer2Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanRaise READ CanRaise CONSTANT)
    Q_PROPERTY(bool CanQuit READ CanQuit CONSTANT)
    Q_PROPERTY(bool HasTrackList READ HasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ Identity CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ SupportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ SupportedMimeTypes CONSTANT)

public:
    MediaPlayer2Adaptor(QObject *parent);

    bool CanRaise();
    bool CanQuit();
    bool HasTrackList();
    QString Identity();
    QStringList SupportedUriSchemes();
    QStringList SupportedMimeTypes();

public slots:
    Q_NOREPLY void Raise();
    Q_NOREPLY void Quit();
};

class PlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ PlaybackStatus NOTIFY PlaybackStatusChanged)
    Q_PROPERTY(double Rate READ Rate NOTIFY RateChanged)
    Q_PROPERTY(QVariantMap Metadata READ Metadata NOTIFY MetadataChanged)
    Q_PROPERTY(double Volume READ Volume NOTIFY VolumeChanged)
    Q_PROPERTY(qint64 Position READ Position)
    Q_PROPERTY(double MinimumRate READ MinimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ MaximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ CanGoNext NOTIFY CanGoNextChanged)
    Q_PROPERTY(bool CanGoPrevious READ CanGoPrevious NOTIFY CanGoPreviousChanged)
    Q_PROPERTY(bool CanPlay READ CanPlay NOTIFY CanPlayChanged)
    Q_PROPERTY(bool CanPause READ CanPause NOTIFY CanPauseChanged)
    Q_PROPERTY(bool CanSeek READ CanSeek NOTIFY CanSeek)
    Q_PROPERTY(bool CanControl READ CanControl CONSTANT)

    Player *player;
    bool has_next_chapter;
    bool has_prev_chapter;

public:
    PlayerAdaptor(Player *p);

    QString PlaybackStatus();

    double Rate();
    void setRate(double);

    QVariantMap Metadata();

    double Volume();
    void setVolume(double);

    qint64 Position();

    double MinimumRate();
    double MaximumRate();

    bool CanGoNext();
    bool CanGoPrevious();
    bool CanPlay();
    bool CanPause();
    bool CanSeek();
    bool CanControl();

public slots:
    Q_NOREPLY void Next();
    Q_NOREPLY void Previous();
    Q_NOREPLY void Pause();
    Q_NOREPLY void PlayPause();
    Q_NOREPLY void Stop();
    Q_NOREPLY void Play();
    Q_NOREPLY void Seek(qint64);
    Q_NOREPLY void SetPosition(QDBusObjectPath, qint64);
    Q_NOREPLY void OpenUri(QString);

signals:
    void Seeked(qint64);

    void PlaybackStatusChanged(QString);
    void RateChanged(double);
    void MetadataChanged(QVariantMap);
    void VolumeChanged(double);
    void CanGoNextChanged(bool);
    void CanGoPreviousChanged(bool);
    void CanPlayChanged(bool);
    void CanPauseChanged(bool);
    void CanSeekChanged(bool);

private slots:
    // Seeked must be called into after chapterChanged if start_time != 0
    void onSeeked();
    void onChapterChanged(Midoku::Library::Chapter *);
    void onPauseChanged(bool);
    void onVolumeChanged(double);
};

}
