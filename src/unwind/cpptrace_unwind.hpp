#ifndef CPPTRACE_UNWIND_HPP
#define CPPTRACE_UNWIND_HPP

#include "../platform/cpptrace_common.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
    namespace detail {
        #ifdef CPPTRACE_HARD_MAX_FRAMES
        constexpr size_t hard_max_frames = CPPTRACE_HARD_MAX_FRAMES;
        #else
        constexpr size_t hard_max_frames = 100;
        #endif
        CPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip);
    }
}

#endif
