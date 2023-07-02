#ifdef LIBCPPTRACE_UNWIND_WITH_EXECINFO

#include <cpptrace/cpptrace.hpp>
#include "libcpp_unwind.hpp"
#include "../platform/libcpp_common.hpp"

#include <algorithm>
#include <vector>

#include <execinfo.h>

namespace cpptrace {
    namespace detail {
        LIBCPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip) {
            std::vector<void*> frames(hard_max_frames + skip, nullptr);
            int n_frames = backtrace(frames.data(), hard_max_frames + skip);
            frames.resize(n_frames);
            frames.erase(frames.begin(), frames.begin() + std::min(skip + 1, frames.size()));
            frames.shrink_to_fit();
            return frames;
        }
    }
}

#endif
