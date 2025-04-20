#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <cpptrace/utils.hpp>

#include "utils/microfmt.hpp"

namespace cpptrace {
namespace detail {
namespace log {
    void error(const char*);
    template<typename... Args>
    void error(const char* format, Args&&... args) {
        error(microfmt::format(format, args...).c_str());
    }
}
}
}

#endif
