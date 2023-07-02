#include <cpptrace/cpptrace.hpp>

#include <cstddef>
#include <string>
#include <vector>
#include <iostream>

#ifndef LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE

#include "symbols/libcpp_symbols.hpp"
#include "unwind/libcpp_unwind.hpp"
#include "demangle/libcpp_demangle.hpp"
#include "platform/libcpp_common.hpp"

namespace cpptrace {
    LIBCPPTRACE_FORCE_NO_INLINE
    std::vector<stacktrace_frame> generate_trace() {
        std::vector<void*> frames = detail::capture_frames(1);
        std::vector<stacktrace_frame> trace;
        detail::symbolizer symbolizer;
        for(const auto frame : frames) {
            auto entry = symbolizer.resolve_frame(frame);
            entry.symbol = detail::demangle(entry.symbol);
            trace.push_back(entry);
        }
        return trace;
    }
}

#else

// full trace

#include "full/libcpp_full_trace.hpp"
#include "demangle/libcpp_demangle.hpp"

namespace cpptrace {
    LIBCPPTRACE_FORCE_NO_INLINE
    std::vector<stacktrace_frame> generate_trace() {
        auto trace = detail::generate_trace(1);
        for(auto& entry : trace) {
            entry.symbol = detail::demangle(entry.symbol);
        }
        return trace;
    }
}

#endif

namespace cpptrace {
    void print_trace() {
        std::cerr<<"Stack trace (most recent call first):"<<std::endl;
        std::size_t i = 0;
        const auto trace = generate_trace();
        // +1 to skip one frame
        for(auto it = trace.begin() + 1; it != trace.end(); it++) {
            const auto& frame = *it;
            std::cerr
                << i++
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
