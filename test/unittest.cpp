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

// Raw trace tests

// This is fickle, however, it's the only way to do it really. It's a reliable test in practice.

CPPTRACE_FORCE_NO_INLINE void raw_trace_basic() {
    auto raw_trace = cpptrace::generate_raw_trace();
    // look for within 90 bytes of the start of the function
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_basic));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_basic) + 90);
}

#ifndef _MSC_VER
CPPTRACE_FORCE_NO_INLINE void raw_trace_basic_precise() {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    // look for within 30 bytes of the start of the function
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
}
#endif

TEST(RawTrace, Basic) {
    raw_trace_basic();
    #ifndef _MSC_VER
    raw_trace_basic_precise();
    #endif
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_1(std::pair<cpptrace::frame_ptr, cpptrace::frame_ptr> parent) {
    auto raw_trace = cpptrace::generate_raw_trace();
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_1));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_1) + 90);
    EXPECT_GE(raw_trace.frames[1], parent.first);
    EXPECT_LE(raw_trace.frames[1], parent.second);
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_top() {
    auto raw_trace = cpptrace::generate_raw_trace();
    raw_trace_multi_1({reinterpret_cast<uintptr_t>(raw_trace_multi_top), reinterpret_cast<uintptr_t>(raw_trace_multi_top) + 300});
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_top));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_top) + 90);
}

#ifndef _MSC_VER
CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_2(std::vector<std::pair<cpptrace::frame_ptr, cpptrace::frame_ptr>>& parents) {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a)); // this frame
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
    for(size_t i = 0; i < parents.size(); i++) { // parent frames
        EXPECT_GE(raw_trace.frames[i + 1], parents[i].first);
        EXPECT_LE(raw_trace.frames[i + 1], parents[i].second);
    }
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_1(std::vector<std::pair<cpptrace::frame_ptr, cpptrace::frame_ptr>>& parents) {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a)); // this frame
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
    for(size_t i = 0; i < parents.size(); i++) { // parent frames
        EXPECT_GE(raw_trace.frames[i + 1], parents[i].first);
        EXPECT_LE(raw_trace.frames[i + 1], parents[i].second);
    }
    parents.insert(parents.begin(), {reinterpret_cast<uintptr_t>(&&c), reinterpret_cast<uintptr_t>(&&d)});
    c:
    raw_trace_multi_precise_2(parents);
    d:;
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_top() {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
    std::vector<std::pair<cpptrace::frame_ptr, cpptrace::frame_ptr>> parents;
    parents.insert(parents.begin(), {reinterpret_cast<uintptr_t>(&&c), reinterpret_cast<uintptr_t>(&&d)});
    c:
    raw_trace_multi_precise_1(parents);
    d:;
}
#endif

TEST(RawTrace, MultipleCalls) {
    raw_trace_multi_top();
    #ifndef _MSC_VER
    raw_trace_multi_precise_top();
    #endif
}

CPPTRACE_FORCE_NO_INLINE void stacktrace_basic() {
    auto line = __LINE__ + 1;
    auto trace = cpptrace::generate_trace();
    EXPECT_THAT(trace.frames[0].filename, testing::EndsWith("unittest.cpp"));
    EXPECT_EQ(trace.frames[0].line.value(), line);
    EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("stacktrace_basic"));
}

TEST(Stacktrace, Basic) {
    stacktrace_basic();
}
