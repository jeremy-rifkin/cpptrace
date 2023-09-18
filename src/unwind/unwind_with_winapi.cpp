#ifdef CPPTRACE_UNWIND_WITH_WINAPI

#include <cpptrace/cpptrace.hpp>
#include "unwind.hpp"
#include "../platform/common.hpp"
#include "../platform/utils.hpp"

#include <cstdint>
#include <vector>

#include <windows.h>

namespace cpptrace {
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<uintptr_t> capture_frames(size_t skip) {
        std::vector<void*> addrs(hard_max_frames, nullptr);
        int frames = CaptureStackBackTrace(static_cast<DWORD>(skip + 1), hard_max_frames, addrs.data(), NULL);
        addrs.resize(frames);
        std::vector<uintptr_t> frames(addrs.size(), 0);
        for(std::size_t i = 0; i < addrs.size(); i++) {
            frames[i] = reinterpret_cast<uintptr_t>(addrs[i]);
        }
        return frames;
    }
}
}

#endif
