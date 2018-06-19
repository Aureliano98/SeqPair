// toolbox.h: small tools
// Author: LYL (Aureliano Lee)

#pragma once
#include <iostream>
#include <utility>
#include "xaureliano.h"

#define AURELIANO_PRINT_EXPRS_V(EXPRS, DELIM, OUT) OUT << #EXPRS << DELIM << \
(EXPRS)
#define AURELIANO_PRINT_EXPRS(EXPRS) AURELIANO_PRINT_EXPRS_V(EXPRS, " == ", \
std::cout)
#define AURELIANO_PRINT_POSITION_V(OUT) AURELIANO detail::print_position( \
__FILE__, __LINE__, __FUNCTION__, OUT)
#define AURELIANO_PRINT_POSITION() AURELIANO_PRINT_POSITION_V(std::cout)

AURELIANO_BEGIN
namespace detail {
    // Prints current position.
    inline std::ostream &print_position(const char *file, int line,
        const char *func, std::ostream &out) {
        return out << file << "(" << line << "), " << func;
    }
}   // namespace detail

// Prints [first, last).
template<typename InIt>
inline std::ostream &print(InIt first, InIt last, 
    std::ostream &out = std::cout) {
    while (first != last)
        out << *first++ << " ";
    return out;
}

// Prints container.
template<typename Container>
inline std::ostream &print(Container &&container, 
    std::ostream &out = std::cout) {
    return print(std::begin(std::forward<Container>(container)), 
        std::end(std::forward<Container>(container)));
}

// Implements std::void_t (C++17).
template<typename ...>
using Void_t = void;

// Checks whether InIt is iterator
// by checking typename InIt::iterator_category
template<typename InIt, typename = void>
struct IsIterator : public std::false_type {};

// Checks whether InIt is iterator.
// by checking typename InIt::iterator_category
template<typename InIt>
struct IsIterator<InIt, Void_t<
    typename std::iterator_traits<InIt>::iterator_category
    > > : public std::true_type {};
AURELIANO_END
