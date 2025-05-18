#ifndef CPPTRACE_FROM_CURRENT_HPP
#define CPPTRACE_FROM_CURRENT_HPP

#include <exception>
#include <typeinfo>

#ifdef _MSC_VER
 #include <windows.h>
#endif

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

    CPPTRACE_EXPORT void clear_current_exception_traces();

    namespace detail {
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

        CPPTRACE_EXPORT CPPTRACE_FORCE_NO_INLINE void collect_current_trace(std::size_t skip);

        #ifdef _MSC_VER
         CPPTRACE_EXPORT bool matches_exception(EXCEPTION_POINTERS* exception_ptrs, const std::type_info& type_info);
         template<typename E>
         CPPTRACE_FORCE_NO_INLINE inline int exception_filter(EXCEPTION_POINTERS* exception_ptrs) {
             if(matches_exception(exception_ptrs, typeid(E))) {
                 collect_current_trace(1);
             }
             return EXCEPTION_CONTINUE_SEARCH;
         }
        #else
         bool check_can_catch(const std::type_info*, const std::type_info*, void**, unsigned);
         template<typename T>
         class unwind_interceptor {
         public:
             static int init;
             CPPTRACE_FORCE_NO_INLINE static bool can_catch(
                 const std::type_info* /* this */,
                 const std::type_info* throw_type,
                 void** throw_obj,
                 unsigned outer
             ) {
                 if(check_can_catch(&typeid(T), throw_type, throw_obj, outer)) {
                     collect_current_trace(1);
                 }
                 return false;
             }
         };
         CPPTRACE_EXPORT void do_prepare_unwind_interceptor(
             const std::type_info&,
             bool(*)(const std::type_info*, const std::type_info*, void**, unsigned)
         );
         template<typename T>
         inline int prepare_unwind_interceptor() {
             do_prepare_unwind_interceptor(typeid(unwind_interceptor<T>), unwind_interceptor<T>::can_catch);
             return 1;
         }
         template<typename T>
         int unwind_interceptor<T>::init = prepare_unwind_interceptor<T>();
         template<typename F>
         using unwind_interceptor_for = unwind_interceptor<typename argument<F>::type>;
        #endif

        inline void nop(int) {}
    }
CPPTRACE_END_NAMESPACE

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
             CPPTRACE_UNREACHABLE(); \
             /* force instantiation of the init-er */ \
             ::cpptrace::detail::nop(CPPTRACE_UNWIND_INTERCEPTOR_FOR(param)::init); \
         } \
     } catch(param)
#endif

// TODO: ELIMINATE
#define CPPTRACE_CATCH_ALT(param) catch(param)

#ifdef CPPTRACE_UNPREFIXED_TRY_CATCH
 #define TRY CPPTRACE_TRY
 #define CATCH(param) CPPTRACE_CATCH(param)
 #define CATCH_ALT(param) CPPTRACE_CATCH_ALT(param)
#endif

#endif
