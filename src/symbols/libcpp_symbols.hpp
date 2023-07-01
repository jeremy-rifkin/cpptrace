#ifndef LIBCPP_SYMBOLIZE_HPP
#define LIBCPP_SYMBOLIZE_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

namespace cpptrace {
    namespace detail {
        class symbolizer {
            struct impl;
            std::unique_ptr<impl> impl;
        public:
            symbolizer();
            ~symbolizer();
            stacktrace_frame resolve_frame(void* addr);
        };
    }
}

#endif
