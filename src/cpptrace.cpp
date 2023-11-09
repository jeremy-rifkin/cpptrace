#include <cpptrace/cpptrace.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "symbols/symbols.hpp"
#include "unwind/unwind.hpp"
#include "demangle/demangle.hpp"
#include "platform/common.hpp"
#include "platform/exception_type.hpp"
#include "platform/object.hpp"
#include "platform/utils.hpp"

#define ESC     "\033["
#define RESET   ESC "0m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define BLUE    ESC "34m"
#define MAGENTA ESC "35m"
#define CYAN    ESC "36m"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE
    raw_trace raw_trace::current(std::uint_least32_t skip) {
        return generate_raw_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE
    raw_trace raw_trace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_raw_trace(skip + 1, max_depth);
    }

    object_trace raw_trace::resolve_object_trace() const {
        try {
            return object_trace{detail::get_frames_object_info(frames)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return object_trace{};
        }
    }

    stacktrace raw_trace::resolve() const {
        try {
            std::vector<stacktrace_frame> trace = detail::resolve_frames(frames);
            for(auto& frame : trace) {
                frame.symbol = detail::demangle(frame.symbol);
            }
            return stacktrace{std::move(trace)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return stacktrace{};
        }
    }

    void raw_trace::clear() {
        frames.clear();
    }

    bool raw_trace::empty() const noexcept {
        return frames.empty();
    }

    CPPTRACE_FORCE_NO_INLINE
    object_trace object_trace::current(std::uint_least32_t skip) {
        return generate_object_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE
    object_trace object_trace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_object_trace(skip + 1, max_depth);
    }

    stacktrace object_trace::resolve() const {
        try {
            return stacktrace{detail::resolve_frames(frames)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return stacktrace();
        }
    }

    void object_trace::clear() {
        frames.clear();
    }

    bool object_trace::empty() const noexcept {
        return frames.empty();
    }

    std::string stacktrace_frame::to_string() const {
        std::ostringstream oss;
        oss << *this;
        return std::move(oss).str();
    }

    std::ostream& operator<<(std::ostream& stream, const stacktrace_frame& frame) {
        stream
            << std::hex
            << "0x"
            << std::setw(2 * sizeof(frame_ptr))
            << std::setfill('0')
            << frame.address
            << std::dec
            << std::setfill(' ')
            << " in "
            << frame.symbol
            << " at "
            << frame.filename;
        if(frame.line != 0) {
            stream
                << ":"
                << frame.line;
            if(frame.column != UINT_LEAST32_MAX) {
                stream << frame.column;
            }
        }
        return stream;
    }

    CPPTRACE_FORCE_NO_INLINE
    stacktrace stacktrace::current(std::uint32_t skip) {
        return generate_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE
    stacktrace stacktrace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_trace(skip + 1, max_depth);
    }

    void stacktrace::print() const {
        print(std::cerr, true);
    }

    void stacktrace::print(std::ostream& stream) const {
        print(stream, true);
    }

    void stacktrace::print(std::ostream& stream, bool color) const {
        print(stream, color, true, nullptr);
    }

    void stacktrace::print(std::ostream& stream, bool color, bool newline_at_end, const char* header) const {
        if(color) {
            detail::enable_virtual_terminal_processing_if_needed();
        }
        stream<<(header ? header : "Stack trace (most recent call first):")<<std::endl;
        std::size_t counter = 0;
        if(frames.empty()) {
            stream<<"<empty trace>"<<std::endl;
            return;
        }
        const auto reset   = color ? ESC "0m" : "";
        const auto green   = color ? ESC "32m" : "";
        const auto yellow  = color ? ESC "33m" : "";
        const auto blue    = color ? ESC "34m" : "";
        const auto frame_number_width = detail::n_digits(static_cast<int>(frames.size()) - 1);
        for(const auto& frame : frames) {
            stream
                << '#'
                << std::setw(static_cast<int>(frame_number_width))
                << std::left
                << counter
                << std::right
                << " ";
            if(frame.is_inline) {
                stream
                    << std::setw(2 * sizeof(frame_ptr) + 2)
                    << "(inlined)";
            } else {
                stream
                    << std::hex
                    << blue
                    << "0x"
                    << std::setw(2 * sizeof(frame_ptr))
                    << std::setfill('0')
                    << frame.address
                    << std::dec
                    << std::setfill(' ')
                    << reset;
            }
            stream
                << " in "
                << yellow
                << frame.symbol
                << reset
                << " at "
                << green
                << frame.filename
                << reset;
            if(frame.line != 0) {
                stream
                    << ":"
                    << blue
                    << frame.line
                    << reset;
                if(frame.column != UINT_LEAST32_MAX) {
                    stream << ':'
                           << blue
                           << std::to_string(frame.column)
                           << reset;
                }
            }
            if(newline_at_end || &frame != &frames.back()) {
                stream << std::endl;
            }
            counter++;
        }
    }

    void stacktrace::clear() {
        frames.clear();
    }

    bool stacktrace::empty() const noexcept {
        return frames.empty();
    }

    std::string stacktrace::to_string(bool color) const {
        std::ostringstream oss;
        print(oss, color, false, nullptr);
        return std::move(oss).str();
    }

    std::ostream& operator<<(std::ostream& stream, const stacktrace& trace) {
        return stream << trace.to_string();
    }

    CPPTRACE_FORCE_NO_INLINE
    raw_trace generate_raw_trace(std::uint_least32_t skip) {
        try {
            return raw_trace{detail::capture_frames(skip + 1, UINT_LEAST32_MAX)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return raw_trace{};
        }
    }

    CPPTRACE_FORCE_NO_INLINE
    raw_trace generate_raw_trace(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        try {
            return raw_trace{detail::capture_frames(skip + 1, max_depth)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return raw_trace{};
        }
    }

    CPPTRACE_FORCE_NO_INLINE
    object_trace generate_object_trace(std::uint_least32_t skip) {
        try {
            return object_trace{detail::get_frames_object_info(detail::capture_frames(skip + 1, UINT_LEAST32_MAX))};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return object_trace{};
        }
    }

    CPPTRACE_FORCE_NO_INLINE
    object_trace generate_object_trace(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        try {
            return object_trace{detail::get_frames_object_info(detail::capture_frames(skip + 1, max_depth))};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return object_trace{};
        }
    }

    CPPTRACE_FORCE_NO_INLINE
    stacktrace generate_trace(std::uint_least32_t skip) {
        return generate_trace(skip + 1, UINT_LEAST32_MAX);
    }

    CPPTRACE_FORCE_NO_INLINE
    stacktrace generate_trace(std::uint32_t skip, std::uint_least32_t max_depth) {
        try {
            std::vector<frame_ptr> frames = detail::capture_frames(skip + 1, max_depth);
            std::vector<stacktrace_frame> trace = detail::resolve_frames(frames);
            for(auto& frame : trace) {
                frame.symbol = detail::demangle(frame.symbol);
            }
            return stacktrace{std::move(trace)};
        } catch(...) { // NOSONAR
            if(!detail::should_absorb_trace_exceptions()) {
                throw;
            }
            return stacktrace();
        }
    }

    std::string demangle(const std::string& name) {
        return detail::demangle(name);
    }

    bool isatty(int fd) {
        return detail::isatty(fd);
    }

    extern const int stdin_fileno = detail::fileno(stdin);
    extern const int stdout_fileno = detail::fileno(stdout);
    extern const int stderr_fileno = detail::fileno(stderr);

    CPPTRACE_FORCE_NO_INLINE void print_terminate_trace() {
        generate_trace(1).print(
            std::cerr,
            isatty(stderr_fileno),
            true,
            "Stack trace to reach terminate handler (most recent call first):"
        );
    }

    [[noreturn]] void terminate_handler() {
        try {
            auto ptr = std::current_exception();
            if(ptr == nullptr) {
                std::cerr << "terminate called without an active exception\n";
                print_terminate_trace();
            } else {
                std::rethrow_exception(ptr);
            }
        } catch(cpptrace::exception& e) {
            std::cerr << "Terminate called after throwing an instance of "
                      << demangle(typeid(e).name())
                      << ": "
                      << e.get_raw_what()
                      << '\n';
            e.get_trace().print(std::cerr, isatty(stderr_fileno));
        } catch(std::exception& e) {
            std::cerr << "Terminate called after throwing an instance of "
                      << demangle(typeid(e).name())
                      << ": "
                      << e.what()
                      << '\n';
            print_terminate_trace();
        } catch(...) {
            std::cerr << "Terminate called after throwing an instance of "
                      << detail::exception_type_name()
                      << "\n";
            print_terminate_trace();
        }
        std::flush(std::cerr);
        abort();
    }

    void register_terminate_handler() {
        std::set_terminate(terminate_handler);
    }

    namespace detail {
        std::atomic_bool absorb_trace_exceptions(true); // NOSONAR
        std::atomic<enum cache_mode> cache_mode(cache_mode::prioritize_speed); // NOSONAR
    }

     void absorb_trace_exceptions(bool absorb) {
        detail::absorb_trace_exceptions = absorb;
    }

    namespace experimental {
         void set_cache_mode(cache_mode mode) {
            detail::cache_mode = mode;
        }
    }

    namespace detail {
         bool should_absorb_trace_exceptions() {
            return absorb_trace_exceptions;
        }

         enum cache_mode get_cache_mode() {
            return cache_mode;
        }

        CPPTRACE_FORCE_NO_INLINE
        raw_trace get_raw_trace_and_absorb(std::uint_least32_t skip, std::uint_least32_t max_depth) noexcept {
            try {
                return generate_raw_trace(skip + 1, max_depth);
            } catch(const std::exception& e) {
                if(!detail::should_absorb_trace_exceptions()) {
                    // TODO: Append to message somehow
                    std::fprintf(
                        stderr,
                        "Cpptrace: Exception ocurred while resolving trace in cpptrace::exception object:\n%s\n",
                        e.what()
                    );
                }
                return raw_trace{};
            }
        }
    }

    exception::exception(std::uint_least32_t skip, std::uint_least32_t max_depth) noexcept
            : trace(detail::get_raw_trace_and_absorb(skip + 1, max_depth)) {}

    const char* exception::what() const noexcept {
        return get_what().c_str();
    }

    const std::string& exception::get_what() const noexcept {
        if(what_string.empty()) {
            what_string = get_raw_what() + std::string(":\n") + get_trace().to_string();
        }
        return what_string;
    }

    const char* exception::get_raw_what() const noexcept {
        return "cpptrace::exception";
    }

    const raw_trace& exception::get_raw_trace() const noexcept {
        return trace;
    }

    const stacktrace& exception::get_trace() const noexcept {
        // I think a non-empty raw trace can never resolve as empty, so this will accurately prevent resolving more
        // than once. Either way the raw trace is cleared.
        try {
            if(resolved_trace.empty() && !trace.empty()) {
                resolved_trace = trace.resolve();
                trace.clear();
            }
        } catch(const std::exception& e) {
            if(!detail::should_absorb_trace_exceptions()) {
                // TODO: Append to message somehow
                std::fprintf(
                    stderr,
                    "Exception ocurred while resolving trace in cpptrace::exception object:\n%s\n",
                    e.what()
                );
            }
        }
        return resolved_trace;
    }

    const char* exception_with_message::get_raw_what() const noexcept {
        return message.c_str();
    }
}
