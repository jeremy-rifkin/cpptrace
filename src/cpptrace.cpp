#include <cpptrace/cpptrace.hpp>

#include <cstddef>
#include <string>
#include <vector>
#include <iostream>

#if !(defined(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE) || defined(CPPTRACE_FULL_TRACE_WITH_STACKTRACE))

#include "symbols/cpptrace_symbols.hpp"
#include "unwind/cpptrace_unwind.hpp"
#include "demangle/cpptrace_demangle.hpp"
#include "platform/cpptrace_common.hpp"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE
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

#include "full/cpptrace_full_trace.hpp"
#include "demangle/cpptrace_demangle.hpp"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<stacktrace_frame> generate_trace(std::uint32_t skip) {
        auto trace = detail::generate_trace(skip + 1);
        for(auto& entry : trace) {
            entry.symbol = detail::demangle(entry.symbol);
        }
        return trace;
    }
}

#endif

namespace cpptrace {
    void print_trace(std::uint32_t skip) {
        std::cerr<<"Stack trace (most recent call first):"<<std::endl;
        std::size_t counter = 0;
        const auto trace = generate_trace(skip + 1);
        // +1 to skip one frame
        for(auto it = trace.begin() + 1; it != trace.end(); it++) {
            const auto& frame = *it;
            std::cerr
                << counter++
                << " "
                << frame.filename
                << ":"
                << frame.line
                << (frame.col > 0 ? ":" + std::to_string(frame.col) : "")
                << " "
                << frame.symbol
                << std::endl;
        }
    }
}
