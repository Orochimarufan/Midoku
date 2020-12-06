#pragma once

#include "../util/result.h"
#include "../util/tuple_util.h"
#include "../util/orm.h"

#include <memory>

#include <QString>
#include <QObject>
#include <QVariant>
#include <QDebug>

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>


namespace Midoku::Library {

// result type
template<typename T>
using DBResult = Util::Result<T, QSqlError>;

using Util::Ok;
using Util::Err;


/**
 * @brief The Database class
 */
class Database : public QObject
{
    QString name;

    DBResult<void> remove_key(const QString &tbl, long key);
    DBResult<bool> contains_key(const QString &tbl, long key);

public:
    Database(QString path);
    virtual ~Database();

    QSqlDatabase qsqldb();

    // SQL Queries
    QSqlQuery prepare(QString sql);
    DBResult<void> exec(QSqlQuery &);

    // Handle bound SQL queries
    template <typename... Ts>
    QSqlQuery prepare(Util::ORM::SQL<Ts...> sql) {
        auto q = prepare(sql.query);
        Util::tuple_enumerate_foreach(sql.binds, [&q](size_t i, auto x) {
            q.bindValue(i, QVariant::fromValue(x));
        });
        return q;
    }

    template <typename... Ts>
    DBResult<QSqlQuery> exec(Util::ORM::SQL<Ts...> sql) {
        auto q = prepare(sql);
        return exec(q).map([q(std::move(q))]() {return std::move(q);});
    }

    template <typename... Ts>
    DBResult<QSqlQuery> exec(const QString &query, Ts&&... binds) {
        auto q = prepare(query);
        Util::tuple_enumerate_foreach(std::tuple(std::forward<Ts>(binds)...), [&q](size_t i, auto x) {
            q.bindValue(i, QVariant::fromValue(x));
        });
        return exec(q).map([q(std::move(q))]() {return std::move(q);});
    }

    template <Util::ORM::SQLQuery Q>
    DBResult<QSqlQuery> exec(Q q) {
        return exec(q.sqlQuery());
    }

    // Existence
    template<typename T>
    inline DBResult<bool> exists(long id) {
        if (id < 0)
            return Ok(false);
        return contains_key(T::Table::name.value, id);
    }

    template<typename T>
    inline DBResult<bool> exists(const T &o) {
        return exists<T>(o.getId());
    }

    template<typename T>
    inline DBResult<bool> exists(T *o) {
        return exists<T>(o->getId());
    }

    // Load
    template<typename T>
    inline DBResult<std::unique_ptr<T>> load(long id) {
        return T::load(*this, id);
    }

    template<typename T>
    inline DBResult<std::vector<std::unique_ptr<T>>> list() {
        return T::list(*this);
    }

    template<typename T, typename... Sel>
    inline DBResult<std::vector<std::unique_ptr<T>>> select(Sel... s) {
        return T::select(*this, s...);
    }
};


namespace detail {
M_TEMPLATE_FUNCTOR(auto, unpack_functor, Col, (const QSqlRecord &r), {
    const QVariant &v = r.value(Col::name.value);
    //qDebug() << "Unpack" << typeid(typename Col::type).name() << Col::name.value << v;
    if constexpr (Col::is_nullable)
        return v.isNull()? Col::nullable_type::unit() : Col::nullable_type::value(v.template value<typename Col::value_type>());
    else
        return v.template value<typename Col::type>();
});

template <typename T>
QVariant qvariant_from_optional(const std::optional<T> &v) {
    if (v)
        return QVariant::fromValue(v.value());
    else
        return QVariant();
}

template <typename T>
std::optional<T> optional_from_qvariant(const QVariant &v) {
    if (v.isNull())
        return std::nullopt;
    else
        return v.value<T>();
}
}


template <typename Table>
static inline auto qsql_unpack_record(const QSqlRecord &r) {
    return Table::Columns::template tuple_map<detail::unpack_functor>(r);
}

#define DB_Q_OBJECT \
    static const QMetaObject staticMetaObject; \
    virtual const QMetaObject *metaObject() const; \
    virtual int qt_metacall(QMetaObject::Call, int, void **); \
    virtual void *qt_metacast(const char *); \


template <typename Self>
class Object;

namespace detail {
// From qmetaobject_p.h
enum class PropertyFlags {
   Invalid = 0x00000000,
   Readable = 0x00000001,
   Writable = 0x00000002,
   Resettable = 0x00000004,
   EnumOrFlag = 0x00000008,
   StdCppSet = 0x00000100,
   //     Override = 0x00000200,
   Constant = 0x00000400,
   Final = 0x00000800,
   Designable = 0x00001000,
   ResolveDesignable = 0x00002000,
   Scriptable = 0x00004000,
   ResolveScriptable = 0x00008000,
   Stored = 0x00010000,
   ResolveStored = 0x00020000,
   Editable = 0x00040000,
   ResolveEditable = 0x00080000,
   User = 0x00100000,
   ResolveUser = 0x00200000,
   Notify = 0x00400000,
   Revisioned = 0x00800000
};
constexpr uint operator|(uint a, PropertyFlags b) { return a | uint(b); }

enum class MethodFlags {
    AccessPrivate = 0x00,
    AccessProtected = 0x01,
    AccessPublic = 0x02,
    AccessMask = 0x03,
    MethodMethod = 0x00,
    MethodSignal = 0x04,
    MethodSlot = 0x08,
    MethodConstructor = 0x0c,
    MethodTypeMask = 0x0c,
};
constexpr uint operator|(uint a, MethodFlags b) { return a | uint(b); }

struct string_const {
    const char *value;
    int size;
};

template <typename T>
struct property_type {
    using column_type = T;
    using type = T;

