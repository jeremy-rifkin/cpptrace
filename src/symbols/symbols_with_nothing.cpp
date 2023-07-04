#ifdef CPPTRACE_GET_SYMBOLS_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_symbols.hpp"
#include "../platform/cpptrace_program_name.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        symbolizer::symbolizer() = default;
        symbolizer::~symbolizer() = default;

        stacktrace_frame symbolizer::resolve_frame(void*) {
            return {
                0,
                -1,
                -1,
                "",
                "",
            };
        }

        struct symbolizer::impl {};
    }
}

#endif
