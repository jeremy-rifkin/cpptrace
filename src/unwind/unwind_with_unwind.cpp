#ifdef CPPTRACE_UNWIND_WITH_UNWIND

#include "cpptrace_unwind.hpp"
#include "../platform/cpptrace_common.hpp"

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
            std::vector<void*>& vec;
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

            assert(state.count < state.vec.size());
            //void* ip = reinterpret_cast<void*>(_Unwind_GetIP(context));
            int is_before_instruction = 0;
            uintptr_t ip = _Unwind_GetIPInfo(context, &is_before_instruction);
            if(!is_before_instruction && ip != uintptr_t(0)) {
                ip--;
            }
            if (ip == uintptr_t(0) || state.count == state.vec.size()) {
                return _URC_END_OF_STACK;
            } else {
                state.vec[state.count++] = (void*)ip;
                return _URC_NO_REASON;
            }
        }

        CPPTRACE_FORCE_NO_INLINE
        std::vector<void*> capture_frames(size_t skip) {
            std::vector<void*> frames(hard_max_frames, nullptr);
            unwind_state state{skip + 1, 0, frames};
            _Unwind_Backtrace(unwind_callback, &state);
            frames.resize(state.count);
            frames.shrink_to_fit();
            return frames;
        }
    }
}

#endif