    inline static type get(void *_v) {
        return *reinterpret_cast<type*>(_v);
    }

    inline static void set(void *_v, const column_type &v) {
        *reinterpret_cast<type*>(_v) = v;
    }
};

template <typename V>
struct property_type<std::optional<V>> {
    using column_type = std::optional<V>;
    using type = QVariant;

    inline static std::optional<V> get(void *_v) {
        return detail::optional_from_qvariant<V>(*reinterpret_cast<QVariant*>(_v));
    }

    inline static void set(void *_v, const std::optional<V> &v) {
        *reinterpret_cast<QVariant*>(_v) = detail::qvariant_from_optional(v);
    }
};

template <typename Self>
struct Object_smo {
    static const string_const classname;

    struct strings {
        static constexpr auto consts = std::array<string_const,1> {
                {"save", 4}
        };

        using This = Object_smo<Self>::strings;
        const QByteArrayData smo_classname;
        const QByteArrayData smo_empty;
        const std::array<QByteArrayData, Self::Table::Columns::size> smo_propnames;
        const std::array<QByteArrayData, Self::Table::Columns::size> smo_signames;
        const std::array<QByteArrayData, std::tuple_size_v<decltype (consts)>> smo_consts;

        static constexpr uint propnames_offset = 2;
        static constexpr uint signames_offset  = propnames_offset + std::tuple_size_v<decltype(smo_propnames)>;
        static constexpr uint consts_offset = signames_offset + std::tuple_size_v<decltype(smo_signames)>;

        strings() :
            smo_classname Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(
                classname.size,
                qptrdiff(classname.value - reinterpret_cast<const char*>(&this->smo_classname))
            ),
            smo_empty Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(
                0,
                0 //qptrdiff(classname.value+classname.size - reinterpret_cast<const char *>(&this->smo_empty))
            ),
            smo_propnames (Util::tuple_enumerate_map<std::array<QByteArrayData, Self::Table::Columns::size>>(Self::Table::columns, [this] (size_t i, auto col) {
                return QByteArrayData Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(
                    col.name.size-1,
                    qptrdiff(col.name.value - reinterpret_cast<const char*>(&this->smo_propnames[i]))
                );
            })),
            smo_signames (Util::tuple_enumerate_map<decltype (smo_signames)>(Self::Table::columns, [this](size_t i, auto col){
                auto s = tstring_concat("changed<"_T, col.name, ">"_T);
                return QByteArrayData Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(
                    s.size-1,
                    qptrdiff(s.value - reinterpret_cast<const char*>(&this->smo_signames[i]))
                );
            })),
            smo_consts (Util::tuple_enumerate_map<decltype (smo_consts)>(consts, [this](size_t i, const string_const &str){
                return QByteArrayData Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(
                    str.size,
                    qptrdiff(static_cast<const char *>(str.value) - reinterpret_cast<const char*>(&this->smo_consts[i]))
                );
            }))
        {}

        static constexpr size_t size() { return consts_offset + std::tuple_size_v<decltype(consts)>; }
    } strings;

    struct data {
        using This = Object_smo<Self>::data;

        struct block {
            uint count;
            uint offset;
        };

        struct method {
            uint name;
            uint argc;
            uint params;
            uint tag;
            uint flags;
        };

        template <size_t NArgs>
        struct method_args {
            uint return_type;
            uint arg_types[NArgs];
            uint arg_names[NArgs];
        };

        struct property {
            uint name;
            uint type;
            uint flags;
        };

