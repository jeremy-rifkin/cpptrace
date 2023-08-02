#include <cpptrace/cpptrace.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>

#if !(defined(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE) || defined(CPPTRACE_FULL_TRACE_WITH_STACKTRACE))

#include "symbols/symbols.hpp"
#include "unwind/unwind.hpp"
#include "demangle/demangle.hpp"
#include "platform/common.hpp"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    std::vector<stacktrace_frame> generate_trace(std::uint32_t skip) {
        std::vector<void*> frames = detail::capture_frames(skip + 1);
        detail::symbolizer symbolizer;
        std::vector<stacktrace_frame> trace = symbolizer.resolve_frames(frames);
        for(auto& frame : trace) {
            frame.symbol = detail::demangle(frame.symbol);
        }
        return trace;
    }
}

#else

// full trace

#include "full/full_trace.hpp"
#include "demangle/demangle.hpp"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    std::vector<stacktrace_frame> generate_trace(std::uint32_t skip) {
        auto trace = detail::generate_trace(skip + 1);
        for(auto& entry : trace) {
            entry.symbol = detail::demangle(entry.symbol);
        }
        return trace;
    }
}

#endif

#define ESC     "\033["
#define RESET   ESC "0m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define BLUE    ESC "34m"
#define MAGENTA ESC "35m"
#define CYAN    ESC "36m"

namespace cpptrace {
    CPPTRACE_API
    void print_trace(std::uint32_t skip) {
        enable_virtual_terminal_processing_if_needed();
        std::cerr<<"Stack trace (most recent call first):"<<std::endl;
        std::size_t counter = 0;
        const auto trace = generate_trace(skip + 1);
        if(trace.empty()) {
            std::cerr<<"<empty trace>"<<std::endl;
            return;
        }
        const auto frame_number_width = n_digits(static_cast<int>(trace.size()) - 1);
        for(const auto& frame : trace) {
            std::cerr
                << '#'
                << std::setw(static_cast<int>(frame_number_width))
                << std::left
                << counter++
                << std::right
                << " "
                << std::hex
                << BLUE
                << "0x"
                << std::setw(2 * sizeof(uintptr_t))
                << std::setfill('0')
                << frame.address
                << std::dec
                << std::setfill(' ')
                << RESET
                << " in "
                << YELLOW
                << frame.symbol
                << RESET
                << " at "
                << GREEN
                << frame.filename
                << RESET
                << ":"
                << BLUE
                << frame.line
                << RESET
                << (frame.col > 0 ? ":" BLUE + std::to_string(frame.col) + RESET : "")
                << std::endl;
        }
    }
}
