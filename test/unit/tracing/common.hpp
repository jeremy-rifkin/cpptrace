#ifndef TRACING_COMMON_HPP
#define TRACING_COMMON_HPP

#ifndef CPPTRACE_BUILD_NO_SYMBOLS
#define EXPECT_FILE(A, B) EXPECT_THAT((A), testing::EndsWith(B))
#define EXPECT_LINE(A, B) EXPECT_EQ((A), (B))
#else
#define EXPECT_FILE(A, B)
#define EXPECT_LINE(A, B)
#endif

#endif
