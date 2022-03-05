#pragma once

#include <tuple>

#include <cstdlib>


namespace Midoku::Util {


/**
 * @brief Describes a sequence/collection of types
 */
template <typename... Ts>
struct type_sequence;

template <typename... Ts>
struct type_sequence_common
{
    template <typename... Ts2> using prepend = type_sequence<Ts2..., Ts...>;
    template <typename... Ts2> using append = type_sequence<Ts..., Ts2...>;
    template <typename T>      using concat = typename T::template prepend<Ts...>;

    static constexpr size_t size = sizeof...(Ts);

    // Mapping
    template <template<typename> typename TypeMap>
    using type_map = type_sequence<typename TypeMap<Ts>::type...>;

    template <template<typename> typename Ftor>
    static constexpr auto tuple_map() {
        return std::make_tuple(Ftor<Ts>()()...);
    }

    template <template<typename> typename Ftor, typename Cookie>
    static constexpr auto tuple_map(Cookie &&c) {
        return std::make_tuple(Ftor<Ts>()(std::forward<Cookie>(c))...);
    }

    template <template<typename> typename Ftor, typename R>
    static constexpr R map() {
        return {Ftor<Ts>()()...};
    }

    // Folding
    template <template <typename> typename Predicate>
    static constexpr bool all() {
        return (Predicate<Ts>::value && ...);
    }

    template <template <typename> typename Predicate>
    static constexpr bool any() {
        return (Predicate<Ts>::value || ...);
    }

    // Tuples
    using tuple = std::tuple<Ts...>;

    // Index sequences
    using index_sequence = std::index_sequence_for<Ts...>;
};

static_assert (type_sequence_common<>::template all<std::is_void>() == true);
static_assert (type_sequence_common<void,void,void>::template all<std::is_void>() == true);
static_assert (type_sequence_common<void,int,void>::template all<std::is_void>() == false);
static_assert (type_sequence_common<int,int,int>::template any<std::is_void>() == false);
static_assert (type_sequence_common<int,int,void>::template any<std::is_void>() == true);


template <typename Head, typename... Tail>
struct type_sequence<Head, Tail...> : public type_sequence_common<Head, Tail...>
{
    using head = Head;
    using tail = type_sequence<Tail...>;

    static constexpr bool is_empty = false;
    static constexpr bool is_last = tail::is_empty;

    template <typename T>
    static constexpr ssize_t index = std::is_same_v<T, Head> ? 0 : tail::template index<T> + 1;

    template <typename T>
    static constexpr bool contains = std::is_same_v<T, Head> ? true : tail::template contains<T>;

    template <bool is_last> struct _last       { using type = typename tail::last; };
    template <>             struct _last<true> { using type = Head; };
    using last = typename _last<tail::is_empty>::type;

    template <std::size_t I> struct _get    { using type = typename tail::template _get<I-1>::type; };
    template <>              struct _get<0> { using type = Head; };
    template <std::size_t I> using get = typename _get<I>::type;

    // Filtering
    template <template<typename>typename TypeFilter>
    using filter = typename std::conditional<TypeFilter<Head>::value,
                typename tail::template filter<TypeFilter>::template prepend<Head>,
                typename tail::template filter<TypeFilter>>::type;

    template <template<typename>typename TypeKey, template<typename>typename TypeFilter>
    using filter_by_key = typename std::conditional<TypeFilter<typename TypeKey<Head>::type>::value,
                typename tail::template filter_by_key<TypeKey, TypeFilter>::template prepend<Head>,
                typename tail::template filter_by_key<TypeKey, TypeFilter>>::type;

    using make_unique = typename std::conditional<!tail::template contains<Head>,
                typename tail::make_unique::template prepend<Head>,
                typename tail::make_unique>::type;

    // Folding
    template <template <typename> typename FoldFunctor, typename B>
    static constexpr B fold(B init) {
        return FoldFunctor<head>()(tail::template fold<FoldFunctor>(init));
    }

    //template<template <typename, typename> typename TypeFold, typename B>
    //using type_fold = std::conditional<is_empty, B, TypeFold<head, typename tail::template type_fold<TypeFold, B>>>;
};

template <>
struct type_sequence<> : public type_sequence_common<>
{
    static constexpr bool is_empty = true;

    template <template<typename> typename TypeFilter>
    using filter = type_sequence<>;
    template <template<typename>typename TypeKey, template<typename>typename TypeFilter>
    using filter_by_key = type_sequence<>;
    using make_unique = type_sequence<>;

    template <typename T>
    static constexpr ssize_t index = -1;

    template <typename T>
    static constexpr bool contains = false;

    template <template<typename> typename FoldFunctor, typename B>
    static constexpr B fold(B init) {
        return init;
    }
};


static_assert (std::is_same_v<type_sequence<int,int,int>::template filter<std::is_void>,
                              type_sequence<>>);
static_assert (std::is_same_v<type_sequence<void,int,void>::template filter<std::is_void>,
                              type_sequence<void,void>>);

static_assert (type_sequence<void,int,long>::index<int> == 1);

}
