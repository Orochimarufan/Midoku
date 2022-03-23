#pragma once

#include "type_sequence.h"
#include "tstring.h"
#include "tuple_util.h"

#include <utility>

#include <QString>
#include <QList>
#include <QDebug>


namespace Midoku::Util::ORM {

// -----------------------------------------------------------------------
// Plumbing
namespace detail {
// tags
struct column {};

struct property {};

struct constraint {};

struct references_tag {};

struct sql_query_type {};

template <typename T>
using is_sql_query = std::is_base_of<sql_query_type, T>;

template <typename T>
using is_sql_where_clause = is_sql_query<decltype(std::declval<T>().sqlWhereClause())>;

template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

template <typename T>
using is_tuple = is_specialization<T, std::tuple>;

// Misc
template <typename T>
struct map_type_type {
    using type = typename T::type;
};

}


// Traits
template <typename T>
using is_column = std::is_base_of<detail::column, T>;

template <typename T>
using is_property = std::is_base_of<detail::property, T>;

template <typename T>
using is_constraint = std::is_base_of<detail::constraint, T>;

template <typename T>
using is_references_clause = std::is_base_of<detail::references_tag, T>;

template <typename T>
concept BoundSQL = std::is_base_of<detail::sql_query_type, T>::value;

template <typename T>
concept SQLQuery = requires(T const a) {{a.sqlQuery()} -> BoundSQL;};


/**
 * @brief Type trait describing database properties
 * Types must be QVariant conversible
 */
template<typename T>
struct schema_type {
    // static constexpr const char *value;
};

/**
 * @brief Type trait describing a nullable variant of t
 */
template <typename T>
struct nullable_type {
    using type = std::optional<T>;

    inline static type unit() {
        return std::nullopt;
    };

    template <typename U>
    inline static type value(U &&v) {
        return std::optional<T>(std::forward<U>(v));
    }

    inline static bool is_unit(const type &o) {
        return !o;
    }

    inline static T get(const type &o) {
        return o.value();
    }
};

// ---------------------
// Specializations
// ---------------
// Long
template<> struct schema_type<long> {
    static constexpr const char *value = "INTEGER";
};

// QString
template<> struct schema_type<QString> {
    static constexpr const char *value = "TEXT";
};

template<> struct nullable_type<QString> {
    using type = QString;

    inline static type unit() {
        return QString();
    }

    template <typename U>
    inline static type value(U &&v) {
        return QString(std::forward<U>(v));
    }

    inline static bool is_unit(const type &o) {
        return o.isNull();
    }

    inline static QString get(const type &o) {
        return o;
    }
};

// QByteArray
template<> struct schema_type<QByteArray> {
    static constexpr const char *value = "BLOB";
};

// -----------------------------------------------------------------------
// Parameterized SQL Queries
template <typename... Binds>
struct SQL : detail::sql_query_type {
    QString query;
    std::tuple<Binds...> binds;

    SQL(SQL<Binds...> &&) = default;
    SQL(const SQL<Binds...> &) = default;

    SQL(const QString &q) :
        query(q),
        binds()
    {}

    SQL(const QString &q, const std::tuple<Binds...> &b) :
        query(q),
        binds(b)
    {}

    SQL(const QString &q, std::tuple<Binds...> &&b) :
        query(q),
        binds(std::move(b))
    {}

    // Append
    template <typename... Bs>
    inline SQL<Binds..., Bs...> append(const SQL<Bs...> &o) {
        return {query + o.query, std::tuple_cat(binds, o.binds)};
    }

    template <typename... Bs>
    inline SQL<Binds..., Bs...> append(SQL<Bs...> &&o) {
        return {query + o.query, std::tuple_cat(binds, std::move(o.binds))};
    }

    // Prepend
    template <typename... Bs>
    inline SQL<Binds..., Bs...> prepend(const SQL<Bs...> &o) {
        return {o.query + query, std::tuple_cat(o.binds, binds)};
    }

    template <typename... Bs>
    inline SQL<Binds..., Bs...> prepend(SQL<Bs...> &&o) {
        return {o.query + query, std::tuple_cat(std::move(o.binds), binds)};
    }

    // Use as clause
    inline auto sqlClause() const {
        return *this;
    }

    inline auto sqlWhereClause() const {
        return *this;
    }

