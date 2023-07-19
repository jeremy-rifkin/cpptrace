#ifdef CPPTRACE_DEMANGLE_WITH_CXXABI

#include "cpptrace_demangle.hpp"

#include <cxxabi.h>

#include <cstdlib>
#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string& name) {
            int status;
            char* demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
            if(demangled) {
                std::string str = demangled;
                // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
                free(demangled);
                return str;
            } else {
                return name;
            }
        }
    }
}

#endif
