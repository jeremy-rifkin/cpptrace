#ifdef CPPTRACE_UNWIND_WITH_NOTHING

#include "unwind.hpp"

#include <cstddef>
#include <vector>

namespace cpptrace {
namespace detail {
    std::vector<frame_ptr> capture_frames(std::size_t, std::size_t) {
        return {};
    }
}
}

#endif
