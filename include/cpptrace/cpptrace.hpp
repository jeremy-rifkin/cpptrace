#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(__CYGWIN__)
 #define CPPTRACE_API __declspec(dllexport)
#else
 #define CPPTRACE_API
#endif

namespace cpptrace {
    struct stacktrace_frame {
        uintptr_t address;
        std::uint_least32_t line;
        std::uint_least32_t col;
        std::string filename;
        std::string symbol;
    };
    CPPTRACE_API std::vector<stacktrace_frame> generate_trace(std::uint32_t skip = 0);
    CPPTRACE_API void print_trace(std::uint32_t skip = 0);
}

#endif