    inline auto sqlExpression(int p=0) const {
        if (p > 0)
            return sql_parenthesize(*this);
        else
            return *this;
    }
};

template <typename... Ts> SQL(QString, const std::tuple<Ts...> &) -> SQL<Ts...>;
template <typename... Ts> SQL(QString, std::tuple<Ts...> &&) -> SQL<Ts...>;

template <typename... Us>
inline SQL<std::decay_t<Us>...> sql(const QString &q, Us&&... binds) {
    return SQL<std::decay_t<Us>...>{q, {std::forward<Us>(binds)...}};
}


// Join parts
template <typename C, typename... Ts> requires (BoundSQL<std::decay_t<Ts>>&&...)
auto sql_join(const C &sep, Ts&&... parts) -> decltype(SQL("", std::tuple_cat(parts.binds...)))
{
    QStringList s{parts.query...};
    s.removeAll(QStringLiteral());
    return SQL{
        s.join(sep),
        std::tuple_cat(parts.binds...)
    };
}

template <typename C, typename T>
auto sql_join(const C &sep, T &&sql) -> T
{
    return std::forward<T>(sql);
}

template <typename C>
auto sql_join(const C &sep) -> SQL<>
{
    static const SQL<> empty(QStringLiteral());
    return empty;
}

template <typename C, typename T>
auto sql_join2(const C &sep, T &&tup)
{
    return std::apply([&sep] (auto &&... os) {return sql_join(sep, os...);}, std::forward<T>(tup));
}

template <typename C, typename... Tups>
auto sql_join2(const C &sep, Tups&&... tups)
{
    return std::apply([&sep] (auto &&... os) {return sql_join(sep, os...);}, std::tuple_cat(std::forward<Tups>(tups)...));
}

template <typename... Ts>
auto sql_parenthesize(const SQL<Ts...> &sql) -> SQL<Ts...> {
    static QString fmt = QStringLiteral("(%1)");
    return SQL{fmt.arg(sql.query), sql.binds };
}


// -----------------------------------------------------------------------
// Expressions

namespace Expr {
template <typename T>
concept Expression = requires(T const a) {
    {a.sqlExpression()} -> BoundSQL;
    {a.sqlExpression(10)} -> BoundSQL;
};

template <typename> struct Value;
template <Expression> struct Not;
template <Expression, Expression> struct And;
template <Expression, Expression> struct Or;
template <Expression, Expression> struct Eq;
template <Expression, Expression> struct Less;
template <Expression, Expression> struct Greater;
template <typename, Expression...> struct Fn;

template <typename T>
auto toExpr(const T &x) {
    if constexpr (Expression<T>)
        return x;
    else
        return Value<T>(x);
}

namespace detail {
template <typename Self, typename T>
struct ExpressionBase {
    using type = T;

protected:
    Self &self() {
        return *static_cast<Self*>(this);
    }

    Self const &self() const {
        return *static_cast<Self const *>(this);
    }

    template <template <Expression, Expression> typename BinExpr, typename U>
    auto make_binary(const U &o) const {
        if constexpr (Expression<U>)
            return BinExpr<Self, U>(self(), o);
        else
            return BinExpr<Self, Value<U>>(self(), Value<U>(o));
    }

public:
    template <typename U>
    auto operator == (const U &o) const {
        return make_binary<Eq>(o);
    }

    template <typename U>
    auto operator < (const U &o) const {
        return make_binary<Less>(o);
    }

    template <typename U>
    auto operator > (const U &o) const {
        return make_binary<Greater>(o);
    }
};

template <typename Self>
struct ExpressionBase<Self, bool> {
    using type = bool;

protected:
    Self &self() {
        return *static_cast<Self*>(this);
    }

    Self const &self() const {
        return *static_cast<Self const *>(this);
    }

public:
    auto sqlExpression(int) const {
        return self().sql();
    }

    template <Expression U>
    auto operator &&(U &&u) const & {
        return And<Self, U>(self(), std::forward<U>(u));
    }

    template <Expression U>
    auto operator &&(U &&u) && {
        return And<Self, U>(std::move(self()), std::forward<U>(u));
    }

    template <Expression U>
    auto operator ||(U &&u) const & {
        return Or<Self, U>(self(), std::forward<U>(u));
    }

