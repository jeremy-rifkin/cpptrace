#ifndef ERROR_HPP
#define ERROR_HPP

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

#include "common.hpp"

#if IS_MSVC
 #define CPPTRACE_PFUNC __FUNCSIG__
#else
 #define CPPTRACE_PFUNC __extension__ __PRETTY_FUNCTION__
#endif

namespace cpptrace {
namespace detail {
    class file_error : public std::exception {
        std::string msg;
    public:
        file_error(std::string path) : msg("Unable to read file " + std::move(path)) {}
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

    inline void primitive_assert_impl(
        bool condition,
        bool verify,
        const char* expression,
        const char* signature,
        source_location location,
        const std::string& message = ""
    ) {
        if(!condition) {
            const char* action = verify ? "verification" : "assertion";
            const char* name   = verify ? "VERIFY"       : "ASSERT";
            if(message == "") {
                throw std::runtime_error(
                    stringf(
                        "Cpptrace %s failed at %s:%d: %s\n"
                        "    CPPTRACE_%s(%s);\n",
                        action, location.file, location.line, signature,
                        name, expression
                    )
                );
            } else {
                throw std::runtime_error(
                    stringf(
                        "Cpptrace %s failed at %s:%d: %s: %s\n"
                        "    CPPTRACE_%s(%s);\n",
                        action, location.file, location.line, signature, message.c_str(),
                        name, expression
                    )
                );
            }
        }
    }

    template<typename T>
    void nullfn() {}

    #define PHONY_USE(E) (nullfn<decltype(E)>())

    // Check condition in both debug and release. std::runtime_error on failure.
    #define CPPTRACE_VERIFY(c, ...) ( \
        ::cpptrace::detail::primitive_assert_impl(c, true, #c, CPPTRACE_PFUNC, {}, ##__VA_ARGS__) \
    )

    #ifndef NDEBUG
     // Check condition in both debug. std::runtime_error on failure.
     #define CPPTRACE_ASSERT(c, ...) ( \
         ::cpptrace::detail::primitive_assert_impl(c, false, #c, CPPTRACE_PFUNC, {}, ##__VA_ARGS__) \
     )
    #else
     // Check condition in both debug. std::runtime_error on failure.
     #define CPPTRACE_ASSERT(c, ...) PHONY_USE(c)
    #endif

    // TODO: Setting to silence these or make them fatal
    inline void nonfatal_error(const std::string& message) {
        fprintf(stderr, "Non-fatal cpptrace error: %s\n", message.c_str());
    }
}
}

#endif
