#ifdef LIBCPPTRACE_UNWIND_WITH_EXECINFO

#include <cpptrace/cpptrace.hpp>
#include "libcpp_unwind.hpp"

#include <vector>

#include <execinfo.h>

namespace cpptrace {
    namespace detail {
        std::vector<void*> capture_frames() {
            std::vector<void*> frames(hard_max_frames, nullptr);
            int n_frames = backtrace(bt.data(), hard_max_frames);
            frames.resize(n_frames);
            frames.shrink_to_fit();
            return frames;
        }
    }
}

#endif
