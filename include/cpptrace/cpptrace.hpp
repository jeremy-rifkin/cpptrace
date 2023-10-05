#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstdint>
#include <cstdio>
#include <exception>
#include <ostream>
#include <string>
#include <vector>

#if __cplusplus >= 202002L
 #ifdef __has_include
  #if __has_include(<format>)
   #define CPPTRACE_STD_FORMAT
   #include <format>
  #endif
 #endif
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
 #define CPPTRACE_API __declspec(dllexport)
#else
 #define CPPTRACE_API
#endif

namespace cpptrace {
    struct object_trace;
    struct stacktrace;

    struct raw_trace {
        std::vector<uintptr_t> frames;
        CPPTRACE_API static raw_trace current(std::uint_least32_t skip = 0);
        CPPTRACE_API static raw_trace current(std::uint_least32_t skip, std::uint_least32_t max_depth);
        CPPTRACE_API object_trace resolve_object_trace() const;
        CPPTRACE_API stacktrace resolve() const;
        CPPTRACE_API void clear();
        CPPTRACE_API bool empty() const noexcept;

        using iterator = std::vector<uintptr_t>::iterator;
        using const_iterator = std::vector<uintptr_t>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator begin() const noexcept { return frames.begin(); }
        inline const_iterator end() const noexcept { return frames.end(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    struct object_frame {
        std::string obj_path;
        std::string symbol;
        uintptr_t raw_address = 0;
        uintptr_t obj_address = 0;
    };

    struct object_trace {
        std::vector<object_frame> frames;
        CPPTRACE_API static object_trace current(std::uint_least32_t skip = 0);
        CPPTRACE_API static object_trace current(std::uint_least32_t skip, std::uint_least32_t max_depth);
        CPPTRACE_API stacktrace resolve() const;
        CPPTRACE_API void clear();
        CPPTRACE_API bool empty() const noexcept;

        using iterator = std::vector<object_frame>::iterator;
        using const_iterator = std::vector<object_frame>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator begin() const noexcept { return frames.begin(); }
        inline const_iterator end() const noexcept { return frames.end(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    struct stacktrace_frame {
        uintptr_t address;
        std::uint_least32_t line; // TODO: This should use UINT_LEAST32_MAX as a sentinel
        std::uint_least32_t column; // UINT_LEAST32_MAX if not present
        std::string filename;
        std::string symbol;
        bool operator==(const stacktrace_frame& other) const {
            return address == other.address
                && line == other.line
                && column == other.column
                && filename == other.filename
                && symbol == other.symbol;
        }
        bool operator!=(const stacktrace_frame& other) const {
            return !operator==(other);
        }
        CPPTRACE_API std::string to_string() const;
        CPPTRACE_API friend std::ostream& operator<<(std::ostream& stream, const stacktrace_frame& frame);
    };

    struct stacktrace {
        std::vector<stacktrace_frame> frames;
        CPPTRACE_API static stacktrace current(std::uint_least32_t skip = 0);
        CPPTRACE_API static stacktrace current(std::uint_least32_t skip, std::uint_least32_t max_depth);
        CPPTRACE_API void print() const;
        CPPTRACE_API void print(std::ostream& stream) const;
        CPPTRACE_API void print(std::ostream& stream, bool color) const;
        CPPTRACE_API void clear();
        CPPTRACE_API bool empty() const noexcept;
        CPPTRACE_API std::string to_string() const;
        CPPTRACE_API friend std::ostream& operator<<(std::ostream& stream, const stacktrace& trace);

        using iterator = std::vector<stacktrace_frame>::iterator;
        using const_iterator = std::vector<stacktrace_frame>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator begin() const noexcept { return frames.begin(); }
        inline const_iterator end() const noexcept { return frames.end(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    private:
        CPPTRACE_API void print(std::ostream& stream, bool color, bool newline_at_end) const;
    };

    CPPTRACE_API raw_trace generate_raw_trace(std::uint_least32_t skip = 0);
    CPPTRACE_API raw_trace generate_raw_trace(std::uint_least32_t skip, std::uint_least32_t max_depth);
    CPPTRACE_API object_trace generate_object_trace(std::uint_least32_t skip = 0);
    CPPTRACE_API object_trace generate_object_trace(std::uint_least32_t skip, std::uint_least32_t max_depth);
    CPPTRACE_API stacktrace generate_trace(std::uint_least32_t skip = 0);
    CPPTRACE_API stacktrace generate_trace(std::uint_least32_t skip, std::uint_least32_t max_depth);

    // utilities:
    CPPTRACE_API std::string demangle(const std::string& name);
    CPPTRACE_API void absorb_trace_exceptions(bool absorb);

    enum class cache_mode {
        // Only minimal lookup tables
        prioritize_memory,
        // Build lookup tables but don't keep them around between trace calls
        hybrid,
        // Build lookup tables as needed
        prioritize_speed
    };

    namespace experimental {
        CPPTRACE_API void set_cache_mode(cache_mode mode);
    }

    namespace detail {
        CPPTRACE_API bool should_absorb_trace_exceptions();
        CPPTRACE_API enum cache_mode get_cache_mode();
    }

    class exception : public std::exception {
    protected:
        mutable raw_trace trace;
        mutable stacktrace resolved_trace;
        mutable std::string resolved_what;
        explicit exception(std::uint_least32_t skip, std::uint_least32_t max_depth) noexcept
            : trace([skip, max_depth] () noexcept {
                try {
                    return generate_raw_trace(skip + 2, max_depth);
                } catch(const std::exception& e) {
                    if(!detail::should_absorb_trace_exceptions()) {
                        // TODO: Append to message somehow
                        fprintf(
                            stderr,
                            "Exception ocurred while resolving trace in cpptrace::exception object:\n%s\n",
                            e.what()
                        );
                    }
                    return raw_trace{};
                }
            } ()) {}
        explicit exception(std::uint_least32_t skip) noexcept : exception(skip + 1, UINT_LEAST32_MAX) {}
        const stacktrace& get_resolved_trace() const noexcept {
            // I think a non-empty raw trace can never resolve as empty, so this will accurately prevent resolving more
            // than once. Either way the raw trace is cleared.
            try {
                if(resolved_trace.empty() && !trace.empty()) {
                    resolved_trace = trace.resolve();
                    trace.clear();
                }
            } catch(const std::exception& e) {
                if(!detail::should_absorb_trace_exceptions()) {
                    // TODO: Append to message somehow
                    fprintf(
                        stderr,
                        "Exception ocurred while resolving trace in cpptrace::exception object:\n%s\n",
                        e.what()
                    );
                }
            }
            return resolved_trace;
        }
        virtual const std::string& get_resolved_what() const noexcept {
            if(resolved_what.empty()) {
                resolved_what = "cpptrace::exception:\n" + get_resolved_trace().to_string();
            }
            return resolved_what;
        }
    public:
        explicit exception() noexcept : exception(1) {}
        const char* what() const noexcept override {
            return get_resolved_what().c_str();
        }
        // what(), but not a C-string
        const std::string& get_what() const noexcept {
            return resolved_what;
        }
        const raw_trace& get_raw_trace() const noexcept {
            return trace;
        }
        const stacktrace& get_trace() const noexcept {
            return resolved_trace;
        }
    };

    class exception_with_message : public exception {
    protected:
        mutable std::string message;
        explicit exception_with_message(
            std::string&& message_arg,
            uint32_t skip
        ) noexcept : exception(skip + 1), message(std::move(message_arg)) {}
        explicit exception_with_message(
            std::string&& message_arg,
            uint_least32_t skip,
            uint_least32_t max_depth
        ) noexcept : exception(skip + 1, max_depth), message(std::move(message_arg)) {}
        const std::string& get_resolved_what() const noexcept override {
            if(resolved_what.empty()) {
                resolved_what = message + "\n" + get_resolved_trace().to_string();
            }
            return resolved_what;
        }
    public:
        explicit exception_with_message(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
        const std::string& get_message() const noexcept {
            return message;
        }
    };

    class logic_error : public exception_with_message {
    public:
        explicit logic_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class domain_error : public exception_with_message {
    public:
        explicit domain_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class invalid_argument : public exception_with_message {
    public:
        explicit invalid_argument(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class length_error : public exception_with_message {
    public:
        explicit length_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class out_of_range : public exception_with_message {
    public:
        explicit out_of_range(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class runtime_error : public exception_with_message {
    public:
        explicit runtime_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class range_error : public exception_with_message {
    public:
        explicit range_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class overflow_error : public exception_with_message {
    public:
        explicit overflow_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };

    class underflow_error : public exception_with_message {
    public:
        explicit underflow_error(std::string&& message_arg) noexcept
            : exception_with_message(std::move(message_arg), 1) {}
    };
}

#if defined(CPPTRACE_STD_FORMAT) && defined(__cpp_lib_format)
 template <>
 struct std::formatter<cpptrace::stacktrace_frame> : std::formatter<std::string> {
     auto format(cpptrace::stacktrace_frame frame, format_context& ctx) const {
         return formatter<string>::format(frame.to_string(), ctx);
     }
 };

 template <>
 struct std::formatter<cpptrace::stacktrace> : std::formatter<std::string> {
     auto format(cpptrace::stacktrace trace, format_context& ctx) const {
         return formatter<string>::format(trace.to_string(), ctx);
     }
 };
#endif

#endif
