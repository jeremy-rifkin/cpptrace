#ifndef ERROR_HPP
#define ERROR_HPP

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "common.hpp"
#include "microfmt.hpp"

#if IS_MSVC
 #define CPPTRACE_PFUNC __FUNCSIG__
#else
 #define CPPTRACE_PFUNC __extension__ __PRETTY_FUNCTION__
#endif

namespace cpptrace {
namespace detail {
    class internal_error : public std::exception {
        std::string msg;
    public:
        internal_error(std::string message) : msg("Cpptrace internal error: " + std::move(message)) {}
        template<typename... Args>
        internal_error(const char* format, Args&&... args) : internal_error(microfmt::format(format, args...)) {}
        const char* what() const noexcept override {
            return msg.c_str();
        }
    };

    // Lightweight std::source_location.
    struct source_location {
        const char* const file;
        //const char* const function; // disabled for now due to static constexpr restrictions
        const int line;
        constexpr source_location(
            //const char* _function /*= __builtin_FUNCTION()*/,
            const char* _file     = __builtin_FILE(),
            int _line             = __builtin_LINE()
        ) : file(_file), /*function(_function),*/ line(_line) {}
    };

    enum class assert_type {
        assert,
        verify,
        panic,
    };

    constexpr const char* assert_actions[] = {"assertion", "verification", "panic"};
    constexpr const char* assert_names[] = {"ASSERT", "VERIFY", "PANIC"};

    [[noreturn]] inline void assert_fail(
        assert_type type,
        const char* expression,
        const char* signature,
        source_location location,
        const std::string& message = ""
    ) {
        const char* action = assert_actions[static_cast<std::underlying_type<assert_type>::type>(type)];
        const char* name   = assert_names[static_cast<std::underlying_type<assert_type>::type>(type)];
        if(message == "") {
            throw internal_error(
                "Cpptrace {} failed at {}:{}: {}\n"
                "    {}({});\n",
                action, location.file, location.line, signature,
                name, expression
            );
        } else {
            throw internal_error(
                "Cpptrace {} failed at {}:{}: {}: {}\n"
                "    {}({});\n",
                action, location.file, location.line, signature, message.c_str(),
                name, expression
            );
        }
    }

    [[noreturn]] inline void panic(
        const char* signature,
        source_location location,
        const std::string& message = ""
    ) {
        if(message == "") {
            throw internal_error(
                "Cpptrace panic {}:{}: {}\n",
                location.file, location.line, signature
            );
        } else {
            throw internal_error(
                "Cpptrace panic {}:{}: {}: {}\n",
                location.file, location.line, signature, message.c_str()
            );
        }
    }

    template<typename T>
    void nullfn() {
        // this method doesn't do anything and is never called.
    }

    #define PHONY_USE(E) (nullfn<decltype(E)>())

    // Workaround a compiler warning
    template<typename T>
    bool as_bool(T&& value) {
        return static_cast<bool>(std::forward<T>(value));
    }

    // Check condition in both debug and release. std::runtime_error on failure.
    #define VERIFY(c, ...) ( \
            (::cpptrace::detail::as_bool(c)) \
                ? static_cast<void>(0) \
                : (::cpptrace::detail::assert_fail)( \
                    ::cpptrace::detail::assert_type::verify, \
                    #c, \
                    CPPTRACE_PFUNC, \
                    {}, \
                    ##__VA_ARGS__) \
    )

    // Workaround a compiler warning
    template<typename T>
    std::string as_string(T&& value) {
        return std::string(std::forward<T>(value));
    }

    inline std::string as_string() {
        return "";
    }

    // Check condition in both debug and release. std::runtime_error on failure.
    #define PANIC(...) ((::cpptrace::detail::panic)(CPPTRACE_PFUNC, {}, ::cpptrace::detail::as_string(__VA_ARGS__)))

    #ifndef NDEBUG
     // Check condition in both debug. std::runtime_error on failure.
     #define ASSERT(c, ...) ( \
             (::cpptrace::detail::as_bool(c)) \
                 ? static_cast<void>(0) \
                 : (::cpptrace::detail::assert_fail)( \
                    ::cpptrace::detail::assert_type::assert, \
                    #c, \
                    CPPTRACE_PFUNC, \
                    {}, \
                    ##__VA_ARGS__) \
     )
    #else
     // Check condition in both debug. std::runtime_error on failure.
     #define ASSERT(c, ...) PHONY_USE(c)
    #endif
}
}

#endif
