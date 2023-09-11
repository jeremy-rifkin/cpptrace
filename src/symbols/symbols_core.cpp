#include "symbols.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        void apply_trace(
            std::vector<stacktrace_frame>& result,
            std::vector<stacktrace_frame>&& trace
        ) {
            for(std::size_t i = 0; i < result.size(); i++) {
                if(result[i].address == 0) {
                    result[i].address = trace[i].address;
                }
                if(result[i].line == 0) {
                    result[i].line = trace[i].line;
                }
                if(result[i].col == 0) {
                    result[i].col = trace[i].col;
                }
                if(result[i].filename.empty()) {
                    result[i].filename = std::move(trace[i].filename);
                }
                if(result[i].symbol.empty()) {
                    result[i].symbol = std::move(trace[i].symbol);
                }
            }
        }

        std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames) {
            std::vector<stacktrace_frame> trace(frames.size());
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL
            apply_trace(trace, libdl::resolve_frames(frames));
            #endif
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
            apply_trace(trace, libdwarf::resolve_frames(frames));
            #endif
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
            apply_trace(trace, dbghelp::resolve_frames(frames));
            #endif
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
            apply_trace(trace, addr2line::resolve_frames(frames));
            #endif
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE
            apply_trace(trace, libbacktrace::resolve_frames(frames));
            #endif
            #ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING
            apply_trace(trace, nothing::resolve_frames(frames));
            #endif
            return trace;
        }
    }
}
