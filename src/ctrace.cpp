#include <ctrace/ctrace.h>
#include <cpptrace/cpptrace.hpp>

#include "ctrace/exception.h"
#include "ctrace/logging.h"
#include "ctrace/string.h"
#include "ctrace/trace.hpp"
#include "demangle/demangle.hpp"

BEGIN_CTRACE
    CTRACE_API ctrace_trace ctrace_generate_raw_trace(unsigned skip) {
        cpptrace::raw_trace raw = cpptrace::generate_raw_trace(skip + 1);
        return ctrace::trace_handler::wrap_trace(std::move(raw));
    }

    CTRACE_API ctrace_trace ctrace_generate_object_trace(unsigned skip) {
        cpptrace::object_trace raw = cpptrace::generate_object_trace(skip + 1);
        return ctrace::trace_handler::wrap_trace(std::move(raw));
    }

    CTRACE_API ctrace_trace ctrace_generate_trace(unsigned skip) {
        cpptrace::stacktrace trace = cpptrace::generate_trace(skip + 1);
        return ctrace::trace_handler::wrap_trace(std::move(trace));
    }

    CTRACE_API ctrace_trace ctrace_resolve(ctrace_trace trace) {
        return ctrace::trace_handler::resolve(trace);
    }

    CTRACE_API ctrace_trace ctrace_move_resolve(ctrace_trace trace) {
        return ctrace::trace_handler::resolve(trace, true);
    }

    CTRACE_API ctrace_trace ctrace_safe_move_resolve(ctrace_trace* trace) {
        ctrace_trace resolved = ctrace::trace_handler::resolve(*trace, true);
        trace->data = nullptr;
        trace->resolved = ctrace_false;
        return resolved;
    }

    CTRACE_API ctrace_iterator ctrace_begin(ctrace_trace trace) {
        return ctrace::trace_iterator_handler::create_iterator(trace);
    }

    CTRACE_API ctrace_boolean ctrace_end(ctrace_iterator iter) {
        return ctrace_boolean(uintptr_t(iter.curr) == uintptr_t(iter.end));
    }

    CTRACE_API void ctrace_next(ctrace_iterator* iter) {
        ctrace::trace_iterator_handler::next(iter);
    }

    CTRACE_API void ctrace_print_trace(ctrace_trace trace, int opts, ...) {
        std::va_list args;
        va_start(args, opts);
        if(trace.resolved) ctrace::trace_handler::print_trace(trace, ctrace_print_options(opts), &args);
        else CTRACE_WARN("the current trace cannot be printed (unresolved)\n");
        va_end(args);
    }

    CTRACE_API ctrace_string ctrace_stringify_trace(ctrace_trace trace) {
        return ctrace::trace_handler::stringify(trace);
    }

    CTRACE_API void ctrace_clear_trace(ctrace_trace trace) {
        ctrace::trace_handler::clear_trace(trace);
    }

    CTRACE_API void ctrace_free_trace(ctrace_trace trace) {
        if(trace.data && trace.resolved)
            ctrace::trace_handler::free_trace(trace);
    }


    CTRACE_API ctrace_string ctrace_demangle(ctrace_string symbol) {
        const char *raw_str = ctrace_get_cstring(symbol);
        auto demangled = cpptrace::detail::demangle(raw_str);
        return ctrace::string_handler::create_string(std::move(demangled));
    }

    CTRACE_API ctrace_string ctrace_move_demangle(ctrace_string symbol) {
        auto demangled = ctrace_demangle(symbol);
        ctrace_free_string(symbol);
        return demangled;
    }
END_CTRACE