#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "common.hpp"

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

struct dlframe {
    std::string obj_path;
    std::string symbol;
    uintptr_t raw_address = 0;
    uintptr_t obj_address = 0;
};

#if IS_LINUX || IS_APPLE
#if !IS_APPLE
static uintptr_t get_module_image_base(const std::string& obj_path) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static std::unordered_map<std::string, uintptr_t> cache;
    auto it = cache.find(obj_path);
    if(it == cache.end()) {
        // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
        // have two threads try to do the same computation
        auto base = elf_get_module_image_base(obj_path);
        cache.insert(it, {obj_path, base});
        return base;
    } else {
        return it->second;
    }
}
#else
static uintptr_t get_module_image_base(const std::string& obj_path) {
    // We have to parse the Mach-O to find the offset of the text section.....
    // I don't know how addresses are handled if there is more than one __TEXT load command. I'm assuming for
    // now that there is only one, and I'm using only the first section entry within that load command.
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static std::unordered_map<std::string, uintptr_t> cache;
    auto it = cache.find(obj_path);
    if(it == cache.end()) {
        // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
        // have two threads try to do the same computation
        auto base = macho_get_text_vmaddr(obj_path.c_str());
        cache.insert(it, {obj_path, base});
        return base;
    } else {
        return it->second;
    }
}
#endif
// aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
static std::vector<dlframe> get_frames_object_info(const std::vector<void*>& addrs) {
    // reference: https://github.com/bminor/glibc/blob/master/debug/backtracesyms.c
    std::vector<dlframe> frames;
    frames.reserve(addrs.size());
    for(const void* addr : addrs) {
        Dl_info info;
        dlframe frame;
        frame.raw_address = reinterpret_cast<uintptr_t>(addr);
        if(dladdr(addr, &info)) { // thread safe
            // dli_sname and dli_saddr are only present with -rdynamic, sname will be included
            // but we don't really need dli_saddr
            frame.obj_path = info.dli_fname;
            frame.obj_address = reinterpret_cast<uintptr_t>(addr)
                                - reinterpret_cast<uintptr_t>(info.dli_fbase)
                                + get_module_image_base(info.dli_fname);
            frame.symbol = info.dli_sname ?: "";
        }
        frames.push_back(frame);
    }
    return frames;
}
#else
static std::string get_module_name(HMODULE handle) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static std::unordered_map<HMODULE, std::string> cache;
    auto it = cache.find(handle);
    if(it == cache.end()) {
        char path[MAX_PATH];
        if(GetModuleFileNameA(handle, path, sizeof(path))) {
            ///fprintf(stderr, "path: %s base: %p\n", path, handle);
            cache.insert(it, {handle, path});
            return path;
        } else {
            fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
            cache.insert(it, {handle, ""});
            return "";
        }
    } else {
        return it->second;
    }
}
static uintptr_t get_module_image_base(const std::string& obj_path) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static std::unordered_map<std::string, uintptr_t> cache;
    auto it = cache.find(obj_path);
    if(it == cache.end()) {
        // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
        // have two threads try to do the same computation
        auto base = pe_get_module_image_base(obj_path);
        cache.insert(it, {obj_path, base});
        return base;
    } else {
        return it->second;
    }
}
// aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
static std::vector<dlframe> get_frames_object_info(const std::vector<void*>& addrs) {
    // reference: https://github.com/bminor/glibc/blob/master/debug/backtracesyms.c
    std::vector<dlframe> frames;
    frames.reserve(addrs.size());
    for(const void* addr : addrs) {
        dlframe frame;
        frame.raw_address = reinterpret_cast<uintptr_t>(addr);
        HMODULE handle;
        // Multithread safe as long as another thread doesn't come along and free the module
        if(GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            static_cast<const char*>(addr),
            &handle
        )) {
            frame.obj_path = get_module_name(handle);
            frame.obj_address = reinterpret_cast<uintptr_t>(addr)
                                - reinterpret_cast<uintptr_t>(handle)
                                + get_module_image_base(frame.obj_path);
        } else {
            fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
        }
        frames.push_back(frame);
    }
    return frames;
}
#endif

#endif
