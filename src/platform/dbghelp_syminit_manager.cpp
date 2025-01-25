#include "platform/platform.hpp"

#if IS_WINDOWS

#include "platform/dbghelp_syminit_manager.hpp"

#include "utils/error.hpp"
#include "utils/microfmt.hpp"

#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>

namespace cpptrace {
namespace detail {

    dbghelp_syminit_manager::~dbghelp_syminit_manager() {
        for(auto kvp : cache) {
            if(!SymCleanup(kvp.second)) {
                ASSERT(false, microfmt::format("Cpptrace SymCleanup failed with code {}\n", GetLastError()).c_str());
            }
            if (!CloseHandle(kvp.second)) {
                ASSERT(false, microfmt::format("Cpptrace CloseHandle failed with code {}\n", GetLastError()).c_str());
            }
        }
    }

    HANDLE dbghelp_syminit_manager::init(HANDLE proc) {
        auto itr = cache.find(proc);

        if (itr != cache.end()) {
            return itr->second;
        }
        HANDLE duplicated_handle  = nullptr;
        if (!DuplicateHandle(proc, proc, proc, &duplicated_handle , 0, FALSE, DUPLICATE_SAME_ACCESS)) {
	        throw internal_error("DuplicateHandle failed {}", GetLastError());
        }

        if(!SymInitialize(duplicated_handle , NULL, TRUE)) {
	        throw internal_error("SymInitialize failed {}", GetLastError());
        }
        cache[proc] = duplicated_handle ;
        return duplicated_handle ;
    }

    // Thread-safety: Must only be called from symbols_with_dbghelp while the dbghelp_lock lock is held
    dbghelp_syminit_manager& get_syminit_manager() {
        static dbghelp_syminit_manager syminit_manager;
        return syminit_manager;
    }

}
}

#endif
