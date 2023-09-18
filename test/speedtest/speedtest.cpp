// https://github.com/jeremy-rifkin/libassert/issues/43

#include <cpptrace/cpptrace.hpp>

#include <gtest/gtest.h>

#include <exception>

TEST(TraceTest, trace_test) {
    ASSERT_THROW((cpptrace::print_trace(), false), std::logic_error);
}
