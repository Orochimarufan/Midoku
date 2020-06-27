#pragma once

#include <QObject>

#include "mpv_type.h"


// ========================================================
// MPV Player Interface
// ========================================================
#define MPV_PROPERTY(type_, id_, name, mpv_name_) \
    Q_PROPERTY(type_ name READ name WRITE set_ ## name NOTIFY name ## Changed) \
    public: type_ name() { return getPropertyValue<type_>(mpv_name_).value_or(type_{}); } \
    void set_ ## name(const type_ &value) { setPropertyValue(mpv_name_, value); } \
    Q_SIGNALS: void name ## Changed(const type_ &); \
    private:\
    template<> struct mpv_property<type_, id_> {\
        using type = type_;\
        static constexpr int id = id_;\
        static constexpr const char *qt_name = #name;\
        static constexpr const char *mpv_name = mpv_name_;\
        static constexpr void (Mpv::*signal)(const type_ &) = &Mpv::name ## Changed;\
    };

#define MPV_PROPERTY_MAX(type, count) \
    template<> struct mpv_property_count<type> {\
        static constexpr int value = count;\
    };

class MpvThread;

class Mpv : public QObject
{
    Q_OBJECT

    mpv_handle *mpv;

    friend class MpvThread;
    MpvThread *mpv_thread;

    // MPV Properties
    template <typename T, int id> struct mpv_property {};
    template <typename T> struct mpv_property_count { /* static int value */ };
    friend struct mpv_property_meta;

    MPV_PROPERTY(bool, 0, pause, "pause")
    MPV_PROPERTY_MAX(bool, 1)

    MPV_PROPERTY(qint64, 0, time, "time-pos")
    MPV_PROPERTY(qint64, 1, duration, "duration")
    MPV_PROPERTY_MAX(qint64, 2)

    MPV_PROPERTY(double, 0, speed, "speed")
    MPV_PROPERTY_MAX(double, 1)

    MPV_PROPERTY(QString, 0, mediaTitle, "media-title")
    MPV_PROPERTY(QString, 1, mediaAlbum, "metadata/by-key/album")
    MPV_PROPERTY(QString, 2, mediaArtist, "metadata/by-key/artist")
    MPV_PROPERTY_MAX(QString, 3)

    using mpv_property_types = mpv_type::seq<bool, qint64, double, QString>;

public:
    Mpv(QObject *parent=nullptr);
    virtual ~Mpv();

    inline mpv_handle *handle()
    {
        return mpv;
    }

    template<typename T>
    std::optional<T> getPropertyValue(const QString &name)
    {
        return mpv_type::property<T>::get(mpv, name.toUtf8().constData());
    }

    template<typename T>
    T getPropertyValue(const QString &name, T fallback)
    {
        return getPropertyValue<T>(name).value_or(fallback);
    }

    template<typename T>
    bool setPropertyValue(const QString &name, const T &value)
    {
        return mpv_type::property<T>::set(mpv, name.toUtf8().constData(), value);
    }

public slots:
    QVariant command(const QVariant &params);
    bool setMpvProperty(const QString &name, const QVariant &value);
    QVariant getMpvProperty(const QString &name);

private:
    static void wakeup(void *);
    void registerObservers();
    void processEvent(mpv_event *e);

private slots:
    void processEvents();

signals:
    void endOfFile();
};

#undef MPV_PROPERTY
#undef MPV_PROPERTY_MAX
