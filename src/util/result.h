#pragma once

#include <optional>
#include <utility>
#include <variant>
#include <functional>


namespace Midoku::Util {

template <typename T, typename E>
class Result;

namespace detail {
template <typename T>
struct Ok {
    Ok(T && v) : v(std::forward<T>(v)) {}
    T v;
};

template <>
struct Ok<void>
{
};

template <typename E>
struct Err {
    Err(E && e) : e(std::forward<E>(e)) {}
    E e;
};

template <typename T, typename E>
struct upgrade_res {
    template <typename R>
    struct up {
        using type = R;
    };

    template <typename V>
    struct up<Ok<V>> {
        using type = Result<V, E>;
    };

    template <typename V>
    struct up<Err<V>> {
        using type = Result<T, V>;
    };
};

template <typename T, typename E, typename R>
using upgrade_res_t = typename upgrade_res<T, E>::template up<R>::type;

}


template <typename T, typename E>
class Result : private std::variant<T, E> {
    static_assert(!std::is_same<E, void>::value, "E cannot be void");

    template <typename T2, typename E2> friend class Result;

    // Internal Constructor
    static constexpr auto _Ok = std::in_place_index<0>;
    static constexpr auto _Err = std::in_place_index<1>;

    template <size_t I, typename U>
    explicit constexpr Result(std::in_place_index_t<I> i, U &&x) :
        std::variant<T, E>(i, std::forward<U>(x))
    {}

public:
    using value_type = T;
    using error_type = E;

    template <typename U>
    constexpr Result(detail::Ok<U> &&ok) :
        std::variant<T, E>(_Ok, std::move(ok.v))
    {}

    constexpr Result(detail::Err<E> &&err) :
        std::variant<T, E>(_Err, std::move(err.e))
    {}

    template <typename U1, typename U2,
              typename=std::enable_if_t<std::is_convertible_v<U1, T> && std::is_convertible_v<U2, E>>>
    constexpr inline Result(const Result<U1, U2> &o) :
        std::variant<T, E>(o)
    {}

    // Check
    constexpr operator bool() const
    {
        return this->index() == 0;
    }

    // Retrieval
    constexpr const T &value() const&
    {
        return std::get<0>(*this);
    }
    constexpr T &value() &
    {
        return std::get<0>(*this);
    }
    constexpr T &&value() &&
    {
        return std::get<0>(std::move(*this));
    }

    template <typename U>
    constexpr const T &value_or(U &&deft) const&
    {
        if (*this)
            return std::get<0>(*this);
        return std::forward<U>(deft);
    }
    template <typename U>
    constexpr T &value_or(U &&deft) &
    {
        if (*this)
            return std::get<0>(*this);
        return std::forward<U>(deft);
    }
    template <typename U>
    constexpr T &&value_or(U &&deft) &&
    {
        if (*this)
            return std::get<0>(std::move(*this));
        return std::forward<U>(deft);
    }

    constexpr const E &error() const&
    {
        return std::get<1>(*this);
    }
    constexpr E &error() &
    {
        return std::get<1>(*this);
    }
    constexpr E &&error() &&
    {
        return std::get<1>(std::move(*this));
    }

    // Map
    template <typename F>
    constexpr auto map(F f) const & -> Result<std::invoke_result_t<F, const T&>, E>
    {
        using result_type = std::invoke_result_t<F, T const&>;
        if (*this) {
            if constexpr (std::is_same_v<result_type, void>) {
                std::invoke(f, value());
                return Result<result_type, E>(_Ok);
            } else
                return Result<result_type, E>(_Ok, std::invoke(f, value()));
        } else
            return Result<result_type, E>(_Err, error());
    }

    template <typename F>
    constexpr auto map(F f) & -> Result<std::invoke_result_t<F, T&>, E>
    {
        using result_type = std::invoke_result_t<F, T&>;
        if (*this) {
            if constexpr (std::is_same_v<result_type, void>) {
                std::invoke(f, value());
                return Result<result_type, E>(_Ok);
            } else
                return Result<result_type, E>(_Ok, std::invoke(f, value()));
        } else
            return Result<result_type, E>(_Err, error());
    }

