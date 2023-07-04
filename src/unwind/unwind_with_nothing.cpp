#ifdef CPPTRACE_UNWIND_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_unwind.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        std::vector<void*> capture_frames(size_t) {
            return {};
        }
    }
}

#endif
