#ifndef CPPTRACE_FROM_CURRENT_HPP
#define CPPTRACE_FROM_CURRENT_HPP

#include <exception>
#include <typeinfo>

#include <cpptrace/basic.hpp>

// https://godbolt.org/z/4MsT6KqP1
#ifdef _MSC_VER
 #define CPPTRACE_UNREACHABLE() __assume(false)
#else
 #define CPPTRACE_UNREACHABLE() __builtin_unreachable()
#endif

// https://godbolt.org/z/7neGPEche
// gcc added support in 4.8 but I'm too lazy to check the minor version
#if defined(__GNUC__) && (__GNUC__ < 5)
 #define CPPTRACE_NORETURN __attribute__((noreturn))
#else
 #define CPPTRACE_NORETURN [[noreturn]]
#endif

CPPTRACE_BEGIN_NAMESPACE
    CPPTRACE_EXPORT const raw_trace& raw_trace_from_current_exception();
    CPPTRACE_EXPORT const stacktrace& from_current_exception();

    CPPTRACE_EXPORT const raw_trace& raw_trace_from_current_exception_rethrow();
    CPPTRACE_EXPORT const stacktrace& from_current_exception_rethrow();

    CPPTRACE_EXPORT bool current_exception_was_rethrown();

    CPPTRACE_NORETURN CPPTRACE_EXPORT CPPTRACE_FORCE_NO_INLINE
    void rethrow();

    CPPTRACE_NORETURN CPPTRACE_EXPORT CPPTRACE_FORCE_NO_INLINE
    void rethrow(std::exception_ptr exception);

    namespace detail {
        #ifdef _MSC_VER
         CPPTRACE_FORCE_NO_INLINE inline int exception_filter() {
             exception_unwind_interceptor(1);
             return 0; // EXCEPTION_CONTINUE_SEARCH
         }
         CPPTRACE_FORCE_NO_INLINE inline int unconditional_exception_filter() {
             collect_current_trace(1);
             return 0; // EXCEPTION_CONTINUE_SEARCH
         }
        #else
         bool do_catch(const std::type_info&, const std::type_info&);
         template<typename T>
         class unwind_interceptor {
         public:
             static int init;
         };
         CPPTRACE_EXPORT int do_prepare_unwind_interceptor(const std::type_info&, const std::type_info&);
         template<typename T>
         inline int prepare_unwind_interceptor() {
             // __attribute__((constructor)) inline functions can be called for every source file they're #included in
             // there is still only one copy of the inline function in the final executable, though
             // LTO can make the redundant constructs fire only once
             // do_prepare_unwind_interceptor prevents against multiple preparations however it makes sense to guard
             // against it here too as a fast path, not that this should matter for performance
             static bool did_prepare = false;
             if(!did_prepare) {
                 return do_prepare_unwind_interceptor(typeid(unwind_interceptor<T>), typeid(T));
                 did_prepare = true;
             }
             return 0;
         }
         #ifndef CPPTRACE_DONT_PREPARE_UNWIND_INTERCEPTOR_ON
         template<typename T>
         int unwind_interceptor<T>::init = prepare_unwind_interceptor<T>();
         #endif
         template<typename>
         struct argument;
         template<typename R, typename Arg>
         struct argument<R(Arg)> {
             using type = Arg;
         };
         template<typename R>
         struct argument<R(...)> {
             using type = void;
         };
         template<typename F>
         using unwind_interceptor_for = unwind_interceptor<typename argument<F>::type>;
        #endif

        inline void nop(int) {}
    }
CPPTRACE_END_NAMESPACE

#ifdef _MSC_VER
 // this awful double-IILE is due to C2713 "You can't use structured exception handling (__try/__except) and C++
 // exception handling (try/catch) in the same function."
 #define CPPTRACE_TRY \
     try { \
         ::cpptrace::detail::try_canary cpptrace_try_canary; \
         [&]() { \
             __try { \
                 [&]() {
 #define CPPTRACE_CATCH(param) \
                 }(); \
             } __except(::cpptrace::detail::exception_filter()) {} \
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
             CPPTRACE_UNREACHABLE(); \
             /* force instantiation of the init-er */ \
             ::cpptrace::detail::nop(CPPTRACE_UNWIND_INTERCEPTOR_FOR(param)::init); \
         } \
     } catch(param)
#endif

#define CPPTRACE_CATCH_ALT(param) catch(param)

#ifdef CPPTRACE_UNPREFIXED_TRY_CATCH
 #define TRY CPPTRACE_TRY
 #define CATCH(param) CPPTRACE_CATCH(param)
 #define CATCH_ALT(param) CPPTRACE_CATCH_ALT(param)
#endif

#endif
