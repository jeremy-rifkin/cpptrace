#ifdef LIBCPP_BACKTRACE_PATH
#include LIBCPP_BACKTRACE_PATH
#else
#include <backtrace.h>
#endif

int main() {
    backtrace_state* state = backtrace_create_state(nullptr, true, nullptr, nullptr);
}
