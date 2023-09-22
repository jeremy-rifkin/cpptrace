#ifndef CTRACE_EXCEPTION_HPP
#define CTRACE_EXCEPTION_HPP

#include <ctrace/ctrace.h>
#include <csetjmp>

namespace ctrace {
    struct exception_info;

    struct exception_handler {
        static struct ctrace_exception_ctx* get_buffer();
        static exception_info* get_exception_info();
    };

    struct exception_info {
        ctrace_exception_t exception;
        ctrace_managed_string message;
        unsigned id;
    };
}

BEGIN_CTRACE
    struct ctrace_exception_ctx {
        jmp_buf buffer;
        unsigned is_set : 1;
    };
END_CTRACE

#endif
