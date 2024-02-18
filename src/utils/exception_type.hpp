#ifndef EXCEPTION_TYPE_HPP
#define EXCEPTION_TYPE_HPP

#include <string>

// libstdc++ and libc++
#if defined(CPPTRACE_HAS_CXX_EXCEPTION_TYPE) && defined(__GLIBCXX__) || defined(__GLIBCPP__) || defined(_LIBCPP_VERSION)
 #include <cxxabi.h>
#endif

#include "../demangle/demangle.hpp"

namespace cpptrace {
namespace detail {
    inline std::string exception_type_name() {
        #if defined(CPPTRACE_HAS_CXX_EXCEPTION_TYPE) && defined(__GLIBCXX__) || defined(__GLIBCPP__) || defined(_LIBCPP_VERSION)
        const std::type_info* t = abi::__cxa_current_exception_type();
        return t ? detail::demangle(t->name()) : "<unknown>";
        #else
        return "<unknown>";
        #endif
    }
}
}

#endif
