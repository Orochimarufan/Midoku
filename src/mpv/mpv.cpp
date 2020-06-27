#include "mpv.h"

#include <array>
#include <cstring>

#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QGuiApplication>

namespace {
// see libmpv/qthelper.hpp
static inline QVariant node_to_variant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        return QVariant(QString::fromUtf8(node->u.string));
    case MPV_FORMAT_FLAG:
        return QVariant(static_cast<bool>(node->u.flag));
    case MPV_FORMAT_INT64:
        qDebug() << "INT64: " << node->u.int64;
        return QVariant(static_cast<qlonglong>(node->u.int64));
    case MPV_FORMAT_DOUBLE:
        return QVariant(node->u.double_);
    case MPV_FORMAT_NODE_ARRAY: {
        mpv_node_list *list = node->u.list;
        QVariantList qlist;
        for (int n = 0; n < list->num; n++)
            qlist.append(node_to_variant(&list->values[n]));
        return QVariant(qlist);
    }
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = node->u.list;
        QVariantMap qmap;
        for (int n = 0; n < list->num; n++) {
            qmap.insert(QString::fromUtf8(list->keys[n]),
                        node_to_variant(&list->values[n]));
        }
        return QVariant(qmap);
    }
    default: // MPV_FORMAT_NONE, unknown values (e.g. future extensions)
        qDebug() << "Mpv/node_to_variant: Unknown Format: " << node->format;
        return QVariant();
    }
}

struct node_builder {
    node_builder(const QVariant& v) {
        set(&node_, v);
    }
    ~node_builder() {
        free_node(&node_);
    }
    mpv_node *node() { return &node_; }
private:
    Q_DISABLE_COPY(node_builder)
    mpv_node node_;
    mpv_node_list *create_list(mpv_node *dst, bool is_map, unsigned int num) {
        dst->format = is_map ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NODE_ARRAY;
        mpv_node_list *list = new mpv_node_list();
        dst->u.list = list;
        if (!list)
            goto err;
        list->values = new mpv_node[num]();
        if (!list->values)
            goto err;
        if (is_map) {
            list->keys = new char*[num]();
            if (!list->keys)
                goto err;
        }
        return list;
    err:
        free_node(dst);
        return nullptr;
    }
    char *dup_qstring(const QString &s) {
        QByteArray b = s.toUtf8();
        char *r = new char[b.size() + 1];
        if (r)
            std::memcpy(r, b.data(), b.size() + 1);
        return r;
    }
    bool test_type(const QVariant &v, QMetaType::Type t) {
        // The Qt docs say: "Although this function is declared as returning
        // "QVariant::Type(obsolete), the return value should be interpreted
        // as QMetaType::Type."
        // So a cast really seems to be needed to avoid warnings (urgh).
        return static_cast<int>(v.type()) == static_cast<int>(t);
    }
    void set(mpv_node *dst, const QVariant &src) {
        if (test_type(src, QMetaType::QString)) {
            dst->format = MPV_FORMAT_STRING;
            dst->u.string = dup_qstring(src.toString());
            if (!dst->u.string)
                goto fail;
        } else if (test_type(src, QMetaType::Bool)) {
            dst->format = MPV_FORMAT_FLAG;
            dst->u.flag = src.toBool() ? 1 : 0;
        } else if (test_type(src, QMetaType::Int) ||
                   test_type(src, QMetaType::LongLong) ||
                   test_type(src, QMetaType::UInt) ||
                   test_type(src, QMetaType::ULongLong))
        {
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = src.toLongLong();
        } else if (test_type(src, QMetaType::Double)) {
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = src.toDouble();
        } else if (src.canConvert<QVariantList>()) {
            QVariantList qlist = src.toList();
            mpv_node_list *list = create_list(dst, false, qlist.size());
            if (!list)
                goto fail;
            list->num = qlist.size();
            for (int n = 0; n < qlist.size(); n++)
                set(&list->values[n], qlist[n]);
        } else if (src.canConvert<QVariantMap>()) {
            QVariantMap qmap = src.toMap();
            mpv_node_list *list = create_list(dst, true, qmap.size());
            if (!list)
                goto fail;
            list->num = qmap.size();
            for (int n = 0; n < qmap.size(); n++) {
                list->keys[n] = dup_qstring(qmap.keys()[n]);
                if (!list->keys[n]) {
                    free_node(dst);
                    goto fail;
                }
                set(&list->values[n], qmap.values()[n]);
            }
        } else {
            goto fail;
        }
        return;
    fail:
        dst->format = MPV_FORMAT_NONE;
    }
    void free_node(mpv_node *dst) {
        switch (dst->format) {
        case MPV_FORMAT_STRING:
            delete[] dst->u.string;
            break;
        case MPV_FORMAT_NODE_ARRAY:
        case MPV_FORMAT_NODE_MAP: {
            mpv_node_list *list = dst->u.list;
            if (list) {
                for (int n = 0; n < list->num; n++) {
                    if (list->keys)
                        delete[] list->keys[n];
                    if (list->values)
                        free_node(&list->values[n]);
                }
                delete[] list->keys;
                delete[] list->values;
            }
            delete list;
            break;
        }
        default: ;
        }
        dst->format = MPV_FORMAT_NONE;
    }
};

