#include <ctrace/ctrace.h>
#include <cassert>
#include <climits>
#include <cstdio>

void test_linker() {
  /* Owning String */ {
    auto str = ctrace_generate_owning_string("Hello C!");
    std::printf("%s\n", str.data);
    ctrace_free_owning_string(&str);
    assert(str.data == nullptr);
  } /* Trace */ {
    ctrace_stacktrace trace = ctrace_generate_trace(0, INT_MAX);
    ctrace_owning_string str = ctrace_stacktrace_to_string(&trace, 0);
    ctrace_free_stacktrace(&trace);
    assert(trace.count == 0);
    std::printf("%s\n", str.data);
    ctrace_free_owning_string(&str);
  }
}

void trace() {
    ctrace_raw_trace raw_trace = ctrace_generate_raw_trace(1, INT_MAX);
    ctrace_object_trace obj_trace = ctrace_raw_trace_resolve_object_trace(&raw_trace);
    ctrace_stacktrace trace = ctrace_object_trace_resolve(&obj_trace);
    ctrace_stacktrace_print(&trace, stdout, 1);
    ctrace_free_stacktrace(&trace);
    ctrace_free_object_trace(&obj_trace);
    ctrace_free_raw_trace(&raw_trace);
    assert(raw_trace.frames == nullptr && obj_trace.count == 0);
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
    function_one(0);
}
