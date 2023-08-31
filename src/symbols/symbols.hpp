#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <cpptrace/cpptrace.hpp>

#include <memory>
#include <vector>

namespace cpptrace {
    namespace detail {
        std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames);
    }
}

#endif
