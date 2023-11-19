#ifndef PROGRAM_NAME_HPP
#define PROGRAM_NAME_HPP

#include <mutex>
#include <string>

#if defined(_WIN32)
#include <windows.h>

#define CPPTRACE_MAX_PATH MAX_PATH

namespace cpptrace {
namespace detail {
    inline std::string program_name() {
        static std::mutex mutex;
        const std::lock_guard<std::mutex> lock(mutex);
        static std::string name;
        static bool did_init = false;
        static bool valid = false;
        if(!did_init) {
            did_init = true;
            char buffer[MAX_PATH + 1];
            int res = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
            if(res) {
                name = buffer;
                valid = true;
            }
        }
        return valid && !name.empty() ? name.c_str() : nullptr;
    }
}
}

#elif defined(__APPLE__)

#include <cstdint>
#include <mach-o/dyld.h>
#include <sys/syslimits.h>

#define CPPTRACE_MAX_PATH PATH_MAX

namespace cpptrace {
namespace detail {
    inline const char* program_name() {
        static std::mutex mutex;
        const std::lock_guard<std::mutex> lock(mutex);
        static std::string name;
        static bool did_init = false;
        static bool valid = false;
        if(!did_init) {
            did_init = true;
            std::uint32_t bufferSize = PATH_MAX + 1;
            char buffer[bufferSize];
            if(_NSGetExecutablePath(buffer, &bufferSize) == 0) {
                name.assign(buffer, bufferSize);
                valid = true;
            }
        }
        return valid && !name.empty() ? name.c_str() : nullptr;
    }
}
}

#elif defined(__linux__)

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

#define CPPTRACE_MAX_PATH PATH_MAX

namespace cpptrace {
namespace detail {
    inline const char* program_name() {
        static std::mutex mutex;
        const std::lock_guard<std::mutex> lock(mutex);
        static std::string name;
        static bool did_init = false;
        static bool valid = false;
        if(!did_init) {
            did_init = true;
            char buffer[PATH_MAX + 1];
            const ssize_t size = readlink("/proc/self/exe", buffer, PATH_MAX);
            if(size == -1) {
                return nullptr;
            }
            buffer[size] = 0;
            name = buffer;
            valid = true;
        }
        return valid && !name.empty() ? name.c_str() : nullptr;
    }
}
}

#endif

#endif
