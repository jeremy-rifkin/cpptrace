#include <cpptrace/cpptrace.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

void trace() {
    cpptrace::print_trace();
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

int main() {
    function_one(0);
}
