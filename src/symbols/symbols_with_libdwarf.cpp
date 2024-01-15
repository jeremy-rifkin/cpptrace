#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../utils/common.hpp"
#include "../utils/dwarf.hpp" // has dwarf #includes
#include "../utils/error.hpp"
#include "../binary/object.hpp"
#include "../utils/utils.hpp"
#include "../utils/program_name.hpp" // For CPPTRACE_MAX_PATH

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

    struct line_entry {
        Dwarf_Addr low;
        // Dwarf_Addr high;
        // int i;
        Dwarf_Line line;
        optional<std::string> path;
        optional<std::uint32_t> line_number;
        optional<std::uint32_t> column_number;
        line_entry(Dwarf_Addr low, Dwarf_Line line) : low(low), line(line) {}
    };

    struct line_table_info {
        Dwarf_Unsigned version;
        Dwarf_Line_Context line_context;
        // sorted by low_addr
        std::vector<line_entry> line_entries;
    };

    struct dwarf_resolver {
        std::string object_path;
        Dwarf_Debug dbg = nullptr;
        bool ok = false;
        // .debug_aranges cache
        Dwarf_Arange* aranges = nullptr;
        Dwarf_Signed arange_count = 0;
        // Map from CU -> Line context
        std::unordered_map<Dwarf_Off, line_table_info> line_contexts;
        // Map from CU -> Sorted subprograms vector
        std::unordered_map<Dwarf_Off, std::vector<subprogram_entry>> subprograms_cache;
        // Vector of ranges and their corresponding CU offsets
        std::vector<cu_entry> cu_cache;
        bool generated_cu_cache = false;
        // Map from CU -> {srcfiles, count}
        std::unordered_map<Dwarf_Off, std::pair<char**, Dwarf_Signed>> srcfiles_cache;

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
        dwarf_resolver(const std::string& _object_path) {
            object_path = _object_path;
            // for universal / fat mach-o files
            unsigned universal_number = 0;
            #if IS_APPLE
            if(directory_exists(object_path + ".dSYM")) {
                object_path += ".dSYM/Contents/Resources/DWARF/" + basename(object_path);
            }
            if(macho_is_fat(object_path)) {
                universal_number = mach_o(object_path).get_fat_index();
            }
            #endif

            // Giving libdwarf a buffer for a true output path is needed for its automatic resolution of debuglink and
            // dSYM files. We don't utilize the dSYM logic here, we just care about debuglink.
            std::unique_ptr<char[]> buffer(new char[CPPTRACE_MAX_PATH]);
            auto ret = wrap(
                dwarf_init_path_a,
                object_path.c_str(),
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
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        ~dwarf_resolver() {
            // TODO: Maybe redundant since dwarf_finish(dbg); will clean up the line stuff anyway but may as well just
            // for thoroughness
            for(auto& entry : line_contexts) {
                dwarf_srclines_dealloc_b(entry.second.line_context);
            }
            for(auto& entry : srcfiles_cache) {
                dwarf_dealloc(dbg, entry.second.first, DW_DLA_LIST);
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

        dwarf_resolver(dwarf_resolver&& other) noexcept :
            object_path(std::move(other.object_path)),
            dbg(other.dbg),
            ok(other.ok),
            aranges(other.aranges),
            arange_count(other.arange_count),
            line_contexts(std::move(other.line_contexts)),
            subprograms_cache(std::move(other.subprograms_cache)),
            cu_cache(std::move(other.cu_cache)),
            generated_cu_cache(other.generated_cu_cache),
            srcfiles_cache(std::move(other.srcfiles_cache))
        {
            other.dbg = nullptr;
            other.aranges = nullptr;
        }

        dwarf_resolver& operator=(dwarf_resolver&& other) noexcept {
            object_path = std::move(other.object_path);
            dbg = other.dbg;
            ok = other.ok;
            aranges = other.aranges;
            arange_count = other.arange_count;
            line_contexts = std::move(other.line_contexts);
            subprograms_cache = std::move(other.subprograms_cache);
            cu_cache = std::move(other.cu_cache);
            generated_cu_cache = other.generated_cu_cache;
            srcfiles_cache = std::move(other.srcfiles_cache);
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
                        std::fprintf(stderr, "End walk_dbg\n");
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
                std::fprintf(stderr, "End walk_compilation_units\n");
            }
        }

        void lazy_generate_cu_cache() {
            if(!generated_cu_cache) {
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
                generated_cu_cache = true;
            }
        }

        std::string subprogram_symbol(
            const die_object& die,
            Dwarf_Half dwversion
        ) {
            ASSERT(die.get_tag() == DW_TAG_subprogram || die.get_tag() == DW_TAG_inlined_subroutine);
            optional<std::string> name;
            if(auto linkage_name = die.get_string_attribute(DW_AT_linkage_name)) {
                name = std::move(linkage_name);
            } else if(auto linkage_name = die.get_string_attribute(DW_AT_MIPS_linkage_name)) {
                name = std::move(linkage_name);
            } else if(auto linkage_name = die.get_string_attribute(DW_AT_name)) {
                name = std::move(linkage_name);
            }
            if(name.has_value()) {
                return std::move(name).unwrap();
            } else {
                if(die.has_attr(DW_AT_specification)) {
                    die_object spec = die.resolve_reference_attribute(DW_AT_specification);
                    return subprogram_symbol(spec, dwversion);
                } else if(die.has_attr(DW_AT_abstract_origin)) {
                    die_object spec = die.resolve_reference_attribute(DW_AT_abstract_origin);
                    return subprogram_symbol(spec, dwversion);
                }
            }
            return "";
        }

        std::string resolve_filename(const die_object& cu_die, Dwarf_Unsigned file_i) {
            std::string filename;
            if(get_cache_mode() == cache_mode::prioritize_memory) {
                char** dw_srcfiles;
                Dwarf_Signed dw_filecount;
                VERIFY(wrap(dwarf_srcfiles, cu_die.get(), &dw_srcfiles, &dw_filecount) == DW_DLV_OK);
                if(Dwarf_Signed(file_i) < dw_filecount) {
                    filename = dw_srcfiles[file_i - 1];
                }
                dwarf_dealloc(cu_die.dbg, dw_srcfiles, DW_DLA_LIST);
            } else {
                auto off = cu_die.get_global_offset();
                auto it = srcfiles_cache.find(off);
                if(it == srcfiles_cache.end()) {
                    char** dw_srcfiles;
                    Dwarf_Signed dw_filecount;
                    VERIFY(wrap(dwarf_srcfiles, cu_die.get(), &dw_srcfiles, &dw_filecount) == DW_DLV_OK);
                    it = srcfiles_cache.insert(it, {off, {dw_srcfiles, dw_filecount}});
                }
                char** dw_srcfiles = it->second.first;
                Dwarf_Signed dw_filecount = it->second.second;
                if(Dwarf_Signed(file_i) < dw_filecount) {
                    filename = dw_srcfiles[file_i - 1];
                }
            }
            return filename;
        }

        void get_inlines_info(
            const die_object& cu_die,
            const die_object& die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            std::vector<stacktrace_frame>& inlines
        ) {
            ASSERT(die.get_tag() == DW_TAG_subprogram || die.get_tag() == DW_TAG_inlined_subroutine);
            auto child = die.get_child();
            if(child) {
                walk_die_list(
                    child,
                    [this, &cu_die, pc, dwversion, &inlines] (const die_object& die) {
                        if(die.get_tag() == DW_TAG_inlined_subroutine && die.pc_in_die(dwversion, pc)) {
                            const auto name = subprogram_symbol(die, dwversion);
                            const auto file_i = die.get_unsigned_attribute(DW_AT_call_file);
                            std::string file = file_i ? resolve_filename(cu_die, file_i.unwrap()) : "";
                            const auto line = die.get_unsigned_attribute(DW_AT_call_line);
                            const auto col = die.get_unsigned_attribute(DW_AT_call_column);
                            inlines.push_back(stacktrace_frame{
                                0,
                                {static_cast<std::uint32_t>(line.value_or(0))},
                                {static_cast<std::uint32_t>(col.value_or(0))},
                                file,
                                name,
                                true
                            });
                            get_inlines_info(cu_die, die, pc, dwversion, inlines);
                        }
                        return true;
                    }
                );
            }
        }

        std::string retrieve_symbol_for_subprogram(
            const die_object& cu_die,
            const die_object& die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            std::vector<stacktrace_frame>& inlines
        ) {
            ASSERT(die.get_tag() == DW_TAG_subprogram);
            const auto name = subprogram_symbol(die, dwversion);
            get_inlines_info(cu_die, die, pc, dwversion, inlines);
            return name;
        }

        // returns true if this call found the symbol
        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        bool retrieve_symbol_walk(
            const die_object& cu_die,
            const die_object& die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame,
            std::vector<stacktrace_frame>& inlines
        ) {
            bool found = false;
            walk_die_list(
                die,
                [this, &cu_die, pc, dwversion, &frame, &inlines, &found] (const die_object& die) {
                    if(dump_dwarf) {
                        std::fprintf(
                            stderr,
                            "-------------> %08llx %s %s\n",
                            to_ull(die.get_global_offset()),
                            die.get_tag_name(),
                            die.get_name().c_str()
                        );
                    }
                    if(!(die.get_tag() == DW_TAG_namespace || die.pc_in_die(dwversion, pc))) {
                        if(dump_dwarf) {
                            std::fprintf(stderr, "pc not in die\n");
                        }
                    } else {
                        if(trace_dwarf) {
                            std::fprintf(
                                stderr,
                                "%s %08llx %s\n",
                                die.get_tag() == DW_TAG_namespace ? "pc maybe in die (namespace)" : "pc in die",
                                to_ull(die.get_global_offset()),
                                die.get_tag_name()
                            );
                        }
                        if(die.get_tag() == DW_TAG_subprogram) {
                            frame.symbol = retrieve_symbol_for_subprogram(cu_die, die, pc, dwversion, inlines);
                            found = true;
                            return false;
                        }
                        auto child = die.get_child();
                        if(child) {
                            if(retrieve_symbol_walk(cu_die, child, pc, dwversion, frame, inlines)) {
                                found = true;
                                return false;
                            }
                        } else {
                            if(dump_dwarf) {
                                std::fprintf(stderr, "(no child)\n");
                            }
                        }
                    }
                    return true;
                }
            );
            if(dump_dwarf) {
                std::fprintf(stderr, "End walk_die_list\n");
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
                                // Walk children to get things like lambdas
                                // TODO: Somehow find a way to get better names here? For gcc it's just "operator()"
                                // On clang it's better
                                auto child = die.get_child();
                                if(child) {
                                    preprocess_subprograms(child, dwversion, vec);
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
                std::fprintf(stderr, "End walk_die_list\n");
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void retrieve_symbol(
            const die_object& cu_die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame,
            std::vector<stacktrace_frame>& inlines
        ) {
            if(get_cache_mode() == cache_mode::prioritize_memory) {
                retrieve_symbol_walk(cu_die, cu_die, pc, dwversion, frame, inlines);
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
                        frame.symbol = retrieve_symbol_for_subprogram(cu_die, vec_it->die, pc, dwversion, inlines);
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
            if(it != line_contexts.end()) {
                // auto& entry = it->second;
                // line_context = entry.line_context;
            } else {
                Dwarf_Unsigned version;
                Dwarf_Small table_count;
                Dwarf_Line_Context line_context;
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

                std::vector<line_entry> line_entries;

                if(get_cache_mode() == cache_mode::prioritize_speed) {
                    // build lookup table
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

                    // TODO: Make any attempt to note PC ranges? Handle line end sequence?
                    for(int i = 0; i < line_count; i++) {
                        Dwarf_Line line = line_buffer[i];
                        Dwarf_Addr low_addr = 0;
                        VERIFY(wrap(dwarf_lineaddr, line, &low_addr) == DW_DLV_OK);
                        // scan ahead for the last line entry matching this pc
                        int j;
                        for(j = i + 1; j < line_count; j++) {
                            Dwarf_Addr addr = 0;
                            VERIFY(wrap(dwarf_lineaddr, line_buffer[j], &addr) == DW_DLV_OK);
                            if(addr != low_addr) {
                                break;
                            }
                        }
                        line = line_buffer[j - 1];
                        // {
                        //     Dwarf_Unsigned line_number = 0;
                        //     VERIFY(wrap(dwarf_lineno, line, &line_number) == DW_DLV_OK);
                        //     frame.line = static_cast<std::uint32_t>(line_number);
                        //     char* filename = nullptr;
                        //     VERIFY(wrap(dwarf_linesrc, line, &filename) == DW_DLV_OK);
                        //     auto wrapper = raii_wrap(
                        //         filename,
                        //         [this] (char* str) { if(str) dwarf_dealloc(dbg, str, DW_DLA_STRING); }
                        //     );
                        //     frame.filename = filename;
                        //     printf("%s : %d\n", filename, line_number);
                        //     Dwarf_Bool is_line_end;
                        //     VERIFY(wrap(dwarf_lineendsequence, line, &is_line_end) == DW_DLV_OK);
                        //     if(is_line_end) {
                        //         puts("Line end");
                        //     }
                        // }
                        line_entries.push_back({
                            low_addr,
                            line
                        });
                        i = j - 1;
                    }
                    // sort lines
                    std::sort(line_entries.begin(), line_entries.end(), [] (const line_entry& a, const line_entry& b) {
                        return a.low < b.low;
                    });
                }

                it = line_contexts.insert({off, {version, line_context, std::move(line_entries)}}).first;
            }

            auto& table_info = it->second;

            if(get_cache_mode() == cache_mode::prioritize_speed) {
                // Lookup in the table
                auto& line_entries = table_info.line_entries;
                auto table_it = std::lower_bound(
                    line_entries.begin(),
                    line_entries.end(),
                    pc,
                    [] (const line_entry& entry, Dwarf_Addr pc) {
                        return entry.low < pc;
                    }
                );
                // vec_it is first >= pc
                // we want first <= pc
                if(table_it != line_entries.begin()) {
                    table_it--;
                }
                // If the vector has been empty this can happen
                if(table_it != line_entries.end()) {
                    Dwarf_Line line = table_it->line;
                    // line number
                    if(!table_it->line_number) {
                        Dwarf_Unsigned line_number = 0;
                        VERIFY(wrap(dwarf_lineno, line, &line_number) == DW_DLV_OK);
                        table_it->line_number = static_cast<std::uint32_t>(line_number);
                    }
                    frame.line = table_it->line_number.unwrap();
                    // column number
                    if(!table_it->column_number) {
                        Dwarf_Unsigned column_number = 0;
                        VERIFY(wrap(dwarf_lineoff_b, line, &column_number) == DW_DLV_OK);
                        table_it->column_number = static_cast<std::uint32_t>(column_number);
                    }
                    frame.column = table_it->column_number.unwrap();
                    // filename
                    if(!table_it->path) {
                        char* filename = nullptr;
                        VERIFY(wrap(dwarf_linesrc, line, &filename) == DW_DLV_OK);
                        auto wrapper = raii_wrap(
                            filename,
                            [this] (char* str) { if(str) dwarf_dealloc(dbg, str, DW_DLA_STRING); }
                        );
                        table_it->path = filename;
                    }
                    frame.filename = table_it->path.unwrap();
                }
            } else {
                Dwarf_Line_Context line_context = table_info.line_context;
                // walk for it
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
                        frame.line = static_cast<std::uint32_t>(line_number);
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
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void lookup_pc(
            Dwarf_Addr pc,
            stacktrace_frame& frame,
            std::vector<stacktrace_frame>& inlines
        ) {
            if(dump_dwarf) {
                std::fprintf(stderr, "%s\n", object_path.c_str());
                std::fprintf(stderr, "%llx\n", to_ull(pc));
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
                        std::fprintf(stderr, "Found CU in aranges\n");
                        cu_die.print();
                    }
                    retrieve_line_info(cu_die, pc, frame); // no offset for line info
                    retrieve_symbol(cu_die, pc, dwversion, frame, inlines);
                    return;
                }
            }
            // otherwise, or if not in aranges
            // one reason to fallback here is if the compilation has dwarf generated from different compilers and only
            // some of them generate aranges (e.g. static linking with cpptrace after specifying clang++ as the c++
            // compiler while the C compiler defaults to an older gcc)
            if(get_cache_mode() == cache_mode::prioritize_memory) {
                // walk for the cu and go from there
                walk_compilation_units([this, pc, &frame, &inlines] (const die_object& cu_die) {
                    Dwarf_Half offset_size = 0;
                    Dwarf_Half dwversion = 0;
                    dwarf_get_version_of_die(cu_die.get(), &dwversion, &offset_size);
                    //auto p = cu_die.get_pc_range(dwversion);
                    //cu_die.print();
                    //fprintf(stderr, "        %llx, %llx\n", p.first, p.second);
                    if(trace_dwarf) {
                        std::fprintf(stderr, "CU: %d %s\n", dwversion, cu_die.get_name().c_str());
                    }
                    if(cu_die.pc_in_die(dwversion, pc)) {
                        if(trace_dwarf) {
                            std::fprintf(
                                stderr,
                                "pc in die %08llx %s (now searching for %08llx)\n",
                                to_ull(cu_die.get_global_offset()),
                                cu_die.get_tag_name(),
                                to_ull(pc)
                            );
                        }
                        retrieve_line_info(cu_die, pc, frame); // no offset for line info
                        retrieve_symbol(cu_die, pc, dwversion, frame, inlines);
                        return false;
                    }
                    return true;
                });
            } else {
                lazy_generate_cu_cache();
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
                        retrieve_symbol(vec_it->die, pc, vec_it->dwversion, frame, inlines);
                    }
                } else {
                    ASSERT(cu_cache.size() == 0, "Vec should be empty?");
                }
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        frame_with_inlines resolve_frame(const object_frame& frame_info) {
            stacktrace_frame frame = null_frame;
            frame.filename = frame_info.object_path;
            frame.address = frame_info.raw_address;
            if(trace_dwarf) {
                std::fprintf(
                    stderr,
                    "Starting resolution for %s %08llx\n",
                    object_path.c_str(),
                    to_ull(frame_info.object_address)
                );
            }
            std::vector<stacktrace_frame> inlines;
            lookup_pc(
                frame_info.object_address,
                frame,
                inlines
            );
            return {std::move(frame), std::move(inlines)};
        }
    };

    CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
    std::vector<stacktrace_frame> resolve_frames(const std::vector<object_frame>& frames) {
        std::vector<frame_with_inlines> trace(frames.size(), {null_frame, {}});
        static std::mutex mutex;
        // cache resolvers since objects are likely to be traced more than once
        static std::unordered_map<std::string, dwarf_resolver> resolver_map;
        // Locking around all libdwarf interaction per https://github.com/davea42/libdwarf-code/discussions/184
        const std::lock_guard<std::mutex> lock(mutex);
        for(const auto& object_entry : collate_frames(frames, trace)) {
            try {
                const auto& object_name = object_entry.first;
                optional<dwarf_resolver> resolver_object = nullopt;
                dwarf_resolver* resolver = nullptr;
                auto it = resolver_map.find(object_name);
                if(it != resolver_map.end()) {
                    resolver = &it->second;
                } else {
                    resolver_object = dwarf_resolver(object_name);
                    resolver = &resolver_object.unwrap();
                }
                // If there's no debug information it'll mark itself as not ok
                if(resolver->ok) {
                    for(const auto& entry : object_entry.second) {
                        try {
                            const auto& dlframe = entry.first.get();
                            auto& frame = entry.second.get();
                            frame = resolver->resolve_frame(dlframe);
                        } catch(...) {
                            if(!should_absorb_trace_exceptions()) {
                                throw;
                            }
                        }
                    }
                } else {
                    // at least copy the addresses
                    for(const auto& entry : object_entry.second) {
                        const auto& dlframe = entry.first.get();
                        auto& frame = entry.second.get();
                        frame.frame.address = dlframe.raw_address;
                    }
                }
                if(resolver_object.has_value() && get_cache_mode() == cache_mode::prioritize_speed) {
                    // .emplace needed, for some reason .insert tries to copy <= gcc 7.2
                    resolver_map.emplace(object_name, std::move(resolver_object).unwrap());
                }
            } catch(...) { // NOSONAR
                if(!should_absorb_trace_exceptions()) {
                    throw;
                }
            }
        }
        // flatten trace with inlines
        std::vector<stacktrace_frame> final_trace;
        for(const auto& entry : trace) {
            // most recent call first
            if(!entry.inlines.empty()) {
                // insert in reverse order
                final_trace.insert(final_trace.end(), entry.inlines.rbegin(), entry.inlines.rend());
            }
            final_trace.push_back(entry.frame);
            if(!entry.inlines.empty()) {
                // rotate line info due to quirk of how dwarf stores this stuff
                // inclusive range
                auto begin = final_trace.end() - (1 + entry.inlines.size());
                auto end = final_trace.end() - 1;
                auto carry_line = end->line;
                auto carry_column = end->column;
                std::string carry_filename = std::move(end->filename);
                for(auto it = end; it != begin; it--) {
                    it->line = (it - 1)->line;
                    it->column = (it - 1)->column;
                    it->filename = std::move((it - 1)->filename);
                }
                begin->line = carry_line;
                begin->column = carry_column;
                begin->filename = std::move(carry_filename);
            }
        }
        return final_trace;
    }
}
}
}

#endif