    template <Expression U>
    auto operator ||(U &&u) && {
        return Or<Self, U>(std::move(self()), std::forward<U>(u));
    }

    template <Expression U>
    auto operator !() const & {
        return Not<U>(self());
    }

    template <Expression U>
    auto operator !() && {
        return Not<U>(std::move(self()));
    }
};

template <typename Self, typename T, int Prec, typename Lhs, typename Rhs>
struct BinaryExpression : ExpressionBase<Self, T> {
    static constexpr int prec = Prec;

    Lhs lhs;
    Rhs rhs;

    BinaryExpression(const Lhs &lhs, const Rhs &rhs) :
        lhs(lhs), rhs(rhs)
    {}

    auto sqlExpression(int into_prec=100) const {
        QString fmt;
        if (into_prec < prec)
            fmt = QStringLiteral("(%1 %0 %2)");
        else
            fmt = QStringLiteral("%1 %0 %2");
        fmt = fmt.arg(this->self().binary_op);

        auto left = lhs.sqlExpression(prec);
        auto right = rhs.sqlExpression(prec);

        return SQL{fmt.arg(left.query).arg(right.query),
                   std::tuple_cat(left.binds, right.binds)};
    }
};

}

/// @brief A literal value
/// @note this is always implemented using a sql bind
template <typename T>
struct Value : detail::ExpressionBase<Value<T>, T> {
    T value;

    Value(const T &val) :
        value(val)
    {}

    Value(T &&val) :
        value(std::move(val))
    {}

    SQL<T> sqlExpression(int=0) const {
        return ORM::sql("?", value);
    }
};

/// @brief Boolean negation
template <Expression Expr>
struct Not : detail::ExpressionBase<Not<Expr>, bool> {
    static_assert(std::is_same_v<typename Expr::type, bool>, "Tried to build SQL Not expression from non-bool expression.");

    Expr expr;

    Not(const Not<Expr> &c) :
        expr(c.expr)
    {}

    explicit Not(const Expr &expr, int=0) :
        expr(expr)
    {}

    //Not(Expr &&expr) :
    //    expr(std::move(expr))
    //{}

    auto sqlExpression(int=0) const -> decltype(expr.sqlExpression(0)) {
        auto inner = expr.sqlExpression(0);
        return SQL{QStringLiteral("NOT " + inner.query), inner.binds};
    }
};

/// @brief Boolean conjunction
template <Expression Lhs, Expression Rhs>
struct And : detail::BinaryExpression<And<Lhs, Rhs>, bool, 70, Lhs, Rhs> {
    using detail::BinaryExpression<And<Lhs, Rhs>, bool, 70, Lhs, Rhs>::BinaryExpression;
    static constexpr const char *binary_op = "AND";
};

template <Expression A, Expression B> And(A, B) -> And<A, B>;

/// @brief Boolean disjunction
template <Expression Lhs, Expression Rhs>
struct Or : detail::BinaryExpression<Or<Lhs, Rhs>, bool, 80, Lhs, Rhs> {
    using detail::BinaryExpression<Or<Lhs,Rhs>,bool,80,Lhs,Rhs>::BinaryExpression;
    static constexpr const char *binary_op = "OR";
};

template <Expression A, Expression B> Or(A, B) -> Or<A, B>;

/// @brief Equality
template <Expression Lhs, Expression Rhs>
struct Eq : detail::BinaryExpression<Eq<Lhs, Rhs>, bool, 60, Lhs, Rhs> {
    using detail::BinaryExpression<Eq<Lhs,Rhs>,bool,60,Lhs,Rhs>::BinaryExpression;
    static constexpr const char *binary_op = "=";
};

template <Expression A, Expression B> Eq(A, B) -> Eq<A, B>;

/// @brief Less than
template <Expression Lhs, Expression Rhs>
struct Less : detail::BinaryExpression<Less<Lhs, Rhs>, bool, 50, Lhs, Rhs> {
    using detail::BinaryExpression<Less<Lhs,Rhs>,bool,50,Lhs,Rhs>::BinaryExpression;
    static constexpr const char *binary_op = "<";
};

/// @brief Greater than
template <Expression Lhs, Expression Rhs>
struct Greater : detail::BinaryExpression<Greater<Lhs, Rhs>, bool, 50, Lhs, Rhs> {
    using detail::BinaryExpression<Greater<Lhs,Rhs>,bool,50,Lhs,Rhs>::BinaryExpression;
    static constexpr const char *binary_op = ">";
};

/// @brief SQL functions
template <typename T, Expression... Args>
struct Fn : detail::ExpressionBase<Fn<T, Args...>, T> {
    QString name;
    std::tuple<Args...> arguments;

