#ifndef CPPTRACE_PROGRAM_NAME_HPP
#define CPPTRACE_PROGRAM_NAME_HPP

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
#include <linux/limits.h>
#include <unistd.h>

namespace cpptrace {
    namespace detail {
        inline const char* program_name() {
            static std::string name;
            static bool did_init = false;
            static bool valid = false;
            if(!did_init) {
                did_init = true;
                char buffer[PATH_MAX + 1];
                ssize_t s = readlink("/proc/self/exe", buffer, PATH_MAX);
                if(s == -1) {
                    return nullptr;
                }
                buffer[s] = 0;
                name = buffer;
                valid = true;
            }
            return valid ? name.c_str() : nullptr;
        }
    }
}

#endif

#endif
