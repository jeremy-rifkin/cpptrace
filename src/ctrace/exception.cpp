#define CTRACE_EXCEPTIONS ON
#include "exception.hpp"

namespace ctrace {
    struct ctrace_exception_ctx* exception_handler::get_buffer() {
        static thread_local struct ctrace_exception_ctx buffer = { };
        return &buffer;
    }

    exception_info* exception_handler::get_exception_info() {
        static thread_local exception_info info = { };
        return &info;
    }


    struct register_standard_exceptions {
        explicit register_standard_exceptions() {
            CTRACE_REGISTER_EXCEPTION(cpptrace_exception);
        }
    };

    CTRACE_INTERNAL register_standard_exceptions X { };
}
