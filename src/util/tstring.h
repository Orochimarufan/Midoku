#pragma once

#include <cstdlib>


namespace Midoku::Util {

template <typename C, C... str>
struct tstring
{
    static constexpr C const value[sizeof...(str)+1] = {str..., '\0'};
    static constexpr size_t size = sizeof...(str)+1;

    constexpr operator const C *() {
        return value;
    }
};

template <typename C, C...  str>
constexpr  const C  tstring<C, str...>::value[sizeof...(str)+1];


// concat
template <typename C, C... str1, C...str2>
constexpr tstring<C, str1..., str2...> tstring_concat(tstring<C, str1...>, tstring<C, str2...>)
{
    return tstring<C, str1..., str2...>();
}

template <typename C, C... str1, C...str2, C...str3>
constexpr tstring<C, str1..., str2..., str3...> tstring_concat(tstring<C, str1...>, tstring<C, str2...>, tstring<C, str3...>)
{
    return tstring<C, str1..., str2..., str3...>();
}


/*#define MACRO_GET_1(str, i) \
(sizeof(str) > (i) ? str[(i)] : 0)

#define MACRO_GET_4(str, i) \
MACRO_GET_1(str, i+0),  \
MACRO_GET_1(str, i+1),  \
MACRO_GET_1(str, i+2),  \
MACRO_GET_1(str, i+3)

#define MACRO_GET_16(str, i) \
MACRO_GET_4(str, i+0),   \
MACRO_GET_4(str, i+4),   \
MACRO_GET_4(str, i+8),   \
MACRO_GET_4(str, i+12)

#define MACRO_GET_64(str, i) \
MACRO_GET_16(str, i+0),  \
MACRO_GET_16(str, i+16), \
MACRO_GET_16(str, i+32), \
MACRO_GET_16(str, i+48)

#define TSTRING(str) ::Midoku::Util::tstring<char, MACRO_GET_64(#str, 0), 0 >*/

}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
// Should be replaced in C++20

template <typename CharT, CharT... chars>
constexpr ::Midoku::Util::tstring<CharT, chars...> operator""_T() {
    return {};
}

#pragma clang diagnostic pop

#define TSTR(s) decltype(s ## _T)
