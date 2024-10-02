#include <string_view>
#include <string>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include <cpptrace/cpptrace.hpp>

using namespace std::literals;


// NOTE: returning something and then return stacktrace_traced_object_3(line_numbers) * 2; later helps prevent the call from
// being optimized to a jmp
CPPTRACE_FORCE_NO_INLINE int stacktrace_traced_object_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    throw cpptrace::runtime_error("foobar");
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_traced_object_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_traced_object_3(line_numbers) * 2;
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_traced_object_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_traced_object_2(line_numbers) * 2;
}

TEST(TracedException, Basic) {
    std::vector<int> line_numbers;
    try {
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        stacktrace_traced_object_1(line_numbers);
    } catch(cpptrace::exception& e) {
        EXPECT_EQ(e.message(), "foobar"sv);
        const auto& trace = e.trace();
        ASSERT_GE(trace.frames.size(), 4);
        size_t i = 0;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(i, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("traced_exception.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_traced_object_3"));
        i++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(i, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("traced_exception.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_traced_object_2"));
        i++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(i, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("traced_exception.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_traced_object_1"));
        i++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(i, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("traced_exception.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("TracedException_Basic_Test::TestBody"));
    }
}
