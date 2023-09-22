#ifndef CTRACE_LOGGING_HPP
#define CTRACE_LOGGING_HPP

#include <ctrace/ctrace.h>
#include <vector>
#include "string.hpp"

namespace ctrace {
    struct logging_handler {
        static void set_ctx(ctrace_logger_t logger);
        static void set_ctx(FILE* output);
        static void log(ctrace_format_t format, std::va_list* pargs);
        static const ctrace_managed_string* messages(unsigned& size);
        static void estimate_and_resize(std::string& str, ctrace_format_t format);
        static void write_to_buffer(std::string& str, ctrace_format_t format, std::va_list* pargs);
    private:
        static struct ctrace_logging_ctx* get_logging_ctx();
    };

    struct message_handler {
        typedef ctrace_managed_string message_t;
        typedef std::vector<message_t> message_log_t;
        message_log_t messages_;
        ~message_handler() {
            for(message_t& str : messages_)
                ctrace_free_string(str);
        }
        void push(std::string& message) {
            auto str = string_handler::create_string(std::move(message));
            messages_.push_back(str);
        }
        message_log_t* operator->() {
            return &messages_;
        }
        message_t& operator[](unsigned idx) {
            return messages_[idx];
        }
    };
}

BEGIN_CTRACE
    struct ctrace_logging_ctx {
        ctrace::message_handler messages;
        ctrace_logger_t logger;
        FILE* output;
    };
END_CTRACE

#endif
