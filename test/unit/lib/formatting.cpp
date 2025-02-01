#include <cpptrace/formatting.hpp>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include "utils/microfmt.hpp"
#include "utils/utils.hpp"

using cpptrace::detail::split;
using testing::ElementsAre;

namespace {

cpptrace::stacktrace make_test_stacktrace() {
    cpptrace::stacktrace trace;
    trace.frames.push_back({0x1, 0x1001, {20}, {30}, "foo.cpp", "foo()", false});
    trace.frames.push_back({0x2, 0x1002, {30}, {40}, "bar.cpp", "bar()", false});
    trace.frames.push_back({0x3, 0x1003, {40}, {25}, "foo.cpp", "main", false});
    return trace;
}

TEST(FormatterTest, Basic) {
    auto res = split(cpptrace::get_default_formatter().format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#1 0x0000000000000002 in bar() at bar.cpp:30:40",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, Inlines) {
    auto trace = make_test_stacktrace();
    trace.frames[1].is_inline = true;
    trace.frames[1].raw_address = 0;
    trace.frames[1].object_address = 0;
    auto res = split(cpptrace::get_default_formatter().format(trace), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#1 (inlined)          in bar() at bar.cpp:30:40",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, Header) {
    auto formatter = cpptrace::formatter{}
        .set_header("Stack trace:");
    auto res = split(formatter.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace:",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#1 0x0000000000000002 in bar() at bar.cpp:30:40",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, NoColumn) {
    auto formatter = cpptrace::formatter{}
        .include_column(false);
    auto res = split(formatter.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20",
            "#1 0x0000000000000002 in bar() at bar.cpp:30",
            "#2 0x0000000000000003 in main at foo.cpp:40"
        )
    );
}

TEST(FormatterTest, ObjectAddresses) {
    auto formatter = cpptrace::formatter{}
        .set_address_mode(cpptrace::formatter::address_mode::object);
    auto res = split(formatter.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000001001 in foo() at foo.cpp:20:30",
            "#1 0x0000000000001002 in bar() at bar.cpp:30:40",
            "#2 0x0000000000001003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, Snippets) {
    cpptrace::stacktrace trace;
    unsigned line = __LINE__ + 1;
    trace.frames.push_back({0x1, 0x1001, {line}, {20}, __FILE__, "foo()", false});
    trace.frames.push_back({0x2, 0x1002, {line + 1}, {20}, __FILE__, "foo()", false});
    auto formatter = cpptrace::formatter{}
        .set_snippets(true);
    auto res = split(formatter.format(trace), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            // frame 1
            cpptrace::microfmt::format("#0 0x0000000000000001 in foo() at {}:{}:20", __FILE__, line),
            cpptrace::microfmt::format("     {}:     cpptrace::stacktrace trace;", line - 2),
            cpptrace::microfmt::format("     {}:     unsigned line = __LINE__ + 1;", line - 1),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x1, 0x1001, {line}, {{20}}, __FILE__, \"foo()\", false});",
                line
            ),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x2, 0x1002, {line + 1}, {{20}}, __FILE__, \"foo()\", false});",
                line + 1
            ),
            cpptrace::microfmt::format("     {}:     auto formatter = cpptrace::formatter{{}}", line + 2),
            // frame 2
            cpptrace::microfmt::format("#1 0x0000000000000002 in foo() at {}:{}:20", __FILE__, line + 1),
            cpptrace::microfmt::format("     {}:     unsigned line = __LINE__ + 1;", line - 1),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x1, 0x1001, {line}, {{20}}, __FILE__, \"foo()\", false});",
                line
            ),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x2, 0x1002, {line + 1}, {{20}}, __FILE__, \"foo()\", false});",
                line + 1
            ),
            cpptrace::microfmt::format("     {}:     auto formatter = cpptrace::formatter{{}}", line + 2),
            cpptrace::microfmt::format("     {}:         .set_snippets(true);", line + 3)
        )
    );
    formatter.set_snippet_context(1);
    res = split(formatter.format(trace), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            // frame 1
            cpptrace::microfmt::format("#0 0x0000000000000001 in foo() at {}:{}:20", __FILE__, line),
            cpptrace::microfmt::format("     {}:     unsigned line = __LINE__ + 1;", line - 1),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x1, 0x1001, {line}, {{20}}, __FILE__, \"foo()\", false});",
                line
            ),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x2, 0x1002, {line + 1}, {{20}}, __FILE__, \"foo()\", false});",
                line + 1
            ),
            // frame 2
            cpptrace::microfmt::format("#1 0x0000000000000002 in foo() at {}:{}:20", __FILE__, line + 1),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x1, 0x1001, {line}, {{20}}, __FILE__, \"foo()\", false});",
                line
            ),
            cpptrace::microfmt::format(
                "     {}:     trace.frames.push_back({0x2, 0x1002, {line + 1}, {{20}}, __FILE__, \"foo()\", false});",
                line + 1
            ),
            cpptrace::microfmt::format("     {}:     auto formatter = cpptrace::formatter{{}}", line + 2)
        )
    );
}

TEST(FormatterTest, Colors) {
    auto formatter = cpptrace::formatter{}
        .set_color_mode(cpptrace::formatter::color_mode::always);
    auto res = split(formatter.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 \x1B[34m0x0000000000000001\x1B[0m in \x1B[33mfoo()\x1B[0m at \x1B[32mfoo.cpp\x1B[0m:\x1B[34m20\x1B[0m:\x1B[34m30\x1B[0m",
            "#1 \x1B[34m0x0000000000000002\x1B[0m in \x1B[33mbar()\x1B[0m at \x1B[32mbar.cpp\x1B[0m:\x1B[34m30\x1B[0m:\x1B[34m40\x1B[0m",
            "#2 \x1B[34m0x0000000000000003\x1B[0m in \x1B[33mmain\x1B[0m at \x1B[32mfoo.cpp\x1B[0m:\x1B[34m40\x1B[0m:\x1B[34m25\x1B[0m"
        )
    );
}

TEST(FormatterTest, Filtering) {
    auto formatter = cpptrace::formatter{}
        .set_filter([] (const cpptrace::stacktrace_frame& frame) -> bool {
            return frame.filename.find("foo.cpp") != std::string::npos;
        });
    auto res = split(formatter.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, MoveSemantics) {
    auto formatter = cpptrace::formatter{}
        .set_filter([] (const cpptrace::stacktrace_frame& frame) -> bool {
            return frame.filename.find("foo.cpp") != std::string::npos;
        });
    auto formatter2 = std::move(formatter);
    auto res = split(formatter2.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
    cpptrace::formatter formatter3;
    formatter3 = std::move(formatter);
    auto res2 = split(formatter2.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res2,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

TEST(FormatterTest, CopySemantics) {
    auto formatter = cpptrace::formatter{}
        .set_filter([] (const cpptrace::stacktrace_frame& frame) -> bool {
            return frame.filename.find("foo.cpp") != std::string::npos;
        });
    auto formatter2 = formatter;
    auto res = split(formatter2.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
    cpptrace::formatter formatter3;
    formatter3 = formatter;
    auto res2 = split(formatter2.format(make_test_stacktrace()), "\n");
    EXPECT_THAT(
        res2,
        ElementsAre(
            "Stack trace (most recent call first):",
            "#0 0x0000000000000001 in foo() at foo.cpp:20:30",
            "#2 0x0000000000000003 in main at foo.cpp:40:25"
        )
    );
}

}
