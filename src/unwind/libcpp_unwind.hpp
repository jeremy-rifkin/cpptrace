#ifndef LIBCPP_UNWIND_HPP
#define LIBCPP_UNWIND_HPP

#include <cpptrace/cpptrace.hpp>
#include "../platform/libcpp_common.hpp"

#include <cstddef>

namespace cpptrace {
    namespace detail {
        #ifdef LIBCPPTRACE_HARD_MAX_FRAMES
        constexpr size_t hard_max_frames = LIBCPPTRACE_HARD_MAX_FRAMES;
        #else
        constexpr size_t hard_max_frames = 100;
        #endif
        LIBCPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip);
    }
}

#endif
