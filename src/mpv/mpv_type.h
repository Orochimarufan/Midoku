#pragma once

#include <optional>
#include <tuple>
#include <utility>

#include <QString>
#include <QVariant>

#include <mpv/client.h>

// ========================================================
// MPV type conversion
// ========================================================
namespace mpv_type {
// Sequence of types like std::index_sequence
template<typename ...Ts>
struct seq {
    using tuple = std::tuple<Ts...>;
    using size = std::tuple_size<tuple>;
    template<size_t I> using element = std::tuple_element<I, tuple>;
    using make_index_sequence = std::make_index_sequence<size::value>;
};

// mpv/C type mapping
template<typename T>
struct format {
    // static mpv_format id;
};

template<mpv_format ID>
struct ctype {
    // using type;
};

// Generic type conversion template
template<typename T>
struct convert {
    using mpv_type = T;
    static constexpr std::optional<T> from(mpv_type *v)
    {
        return *v;
    }
    static constexpr std::optional<T> to(const T &v)
    {
        return v;
    }
};

// Typed access functions for properties
template<typename T>
struct property {
    static inline std::optional<T> get(mpv_handle *mpv, const char *name) {
        typename convert<T>::mpv_type res;
        if (mpv_get_property(mpv, name, format<T>::id, &res) < 0)
            return {};
        return convert<T>::from(&res);
    }

    static inline bool set(mpv_handle *mpv, const char *name, const T &value) {
        auto mv = convert<T>::to(value);
        if (!mv)
            return false;
        return mpv_set_property(mpv, name, format<T>::id, mv.operator->()) >= 0;
    }

    static inline std::optional<T> unpack_event(mpv_event_property *ep) {
        if (ep->format != format<T>::id)
            return {};
        return convert<T>::from(reinterpret_cast<typename convert<T>::mpv_type*>(ep->data));
    }
};

// ------------------------------------------------------------------
// Concrete Type Specializations
#define MPV_TYPE(type_, fmt) \
    template<> struct format<type_> { static constexpr mpv_format id = fmt; }; \
    template<> struct ctype<fmt> { using type = type_; };

MPV_TYPE(bool, MPV_FORMAT_FLAG)

template<> struct convert<bool> {
    using mpv_type = int;
    static constexpr std::optional<bool> from(int *v)
    {
        return *v == 1;
    }
    static constexpr std::optional<int> to(const bool &v)
    {
        return v ? 1 : 0;
    }
};

MPV_TYPE(qint64, MPV_FORMAT_INT64)
MPV_TYPE(double, MPV_FORMAT_DOUBLE)

MPV_TYPE(QString, MPV_FORMAT_STRING)

template<> struct convert<QString> {
    using mpv_type = char *;
    static inline std::optional<QString> from(char **v)
    {
        return QString::fromUtf8(*v);
    }
    static inline std::optional<char*> to(const QString &v)
    {
        return v.toUtf8().data();
    }
};

MPV_TYPE(QVariant, MPV_FORMAT_NODE)

template<> struct convert<QVariant> {
    struct AutoNode : mpv_node{
        AutoNode();
        AutoNode(AutoNode &&);
        ~AutoNode();
    };
    using mpv_type = AutoNode;
    static std::optional<QVariant> from(AutoNode *);
    static std::optional<AutoNode> to(const QVariant &);
};

#undef MPV_TYPE
}
