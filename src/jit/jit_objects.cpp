#include "jit/jit_objects.hpp"

#include "cpptrace/forward.hpp"
#include "jit/jit_interface.hpp"
#include "utils/error.hpp"
#include "utils/microfmt.hpp"
#include "utils/optional.hpp"
#include "utils/span.hpp"
#include "binary/elf.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

namespace cpptrace {
namespace detail {
    class jit_object_manager {
        struct range_entry {
            frame_ptr low;
            frame_ptr high; // not inclusive
            elf* object;
            bool operator<(const range_entry& other) const {
                return low < other.low;
            }
        };
        std::vector<std::unique_ptr<elf>> objects;
        // TODO: Maybe use a set...
        std::vector<range_entry> range_list;

    public:
        void add_jit_object(cbspan object) {
            auto elf_res = elf::open_elf(object);
            if(elf_res.is_error()) {
                // if(!should_absorb_trace_exceptions()) { // TODO
                    elf_res.drop_error();
                // }
                return;
            }
            objects.push_back(make_unique<elf>(std::move(elf_res).unwrap_value()));
            auto* elf_file = objects.back().get();
            auto ranges_res = elf_file->get_pc_ranges();
            if(ranges_res.is_error()) {
                // if(!should_absorb_trace_exceptions()) { // TODO
                    ranges_res.drop_error();
                // }
                return;
            }
            auto& ranges = ranges_res.unwrap_value();
            for(auto range : ranges) {
                microfmt::print("> {:h} - {:h}\n", range.low, range.high);
                range_entry entry{range.low, range.high, elf_file};
                range_list.insert(std::upper_bound(range_list.begin(), range_list.end(), entry), entry);
            }
        }

        optional<elf_lookup_result> lookup(frame_ptr pc) const {
            microfmt::print("lookup {:h}\n", pc);
            for(const auto& range : range_list) {
                microfmt::print("  {:h} - {:h}\n", range.low, range.high);
            }
            auto it = first_less_than_or_equal(
                range_list.begin(),
                range_list.end(),
                pc,
                [](frame_ptr pc, const range_entry& entry) {
                    return pc < entry.low;
                }
            );
            if(it == range_list.end()) {
                microfmt::print("  not found\n");
                return nullopt;
            }
            ASSERT(pc >= it->low);
            if(pc < it->high) {
                microfmt::print("  found\n");
                return elf_lookup_result{*it->object, it->low};
            } else {
                microfmt::print("  not in range\n");
                return nullopt;
            }
        }

        void clear_jit_objects() {
            objects.clear();
        }
    };

    jit_object_manager& get_jit_object_manager() {
        static jit_object_manager manager;
        return manager;
    }

    void load_jit_objects() {
        {
            std::cout<<"scanning jit debug objects "<<__jit_debug_descriptor.version<<std::endl;
            jit_code_entry* entry = __jit_debug_descriptor.first_entry;
            int i = 0;
            while(entry) {
                std::cout<<"  "<<reinterpret_cast<const void*>(entry->symfile_addr)<<" "<<std::hex<<entry->symfile_size<<std::endl;
                std::ofstream f(microfmt::format("obj_{}.o", i++), std::ios::out | std::ios::binary);
                f.write(entry->symfile_addr, entry->symfile_size);
                entry = entry->next_entry;
            }
        }
        // std::cout<<"---"<<std::endl;
        // for(const auto& frame : frames) {
        //     std::cout<<"  0x"<<std::hex<<frame<<std::endl;
        //     jit_code_entry* entry = __jit_debug_descriptor.first_entry;
        //     while(entry) {
        //         if(frame >= reinterpret_cast<std::uintptr_t>(entry->symfile_addr) && frame < reinterpret_cast<std::uintptr_t>(entry->symfile_addr) + entry->symfile_size) {
        //             std::cout<<"   match"<<std::endl;
        //         }
        //         entry = entry->next_entry;
        //     }
        // }
        auto& manager = get_jit_object_manager();
        manager.clear_jit_objects();
        jit_code_entry* entry = __jit_debug_descriptor.first_entry;
        while(entry) {
            manager.add_jit_object(make_span(entry->symfile_addr, entry->symfile_addr + entry->symfile_size));
            entry = entry->next_entry;
        }
    }

    optional<elf_lookup_result> lookup_jit_object(frame_ptr pc) {
        return get_jit_object_manager().lookup(pc);
    }
}
}
