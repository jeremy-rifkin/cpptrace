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
 // NOLINTNEXTLINE(misc-include-cleaner)
 #include <sys/types.h>
 #include <sys/wait.h>
#endif

#include "../platform/object.hpp"

namespace cpptrace {
    namespace detail {
        namespace addr2line {
            #if IS_LINUX || IS_APPLE
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
                    execlp("atos", "atos", "-o", executable.c_str(), "-fullPath", nullptr);
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
                    execl(
                        CPPTRACE_ADDR2LINE_PATH,
                        CPPTRACE_ADDR2LINE_PATH,
                        "-o", executable.c_str(),
                        "-fullPath",
                        nullptr
                    );
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
            #elif IS_WINDOWS
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
                FILE* p = popen(("addr2line -e \"" + executable + "\" -fCp " + addresses).c_str(), "r");
                #else
                #ifndef CPPTRACE_ADDR2LINE_PATH
                #error "CPPTRACE_ADDR2LINE_PATH must be defined if CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH is not"
                #endif
                FILE* p = popen(
                    (CPPTRACE_ADDR2LINE_PATH " -e \"" + executable + "\" -fCp " + addresses).c_str(),
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
            #endif

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
                        ///fprintf(
                        ///    stderr,
                        ///    "%s %s\n",
                        ///    to_hex(entry.raw_address).c_str(),
                        ///    to_hex(entry.raw_address - entry.obj_base + base).c_str()
                        ///);
                        try {
                            entries[entry.obj_path].emplace_back(
                                to_hex(entry.obj_address),
                                trace[i]
                            );
                        } catch(file_error&) {
                            //
                        } catch(...) {
                            throw;
                        }
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
                entries_vec[entry_index].second.get().filename = line.substr(
                    filename_start + 3,
                    filename_end - filename_start - 3
                );
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
                    const std::vector<dlframe> dlframes = get_frames_object_info(frames);
                    const auto entries = get_addr2line_targets(dlframes, trace);
                    for(const auto& entry : entries) {
                        const auto& object_name = entry.first;
                        const auto& entries_vec = entry.second;
                        // You may ask why it'd ever happen that there could be an empty entries_vec array, if there're
                        // no addresses why would get_addr2line_targets do anything? The reason is because if things in
                        // get_addr2line_targets fail it will silently skip. This is partly an optimization but also an
                        // assertion below will fail if addr2line is given an empty input.
                        if(entries_vec.empty()) {
                            continue;
                        }
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
        }
    }
}

#endif
