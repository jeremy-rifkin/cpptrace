#include <gtest/gtest.h>

#ifdef TEST_MODULE
import cpptrace;
#else
#include <cpptrace/cpptrace.hpp>
#endif

void init() {
    cpptrace::absorb_trace_exceptions(false);
    cpptrace::use_default_stderr_logger();
}

#if !defined(_MSC_VER) && !defined(__APPLE__)
#define CONSTRUCTOR_INIT 1
__attribute__((constructor(101)))
void do_init() {
    init();
}
#else
#define CONSTRUCTOR_INIT 0
#endif

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    #if !CONSTRUCTOR_INIT
     init();
    #endif
    return RUN_ALL_TESTS();
}
