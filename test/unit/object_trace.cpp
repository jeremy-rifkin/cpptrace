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



TEST(ObjectTrace, Empty) {
    cpptrace::object_trace empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.resolve().to_string(), "Stack trace (most recent call first):\n<empty trace>\n");
}



CPPTRACE_FORCE_NO_INLINE void object_basic() {
    auto trace = cpptrace::generate_object_trace();
    EXPECT_FALSE(trace.empty());
    EXPECT_NE(trace.frames[0].raw_address, 0);
    EXPECT_NE(trace.frames[0].object_address, 0);
    EXPECT_THAT(trace.frames[0].object_path, testing::HasSubstr("unittest"));
}

TEST(ObjectTrace, Basic) {
    object_basic();
}



CPPTRACE_FORCE_NO_INLINE void object_basic_resolution() {
    auto line = __LINE__ + 1;
    auto trace = cpptrace::generate_object_trace().resolve();
    EXPECT_THAT(trace.frames[0].filename, testing::EndsWith("object_trace.cpp"));
    EXPECT_EQ(trace.frames[0].line.value(), line);
    EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("object_basic_resolution"));
}

TEST(ObjectTrace, BasicResolution) {
    object_basic();
}


// TODO: dbghelp uses raw address, not object
#ifndef _MSC_VER
CPPTRACE_FORCE_NO_INLINE int object_resolve_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    auto dummy = cpptrace::generate_trace();
    auto dummy_otrace = cpptrace::generate_object_trace();
    cpptrace::object_trace otrace;
    otrace.frames.push_back(
        cpptrace::object_frame{0, dummy.frames[0].object_address, dummy_otrace.frames[0].object_path}
    );
    otrace.frames.push_back(
        cpptrace::object_frame{0, dummy.frames[1].object_address, dummy_otrace.frames[1].object_path}
    );
    otrace.frames.push_back(
        cpptrace::object_frame{0, dummy.frames[2].object_address, dummy_otrace.frames[2].object_path}
    );
    otrace.frames.push_back(
        cpptrace::object_frame{0, dummy.frames[3].object_address, dummy_otrace.frames[3].object_path}
    );
    auto trace = otrace.resolve();
    int i = 0;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("object_trace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("object_resolve_3"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("object_trace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("object_resolve_2"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("object_trace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("object_resolve_1"));
    i++;
    EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("object_trace.cpp"));
    EXPECT_EQ(trace.frames[i].line.value(), line_numbers[i]);
    EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("ObjectTrace_Resolution_Test::TestBody"));
    return 2;
}

CPPTRACE_FORCE_NO_INLINE int object_resolve_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return object_resolve_3(line_numbers) * 2;
}

CPPTRACE_FORCE_NO_INLINE int object_resolve_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return object_resolve_2(line_numbers) * 2;
}

TEST(ObjectTrace, Resolution) {
    std::vector<int> line_numbers;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    object_resolve_1(line_numbers);
}
#endif
