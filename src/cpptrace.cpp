#include <cpptrace/cpptrace.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "symbols/symbols.hpp"
#include "unwind/unwind.hpp"
#include "demangle/demangle.hpp"
#include "platform/common.hpp"
#include "platform/utils.hpp"
#include "platform/object.hpp"

#define ESC     "\033["
#define RESET   ESC "0m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define BLUE    ESC "34m"
#define MAGENTA ESC "35m"
#define CYAN    ESC "36m"

namespace cpptrace {
    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    raw_trace raw_trace::current(std::uint_least32_t skip) {
        return generate_raw_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    raw_trace raw_trace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_raw_trace(skip + 1, max_depth);
    }

    CPPTRACE_API
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

    CPPTRACE_API
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

    CPPTRACE_API
    void raw_trace::clear() {
        frames.clear();
    }

    CPPTRACE_API
    bool raw_trace::empty() const noexcept {
        return frames.empty();
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    object_trace object_trace::current(std::uint_least32_t skip) {
        return generate_object_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    object_trace object_trace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_object_trace(skip + 1, max_depth);
    }

    CPPTRACE_API
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

    CPPTRACE_API
    void object_trace::clear() {
        frames.clear();
    }

    CPPTRACE_API
    bool object_trace::empty() const noexcept {
        return frames.empty();
    }

    CPPTRACE_API std::string stacktrace_frame::to_string() const {
        std::ostringstream oss;
        oss << *this;
        return std::move(oss).str();
    }

    CPPTRACE_API std::ostream& operator<<(std::ostream& stream, const stacktrace_frame& frame) {
        stream
            << std::hex
            << "0x"
            << std::setw(2 * sizeof(uintptr_t))
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

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    stacktrace stacktrace::current(std::uint32_t skip) {
        return generate_trace(skip + 1);
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    stacktrace stacktrace::current(std::uint_least32_t skip, std::uint_least32_t max_depth) {
        return generate_trace(skip + 1, max_depth);
    }

    CPPTRACE_API
    void stacktrace::print() const {
        print(std::cerr, true);
    }

    CPPTRACE_API
    void stacktrace::print(std::ostream& stream) const {
        print(stream, true);
    }

    CPPTRACE_API
    void stacktrace::print(std::ostream& stream, bool color) const {
        print(stream, color, true);
    }

    CPPTRACE_API
    void stacktrace::print(std::ostream& stream, bool color, bool newline_at_end) const {
        if(color) {
            detail::enable_virtual_terminal_processing_if_needed();
        }
        stream<<"Stack trace (most recent call first):"<<std::endl;
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
                << " "
                << std::hex
                << blue
                << "0x"
                << std::setw(2 * sizeof(uintptr_t))
                << std::setfill('0')
                << frame.address
                << std::dec
                << std::setfill(' ')
                << reset
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

    CPPTRACE_API
    void stacktrace::clear() {
        frames.clear();
    }

    CPPTRACE_API
    bool stacktrace::empty() const noexcept {
        return frames.empty();
    }

    CPPTRACE_API
    std::string stacktrace::to_string() const {
        std::ostringstream oss;
        print(oss, false, false);
        return std::move(oss).str();
    }

    CPPTRACE_API std::ostream& operator<<(std::ostream& stream, const stacktrace& trace) {
        return stream << trace.to_string();
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
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

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
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

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
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

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
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

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    stacktrace generate_trace(std::uint_least32_t skip) {
        return generate_trace(skip + 1, UINT_LEAST32_MAX);
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    stacktrace generate_trace(std::uint32_t skip, std::uint_least32_t max_depth) {
        try {
            std::vector<uintptr_t> frames = detail::capture_frames(skip + 1, max_depth);
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

    CPPTRACE_API
    std::string demangle(const std::string& name) {
        return detail::demangle(name);
    }

    namespace detail {
        std::atomic_bool absorb_trace_exceptions(true); // NOSONAR
        std::atomic<enum cache_mode> cache_mode(cache_mode::prioritize_speed); // NOSONAR
    }

    CPPTRACE_API void absorb_trace_exceptions(bool absorb) {
        detail::absorb_trace_exceptions = absorb;
    }

    namespace experimental {
        CPPTRACE_API void set_cache_mode(cache_mode mode) {
            detail::cache_mode = mode;
        }
    }

    namespace detail {
        CPPTRACE_API bool should_absorb_trace_exceptions() {
            return absorb_trace_exceptions;
        }

        CPPTRACE_API enum cache_mode get_cache_mode() {
            return cache_mode;
        }
    }
}
