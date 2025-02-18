#include "symbols/dwarf/dwarf_options.hpp"

#include <cpptrace/utils.hpp>

#include <atomic>

namespace cpptrace {
    namespace detail {
        std::atomic<nullable<std::size_t>> dwarf_resolver_line_table_cache_size{nullable<std::size_t>::null()};

        optional<std::size_t> get_dwarf_resolver_line_table_cache_size() {
            auto max_entries = dwarf_resolver_line_table_cache_size.load();
            return max_entries.has_value() ? optional<std::size_t>(max_entries.value()) : nullopt;
        }
    }

    namespace experimental {
        void set_dwarf_resolver_line_table_cache_size(nullable<std::size_t> max_entries) {
            detail::dwarf_resolver_line_table_cache_size.store(max_entries);
        }
    }
}
