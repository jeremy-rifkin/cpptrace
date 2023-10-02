#ifdef CPPTRACE_UNWIND_WITH_EXECINFO

#include "unwind.hpp"
#include "../platform/common.hpp"
#include "../platform/utils.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <execinfo.h>

namespace cpptrace {
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<uintptr_t> capture_frames(size_t skip, size_t max_depth) {
        skip++;
        std::vector<void*> addrs(std::min(hard_max_frames, skip + max_depth), nullptr);
        const int n_frames = backtrace(addrs.data(), static_cast<int>(addrs.size())); // thread safe
        // I hate the copy here but it's the only way that isn't UB
        std::vector<uintptr_t> frames(n_frames - skip, 0);
        for(int i = skip; i < n_frames; i++) {
            // On x86/x64/arm, as far as I can tell, the frame return address is always one after the call
            // So we just decrement to get the pc back inside the `call` / `bl`
            // This is done with _Unwind too but conditionally based on info from _Unwind_GetIPInfo.
            frames[i - skip] = reinterpret_cast<uintptr_t>(addrs[i]) - 1;
        }
        return frames;
    }
}
}

#endif
