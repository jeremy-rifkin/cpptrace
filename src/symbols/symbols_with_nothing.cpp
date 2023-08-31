#ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames) {
            return std::vector<stacktrace_frame>(frames.size(), {
                0,
                0,
                0,
                "",
                ""
            });
        }
    }
}

#endif
