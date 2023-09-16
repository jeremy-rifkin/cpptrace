#ifdef CPPTRACE_DEMANGLE_WITH_CXXABI

#include "demangle.hpp"

#include <cxxabi.h>

#include <cstdlib>
#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string& name) {
            // presumably thread-safe
            char* const demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, nullptr);
            if(demangled) {
                const std::string str = demangled;
                // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
                std::free(demangled);
                return str;
            } else {
                return name;
            }
        }
    }
}

#endif
