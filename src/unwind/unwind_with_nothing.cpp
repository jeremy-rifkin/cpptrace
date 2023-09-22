#ifdef CPPTRACE_UNWIND_WITH_NOTHING

#include "unwind.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
namespace detail {
    std::vector<uintptr_t> capture_frames(size_t, size_t) {
        return {};
    }
}
}

#endif
