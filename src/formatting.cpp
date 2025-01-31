#include <cpptrace/formatting.hpp>
#include <cpptrace/utils.hpp>

#include "utils/optional.hpp"
#include "utils/utils.hpp"
#include "snippets/snippet.hpp"

#include <memory>
#include <cstdio>
#include <string>
#include <functional>
#include <iostream>
#include <sstream>

namespace cpptrace {
    class formatter::impl {
        std::string header = "Stack trace (most recent call first):";
        color_mode color = color_mode::automatic;
        address_mode addresses = address_mode::raw;
        bool snippets = false;
        int context_lines = 2;
        bool columns = true;
        std::function<bool(const stacktrace_frame&)> filter = [] (const stacktrace_frame&) { return true; };

    public:
        void set_header(std::string header) {
            this->header = std::move(header);
        }
        void set_color_mode(formatter::color_mode mode) {
            this->color = mode;
        }
        void set_address_mode(formatter::address_mode mode) {
            this->addresses = mode;
        }
        void set_snippets(bool snippets) {
            this->snippets = snippets;
        }
        void set_snippet_context(int lines) {
            this->context_lines = lines;
        }
        void include_column(bool columns) {
            this->columns = columns;
        }
        void set_filter(std::function<bool(const stacktrace_frame&)> filter) {
            this->filter = filter;
        }

        std::string format(const stacktrace_frame& frame, detail::optional<bool> color_override = detail::nullopt) const {
            return frame_to_string(frame, color_override.value_or(color == color_mode::always));
        }

        std::string format(const stacktrace& trace, detail::optional<bool> color_override = detail::nullopt) const {
            std::ostringstream oss;
            print_internal(oss, trace, false, color_override);
            return std::move(oss).str();
        }

        void print(const stacktrace_frame& frame, detail::optional<bool> color_override = detail::nullopt) const {
            print(std::cout, frame, color_override);
        }
        void print(
            std::ostream& stream,
            const stacktrace_frame& frame,
            detail::optional<bool> color_override = detail::nullopt
        ) const {
            print_frame_internal(stream, frame, color_override);
        }
        void print(
            std::FILE* file,
            const stacktrace_frame& frame,
            detail::optional<bool> color_override = detail::nullopt
        ) const {
            auto str = format(frame, color_override);
            std::fwrite(str.data(), 1, str.size(), file);
        }

        void print(const stacktrace& trace, detail::optional<bool> color_override = detail::nullopt) const {
            print(std::cout, trace, color_override);
        }
        void print(
            std::ostream& stream,
            const stacktrace& trace,
            detail::optional<bool> color_override = detail::nullopt
        ) const {
            print_internal(stream, trace, true, color_override);
        }
        void print(
            std::FILE* file,
            const stacktrace& trace,
            detail::optional<bool> color_override = detail::nullopt
        ) const {
            auto str = format(trace, color_override);
            std::fwrite(str.data(), 1, str.size(), file);
        }

    private:
        bool stream_is_tty(std::ostream& stream) const {
            // not great, but it'll have to do
            return (&stream == &std::cout && isatty(stdout_fileno))
                || (&stream == &std::cerr && isatty(stderr_fileno));
        }

        void maybe_ensure_virtual_terminal_processing(std::ostream& stream, bool color) const {
            if(color && stream_is_tty(stream)) {
                detail::enable_virtual_terminal_processing_if_needed();
            }
        }

        bool should_do_color(std::ostream& stream, detail::optional<bool> color_override) const {
            bool do_color = color == color_mode::always || color_override.value_or(false);
            if(
                (color == color_mode::automatic || color == color_mode::always) &&
                (!color_override || color_override.unwrap() != false) &&
                stream_is_tty(stream)
            ) {
                detail::enable_virtual_terminal_processing_if_needed();
                do_color = true;
            }
            return do_color;
        }

        void print_internal(std::ostream& stream, const stacktrace& trace, bool newline_at_end, detail::optional<bool> color_override) const {
            bool do_color = should_do_color(stream, color_override);
            maybe_ensure_virtual_terminal_processing(stream, do_color);
            print_internal(stream, trace, newline_at_end, do_color);
        }

        void print_internal(std::ostream& stream, const stacktrace& trace, bool newline_at_end, bool color) const {
            if(!header.empty()) {
                stream << header << '\n';
            }
            std::size_t counter = 0;
            const auto& frames = trace.frames;
            if(frames.empty()) {
                stream << "<empty trace>\n";
                return;
            }
            const auto frame_number_width = detail::n_digits(static_cast<int>(frames.size()) - 1);
            for(const auto& frame : frames) {
                print_frame_internal(stream, frame, color, frame_number_width, counter);
                if(newline_at_end || &frame != &frames.back()) {
                    stream << '\n';
                }
                if(frame.line.has_value() && !frame.filename.empty() && snippets) {
                    stream << detail::get_snippet(frame.filename, frame.line.value(), context_lines, color);
                }
                counter++;
            }
        }

