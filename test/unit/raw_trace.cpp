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

// This is fickle, however, it's the only way to do it really. It's reasonably reliable test in practice.

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



CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_2(
    cpptrace::frame_ptr parent_low_bound,
    cpptrace::frame_ptr parent_high_bound
) {
    auto raw_trace = cpptrace::generate_raw_trace();
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_2));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_2) + 90);
    EXPECT_GE(raw_trace.frames[1], parent_low_bound);
    EXPECT_LE(raw_trace.frames[1], parent_high_bound);
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_1() {
    auto raw_trace = cpptrace::generate_raw_trace();
    raw_trace_multi_2(reinterpret_cast<uintptr_t>(raw_trace_multi_1), reinterpret_cast<uintptr_t>(raw_trace_multi_1) + 300);
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_1));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(raw_trace_multi_1) + 90);
}

std::vector<std::pair<cpptrace::frame_ptr, cpptrace::frame_ptr>> parents;

CPPTRACE_FORCE_NO_INLINE void record_parent(cpptrace::frame_ptr low_bound, cpptrace::frame_ptr high_bound) {
    parents.insert(parents.begin(), {reinterpret_cast<uintptr_t>(low_bound), reinterpret_cast<uintptr_t>(high_bound)});
}

#ifndef _MSC_VER
CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_3() {
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

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_2() {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a)); // this frame
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
    for(size_t i = 0; i < parents.size(); i++) { // parent frames
        EXPECT_GE(raw_trace.frames[i + 1], parents[i].first);
        EXPECT_LE(raw_trace.frames[i + 1], parents[i].second);
    }
    record_parent(reinterpret_cast<uintptr_t>(&&c), reinterpret_cast<uintptr_t>(&&d));
    c:
    raw_trace_multi_precise_3();
    d:;
}

CPPTRACE_FORCE_NO_INLINE void raw_trace_multi_precise_1() {
    a:
    auto raw_trace = cpptrace::generate_raw_trace();
    b:
    EXPECT_GE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&a));
    EXPECT_LE(raw_trace.frames[0], reinterpret_cast<uintptr_t>(&&b));
    record_parent(reinterpret_cast<uintptr_t>(&&c), reinterpret_cast<uintptr_t>(&&d));
    c:
    raw_trace_multi_precise_2();
    d:;
}
#endif

TEST(RawTrace, MultipleCalls) {
    parents.clear();
    raw_trace_multi_1();
    #ifndef _MSC_VER
    raw_trace_multi_precise_1();
    #endif
}
