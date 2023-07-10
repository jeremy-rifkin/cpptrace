#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cpptrace {
    struct stacktrace_frame {
        uintptr_t address;
        std::uint_least32_t line;
        std::uint_least32_t col;
        std::string filename;
        std::string symbol;
    };
    std::vector<stacktrace_frame> generate_trace(std::uint32_t skip = 0);
    void print_trace(std::uint32_t skip = 0);
}

#endif
