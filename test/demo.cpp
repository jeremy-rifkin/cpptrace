#include <cpptrace/cpptrace.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include <ctrace/ctrace.h>
#include <cassert>

void test_linker() {
    auto str = ctrace_generate_owning_string("Hello C!");
    std::printf("%s\n", str.data);
    ctrace_free_owning_string(&str);
    assert(str.data == nullptr);

    ctrace_stacktrace trace = ctrace_generate_trace(0, INT_MAX);
    ctrace_free_stacktrace(&trace);
    assert(trace.count == 0);
}

void trace() {
    ctrace_stacktrace trace = ctrace_generate_trace(0, INT_MAX);
    ctrace_stacktrace_print(&trace, stdout, 1);
    ctrace_free_stacktrace(&trace);
    
    cpptrace::generate_trace().print();
    // throw cpptrace::logic_error("foobar");
}

void foo(int n) {
    if(n == 0) {
        trace();
    } else {
        foo(n - 1);
    }
}

template<typename... Args>
void foo(int x, Args... args) {
    foo(args...);
}

void function_two(int, float) {
    foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

CTRACE_FORCE_INLINE
void function_one(int) {
    function_two(0, 0);
}

int main() {
    test_linker();
    cpptrace::absorb_trace_exceptions(false);
    cpptrace::register_terminate_handler();
    function_one(0);
}
