#ifdef CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE

#include <cpptrace/cpptrace.hpp>
#include "cpptrace_symbols.hpp"
#include "../platform/cpptrace_common.hpp"

#include <stdio.h>
#include <signal.h>
#include <vector>
#include <unordered_map>

#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>

namespace cpptrace {
    namespace detail {
        struct dlframe {
            std::string obj_path;
            std::string symbol;
            uintptr_t obj_base = 0;
            uintptr_t raw_address = 0;
        };

        // aladdr queries are needed to get pre-ASLR addresses and targets to run addr2line on
        std::vector<dlframe> backtrace_frames(const std::vector<void*>& addrs) {
            // reference: https://github.com/bminor/glibc/blob/master/debug/backtracesyms.c
            std::vector<dlframe> frames;
            frames.reserve(addrs.size());
            for(const auto addr : addrs) {
                Dl_info info;
                dlframe frame;
                frame.raw_address = reinterpret_cast<uintptr_t>(addr);
                if(dladdr(addr, &info)) {
                    // dli_sname and dli_saddr are only present with -rdynamic, sname will be included
                    // but we don't really need dli_saddr
                    frame.obj_path = info.dli_fname;
                    frame.obj_base = reinterpret_cast<uintptr_t>(info.dli_fbase);
                    frame.symbol = info.dli_sname ?: "?";
                }
                frames.push_back(frame);
            }
            return frames;
        }

        bool has_addr2line() {
            // Detects if addr2line exists by trying to invoke addr2line --help
            constexpr int magic = 42;
            pid_t pid = fork();
            if(pid == -1) { return false; }
            if(pid == 0) { // child
                close(STDOUT_FILENO);
                // TODO: path
                execlp("addr2line", "addr2line", "--help", nullptr);
                exit(magic);
            }
            int status;
            waitpid(pid, &status, 0);
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

        static std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
            pipe_t output_pipe;
            pipe_t input_pipe;
            internal_verify(pipe(output_pipe.data) == 0);
            internal_verify(pipe(input_pipe.data) == 0);
            pid_t pid = fork();
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
            std::cerr<<"---E---"<<std::endl;
            std::cerr<<output<<std::endl;
            std::cerr<<"---F---"<<std::endl;
            // TODO: check status from addr2line?
            waitpid(pid, nullptr, 0);
            return output;
        }

        struct symbolizer::impl {
            std::vector<stacktrace_frame> resolve_frames(const std::vector<void*>& frames) {
                std::cerr<<"---B---"<<std::endl;
                std::vector<stacktrace_frame> trace(frames.size(), stacktrace_frame { 0, 0, 0, "", "" });
                if(has_addr2line()) {
                    std::cerr<<"---C---"<<std::endl;
                    std::vector<dlframe> dlframes = backtrace_frames(frames);
                    std::unordered_map<
                        std::string,
                        std::vector<std::pair<std::string, std::reference_wrapper<stacktrace_frame>>>
                    > entries;
                    for(size_t i = 0; i < dlframes.size(); i++) {
                        const auto& entry = dlframes[i];
                        entries[entry.obj_path].push_back({
                            to_hex(entry.raw_address - entry.obj_base),
                            trace[i]
                        });
                        // Set what is known for now, and resolutions from addr2line should overwrite
                        trace[i].filename = entry.obj_path;
                        trace[i].symbol = entry.symbol;
                    }
                    std::cerr<<"---D---"<<std::endl;
                    for(const auto& entry : entries) {
                        const auto& object_name = entry.first;
                        const auto& entries_vec = entry.second;
                        std::string address_input;
                        for(const auto& pair : entries_vec) {
                            address_input += pair.first;
                            address_input += '\n';
                        }
                        auto output = split(trim(resolve_addresses(address_input, object_name)), "\n");
                        internal_verify(output.size() == entries_vec.size());
                        for(size_t i = 0; i < output.size(); i++) {
                            // Result will be of the form <identifier> " at " path:line
                            // The path may be ?? if addr2line cannot resolve, line may be ?
                            // Edge cases:
                            // ?? ??:0
                            const auto& line = output[i];
                            std::size_t at_location = line.find(" at ");
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
                            auto colon = line.rfind(":");
                            internal_verify(colon != std::string::npos);
                            internal_verify(colon > filename_start);
                            auto filename = line.substr(filename_start, colon - filename_start);
                            auto line_number = line.substr(colon + 1);
                            if(line_number != "?") {
                                entries_vec[i].second.get().line = std::stoi(line_number);
                            }
                            if(filename != "??") {
                                entries_vec[i].second.get().filename = filename;
                            }
                            if(symbol != "") {
                                entries_vec[i].second.get().symbol = symbol;
                            }
                        }
                    }
                }
                return trace;
            }
        };

        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        //stacktrace_frame symbolizer::resolve_frame(void* addr) {
        //    return pimpl->resolve_frame(addr);
        //}

        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            std::cerr<<"---A---"<<std::endl;
            return pimpl->resolve_frames(frames);
        }
    }
}

#endif
