#include <backtrace.h>

int main() {
    backtrace_state* state = backtrace_create_state(nullptr, true, nullptr, nullptr);
}
