#ifndef CPPTRACE_DEMANGLE_HPP
#define CPPTRACE_DEMANGLE_HPP

#include <cpptrace/cpptrace.hpp>

#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string&);
    }
}

#endif
