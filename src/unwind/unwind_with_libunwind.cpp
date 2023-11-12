#ifdef CPPTRACE_UNWIND_WITH_LIBUNWIND

#include "unwind.hpp"
#include "../platform/common.hpp"
#include "../platform/error.hpp"
#include "../platform/utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

#include <libunwind.h>

namespace cpptrace {
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<frame_ptr> capture_frames(std::size_t skip, std::size_t max_depth) {
        skip++;
        std::vector<frame_ptr> frames;
        unw_context_t context;
        unw_cursor_t cursor;
        unw_getcontext(&context);
        unw_init_local(&cursor, &context);
        do {
            unw_word_t pc;
            unw_word_t sp;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            unw_get_reg(&cursor, UNW_REG_SP, &sp);
            if(skip) {
                skip--;
            } else {
                // pc is the instruction after the `call`, adjust back to the previous instruction
                frames.push_back(to_frame_ptr(pc) - 1);
            }
        } while(unw_step(&cursor) > 0 && frames.size() < max_depth);
        return frames;
    }
}
}

#endif
