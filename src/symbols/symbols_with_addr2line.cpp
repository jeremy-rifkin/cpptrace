#ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_symbols.hpp"
#include "../platform/cpptrace_common.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <functional>
#include <vector>

#if IS_LINUX || IS_APPLE
 #include <unistd.h>
 #include <dlfcn.h>
 // NOLINTNEXTLINE(misc-include-cleaner)
 #include <sys/types.h>
 #include <sys/wait.h>
#elif IS_WINDOWS
 #include <windows.h>
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
                if(dladdr(addr, &info)) {
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
            // Detects if addr2line exists by trying to invoke addr2line --help
            constexpr int magic = 42;
            // NOLINTNEXTLINE(misc-include-cleaner)
            const pid_t pid = fork();
            if(pid == -1) { return false; }
            if(pid == 0) { // child
                close(STDOUT_FILENO);
                // TODO: path
                execlp("addr2line", "addr2line", "--help", nullptr);
                _exit(magic);
            }
            int status;
            waitpid(pid, &status, 0);
            // NOLINTNEXTLINE(misc-include-cleaner)
            return WEXITSTATUS(status) == 0;
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
                // TODO: Prevent against path injection?
                execlp("addr2line", "addr2line", "-e", executable.c_str(), "-f", "-C", "-p", nullptr);
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

        uintptr_t get_module_image_base(const dlframe &entry) {
            (void)entry;
            return 0;
        }
        #elif IS_WINDOWS
        // aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
        std::vector<dlframe> backtrace_frames(const std::vector<void*>& addrs) {
            // reference: https://github.com/bminor/glibc/blob/master/debug/backtracesyms.c
            std::vector<dlframe> frames;
            frames.reserve(addrs.size());
            for(const void* addr : addrs) {
                dlframe frame;
                frame.raw_address = reinterpret_cast<uintptr_t>(addr);
                HMODULE handle;
                if(GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    static_cast<const char*>(addr),
                    &handle
                )) {
                    fflush(stderr);
                    char path[MAX_PATH];
                    if(GetModuleFileNameA(handle, path, sizeof(path))) {
                        ///fprintf(stderr, "path: %s base: %p\n", path, handle);
                        frame.obj_path = path;
                        frame.obj_base = reinterpret_cast<uintptr_t>(handle);
                        frame.symbol = "";
                    } else {
                        fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
                    }
                } else {
                    fprintf(stderr, "%s\n", std::system_error(GetLastError(), std::system_category()).what());
                }
                frames.push_back(frame);
            }
            return frames;
        }

        bool has_addr2line() {
            // TODO: Popen is a hack. Implement properly with CreateProcess and pipes later.
            FILE* p = popen("addr2line --version", "r");
            return pclose(p) == 0;
        }

        std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
            // TODO: Popen is a hack. Implement properly with CreateProcess and pipes later.
            ///fprintf(stderr, ("addr2line -e " + executable + " -fCp " + addresses + "\n").c_str());
            FILE* p = popen(("addr2line -e " + executable + " -fCp " + addresses).c_str(), "r");
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

        // TODO: Refactor into backtrace_frames...
        // TODO: Memoize
        uintptr_t get_module_image_base(const dlframe &entry) {
            // PE header values are little endian
            bool do_swap = !is_little_endian();
            FILE* file = fopen(entry.obj_path.c_str(), "rb");
            char magic[2];
            internal_verify(fread(magic, 1, 2, file) == 2); // file + 0x0
            internal_verify(memcmp(magic, "MZ", 2) == 0);
            DWORD e_lfanew;
            internal_verify(fseek(file, 0x3c, SEEK_SET) == 0);
            internal_verify(fread(&e_lfanew, sizeof(DWORD), 1, file) == 1); // file + 0x3c
            if(do_swap) e_lfanew = byteswap(e_lfanew);
            long nt_header_offset = e_lfanew;
            char signature[4];
            internal_verify(fseek(file, nt_header_offset, SEEK_SET) == 0);
            internal_verify(fread(signature, 1, 4, file) == 4); // NT header + 0x0
            internal_verify(memcmp(signature, "PE\0\0", 4) == 0);
            //WORD machine;
            //internal_verify(fseek(file, nt_header_offset + 4, SEEK_SET) == 0); // file header + 0x0
            //internal_verify(fread(&machine, sizeof(WORD), 1, file) == 1);
            WORD size_of_optional_header;
            internal_verify(fseek(file, nt_header_offset + 4 + 0x10, SEEK_SET) == 0); // file header + 0x10
            internal_verify(fread(&size_of_optional_header, sizeof(DWORD), 1, file) == 1);
            if(do_swap) size_of_optional_header = byteswap(size_of_optional_header);
            internal_verify(size_of_optional_header != 0);
            WORD optional_header_magic;
            internal_verify(fseek(file, nt_header_offset + 0x18, SEEK_SET) == 0); // optional header + 0x0
            internal_verify(fread(&optional_header_magic, sizeof(DWORD), 1, file) == 1);
            if(do_swap) optional_header_magic = byteswap(optional_header_magic);
            internal_verify(optional_header_magic == IMAGE_NT_OPTIONAL_HDR_MAGIC);
            uintptr_t image_base;
            if(optional_header_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
                // 32 bit
                DWORD base;
                internal_verify(fseek(file, nt_header_offset + 0x18 + 0x1c, SEEK_SET) == 0); // optional header + 0x1c
                internal_verify(fread(&base, sizeof(DWORD), 1, file) == 1);
                if(do_swap) base = byteswap(base);
                image_base = base;
            } else {
                // 64 bit
                // I get an "error: 'QWORD' was not declared in this scope" for some reason when using QWORD
                unsigned __int64 base;
                internal_verify(fseek(file, nt_header_offset + 0x18 + 0x18, SEEK_SET) == 0); // optional header + 0x18
                internal_verify(fread(&base, sizeof(unsigned __int64), 1, file) == 1);
                if(do_swap) base = byteswap(base);
                image_base = base;
            }
            fclose(file);
            return image_base;
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
                    ///fprintf(stderr, "%s %s\n", to_hex(entry.raw_address).c_str(), to_hex(entry.raw_address - entry.obj_base + base).c_str());
                    entries[entry.obj_path].emplace_back(
                        to_hex(entry.raw_address - entry.obj_base + get_module_image_base(entry)),
                        trace[i]
                    );
                    // Set what is known for now, and resolutions from addr2line should overwrite
                    trace[i].filename = entry.obj_path;
                    trace[i].symbol = entry.symbol;
                }
                return entries;
            }

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            void update_trace(const std::string& line, size_t entry_index, const target_vec& entries_vec) {
                // Result will be of the form <identifier> " at " path:line
                // The path may be ?? if addr2line cannot resolve, line may be ?
                // Edge cases:
                // ?? ??:0
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
                internal_verify(colon > filename_start);
                auto filename = line.substr(filename_start, colon - filename_start);
                auto line_number = line.substr(colon + 1);
                if(line_number != "?") {
                    entries_vec[entry_index].second.get().line = std::stoi(line_number);
                }
                if(filename != "??") {
                    entries_vec[entry_index].second.get().filename = filename;
                }
                if(!symbol.empty()) {
                    entries_vec[entry_index].second.get().symbol = symbol;
                }
            }

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames) {
                // TODO: Refactor better
                std::vector<stacktrace_frame> trace(frames.size(), stacktrace_frame { 0, 0, 0, "", "" });
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
