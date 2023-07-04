#ifndef CPPTRACE_FULL_TRACE_HPP
#define CPPTRACE_FULL_TRACE_HPP

#include <cpptrace/cpptrace.hpp>
#include "../platform/cpptrace_common.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
    namespace detail {
        CPPTRACE_FORCE_NO_INLINE
        std::vector<stacktrace_frame> generate_trace(size_t skip);
    }
}

#endif
