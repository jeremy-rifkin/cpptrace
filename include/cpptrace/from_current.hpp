#ifndef CPPTRACE_FROM_CURRENT_HPP
#define CPPTRACE_FROM_CURRENT_HPP

#include <exception>
#include <typeinfo>
#include <utility>

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
         CPPTRACE_EXPORT bool check_can_catch(const std::type_info*, const std::type_info*, void**, unsigned);
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
         inline void nop(int) {}
        #endif
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

CPPTRACE_BEGIN_NAMESPACE
    namespace detail {
        template<typename R, typename Arg>
        Arg get_callable_argument_helper(R(*) (Arg));
        template<typename R, typename F, typename Arg>
        Arg get_callable_argument_helper(R(F::*) (Arg));
        template<typename R, typename F, typename Arg>
        Arg get_callable_argument_helper(R(F::*) (Arg) const);
        template<typename R>
        void get_callable_argument_helper(R(*) ());
        template<typename R, typename F>
        void get_callable_argument_helper(R(F::*) ());
        template<typename R, typename F>
        void get_callable_argument_helper(R(F::*) () const);
        template<typename F>
        decltype(get_callable_argument_helper(&F::operator())) get_callable_argument_wrapper(F);
        template<typename T>
        using get_callable_argument = decltype(get_callable_argument_wrapper(std::declval<T>()));

        template<typename E, typename F, typename Catch, typename std::enable_if<!std::is_same<E, void>::value, int>::type = 0>
        void do_try_catch(F&& f, Catch&& catcher) {
            CPPTRACE_TRY {
                f();
            } CPPTRACE_CATCH(E e) {
                catcher(std::forward<E>(e));
            }
        }

        template<typename E, typename F, typename Catch, typename std::enable_if<std::is_same<E, void>::value, int>::type = 0>
        void do_try_catch(F&& f, Catch&& catcher) {
            CPPTRACE_TRY {
                f();
            } CPPTRACE_CATCH(...) {
                catcher();
            }
        }

        template<typename F>
        void try_catch_impl(F&& f) {
            f();
        }

        // TODO: This could be made more efficient to reduce the number of interceptor levels that do typeid checks
        // and possible traces
        template<typename F, typename Catch, typename... Catches>
        void try_catch_impl(F&& f, Catch&& catcher, Catches&&... catches) {
            // match the first catch at the inner-most level... no real way to reverse a pack or extract from the end so
            // we have to wrap with a lambda
            auto wrapped = [&] () {
                using E = get_callable_argument<Catch>;
                do_try_catch<E>(std::forward<F>(f), std::forward<Catch>(catcher));
            };
            try_catch_impl(std::move(wrapped), std::forward<Catches>(catches)...);
        }
    }

    template<typename F, typename... Catches>
    void try_catch(F&& f, Catches&&... catches) {
        return detail::try_catch_impl(std::forward<F>(f), std::forward<Catches>(catches)...);
    }
CPPTRACE_END_NAMESPACE

#ifdef CPPTRACE_UNPREFIXED_TRY_CATCH
 #define TRY CPPTRACE_TRY
 #define CATCH(param) CPPTRACE_CATCH(param)
#endif

#endif
