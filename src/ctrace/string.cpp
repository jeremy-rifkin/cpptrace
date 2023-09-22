#include "string.hpp"
#include <cstring>

#if CTRACE_FREESTANDING == OFF
#include <cstdlib>
#else
#warning Cannot use dynamic allocation on freestanding implementations!
#endif

namespace ctrace {
    static struct ctrace_buffered_string* allocate_buffered_string(unsigned len) {
        unsigned alloc_base = sizeof(struct ctrace_buffered_string);
        unsigned alloc_total = alloc_base + len;
        return (struct ctrace_buffered_string* ) std::malloc(alloc_total);
    }

    /* unsafe, does no checks */
    static struct ctrace_buffered_string* get_as_buffered_string(ctrace_string string) {
        const auto* data = static_cast<const ctrace_buffered_string*>(string.data);
        auto* mut_data = const_cast<ctrace_buffered_string*>(data);
        return mut_data;
    }


    ctrace_string string_handler::create_string(std::string&& string) {
        auto* data = new const std::string(std::move(string));
        return { data, std_string };
    }

    ctrace_string string_handler::create_string(const char* string) {
        return { string, c_string };
    }


    ctrace_mut_string string_handler::create_buffered_string(unsigned len) {
        unsigned size = len + 1;
        auto* data = allocate_buffered_string(len);
        data->capacity = size;
        data->size = 0;
        data->data[0] = '\0';
        data->data[size - 1] = '\0';
        return { data, buf_string };
    }

    void string_handler::clear_string(ctrace_mut_string string) {
        if(string.type == buf_string) {
            auto* data = get_as_buffered_string(string);
            data->size = 0;
            data->data[0] = '\0';
        }

        CTRACE_WARN("string of this type cannot be cleared (%s)\n", get_type(string));
    }

    void string_handler::copy_string(ctrace_mut_string to, ctrace_string from) {
        if(to.type == buf_string) {
            auto* data = get_as_buffered_string(to);
            char* to_data = data->get();
            const char* from_data = get_cstring(from);
            if(data->data == from_data) return;

            unsigned other_len = get_size(from);
            unsigned max_size = data->remaining_storage();
            unsigned write_size =
                (other_len > max_size) ? max_size : other_len;
            data->prepare_write(write_size);
            std::memcpy(to_data, from_data, write_size);
            return;
        }

        CTRACE_WARN("string of this type cannot be written to (%s)\n", get_type(to));
    }

    void string_handler::format_string(ctrace_mut_string to, const char* format, std::va_list* pargs) {
        if(to.type == buf_string) {
            auto* data = get_as_buffered_string(to);
            char* to_data = data->get();
            const unsigned remaining_storage = data->remaining_storage();
            if(!remaining_storage) return;

            unsigned max_size = remaining_storage + 1;
            int n_written = std::vsnprintf(to_data, max_size, format, *pargs);
            if(n_written > 0) {
                data->size += (unsigned(n_written) > remaining_storage) ? remaining_storage : n_written;
            }
            return;
        }

        CTRACE_WARN("string of this type cannot be written to (%s)\n", get_type(to));
    }


    const char* string_handler::get_cstring(ctrace_string string) {
        if(string.type == std_string) {
            return static_cast<const std::string*>(string.data)->c_str();
        }
        else if(string.type == buf_string) {
            return get_as_buffered_string(string)->data;
        }

        return static_cast<const char*>(string.data);
    }

    const char* string_handler::get_type(ctrace_string string) {
        static constexpr const char* names[] = {
            "String Literal",
            "Internal String",
            "Buffered String",
        };

        return (string.type <= valid) ? names[string.type] : "Invalid String";
    }

    const char* string_handler::get_underlying_type(ctrace_string string) {
        static constexpr const char* names[] = {
            "const char*",
            "std::string*",
            "struct ctrace_buffered_string*",
        };

        return (string.type <= valid) ? names[string.type] : "void";
    }

    unsigned string_handler::get_size(ctrace_string string) {
        if(string.type == std_string) {
            return static_cast<const std::string*>(string.data)->size();
        }
        else if(string.type == buf_string) {
            return get_as_buffered_string(string)->size;
        }

        return std::char_traits<char>::length(get_cstring(string));
    }

    void string_handler::free_string(ctrace_string string) {
        if(string.type == std_string) {
            const auto* data = static_cast<const std::string*>(string.data);
            delete data;
        }
        else if(string.type == buf_string) {
            auto* data = get_as_buffered_string(string);
            std::free(data);
        }
    }
}
