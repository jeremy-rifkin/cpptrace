#ifndef UNWIND_HPP
#define UNWIND_HPP

#include "../platform/common.hpp"
#include "../platform/utils.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
namespace detail {
    #ifdef CPPTRACE_HARD_MAX_FRAMES
    constexpr std::size_t hard_max_frames = CPPTRACE_HARD_MAX_FRAMES;
    #else
    constexpr std::size_t hard_max_frames = 100;
    #endif
    CPPTRACE_FORCE_NO_INLINE
    std::vector<std::uintptr_t> capture_frames(std::size_t skip, std::size_t max_depth);
}
}

#endif