    template <typename F>
    constexpr auto map(F f) && -> Result<std::invoke_result_t<F, T&&>, E>
    {
        using result_type = std::invoke_result_t<F, T&&>;
        if (*this) {
            if constexpr (std::is_same_v<result_type, void>) {
                std::invoke(f, std::move(*this).value());
                return Result<result_type, E>(_Ok);
            } else
                return Result<result_type, E>(_Ok, std::invoke(f, std::move(*this).value()));
        } else
            return Result<result_type, E>(_Err, std::move(*this).error());
    }

    // Bind
    template <typename F>
    constexpr auto bind(F f) const & -> detail::upgrade_res_t<T, E, std::invoke_result_t<F, const T&>>
    {
        using R = detail::upgrade_res_t<T, E, std::invoke_result_t<F, const T&>>;
        static_assert(std::is_convertible_v<E, typename R::error_type>, "Result::bind(): cannot return different error type");
        if (*this)
            return std::invoke(f, value());
        else
            return R(_Err, error());
    }

    template <typename F>
    constexpr auto bind(F f) & -> detail::upgrade_res_t<T, E, std::invoke_result_t<F, T&>>
    {
        using R = detail::upgrade_res_t<T, E, std::invoke_result_t<F, T&>>;
        static_assert(std::is_convertible_v<E, typename R::error_type>, "Result::bind(): cannot return different error type");
        if (*this)
            return std::invoke(f, value());
        else
            return R(_Err, error());
    }

    template <typename F>
    constexpr auto bind(F f) && -> detail::upgrade_res_t<T, E, std::invoke_result_t<F, T&&>>
    {
        using R = detail::upgrade_res_t<T, E, std::invoke_result_t<F, T&&>>;
        static_assert(std::is_convertible_v<E, typename R::error_type>, "Result::bind(): cannot return different error type");
        if (*this)
            return std::invoke(f, std::move(*this).value());
        else
            return R(_Err, std::move(*this).error());
    }

    // Bind on error
    template <typename F>
    constexpr auto or_else(F f) const &-> detail::upgrade_res_t<T, E, std::invoke_result_t<F, const E&>>
    {
        using R = detail::upgrade_res_t<T, E, std::invoke_result_t<F, const E&>>;
        static_assert(std::is_same_v<T, typename R::value_type>, "Result::or_else(): cannot return different value type");
        if (*this)
            return R(_Ok, value());
        else
            return std::invoke(f, error());
    }

    template <typename F>
    constexpr auto or_else(F f) && -> detail::upgrade_res_t<T, E, std::invoke_result_t<F, E&&>>
    {
        using R = detail::upgrade_res_t<T, E, std::invoke_result_t<F, E&&>>;
        static_assert(std::is_same_v<T, typename R::value_type>, "Result::or_else(): cannot return different value type");
        if (*this)
            return R(_Ok, std::move(*this).value());
        else
            return std::invoke(f, std::move(*this).error());
    }

    // Discard value
    constexpr auto discard() const & -> Result<void, E>
    {
        if (*this)
            return Result<void, E>(_Ok);
        else
            return Result<void, E>(_Err, error());
    }

    constexpr auto discard() && -> Result<void, E>
    {
        if (*this)
            return Result<void, E>(_Ok);
        else
            return Result<void, E>(_Err, std::move(*this).error());
    }
};

template <typename E>
class Result<void, E> : private std::optional<E> {
    template <typename T2, typename E2> friend class Result;

    // Internal Constructor.
    // Format compatible w/ non-void
    static constexpr auto _Ok = std::in_place_index<0>;
    static constexpr auto _Err = std::in_place_index<1>;

    template <typename U>
    explicit constexpr Result(std::in_place_index_t<1>, U &&e) :
        std::optional<E>(std::forward<U>(e))
    {}

    explicit constexpr Result(std::in_place_index_t<0>) :
        std::optional<E>()
    {}

public:
    using value_type = void;
    using error_type = E;

    constexpr Result(detail::Ok<void> &&) :
        std::optional<E>()
    {}

