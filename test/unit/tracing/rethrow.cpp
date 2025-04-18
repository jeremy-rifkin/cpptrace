#include <algorithm>
#include <exception>
#include <string_view>
#include <string>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

#include "common.hpp"

using namespace std::literals;


static volatile int truthy = 2;

// NOTE: returning something and then return stacktrace_multi_3(line_numbers) * rand(); is done to prevent TCO even
// under LTO https://github.com/jeremy-rifkin/cpptrace/issues/179#issuecomment-2467302052
CPPTRACE_FORCE_NO_INLINE int stacktrace_from_current_rethrow_3(std::vector<int>& line_numbers) {
    static volatile int lto_guard; lto_guard = lto_guard + 1;
    if(truthy) { // due to a MSVC warning about unreachable code
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        throw std::runtime_error("foobar");
    }
    return 2;
}

CPPTRACE_FORCE_NO_INLINE
int stacktrace_from_current_rethrow_2(std::vector<int>& line_numbers, std::vector<int>& rethrow_line_numbers) {
    CPPTRACE_TRY {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        return stacktrace_from_current_rethrow_3(line_numbers) * rand();
    } CPPTRACE_CATCH(const std::exception& e) {
        rethrow_line_numbers.insert(rethrow_line_numbers.begin(), __LINE__ + 1);
        cpptrace::rethrow();
    }
}

CPPTRACE_FORCE_NO_INLINE
int stacktrace_from_current_rethrow_1(std::vector<int>& line_numbers, std::vector<int>& rethrow_line_numbers) {
    static volatile int lto_guard; lto_guard = lto_guard + 1;
    rethrow_line_numbers.insert(rethrow_line_numbers.begin(), __LINE__ + 2);
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_from_current_rethrow_2(line_numbers, rethrow_line_numbers) * rand();
}

TEST(Rethrow, RethrowPreservesTrace) {
    std::vector<int> line_numbers;
    std::vector<int> rethrow_line_numbers;
    CPPTRACE_TRY {
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        static volatile int tco_guard = stacktrace_from_current_rethrow_1(line_numbers, rethrow_line_numbers);
        (void)tco_guard;
    } CPPTRACE_CATCH(const std::runtime_error& e) {
        EXPECT_EQ(e.what(), "foobar"sv);
        const auto& trace = cpptrace::from_current_exception();
        ASSERT_GE(trace.frames.size(), 4);
        auto it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.symbol.find("stacktrace_from_current_rethrow_3") != std::string::npos;
            }
        );
        ASSERT_NE(it, trace.frames.end());
        size_t i = static_cast<size_t>(it - trace.frames.begin());
        int j = 0;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_FILE(trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_rethrow_3"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_FILE(trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_rethrow_2"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_FILE(trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_rethrow_1"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_FILE(trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("Rethrow_RethrowPreservesTrace_Test::TestBody"));
    }
}

TEST(Rethrow, RethrowTraceCorrect) {
    std::vector<int> line_numbers;
    std::vector<int> rethrow_line_numbers;
    CPPTRACE_TRY {
        rethrow_line_numbers.insert(rethrow_line_numbers.begin(), __LINE__ + 2);
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        static volatile int tco_guard = stacktrace_from_current_rethrow_1(line_numbers, rethrow_line_numbers);
        (void)tco_guard;
    } CPPTRACE_CATCH(const std::runtime_error& e) {
        EXPECT_EQ(e.what(), "foobar"sv);
        const auto& rethrow_trace = cpptrace::from_current_exception_last_throw_point();
        ASSERT_GE(rethrow_trace.frames.size(), 4);
        auto it = std::find_if(
            rethrow_trace.frames.begin(),
            rethrow_trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.symbol.find("stacktrace_from_current_rethrow_2") != std::string::npos;
            }
        );
        ASSERT_NE(it, rethrow_trace.frames.end());
        size_t i = static_cast<size_t>(it - rethrow_trace.frames.begin());
        int j = 0;
        ASSERT_LT(i, rethrow_trace.frames.size());
        ASSERT_LT(j, rethrow_line_numbers.size());
        EXPECT_FILE(rethrow_trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(rethrow_trace.frames[i].line.value(), rethrow_line_numbers[j]);
        EXPECT_THAT(rethrow_trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_rethrow_2"));
        i++;
        j++;
        ASSERT_LT(i, rethrow_trace.frames.size());
        ASSERT_LT(j, rethrow_line_numbers.size());
        EXPECT_FILE(rethrow_trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(rethrow_trace.frames[i].line.value(), rethrow_line_numbers[j]);
        EXPECT_THAT(rethrow_trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_rethrow_1"));
        i++;
        j++;
        ASSERT_LT(i, rethrow_trace.frames.size());
        ASSERT_LT(j, rethrow_line_numbers.size());
        EXPECT_FILE(rethrow_trace.frames[i].filename, "rethrow.cpp");
        EXPECT_LINE(rethrow_trace.frames[i].line.value(), rethrow_line_numbers[j]);
        EXPECT_THAT(rethrow_trace.frames[i].symbol, testing::HasSubstr("Rethrow_RethrowTraceCorrect_Test::TestBody"));
    }
}
