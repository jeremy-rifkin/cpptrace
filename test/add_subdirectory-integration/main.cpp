#include <cpptrace/cpptrace.hpp>

void trace() {
    cpptrace::print_trace();
}

void foo(int) {
    trace();
}

int main() {
    foo(0);
}
