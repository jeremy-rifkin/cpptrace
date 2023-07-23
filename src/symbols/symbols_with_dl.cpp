#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include <dlfcn.h>

namespace cpptrace {
    namespace detail {
        struct symbolizer::impl {
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            stacktrace_frame resolve_frame(const void* addr) {
                Dl_info info;
                if(dladdr(addr, &info)) { // thread-safe
                    return {
                        reinterpret_cast<uintptr_t>(addr),
                        0,
                        0,
                        info.dli_fname ? info.dli_fname : "",
                        info.dli_sname ? info.dli_sname : ""
                    };
                } else {
                    return {
                        reinterpret_cast<uintptr_t>(addr),
                        0,
                        0,
                        "",
                        ""
                    };
                }
            }
        };

        // NOLINTNEXTLINE(bugprone-unhandled-exception-at-new)
        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        //stacktrace_frame symbolizer::resolve_frame(void* addr) {
        //    return pimpl->resolve_frame(addr);
        //}

        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            std::vector<stacktrace_frame> trace;
            trace.reserve(frames.size());
            for(const void* frame : frames) {
                trace.push_back(pimpl->resolve_frame(frame));
            }
            return trace;
        }
    }
}

#endif