        // Header
        const struct {
            uint revision = 8; // Qt 5.12
            uint classname = 0;
            block classinfo = {0, 0};
            block methods = {std::tuple_size_v<decltype (data::method_signals)> + 1, offsetof(This, method_signals)/sizeof(uint)};
            block properties = {std::tuple_size_v<decltype (data::properties)>, offsetof(This, properties)/sizeof(uint)};
            block enums_sets = {0, 0};
            block constructors = {0, 0};
            uint flags = 0;
            uint signalCount = Self::Table::Columns::size;
        } header;

        // Methods
        const std::array<method, Self::Table::Columns::size> method_signals =
                Util::tuple_enumerate_map<decltype(method_signals)>(Self::Table::columns, [] (uint i, auto col) {
            return method{
                strings::signames_offset + i,
                0,
                (uint)(offsetof(This, method_args_signals) / sizeof(uint) + i),
                1,
                0u|MethodFlags::AccessPublic|MethodFlags::MethodSignal
            };
        });

        const method method_save {
            strings::consts_offset + 0,
            0,
            (uint)offsetof(This, method_args_save) / sizeof(uint),
            1,
            0u|MethodFlags::AccessPublic|MethodFlags::MethodMethod
        };

        const std::array<method_args<0>, Self::Table::Columns::size> method_args_signals =
                Util::tuple_map<decltype(method_args_signals)>(Self::Table::columns, [] (auto col) {
            return method_args<0>{QMetaType::Void};
        });

        const method_args<0> method_args_save { QMetaType::QVariant };

        // Properties
        const std::array<property, Self::Table::Columns::size> properties =
                Util::tuple_enumerate_map<std::array<property, Self::Table::Columns::size>>(Self::Table::columns, [](uint i, auto col) {
            constexpr uint type = [&col] {
                if constexpr (col.is_nullable)
                    return qMetaTypeId<QVariant>();
                else if constexpr (QtPrivate::QMetaTypeIdHelper<typename decltype(col)::type>::qt_metatype_id() != -1)
                    return qMetaTypeId<typename decltype(col)::type>();
                else
                    return qRegisterMetaType<typename decltype(col)::type>();
            }();
            return property{
                strings::propnames_offset + i,
                // The easiest way to implement nullable columns is to just return a QVariant directly
                qMetaTypeId<typename property_type<typename decltype(col)::type>::type>(),
                0u|PropertyFlags::Readable|PropertyFlags::Writable|PropertyFlags::Scriptable|PropertyFlags::Notify
            };
        });
        const std::array<uint, Self::Table::Columns::size> property_signals =
                Util::tuple_enumerate_map<decltype (property_signals)>(Self::Table::columns, [] (uint i, auto col) {
            return i;
        });

        // End
        const uint eod = 0;
    } data;

    const QByteArrayData *get_strings() const {
        return reinterpret_cast<const QByteArrayData*>(&strings);
    }

    const uint *get_data() const {
        return reinterpret_cast<const uint*>(&data);
    }

    static const Object_smo<Self> instance;
};


}

/**
 * @brief The Object class
 */
template <typename Self>
class Object : public QObject, public Util::ORM::Base<Self>
{
    Database &db;

protected:
    // Constructors
    Object(Database &db) :
        db(db),
        Util::ORM::Base<Self>()
    {}

    Database &database() {
        return db;
    }

    // Create table description
    template<typename Name, typename... Ts>
    static constexpr auto orm_table(Name name, Ts... ts) {
        return Util::ORM::Base<Self>::orm_table(name, id, ts...);
    }

    template <typename Col>
    void changed() {
        void *_a[] = {nullptr, };
        QMetaObject::activate(this, &staticMetaObject, Self::Table::template column_index<Col>(), _a);
    }

