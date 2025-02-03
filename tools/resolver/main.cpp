#include "cpptrace/formatting.hpp"
#include "cpptrace/forward.hpp"
#include <lyra/lyra.hpp>
#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ostream.h>
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

#include <filesystem>
#include <stdexcept>

#include "symbols/symbols.hpp"
#include "demangle/demangle.hpp"

using namespace std::literals;
using namespace cpptrace::detail;

template<> struct fmt::formatter<lyra::cli> : ostream_formatter {};

void resolve(std::filesystem::path path, const std::vector<cpptrace::frame_ptr>& addresses) {
    auto formatter = cpptrace::formatter{}.addresses(cpptrace::formatter::address_mode::object);
    for(const auto& address : addresses) {
        cpptrace::object_frame obj_frame{0, address, path};
        std::vector<cpptrace::stacktrace_frame> trace = cpptrace::detail::resolve_frames({obj_frame});
        if(trace.size() != 1) {
            throw std::runtime_error("Something went wrong, trace vector size didn't match");
        }
        trace[0].symbol = cpptrace::detail::demangle(trace[0].symbol);
        formatter.print(trace[0]);
        std::cout<<std::endl;
    }
}

int main(int argc, char** argv) CPPTRACE_TRY {
    bool show_help = false;
    std::filesystem::path path;
    std::vector<std::string> address_strings;
    auto cli = lyra::cli()
        | lyra::help(show_help)
        | lyra::arg(path, "binary path")("binary to look in").required()
        | lyra::arg(address_strings, "addresses")("addresses").required();
    if(auto result = cli.parse({ argc, argv }); !result) {
        fmt::println(stderr, "Error in command line: {}", result.message());
        fmt::println("{}", cli);
        return 1;
    }
    if(show_help) {
        fmt::println("{}", cli);
        return 0;
    }
    if(!std::filesystem::exists(path)) {
        fmt::println(stderr, "Error: Path doesn't exist {}", path);
        return 1;
    }
    if(!std::filesystem::is_regular_file(path)) {
        fmt::println(stderr, "Error: Path isn't a regular file {}", path);
        return 1;
    }
    std::vector<cpptrace::frame_ptr> addresses;
    for(const auto& address : address_strings) {
        addresses.push_back(std::stoi(address, nullptr, 16));
    }
    resolve(path, addresses);
} CPPTRACE_CATCH(const std::exception& e) {
    fmt::println(stderr, "Caught exception {}: {}", cpptrace::demangle(typeid(e).name()), e.what());
    cpptrace::from_current_exception().print();
}
