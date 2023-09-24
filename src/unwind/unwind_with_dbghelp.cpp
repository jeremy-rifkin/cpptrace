#ifdef CPPTRACE_UNWIND_WITH_DBGHELP

#include <cpptrace/cpptrace.hpp>
#include "unwind.hpp"
#include "../platform/common.hpp"
#include "../platform/utils.hpp"
#include "../platform/dbghelp_syminit_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>
#include <mutex>

#include <windows.h>
#include <dbghelp.h>

// Fucking windows headers
#ifdef min
 #undef min
#endif

namespace cpptrace {
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    std::vector<uintptr_t> capture_frames(size_t skip, size_t max_depth) {
        skip++;
        // https://jpassing.com/2008/03/12/walking-the-stack-of-the-current-thread/

        // Get current thread context
        // GetThreadContext cannot be used on the current thread.
        // RtlCaptureContext doesn't work on i386
        CONTEXT context;
        #ifdef _M_IX86
        ZeroMemory(&context, sizeof(CONTEXT));
        context.ContextFlags = CONTEXT_CONTROL;
        __asm {
            label:
            mov [context.Ebp], ebp;
            mov [context.Esp], esp;
            mov eax, [label];
            mov [context.Eip], eax;
        }
        #else
        RtlCaptureContext(&context);
        #endif
        // Setup current frame
        STACKFRAME64 frame;
        ZeroMemory(&frame, sizeof(STACKFRAME64));
        DWORD machine_type;
        #ifdef _M_IX86
        machine_type           = IMAGE_FILE_MACHINE_I386;
        frame.AddrPC.Offset    = context.Eip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = context.Esp;
        frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_X64
        machine_type           = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset    = context.Rip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rsp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_IA64
        machine_type           = IMAGE_FILE_MACHINE_IA64;
        frame.AddrPC.Offset    = context.StIIP;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = context.IntSp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrBStore.Offset= context.RsBSP;
        frame.AddrBStore.Mode  = AddrModeFlat;
        frame.AddrStack.Offset = context.IntSp;
        frame.AddrStack.Mode   = AddrModeFlat;
        #else
        #error "Cpptrace: StackWalk64 not supported for this platform yet"
        #endif

        std::vector<uintptr_t> trace;

        // Dbghelp is is single-threaded, so acquire a lock.
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        // For some reason SymInitialize must be called before StackWalk64
        // Note that the code assumes that
        // SymInitialize( GetCurrentProcess(), NULL, TRUE ) has
        // already been called.
        //
        HANDLE proc = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        get_syminit_manager().init(proc);
        while(trace.size() < max_depth) {
            if(
                !StackWalk64(
                    machine_type,
                    proc,
                    thread,
                    &frame,
                    machine_type == IMAGE_FILE_MACHINE_I386 ? NULL : &context,
                    NULL,
                    SymFunctionTableAccess64,
                    SymGetModuleBase64,
                    NULL
                )
            ) {
                // Either failed or finished walking
                break;
            }
            if(frame.AddrPC.Offset != 0) {
                // Valid frame
                if(skip) {
                    skip--;
                } else {
                    trace.push_back(frame.AddrPC.Offset);
                }
            } else {
                // base
                break;
            }
        }
        return trace;
    }
}
}

#endif
