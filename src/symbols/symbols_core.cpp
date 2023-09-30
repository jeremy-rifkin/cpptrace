#include "symbols.hpp"

#include <vector>
#include <unordered_map>

#include "../platform/common.hpp"
#include "../platform/object.hpp"

namespace cpptrace {
namespace detail {
    std::unordered_map<std::string, collated_vec> collate_frames(
        const std::vector<object_frame>& frames,
        std::vector<stacktrace_frame>& trace
    ) {
        std::unordered_map<std::string, collated_vec> entries;
        for(std::size_t i = 0; i < frames.size(); i++) {
            const auto& entry = frames[i];
            // If libdl fails to find the shared object for a frame, the path will be empty. I've observed this
            // on macos when looking up the shared object containing `start`.
            if(!entry.obj_path.empty()) {
                entries[entry.obj_path].emplace_back(
                    entry,
                    trace[i]
                );
            }
        }
        return entries;
    }

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
            if(result[i].column == UINT_LEAST32_MAX) {
                result[i].column = trace[i].column;
            }
            if(result[i].filename.empty()) {
                result[i].filename = std::move(trace[i].filename);
            }
            if(result[i].symbol.empty()) {
                result[i].symbol = std::move(trace[i].symbol);
            }
        }
    }

    std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames) {
        std::vector<stacktrace_frame> trace(frames.size(), null_frame);
        #if defined(CPPTRACE_GET_SYMBOLS_WITH_LIBDL) \
            || defined(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP) \
            || defined(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
         // actually need to go backwards to a void*
         std::vector<uintptr_t> raw_frames(frames.size());
         for(std::size_t i = 0; i < frames.size(); i++) {
             raw_frames[i] = frames[i].raw_address;
         }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL
         apply_trace(trace, libdl::resolve_frames(raw_frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
         apply_trace(trace, libdwarf::resolve_frames(frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
         apply_trace(trace, dbghelp::resolve_frames(raw_frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
         apply_trace(trace, addr2line::resolve_frames(frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE
         apply_trace(trace, libbacktrace::resolve_frames(raw_frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING
         apply_trace(trace, nothing::resolve_frames(frames));
        #endif
        return trace;
    }

    std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames) {
        #if defined(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF) \
            || defined(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
         auto dlframes = get_frames_object_info(frames);
        #endif
        std::vector<stacktrace_frame> trace(frames.size(), null_frame);
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL
         apply_trace(trace, libdl::resolve_frames(frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
         apply_trace(trace, libdwarf::resolve_frames(dlframes));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
         apply_trace(trace, dbghelp::resolve_frames(frames));
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
         apply_trace(trace, addr2line::resolve_frames(dlframes));
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
