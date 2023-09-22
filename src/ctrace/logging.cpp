#include "logging.hpp"

namespace ctrace {
    struct ctrace_logging_ctx* logging_handler::get_logging_ctx() {
        static struct ctrace_logging_ctx ctx { {}, ctrace_loff, stdout };
        return &ctx;
    }

    void logging_handler::set_ctx(ctrace_logger_t logger) {
        get_logging_ctx()->logger = logger;
    }

    void logging_handler::set_ctx(FILE* output) {
        get_logging_ctx()->output = output;
    }

    void logging_handler::log(ctrace_format_t format, std::va_list* pargs) {
        auto ctx = get_logging_ctx();
        ctx->logger(ctx, format, pargs);
    }

    const ctrace_managed_string* logging_handler::messages(unsigned& size) {
        auto& messages = get_logging_ctx()->messages;
        size = messages->size();
        if(!messages->empty())
            return messages->data();

        return nullptr;
    }

    void logging_handler::estimate_and_resize(std::string& str, ctrace_format_t format) {
        unsigned raw_length = std::char_traits<char>::length(format);
        auto length_estimate = ((raw_length + 2) * 2) + 4;
        str.resize(length_estimate);
    }

    void logging_handler::write_to_buffer(std::string& str, ctrace_format_t format, std::va_list* pargs) {
        unsigned raw_len = str.size();
        unsigned buffer_len = raw_len - 3;
        int return_code = std::vsnprintf(&str.front(), buffer_len, format, *pargs);
        if(return_code < 1) return;

        auto written = unsigned(return_code);
        if(written > buffer_len) {
            str.resize(raw_len - 4);
            str += " ...";
        }
    }
}
