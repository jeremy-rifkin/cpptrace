#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace cpptrace {
    struct stacktrace_frame {
        uintptr_t address;
        int line;
        int col;
        std::string filename;
        std::string symbol;
    };
    std::vector<stacktrace_frame> generate_trace();
    void print_trace();
}

#endif
