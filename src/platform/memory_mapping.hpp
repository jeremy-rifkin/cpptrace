#include "utils/common.hpp"

#ifndef _MSC_VER

#if IS_WINDOWS || IS_CYGWIN
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <sys/mman.h>
#endif

CPPTRACE_BEGIN_NAMESPACE
namespace detail {
    #if IS_WINDOWS || IS_CYGWIN
     constexpr auto memory_readonly = PAGE_READONLY;
     constexpr auto memory_readwrite = PAGE_READWRITE;
    #else
     constexpr auto memory_readonly = PROT_READ;
     constexpr auto memory_readwrite = PROT_READ | PROT_WRITE;
    #endif
    int get_page_size();
    #if !IS_WINDOWS && !IS_CYGWIN
     int get_page_protections(void* page);
    #endif
    int mprotect_page_and_return_old_protections(void* page, int page_size, int protections);
    void mprotect_page(void* page, int page_size, int protections);
    void* allocate_page(int page_size);
}
CPPTRACE_END_NAMESPACE

#endif
