#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDL

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_symbols.hpp"
#include "../platform/cpptrace_program_name.hpp"

#include <memory>
#include <vector>

#include <dlfcn.h>

namespace cpptrace {
    namespace detail {
        struct symbolizer::impl {
            stacktrace_frame resolve_frame(void* addr) {
                Dl_info info;
                if(dladdr(addr, &info)) {
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

        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        //stacktrace_frame symbolizer::resolve_frame(void* addr) {
        //    return pimpl->resolve_frame(addr);
        //}

        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            std::vector<stacktrace_frame> trace;
            trace.reserve(frames.size());
            for(const auto frame : frames) {
                trace.push_back(pimpl->resolve_frame(frame));
            }
            return trace;
        }
    }
}

#endif
