#ifdef CPPTRACE_FULL_TRACE_WITH_STACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "full_trace.hpp"
#include "../platform/common.hpp"

#include <vector>
#include <stacktrace>

namespace cpptrace {
    namespace detail {
        CPPTRACE_FORCE_NO_INLINE
        std::vector<stacktrace_frame> generate_trace(size_t skip) {
            std::vector<stacktrace_frame> frames;
            std::stacktrace trace = std::stacktrace::current(skip + 1);
            for(const auto entry : trace) {
                frames.push_back({
                    entry.native_handle(),
                    entry.source_line(),
                    0,
                    entry.source_file(),
                    entry.description()
                });
            }
            return frames;
        }
    }
}

#endif
