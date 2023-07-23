#ifdef CPPTRACE_UNWIND_WITH_EXECINFO

#include "unwind.hpp"
#include "../platform/common.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <execinfo.h>

namespace cpptrace {
    namespace detail {
        CPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip) {
            std::vector<void*> frames(hard_max_frames + skip, nullptr);
            const int n_frames = backtrace(frames.data(), int(hard_max_frames + skip)); // thread safe
            frames.resize(n_frames);
            frames.erase(frames.begin(), frames.begin() + ptrdiff_t(std::min(skip + 1, frames.size())));
            frames.shrink_to_fit();
            return frames;
        }
    }
}

#endif
