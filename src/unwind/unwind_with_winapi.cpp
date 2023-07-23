#ifdef CPPTRACE_UNWIND_WITH_WINAPI

#include <cpptrace/cpptrace.hpp>
#include "unwind.hpp"
#include "../platform/common.hpp"

#include <vector>

#include <windows.h>

namespace cpptrace {
    namespace detail {
        CPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip) {
            std::vector<PVOID> addrs(hard_max_frames, nullptr);
            int frames = CaptureStackBackTrace(static_cast<DWORD>(skip + 1), hard_max_frames, addrs.data(), NULL);
            addrs.resize(frames);
            addrs.shrink_to_fit();
            return addrs;
        }
    }
}

#endif
