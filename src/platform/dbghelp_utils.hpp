#ifndef DBGHELP_UTILS_HPP
#define DBGHELP_UTILS_HPP

#if defined(CPPTRACE_UNWIND_WITH_DBGHELP) \
    || defined(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP) \
    || defined(CPPTRACE_DEMANGLE_WITH_WINAPI)

#include <unordered_map>
#include <mutex>

namespace cpptrace {
namespace detail {
    struct dbghelp_syminit_manager {
        // The set below contains Windows `HANDLE` objects, `void*` is used to avoid
        // including the (expensive) Windows header here
        std::unordered_map<void*, void*> cache;

        ~dbghelp_syminit_manager();
        void* init(void* proc);
    };

    dbghelp_syminit_manager& get_syminit_manager();

    std::unique_lock<std::recursive_mutex> get_dbghelp_lock();
}
}

#endif

#endif