    Fn(QString name, Args... args) :
        name(name), arguments(args...)
    {}

    template <typename... Us>
    Fn(QString name, const Us &... args) :
        name(name), arguments(toExpr(args)...)
    {}

    auto sqlExpression(int=0) const {
        auto templ = QStringLiteral("%0(%1)").arg(name);
        auto args = sql_join2(", ", Util::tuple_map(arguments, [](auto a){return a.sqlExpression();}));
        return SQL{templ.arg(args.query), args.binds};
    }
};

/// @brief SQLite datetime() function
template <Expression Expr>
Fn<void, Expr> datetime(Expr e) {
    return {"datetime", e};
}

template <Expression Expr>
Fn<long, Expr> sum(Expr e) {
    return {"sum", e};
}

template <Expression Expr>
Fn<long, Expr> min(Expr e) {
    return {"min", e};
}

inline
Fn<long> count() {
    return {"count"};
}

// Ensure concepts
struct Stub {
    using type = int;

    static SQL<> sqlExpression(int=1) {
        return sql("1");
    }
};

static_assert(Expression<Value<long>>);
static_assert(Expression<Not<Value<bool>>>);
static_assert(Expression<Eq<Stub, Stub>>);
static_assert(Expression<And<Stub, Stub>>);
static_assert(Expression<Fn<long>>);
}

namespace Sel {

template <typename T>
concept Constraint = requires (T const a) {
    {a.sqlSelectConstraint()} -> BoundSQL;
};

template <typename T>
concept SourceTable = requires (T const a) {
    {a.getColumnNames()} -> std::convertible_to<QStringList>;
    {a.getTableName()} -> std::convertible_to<QString>;
};

template <typename T>
concept SourceExpression = requires (T const a) {
    {a.sqlSelectColumns()} -> BoundSQL; // TODO: change this to be a tuple of expressions instead to preserve type info
    {a.getTableName()} -> std::convertible_to<QString>;
};

template <typename T>
concept Source = SourceTable<T> || SourceExpression<T>;

template <Expr::Expression Expr>
struct Where {
    Expr expr;

    Where(const Expr &expr) :
        expr(expr)
    {}

    auto sqlSelectConstraint() const {
        auto sql = expr.sqlExpression();
        return SQL{"WHERE " + sql.query, sql.binds};
    }
};

template <Source From, Constraint... Constraints>
struct Select {
    From source;
    std::tuple<Constraints...> constraints;

    Select(From source, Constraints... constraints) :
        source(source), constraints(constraints...)
    {}

private:
    auto make_sql(QString templ) const {
        auto cols = [this]() {
            if constexpr (SourceTable<From>)
                return sql(source.getColumnNames().join(", "));
            else
                return source.sqlSelectColumns();
        }();
        auto cs = sql_join2(" ", Util::tuple_map(constraints, [](auto c) {return c.sqlSelectConstraint();}));
        return SQL{templ.arg(cols.query).arg(source.getTableName()).arg(cs.query), std::tuple_cat(cols.binds, cs.binds)};
    }

public:
    auto sqlQuery() const {
        return make_sql(QStringLiteral("SELECT %0 FROM %1 %2;"));
    }

    auto sqlExpression(int=0) const {
        return make_sql(QStringLiteral("(SELECT %0 FROM %1 %2)"));
    }
};

template <Source F, Constraint... Cs>
Select(F, Cs...) -> Select<F, Cs...>;

struct Limit {
    size_t limit;

    Limit(size_t limit) :
        limit(limit)
    {}

    SQL<size_t> sqlSelectConstraint() const {
        return SQL{QStringLiteral("LIMIT ?"), std::tuple{limit}};
    }
};

template <Expr::Expression Expr>
struct OrderBy {
    Expr expr;
    bool descending = false;

    OrderBy(const Expr &expr, bool desc=false) :
        expr(expr), descending(desc)
    {}

