#ifndef DBGHELP_SYMINIT_MANAGER_HPP
#define DBGHELP_SYMINIT_MANAGER_HPP

#include "common.hpp"
#include "utils.hpp"

#include <mutex>
#include <unordered_set>

#include <windows.h>
#include <dbghelp.h>

namespace cpptrace {
namespace detail {
    struct dbghelp_syminit_manager {
        std::unordered_set<HANDLE> set;
        std::mutex mutex;

        ~dbghelp_syminit_manager() {
            for(auto handle : set) {
                if(!SymCleanup(handle)) {
                    ASSERT(false, stringf("Cpptrace SymCleanup failed with code %llu\n", to_ull(GetLastError())));
                }
            }
        }

        void init(HANDLE proc) {
            if(set.count(proc) == 0) {
                std::lock_guard<std::mutex> lock(mutex);
                if(!SymInitialize(proc, NULL, TRUE)) {
                    throw std::logic_error(stringf("SymInitialize failed %llu", to_ull(GetLastError())));
                }
                set.insert(proc);
            }
        }
    };

    inline dbghelp_syminit_manager& get_syminit_manager() {
        static dbghelp_syminit_manager syminit_manager;
        return syminit_manager;
    }
}
}

#endif
