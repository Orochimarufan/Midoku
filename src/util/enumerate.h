#pragma once

#include <utility>
#include <cstdlib>


namespace Midoku::Util {

/**
 * @brief The enumerate class
 *
 */
template <typename T>
struct enumerate {
    T & source;

    enumerate(T &s) :
        source(s)
    {}

    struct iter {
        using source_type = decltype(std::declval<T>().begin());
        using item_type = decltype(*std::declval<source_type>());

        source_type here;
        size_t index;

        iter(source_type &&s) :
            here(s),
            index(0)
        {}

        iter(source_type &s) :
            here(s),
            index(0)
        {}

        bool operator != (const iter &o) const {
            return here != o.here;
        }

        bool operator == (const iter &o) const {
            return here == o.here;
        }

        iter &operator++() {
            here++;
            index++;
            return *this;
        }

        std::pair<size_t, item_type> operator *() const {
            return {index, *here};
        }
    };

    iter begin() {
        return iter(source.begin());
    }

    iter end() {
        return iter(source.end());
    }
};

}