    // ----------------------------------------------------
    // QObject implementation
public:
    // static
    static void qt_static_metacall(QObject *_o, QMetaObject::Call _c, int id, void **_a) {
        static constexpr int column_ct = Self::Table::Columns::size;
        auto *o = static_cast<Self*>(_o);
        if (_c == QMetaObject::InvokeMetaMethod) {
            //qDebug() << "QSM/invoke:" << id;
            if (id < column_ct) {
                Util::tuple_dispatch(Self::Table::columns, id, [o] (auto col) {
                    o->template changed<decltype(col)>();
                });
            } else {
                id -= column_ct;
                if (id == 0) {
                    *reinterpret_cast<QVariant*>(_a[0]) = o->saveInvokable();
                }
            }
        } else if (_c == QMetaObject::IndexOfMethod) {
            int *result = reinterpret_cast<int *>(_a[0]);
            {
                using _t = QVariant (Object<Self>::*)();
                auto method = reinterpret_cast<_t*>(_a[1]);
                if (*method == static_cast<_t>(&Object<Self>::saveInvokable)) {
                    *result = column_ct + 0;
                }
            }
            {
                using _t = void (Object<Self>::*)();
                auto method = reinterpret_cast<_t*>(_a[1]);
                Util::tuple_enumerate_foreach(Self::Table::columns, [result, method] (int i, auto col) {
                    if (*method == static_cast<_t>(&Object<Self>::changed<std::decay_t<decltype(col)>>))
                        *result = i;
                });
            }
        } else if (_c == QMetaObject::ReadProperty) {
            //qDebug() << "QSM/read:" << id;
            void *_v = _a[0];
            Util::tuple_dispatch(Self::Table::columns, id, [o, _v] (auto col) {
                detail::property_type<typename decltype(col)::type>::set(_v, o->get(col));
            });
        } else if (_c == QMetaObject::WriteProperty) {
            void *_v = _a[0];
            Util::tuple_dispatch(Self::Table::columns, id, [o, _v] (auto col) {
                o->set(col, detail::property_type<typename decltype(col)::type>::get(_v));
            });
        } //else if (_c == QMetaObject::RegisterPropertyMetaType) {
          //  Util::tuple_dispatch(Self::Table::columns, id, [o, _v] (auto col) {
          //  });
        //}
    }

    static const QMetaObject staticMetaObject;

    virtual int qt_metacall(QMetaObject::Call call, int id, void **args) override {
        id = QObject::qt_metacall(call, id, args);
        if (id < 0)
            return id;

        static constexpr int column_ct = Self::Table::Columns::size;
        static constexpr int method_ct = column_ct + 1;

        if (call == QMetaObject::InvokeMetaMethod) {
            if (id < method_ct)
                qt_static_metacall(this, call, id, args);
            id -= method_ct;
        } else if (call == QMetaObject::RegisterMethodArgumentMetaType) {
            if (id < method_ct)
                *reinterpret_cast<int*>(args[0]) = -1;
            id -= method_ct;
        } else if (call == QMetaObject::ReadProperty || call == QMetaObject::WriteProperty
                    || call == QMetaObject::ResetProperty || call == QMetaObject::RegisterPropertyMetaType) {
            qt_static_metacall(this, call, id, args);
            id -= column_ct;
        }

        return id;
    }

    const QMetaObject *metaObject() const override
    {
        return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
    }

    template <typename Col>
    void (Object<Self>::*changedSignal())()  {
        return &Object<Self>::template changed<Col>;
    }

    template <typename Col>
    void (Object<Self>::*changedSignal(Col))()  {
        return &Object<Self>::template changed<std::decay_t<Col>>;
    }

    static Util::ORM::SQL<long> sqlSelectId(long id) {
        static QString query = QStringLiteral("SELECT %0 FROM %1 WHERE id = ?;")
                .arg(Self::table.getColumnNames().join(", "))
                .arg(Self::table.name.value);
        return {query, {id}};
    }

public:
    ORM_COLUMN(long, id, Util::ORM::PrimaryKey<>);

    /**
     * @return Return the row ID or -1 if the object hasn't been saved yet
     */
    inline long getId() const {
        return Util::ORM::Base<Self>::get(id).value_or(-1);
    }

    // Load from DB
    static inline DBResult<std::unique_ptr<Self>> load(Database &db, long id) {
        return db.exec(sqlSelectId(id)).map([&db](QSqlQuery &&q) {
            q.next();
            return std::make_unique<Self>(db, q.record());
        });
    }

    static inline DBResult<std::vector<std::unique_ptr<Self>>> list(Database &db) {
        return db.exec(Self::table.sqlSelectAll()).map([&db](QSqlQuery &&q) {
            std::vector<std::unique_ptr<Self>> r;;
            if (!q.next())
                return r;
            // don't use q.size() w/ sqlite!
            //r.reserve(q.size());
            do
                r.emplace_back(std::make_unique<Self>(db, q.record()));
            while (q.next());
            return r;
        });
    }

    template <typename Sel, typename... Opts>
    static inline DBResult<std::vector<std::unique_ptr<Self>>> select(Database &db, const Sel &sel, Opts... opts) {
        return db.exec(Self::table.sqlSelect(sel, opts...)).map([&db](QSqlQuery q) {
            std::vector<std::unique_ptr<Self>> r;;
            if (!q.next())
                return r;
            // don't use q.size() w/ sqlite!
            //r.reserve(q.size());
            do
                r.emplace_back(std::make_unique<Self>(db, q.record()));
            while (q.next());
            return r;
        });
    }

