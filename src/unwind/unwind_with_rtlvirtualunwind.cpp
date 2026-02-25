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
        while(trace.size() < max_depth) {
            DWORD64 image_base;
            PRUNTIME_FUNCTION function_entry = RtlLookupFunctionEntry(context_pc(context), &image_base, NULL);
            if(!function_entry) {
                break;
            }
            if(skip) {
                skip--;
            } else {
                // Same adjustment as StackWalk64
                trace.push_back(to_frame_ptr(context_pc(context)) - 1);
            }
            #if defined(_M_X64) || defined(__x86_64__)
            PVOID handler_data;
            DWORD64 establisher_frame;
            RtlVirtualUnwind(
                UNW_FLAG_NHANDLER,
                image_base,
                context_pc(context),
                function_entry,
                &context,
                &handler_data,
                &establisher_frame,
                NULL
            );
            #elif defined(_M_ARM64) || defined(__aarch64__)
            BOOLEAN in_function;
            FRAME_POINTERS establisher_frame;
            RtlVirtualUnwind(
                image_base,
                context_pc(context),
                function_entry,
                &context,
                &in_function,
                &establisher_frame,
                NULL
            );
            #endif
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