    SQL<> sqlSelectConstraint() const {
        auto sql = expr.sqlExpression(0);
        return SQL{QStringLiteral("ORDER BY %0%1")
                    .arg(sql.query)
                    .arg(descending ? " DESC" : ""),
                sql.binds};
    }
};

template <Expr::Expression E>
OrderBy(const E &) -> OrderBy<E>;

template <Expr::Expression E>
OrderBy(const E &, bool) -> OrderBy<E>;


template<typename Tbl, typename... Cols>
struct Columns {
    using Table = Tbl;
    using Columns_ = type_sequence<Cols...>;
    static constexpr auto columns = typename Columns_::tuple();

    static_assert (Columns_::template all<is_column>(), "Invalid column argument");

    constexpr Columns() {}
    constexpr Columns(Tbl, Cols...) {}

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }

    static QString getTableName() {
        return Tbl::getTableName();
    }
};

template <typename Tbl, Expr::Expression... Cols>
struct From {
    Tbl const &table;
    std::tuple<Cols...> columns;

    From(const Tbl &table, const Cols &... columns) :
        table(table), columns(columns...)
    {}

    QString getTableName() const {
        return table.getTableName();
    }

    auto sqlSelectColumns() const {
        return sql_join2(", ", Util::tuple_map(columns, [](auto c){return c.sqlExpression();}));
    }
};


// Invariants
struct Stub {
    static QString getTableName() {
        return "abcdef";
    }

    static SQL<> sqlExpression(int=0) {
        return sql("fail");
    }

    static SQL<> sqlSelectColumns() {
        return sql("a, b, c");
    }

    static SQL<> sqlSelectConstraint() {
        return sql("");
    }
};

static_assert(Source<From<Stub, Stub>>);
static_assert(Constraint<Where<Stub>>);
static_assert(Constraint<OrderBy<Stub>>);
static_assert(Constraint<Limit>);
static_assert(SQLQuery<Select<Stub>>);
static_assert(Expr::Expression<Select<Stub>>);
}


template <Sel::Source From, Expr::Expression WExpr, Sel::Constraint... Cs>
auto select(From from, WExpr where, Cs... constraints) -> Sel::Select<From, Sel::Where<WExpr>, Cs...> {
    return {from, {where}, constraints...};
}

template <Sel::Source From, Sel::Constraint... Cs>
auto select(From from, Cs... constraints) -> Sel::Select<From, Cs...> {
    return {from, constraints...};
}


// -----------------------------------------------------------------------
// Layout classes

template <typename T>
concept ColumnConstraint = requires() {
    {T::sqlCreateTableColumnProperty()} -> BoundSQL;
};

// Column Properties
struct NotNull : public detail::property {
    static SQL<> sqlCreateTableColumnProperty() {
        return QStringLiteral("NOT NULL");
    }
};

template <typename... Cols>
struct PrimaryKey;

template <>
struct PrimaryKey<> : public detail::property {
    static SQL<> sqlCreateTableColumnProperty() {
        return QStringLiteral("PRIMARY KEY");
    }
};

template <typename... Cols>
struct Unique;

template <>
struct Unique<> : public detail::property {
    static SQL<> sqlCreateTableColumnProperty() {
        return QStringLiteral("UNIQUE");
    }
};


/**
 * @brief Describes a table column
 * @tparam NameTStr [typename Util::tstring<char,...>] Column name
 * @tparam T        [typename] C++ type
 * @tparam Props... [typename] Other propertiy tags
 */
template<typename NameTStr, typename T, ColumnConstraint... Props>
struct Column : Expr::detail::ExpressionBase<Column<NameTStr, T, Props...>, T>, detail::column {
    using Properties = Util::type_sequence<Props...>;

    static constexpr auto name = NameTStr();
    static constexpr auto properties = typename Properties::tuple();
    static constexpr bool is_nullable = !Properties::template contains<NotNull>;

    using value_type = T;
    using nullable_type = nullable_type<T>;
    using type = std::conditional_t<is_nullable, typename nullable_type::type, T>;

    static_assert(Properties::template all<is_property>(), "Invalid properties");

    constexpr Column(NameTStr, T, Props...) {}
    constexpr Column() {}

    static QString getName() {
        return name.value;
    }

