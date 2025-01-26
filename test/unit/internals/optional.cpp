#include <gtest/gtest.h>

#include "utils/optional.hpp"

using namespace cpptrace::detail;

TEST(OptionalTest, DefaultConstructor) {
    optional<int> o;
    EXPECT_FALSE(o.has_value());
    EXPECT_FALSE(static_cast<bool>(o));
}

TEST(OptionalTest, ConstructWithNullopt) {
    optional<int> o(nullopt);
    EXPECT_FALSE(o.has_value());
}

TEST(OptionalTest, ValueConstructor) {
    optional<int> o(42);
    EXPECT_TRUE(o.has_value());
    EXPECT_EQ(o.unwrap(), 42);

    int x = 100;
    optional<int> o2(x);
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o2.unwrap(), 100);
}

TEST(OptionalTest, CopyConstructor) {
    optional<int> o1(42);
    optional<int> o2(o1);
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o2.unwrap(), 42);

    optional<int> o3(nullopt);
    optional<int> o4(o3);
    EXPECT_FALSE(o4.has_value());
}

TEST(OptionalTest, MoveConstructor) {
    optional<int> o1(42);
    optional<int> o2(std::move(o1));
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o2.unwrap(), 42);

    optional<int> o3(nullopt);
    optional<int> o4(std::move(o3));
    EXPECT_FALSE(o4.has_value());
}

TEST(OptionalTest, CopyAssignmentOperator) {
    optional<int> o1(42);
    optional<int> o2;
    o2 = o1;
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o2.unwrap(), 42);

    optional<int> o3(nullopt);
    optional<int> o4(100);
    o4 = o3;
    EXPECT_FALSE(o4.has_value());
}

TEST(OptionalTest, MoveAssignmentOperator) {
    optional<int> o1(42);
    optional<int> o2;
    o2 = std::move(o1);
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o2.unwrap(), 42);

    optional<int> o3(nullopt);
    optional<int> o4(99);
    o4 = std::move(o3);
    EXPECT_FALSE(o4.has_value());
}

TEST(OptionalTest, AssignmentFromValue) {
    optional<int> o;
    o = 123;
    EXPECT_TRUE(o.has_value());
    EXPECT_EQ(o.unwrap(), 123);

    o = nullopt;
    EXPECT_FALSE(o.has_value());
}

TEST(OptionalTest, Reset) {
    optional<int> o(42);
    EXPECT_TRUE(o.has_value());
    o.reset();
    EXPECT_FALSE(o.has_value());
}

TEST(OptionalTest, Swap) {
    optional<int> o1(42);
    optional<int> o2(100);

    o1.swap(o2);
    EXPECT_TRUE(o1.has_value());
    EXPECT_TRUE(o2.has_value());
    EXPECT_EQ(o1.unwrap(), 100);
    EXPECT_EQ(o2.unwrap(), 42);

    // Swap a value-holding optional with an empty optional
    optional<int> o3(7);
    optional<int> o4(nullopt);
    o3.swap(o4);
    EXPECT_FALSE(o3.has_value());
    EXPECT_TRUE(o4.has_value());
    EXPECT_EQ(o4.unwrap(), 7);
}

TEST(OptionalTest, ValueOr) {
    optional<int> o1(42);
    EXPECT_EQ(o1.value_or(100), 42);

    optional<int> o2(nullopt);
    EXPECT_EQ(o2.value_or(100), 100);
}
