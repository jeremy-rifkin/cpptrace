#include <cpptrace/cpptrace.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

std::string normalize_filename(std::string name) {
    if(name.find('/') == 0 || (name.find(':') == 1 && std::isupper(name[0]))) {
        // build/test if the file is really an object name resolved by libdl
        auto p = std::min({name.rfind("test/"), name.rfind("test\\"), name.rfind("build/test")});
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

int x;

void foo(int n) {
    if(n == 0) {
        x = 0;
        trace();
        x = 0;
    } else {
        x = 0;
        foo(n - 1);
        x = 0;
    }
}

template<typename... Args>
void foo(int x, Args... args) {
    x = 0;
    foo(args...);
    x = 0;
}

void function_two(int, float) {
    x = 0;
    foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    x = 0;
}

void function_one(int) {
    x = 0;
    function_two(0, 0);
    x = 0;
}

int main() {
    x = 0;
    function_one(0);
    x = 0;
}
