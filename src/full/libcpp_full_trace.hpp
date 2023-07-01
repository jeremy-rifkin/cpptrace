#ifndef LIBCPP_FULL_TRACE_HPP
#define LIBCPP_FULL_TRACE_HPP

#include <cpptrace/cpptrace.hpp>

#include <vector>

namespace cpptrace {
    namespace detail {
        std::vector<stacktrace_frame> generate_trace();
    }
}

#endif
