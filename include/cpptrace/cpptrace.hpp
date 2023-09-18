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

    struct CPPTRACE_API raw_trace {
        std::vector<uintptr_t> frames;
        explicit raw_trace(std::vector<uintptr_t>&& frames_) : frames(frames_) {}
        object_trace resolve_object_trace() const;
        stacktrace resolve() const;
        void clear();

        using iterator = std::vector<uintptr_t>::iterator;
        using const_iterator = std::vector<uintptr_t>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    struct CPPTRACE_API object_frame {
        std::string obj_path;
        std::string symbol;
        uintptr_t raw_address = 0;
        uintptr_t obj_address = 0;
    };

    struct CPPTRACE_API object_trace {
        std::vector<object_frame> frames;
        explicit object_trace(std::vector<object_frame>&& frames_) : frames(frames_) {}
        stacktrace resolve() const;
        void clear();

        using iterator = std::vector<object_frame>::iterator;
        using const_iterator = std::vector<object_frame>::const_iterator;
        inline iterator begin() noexcept { return frames.begin(); }
        inline const_iterator cbegin() const noexcept { return frames.cbegin(); }
        inline iterator end() noexcept { return frames.end(); }
        inline const_iterator cend() const noexcept { return frames.cend(); }
    };

    struct CPPTRACE_API stacktrace_frame {
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

    struct CPPTRACE_API stacktrace {
        std::vector<stacktrace_frame> frames;
        explicit stacktrace(std::vector<stacktrace_frame>&& frames_) : frames(frames_) {}
        void print() const;
        void print(std::ostream& stream) const;
        void print(std::ostream& stream, bool color) const;
        std::string to_string() const;
        void clear();

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
    CPPTRACE_API std::string demangle(const std::string& str);

    class CPPTRACE_API exception : public std::exception {
    protected:
        mutable raw_trace trace;
        mutable std::string resolved_message;
    public:
        explicit exception() : trace(generate_raw_trace()) {}
        virtual const std::string& get_resolved_message() const {
            if(resolved_message.empty()) {
                resolved_message = "cpptrace::exception:\n" + trace.resolve().to_string();
                trace.clear();
            }
            return resolved_message;
        }
        const char* what() const noexcept override {
            return get_resolved_message().c_str();
        }
    };

    class CPPTRACE_API exception_with_message : public exception {
        mutable std::string message;
    public:
        // NOLINTNEXTLINE(modernize-pass-by-value)
        explicit exception_with_message(const std::string& message_arg) : message(message_arg) {}
        explicit exception_with_message(const char* message_arg) : message(message_arg) {}
        const std::string& get_resolved_message() const override {
            if(resolved_message.empty()) {
                resolved_message = message + "\n" + trace.resolve().to_string();
                trace.clear();
                message.clear();
            }
            return resolved_message;
        }
        const char* what() const noexcept override {
            return get_resolved_message().c_str();
        }
    };

    class CPPTRACE_API logic_error : public exception_with_message {};
    class CPPTRACE_API domain_error : public exception_with_message {};
    class CPPTRACE_API invalid_argument : public exception_with_message {};
    class CPPTRACE_API length_error : public exception_with_message {};
    class CPPTRACE_API out_of_range : public exception_with_message {};
    class CPPTRACE_API runtime_error : public exception_with_message {};
    class CPPTRACE_API range_error : public exception_with_message {};
    class CPPTRACE_API overflow_error : public exception_with_message {};
    class CPPTRACE_API underflow_error : public exception_with_message {};
}

#endif
