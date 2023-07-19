#ifndef CPPTRACE_SYMBOLS_HPP
#define CPPTRACE_SYMBOLS_HPP

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
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        };
    }
}

#endif