    static auto sqlCreateTableColumn() {
        auto s = sql_join(' ', Props::sqlCreateTableColumnProperty()...);
        QStringList parts {name.value, schema_type<T>::value, s.query};
        parts.removeAll(QStringLiteral());
        return SQL{parts.join(' '), s.binds};
    }

    // Expression
    SQL<> sqlExpression(int=0) const {
        return ORM::sql(getName());
    }
};


/**
 * @brief Describes a database table
 * @tparam NameTStr [typename Util::tstring<char, ...>] Table name
 * @tparam Cols...  [typename] Columns
 * @tparam Conss... [typename] Constraints
 */
template<typename NameTStr, typename... ArgsPack>
struct Table {
    using Args = Util::type_sequence<ArgsPack...>;
    using Columns = typename Args::template filter<is_column>;
    using ColumnTypes = typename Columns::template type_map<detail::map_type_type>;
    using Constraints = typename Args::template filter<is_constraint>;

    static constexpr auto name = NameTStr();
    static constexpr auto columns = typename Columns::tuple();
    static constexpr auto constraints = typename Constraints::tuple();

    static_assert(Columns::size > 0, "At least one column required");
    static_assert(Args::size == Columns::size + Constraints::size, "Invalid arguments");

    constexpr Table() {}
    constexpr Table(NameTStr, Args...) {}

    template <typename Col>
    static constexpr size_t column_index() {
        return Columns::template index<Col>;
    }

    template <typename Col>
    static constexpr size_t column_index(Col) {
        return Columns::template index<Col>;
    }

    static QString getName() {
        return name.value;
    }
    static QString getTableName() { // Select::Source
        return name.value;
    }

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }

    static auto sqlCreateTable(bool if_not_exists = false) {
        auto q = sql_join2(", ",
                Util::tuple_map(columns, [] (auto col) {
                    return col.sqlCreateTableColumn();
                }),
                Util::tuple_map(constraints, [] (auto ct) {
                    return ct.sqlCreateTableConstraint();
                }));
        return SQL{
            QStringLiteral("CREATE TABLE%3 %0 (%1);")
                    .arg(getName()).arg(q.query)
                    .arg(if_not_exists? " IF NOT EXISTS" : ""),
            std::move(q.binds)
        };
    }

    // Old Select
    template <Expr::Expression Where, Sel::Constraint... Optionals>
    static auto sqlSelect(Where w, Optionals... opts) {
        static SQL<> templ = QStringLiteral("SELECT %0 FROM %1 WHERE")
                .arg(getColumnNames().join(", "))
                .arg(name.value);
        return sql_join(" ", templ, w.sqlExpression(), opts.sqlSelectConstraint()..., SQL<>{";"});
    }

    template <Sel::Constraint... Opts>
    auto sqlSelect(Opts... opts) const {
        auto s = Sel::Select{*this, opts...};
        return s.sqlQuery();
    }

    static Util::ORM::SQL<> sqlSelectAll() {
        static Util::ORM::SQL<> q {
            QStringLiteral("SELECT %0 FROM %1;")
                    .arg(getColumnNames().join(", "))
                    .arg(name.value),
            {}
        };
        return q;
    }
};


// Constraints
template<typename... Cols>
struct PrimaryKey : public detail::constraint {
    using Columns = type_sequence<Cols...>;
    static constexpr auto columns = Columns::tuple();

    static_assert (Columns::template all<is_column>(), "Invalid column argument");

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }

    static SQL<> sqlCreateTableConstraint() {
        return QStringLiteral("PRIMARY KEY (%0)").arg(getColumnNames().join(", "));
    }
};

template<typename Tbl, typename... Cols>
struct References : public detail::references_tag {
    using Table = Tbl;
    using Columns = type_sequence<Cols...>;
    static constexpr auto columns = typename Columns::tuple();

    static_assert (Columns::template all<is_column>(), "Invalid column argument");

    constexpr References() {}
    constexpr References(Tbl, Cols...) {}

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }
};

template<typename... Args>
struct ForeignKey : public detail::constraint {
    using args = Util::type_sequence<Args...>;
    using Columns = typename args::template filter<is_column>;
    using References = typename args::last;

    static constexpr auto columns = typename Columns::tuple();
    static constexpr auto references = References();

