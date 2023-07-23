#ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        symbolizer::symbolizer() = default;
        symbolizer::~symbolizer() = default;

        // stacktrace_frame symbolizer::resolve_frame(void*) {
        //     return {
        //         0,
        //         0,
        //         0,
        //         "",
        //         "",
        //     };
        // }

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            return std::vector<stacktrace_frame>(frames.size(), {
                0,
                0,
                0,
                "",
                ""
            });
        }

        struct symbolizer::impl {};
    }
}

#endif
