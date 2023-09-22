#ifndef CTRACE_LOGGING_H
#define CTRACE_LOGGING_H

#include "logging.hpp"

BEGIN_CTRACE
    CTRACE_API void ctrace_log_mode(ctrace_logger_t logger) {
        if(logger) ctrace::logging_handler::set_ctx(logger);
    }

    CTRACE_API void ctrace_log_to(FILE* stream) {
        if(stream) ctrace::logging_handler::set_ctx(stream);
    }

    CTRACE_API void ctrace_log(ctrace_format_t format, ...) {
        std::va_list args;
        va_start(args, format);
        ctrace::logging_handler::log(format, &args);
        va_end(args);
    }

    CTRACE_API ctrace_log_iterator ctrace_log_begin() {
        unsigned size = 0;
        auto* begin = ctrace::logging_handler::messages(size);
        return { begin, begin + size };
    }

    CTRACE_API ctrace_boolean ctrace_log_end(ctrace_log_iterator iter) {
        return ctrace_boolean(iter.curr == iter.end);
    }

    CTRACE_API void ctrace_log_next(ctrace_log_iterator* iter) {
        iter->curr = std::next(iter->curr);
    }


    CTRACE_API void ctrace_loff(ctrace_plog_ctx, ctrace_format_t, va_list*) { }

    CTRACE_API void ctrace_lquiet(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs) {
        std::string buffer {};
        ctrace::logging_handler::estimate_and_resize(buffer, format);
        ctrace::logging_handler::write_to_buffer(buffer, format, pargs);
        ctx->messages.push(buffer);
    }

    CTRACE_API void ctrace_ldebug(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs) {
        std::vfprintf(ctx->output, format, *pargs);
    }

    CTRACE_API void ctrace_ltrace(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs) {
        std::vfprintf(ctx->output, format, *pargs);
        auto trace = cpptrace::generate_trace(CTRACE_LOG_CALL_DEPTH);
        std::string trace_string = trace.to_string();
        std::fprintf(ctx->output, "%s\n", trace_string.c_str());
    }
END_CTRACE

#endif
