#ifdef CPPTRACE_DEMANGLE_WITH_NOTHING

#include "demangle/demangle.hpp"

#include <string>

namespace cpptrace {
namespace internal {
    std::string demangle(const std::string& name, bool) {
        return name;
    }
}
}

#endif
