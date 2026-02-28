#include <vector>

#include <gtest/gtest.h>
#include <gtest/gtest-matchers.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include "common.hpp"

#ifdef TEST_MODULE
import cpptrace;
#else
#include <cpptrace/cpptrace.hpp>
#endif


#ifdef _MSC_VER
 #define CPPTRACE_FORCE_INLINE [[msvc::flatten]]
#else
 #define CPPTRACE_FORCE_INLINE [[gnu::always_inline]] static
#endif


namespace ns_test {
    CPPTRACE_FORCE_NO_INLINE void namespaced_func() {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        auto line = __LINE__ + 1;
        auto trace = cpptrace::generate_trace();
        ASSERT_GE(trace.frames.size(), 1);
        EXPECT_FILE(trace.frames[0].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[0].line.value(), line);
        EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("ns_test::namespaced_func"));
    }
}

TEST(NamespaceResolution, BasicNamespace) {
    ns_test::namespaced_func();
}

namespace ns_outer { namespace ns_inner {
    CPPTRACE_FORCE_NO_INLINE void nested_ns_func() {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        auto line = __LINE__ + 1;
        auto trace = cpptrace::generate_trace();
        ASSERT_GE(trace.frames.size(), 1);
        EXPECT_FILE(trace.frames[0].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[0].line.value(), line);
        EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("ns_outer::ns_inner::nested_ns_func"));
    }
}}

TEST(NamespaceResolution, NestedNamespace) {
    ns_outer::ns_inner::nested_ns_func();
}

struct TestStruct {
    CPPTRACE_FORCE_NO_INLINE static void struct_method() {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        auto line = __LINE__ + 1;
        auto trace = cpptrace::generate_trace();
        ASSERT_GE(trace.frames.size(), 1);
        EXPECT_FILE(trace.frames[0].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[0].line.value(), line);
        EXPECT_THAT(trace.frames[0].symbol, testing::HasSubstr("TestStruct::struct_method"));
    }
};

TEST(NamespaceResolution, StructMethod) {
    TestStruct::struct_method();
}


#if defined(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF) && !defined(CPPTRACE_BUILD_NO_SYMBOLS)
namespace ns_inline_test {
    CPPTRACE_FORCE_NO_INLINE int ns_inline_leaf(std::vector<int>& line_numbers) {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        auto trace = cpptrace::generate_trace();
        if(trace.frames.size() < 4) {
            ADD_FAILURE() << "trace.frames.size() >= 4";
            return 2;
        }
        int i = 0;
        EXPECT_FILE(trace.frames[i].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("ns_inline_test::ns_inline_leaf"));
        EXPECT_FALSE(trace.frames[i].is_inline);
        i++;
        EXPECT_FILE(trace.frames[i].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("ns_inline_test::ns_inline_caller"));
        EXPECT_TRUE(trace.frames[i].is_inline);
        i++;
        EXPECT_FILE(trace.frames[i].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(trace.frames[i].symbol, testing::HasSubstr("ns_inline_entry"));
        EXPECT_FALSE(trace.frames[i].is_inline);
        i++;
        EXPECT_FILE(trace.frames[i].filename, "namespace_resolution.cpp");
        EXPECT_LINE(trace.frames[i].line.value(), line_numbers[i]);
        EXPECT_THAT(
            trace.frames[i].symbol,
            testing::HasSubstr("NamespaceResolution_InlinedInNamespace_Test")
        );
        return 2;
    }

    CPPTRACE_FORCE_INLINE int ns_inline_caller(std::vector<int>& line_numbers) {
        static volatile int lto_guard; lto_guard = lto_guard + 1;
        line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
        return ns_inline_leaf(line_numbers) * rand();
    }
}

CPPTRACE_FORCE_NO_INLINE int ns_inline_entry(std::vector<int>& line_numbers) {
    static volatile int lto_guard; lto_guard = lto_guard + 1;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    return ns_inline_test::ns_inline_caller(line_numbers) * rand();
}

TEST(NamespaceResolution, InlinedInNamespace) {
    std::vector<int> line_numbers;
    line_numbers.insert(line_numbers.begin(), __LINE__ + 1);
    ns_inline_entry(line_numbers);
}
#endif