        void print_frame_internal(
            std::ostream& stream,
            const stacktrace_frame& frame,
            bool color,
            unsigned frame_number_width,
            std::size_t counter
        ) const {
            std::string line = microfmt::format("#{<{}} {}", frame_number_width, counter, frame_to_string(frame, color));
            stream << line;
        }

        void print_frame_internal(
            std::ostream& stream,
            const stacktrace_frame& frame,
            detail::optional<bool> color_override
        ) const {
            bool do_color = should_do_color(stream, color_override);
            maybe_ensure_virtual_terminal_processing(stream, do_color);
            stream << frame_to_string(frame, do_color);
        }

        std::string frame_to_string(const stacktrace_frame& frame, bool color) const {
            const auto reset  = color ? RESET : "";
            const auto green  = color ? GREEN : "";
            const auto yellow = color ? YELLOW : "";
            const auto blue   = color ? BLUE : "";
            std::string str;
            if(frame.is_inline) {
                str += microfmt::format("{<{}}", 2 * sizeof(frame_ptr) + 2, "(inlined)");
            } else {
                auto address = addresses == address_mode::raw ? frame.raw_address : frame.object_address;
                str += microfmt::format("{}0x{>{}:0h}{}", blue, 2 * sizeof(frame_ptr), address, reset);
            }
            if(!frame.symbol.empty()) {
                str += microfmt::format(" in {}{}{}", yellow, frame.symbol, reset);
            }
            if(!frame.filename.empty()) {
                str += microfmt::format(" at {}{}{}", green, frame.filename, reset);
                if(frame.line.has_value()) {
                    str += microfmt::format(":{}{}{}", blue, frame.line.value(), reset);
                    if(frame.column.has_value() && columns) {
                        str += microfmt::format(":{}{}{}", blue, frame.column.value(), reset);
                    }
                }
            }
            return str;
        }
    };

    formatter::formatter() : pimpl(std::unique_ptr<impl>(new impl)) {}
    formatter::~formatter() = default;

    formatter::formatter(const formatter& other) : pimpl(std::unique_ptr<impl>(new impl(*other.pimpl))) {}
    formatter& formatter::operator=(const formatter& other) {
        pimpl = std::unique_ptr<impl>(new impl(*other.pimpl));
        return *this;
    }

    formatter& formatter::set_header(std::string header) {
        pimpl->set_header(std::move(header));
        return *this;
    }
    formatter& formatter::set_color_mode(color_mode mode) {
        pimpl->set_color_mode(mode);
        return *this;
    }
    formatter& formatter::set_address_mode(address_mode mode) {
        pimpl->set_address_mode(mode);
        return *this;
    }
    formatter& formatter::set_snippets(bool snippets) {
        pimpl->set_snippets(snippets);
        return *this;
    }
    formatter& formatter::set_snippet_context(int lines) {
        pimpl->set_snippet_context(lines);
        return *this;
    }
    formatter& formatter::include_column(bool columns) {
        pimpl->include_column(columns);
        return *this;
    }
    formatter& formatter::set_filter(std::function<bool(const stacktrace_frame&)> filter) {
        pimpl->set_filter(std::move(filter));
        return *this;
    }

    std::string formatter::format(const stacktrace_frame& frame) const {
        return pimpl->format(frame);
    }
    std::string formatter::format(const stacktrace_frame& frame, bool color) const {
        return pimpl->format(frame, color);
    }

    std::string formatter::format(const stacktrace& trace) const {
        return pimpl->format(trace);
    }
    std::string formatter::format(const stacktrace& trace, bool color) const {
        return pimpl->format(trace, color);
    }

    void formatter::print(const stacktrace& trace) const {
        pimpl->print(trace);
    }
    void formatter::print(const stacktrace& trace, bool color) const {
        pimpl->print(trace, color);
    }
    void formatter::print(std::ostream& stream, const stacktrace& trace) const {
        pimpl->print(stream, trace);
    }
    void formatter::print(std::ostream& stream, const stacktrace& trace, bool color) const {
        pimpl->print(stream, trace, color);
    }
    void formatter::print(std::FILE* file, const stacktrace& trace) const {
        pimpl->print(file, trace);
    }
    void formatter::print(std::FILE* file, const stacktrace& trace, bool color) const {
        pimpl->print(file, trace, color);
    }

    void formatter::print(const stacktrace_frame& frame) const {
        pimpl->print(frame);
    }
    void formatter::print(const stacktrace_frame& frame, bool color) const {
        pimpl->print(frame, color);
    }
    void formatter::print(std::ostream& stream, const stacktrace_frame& frame) const {
        pimpl->print(stream, frame);
    }
    void formatter::print(std::ostream& stream, const stacktrace_frame& frame, bool color) const {
        pimpl->print(stream, frame, color);
    }
    void formatter::print(std::FILE* file, const stacktrace_frame& frame) const {
        pimpl->print(file, frame);
    }
    void formatter::print(std::FILE* file, const stacktrace_frame& frame, bool color) const {
        pimpl->print(file, frame, color);
    }

    const formatter& get_default_formatter() {
        static formatter formatter;
        return formatter;
    }
}
