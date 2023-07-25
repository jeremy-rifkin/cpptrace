#ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/common.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if IS_LINUX || IS_APPLE
 #include <unistd.h>
 #include <dlfcn.h>
 // NOLINTNEXTLINE(misc-include-cleaner)
 #include <sys/types.h>
 #include <sys/wait.h>
 #if IS_APPLE
  #include "../platform/mach-o.hpp"
 #else
  #include "../platform/elf.hpp"
 #endif
#elif IS_WINDOWS
 #include "../platform/pe.hpp"
#endif

namespace cpptrace {
    namespace detail {
        struct dlframe {
            std::string obj_path;
            std::string symbol;
            uintptr_t obj_base = 0;
            uintptr_t raw_address = 0;
        };

        #if IS_LINUX || IS_APPLE
        // aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
        std::vector<dlframe> backtrace_frames(const std::vector<void*>& addrs) {
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
                    frame.obj_base = reinterpret_cast<uintptr_t>(info.dli_fbase);
                    frame.symbol = info.dli_sname ?: "";
                }
                frames.push_back(frame);
            }
            return frames;
        }

        bool has_addr2line() {
            static std::mutex mutex;
            static bool has_addr2line = false;
            static bool checked = false;
            std::lock_guard<std::mutex> lock(mutex);
            if(!checked) {
                checked = true;
                // Detects if addr2line exists by trying to invoke addr2line --help
                constexpr int magic = 42;
                // NOLINTNEXTLINE(misc-include-cleaner)
                const pid_t pid = fork();
                if(pid == -1) { return false; }
                if(pid == 0) { // child
                    close(STDOUT_FILENO);
                    close(STDERR_FILENO); // atos --help writes to stderr
                    #ifdef CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH
                     #if !IS_APPLE
                      execlp("addr2line", "addr2line", "--help", nullptr);
                     #else
                      execlp("atos", "atos", "--help", nullptr);
                     #endif
                    #else
                     #ifndef CPPTRACE_ADDR2LINE_PATH
                      #error "CPPTRACE_ADDR2LINE_PATH must be defined if CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH is not"
                     #endif
                     execl(CPPTRACE_ADDR2LINE_PATH, CPPTRACE_ADDR2LINE_PATH, "--help", nullptr);
                    #endif
                    _exit(magic);
                }
                int status;
                waitpid(pid, &status, 0);
                // NOLINTNEXTLINE(misc-include-cleaner)
                has_addr2line = WEXITSTATUS(status) == 0;
            }
            return has_addr2line;
        }

        struct pipe_t {
            union {
                struct {
                    int read_end;
                    int write_end;
                };
                int data[2];
            };
        };
        static_assert(sizeof(pipe_t) == 2 * sizeof(int), "Unexpected struct packing");

        std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
            pipe_t output_pipe;
            pipe_t input_pipe;
            internal_verify(pipe(output_pipe.data) == 0);
            internal_verify(pipe(input_pipe.data) == 0);
            // NOLINTNEXTLINE(misc-include-cleaner)
            const pid_t pid = fork();
            if(pid == -1) { return ""; } // error? TODO: Diagnostic
            if(pid == 0) { // child
                dup2(output_pipe.write_end, STDOUT_FILENO);
                dup2(input_pipe.read_end, STDIN_FILENO);
                close(output_pipe.read_end);
                close(output_pipe.write_end);
                close(input_pipe.read_end);
                close(input_pipe.write_end);
                close(STDERR_FILENO); // TODO: Might be worth conditionally enabling or piping
                #ifdef CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH
                 #if !IS_APPLE
                  execlp("addr2line", "addr2line", "-e", executable.c_str(), "-f", "-C", "-p", nullptr);
                 #else
                  execlp("atos", "atos", "-o", executable.c_str(), nullptr);
                 #endif
                #else
                 #ifndef CPPTRACE_ADDR2LINE_PATH
                  #error "CPPTRACE_ADDR2LINE_PATH must be defined if CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH is not"
                 #endif
                 #if !IS_APPLE
                  execl(
                    CPPTRACE_ADDR2LINE_PATH,
                    CPPTRACE_ADDR2LINE_PATH,
                    "-e",
                    executable.c_str(),
                    "-f",
                    "-C",
                    "-p",
                    nullptr
                  );
                 #else
                  execl(CPPTRACE_ADDR2LINE_PATH, CPPTRACE_ADDR2LINE_PATH, "-o", executable.c_str(), nullptr);
                 #endif
                #endif
                _exit(1); // TODO: Diagnostic?
            }
            internal_verify(write(input_pipe.write_end, addresses.data(), addresses.size()) != -1);
            close(input_pipe.read_end);
            close(input_pipe.write_end);
            close(output_pipe.write_end);
            std::string output;
            constexpr int buffer_size = 4096;
            char buffer[buffer_size];
            size_t count = 0;
            while((count = read(output_pipe.read_end, buffer, buffer_size)) > 0) {
                output.insert(output.end(), buffer, buffer + count);
            }
            // TODO: check status from addr2line?
            waitpid(pid, nullptr, 0);
            return output;
        }

        #if !IS_APPLE
        uintptr_t get_module_image_base(const dlframe &entry) {
            return elf_get_module_image_base(entry.obj_path);
        }
        #else
        uintptr_t get_module_image_base(const dlframe &entry) {
            // We have to parse the Mach-O to find the offset of the text section.....
            // I don't know how addresses are handled if there is more than one __TEXT load command. I'm assuming for
            // now that there is only one, and I'm using only the first section entry within that load command.
            static std::mutex mutex;
            std::lock_guard<std::mutex> lock(mutex);
            static std::unordered_map<std::string, uintptr_t> cache;
            auto it = cache.find(entry.obj_path);
            if(it == cache.end()) {
                // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
                // have two threads try to do the same computation
                auto base = macho_get_text_vmaddr(entry.obj_path.c_str());
                cache.insert(it, {entry.obj_path, base});
                return base;
            } else {
                return it->second;
            }
        }
        #endif
        #elif IS_WINDOWS
        std::string get_module_name(HMODULE handle) {
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
        // aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
        std::vector<dlframe> backtrace_frames(const std::vector<void*>& addrs) {
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
                    frame.obj_base = reinterpret_cast<uintptr_t>(handle);
                    frame.obj_path = get_module_name(handle);
                } else {
                    fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
                }
                frames.push_back(frame);
            }
            return frames;
        }

        bool has_addr2line() {
            static std::mutex mutex;
            static bool has_addr2line = false;
            static bool checked = false;
            std::lock_guard<std::mutex> lock(mutex);
            if(!checked) {
                // TODO: Popen is a hack. Implement properly with CreateProcess and pipes later.
                checked = true;
                #ifdef CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH
                 FILE* p = popen("addr2line --version", "r");
                #else
                 #ifndef CPPTRACE_ADDR2LINE_PATH
                  #error "CPPTRACE_ADDR2LINE_PATH must be defined if CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH is not"
                 #endif
                 FILE* p = popen(CPPTRACE_ADDR2LINE_PATH " --version", "r");
                #endif
                if(p) {
                    has_addr2line = pclose(p) == 0;
                }
            }
            return has_addr2line;
        }

        std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
            // TODO: Popen is a hack. Implement properly with CreateProcess and pipes later.
            ///fprintf(stderr, ("addr2line -e " + executable + " -fCp " + addresses + "\n").c_str());
            #ifdef CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH
             FILE* p = popen(("addr2line -e " + executable + " -fCp " + addresses).c_str(), "r");
            #else
             #ifndef CPPTRACE_ADDR2LINE_PATH
              #error "CPPTRACE_ADDR2LINE_PATH must be defined if CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH is not"
             #endif
             FILE* p = popen(
                (CPPTRACE_ADDR2LINE_PATH " -e " + executable + " -fCp " + addresses).c_str(),
                "r"
             );
            #endif
            std::string output;
            constexpr int buffer_size = 4096;
            char buffer[buffer_size];
            size_t count = 0;
            while((count = fread(buffer, 1, buffer_size, p)) > 0) {
                output.insert(output.end(), buffer, buffer + count);
            }
            pclose(p);
            ///fprintf(stderr, "%s\n", output.c_str());
            return output;
        }

        uintptr_t get_module_image_base(const dlframe &entry) {
            static std::mutex mutex;
            std::lock_guard<std::mutex> lock(mutex);
            static std::unordered_map<std::string, uintptr_t> cache;
            auto it = cache.find(entry.obj_path);
            if(it == cache.end()) {
                // arguably it'd be better to release the lock while computing this, but also arguably it's good to not
                // have two threads try to do the same computation
                auto base = pe_get_module_image_base(entry.obj_path);
                cache.insert(it, {entry.obj_path, base});
                return base;
            } else {
                return it->second;
            }
        }
        #endif

        struct symbolizer::impl {
            using target_vec = std::vector<std::pair<std::string, std::reference_wrapper<stacktrace_frame>>>;

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            std::unordered_map<std::string, target_vec> get_addr2line_targets(
                const std::vector<dlframe>& dlframes,
                std::vector<stacktrace_frame>& trace
            ) {
                std::unordered_map<std::string, target_vec> entries;
                for(std::size_t i = 0; i < dlframes.size(); i++) {
                    const auto& entry = dlframes[i];
                    // If libdl fails to find the shared object for a frame, the path will be empty. I've observed this
                    // on macos when looking up the shared object containing `start`.
                    if(!entry.obj_path.empty()) {
                        ///fprintf(stderr, "%s %s\n", to_hex(entry.raw_address).c_str(), to_hex(entry.raw_address - entry.obj_base + base).c_str());
                        entries[entry.obj_path].emplace_back(
                            to_hex(entry.raw_address - entry.obj_base + get_module_image_base(entry)),
                            trace[i]
                        );
                        // Set what is known for now, and resolutions from addr2line should overwrite
                        trace[i].filename = entry.obj_path;
                        trace[i].symbol = entry.symbol;
                    }
                }
                return entries;
            }

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            void update_trace(const std::string& line, size_t entry_index, const target_vec& entries_vec) {
                #if !IS_APPLE
                // Result will be of the form "<symbol> at path:line"
                // The path may be ?? if addr2line cannot resolve, line may be ?
                // Edge cases:
                // ?? ??:0
                // symbol :?
                const std::size_t at_location = line.find(" at ");
                std::size_t symbol_end;
                std::size_t filename_start;
                if(at_location != std::string::npos) {
                    symbol_end = at_location;
                    filename_start = at_location + 4;
                } else {
                    internal_verify(line.find("?? ") == 0, "Unexpected edge case while processing addr2line output");
                    symbol_end = 2;
                    filename_start = 3;
                }
                auto symbol = line.substr(0, symbol_end);
                auto colon = line.rfind(':');
                internal_verify(colon != std::string::npos);
                internal_verify(colon >= filename_start); // :? to deal with "symbol :?" edge case
                auto filename = line.substr(filename_start, colon - filename_start);
                auto line_number = line.substr(colon + 1);
                if(line_number != "?") {
                    entries_vec[entry_index].second.get().line = std::stoi(line_number);
                }
                if(!filename.empty() && filename != "??") {
                    entries_vec[entry_index].second.get().filename = filename;
                }
                if(!symbol.empty()) {
                    entries_vec[entry_index].second.get().symbol = symbol;
                }
                #else
                // Result will be of the form "<symbol> (in <object name>) (file:line)"
                // The symbol may just be the given address if atos can't resolve it
                // Examples:
                // trace() (in demo) (demo.cpp:8)
                // 0x100003b70 (in demo)
                // 0xffffffffffffffff
                // foo (in bar) + 14
                // I'm making some assumptions here. Support may need to be improved later. This is tricky output to
                // parse.
                const std::size_t in_location = line.find(" (in ");
                if(in_location == std::string::npos) {
                    // presumably the 0xffffffffffffffff case
                    return;
                }
                const std::size_t symbol_end = in_location;
                entries_vec[entry_index].second.get().symbol = line.substr(0, symbol_end);
                const std::size_t obj_end = line.find(")", in_location);
                internal_verify(
                    obj_end != std::string::npos,
                    "Unexpected edge case while processing addr2line/atos output"
                );
                const std::size_t filename_start = line.find(") (", obj_end);
                if(filename_start == std::string::npos) {
                    // presumably something like 0x100003b70 (in demo) or foo (in bar) + 14
                    return;
                }
                const std::size_t filename_end = line.find(":", filename_start);
                internal_verify(
                    filename_end != std::string::npos,
                    "Unexpected edge case while processing addr2line/atos output"
                );
                entries_vec[entry_index].second.get().filename = line.substr(filename_start + 3, filename_end - filename_start - 3);
                const std::size_t line_start = filename_end + 1;
                const std::size_t line_end = line.find(")", filename_end);
                internal_verify(
                    line_end == line.size() - 1,
                    "Unexpected edge case while processing addr2line/atos output"
                );
                entries_vec[entry_index].second.get().line = std::stoi(line.substr(line_start, line_end - line_start));
                #endif
            }

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames) {
                // TODO: Refactor better
                std::vector<stacktrace_frame> trace(frames.size(), stacktrace_frame { 0, 0, 0, "", "" });
                for(size_t i = 0; i < frames.size(); i++) {
                    trace[i].address = reinterpret_cast<uintptr_t>(frames[i]);
                }
                if(has_addr2line()) {
                    const std::vector<dlframe> dlframes = backtrace_frames(frames);
                    const auto entries = get_addr2line_targets(dlframes, trace);
                    for(const auto& entry : entries) {
                        const auto& object_name = entry.first;
                        const auto& entries_vec = entry.second;
                        std::string address_input;
                        for(const auto& pair : entries_vec) {
                            address_input += pair.first;
                            #if !IS_WINDOWS
                             address_input += '\n';
                            #else
                             address_input += ' ';
                            #endif
                        }
                        auto output = split(trim(resolve_addresses(address_input, object_name)), "\n");
                        internal_verify(output.size() == entries_vec.size());
                        for(size_t i = 0; i < output.size(); i++) {
                            update_trace(output[i], i, entries_vec);
                        }
                    }
                }
                return trace;
            }
        };

        // NOLINTNEXTLINE(bugprone-unhandled-exception-at-new)
        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        //stacktrace_frame symbolizer::resolve_frame(void* addr) {
        //    return pimpl->resolve_frame(addr);
        //}

        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            return pimpl->resolve_frames(frames);
        }
    }
}

#endif