    static_assert(Columns::template all<is_column>(), "Invalid column argument");
    static_assert(is_references_clause<References>::value, "Last argument to ForeignKey must be References");
    static_assert(Columns::size > 0, "At least one column must be specified");
    static_assert(Columns::size == References::Columns::size, "Number of columns in foreign key clause and references clause must match");

    constexpr ForeignKey() {}
    constexpr ForeignKey(Args...) {}

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }

    static SQL<> sqlCreateTableConstraint() {
        return QStringLiteral("FOREIGN KEY (%0) REFERENCES %1 (%2)")
                .arg(getColumnNames().join(", "))
                .arg(References::Table::getName())
                .arg(References::getColumnNames().join(", "));
    }
};

template <typename... Cols>
struct Unique : public detail::constraint {
    using Columns = type_sequence<Cols...>;
    static constexpr auto columns = typename Columns::tuple();

    static_assert (Columns::template all<is_column>(), "Invalid column argument");

    constexpr Unique() {}
    constexpr Unique(Cols...) {}

    static QStringList getColumnNames() {
        return Util::tuple_map<QStringList>(columns, [] (auto c) {
            return c.getName();
        });
    }

    static SQL<> sqlCreateTableConstraint() {
        return QStringLiteral("UNIQUE (%0)").arg(getColumnNames().join(", "));
    }
};


// -----------------------------------------------------------------------
// ORM Base class

template <typename Table>
class RowStorage
{
    using stor_type = typename Table::Columns::template type_map<detail::map_type_type>::tuple;
    using flag_type = typename std::array<bool, Table::Columns::size>;

    stor_type data;
    flag_type dirty;

public:
    RowStorage(const stor_type &tup, bool is_new=false) :
        data(tup)
    {
        dirty.fill(is_new);
    }

    RowStorage(stor_type &&tup, bool is_new=false) :
        data(tup)
    {
        dirty.fill(is_new);
    }

    RowStorage()
    {
        dirty.fill(true);
    }

    // Dirty
    inline const flag_type &get_dirty() {
        return dirty;
    }

    inline void reset_dirty() {
        dirty.fill(false);
    }

    // Get stuff
    template <typename Col>
    inline auto get() const
            -> std::add_lvalue_reference_t<std::add_const_t<typename Col::type>>
    {
        return std::get<Table::template column_index<Col>()>(data);
    }

    template <typename Col>
    inline auto get(Col) const {
        return get<std::decay_t<Col>>();
    }

    // Set stuff
    template <typename Col>
    inline void set(const typename Col::type &v) {
        constexpr size_t i = Table::template column_index<Col>();
        std::get<i>(dirty) = true;
        std::get<i>(data) = v;
    }

    template <typename Col>
    inline void set(Col, const typename Col::type &v) {
        set<std::decay_t<Col>>(v);
    }

    // Retrieve
    inline stor_type get() {
        return data;
    }
};


template <typename Self>
class Base {
protected:
//public:
    template <typename... Args>
    static constexpr auto orm_table(Args...) {
        return ORM::Table<std::decay_t<Args>...>();
    }

    template<typename Col>
    inline void changed() {
    }

public:
    // Get stuff
    template <typename Col>
    inline auto get() const {
        return static_cast<const Self*>(this)->row.template get<Col>();
    }

    template <typename Col>
    inline auto get(Col) const {
        return get<std::decay_t<Col>>();
    }

    // Set stuff
    template <typename Col>
    inline void set(const typename Col::type &v) {
        static_cast<Self*>(this)->row.template set<Col>(v);
        static_cast<Self*>(this)->template changed<Col>();
    }

    template <typename Col>
    inline void set(Col, const typename Col::type &v) {
        set<std::decay_t<Col>>(v);
    }
};


#define ORM_COLUMN(type, name, ...) \
    static constexpr ::Midoku::Util::ORM::Column<TSTR(#name), type, ##__VA_ARGS__> name = {}

#define ORM_OBJECT(self, make_table, name, ...) \
    static constexpr auto table = make_table(name ##_T, __VA_ARGS__); \
    using Table = typename std::decay_t<decltype(make_table(name ##_T, __VA_ARGS__))>; \
private: \
    ::Midoku::Util::ORM::RowStorage<Table> row; \
    friend class ::Midoku::Util::ORM::Base<self>; \
public:


}
