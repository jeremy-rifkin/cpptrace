#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

namespace cpptrace {
    namespace detail {
        // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
        class symbolizer {
            struct impl;
            std::unique_ptr<impl> pimpl;
        public:
            symbolizer();
            ~symbolizer();
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
        };
    }
}

#endif
