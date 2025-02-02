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
        struct {
            std::string header = "Stack trace (most recent call first):";
            color_mode color = color_mode::automatic;
            address_mode addresses = address_mode::raw;
            bool snippets = false;
            int context_lines = 2;
            bool columns = true;
            bool show_filtered_frames = true;
            std::function<bool(const stacktrace_frame&)> filter;
        } options;

    public:
        void set_header(std::string header) {
            options.header = std::move(header);
        }
        void set_color_mode(formatter::color_mode mode) {
            options.color = mode;
        }
        void set_address_mode(formatter::address_mode mode) {
            options.addresses = mode;
        }
        void set_snippets(bool snippets) {
            options.snippets = snippets;
        }
        void set_snippet_context(int lines) {
            options.context_lines = lines;
        }
        void include_column(bool columns) {
            options.columns = columns;
        }
        void show_filtered_frames(bool show) {
            options.show_filtered_frames = show;
        }
        void set_filter(std::function<bool(const stacktrace_frame&)> filter) {
            options.filter = filter;
        }

        std::string format(const stacktrace_frame& frame, detail::optional<bool> color_override = detail::nullopt) const {
            std::ostringstream oss;
            print_frame_inner(oss, frame, color_override.value_or(options.color == color_mode::always));
            return std::move(oss).str();
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
            bool do_color = options.color == color_mode::always || color_override.value_or(false);
            if(
                (options.color == color_mode::automatic || options.color == color_mode::always) &&
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
            if(!options.header.empty()) {
                stream << options.header << '\n';
            }
            std::size_t counter = 0;
            const auto& frames = trace.frames;
            if(frames.empty()) {
                stream << "<empty trace>\n";
                return;
            }
            const auto frame_number_width = detail::n_digits(static_cast<int>(frames.size()) - 1);
            for(const auto& frame : frames) {
                if(options.filter && !options.filter(frame)) {
                    if(!options.show_filtered_frames) {
                        counter++;
                        continue;
                    }
                    print_placeholder_frame(stream, frame_number_width, counter);
                } else {
                    print_frame_internal(stream, frame, color, frame_number_width, counter);
                    if(frame.line.has_value() && !frame.filename.empty() && options.snippets) {
                        auto snippet = detail::get_snippet(
                            frame.filename,
                            frame.line.value(),
                            options.context_lines,
                            color
                        );
                        if(!snippet.empty()) {
                            stream << '\n';
                            stream << snippet;
                        }
                    }
                }
                if(newline_at_end || &frame != &frames.back()) {
                    stream << '\n';
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
            microfmt::print(stream, "#{<{}} ", frame_number_width, counter);
            print_frame_inner(stream, frame, color);
        }

        void print_placeholder_frame(std::ostream& stream, unsigned frame_number_width, std::size_t counter) const {
            microfmt::print(stream, "#{<{}} (filtered)", frame_number_width, counter);
        }

        void print_frame_internal(
            std::ostream& stream,
            const stacktrace_frame& frame,
            detail::optional<bool> color_override
        ) const {
            bool do_color = should_do_color(stream, color_override);
            maybe_ensure_virtual_terminal_processing(stream, do_color);
            print_frame_inner(stream, frame, do_color);
        }

        void print_frame_inner(std::ostream& stream, const stacktrace_frame& frame, bool color) const {
            const auto reset  = color ? RESET : "";
            const auto green  = color ? GREEN : "";
            const auto yellow = color ? YELLOW : "";
            const auto blue   = color ? BLUE : "";
            if(frame.is_inline) {
                microfmt::print(stream, "{<{}}", 2 * sizeof(frame_ptr) + 2, "(inlined)");
            } else {
                auto address = options.addresses == address_mode::raw ? frame.raw_address : frame.object_address;
                microfmt::print(stream, "{}0x{>{}:0h}{}", blue, 2 * sizeof(frame_ptr), address, reset);
            }
            if(!frame.symbol.empty()) {
                microfmt::print(stream, " in {}{}{}", yellow, frame.symbol, reset);
            }
            if(!frame.filename.empty()) {
                microfmt::print(stream, " at {}{}{}", green, frame.filename, reset);
                if(frame.line.has_value()) {
                    microfmt::print(stream, ":{}{}{}", blue, frame.line.value(), reset);
                    if(frame.column.has_value() && options.columns) {
                        microfmt::print(stream, ":{}{}{}", blue, frame.column.value(), reset);
                    }
                }
            }
        }
    };

    formatter::formatter() : pimpl(new impl) {}
    formatter::~formatter() {
        delete pimpl;
    }

    formatter::formatter(formatter&& other) : pimpl(detail::exchange(other.pimpl, nullptr)) {}
    formatter::formatter(const formatter& other) : pimpl(new impl(*other.pimpl)) {}
    formatter& formatter::operator=(formatter&& other) {
        if(pimpl) {
            delete pimpl;
        }
        pimpl = detail::exchange(other.pimpl, nullptr);
        return *this;
    }
    formatter& formatter::operator=(const formatter& other) {
        if(pimpl) {
            delete pimpl;
        }
        pimpl = new impl(*other.pimpl);
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
    formatter& formatter::show_filtered_frames(bool show) {
        pimpl->show_filtered_frames(show);
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
