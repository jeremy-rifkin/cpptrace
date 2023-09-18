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
    std::vector<uintptr_t> capture_frames(size_t skip) {
        std::vector<void*> addrs(hard_max_frames + skip, nullptr);
        const int n_frames = backtrace(addrs.data(), int(hard_max_frames + skip)); // thread safe
        addrs.resize(n_frames);
        addrs.erase(addrs.begin(), addrs.begin() + ptrdiff_t(std::min(skip + 1, addrs.size())));
        addrs.shrink_to_fit();
        std::vector<uintptr_t> frames(addrs.size(), 0);
        for(std::size_t i = 0; i < addrs.size(); i++) {
            frames[i] = reinterpret_cast<uintptr_t>(addrs[i]);
        }
        return frames;
    }
}
}

#endif
