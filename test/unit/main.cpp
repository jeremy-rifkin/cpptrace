#include <gtest/gtest.h>
#include <cpptrace/cpptrace.hpp>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    cpptrace::absorb_trace_exceptions(false);
    cpptrace::use_default_stderr_logger();
    return RUN_ALL_TESTS();
}
