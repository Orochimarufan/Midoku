/**
 * Flat tuple implementation
 * Based on libc++
 */

#pragma once

#include <memory>
#include <type_traits>
#include <utility>


namespace Midoku::Util {

// Index sequence
template <size_t... Is>
using index_sequence = std::index_sequence<Is...>;

template <typename... Ts>
using index_sequence_for = std::index_sequence_for<Ts...>;


// Tuple Implementation
template <typename... Ts>
class tuple;

namespace detail {

template <bool... conds>
struct is_all {
    using value_type = bool;
    static constexpr bool value = (conds&&...);
};

template <bool... conds>
constexpr bool is_all_v = is_all<conds...>::value;

template <typename T, typename>
struct dependent_type : T {};

template <std::size_t I, typename T, bool E=std::is_empty_v<T>>
struct tuple_leaf {
    using value_type = T;

    T value;

    // Default Constructor
    constexpr tuple_leaf() noexcept(std::is_nothrow_default_constructible_v<T>) :
        value()
    {}

    // Forwarding
    template <typename U,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, tuple_leaf> && std::is_constructible_v<T, U>>>
    explicit constexpr tuple_leaf(U &&u) noexcept(std::is_nothrow_constructible_v<T, U>) :
        value(std::forward<U>(u))
    {}

    /*// Allocator
    template <typename A>
    inline tuple_leaf(std::allocator_arg_t, const std::enable_if_t<!std::uses_allocator_v<T, A>, A> &alloc) :
        value()
    {}

    template <typename A>
    inline tuple_leaf(std::allocator_arg_t, const std::enable_if_t<std::uses_allocator_v<T, A> && std::is_constructible_v<T, std::allocator_arg_t, A>, A> &alloc) :
        value(std::allocator_arg, alloc)
    {}

    template <typename A>
    inline tuple_leaf(std::allocator_arg_t, const std::enable_if_t<std::uses_allocator_v<T, A> && !std::is_constructible_v<T, std::allocator_arg_t, A>, A> &alloc) :
        value(alloc)
    {}*/

    // Forwarding Allocator
    template <typename A, typename... Us>
    explicit inline tuple_leaf(std::allocator_arg_t, const std::enable_if_t<!std::uses_allocator_v<T, A>, A> &alloc, Us&&... us) :
        value(std::forward<Us>(us)...)
    {}

    template <typename A, typename... Us>
    explicit inline tuple_leaf(std::allocator_arg_t, const std::enable_if_t<std::uses_allocator_v<T, A> && std::is_constructible_v<T, std::allocator_arg_t, A>, A> &alloc, Us&&... us) :
        value(std::allocator_arg, alloc, std::forward<Us>(us)...)
    {}

    template <typename A, typename... Us>
    explicit inline tuple_leaf(const std::enable_if_t<std::uses_allocator_v<T, A> && !std::is_constructible_v<T, std::allocator_arg_t, A>, A> &alloc, Us&&... us) :
        value(alloc, std::forward<Us>(us)...)
    {}

    // Copy/Move
    tuple_leaf(const tuple_leaf &) = default;
    tuple_leaf(tuple_leaf &&) = default;

    // Assign
    template <typename U>
    tuple_leaf &operator =(U &&u) noexcept(std::is_nothrow_assignable_v<T&, U>)
    {
        value = std::forward<U>(u);
        return *this;
    }

    // Swap
    template <typename U>
    void swap(tuple_leaf &o) noexcept(std::is_nothrow_swappable_v<T>)
    {
        std::swap(value, o.value);
    }

    T &get()
    {
        return value;
    }

    const T &get() const
    {
        return value;
    }
};

template <typename ISeq, typename... Ts>
struct tuple_data;

template <std::size_t... Is, typename... Ts>
struct tuple_data<index_sequence<Is...>, Ts...> : public tuple_leaf<Is, Ts>... {
    // Constructors
    constexpr tuple_data() = default;

    template <typename... Us>
    explicit constexpr tuple_data(Us&&... us) noexcept((std::is_nothrow_constructible_v<Ts, Us>&&...)) :
        tuple_leaf<Is, Ts>(std::forward<Us>(us))...
    {}

    template <typename A, typename... Us>
    constexpr tuple_data(std::allocator_arg_t, const A &alloc, Us&&... us) :
        tuple_leaf<Is, Ts>(std::allocator_arg, alloc, std::forward<Us>(us))...
    {}

    template <typename Tup,
              typename = std::enable_if_t<std::tuple_size_v<Tup> == sizeof...(Ts) && detail::is_all_v<std::is_constructible_v<Ts, std::tuple_element_t<Is, Tup>>...>>>
    constexpr tuple_data(Tup &&tup) :
        tuple_leaf<Is, Ts>(std::forward<std::tuple_element_t<Is, Tup>>(std::get<Is>(tup)))...
    {}

    tuple_data(const tuple_data &) = default;
    tuple_data(tuple_data &&) = default;

