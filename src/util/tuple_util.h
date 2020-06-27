#pragma once

#include <tuple>
#include <utility>
#include <cstdlib>
#include <optional>

namespace Midoku::Util {


#define M_TEMPLATE_FUNCTOR(R, name, T, args, body) \
    template <typename T> \
    struct name { \
        R operator() args { \
            body\
        } \
    }


// ----------------------------------------------------------------------------
// Template Functor Map
template <typename R, template <typename> typename Ftor, typename T, size_t... Is>
constexpr R tuple_map(const T &tup, std::index_sequence<Is...>) {
    return {Ftor<typename std::tuple_element<Is, T>::type>()(std::get<Is>(tup))...};
}

template <typename R, template <typename> typename Ftor, typename... Ts>
constexpr R tuple_map(const std::tuple<Ts...> &tup) {
    return tuple_map(tup, std::index_sequence_for<Ts...>());
}

// ----------------------------------------------------------------------------
// Get Index sequence for tuple-like
template <typename Tup>
constexpr auto tuple_index_sequence() {
    return std::make_index_sequence<std::tuple_size_v<std::decay_t<Tup>>>();
}

template <size_t I, size_t... Is>
constexpr size_t index_first(std::index_sequence<I, Is...>) {
    return I;
}

template <size_t I, size_t... Is>
constexpr std::index_sequence<Is...> index_rest(std::index_sequence<I, Is...>) {
    return std::index_sequence<Is...>();
}

// ----------------------------------------------------------------------------
// Class Functor fold
template <typename R, typename Tup, typename F, size_t... Is>
constexpr R tuple_fold(Tup &&t, F f, R &&acc, std::index_sequence<Is...> i) {
    if constexpr(!i.size()) {
        return acc;
    } else {
        return tuple_fold(std::forward<Tup>(t), f,
                          f(std::forward<R>(acc), std::get<index_first(i)>(std::forward<Tup>(t))),
                          index_rest(i));
    }
}

template <typename R, typename Tup, typename F>
constexpr R tuple_fold(Tup &&t, F f, R &&acc) {
    return tuple_fold(std::forward<Tup>(t), f, std::forward<R>(acc), tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Enumerate/fold
template <typename R, typename Tup, typename F, size_t... Is>
constexpr auto tuple_enumerate_fold(Tup &&t, F f, R &&acc, std::index_sequence<Is...> i) {
    if constexpr(!i.size()) {
        return acc;
    } else {
        return tuple_enumerate_fold(std::forward<Tup>(t), f,
                          f(std::forward<R>(acc), index_first(i), std::get<index_first(i)>(std::forward<Tup>(t))),
                                    index_rest(i));
    }
}

template <typename R, typename Tup, typename F>
constexpr auto tuple_enumerate_fold(Tup &&t, F f, R &&acc) {
    return tuple_emumerate_fold(std::forward<Tup>(t), f, std::forward<R>(acc), tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Map
template <typename R, typename F, typename Tup, size_t... Is>
constexpr R tuple_map(const Tup &tup, F f, std::index_sequence<Is...>) {
    return {f(std::get<Is>(tup))...};
}

template <typename R, typename F, typename Tup>
constexpr R tuple_map(const Tup &tup, F f) {
    return tuple_map<R>(tup, f, tuple_index_sequence<Tup>());
}

template <template <typename...> typename R=std::tuple, typename F, typename Tup, size_t... Is>
constexpr auto tuple_map(const Tup &tup, F f, std::index_sequence<Is...>) {
    return R{f(std::get<Is>(tup))...};
}

template <template <typename...> typename R=std::tuple, typename F, typename Tup>
constexpr auto tuple_map(const Tup &tup, F f) {
    return tuple_map<R>(tup, f, tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Enumerate/Map
template <typename R, typename F, typename Tup, size_t... Is>
constexpr auto tuple_enumerate_map(Tup &&tup, F f, std::index_sequence<Is...>) {
    return R{f(Is, std::get<Is>(std::forward<Tup>(tup)))...};
}

template <typename R, typename F, typename Tup>
constexpr auto tuple_enumerate_map(Tup &&tup, F f) {
    return tuple_enumerate_map<R>(std::forward<Tup>(tup), f, tuple_index_sequence<Tup>());
}

template <template <typename...> typename R=std::tuple, typename F, typename Tup, size_t... Is>
constexpr auto tuple_enumerate_map(Tup &&tup, F f, std::index_sequence<Is...>) {
    return R{f(Is, std::get<Is>(std::forward<Tup>(tup)))...};
}

template <template <typename...> typename R=std::tuple, typename F, typename Tup>
constexpr auto tuple_enumerate_map(Tup &&tup, F f) {
    return tuple_enumerate_map<R>(std::forward<Tup>(tup), f, tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Foreach
template <typename F, typename Tup, size_t... Is>
constexpr void tuple_foreach(const Tup &tup, F f, std::index_sequence<Is...>) {
    return (f(std::get<Is>(tup)), ...);
}

template <typename F, typename Tup>
constexpr void tuple_foreach(const Tup &tup, F f) {
    return tuple_foreach(tup, f, tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Foreach/Enumerate
template <typename F, typename Tup, size_t... Is>
constexpr void tuple_enumerate_foreach(const Tup &tup, F f, std::index_sequence<Is...>) {
    return (f(Is, std::get<Is>(tup)), ...);
}

template <typename F, typename Tup>
constexpr void tuple_enumerate_foreach(const Tup &tup, F f) {
    return tuple_enumerate_foreach(tup, f, tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Class Functor Dispatch
template <typename R, typename Tup, typename F, size_t... Is>
constexpr std::optional<R> tuple_dispatch2(const Tup &tup, size_t index, F f, std::index_sequence<Is...> i) {
    if constexpr (!i.size())
        return std::nullopt;
    else if (index_first(i) == index)
        return f(std::get<index_first(i)>(tup));
    else
        return tuple_dispatch2(tup, index, f, index_rest(i));
}

template <typename R, typename Tup, typename F>
constexpr std::optional<R> tuple_dispatch2(const Tup &tup, size_t index, F f) {
    return tuple_dispatch2(tup, index, f, tuple_index_sequence<Tup>());
}


template <typename F, typename Tup, size_t... Is>
constexpr void tuple_dispatch(const Tup &tup, size_t index, F f, std::index_sequence<Is...> i) {
    if constexpr (i.size()> 0) {
        if (index_first(i) == index)
            f(std::get<index_first(i)>(tup));
        else
            tuple_dispatch(tup, index, f, index_rest(i));
    }
}

template <typename F, typename Tup>
constexpr void tuple_dispatch(const Tup &tup, size_t index, F f) {
    tuple_dispatch(tup, index, f, tuple_index_sequence<Tup>());
}

// ----------------------------------------------------------------------------
// Append/Prepend
namespace detail {
template <template <typename...> typename T, typename... Ts, size_t... I1, typename... Tas>
auto append_impl(const T<Ts...> &t, std::index_sequence<I1...>, Tas &&... os)
{
    return T{std::get<I1>(t)..., std::forward<Tas>(os)...};
}

template <template <typename...> typename T, typename... Ts, size_t... I1, typename... Tas>
auto prepend_impl(const T<Ts...> &t, std::index_sequence<I1...>, Tas &&... os)
{
    return T{std::forward<Tas>(os)..., std::get<I1>(t)...};
}
}

template <template <typename...> typename T, typename... Ts, typename... Tas>
T<Ts..., Tas...> tuple_append(const T<Ts...> &t, Tas &&... os) {
    return detail::append_impl(t, std::index_sequence_for<Ts...>(), std::forward<Tas>(os)...);
}

template <template <typename...> typename T, typename... Ts, typename... Tas>
T<Ts..., Tas...> tuple_prepend(const T<Ts...> &t, Tas &&... os) {
    return detail::prepend_impl(t, std::index_sequence_for<Ts...>(), std::forward<Tas>(os)...);
}

// ----------------------------------------------------------------------------
// Fold into monad
template <typename M, typename F, typename Tup>
constexpr auto tuple_fold_bind(M &&m, Tup &&, F, std::index_sequence<>) {
    return std::forward<M>(m);
}

template <typename M, typename F, typename Tup, size_t I, size_t... Is>
constexpr auto tuple_fold_bind(M &&m, Tup &&tup, F f, std::index_sequence<I, Is...>) {
    return tuple_fold_bind(
                std::forward<M>(m).bind([&f,&tup](auto ...xs) {
                    return f(std::get<I>(std::forward<Tup>(tup)), xs...);
                }),
                std::forward<Tup>(tup),
                f,
                std::index_sequence<Is...>());
}

/**
 * @brief fold tuple by binding to m consecutively
 * @param m The monadish object
 * @param tup A tuple
 * @param f A function f([tuple element], [bind value(s)...]) -> M
 */
template <typename M, typename F, typename... Ts>
constexpr auto tuple_fold_bind(M &&m, const std::tuple<Ts...> &tup, F f) {
    return tuple_fold_bind(std::forward<M>(m), tup, f, std::index_sequence_for<Ts...>());
}

}

