#ifndef LIBCPP_SYMBOLS_HPP
#define LIBCPP_SYMBOLS_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

namespace cpptrace {
    namespace detail {
        class symbolizer {
            struct impl;
            std::unique_ptr<impl> pimpl;
        public:
            symbolizer();
            ~symbolizer();
            stacktrace_frame resolve_frame(void* addr);
        };
    }
}

#endif
