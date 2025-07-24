#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

void fail() {
    std::cout << "Throwing an exception from:" << std::endl;
    cpptrace::generate_trace().print();
    throw std::runtime_error("foobar");
}

void foo(int n) {
    if(n == 0) {
        fail();
    } else {
        foo(n - 1);
    }
}

template<typename... Args>
void foo(int, Args... args) {
    foo(args...);
}

void function_two(int, float) {
    foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

void function_one(int) {
    function_two(0, 0);
}

int main() {
    cpptrace::absorb_trace_exceptions(false);
    cpptrace::use_default_stderr_logger();
    cpptrace::register_terminate_handler();
    CPPTRACE_TRY {
        function_one(0);
    } CPPTRACE_CATCH(const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        cpptrace::from_current_exception().print();
    }
}
