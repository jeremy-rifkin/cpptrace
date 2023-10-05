#ifndef COMMON_HPP
#define COMMON_HPP

#ifdef _MSC_VER
 #define CPPTRACE_FORCE_NO_INLINE __declspec(noinline)
#else
 #define CPPTRACE_FORCE_NO_INLINE __attribute__((noinline))
#endif

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

namespace cpptrace {
namespace detail {
    // Placed here instead of utils because it's used by error.hpp and utils.hpp
    template<typename... T> std::string stringf(T... args) {
        int length = snprintf(nullptr, 0, args...);
        if(length < 0) {
            throw std::logic_error("invalid arguments to stringf");
        }
        std::string str(length, 0);
        // .data is const char* in c++11, but &str[0] should be legal
        snprintf(&str[0], length + 1, args...);
        return str;
    }

    static const stacktrace_frame null_frame {0, 0, UINT_LEAST32_MAX, "", ""};
}
}

#endif
