#ifdef LIBCPPTRACE_DEMANGLE_WITH_CXXABI

#include <cpptrace/cpptrace.hpp>
#include "libcpp_demangle.hpp"

#include <cxxabi.h>

#include <cstdlib>
#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string& name) {
            int status;
            char* demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
            if(demangled) {
                std::string s = demangled;
                free(demangled);
                return s;
            } else {
                return name;
            }
        }
    }
}

#endif