    constexpr Result(detail::Err<E> &&err) :
        std::optional<E>(std::move(err.e))
    {}

    template <typename U2, typename=std::enable_if_t<std::is_convertible_v<U2, E>>>
    constexpr inline Result(const Result<void, U2> &o) :
        std::optional<E>(o)
    {}

    // Check
    constexpr operator bool() const
    {
        return !std::optional<E>::operator bool();
    }

    // Retrieval
    constexpr const E &error() const &
    {
        return std::optional<E>::value();
    }
    constexpr E &error() &
    {
        return std::optional<E>::value();
    }
    constexpr E &&error() &&
    {
        return static_cast<std::optional<E>&&>(std::move(*this)).value();
    }

    // Map
    template <typename F>
    constexpr auto map(F f) const & -> Result<std::invoke_result_t<F>, E>
    {
        using result_type = std::invoke_result_t<F>;
        if (*this) {
            if constexpr (std::is_same_v<result_type, void>) {
                std::invoke(f);
                return Result<result_type, E>(_Ok);
            } else
                return Result<result_type, E>(_Ok, std::invoke(f));
        } else
            return Result<result_type, E>(_Err, error());
    }

    template <typename F>
    constexpr auto map(F f) && -> Result<std::invoke_result_t<F>, E>
    {
        using result_type = std::invoke_result_t<F>;
        if (*this) {
            if constexpr (std::is_same_v<result_type, void>) {
                std::invoke(f);
                return Result<result_type, E>(_Ok);
            } else
                return Result<result_type, E>(_Ok, std::invoke(f));
        } else
            return Result<result_type, E>(_Err, std::move(*this).error());
    }

    // Bind
    template <typename F>
    constexpr auto bind(F f) const & -> detail::upgrade_res_t<void, E, std::invoke_result_t<F>>
    {
        using R = detail::upgrade_res_t<void, E, std::invoke_result_t<F>>;
        static_assert(std::is_convertible_v<E, typename R::error_type>, "Result::bind(): cannot return different error type");
        if (*this)
            return std::invoke(f);
        else
            return R(_Err, error());
    }

    template <typename F>
    constexpr auto bind(F f) && -> detail::upgrade_res_t<void, E, std::invoke_result_t<F>>
    {
        using R = detail::upgrade_res_t<void, E, std::invoke_result_t<F>>;
        static_assert(std::is_convertible_v<E, typename R::error_type>, "Result::bind(): cannot return different error type");
        if (*this)
            return std::invoke(f);
        else
            return R(_Err, std::move(*this).error());
    }

    // Misc
    template <typename F>
    constexpr auto or_else(F f) const & -> detail::upgrade_res_t<void, E, std::invoke_result_t<F, const E&>>
    {
        using R = detail::upgrade_res_t<void, E, std::invoke_result_t<F, const E&>>;
        static_assert(std::is_same_v<void, typename R::value_type>, "Result::or_else(): cannot return different value_type");
        if (*this)
            return R(_Ok);
        else
            return std::invoke(f, error());
    }

    template <typename F>
    constexpr auto or_else(F f) && -> detail::upgrade_res_t<void, E, std::invoke_result_t<F, E&&>>
    {
        using R = detail::upgrade_res_t<void, E, std::invoke_result_t<F, E&&>>;
        static_assert(std::is_same_v<void, typename R::value_type>, "Result::or_else(): cannot return different value_type");
        if (*this)
            return R(_Ok);
        else
            return std::invoke(f, std::move(this).error());
    }

    constexpr auto discard() const & -> Result<void, E>
    {
        if (*this)
            return Result(_Ok);
        else
            return Result(_Err, error());
    }

    constexpr auto discard() && -> Result<void, E>
    {
        if (*this)
            return Result(_Ok);
        else
            return Result(_Err, std::move(*this).error());
    }
};

// Ok
template <typename T>
constexpr detail::Ok<T> Ok(T &&result)
{
    return std::forward<T>(result);
}

constexpr detail::Ok<void> Ok()
{
    return detail::Ok<void>();
}

// Error
template <typename E>
constexpr detail::Err<E> Err(E &&error)
{
    return std::forward<E>(error);
}

}
