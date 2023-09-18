#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

#include "../platform/object.hpp"

namespace cpptrace {
namespace detail {
    using collated_vec = std::vector<
        std::pair<std::reference_wrapper<const object_frame>, std::reference_wrapper<stacktrace_frame>>
    >;

    std::unordered_map<std::string, collated_vec> collate_frames(
        const std::vector<object_frame>& frames,
        std::vector<stacktrace_frame>& trace
    );

    #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE
    namespace libbacktrace {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames);
    }
    #endif
    #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
    namespace libdwarf {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames);
    }
    #endif
    #ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL
    namespace libdl {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames);
    }
    #endif
    #ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
    namespace addr2line {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames);
    }
    #endif
    #ifdef CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
    namespace dbghelp {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames);
    }
    #endif
    #ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING
    namespace nothing {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames);
        std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames);
    }
    #endif

    std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames);
    std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames);
}
}

#endif
