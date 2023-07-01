#ifdef LIBCPPTRACE_UNWIND_WITH_WINAPI

#include <cpptrace/cpptrace.hpp>
#include "libcpp_unwind.hpp"

#include <vector>

#include <windows.h>

namespace cpptrace {
    namespace detail {
        std::vector<void*> capture_frames() {
            // TODO: When does this need to be called? Can it be moved to the symbolizer?
            //SymSetOptions(SYMOPT_ALLOW_ABSOLUTE_SYMBOLS);
            //HANDLE proc = GetCurrentProcess();
            //if(!SymInitialize(proc, NULL, TRUE)) return {};
            std::vector<PVOID> addrs(hard_max_frames, nullptr);
            int frames = CaptureStackBackTrace(0, hard_max_frames, addrs.data(), NULL);
            addrs.resize(frames);
            addrs.shrink_to_fit();
            return addrs;
        }
    }
}

#endif
