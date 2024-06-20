#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <string>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <cpptrace/cpptrace.hpp>

using namespace std::literals;

#ifdef _MSC_VER
 #define CPPTRACE_FORCE_INLINE [[msvc::flatten]]
#else
 #define CPPTRACE_FORCE_INLINE [[gnu::always_inline]]
#endif



TEST(Stacktrace, Empty) {
    cpptrace::stacktrace empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.to_string(), "Stack trace (most recent call first):\n<empty trace>\n");
}



CPPTRACE_FORCE_NO_INLINE void stacktrace_basic() {
    auto line = __LINE__ + 1;
    auto trace = cpptrace::generate_trace();
    EXPECT_THAT(trace.frames[0].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[0].line.value(), line);
    EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("stacktrace_basic"));
}

TEST(Stacktrace, Basic) {
    stacktrace_basic();
}



// NOTE: returning something and then return stacktrace_multi_3(line_numbers) * 2; later helps prevent the call from
// being optimized to a jmp
CPPTRACE_FORCE_NO_INLINE int stacktrace_multi_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    auto trace = cpptrace::generate_trace();
    int i = 0;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_multi_3"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_multi_2"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_multi_1"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("Stacktrace_MultipleFrames_Test::TestBody"));
    return 2;
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_multi_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_multi_3(line_numbers) * 2;
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_multi_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_multi_2(line_numbers) * 2;
}

TEST(Stacktrace, MultipleFrames) {
    std::vector<int> line_numbers;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    stacktrace_multi_1(line_numbers);
}



CPPTRACE_FORCE_NO_INLINE cpptrace::raw_trace stacktrace_raw_resolve_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return cpptrace::generate_raw_trace();
}

CPPTRACE_FORCE_NO_INLINE cpptrace::raw_trace stacktrace_raw_resolve_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_raw_resolve_3(line_numbers);
}

CPPTRACE_FORCE_NO_INLINE cpptrace::raw_trace stacktrace_raw_resolve_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_raw_resolve_2(line_numbers);
}

TEST(Stacktrace, RawTraceResolution) {
    std::vector<int> line_numbers;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    auto raw = stacktrace_raw_resolve_1(line_numbers);
    auto trace = raw.resolve();
    int i = 0;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_raw_resolve_3"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_raw_resolve_2"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_raw_resolve_1"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("Stacktrace_RawTraceResolution_Test::TestBody"));
}



#ifndef _MSC_VER
CPPTRACE_FORCE_NO_INLINE int stacktrace_inline_resolution_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    auto trace = cpptrace::generate_trace();
    int i = 0;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_inline_resolution_3"));
    EXPECT_FALSE(trace.frames[i].is_inline);
    EXPECT_NE(trace.frames[i].raw_address, 0);
    EXPECT_NE(trace.frames[i].object_address, 0);
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_inline_resolution_2"));
    EXPECT_TRUE(trace.frames[i].is_inline);
    EXPECT_EQ(trace.frames[i].raw_address, 0);
    EXPECT_EQ(trace.frames[i].object_address, 0);
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_inline_resolution_1"));
    EXPECT_FALSE(trace.frames[i].is_inline);
    EXPECT_NE(trace.frames[i].raw_address, 0);
    EXPECT_NE(trace.frames[i].object_address, 0);
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("stacktrace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("Stacktrace_InlineResolution_Test::TestBody"));
    EXPECT_FALSE(trace.frames[i].is_inline);
    EXPECT_NE(trace.frames[i].raw_address, 0);
    EXPECT_NE(trace.frames[i].object_address, 0);
    return 2;
}

CPPTRACE_FORCE_INLINE int stacktrace_inline_resolution_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_inline_resolution_3(line_numbers) * 2;
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_inline_resolution_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_inline_resolution_2(line_numbers) * 2;
}

TEST(Stacktrace, InlineResolution) {
    std::vector<int> line_numbers;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    stacktrace_inline_resolution_1(line_numbers);
}
#endif
