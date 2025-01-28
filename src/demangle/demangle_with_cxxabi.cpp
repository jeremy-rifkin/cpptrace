#ifdef CPPTRACE_DEMANGLE_WITH_CXXABI

#include "demangle/demangle.hpp"

#include "utils/utils.hpp"

#include <cxxabi.h>

#include <cstdlib>
#include <string>

namespace cpptrace {
namespace detail {
    std::string demangle(const std::string& name) {
        int status;
        // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#demangler
        // check both _Z and __Z, apple prefixes all symbols with an underscore
        if(!(starts_with(name, "_Z") || starts_with(name, "__Z"))) {
            return name;
        }
        // presumably thread-safe
        // it appears safe to pass nullptr for status however the docs don't explicitly say it's safe so I don't
        // want to rely on it
        char* const demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
        // demangled will always be nullptr on non-zero status, and if __cxa_demangle ever fails for any reason
        // we'll just quietly return the mangled name
        if(demangled) {
            std::string str = demangled;
            std::free(demangled);
            return str;
        } else {
            return name;
        }
    }
}
}

#endif
