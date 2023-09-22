#ifndef CTRACE_EXCEPTION_H
#define CTRACE_EXCEPTION_H

#include <ctrace/ctrace.h>
#include <atomic>
#include <csetjmp>
#include <iostream>
#include "exception.hpp"

BEGIN_CTRACE
    CTRACE_INTERNAL unsigned ctrace_internal_unique_id();

    CTRACE_API void ctrace_register_exception(ctrace_exception_t type, ctrace_message_t what) {
        auto* type_data = type();
        if(!type_data->id) {
            type_data->id = ctrace_internal_unique_id();
            type_data->what = what;
        }
    }

    CTRACE_API ctrace_managed_string ctrace_exception_string(ctrace_string string) {
        auto* info = ctrace::exception_handler::get_exception_info();
        info->message = string;
        return info->message;
    }

    CTRACE_NORETURN
    CTRACE_API void ctrace_throw(ctrace_exception_t type) {
        auto* info = ctrace::exception_handler::get_exception_info();
        auto* buffer = ctrace::exception_handler::get_buffer();
        auto trace = ctrace_generate_raw_trace(1);
        info->exception = type;
        info->id = type()->id;
        type()->trace = trace;

        if(buffer->is_set == OFF) {
            ctrace_managed_string string = type()->what();
            ctrace_trace resolved = ctrace_move_resolve(trace);
            {
                std::string to_print = "abort called after throwing an instance of `";
                to_print += type()->name;
                to_print += "`\n   what():  ";
                to_print += ctrace_get_cstring(string);
                std::cerr << to_print << '\n';
                ctrace_print_trace(resolved, ectrace_err);
                std::cerr << std::flush;
            }
            ctrace_free_trace(resolved);
            ctrace_free_string(string);
            abort();
        }
        else if(!(info->id && info->exception)) {
            ctrace_free_trace(trace);
            CTRACE_FATAL("exception type `%s` has not been registered!\n",
                type()->name);
        }

        buffer->is_set = OFF;
        std::longjmp(buffer->buffer, int(info->id));
    }

    CTRACE_API ctrace_exception_ptr_t ctrace_exception_ptr() {
        auto* info = ctrace::exception_handler::get_exception_info();
        return info->exception();
    }

    CTRACE_API unsigned ctrace_exception_id() {
        auto* info = ctrace::exception_handler::get_exception_info();
        return info->id;
    }

    CTRACE_API void ctrace_exception_release() {
        auto* info = ctrace::exception_handler::get_exception_info();
        if(info->id != 0) {
            auto* ex = info->exception();
            ctrace_free_trace(ex->trace);
            ex->trace = { };

            ctrace_free_string(info->message);
            info->message.data = nullptr;
            info->id = 0;
        }
    }

    CTRACE_API struct ctrace_exception_ctx* ctrace_internal_get_buf() {
        auto* buffer = ctrace::exception_handler::get_buffer();
        buffer->is_set = ON;
        return buffer;
    }

    CTRACE_API struct ctrace_registry_data* cpptrace_exception() {
        static struct ctrace_registry_data reg = { 0, "cpptrace_exception", CTRACE_NULL, {} };
        return &reg;
    }

    CTRACE_API ctrace_managed_string ctrace_user_cpptrace_exception_what() {
        ctrace_string str = ctrace_make_string("ctrace::cpptrace_exception:");
        return ctrace_exception_string(str);
    }

    unsigned ctrace_internal_unique_id() {
        static std::atomic<unsigned> id;
        return ++id;
    }
END_CTRACE

#endif
