#ifndef PAC_HPP
#define PAC_HPP

#include <cstdint>

#include "utils/common.hpp"

CPPTRACE_BEGIN_NAMESPACE
namespace detail {
    // Strip Pointer Authentication Code (PAC) bits from an instruction address. No-op on non-ARM64.
    inline uintptr_t depac(uintptr_t addr) {
        #if defined(__aarch64__) || defined(_M_ARM64)
         // only gcc/clang support inline asm here and xpaci requires FEAT_PAuth
         #if (IS_CLANG || IS_GCC) && (defined(__APPLE__) || defined(__ARM_FEATURE_PAUTH))
          __asm__ volatile("xpaci %0" : "+r"(addr));
         #else
          addr &= 0x0000FFFFFFFFFFFF;
         #endif
        #endif
        return addr;
    }
}
CPPTRACE_END_NAMESPACE

#endif
