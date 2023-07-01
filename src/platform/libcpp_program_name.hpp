#ifndef LIBCPP_PROGRAM_NAME_HPP
#define LIBCPP_PROGRAM_NAME_HPP

#include <string>

#ifdef _WIN32
#include <windows.h>

namespace cpptrace {
    namespace detail {
        inline std::string program_name() {
            char buffer[MAX_PATH + 1];
            int res = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
            if(res) {
                return buffer;
            } else {
                return "";
            }
        }
    }
}

#else

namespace cpptrace {
    namespace detail {
        inline std::string program_name() {
            return "";
        }
    }
}

#endif

#endif
