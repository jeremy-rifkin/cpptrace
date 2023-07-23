#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/program_name.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>

#ifdef CPPTRACE_BACKTRACE_PATH
#include CPPTRACE_BACKTRACE_PATH
#else
#include <backtrace.h>
#endif

namespace cpptrace {
    namespace detail {
        int full_callback(void* data, uintptr_t address, const char* file, int line, const char* symbol) {
            stacktrace_frame& frame = *static_cast<stacktrace_frame*>(data);
            if(line == 0) {
                ///fprintf(stderr, "Getting bad data for some reason\n"); // TODO: Eliminate
            }
            frame.address = address;
            frame.line = line;
            frame.filename = file ? file : "";
            frame.symbol = symbol ? symbol : "";
            return 0;
        }

        void syminfo_callback(void* data, uintptr_t address, const char* symbol, uintptr_t, uintptr_t) {
            stacktrace_frame& frame = *static_cast<stacktrace_frame*>(data);
            frame.address = address;
            frame.line = 0;
            frame.filename = "";
            frame.symbol = symbol ? symbol : "";
        }

        void error_callback(void*, const char* msg, int errnum) {
            fprintf(stderr, "Libbacktrace error: %s, code %d\n", msg, errnum);
        }

        backtrace_state* get_backtrace_state() {
            static std::mutex mutex;
            const std::lock_guard<std::mutex> lock(mutex);
            // backtrace_create_state must be called only one time per program
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
            static backtrace_state* state = nullptr;
            static bool called = false;
            if(!called) {
                state = backtrace_create_state(program_name(), true, error_callback, nullptr);
                called = true;
            }
            return state;
        }

        // TODO: Handle backtrace_pcinfo calling the callback multiple times on inlined functions
        struct symbolizer::impl {
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            stacktrace_frame resolve_frame(const void* addr) {
                stacktrace_frame frame;
                frame.col = 0;
                backtrace_pcinfo(
                    get_backtrace_state(),
                    reinterpret_cast<uintptr_t>(addr),
                    full_callback,
                    error_callback,
                    &frame
                );
                if(frame.symbol.empty()) {
                    // fallback, try to at least recover the symbol name with backtrace_syminfo
                    backtrace_syminfo(
                        get_backtrace_state(),
                        reinterpret_cast<uintptr_t>(addr),
                        syminfo_callback,
                        error_callback,
                        &frame
                    );
                }
                return frame;
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
