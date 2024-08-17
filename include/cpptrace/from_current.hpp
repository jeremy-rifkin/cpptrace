#ifndef CPPTRACE_FROM_CURRENT_HPP
#define CPPTRACE_FROM_CURRENT_HPP

#include <cpptrace/cpptrace.hpp>

namespace cpptrace {
    CPPTRACE_EXPORT const raw_trace& raw_trace_from_current_exception();
    CPPTRACE_EXPORT const stacktrace& from_current_exception();

    namespace detail {
        #ifdef _MSC_VER
         CPPTRACE_EXPORT CPPTRACE_FORCE_NO_INLINE int exception_filter();
        #else
         class CPPTRACE_EXPORT unwind_interceptor {
         public:
             virtual ~unwind_interceptor();
         };

         CPPTRACE_EXPORT void do_prepare_unwind_interceptor();

         #ifndef CPPTRACE_DONT_PREPARE_UNWIND_INTERCEPTOR_ON
          __attribute__((constructor)) inline void prepare_unwind_interceptor() {
              // __attribute__((constructor)) inline functions can be called for every source file they're #included in
              // there is still only one copy of the inline function in the final executable, though
              // LTO can make the redundant constructs fire only once
              // do_prepare_unwind_interceptor prevents against multiple preparations however it makes sense to guard
              // against it here too as a fast path, not that this should matter for performance
              static bool did_prepare = false;
              if(!did_prepare) {
                 do_prepare_unwind_interceptor();
                 did_prepare = true;
              }
          }
         #endif
        #endif
    }
}

#ifdef _MSC_VER
 #define CPPTRACE_TRY \
     try { \
         [&]() { \
             __try
 #define CPPTRACE_CATCH(param) \
             __except(::cpptrace::detail::exception_filter()) {} \
         }(); \
     } catch(param)
#else
 #define CPPTRACE_TRY \
     try { \
         try
 #define CPPTRACE_CATCH(param) \
         catch(::cpptrace::detail::unwind_interceptor&) {} \
     } catch(param)
#endif

#endif