/**
 * RAII wrapper that calls mpv_free_node_contents() on the pointer.
 */
struct node_autofree {
    mpv_node *ptr;
    node_autofree(mpv_node *a_ptr) : ptr(a_ptr) {}
    ~node_autofree() { mpv_free_node_contents(ptr); }
};
}

// mpv_type::convert specialization for QVariant
namespace mpv_type {
inline convert<QVariant>::AutoNode::AutoNode()
{
    format = MPV_FORMAT_NONE;
}

inline convert<QVariant>::AutoNode::AutoNode(AutoNode &&n)
{
    format = n.format;
    u = n.u;
    n.format = MPV_FORMAT_NONE;
}

inline convert<QVariant>::AutoNode::~AutoNode()
{
    mpv_free_node_contents(this);
}

inline std::optional<QVariant> convert<QVariant>::from(mpv_type *nd)
{
    return node_to_variant(nd);
}

static inline constexpr bool qv_is_type(const QVariant &v, QMetaType::Type t)
{
    return static_cast<QMetaType::Type>(v.type()) == t;
}

static inline bool set_node(mpv_node *n, const QVariant &v)
{
    if (qv_is_type(v, QMetaType::Bool))
    {
        n->format = MPV_FORMAT_FLAG;
        n->u.flag = v.toBool() ? 1 : 0;
    }
    else if (qv_is_type(v, QMetaType::Int)
             || qv_is_type(v, QMetaType::Long)
             || qv_is_type(v, QMetaType::LongLong))
    {
        n->format = MPV_FORMAT_INT64;
        n->u.int64 = v.toLongLong();
    }
    else if (qv_is_type(v, QMetaType::Float)
             || qv_is_type(v, QMetaType::Double))
    {
        n->format = MPV_FORMAT_DOUBLE;
        n->u.double_ = v.toDouble();
    }
    else if (qv_is_type(v, QMetaType::QString))
    {
        n->format = MPV_FORMAT_STRING;
        n->u.string = v.toString().toUtf8().data();
    }
    else
        return false;
    return true;
}

inline std::optional<convert<QVariant>::mpv_type> convert<QVariant>::to(const QVariant &v)
{
    mpv_type n;
    if (!set_node(&n, v))
        return {};
    return n;
}
}

// Mpv Comms Thread
class MpvThread : public QThread
{
    Mpv *mpv;
    bool shutting_down;

    virtual void run() override
    {
        while (!shutting_down)
        {
            mpv_event *e = mpv_wait_event(mpv->mpv, 10);

            if (e->event_id != MPV_EVENT_NONE)
                mpv->processEvent(e);

            if (e->event_id == MPV_EVENT_SHUTDOWN)
                break;
        }
    }

public:
    MpvThread(Mpv *mpv) :
        QThread(mpv),
        mpv(mpv),
        shutting_down(false)
    {
    }

