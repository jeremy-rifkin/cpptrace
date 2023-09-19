#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include <dlfcn.h>

namespace cpptrace {
namespace detail {
namespace libdl {
    stacktrace_frame resolve_frame(const uintptr_t addr) {
        Dl_info info;
        if(dladdr(reinterpret_cast<void*>(addr), &info)) { // thread-safe
            return {
                addr,
                0,
                UINT_LEAST32_MAX,
                info.dli_fname ? info.dli_fname : "",
                info.dli_sname ? info.dli_sname : ""
            };
        } else {
            return {
                addr,
                0,
                UINT_LEAST32_MAX,
                "",
                ""
            };
        }
    }

    std::vector<stacktrace_frame> resolve_frames(const std::vector<uintptr_t>& frames) {
        std::vector<stacktrace_frame> trace;
        trace.reserve(frames.size());
        for(const auto frame : frames) {
            trace.push_back(resolve_frame(frame));
        }
        return trace;
    }
}
}
}

#endif
