#ifndef REPLACE_ALL
#define REPLACE_ALL

#include <string>
#include <regex>

namespace cpptrace {
namespace detail {
    // replace all instances of substr with the replacement
    void replace_all(std::string& str, std::string_view substr, std::string_view replacement);

    // replace all regex matches with the replacement
    void replace_all(std::string& str, const std::regex& re, std::string_view replacement);

    // replace all instances of substr with the replacement, including new instances introduced by the replacement
    void replace_all_dynamic(std::string& str, std::string_view substr, std::string_view replacement);

    // replace all matches of a regex including template parameters
    void replace_all_template(std::string& str, const std::pair<std::regex, std::string_view>& rule);
}
}

#endif
