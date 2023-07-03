#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE

#include <cpptrace/cpptrace.hpp>
#include "libcpp_symbols.hpp"
#include "../platform/libcpp_program_name.hpp"

#include <memory>
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
                fprintf(stderr, "Getting bad data for some reason\n");
            }
            frame.address = address;
            frame.line = line;
            frame.filename = file ? file : "";
            frame.symbol = symbol ? symbol : "";
            return 0;
        }

        void error_callback(void* data, const char* msg, int errnum) {
            // nothing at the moment
            fprintf(stderr, "Backtrace error %s %d %p\n", msg, errnum, data);
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

        // TODO: Handle backtrace_pcinfo calling the callback multiple times on inlined functions
        struct symbolizer::impl {
            stacktrace_frame resolve_frame(void* addr) {
                stacktrace_frame frame;
                frame.col = -1;
                backtrace_pcinfo(
                    get_backtrace_state(),
                    reinterpret_cast<uintptr_t>(addr),
                    full_callback,
                    error_callback,
                    &frame
                );
                return frame;
            }
        };

        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        stacktrace_frame symbolizer::resolve_frame(void* addr) {
            return pimpl->resolve_frame(addr);
        }
    }
}

#endif
