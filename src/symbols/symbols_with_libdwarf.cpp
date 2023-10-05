#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/common.hpp"
#include "../platform/dwarf.hpp"
#include "../platform/error.hpp"
#include "../platform/object.hpp"
#include "../platform/program_name.hpp"
#include "../platform/utils.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <libdwarf.h>
#include <dwarf.h>

// It's been tricky to piece together how to handle all this dwarf stuff. Some resources I've used are
// https://www.prevanders.net/libdwarf.pdf
// https://github.com/davea42/libdwarf-addr2line
// https://github.com/ruby/ruby/blob/master/addr2line.c

// TODO
// Inlined calls
// More utils to clean this up, some wrapper for unique_ptr
// Ensure memory is being cleaned up properly

#if false
 #define CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING CPPTRACE_FORCE_NO_INLINE
#else
 #define CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
#endif

namespace cpptrace {
namespace detail {
namespace libdwarf {
    // printbugging as we go
    constexpr bool dump_dwarf = false;
    constexpr bool trace_dwarf = false;

    struct line_context {
        Dwarf_Unsigned version;
        Dwarf_Line_Context line_context;
    };

    struct subprogram_entry {
        die_object die;
        Dwarf_Addr low;
        Dwarf_Addr high;
    };

    struct cu_entry {
        die_object die;
        Dwarf_Half dwversion;
        Dwarf_Addr low;
        Dwarf_Addr high;
    };

    struct dwarf_resolver {
        std::string obj_path;
        Dwarf_Debug dbg = nullptr;
        bool ok = false;
        // .debug_aranges cache
        Dwarf_Arange* aranges = nullptr;
        Dwarf_Signed arange_count = 0;
        // Map from CU -> Line context
        std::unordered_map<Dwarf_Off, line_context> line_contexts;
        // Map from CU -> Sorted subprograms vector
        std::unordered_map<Dwarf_Off, std::vector<subprogram_entry>> subprograms_cache;
        // Vector of ranges and their corresponding CU offsets
        std::vector<cu_entry> cu_cache;

