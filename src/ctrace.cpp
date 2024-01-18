#include <ctrace/ctrace.h>
#include <cpptrace/cpptrace.hpp>
#include <algorithm>

#include "symbols/symbols.hpp"
#include "unwind/unwind.hpp"
#include "demangle/demangle.hpp"
#include "utils/exception_type.hpp"
#include "utils/common.hpp"
#include "utils/utils.hpp"
#include "binary/object.hpp"
#include "binary/safe_dl.hpp"

#define ESC     "\033["
#define RESET   ESC "0m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define BLUE    ESC "34m"
#define MAGENTA ESC "35m"
#define CYAN    ESC "36m"

namespace cpp_detail = cpptrace::detail;

namespace ctrace {
    static constexpr std::uint32_t invalid_pos = ~0U;

    template <typename...Args>
    static void ffprintf(std::FILE* f, const char fmt[], Args&&...args) {
        (void)std::fprintf(f, fmt, args...);
        fflush(f);
    }

    static bool is_empty(std::uint32_t pos) noexcept {
        return (pos == invalid_pos);
    }

    static bool is_empty(const char* str) noexcept {
        return (!str) || (std::char_traits<char>::length(str) == 0);
    }

    static ctrace_owning_string generate_owning_string(const char* raw_string) noexcept {
        // Returns length to the null terminator.
        std::size_t count = std::char_traits<char>::length(raw_string);
        char* new_string = new char[count + 1];
        std::char_traits<char>::copy(new_string, raw_string, count);
        new_string[count] = '\0';
        return { new_string };
    }

    static ctrace_owning_string generate_owning_string(const std::string& std_string) {
        return generate_owning_string(std_string.c_str());
    }

    static void free_owning_string(const char* owned_string) noexcept {
        if(!owned_string) return;
        // ok because we allocated it.
        char* data = const_cast<char*>(owned_string);
        delete[] data;
    }

    static void free_owning_string(ctrace_owning_string& owned_string) noexcept {
        free_owning_string(owned_string.data);
    }

    static ctrace_object_trace c_convert(const std::vector<cpptrace::object_frame>& trace) {
        std::size_t count = trace.size();
        auto* frames = new ctrace_object_frame[count];
        std::transform(trace.begin(), trace.end(), frames,
        [] (const cpptrace::object_frame& frame) -> ctrace_object_frame {
            const char* new_path = generate_owning_string(frame.object_path).data;
            return { frame.raw_address, frame.object_address, new_path };
        });
        return { frames, count };
    }

    static ctrace_stacktrace c_convert(const std::vector<cpptrace::stacktrace_frame>& trace) {
        std::size_t count = trace.size();
        auto* frames = new ctrace_stacktrace_frame[count];
        std::transform(trace.begin(), trace.end(), frames,
        [] (const cpptrace::stacktrace_frame& frame) -> ctrace_stacktrace_frame {
            ctrace_stacktrace_frame new_frame;
            new_frame.address = frame.address;
            new_frame.line = frame.line.value_or(invalid_pos);
            new_frame.column = frame.column.value_or(invalid_pos);
            new_frame.filename = generate_owning_string(frame.filename).data;
            new_frame.symbol = generate_owning_string(
                cpp_detail::demangle(frame.symbol)).data;
            new_frame.is_inline = ctrace_bool(frame.is_inline);
            return new_frame;
        });
        return { frames, count };
    }
}

