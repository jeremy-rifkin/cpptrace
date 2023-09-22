#ifndef CTRACE_STRING_H
#define CTRACE_STRING_H

#include <string>
#include "string.hpp"

BEGIN_CTRACE
    CTRACE_INTERNAL const char* check_writes(ctrace_mut_string to, ctrace_string from);

    CTRACE_API ctrace_mut_string ctrace_make_buffered_string(unsigned len) {
        return ctrace::string_handler::create_buffered_string(len);
    }

    CTRACE_API void ctrace_clear_string(ctrace_mut_string string) {
        if(string.data) ctrace::string_handler::clear_string(string);
        else CTRACE_WARN("string cannot be cleared (uninitialized)\n");
    }

    CTRACE_API void ctrace_write_string(ctrace_mut_string to, ctrace_string from) {
        if(to.data && from.data) ctrace::string_handler::copy_string(to, from);
        else CTRACE_WARN("string cannot be written to (%s)\n", check_writes(to, from));
    }

    CTRACE_API void ctrace_write_move_string(ctrace_mut_string to, ctrace_string from) {
        if(to.data && from.data) {
            ctrace_write_string(to, from);
            ctrace_free_string(from);
        }
        else {
            const char* msg = check_writes(to, from);
            if(!to.data && from.data) CTRACE_ERR("string cannot be moved (%s)", msg);
            else CTRACE_WARN("string cannot be written to (%s)\n", msg);
        }
    }

    CTRACE_API void ctrace_format_string(ctrace_mut_string to, const char* format, ...) {
        std::va_list args;
        va_start(args, format);
        if(to.data) ctrace::string_handler::format_string(to, format, &args);
        else CTRACE_WARN("string cannot be written to (uninitialized)\n");
        va_end(args);
    }


    CTRACE_API ctrace_string ctrace_make_string(const char* cstring) {
        if(cstring) return ctrace::string_handler::create_string(cstring);
        else {
            CTRACE_ERR("string cannot be created (NULL input)\n");
            return { };
        }
    }

    CTRACE_API const char* ctrace_get_cstring(ctrace_string string) {
        if(string.data) return ctrace::string_handler::get_cstring(string);
        else {
            CTRACE_WARN("cstring could not be extracted (uninitialized)\n");
            return "<null>";
        }
    }

    CTRACE_API const char* ctrace_get_string_type(ctrace_string string) {
        return ctrace::string_handler::get_type(string);
    }

    CTRACE_API const char* ctrace_get_underlying_string_type(ctrace_string string) {
        return ctrace::string_handler::get_underlying_type(string);
    }

    CTRACE_API unsigned ctrace_get_string_size(ctrace_string string) {
        if(string.data) return ctrace::string_handler::get_size(string);
        else {
            CTRACE_WARN("cstring could not be extracted (uninitialized)\n");
            return 0;
        }
    }

    CTRACE_API void ctrace_free_string(ctrace_string string) {
        if(string.data) ctrace::string_handler::free_string(string);
        else CTRACE_WARN("string could not be freed (uninitialized)\n");
    }

    CTRACE_API int ctrace_fputs(ctrace_string string, FILE* f) {
        if(string.data) return std::fputs(ctrace_get_cstring(string), f);
        else return std::fputs("<null>", f);
    }

    CTRACE_API int ctrace_puts(ctrace_string string) {
        if(string.data) return std::puts(ctrace_get_cstring(string));
        else return std::puts("<null>");
    }

    const char* check_writes(ctrace_mut_string to, ctrace_string from) {
        if(!to.data && !from.data) return "`to` and `from` uninitialized";
        else if(!to.data) return "`to` uninitialized";
        else if(!from.data) return "`from` uninitialized";
        else return "";
    }
END_CTRACE

#endif