        // Error handling helper
        // For some reason R (*f)(Args..., void*)-style deduction isn't possible, seems like a bug in all compilers
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56190
        template<
            typename... Args,
            typename... Args2,
            typename std::enable_if<
                std::is_same<
                    decltype(
                        (void)std::declval<int(Args...)>()(std::forward<Args2>(std::declval<Args2>())..., nullptr)
                    ),
                    void
                >::value,
                int
            >::type = 0
        >
        int wrap(int (*f)(Args...), Args2&&... args) const {
            Dwarf_Error error = nullptr;
            int ret = f(std::forward<Args2>(args)..., &error);
            if(ret == DW_DLV_ERROR) {
                handle_dwarf_error(dbg, error);
            }
            return ret;
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        dwarf_resolver(const std::string& object_path) {
            obj_path = object_path;
            // for universal / fat mach-o files
            unsigned universal_number = 0;
            #if IS_APPLE
            if(directory_exists(obj_path + ".dSYM")) {
                obj_path += ".dSYM/Contents/Resources/DWARF/" + basename(object_path);
            }
            if(macho_is_fat(obj_path)) {
                universal_number = get_fat_macho_index(obj_path);
            }
            #endif

            // Giving libdwarf a buffer for a true output path is needed for its automatic resolution of debuglink and
            // dSYM files. We don't utilize the dSYM logic here, we just care about debuglink.
            std::unique_ptr<char[]> buffer(new char[CPPTRACE_MAX_PATH]);
            auto ret = wrap(
                dwarf_init_path_a,
                obj_path.c_str(),
                buffer.get(),
                CPPTRACE_MAX_PATH,
                DW_GROUPNUMBER_ANY,
                universal_number,
                nullptr,
                nullptr,
                &dbg
            );
            if(ret == DW_DLV_OK) {
                ok = true;
            } else if(ret == DW_DLV_NO_ENTRY) {
                // fail, no debug info
                ok = false;
            } else {
                ok = false;
                PANIC("Unknown return code from dwarf_init_path");
            }

            if(ok) {
                // Check for .debug_aranges for fast lookup
                wrap(dwarf_get_aranges, dbg, &aranges, &arange_count);
            }
            if(ok && !aranges && get_cache_mode() != cache_mode::prioritize_memory) {
                walk_compilation_units([this] (const die_object& cu_die) {
                    Dwarf_Half offset_size = 0;
                    Dwarf_Half dwversion = 0;
                    dwarf_get_version_of_die(cu_die.get(), &dwversion, &offset_size);
                    auto ranges_vec = cu_die.get_rangelist_entries(dwversion);
                    for(auto range : ranges_vec) {
                        cu_cache.push_back({ cu_die.clone(), dwversion, range.first, range.second });
                    }
                    return true;
                });
                std::sort(cu_cache.begin(), cu_cache.end(), [] (const cu_entry& a, const cu_entry& b) {
                    return a.low < b.low;
                });
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        ~dwarf_resolver() {
            // TODO: Maybe redundant since dwarf_finish(dbg); will clean up the line stuff anyway but may as well just
            // for thoroughness
            for(auto& entry : line_contexts) {
                dwarf_srclines_dealloc_b(entry.second.line_context);
            }
            // subprograms_cache needs to be destroyed before dbg otherwise there will be another use after free
            subprograms_cache.clear();
            if(aranges) {
                dwarf_dealloc(dbg, aranges, DW_DLA_LIST);
            }
            cu_cache.clear();
            dwarf_finish(dbg);
        }

        dwarf_resolver(const dwarf_resolver&) = delete;
        dwarf_resolver& operator=(const dwarf_resolver&) = delete;

        dwarf_resolver(dwarf_resolver&& other) :
            obj_path(std::move(other.obj_path)),
            dbg(other.dbg),
            ok(other.ok),
            aranges(other.aranges),
            arange_count(other.arange_count),
            line_contexts(std::move(other.line_contexts)),
            subprograms_cache(std::move(other.subprograms_cache)),
            cu_cache(std::move(other.cu_cache))
        {
            other.dbg = nullptr;
            other.aranges = nullptr;
        }

        dwarf_resolver& operator=(dwarf_resolver&& other) {
            obj_path = std::move(other.obj_path);
            dbg = other.dbg;
            ok = other.ok;
            aranges = other.aranges;
            arange_count = other.arange_count;
            line_contexts = std::move(other.line_contexts);
            subprograms_cache = std::move(other.subprograms_cache);
            cu_cache = std::move(other.cu_cache);
            other.dbg = nullptr;
            other.aranges = nullptr;
            return *this;
        }

        // walk all CU's in a dbg, callback is called on each die and should return true to
        // continue traversal
        void walk_compilation_units(const std::function<bool(const die_object&)>& fn) {
            // libdwarf keeps track of where it is in the file, dwarf_next_cu_header_d is statefull
            Dwarf_Unsigned next_cu_header;
            Dwarf_Half header_cu_type;
            while(true) {
                int ret = wrap(
                    dwarf_next_cu_header_d,
                    dbg,
                    true,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    &next_cu_header,
                    &header_cu_type
                );
                if(ret == DW_DLV_NO_ENTRY) {
                    if(dump_dwarf) {
                        fprintf(stderr, "End walk_dbg\n");
                    }
                    return;
                }
                if(ret != DW_DLV_OK) {
                    PANIC("Unexpected return code from dwarf_next_cu_header_d");
                    return;
                }
                // 0 passed as the die to the first call of dwarf_siblingof_b immediately after dwarf_next_cu_header_d
                // to fetch the cu die
                die_object cu_die(dbg, nullptr);
                cu_die = cu_die.get_sibling();
                if(!cu_die) {
                    break;
                }
                if(!walk_die_list(cu_die, fn)) {
                    break;
                }
            }
            if(dump_dwarf) {
                fprintf(stderr, "End walk_compilation_units\n");
            }
        }

        void retrieve_symbol_for_subprogram(
            const die_object& die,
            Dwarf_Half dwversion,
            stacktrace_frame& frame
        ) {
            ASSERT(die.get_tag() == DW_TAG_subprogram);
            optional<std::string> name;
            if(auto linkage_name = die.get_string_attribute(DW_AT_linkage_name)) {
                name = std::move(linkage_name);
            } else if(auto linkage_name = die.get_string_attribute(DW_AT_MIPS_linkage_name)) {
                name = std::move(linkage_name);
            } else if(auto linkage_name = die.get_string_attribute(DW_AT_name)) {
                name = std::move(linkage_name);
            }
            if(name.has_value()) {
                frame.symbol = std::move(name).unwrap();
            } else {
                if(die.has_attr(DW_AT_specification)) {
                    die_object spec = die.resolve_reference_attribute(DW_AT_specification);
                    return retrieve_symbol_for_subprogram(spec, dwversion, frame);
                } else if(die.has_attr(DW_AT_abstract_origin)) {
                    die_object spec = die.resolve_reference_attribute(DW_AT_abstract_origin);
                    return retrieve_symbol_for_subprogram(spec, dwversion, frame);
                }
            }
        }

        // returns true if this call found the symbol
        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        bool retrieve_symbol_walk(
            const die_object& die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame
        ) {
            bool found = false;
            walk_die_list(
                die,
                [this, pc, dwversion, &frame, &found] (const die_object& die) {
                    if(dump_dwarf) {
                        fprintf(
                            stderr,
                            "-------------> %08llx %s %s\n",
                            to_ull(die.get_global_offset()),
                            die.get_tag_name(),
                            die.get_name().c_str()
                        );
                    }
                    if(!(die.get_tag() == DW_TAG_namespace || die.pc_in_die(dwversion, pc))) {
                        if(dump_dwarf) {
                            fprintf(stderr, "pc not in die\n");
                        }
                    } else {
                        if(trace_dwarf) {
                            fprintf(
                                stderr,
                                "%s %08llx %s\n",
                                die.get_tag() == DW_TAG_namespace ? "pc maybe in die (namespace)" : "pc in die",
                                to_ull(die.get_global_offset()),
                                die.get_tag_name()
                            );
                        }
                        if(die.get_tag() == DW_TAG_subprogram) {
                            retrieve_symbol_for_subprogram(die, dwversion, frame);
                            found = true;
                            return false;
                        }
                        auto child = die.get_child();
                        if(child) {
                            if(retrieve_symbol_walk(child, pc, dwversion, frame)) {
                                found = true;
                                return false;
                            }
                        } else {
                            if(dump_dwarf) {
                                fprintf(stderr, "(no child)\n");
                            }
                        }
                    }
                    return true;
                }
            );
            if(dump_dwarf) {
                fprintf(stderr, "End walk_die_list\n");
            }
            return found;
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void preprocess_subprograms(
            const die_object& die,
            Dwarf_Half dwversion,
            std::vector<subprogram_entry>& vec
        ) {
            walk_die_list(
                die,
                [this, dwversion, &vec] (const die_object& die) {
                    switch(die.get_tag()) {
                        case DW_TAG_subprogram:
                            {
                                auto ranges_vec = die.get_rangelist_entries(dwversion);
                                // TODO: Feels super inefficient and some day should maybe use an interval tree.
                                for(auto range : ranges_vec) {
                                    vec.push_back({ die.clone(), range.first, range.second });
                                }
                            }
                            break;
                        case DW_TAG_namespace:
                        case DW_TAG_structure_type:
                        case DW_TAG_class_type:
                        case DW_TAG_module:
                        case DW_TAG_imported_module:
                        case DW_TAG_compile_unit:
                            {
                                auto child = die.get_child();
                                if(child) {
                                    preprocess_subprograms(child, dwversion, vec);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    return true;
                }
            );
            if(dump_dwarf) {
                fprintf(stderr, "End walk_die_list\n");
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void retrieve_symbol(
            const die_object& cu_die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame
        ) {
            if(get_cache_mode() == cache_mode::prioritize_memory) {
                retrieve_symbol_walk(cu_die, pc, dwversion, frame);
            } else {
                auto off = cu_die.get_global_offset();
                auto it = subprograms_cache.find(off);
                if(it == subprograms_cache.end()) {
                    // TODO: Refactor. Do the sort in the preprocess function and return the vec directly.
                    std::vector<subprogram_entry> vec;
                    preprocess_subprograms(cu_die, dwversion, vec);
                    std::sort(vec.begin(), vec.end(), [] (const subprogram_entry& a, const subprogram_entry& b) {
                        return a.low < b.low;
                    });
                    subprograms_cache.emplace(off, std::move(vec));
                    it = subprograms_cache.find(off);
                }
                auto& vec = it->second;
                auto vec_it = std::lower_bound(
                    vec.begin(),
                    vec.end(),
                    pc,
                    [] (const subprogram_entry& entry, Dwarf_Addr pc) {
                        return entry.low < pc;
                    }
                );
                // vec_it is first >= pc
                // we want first <= pc
                if(vec_it != vec.begin()) {
                    vec_it--;
                }
                // If the vector has been empty this can happen
                if(vec_it != vec.end()) {
                    //vec_it->die.print();
                    if(vec_it->die.pc_in_die(dwversion, pc)) {
                        retrieve_symbol_for_subprogram(vec_it->die, dwversion, frame);
                    }
                } else {
                    ASSERT(vec.size() == 0, "Vec should be empty?");
                }
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void retrieve_line_info(
            const die_object& die,
            Dwarf_Addr pc,
            stacktrace_frame& frame
        ) {
            auto off = die.get_global_offset();
            auto it = line_contexts.find(off);
            Dwarf_Line_Context line_context;
            if(it != line_contexts.end()) {
                auto& entry = it->second;
                line_context = entry.line_context;
            } else {
                Dwarf_Unsigned version;
                Dwarf_Small table_count;
                int ret = wrap(
                    dwarf_srclines_b,
                    die.get(),
                    &version,
                    &table_count,
                    &line_context
                );
                static_assert(std::is_unsigned<decltype(table_count)>::value, "Expected unsigned Dwarf_Small");
                VERIFY(/*table_count >= 0 &&*/ table_count <= 2, "Unknown dwarf line table count");
                if(ret == DW_DLV_NO_ENTRY) {
                    // TODO: Failing silently for now
                    return;
                }
                VERIFY(ret == DW_DLV_OK);
                line_contexts.insert({off, {version, line_context}});
            }
            Dwarf_Line* line_buffer = nullptr;
            Dwarf_Signed line_count = 0;
            Dwarf_Line* linebuf_actuals = nullptr;
            Dwarf_Signed linecount_actuals = 0;
            VERIFY(
                wrap(
                    dwarf_srclines_two_level_from_linecontext,
                    line_context,
                    &line_buffer,
                    &line_count,
                    &linebuf_actuals,
                    &linecount_actuals
                ) == DW_DLV_OK
            );
            Dwarf_Addr last_lineaddr = 0;
            Dwarf_Line last_line = nullptr;
            for(int i = 0; i < line_count; i++) {
                Dwarf_Line line = line_buffer[i];
                Dwarf_Addr lineaddr = 0;
                VERIFY(wrap(dwarf_lineaddr, line, &lineaddr) == DW_DLV_OK);
                Dwarf_Line found_line = nullptr;
                if(pc == lineaddr) {
                    // Multiple PCs may correspond to a line, find the last one
                    found_line = line;
                    for(int j = i + 1; j < line_count; j++) {
                        Dwarf_Line line = line_buffer[j];
                        Dwarf_Addr lineaddr = 0;
                        VERIFY(wrap(dwarf_lineaddr, line, &lineaddr) == DW_DLV_OK);
                        if(pc == lineaddr) {
                            found_line = line;
                        }
                    }
                } else if(last_line && pc > last_lineaddr && pc < lineaddr) {
                    // Guess that the last line had it
                    found_line = last_line;
                }
                if(found_line) {
                    Dwarf_Unsigned line_number = 0;
                    VERIFY(wrap(dwarf_lineno, found_line, &line_number) == DW_DLV_OK);
                    frame.line = static_cast<uint_least32_t>(line_number);
                    char* filename = nullptr;
                    VERIFY(wrap(dwarf_linesrc, found_line, &filename) == DW_DLV_OK);
                    auto wrapper = raii_wrap(
                        filename,
                        [this] (char* str) { if(str) dwarf_dealloc(dbg, str, DW_DLA_STRING); }
                    );
                    frame.filename = filename;
                } else {
                    Dwarf_Bool is_line_end;
                    VERIFY(wrap(dwarf_lineendsequence, line, &is_line_end) == DW_DLV_OK);
                    if(is_line_end) {
                        last_lineaddr = 0;
                        last_line = nullptr;
                    } else {
                        last_lineaddr = lineaddr;
                        last_line = line;
                    }
                }
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void lookup_pc(
            Dwarf_Addr pc,
            stacktrace_frame& frame
        ) {
            if(dump_dwarf) {
                fprintf(stderr, "%s\n", obj_path.c_str());
                fprintf(stderr, "%llx\n", to_ull(pc));
            }
            // Check for .debug_aranges for fast lookup
            if(aranges) {
                // Try to find pc in aranges
                Dwarf_Arange arange;
                if(wrap(dwarf_get_arange, aranges, arange_count, pc, &arange) == DW_DLV_OK) {
                    // Address in table, load CU die
                    Dwarf_Off cu_die_offset;
                    VERIFY(wrap(dwarf_get_cu_die_offset, arange, &cu_die_offset) == DW_DLV_OK);
                    Dwarf_Die raw_die;
                    // Setting is_info = true for now, assuming in .debug_info rather than .debug_types
                    VERIFY(wrap(dwarf_offdie_b, dbg, cu_die_offset, true, &raw_die) == DW_DLV_OK);
                    die_object cu_die(dbg, raw_die);
                    Dwarf_Half offset_size = 0;
                    Dwarf_Half dwversion = 0;
                    VERIFY(dwarf_get_version_of_die(cu_die.get(), &dwversion, &offset_size) == DW_DLV_OK);
                    if(trace_dwarf) {
                        fprintf(stderr, "Found CU in aranges\n");
                        cu_die.print();
                    }
                    retrieve_line_info(cu_die, pc, frame); // no offset for line info
                    retrieve_symbol(cu_die, pc, dwversion, frame);
                }
            } else {
                if(get_cache_mode() == cache_mode::prioritize_memory) {
                    // walk for the cu and go from there
                    walk_compilation_units([this, pc, &frame] (const die_object& cu_die) {
                        Dwarf_Half offset_size = 0;
                        Dwarf_Half dwversion = 0;
                        dwarf_get_version_of_die(cu_die.get(), &dwversion, &offset_size);
                        //auto p = cu_die.get_pc_range(dwversion);
                        //cu_die.print();
                        //fprintf(stderr, "        %llx, %llx\n", p.first, p.second);
                        if(trace_dwarf) {
                            fprintf(stderr, "CU: %d %s\n", dwversion, cu_die.get_name().c_str());
                        }
                        if(cu_die.pc_in_die(dwversion, pc)) {
                            if(trace_dwarf) {
                                fprintf(
                                    stderr,
                                    "pc in die %08llx %s (now searching for %08llx)\n",
                                    to_ull(cu_die.get_global_offset()),
                                    cu_die.get_tag_name(),
                                    to_ull(pc)
                                );
                            }
                            retrieve_line_info(cu_die, pc, frame); // no offset for line info
                            retrieve_symbol(cu_die, pc, dwversion, frame);
                            return false;
                        }
                        return true;
                    });
                } else {
                    // look up the cu
                    auto vec_it = std::lower_bound(
                       cu_cache.begin(),
                       cu_cache.end(),
                       pc,
                       [] (const cu_entry& entry, Dwarf_Addr pc) {
                           return entry.low < pc;
                       }
                    );
                    // vec_it is first >= pc
                    // we want first <= pc
                    if(vec_it != cu_cache.begin()) {
                       vec_it--;
                    }
                    // If the vector has been empty this can happen
                    if(vec_it != cu_cache.end()) {
                       //vec_it->die.print();
                       if(vec_it->die.pc_in_die(vec_it->dwversion, pc)) {
                           retrieve_line_info(vec_it->die, pc, frame); // no offset for line info
                           retrieve_symbol(vec_it->die, pc, vec_it->dwversion, frame);
                       }
                    } else {
                       ASSERT(cu_cache.size() == 0, "Vec should be empty?");
                    }
                }
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        stacktrace_frame resolve_frame(const object_frame& frame_info) {
            stacktrace_frame frame = null_frame;
            frame.filename = frame_info.obj_path;
            frame.symbol = frame_info.symbol;
            frame.address = frame_info.raw_address;
            if(trace_dwarf) {
                fprintf(
                    stderr,
                    "Starting resolution for %s %08llx %s\n",
                    obj_path.c_str(),
                    to_ull(frame_info.obj_address),
                    frame_info.symbol.c_str()
                );
            }
            lookup_pc(
                frame_info.obj_address,
                frame
            );
            return frame;
        }
    };

    CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
    std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames) {
        std::vector<stacktrace_frame> trace(frames.size(), null_frame);
        static std::mutex mutex;
        // cache resolvers since objects are likely to be traced more than once
        static std::unordered_map<std::string, dwarf_resolver> resolver_map;
        // Locking around all libdwarf interaction per https://github.com/davea42/libdwarf-code/discussions/184
        const std::lock_guard<std::mutex> lock(mutex);
        for(const auto& obj_entry : collate_frames(frames, trace)) {
            try {
                const auto& obj_name = obj_entry.first;
                optional<dwarf_resolver> resolver_object = nullopt;
                dwarf_resolver* resolver = nullptr;
                auto it = resolver_map.find(obj_name);
                if(it != resolver_map.end()) {
                    resolver = &it->second;
                } else {
                    resolver_object = dwarf_resolver(obj_name);
                    resolver = &resolver_object.unwrap();
                }
                // If there's no debug information it'll mark itself as not ok
                if(resolver->ok) {
                    for(const auto& entry : obj_entry.second) {
                        try {
                            const auto& dlframe = entry.first.get();
                            auto& frame = entry.second.get();
                            frame = resolver->resolve_frame(dlframe);
                        } catch(...) { // NOSONAR
                            if(!should_absorb_trace_exceptions()) {
                                throw;
                            }
                        }
                    }
                }
                if(resolver_object.has_value() && get_cache_mode() == cache_mode::prioritize_speed) {
                    // .emplace needed, for some reason .insert tries to copy <= gcc 7.2
                    resolver_map.emplace(obj_name, std::move(resolver_object).unwrap());
                }
            } catch(...) { // NOSONAR
                if(!should_absorb_trace_exceptions()) {
                    throw;
                }
            }
        }
        return trace;
    }
}
}
}

#endif
