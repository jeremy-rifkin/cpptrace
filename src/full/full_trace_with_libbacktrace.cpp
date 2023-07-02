#ifdef LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "libcpp_full_trace.hpp"
#include "../platform/libcpp_program_name.hpp"
#include "../platform/libcpp_common.hpp"

#include <vector>

#ifdef LIBCPP_BACKTRACE_PATH
#include LIBCPP_BACKTRACE_PATH
#else
#include <backtrace.h>
#endif

namespace cpptrace {
    namespace detail {
        struct trace_data {
            std::vector<stacktrace_frame>& frames;
            size_t& skip;
        };

        int full_callback(void* data_pointer, uintptr_t address, const char* file, int line, const char* symbol) {
            trace_data& data = *reinterpret_cast<trace_data*>(data_pointer);
            if(data.skip > 0) {
                data.skip--;
            } else {
                data.frames.push_back({
                    address,
                    line,
                    -1,
                    file ? file : "",
                    symbol ? symbol : ""
                });
            }
            return 0;
        }

        void error_callback(void*, const char*, int) {
            // nothing for now
        }

        backtrace_state* get_backtrace_state() {
            // backtrace_create_state must be called only one time per program
            static backtrace_state* state = nullptr;
            static bool called = false;
            if(!called) {
                state = backtrace_create_state(program_name().c_str(), true, error_callback, nullptr);
                called = true;
            }
            return state;
        }

        LIBCPPTRACE_FORCE_NO_INLINE
        std::vector<stacktrace_frame> generate_trace(size_t skip) {
            std::vector<stacktrace_frame> frames;
            skip++; // add one for this call
            trace_data data { frames, skip };
            backtrace_full(get_backtrace_state(), 0, full_callback, error_callback, &data);
            return frames;
        }
    }
}

#endif
