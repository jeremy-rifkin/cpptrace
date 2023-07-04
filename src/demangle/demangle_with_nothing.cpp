#ifdef CPPTRACE_DEMANGLE_WITH_NOTHING

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_demangle.hpp"

#include <cstdlib>
#include <string>

namespace cpptrace {
    namespace detail {
        std::string demangle(const std::string& name) {
            return name;
        }
    }
}

#endif
