#ifndef SNIPPET_HPP
#define SNIPPET_HPP

#include <cstddef>
#include <string>

namespace cpptrace {
namespace detail {
    std::string get_snippet(const std::string& path, std::size_t line, bool color);
}
}

#endif
