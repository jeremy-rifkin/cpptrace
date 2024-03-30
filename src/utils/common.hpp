#ifndef COMMON_HPP
#define COMMON_HPP

#define IS_WINDOWS 0
#define IS_LINUX 0
#define IS_APPLE 0

#if defined(_WIN32)
 #undef IS_WINDOWS
 #define IS_WINDOWS 1
#elif defined(__linux)
 #undef IS_LINUX
 #define IS_LINUX 1
#elif defined(__APPLE__)
 #undef IS_APPLE
 #define IS_APPLE 1
#else
 #error "Unexpected platform"
#endif

#define IS_CLANG 0
#define IS_GCC 0
#define IS_MSVC 0

#if defined(__clang__)
 #undef IS_CLANG
 #define IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #undef IS_GCC
 #define IS_GCC 1
#elif defined(_MSC_VER)
 #undef IS_MSVC
 #define IS_MSVC 1
#else
 #error "Unsupported compiler"
#endif

#include <cstdio>
#include <stdexcept>
#include <string>

#include <cpptrace/cpptrace.hpp>

#define ESC     "\033["
#define RESET   ESC "0m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define BLUE    ESC "34m"
#define MAGENTA ESC "35m"
#define CYAN    ESC "36m"

#if IS_GCC || IS_CLANG
 #define NODISCARD __attribute__((warn_unused_result))
// #elif IS_MSVC && _MSC_VER >= 1700
//  #define NODISCARD _Check_return_
#else
 #define NODISCARD
#endif

namespace cpptrace {
namespace detail {
    static const stacktrace_frame null_frame {
        0,
        0,
        nullable<uint32_t>::null(),
        nullable<uint32_t>::null(),
        "",
        "",
        false
    };

    bool should_absorb_trace_exceptions();
    bool should_resolve_inlined_calls();
    enum cache_mode get_cache_mode();
}
}

#endif
