#ifdef CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "../platform/program_name.hpp"
#include "../platform/common.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

#ifdef CPPTRACE_BACKTRACE_PATH
#include CPPTRACE_BACKTRACE_PATH
#else
#include <backtrace.h>
#endif

namespace cpptrace {
    namespace detail {
        struct trace_data {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
            std::vector<stacktrace_frame>& frames;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
            size_t& skip;
        };

        int full_callback(void* data_pointer, uintptr_t address, const char* file, int line, const char* symbol) {
            trace_data& data = *reinterpret_cast<trace_data*>(data_pointer);
            if(data.skip > 0) {
                data.skip--;
            } else if(address == uintptr_t(-1)) {
                // sentinel for libbacktrace, stop tracing
                return 1;
            } else {
                data.frames.push_back({
                    address,
                    static_cast<std::uint_least32_t>(line),
                    0,
                    file ? file : "",
                    symbol ? symbol : ""
                });
            }
            return 0;
        }

        void syminfo_callback(void* data, uintptr_t, const char* symbol, uintptr_t, uintptr_t) {
            stacktrace_frame& frame = *static_cast<stacktrace_frame*>(data);
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
                state = backtrace_create_state(nullptr, true, error_callback, nullptr);
                called = true;
            }
            return state;
        }

        CPPTRACE_FORCE_NO_INLINE
        std::vector<stacktrace_frame> generate_trace(size_t skip) {
            std::vector<stacktrace_frame> frames;
            skip++; // add one for this call
            trace_data data { frames, skip };
            backtrace_full(get_backtrace_state(), 0, full_callback, error_callback, &data);
            for(auto& frame : frames) {
                if(frame.symbol.empty()) {
                    // fallback, try to at least recover the symbol name with backtrace_syminfo
                    backtrace_syminfo(
                        get_backtrace_state(),
                        frame.address,
                        syminfo_callback,
                        error_callback,
                        &frame
                    );
                }
            }
            return frames;
        }
    }
}

#endif