    void requestStop() {
        shutting_down = true;
        mpv_wakeup(mpv->mpv);
    }
};

// --------------------------------------------------------
// Mpv class implementation
// --------------------------------------------------------
Mpv::Mpv(QObject *parent)
    : QObject (parent),
      mpv{mpv_create()}
{
    if (!mpv)
        throw std::runtime_error("Could not create mpv context");

    //mpv_set_option_string(mpv, "terminal", "yes");
    //mpv_set_option_string(mpv, "msg-level", "all=v");

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("Could not initialize mpv context");

    int _false = 0;
    //mpv_set_option_string(mpv, "hwdec", "auto");
    mpv_set_property(mpv, "ytdl", MPV_FORMAT_FLAG, &_false);
    mpv_set_property(mpv, "video", MPV_FORMAT_FLAG, &_false);

    // Observe property changes
    registerObservers();

    // Doesn't work?
    //mpv_set_wakeup_callback(mpv, wakeup, this);

    // Do blocking comms in thread instead
    mpv_thread = new MpvThread(this);
    mpv_thread->start();

    // FIXME: should really do this properly, but MpvThread isn't currently run through MOC
    connect(QGuiApplication::instance(), &QCoreApplication::aboutToQuit, [this]() {mpv_thread->requestStop();});
}

Mpv::~Mpv()
{
    if (mpv_thread)
        mpv_thread->requestStop();
    mpv_terminate_destroy(mpv);
}

// Event stuff
void Mpv::wakeup(void *ctx)
{
    // Schedule a call to processEvents
    qDebug() << "Mpv/wakeup: Woke up.";
    //QTimer::singleShot(0, reinterpret_cast<Mpv*>(ctx), &Mpv::processEvents);
    QMetaObject::invokeMethod(reinterpret_cast<Mpv*>(ctx), &Mpv::processEvents, Qt::QueuedConnection);
    //reinterpret_cast<Mpv*>(ctx)->processEvents();
}

// ----------------------------------------------------------------------------
// Property & Type black magic
struct mpv_property_meta {
    template<typename T, int id>
    using property = Mpv::mpv_property<T, id>;

    template<typename T>
    using property_count = Mpv::mpv_property_count<T>;

    using property_types = Mpv::mpv_property_types;

    // --------------------------------------------------------------
    // Registering MPV properties
    static inline void register_all(mpv_handle *mpv) {
        register_for_types(mpv, property_types());
    }

    // Register properties of given types with mpv
    template<typename... Ts>
    static inline void register_for_types(mpv_handle *mpv, mpv_type::seq<Ts...>) {
        (register_for_type<Ts>(mpv), ...);
    }

    template<typename T>
    static inline void register_for_type(mpv_handle *mpv) {
        register_for_type_seq<T>(mpv, std::make_index_sequence<property_count<T>::value>());
    }

    template<typename T, std::size_t... Is>
    static inline void register_for_type_seq(mpv_handle *mpv, std::index_sequence<Is...>) {
        (register_property<property<T, Is>>(mpv), ...);
    }

    // Register a property with mpv
    template <typename Prop>
    static inline void register_property(mpv_handle *mpv) {
        qDebug() << "Mpv/register_property: Observing" << Prop::mpv_name << "as" << Prop::qt_name << "(" << typeid(typename Prop::type).name() << Prop::id << ")";
        if (mpv_observe_property(mpv, Prop::id, Prop::mpv_name, mpv_type::format<typename Prop::type>::id))
            qWarning() << "Mpv/register_property: Failed to observe" << Prop::mpv_name;
    }

    // --------------------------------------------------------------
    // Signal Tables
    template <typename T>
    using signal_t = void (Mpv::*)(const T &);

    template <typename T>
    using signal_table_t = std::array<signal_t<T>, property_count<T>::value>;

