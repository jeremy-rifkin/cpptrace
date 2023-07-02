#ifndef LIBCPP_FULL_TRACE_HPP
#define LIBCPP_FULL_TRACE_HPP

#include <cpptrace/cpptrace.hpp>
#include "../platform/libcpp_common.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
    namespace detail {
        LIBCPPTRACE_FORCE_NO_INLINE
        std::vector<stacktrace_frame> generate_trace(size_t skip);
    }
}

#endif
