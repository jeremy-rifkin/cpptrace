#include <algorithm>
#include <string_view>
#include <string>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

using namespace std::literals;


// NOTE: returning something and then return stacktrace_from_current_3(line_numbers) * 2; later helps prevent the call from
// being optimized to a jmp
CPPTRACE_FORCE_NO_INLINE int stacktrace_from_current_z_3(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    throw std::runtime_error("foobar");
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_from_current_z_2(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_from_current_z_3(line_numbers) * 2;
}

CPPTRACE_FORCE_NO_INLINE int stacktrace_from_current_z_1(std::vector<int>& line_numbers) {
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return stacktrace_from_current_z_2(line_numbers) * 2;
}

TEST(FromCurrentZ, Basic) {
    std::vector<int> line_numbers;
    CPPTRACE_TRYZ {
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        stacktrace_from_current_z_1(line_numbers);
    } CPPTRACE_CATCHZ(const std::runtime_error& e) {
        EXPECT_EQ(e.what(), "foobar"sv);
        const auto& trace = cpptrace::from_current_exception();
        ASSERT_GE(trace.frames.size(), 4);
        auto it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.filename.find("from_current_z.cpp") != std::string::npos
                    && frame.symbol.find("lambda") == std::string::npos; // due to msvc
            }
        );
        ASSERT_NE(it, trace.frames.end());
        size_t i = static_cast<size_t>(it - trace.frames.begin());
        int j = 0;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("from_current_z.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_z_3"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("from_current_z.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_z_2"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("from_current_z.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("stacktrace_from_current_z_1"));
        i++;
        j++;
        ASSERT_LT(i, trace.frames.size());
        ASSERT_LT(j, line_numbers.size());
        EXPECT_THAT(trace.frames[i].filename, testing::EndsWith("from_current_z.cpp"));
        EXPECT_EQ(trace.frames[i].line.value(), line_numbers[j]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("FromCurrentZ_Basic_Test::TestBody"));
    }
}

TEST(FromCurrentZ, CorrectHandler) {
    std::vector<int> line_numbers;
    CPPTRACE_TRYZ {
        CPPTRACE_TRYZ {
            line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
            stacktrace_from_current_z_1(line_numbers);
        } CPPTRACE_CATCHZ(const std::logic_error&) {
            FAIL();
        }
    } CPPTRACE_CATCHZ(const std::exception& e) {
        EXPECT_EQ(e.what(), "foobar"sv);
        const auto& trace = cpptrace::from_current_exception();
        auto it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.filename.find("from_current_z.cpp") != std::string::npos
                    && frame.symbol.find("lambda") == std::string::npos;
            }
        );
        EXPECT_NE(it, trace.frames.end());
        it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.symbol.find("FromCurrentZ_CorrectHandler_Test::TestBody") != std::string::npos;
            }
        );
        EXPECT_NE(it, trace.frames.end());
    }
}

TEST(FromCurrentZ, RawTrace) {
    std::vector<int> line_numbers;
    CPPTRACE_TRYZ {
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        stacktrace_from_current_z_1(line_numbers);
    } CPPTRACE_CATCHZ(const std::exception& e) {
        EXPECT_EQ(e.what(), "foobar"sv);
        const auto& raw_trace = cpptrace::raw_trace_from_current_exception();
        auto trace = raw_trace.resolve();
        auto it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.filename.find("from_current_z.cpp") != std::string::npos
                    && frame.symbol.find("lambda") == std::string::npos;
            }
        );
        EXPECT_NE(it, trace.frames.end());
        it = std::find_if(
            trace.frames.begin(),
            trace.frames.end(),
            [](const cpptrace::stacktrace_frame& frame) {
                return frame.symbol.find("FromCurrentZ_RawTrace_Test::TestBody") != std::string::npos;
            }
        );
        EXPECT_NE(it, trace.frames.end());
    }
}
