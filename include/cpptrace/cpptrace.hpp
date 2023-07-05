#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace cpptrace {
    struct stacktrace_frame {
        uintptr_t address = 0;
        int line = 0;
        int col = 0;
        std::string filename;
        std::string symbol;
    };
    std::vector<stacktrace_frame> generate_trace();
    void print_trace();
}

#endif
