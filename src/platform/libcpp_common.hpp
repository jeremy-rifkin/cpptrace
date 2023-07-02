#ifndef LIBCPP_COMMON_HPP
#define LIBCPP_COMMON_HPP

#ifdef _MSC_VER
#define CPPTRACE_FORCE_NO_INLINE __declspec(noinline)
#else
#define CPPTRACE_FORCE_NO_INLINE __attribute__((noinline))
#endif

#endif
