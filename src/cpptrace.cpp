#include <cpptrace/cpptrace.hpp>

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
    object_trace raw_trace::resolve_object_trace() const {
        return object_trace(detail::get_frames_object_info(frames));
    }

    stacktrace raw_trace::resolve() const {
        std::vector<stacktrace_frame> trace = detail::resolve_frames(frames);
        for(auto& frame : trace) {
            frame.symbol = detail::demangle(frame.symbol);
        }
        return stacktrace(std::move(trace));
    }

    void raw_trace::clear() {
        frames.clear();
    }

    stacktrace object_trace::resolve() const {
        return stacktrace(detail::resolve_frames(frames));
    }

    void object_trace::clear() {
        frames.clear();
    }

    void stacktrace::print() const {
        print(std::cerr, true);
    }

    void stacktrace::print(std::ostream& stream) const {
        print(stream, true);
    }

    void stacktrace::print(std::ostream& stream, bool color) const {
        if(color) {
            detail::enable_virtual_terminal_processing_if_needed();
        }
        stream<<"Stack trace (most recent call first):"<<std::endl;
        std::size_t counter = 0;
        if(frames.empty()) {
            stream<<"<empty trace>"<<std::endl;
            return;
        }
        const auto frame_number_width = detail::n_digits(static_cast<int>(frames.size()) - 1);
        for(const auto& frame : frames) {
            stream
                << '#'
                << std::setw(static_cast<int>(frame_number_width))
                << std::left
                << counter++
                << std::right
                << " "
                << std::hex
                << (color ? BLUE : "")
                << "0x"
                << std::setw(2 * sizeof(uintptr_t))
                << std::setfill('0')
                << frame.address
                << std::dec
                << std::setfill(' ')
                << (color ? RESET : "")
                << " in "
                << (color ? YELLOW : "")
                << frame.symbol
                << (color ? RESET : "")
                << " at "
                << (color ? GREEN : "")
                << frame.filename
                << (color ? RESET : "")
                << ":"
                << (color ? BLUE : "")
                << frame.line
                << (color ? RESET : "")
                << (frame.col > 0 ? (color ? ":" BLUE : ":") + std::to_string(frame.col) + (color ? RESET : "") : "")
                << std::endl;
        }
    }

    std::string stacktrace::to_string() const {
        std::ostringstream oss;
        print(oss, false);
        return std::move(oss).str();
    }

    void stacktrace::clear() {
        frames.clear();
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    raw_trace generate_raw_trace(std::uint32_t skip) {
        return raw_trace(detail::capture_frames(skip + 1));
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    object_trace generate_object_trace(std::uint32_t skip) {
        return object_trace(detail::get_frames_object_info(detail::capture_frames(skip + 1)));
    }

    CPPTRACE_FORCE_NO_INLINE CPPTRACE_API
    stacktrace generate_trace(std::uint32_t skip) {
        std::vector<uintptr_t> frames = detail::capture_frames(skip + 1);
        std::vector<stacktrace_frame> trace = detail::resolve_frames(frames);
        for(auto& frame : trace) {
            frame.symbol = detail::demangle(frame.symbol);
        }
        return stacktrace(std::move(trace));
    }

    CPPTRACE_API
    std::string demangle(const std::string& str) {
        return detail::demangle(str);
    }
}