    template <typename... Opts>
    static inline DBResult<std::unique_ptr<Self>> selectOne(Database &db, Opts... opts) {
        return db.exec(Self::table.sqlSelect(opts..., Util::ORM::Sel::Limit(1))).map([&db](QSqlQuery q) {
            if (!q.next())
                return std::unique_ptr<Self>{};
            return std::make_unique<Self>(db, q.record());
        });
    }

    // Save to DB
    DBResult<long> save() {
        static QStringList columns = Self::Table::getColumnNames();
        static QStringList placeholders = [] {
            QStringList r;
            r.reserve(columns.size());
            for (auto col : columns)
                r.append(QStringLiteral(":%0").arg(col));
            return r;
        }();

        // Pure Insert
        static QString sql_insert = QStringLiteral("INSERT INTO %0 (%1) VALUES (%2)")
                .arg(Self::Table::name.value)
                .arg(columns.join(", "))
                .arg(placeholders.join(", "));

        QString query = sql_insert;

        // Upsert if exists
        long id = getId();
#if 1
        if (id > 0) { // invalid ID can't exist
            static QString sql_upsert_tmpl = QStringLiteral(" ON CONFLICT (id) DO UPDATE SET (%0) = (%1)");

            QStringList update_cols;
            QStringList update_phs;
            const auto &dirty = static_cast<Self*>(this)->row.get_dirty();

            for (int i = 0; i < columns.size(); i++)
                if (dirty[i]) {
                    assert(i != 0); // ID column should never have to be updated
                    update_cols.append(columns[i]);
                    update_phs.append(placeholders[i]);
                }

            query.append(sql_upsert_tmpl.arg(update_cols.join(", ")).arg(update_phs.join(", ")));
        }
#endif

        // Prepare
        qDebug() << "Save/SQL:" << query;
        auto q = database().prepare(query);

        Util::tuple_enumerate_foreach(Self::Table::columns, [&q, this, id] (size_t i, auto col) {
            QVariant v;
            if constexpr (col.is_nullable) {
                    using Opt = typename decltype(col)::nullable_type;
                    auto opt = static_cast<Self*>(this)->row.get(col);
                    if (!Opt::is_unit(opt))
                        v = QVariant::fromValue(Opt::get(opt));
            } else
                v = QVariant::fromValue(static_cast<Self*>(this)->row.get(col));
            //qDebug() << "Save/bind:" << placeholders[i] << v;
            q.bindValue(placeholders[i], v);
        });

        qDebug() << "Save/bound:" << q.boundValues();

        // Execute
        if (!q.exec())
            return Err(q.lastError());

        if (id < 0) {
            id = q.lastInsertId().toInt();
            this->set(this->id, id);
        }

        static_cast<Self*>(this)->row.reset_dirty();

        return Ok(id);
    }

    // Exported to QML as just save()
    Q_INVOKABLE QVariant saveInvokable() {
        auto r = save();
        if (r)
            return QVariant::fromValue(r.value());
        else
            return QVariant();
    }
};

#define DB_OBJECT(Self, name, ...) \
    private: \
        friend class ::Midoku::Library::Object<Self>; \
    public: \
        ORM_OBJECT(Self, Object<Self>::orm_table, name, __VA_ARGS__)

#define DB_OBJECT_POST(Self) \
    /* Explicit instantiations to quiet down -Wundefined-var-template */ \
    template<> const Midoku::Library::detail::string_const Midoku::Library::detail::Object_smo<Self>::classname; \
    template<> const Midoku::Library::detail::Object_smo<Self> Midoku::Library::detail::Object_smo<Self>::instance; \
    template<> const QMetaObject Midoku::Library::Object<Self>::staticMetaObject;


#define DB_OBJECT_IMPL(Self) \
    template<> const Midoku::Library::detail::string_const Midoku::Library::detail::Object_smo<Self>::classname = {"Midoku::Library::Object::template::" #Self, sizeof("Midoku::Library::Object::template::" #Self)-1}; \
    template<> const Midoku::Library::detail::Object_smo<Self> Midoku::Library::detail::Object_smo<Self>::instance = {}; \
    template<> const QMetaObject Midoku::Library::Object<Self>::staticMetaObject = {{ \
                     QMetaObject::SuperData::link<QObject::staticMetaObject>(), \
                     /*&QObject::staticMetaObject,*/ \
                     Midoku::Library::detail::Object_smo<Self>::instance.get_strings(), \
                     Midoku::Library::detail::Object_smo<Self>::instance.get_data(), \
                     Midoku::Library::Object<Self>::qt_static_metacall, \
                     nullptr, \
                     nullptr \
                }};

}

