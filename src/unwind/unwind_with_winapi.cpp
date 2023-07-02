#ifdef LIBCPPTRACE_UNWIND_WITH_WINAPI

#include <cpptrace/cpptrace.hpp>
#include "libcpp_unwind.hpp"

#include <vector>

#include <windows.h>

namespace cpptrace {
    namespace detail {
        std::vector<void*> capture_frames() {
            std::vector<PVOID> addrs(hard_max_frames, nullptr);
            int frames = CaptureStackBackTrace(0, hard_max_frames, addrs.data(), NULL);
            addrs.resize(frames);
            addrs.shrink_to_fit();
            return addrs;
        }
    }
}

#endif