    // construct signal table for type
    template <typename T>
    static constexpr auto signals_for_type() {
        return signals_for_type_seq<T>(std::make_index_sequence<property_count<T>::value>());
    }

    template<typename T, std::size_t... Is>
    static constexpr auto signals_for_type_seq(std::index_sequence<Is...>) {
        return signal_table_t<T>{property<T, Is>::signal...};
    }

    // static tables
    template <typename T>
    static constexpr signal_table_t<T> signal_table = signals_for_type<T>();

    // --------------------------------------------------------------
    // Emitting
    template <typename T>
    inline static void try_emit(Mpv *self, mpv_event_property *p, uint64_t id) {
        std::optional<T> v = mpv_type::property<T>::unpack_event(p);
        if (v.has_value()) {
            //qDebug() << "Mpv/try_emit:" << p->name << "=" << v.value();
            emit (self->*signal_table<T>[id])(v.value());
        } else
            qWarning() << "Mpv/try_emit: Wrong Format: Expected" << mpv_type::format<T>::id << "but got" << p->format << "with id" << id;
    }

    // Type inference
    // Infer signal type from given format by recursing over mpv_property_types
    static inline void try_emit_infer(Mpv *mpv, mpv_event_property *p, uint64_t u) {
        try_emit_infer(mpv, p, u, property_types());
    }

    template<typename A, typename... Ts>
    static inline void try_emit_infer(Mpv *mpv, mpv_event_property *p, uint64_t u, mpv_type::seq<A, Ts...>) {
        if (p->format == mpv_type::format<A>::id)
            return try_emit<A>(mpv, p, u);
        return try_emit_infer(mpv, p, u, mpv_type::seq<Ts...>());
    }

    template<typename...>
    static inline void try_emit_infer(Mpv *, mpv_event_property *p, uint64_t u, mpv_type::seq<>) {
        // Default case
        qWarning() << "Mpv/processEvents: Unknown format in property notification:" << p->format << u;
    }
};

void Mpv::registerObservers()
{
    mpv_property_meta::register_all(mpv);
}

// ----------------------------------------------------------------------------
// Events
void Mpv::processEvents()
{
    mpv_event *e;

    while (true)
    {
        e = mpv_wait_event(mpv, 0);
        if (e->event_id == MPV_EVENT_NONE)
            return;

        processEvent(e);
    }
}

void Mpv::processEvent(mpv_event *e) {
    // Observers
    if (e->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        mpv_event_property *p = reinterpret_cast<mpv_event_property*>(e->data);
        //qDebug() << "Mpv/processEvents: Property notification: " << p->name;
        if (p->format == MPV_FORMAT_NONE) {
            qWarning() << "Mpv/processEvents: Got MPV_FORMAT_NONE property notification";
        } else {
            mpv_property_meta::try_emit_infer(this, p, e->reply_userdata);
        }
    }

    else if (e->event_id == MPV_EVENT_END_FILE) {
        mpv_event_end_file *ef = reinterpret_cast<mpv_event_end_file*>(e->data);
        qDebug() << "Mpv/end-file" << ef->reason;
        if (ef->reason == MPV_END_FILE_REASON_EOF)
            emit endOfFile();
    }

    else
        qDebug() << "Mpv/Event: " << mpv_event_name(e->event_id);
}

QVariant Mpv::command(const QVariant &params)
{
    qDebug() << "Mpv/command: " << params;
    node_builder node(params);
    mpv_node res;
    if (mpv_command_node(mpv, node.node(), &res) < 0)
        return QVariant();
    node_autofree f(&res);
    return node_to_variant(&res);
}

bool Mpv::setMpvProperty(const QString &name, const QVariant &value)
{
    return mpv_type::property<QVariant>::set(mpv, name.toUtf8().data(), value);
}

QVariant Mpv::getMpvProperty(const QString &name)
{
    qDebug() << "Mpv/getMpvProperty " << name;
    return mpv_type::property<QVariant>::get(mpv, name.toUtf8().constData()).value_or(QVariant());
}
