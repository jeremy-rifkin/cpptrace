#ifndef CPPTRACE_HPP
#define CPPTRACE_HPP

#include <cstdint>
#include <exception>
#include <ostream>
#include <string>
#include <vector>

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
        explicit raw_trace(std::vector<uintptr_t>&& frames_) : frames(frames_) {}
        CPPTRACE_API object_trace resolve_object_trace() const;
        CPPTRACE_API stacktrace resolve() const;
        CPPTRACE_API void clear();

        using iterator = std::vector<uintptr_t>::iterator;
        using const_iterator = std::vector<uintptr_t>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline iterator end() noexcept { return frames.end(); }
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
        explicit object_trace(std::vector<object_frame>&& frames_) : frames(frames_) {}
        CPPTRACE_API stacktrace resolve() const;
        CPPTRACE_API void clear();

        using iterator = std::vector<object_frame>::iterator;
        using const_iterator = std::vector<object_frame>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    struct stacktrace_frame {
        uintptr_t address;
        std::uint_least32_t line;
        std::uint_least32_t col;
        std::string filename;
        std::string symbol;
        bool operator==(const stacktrace_frame& other) const {
            return address == other.address
                && line == other.line
                && col == other.col
                && filename == other.filename
                && symbol == other.symbol;
        }
        bool operator!=(const stacktrace_frame& other) const {
            return !operator==(other);
        }
    };

    struct stacktrace {
        std::vector<stacktrace_frame> frames;
        explicit stacktrace(std::vector<stacktrace_frame>&& frames_) : frames(frames_) {}
        CPPTRACE_API void print() const;
        CPPTRACE_API void print(std::ostream& stream) const;
        CPPTRACE_API void print(std::ostream& stream, bool color) const;
        CPPTRACE_API std::string to_string() const;
        CPPTRACE_API void clear();

        using iterator = std::vector<stacktrace_frame>::iterator;
        using const_iterator = std::vector<stacktrace_frame>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    CPPTRACE_API raw_trace generate_raw_trace(std::uint32_t skip = 0);
    CPPTRACE_API object_trace generate_object_trace(std::uint32_t skip = 0);
    CPPTRACE_API stacktrace generate_trace(std::uint32_t skip = 0);

    // utilities:
    CPPTRACE_API std::string demangle(const std::string& name);

    class exception : public std::exception {
    protected:
        mutable raw_trace trace;
        mutable std::string resolved_message;
        explicit exception(uint32_t skip) : trace(generate_raw_trace(skip + 1)) {}
        virtual const std::string& get_resolved_message() const {
            if(resolved_message.empty()) {
                resolved_message = "cpptrace::exception:\n" + trace.resolve().to_string();
                trace.clear();
            }
            return resolved_message;
        }
    public:
        explicit exception() : exception(1) {}
        const char* what() const noexcept override {
            return get_resolved_message().c_str();
        }
    };

    class exception_with_message : public exception {
    protected:
        mutable std::string message;
        explicit exception_with_message(std::string&& message_arg, uint32_t skip)
            : exception(skip + 1), message(std::move(message_arg)) {}
        const std::string& get_resolved_message() const override {
            if(resolved_message.empty()) {
                resolved_message = message + "\n" + trace.resolve().to_string();
                trace.clear();
                message.clear();
            }
            return resolved_message;
        }
    public:
        explicit exception_with_message(std::string&& message_arg)
            : exception_with_message(std::move(message_arg), 1) {}
        const char* what() const noexcept override {
            return get_resolved_message().c_str();
        }
    };

    class logic_error : public exception_with_message {
    public:
        explicit logic_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class domain_error : public exception_with_message {
    public:
        explicit domain_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class invalid_argument : public exception_with_message {
    public:
        explicit invalid_argument(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class length_error : public exception_with_message {
    public:
        explicit length_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class out_of_range : public exception_with_message {
    public:
        explicit out_of_range(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class runtime_error : public exception_with_message {
    public:
        explicit runtime_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class range_error : public exception_with_message {
    public:
        explicit range_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class overflow_error : public exception_with_message {
    public:
        explicit overflow_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };

    class underflow_error : public exception_with_message {
    public:
        explicit underflow_error(std::string&& message_arg) : exception_with_message(std::move(message_arg), 1) {}
    };
}

#endif
