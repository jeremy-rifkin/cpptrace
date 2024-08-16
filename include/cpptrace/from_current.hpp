#include <cpptrace/cpptrace.hpp>

#include <iostream>
#include <sys/mman.h>
#include <array>
#include <cstring>

#ifdef _MSC_VER

#else

namespace cpptrace {
    const raw_trace& raw_trace_from_current_exception();
    const stacktrace& from_current_exception();

    class unwind_interceptor {
    public:
        virtual ~unwind_interceptor();
    };
}

/*

CPPTRACE_TRY {
    foo();
} CPPTRACE_CATCH(std::runtime_error& e) {
    fmt::println("Exception occurred: {}", e.get().what());
    fmt::println("{}", cpptrace::from_current_exception());
}

try {
    try {
        foo();
    } catch(const ::cpptrace::unwind_interceptor&) {}
} catch(std::runtime_error& e) {
    fmt::println("Exception occurred: {}", e.get().what());
    fmt::println("{}", cpptrace::from_current_exception());
}

*/

#define CPPTRACE_TRY \
    try { \
        try

#define CPPTRACE_CATCH(param) \
        catch(cpptrace::unwind_interceptor&) { puts("shouldn't be here"); } \
    } catch(param)


#endif
