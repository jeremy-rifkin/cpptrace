#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

namespace cpptrace {
    namespace detail {
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE
        namespace libbacktrace {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
        namespace libdwarf {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL
        namespace libdl {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
        namespace addr2line {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
        namespace dbghelp {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        #ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING
        namespace nothing {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        }
        #endif
        std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
    }
}

#endif