extern "C" {
    // ctrace::string
    ctrace_owning_string ctrace_generate_owning_string(const char* raw_string) {
        return ctrace::generate_owning_string(raw_string);
    }

    void ctrace_free_owning_string(ctrace_owning_string* string) {
        if(!string) return;
        ctrace::free_owning_string(*string);
        string->data = nullptr;
    }

    // ctrace::generation:
    CTRACE_FORCE_NO_INLINE
    ctrace_raw_trace ctrace_generate_raw_trace(size_t skip, size_t max_depth) {
        try {
            std::vector<cpptrace::frame_ptr> trace = 
                cpp_detail::capture_frames(skip + 1, max_depth);
            std::size_t count = trace.size();
            auto* frames = new ctrace_frame_ptr[count];
            std::copy(trace.data(), trace.data() + count, frames);
            return { frames, count };
        } catch(...) {
            // Don't check rethrow condition, it's risky.
            return { nullptr, 0 };
        }
    }

    CTRACE_FORCE_NO_INLINE
    ctrace_object_trace ctrace_generate_object_trace(size_t skip, size_t max_depth) {
        try {
            std::vector<cpptrace::object_frame> trace = 
                cpp_detail::get_frames_object_info(
                    cpp_detail::capture_frames(skip + 1, max_depth));
            return ctrace::c_convert(trace);
        } catch(...) {
            // Don't check rethrow condition, it's risky.
            return { nullptr, 0 };
        }
    }

    CTRACE_FORCE_NO_INLINE
    ctrace_stacktrace ctrace_generate_trace(size_t skip, size_t max_depth) {
        try {
            std::vector<cpptrace::frame_ptr> frames = cpp_detail::capture_frames(skip + 1, max_depth);
            std::vector<cpptrace::stacktrace_frame> trace = cpp_detail::resolve_frames(frames);
            return ctrace::c_convert(trace);
        } catch(...) {
            // Don't check rethrow condition, it's risky.
            return { nullptr, 0 };
        }
    }


    // ctrace::freeing:
    void ctrace_free_raw_trace(ctrace_raw_trace* trace) {
        if(!trace) return;
        ctrace_frame_ptr* frames = trace->frames;
        delete[] frames;
        trace->frames = nullptr;
        trace->count = 0;
    }

    void ctrace_free_object_trace(ctrace_object_trace* trace) {
        if(!trace || !trace->frames) return;
        ctrace_object_frame* frames = trace->frames;
        for(std::size_t I = 0, E = trace->count; I < E; ++I) {
            const char* path = frames[I].obj_path;
            ctrace::free_owning_string(path);
        }

        delete[] frames;
        trace->frames = nullptr;
        trace->count = 0;
    }

    void ctrace_free_stacktrace(ctrace_stacktrace* trace) {
        if(!trace || !trace->frames) return;
        ctrace_stacktrace_frame* frames = trace->frames;
        for(std::size_t I = 0, E = trace->count; I < E; ++I) {
            ctrace::free_owning_string(frames[I].filename);
            ctrace::free_owning_string(frames[I].symbol);
        }

        delete[] frames;
        trace->frames = nullptr;
        trace->count = 0;
    }

    // ctrace::resolve:

    // ctrace::io:
    ctrace_owning_string ctrace_stacktrace_to_string(const ctrace_stacktrace* trace, ctrace_bool use_color) {
        // TODO: Implement
        (void)trace;
        (void)use_color;
        return ctrace::generate_owning_string("");
    }

    void ctrace_stacktrace_print(const ctrace_stacktrace* trace, FILE* to, ctrace_bool use_color) {
        if(use_color) cpp_detail::enable_virtual_terminal_processing_if_needed();
        ctrace::ffprintf(to, "Stack trace (most recent call first):\n");
        if(trace->count == 0 || !trace->frames) {
            ctrace::ffprintf(to, "<empty trace>\n");
            return;
        }
        const auto reset   = use_color ? ESC "0m" : "";
        const auto green   = use_color ? ESC "32m" : "";
        const auto yellow  = use_color ? ESC "33m" : "";
        const auto blue    = use_color ? ESC "34m" : "";
        const auto frame_number_width = cpp_detail::n_digits(unsigned(trace->count - 1));
        ctrace_stacktrace_frame* frames = trace->frames;
        for(std::size_t I = 0, E = trace->count; I < E; ++I) {
            static constexpr auto ptr_len = 2 * sizeof(cpptrace::frame_ptr);
            ctrace::ffprintf(to, "#%-*zu ", int(frame_number_width), I);
            if(frames[I].is_inline) {
                (void)std::fprintf(to, "%*s", 
                    int(ptr_len + 2), 
                    "(inlined)");
            } else {
                (void)std::fprintf(to, "%s0x%0*zx%s",
                    blue, 
                    int(ptr_len), 
                    std::size_t(frames[I].address), 
                    reset);
            }
            if(!ctrace::is_empty(frames[I].symbol)) {
                (void)std::fprintf(to, " in %s%s%s", 
                    yellow, 
                    frames[I].symbol, 
                    reset);
            }
            if(!ctrace::is_empty(frames[I].filename)) {
                (void)std::fprintf(to, " at %s%s%s",
                    green, 
                    frames[I].filename, 
                    reset);
                if(ctrace::is_empty(frames[I].line)) goto end;
                (void)std::fprintf(to, ":%s%zu%s", 
                    blue, 
                    std::size_t(frames[I].line), 
                    reset);
                if(ctrace::is_empty(frames[I].column)) goto end;
                (void)std::fprintf(to, ":%s%zu%s", 
                    blue, 
                    std::size_t(frames[I].column), 
                    reset);
            }
            // always print newline at end :M
            end: ctrace::ffprintf(to, "\n");
        }
    }

    // utility::demangle:
    ctrace_owning_string ctrace_demangle(const char* mangled) {
        if(!mangled) return ctrace::generate_owning_string("");
        std::string demangled = cpptrace::demangle(mangled);
        return ctrace::generate_owning_string(demangled);
    }

    // utility::io
    int ctrace_stdin_fileno(void) {
        return cpptrace::stdin_fileno;
    }

    int ctrace_stderr_fileno(void) {
        return cpptrace::stderr_fileno;
    }

    int ctrace_stdout_fileno(void) {
        return cpptrace::stdout_fileno;
    }

    ctrace_bool ctrace_isatty(int fd) {
        return cpptrace::isatty(fd);
    }

    // utility::cache:
    void ctrace_set_cache_mode(ctrace_cache_mode mode) {
        static constexpr auto cache_max = cpptrace::cache_mode::prioritize_speed;
        if(mode > unsigned(cache_max)) return;
        auto cache_mode = static_cast<cpptrace::cache_mode>(mode);
        cpptrace::experimental::set_cache_mode(cache_mode);
    }

    ctrace_cache_mode ctrace_get_cache_mode(void) {
        auto cache_mode = cpp_detail::get_cache_mode();
        return static_cast<ctrace_cache_mode>(cache_mode);
    }
}
