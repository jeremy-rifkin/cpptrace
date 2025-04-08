#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include "utils/span.hpp"

using cpptrace::detail::span;
using cpptrace::detail::make_span;
using cpptrace::detail::make_bspan;

namespace {

TEST(SpanTest, Basic) {
    std::array<int, 5> arr{1, 2, 3, 4, 5};
    auto span = make_span(arr.begin(), arr.end());
    EXPECT_EQ(span.data(), arr.data());
    EXPECT_EQ(span.size(), 5);
    EXPECT_EQ(span.data()[0], 1);
    EXPECT_EQ(span.data()[1], 2);
    EXPECT_EQ(span.data()[2], 3);
    EXPECT_EQ(span.data()[3], 4);
    EXPECT_EQ(span.data()[4], 5);
}

TEST(SpanTest, PtrSize) {
    std::array<int, 5> arr{1, 2, 3, 4, 5};
    auto span = make_span(arr.begin(), arr.size());
    EXPECT_EQ(span.data(), arr.data());
    EXPECT_EQ(span.size(), 5);
    EXPECT_EQ(span.data()[0], 1);
    EXPECT_EQ(span.data()[1], 2);
    EXPECT_EQ(span.data()[2], 3);
    EXPECT_EQ(span.data()[3], 4);
    EXPECT_EQ(span.data()[4], 5);
}

TEST(SpanTest, Bspan) {
    struct S {
        std::array<char, 4> data;
    };
    S s{{'a', 'b', 'c', 'd'}};
    auto span = make_bspan(s);
    EXPECT_EQ(span.data(), s.data.data());
    EXPECT_EQ(span.size(), 4);
    EXPECT_EQ(span.data()[0], 'a');
    EXPECT_EQ(span.data()[1], 'b');
    EXPECT_EQ(span.data()[2], 'c');
    EXPECT_EQ(span.data()[3], 'd');
}

}
