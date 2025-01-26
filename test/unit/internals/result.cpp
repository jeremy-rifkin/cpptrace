#include <gtest/gtest.h>

#include "utils/result.hpp"

using namespace cpptrace::detail;

// A simple custom error type that behaves like a standard exception.
struct error {
    int x;
    const char* what() const {
        return "error...";
    }
};

class ResultFixture : public testing::Test {
public:
    ResultFixture() {
        cpptrace::absorb_trace_exceptions(true);
    }

    ~ResultFixture() override {
        cpptrace::absorb_trace_exceptions(false);
    }
};

TEST_F(ResultFixture, ConstructWithValueRValue) {
    cpptrace::detail::Result<std::string, error> result("test");
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.is_error());
    EXPECT_TRUE(static_cast<bool>(result));

    EXPECT_EQ(result.unwrap_value(), "test");
    EXPECT_FALSE(result.error().has_value());
}

TEST_F(ResultFixture, ConstructWithValueLValue) {
    std::string s = "test";
    cpptrace::detail::Result<std::string, error> result(s);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(result.unwrap_value(), "test");

    s = "x";
    EXPECT_EQ(result.unwrap_value(), "test");
}

TEST_F(ResultFixture, ConstructWithErrorRValue) {
    cpptrace::detail::Result<std::string, error> result(error{1});
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.is_error());
    EXPECT_FALSE(static_cast<bool>(result));

    EXPECT_EQ(result.unwrap_error().x, 1);

    // Check that value() returns nullopt in this scenario
    EXPECT_FALSE(result.value().has_value());
}

TEST_F(ResultFixture, ConstructWithErrorLValue) {
    error e{1};
    cpptrace::detail::Result<std::string, error> result(e);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.unwrap_error().x, 1);
}

TEST_F(ResultFixture, MoveConstructorValue) {
    cpptrace::detail::Result<std::string, error> original(std::string("move"));
    cpptrace::detail::Result<std::string, error> moved(std::move(original));

    EXPECT_TRUE(moved.has_value());
    EXPECT_EQ(moved.unwrap_value(), "move");
    EXPECT_TRUE(original.has_value());
}

TEST_F(ResultFixture, MoveConstructorError) {
    cpptrace::detail::Result<std::string, error> original(error{1});
    cpptrace::detail::Result<std::string, error> moved(std::move(original));

    EXPECT_TRUE(moved.is_error());
    EXPECT_EQ(moved.unwrap_error().x, 1);
    EXPECT_TRUE(original.is_error());
}

TEST_F(ResultFixture, ValueOr) {
    {
        cpptrace::detail::Result<int, error> res_with_value(42);
        EXPECT_EQ(res_with_value.value_or(-1), 42);
        EXPECT_EQ(std::move(res_with_value).value_or(-1), 42);
    }
    {
        cpptrace::detail::Result<int, error> res_with_error(error{});
        EXPECT_EQ(res_with_error.value_or(-1), -1);
        EXPECT_EQ(std::move(res_with_error).value_or(-1), -1);
    }
}
