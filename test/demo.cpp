#include <cpptrace/cpptrace.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

void trace() {
    cpptrace::generate_trace().print();
    throw cpptrace::exception_with_message("foobar");
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

void function_one(int) {
    function_two(0, 0);
}

int main() try {
    cpptrace::absorb_trace_exceptions(false);
    function_one(0);
} catch(cpptrace::exception& e) {
    std::cerr << "Error: "
              << e.get_raw_what()
              << '\n';
    e.get_trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
}
