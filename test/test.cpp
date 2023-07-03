#include <cpptrace/cpptrace.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

std::string normalize_filename(std::string name) {
    if(name.find('/') == 0 || (name.find(':') == 1 && std::isupper(name[0]))) {
        auto p = std::min(name.rfind("test/"), name.rfind("test\\"));
        return p == std::string::npos ? name : name.substr(p);
    } else {
        return name;
    }
}

void trace() {
    for(const auto& frame : cpptrace::generate_trace()) {
        std::cout
            << normalize_filename(frame.filename)
            << "||"
            << frame.line
            << "||"
            << frame.symbol
            << std::endl;
    }
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
