#ifndef CPPTRACE_FROM_CURRENT_HPP
#define CPPTRACE_FROM_CURRENT_HPP

#include <cpptrace/cpptrace.hpp>

#ifdef _MSC_VER
#include <windows.h>
#else
 #define CPPTRACE_LIBSTDCPP 0
 #define CPPTRACE_LIBCPP 0
 #if defined(__GLIBCXX__) || defined(__GLIBCPP__)
  #undef CPPTRACE_LIBSTDCPP
  #undef CPPTRACE_LIBCPP
  #define CPPTRACE_LIBSTDCPP 1
 #elif defined(_LIBCPP_VERSION)
  #undef CPPTRACE_LIBSTDCPP
  #undef CPPTRACE_LIBCPP
  #define CPPTRACE_LIBCPP 1
 #else
  #error "Cpptrace from_current: Unsupported C++ standard library"
 #endif
#endif

#include <iostream>

// #if defined(__clang__)
// // pass
// #elif defined(__GNUC__) || defined(__GNUG__)
// // pass
// #elif defined(_MSC_VER)
// // pass
// #else
// #error "Cpptrace from_current: Unsupported C++ compiler"
// #endif

namespace cpptrace {
    const raw_trace& raw_trace_from_current_exception();
    const stacktrace& from_current_exception();

    namespace detail {
        #ifdef _MSC_VER
         CPPTRACE_FORCE_NO_INLINE int exception_filter();
        #else
         class unwind_interceptor {
         public:
             virtual ~unwind_interceptor();
         };
        #endif
    }
}

#ifdef _MSC_VER
 #define CPPTRACE_TRY \
     try { \
         [&]() { \
             __try
 #define CPPTRACE_CATCH(param) \
             __except(::cpptrace::detail::exception_filter()) { puts("shouldn't be here"); } \
         }(); \
     } catch(param)
#else
 #define CPPTRACE_TRY \
     try { \
         try
 #define CPPTRACE_CATCH(param) \
         catch(::cpptrace::detail::unwind_interceptor&) { puts("shouldn't be here"); } \
     } catch(param)
#endif

#endif
