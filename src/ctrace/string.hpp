#ifndef CTRACE_STRING_HPP
#define CTRACE_STRING_HPP

#include <ctrace/ctrace.h>
#include <cstdarg>
#include <string>

#if defined(__cplusplus)
# define CTRACE_BUFFER_LEN 1
#else
# define CTRACE_BUFFER_LEN
#endif

namespace ctrace {
    struct string_handler {
        static ctrace_string create_string(std::string&& string);
        static ctrace_string create_string(const char* string);
        static ctrace_mut_string create_buffered_string(unsigned len);
        static void clear_string(ctrace_mut_string string);
        static void copy_string(ctrace_mut_string to, ctrace_string from);
        static void format_string(ctrace_mut_string to, const char* format, std::va_list* pargs);
        static const char* get_cstring(ctrace_string string);
        static const char* get_type(ctrace_string string);
        static const char* get_underlying_type(ctrace_string string);
        static unsigned get_size(ctrace_string string);
        static void free_string(ctrace_string string);
    private:
        enum : unsigned {
            c_string,
            std_string,
            buf_string,
            valid = buf_string,
        };
    };
}

BEGIN_CTRACE
    struct ctrace_buffered_string {
        friend struct ctrace::string_handler;
    private:
        unsigned capacity;
        unsigned size;

        /* Since FAMs aren't standard in C++,
         * we need to add a size :(
         */

        union {
            char data[CTRACE_BUFFER_LEN];
            char _pad_[1]; // To ensure the size is same in C
        };
    public:
        char* get() noexcept {
            auto* buf = this->data;
            return &buf[size];
        }
        void prepare_write(unsigned len) {
            this->size += len;
            *this->get() = '\0';
        }
        unsigned max_size() const noexcept {
            return this->capacity - 1;
        }
        unsigned remaining_storage() const noexcept {
            return max_size() - this->size;
        }
    };
END_CTRACE

#endif