    // Assignment
    inline tuple_data &operator =(const tuple_data &t) noexcept((std::is_nothrow_copy_assignable_v<Ts>&&...))
    {
        (static_cast<void>(tuple_leaf<Is, Ts>::operator =(static_cast<const tuple_leaf<Is, Ts> &>(t).get())),...);
        return *this;
    }

    inline tuple_data &operator =(tuple_data &&t) noexcept((std::is_nothrow_move_assignable_v<Ts>&&...))
    {
        (static_cast<void>(tuple_leaf<Is, Ts>::operator =(std::forward<Ts>(static_cast<tuple_leaf<Is, Ts> &>(t).get()))),...);
        return *this;
    }

    template <typename Tup,
              typename = std::enable_if_t<std::is_assignable_v<Tup, tuple<Ts...>>>>
    inline tuple_data &operator =(Tup &&tup) noexcept((std::is_nothrow_assignable_v<Ts&, typename std::tuple_element_t<Is, Tup>::type>&&...))
    {
        (static_cast<void>(tuple_leaf<Is, Ts>::operator =(std::forward<typename std::tuple_element_t<Is, Tup>::type>(std::get<Is>(std::forward<Tup>(tup))))),...);
        return *this;
    }

    inline void swap(tuple_data &o) noexcept((std::is_nothrow_swappable_v<Ts>&&...))
    {
        (static_cast<void>(tuple_leaf<Is, Ts>::swap(static_cast<tuple_leaf<Is, Ts>&>(o))),...);
    }
};

}

// Tuple
template <typename... Ts>
class tuple {
    using data_type = detail::tuple_data<index_sequence_for<Ts...>, Ts...>;

    data_type data;

public:
    tuple(const tuple &) = default;
    tuple(tuple &&) = default;

    // Value constructors
    template <typename _d = void,
              typename = std::enable_if_t<detail::is_all_v<detail::dependent_type<std::is_default_constructible<Ts>, _d>::value...>>>
    constexpr tuple() noexcept(detail::is_all_v<std::is_nothrow_default_constructible_v<Ts>...>)
    {}

    template <typename _d = void,
              typename = std::enable_if_t<detail::is_all_v<detail::dependent_type<std::is_copy_constructible<Ts>, _d>::value...>>>
    constexpr tuple(const Ts&... ts) noexcept(detail::is_all_v<std::is_nothrow_copy_constructible_v<Ts>...>) :
        data{ts...}
    {}

    template <typename... Us,
              typename = std::enable_if_t<sizeof...(Ts)==sizeof...(Us) && detail::is_all_v<std::is_constructible_v<Ts, Us>...>>>
    constexpr tuple(Us&&... us) noexcept(detail::is_all_v<std::is_nothrow_constructible_v<Ts, Us>...>) :
        data{std::forward<Us>(us)...}
    {}

    // Allocator constructors
    template <typename A,
              typename _d = void,
              typename = std::enable_if_t<detail::is_all_v<detail::dependent_type<std::is_default_constructible<Ts>, _d>::value...>>>
    tuple(std::allocator_arg_t, const A &alloc) :
        data(std::allocator_arg, alloc)
    {}

    template <typename A,
              typename _d = void,
              typename = std::enable_if_t<detail::is_all_v<detail::dependent_type<std::is_copy_constructible<Ts>, _d>::value...>>>
    tuple(std::allocator_arg_t, const A &alloc, const Ts&... ts) :
        data(std::allocator_arg, alloc, ts...)
    {}

    template <typename A,
              typename... Us,
              typename = std::enable_if_t<detail::is_all_v<std::is_constructible_v<Ts, Us>...>>>
    tuple(std::allocator_arg_t, const A &alloc, Us&&... us) :
        data(std::allocator_arg, alloc, std::forward<Us>(us)...)
    {}

    // Tuple-Like constructor
    template <typename Tup,
              typename = std::enable_if<std::is_constructible_v<data_type, Tup>>>
    tuple(Tup &&tup) :
        data(std::forward<Tup>(tup))
    {}

    // Assignment
    template <typename Tup,
              typename = std::enable_if<std::is_assignable_v<data_type&, Tup>>>
    tuple &operator =(Tup &&tup) noexcept(std::is_nothrow_assignable_v<data_type&, Tup>)
    {
        data = std::forward<Tup>(tup);
        return *this;
    }

    // Swap
    void swap(tuple &t) noexcept(std::is_nothrow_swappable_v<data_type>)
    {
        data.swap(t.data);
    }
};

template <>
class tuple<> {
    constexpr tuple() noexcept
    {}

    template <typename A>
    tuple(std::allocator_arg_t, const A &) noexcept
    {}

    template <typename A>
    tuple(std::allocator_arg_t, const A &, const tuple &) noexcept
    {}

    void swap(tuple &) noexcept
    {}
};

}

namespace std {

//template <typename... Ts, typename A>
//struct uses_allocator<Midoku::Util::tuple<Ts...>, A> : std::true_type {};

}
