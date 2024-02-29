#include "snippet.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>

#include "../utils/common.hpp"

namespace cpptrace {
namespace detail {
    constexpr std::int64_t max_size = 1024 * 1024 * 10; // 10 MiB

    class snippet_manager {
        bool loaded_contents;
        std::string contents;
        // for index i, gives the index in `contents` of one past the end of the line (i.e. the \n or contents.end())
        std::vector<std::size_t> line_table;
    public:
        snippet_manager(const std::string& path) : loaded_contents(false) {
            std::ifstream file;
            try {
                file.open(path, std::ios::ate);
                if(file.is_open()) {
                    std::ifstream::pos_type size = file.tellg();
                    if(size == std::ifstream::pos_type(-1) || size > max_size) {
                        return;
                    }
                    // else load file
                    file.seekg(0, std::ios::beg);
                    contents.resize(size);
                    if(!file.read(&contents[0], size)) {
                        // error ...
                    }
                    build_line_table();
                    loaded_contents = true;
                }
            } catch(const std::ifstream::failure&) {
                // ...
            }
        }

        std::string get_line(std::size_t line) const { // 0-indexed line TODO: reconsider
            if(!loaded_contents || line >= line_table.size()) {
                return "";
            } else if(line == 0) {
                return contents.substr(0, line_table[line]);
            }  else {
                return contents.substr(line_table[line - 1] + 1, line_table[line] - line_table[line - 1] - 1);
            }
        }

        std::size_t num_lines() const {
            return line_table.size();
        }

        bool ok() const {
            return loaded_contents;
        }
    private:
        void build_line_table() {
            std::size_t pos = 0;
            while(true) {
                std::size_t new_pos = contents.find('\n', pos);
                if(new_pos == std::string::npos) {
                    line_table.push_back(contents.size());
                    break;
                } else {
                    line_table.push_back(new_pos);
                    pos = new_pos + 1;
                }
            }
        }
    };

    std::mutex snippet_manager_mutex;
    std::unordered_map<std::string, const snippet_manager> snippet_managers;

    const snippet_manager& get_manager(const std::string& path) {
        std::unique_lock<std::mutex> lock(snippet_manager_mutex);
        auto it = snippet_managers.find(path);
        if(it == snippet_managers.end()) {
            return snippet_managers.insert({path, snippet_manager(path)}).first->second;
        } else {
            return it->second;
        }
    }

    std::string get_snippet(const std::string& path, std::size_t target_line, std::size_t context_size, bool color) {
        target_line--;
        const auto& manager = get_manager(path);
        if(!manager.ok()) {
            return "";
        }
        auto begin = target_line <= context_size ? 0 : target_line - context_size;
        auto original_begin = begin;
        auto end = std::min(target_line + context_size, manager.num_lines() - 1);
        std::vector<std::string> lines;
        for(auto line = begin; line <= end; line++) {
            lines.push_back(manager.get_line(line));
        }
        // trim blank lines
        while(begin < target_line && lines[begin - original_begin].empty()) {
            begin++;
        }
        while(end > target_line && lines[end - original_begin].empty()) {
            end--;
        }
        // make the snippet
        std::string snippet;
        constexpr std::size_t margin_width = 8;
        for(auto line = begin; line <= end; line++) {
            if(color && line == target_line) {
                snippet += YELLOW;
            }
            auto line_str = std::to_string(line);
            snippet += std::string(margin_width - line_str.size(), ' ') + line_str + ": ";
            if(color && line == target_line) {
                snippet += RESET;
            }
            snippet += lines[line - original_begin] + "\n";
        }
        return snippet;
    }
}
}
