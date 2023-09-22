#ifdef CPPTRACE_UNWIND_WITH_UNWIND

#include "unwind.hpp"
#include "../platform/common.hpp"
#include "../platform/error.hpp"
#include "../platform/utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

#include <unwind.h>

namespace cpptrace {
namespace detail {
    struct unwind_state {
        std::size_t skip;
        std::size_t count;
        std::vector<uintptr_t>& vec;
    };

    _Unwind_Reason_Code unwind_callback(_Unwind_Context* context, void* arg) {
        unwind_state& state = *static_cast<unwind_state*>(arg);
        if(state.skip) {
            state.skip--;
            if(_Unwind_GetIP(context) == uintptr_t(0)) {
                return _URC_END_OF_STACK;
            } else {
                return _URC_NO_REASON;
            }
        }

        VERIFY(
            state.count < state.vec.size(),
            "Somehow cpptrace::detail::unwind_callback is overflowing a vector"
        );
        int is_before_instruction = 0;
        uintptr_t ip = _Unwind_GetIPInfo(context, &is_before_instruction);
        if(!is_before_instruction && ip != uintptr_t(0)) {
            ip--;
        }
        if (ip == uintptr_t(0)) {
            return _URC_END_OF_STACK;
        } else {
            // TODO: push_back?...
            state.vec[state.count++] = ip;
            if(state.count == state.vec.size()) {
                return _URC_END_OF_STACK;
            } else {
                return _URC_NO_REASON;
            }
        }
    }

    CPPTRACE_FORCE_NO_INLINE
    std::vector<uintptr_t> capture_frames(size_t skip, size_t max_depth) {
        std::vector<uintptr_t> frames(std::min(hard_max_frames, max_depth), 0);
        unwind_state state{skip + 1, 0, frames};
        _Unwind_Backtrace(unwind_callback, &state); // presumably thread-safe
        frames.resize(state.count);
        frames.shrink_to_fit();
        return frames;
    }
}
}

#endif
