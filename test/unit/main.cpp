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

#ifndef _MSC_VER
__attribute__((constructor(101)))
void do_init() {
    init();
}
#endif

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    #ifdef _MSC_VER
     init();
    #endif
    return RUN_ALL_TESTS();
}
