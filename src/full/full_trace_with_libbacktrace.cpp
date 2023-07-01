#ifdef LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "libcpp_full_trace.hpp"
#include "../platform/libcpp_program_name.hpp"

#include <vector>

#include <backtrace.h>

namespace cpptrace {
    namespace detail {
        int full_callback(void* data, uintptr_t address, const char* file, int line, const char* symbol) {
            reinterpret_cast<std::vector<stacktrace_frame>*>(data)->push_back({
                address,
                line,
                -1,
                file ? file : "",
                symbol ? symbol : ""
            });
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

        std::vector<stacktrace_frame> generate_trace() {
            std::vector<stacktrace_frame> frames;
            backtrace_full(get_backtrace_state(), 0, full_callback, error_callback, &frames);
            return frames;
        }
    }
}

#endif
