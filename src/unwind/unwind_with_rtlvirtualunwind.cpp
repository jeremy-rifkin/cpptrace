#ifdef CPPTRACE_UNWIND_WITH_RTLVIRTUALUNWIND

#include <cpptrace/basic.hpp>
#include "unwind/unwind.hpp"
#include "utils/common.hpp"
#include "utils/utils.hpp"

#include <cstddef>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#if !defined(_M_X64) && !defined(__x86_64__) && !defined(_M_ARM64) && !defined(__aarch64__)
 #error "Cpptrace: RtlVirtualUnwind is only supported on 64-bit Windows"
#endif

namespace {
DWORD64 context_pc(CONTEXT& context) {
    #if defined(_M_X64) || defined(__x86_64__)
     return context.Rip;
    #elif defined(_M_ARM64) || defined(__aarch64__)
     return context.Pc;
    #endif
}
}

CPPTRACE_BEGIN_NAMESPACE
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<frame_ptr> capture_frames(
        std::size_t skip,
        std::size_t max_depth,
        EXCEPTION_POINTERS* exception_pointers
    ) {
        CONTEXT context;
        ZeroMemory(&context, sizeof(CONTEXT));
        if(exception_pointers) {
            context = *exception_pointers->ContextRecord;
        } else {
            skip++; // we're unwinding from the capture_frames frame, skip it
            RtlCaptureContext(&context);
        }

        std::vector<frame_ptr> trace;
        bool handled_leaf = false;
        while(trace.size() < max_depth) {
            DWORD64 image_base;
            auto pc = context_pc(context);
            PRUNTIME_FUNCTION function_entry = RtlLookupFunctionEntry(pc, &image_base, NULL);
            if(!function_entry) {
                if(handled_leaf) {
                    break;
                }
                handled_leaf = true;
            }
            if(skip) {
                skip--;
            } else {
                trace.push_back(to_frame_ptr(pc) - 1);
            }
            if(function_entry) {
                PVOID handler_data;
                DWORD64 establisher_frame;
                RtlVirtualUnwind(
                    UNW_FLAG_NHANDLER,
                    image_base,
                    pc,
                    function_entry,
                    &context,
                    &handler_data,
                    &establisher_frame,
                    NULL
                );
            } else {
                // Leaf functions may not have unwind / .pdata entries. Handle that case here.
                // We only come here once and handled_leaf = true is set above
                // https://learn.microsoft.com/en-us/cpp/build/arm-exception-handling?view=msvc-170
                // https://searchfox.org/firefox-main/source/mozglue/misc/StackWalk.cpp#653-655
                // https://github.com/dotnet/runtime/blob/133c7bde3fb7dd914f423ffb3e408cc61787e0bb/src/coreclr/vm/stackwalk.cpp#L573-L600
                #if defined(_M_X64) || defined(__x86_64__)
                 context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
                 context.Rsp += sizeof(DWORD64);
                #elif defined(_M_ARM64) || defined(__aarch64__)
                 context.Pc = context.Lr;
                #else
                 #error "Unsupported arch for leaf function handling"
                #endif
            }
            if(context_pc(context) == 0) {
                break;
            }
        }
        return trace;
    }

    CPPTRACE_FORCE_NO_INLINE
    std::size_t safe_capture_frames(frame_ptr*, std::size_t, std::size_t, std::size_t) {
        // Can't safe trace with RtlVirtualUnwind
        return 0;
    }

    bool has_safe_unwind() {
        return false;
    }
}
CPPTRACE_END_NAMESPACE

#endif
