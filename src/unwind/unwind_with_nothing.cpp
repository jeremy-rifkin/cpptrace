#ifdef LIBCPPTRACE_UNWIND_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "libcpp_unwind.hpp"

#include <vector>

namespace cpptrace {
    namespace detail {
        std::vector<void*> capture_frames() {
            return {};
        }
    }
}

#endif
