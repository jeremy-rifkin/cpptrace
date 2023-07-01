#ifndef LIBCPP_DEMANGLE_HPP
#define LIBCPP_DEMANGLE_HPP

#include <cpptrace/cpptrace.hpp>

#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string&);
    }
}

#endif
