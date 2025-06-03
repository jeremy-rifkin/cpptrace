#ifndef CPPTRACE_FROM_CURRENT_MACROS_HPP
#define CPPTRACE_FROM_CURRENT_MACROS_HPP

// https://godbolt.org/z/7neGPEche
// gcc added support in 4.8 but I'm too lazy to check the minor version
#if defined(__GNUC__) && (__GNUC__ < 5)
 #define CPPTRACE_NORETURN __attribute__((noreturn))
#else
 #define CPPTRACE_NORETURN [[noreturn]]
#endif

#ifdef _MSC_VER
 #define CPPTRACE_TYPE_FOR(param) \
     ::cpptrace::detail::argument<void(param)>::type
 // this awful double-IILE is due to C2713 "You can't use structured exception handling (__try/__except) and C++
 // exception handling (try/catch) in the same function."
 #define CPPTRACE_TRY \
     try { \
         [&]() { \
             __try { \
                 [&]() {
 #define CPPTRACE_CATCH(param) \
                 }(); \
             } __except(::cpptrace::detail::exception_filter<CPPTRACE_TYPE_FOR(param)>(GetExceptionInformation())) {} \
         }(); \
     } catch(param)
#else
 #define CPPTRACE_UNWIND_INTERCEPTOR_FOR(param) \
     ::cpptrace::detail::unwind_interceptor_for<void(param)>
 #define CPPTRACE_TRY \
     try { \
         try {
 #define CPPTRACE_CATCH(param) \
         } catch(const CPPTRACE_UNWIND_INTERCEPTOR_FOR(param)&) { \
             ::cpptrace::detail::unreachable(); \
             /* force instantiation of the init-er */ \
             ::cpptrace::detail::nop(CPPTRACE_UNWIND_INTERCEPTOR_FOR(param)::init); \
         } \
     } catch(param)
#endif

#ifdef CPPTRACE_UNPREFIXED_TRY_CATCH
 #define TRY CPPTRACE_TRY
 #define CATCH(param) CPPTRACE_CATCH(param)
#endif

#endif // CPPTRACE_FROM_CURRENT_MACROS_HPP
