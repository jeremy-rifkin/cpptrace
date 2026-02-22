#ifdef CPPTRACE_UNWIND_WITH_LIBUNWIND

#include "unwind/unwind.hpp"
#include "utils/common.hpp"
#include "utils/error.hpp"
#include "utils/utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

#include <libunwind.h>

#if IS_APPLE && (defined(__arm64__) || defined(__aarch64__))
 #include <ptrauth.h>
#endif

namespace {
// Strip pointer authentication code from an instruction address.
// Apple's unw_get_reg(UNW_REG_IP) may return PAC-signed addresses with signature bits in
// the upper bytes.
inline uintptr_t depaci(uintptr_t pc) {
    #if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
     return reinterpret_cast<uintptr_t>(
         ptrauth_strip(reinterpret_cast<void*>(pc), ptrauth_key_asia)
     );
    #else
     return pc;
    #endif
}
}

CPPTRACE_BEGIN_NAMESPACE
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
            pc = depaci(pc);
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

    CPPTRACE_FORCE_NO_INLINE
    std::size_t safe_capture_frames(frame_ptr* buffer, std::size_t size, std::size_t skip, std::size_t max_depth) {
        // some code duplication, but whatever
        skip++;
        unw_context_t context;
        unw_cursor_t cursor;
        // thread and signal-safe https://www.nongnu.org/libunwind/man/unw_getcontext(3).html
        unw_getcontext(&context);
        // thread and signal-safe https://www.nongnu.org/libunwind/man/unw_init_local(3).html
        unw_init_local(&cursor, &context);
        size_t i = 0;
        while(i < size && i < max_depth) {
            unw_word_t pc;
            unw_word_t sp;
            // thread and signal-safe https://www.nongnu.org/libunwind/man/unw_get_reg(3).html
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            pc = depaci(pc);
            unw_get_reg(&cursor, UNW_REG_SP, &sp);
            if(skip) {
                skip--;
            } else {
                // thread and signal-safe
                if(unw_is_signal_frame(&cursor)) {
                    // pc is the instruction that caused the signal
                    // just a cast, thread and signal safe
                    buffer[i] = to_frame_ptr(pc);
                } else {
                    // pc is the instruction after the `call`, adjust back to the previous instruction
                    // just a cast, thread and signal safe
                    buffer[i] = to_frame_ptr(pc) - 1;
                }
                i++;
            }
            // thread and signal-safe as long as the cursor is in the local address space, which it is
            // https://www.nongnu.org/libunwind/man/unw_step(3).html
            if(unw_step(&cursor) <= 0) {
                break;
            }
        }
        return i;
    }

    bool has_safe_unwind() {
        return true;
    }
}
CPPTRACE_END_NAMESPACE

#endif
