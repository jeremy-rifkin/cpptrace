#ifndef IMAGE_MODULE_BASE_HPP
#define IMAGE_MODULE_BASE_HPP

#include "../utils/common.hpp"
#include "../utils/utils.hpp"

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

#if IS_LINUX || IS_APPLE
 #include <unistd.h>
 #include <dlfcn.h>
 #if IS_APPLE
  #include "mach-o.hpp"
 #else
  #include "elf.hpp"
 #endif
#elif IS_WINDOWS
 #include <windows.h>
 #include "pe.hpp"
#endif

namespace cpptrace {
namespace detail {
    #if IS_LINUX
    inline std::uintptr_t get_module_image_base(const std::string& object_path) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        static std::unordered_map<std::string, std::uintptr_t> cache;
        auto it = cache.find(object_path);
        if(it == cache.end()) {
            // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
            // have two threads try to do the same computation
            auto base = elf_get_module_image_base(object_path);
            cache.insert(it, {object_path, base});
            return base;
        } else {
            return it->second;
        }
    }
    #elif IS_APPLE
    inline std::uintptr_t get_module_image_base(const std::string& object_path) {
        // We have to parse the Mach-O to find the offset of the text section.....
        // I don't know how addresses are handled if there is more than one __TEXT load command. I'm assuming for
        // now that there is only one, and I'm using only the first section entry within that load command.
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        static std::unordered_map<std::string, std::uintptr_t> cache;
        auto it = cache.find(object_path);
        if(it == cache.end()) {
            // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
            // have two threads try to do the same computation
            auto base = mach_o(object_path).get_text_vmaddr();
            cache.insert(it, {object_path, base});
            return base;
        } else {
            return it->second;
        }
    }
    #else // Windows
    inline std::string get_module_name(HMODULE handle) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        static std::unordered_map<HMODULE, std::string> cache;
        auto it = cache.find(handle);
        if(it == cache.end()) {
            char path[MAX_PATH];
            if(GetModuleFileNameA(handle, path, sizeof(path))) {
                ///std::fprintf(stderr, "path: %s base: %p\n", path, handle);
                cache.insert(it, {handle, path});
                return path;
            } else {
                std::fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
                cache.insert(it, {handle, ""});
                return "";
            }
        } else {
            return it->second;
        }
    }

    inline std::uintptr_t get_module_image_base(const std::string& object_path) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        static std::unordered_map<std::string, std::uintptr_t> cache;
        auto it = cache.find(object_path);
        if(it == cache.end()) {
            // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
            // have two threads try to do the same computation
            auto base = pe_get_module_image_base(object_path);
            cache.insert(it, {object_path, base});
            return base;
        } else {
            return it->second;
        }
    }
    #endif
}
}

#endif
