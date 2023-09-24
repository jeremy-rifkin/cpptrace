#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/common.hpp"
#include "../platform/program_name.hpp"
#include "../platform/object.hpp"
#include "../platform/error.hpp"
#include "../platform/utils.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
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

    static_assert(std::is_pointer<Dwarf_Die>::value, "Dwarf_Die not a pointer");
    static_assert(std::is_pointer<Dwarf_Debug>::value, "Dwarf_Debug not a pointer");

    void handle_error(Dwarf_Debug dbg, Dwarf_Error error) {
        int ev = dwarf_errno(error);
        char* msg = dwarf_errmsg(error);
        dwarf_dealloc_error(dbg, error);
        throw std::runtime_error(stringf("Cpptrace dwarf error %d %s\n", ev, msg));
    }

    struct die_object {
        Dwarf_Debug dbg = nullptr;
        Dwarf_Die die = nullptr;

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
            Dwarf_Error error = 0;
            int ret = f(std::forward<Args2>(args)..., &error);
            if(ret == DW_DLV_ERROR) {
                handle_error(dbg, error);
            }
            return ret;
        }

        die_object(Dwarf_Debug dbg, Dwarf_Die die) : dbg(dbg), die(die) {
            ASSERT(dbg != nullptr);
        }

        ~die_object() {
            if(die) {
                dwarf_dealloc(dbg, die, DW_DLA_DIE);
            }
        }

        die_object(const die_object&) = delete;

        die_object& operator=(const die_object&) = delete;

        die_object(die_object&& other) : dbg(other.dbg), die(other.die) {
            // done for finding mistakes, attempts to use the die_object after this should segfault
            // a valid use otherwise would be moved_from.get_sibling() which would get the next CU
            other.dbg = nullptr;
            other.die = nullptr;
        }

        die_object& operator=(die_object&& other) {
            dbg = other.dbg;
            die = other.die;
            other.die = nullptr;
            return *this;
        }

        die_object clone() const {
            Dwarf_Off global_offset = get_global_offset();
            Dwarf_Bool is_info = dwarf_get_die_infotypes_flag(die);
            Dwarf_Die die_copy = 0;
            VERIFY(wrap(dwarf_offdie_b, dbg, global_offset, is_info, &die_copy) == DW_DLV_OK);
            return {dbg, die_copy};
        }

        die_object get_child() const {
            Dwarf_Die child = nullptr;
            int ret = wrap(dwarf_child, die, &child);
            if(ret == DW_DLV_OK) {
                return die_object(dbg, child);
            } else if(ret == DW_DLV_NO_ENTRY) {
                return die_object(dbg, 0);
            } else {
                PANIC();
            }
        }

        die_object get_sibling() const {
            Dwarf_Die sibling = 0;
            int ret = wrap(dwarf_siblingof_b, dbg, die, true, &sibling);
            if(ret == DW_DLV_OK) {
                return die_object(dbg, sibling);
            } else if(ret == DW_DLV_NO_ENTRY) {
                return die_object(dbg, 0);
            } else {
                PANIC();
            }
        }

        operator bool() const {
            return die != nullptr;
        }

        Dwarf_Die get() const {
            return die;
        }

        std::string get_name() const {
            char empty[] = "";
            char* name = empty;
            int ret = wrap(dwarf_diename, die, &name);
            auto wrapper = raii_wrap(name, [this] (char* str) { dwarf_dealloc(dbg, str, DW_DLA_STRING); });
            std::string str;
            if(ret != DW_DLV_NO_ENTRY) {
                str = name;
            }
            return name;
        }

        optional<std::string> get_string_attribute(Dwarf_Half attr_num) const {
            Dwarf_Attribute attr;
            if(wrap(dwarf_attr, die, attr_num, &attr) == DW_DLV_OK) {
                auto attwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
                char* raw_str;
                VERIFY(wrap(dwarf_formstring, attr, &raw_str) == DW_DLV_OK);
                auto strwrapper = raii_wrap(raw_str, [this] (char* str) { dwarf_dealloc(dbg, str, DW_DLA_STRING); });
                std::string str = raw_str;
                return str;
            } else {
                return nullopt;
            }
        }

        bool has_attr(Dwarf_Half attr_num) const {
            Dwarf_Bool present = false;
            VERIFY(wrap(dwarf_hasattr, die, attr_num, &present) == DW_DLV_OK);
            return present;
        }

        Dwarf_Half get_tag() const {
            Dwarf_Half tag = 0;
            VERIFY(wrap(dwarf_tag, die, &tag) == DW_DLV_OK);
            return tag;
        }

        const char* get_tag_name() const {
            const char* tag_name;
            if(dwarf_get_TAG_name(get_tag(), &tag_name) == DW_DLV_OK) {
                return tag_name;
            } else {
                return "<unknown tag name>";
            }
        }

        Dwarf_Off get_global_offset() const {
            Dwarf_Off off;
            VERIFY(wrap(dwarf_dieoffset, die, &off) == DW_DLV_OK);
            return off;
        }

        die_object resolve_reference_attribute(Dwarf_Half attr_num) const {
            Dwarf_Attribute attr;
            VERIFY(dwarf_attr(die, attr_num, &attr, nullptr) == DW_DLV_OK);
            auto wrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            Dwarf_Half form = 0;
            VERIFY(wrap(dwarf_whatform, attr, &form) == DW_DLV_OK);
            switch(form) {
                case DW_FORM_ref1:
                case DW_FORM_ref2:
                case DW_FORM_ref4:
                case DW_FORM_ref8:
                case DW_FORM_ref_udata:
                    {
                        Dwarf_Off off = 0;
                        Dwarf_Bool is_info = dwarf_get_die_infotypes_flag(die);
                        VERIFY(wrap(dwarf_formref, attr, &off, &is_info) == DW_DLV_OK);
                        Dwarf_Off global_offset = 0;
                        VERIFY(wrap(dwarf_convert_to_global_offset, attr, off, &global_offset) == DW_DLV_OK);
                        Dwarf_Die target = 0;
                        VERIFY(wrap(dwarf_offdie_b, dbg, global_offset, is_info, &target) == DW_DLV_OK);
                        return die_object(dbg, target);
                    }
                case DW_FORM_ref_addr:
                    {
                        Dwarf_Off off;
                        VERIFY(wrap(dwarf_global_formref, attr, &off) == DW_DLV_OK);
                        int is_info = dwarf_get_die_infotypes_flag(die);
                        Dwarf_Die target = 0;
                        VERIFY(wrap(dwarf_offdie_b, dbg, off, is_info, &target) == DW_DLV_OK);
                        return die_object(dbg, target);
                    }
                case DW_FORM_ref_sig8:
                    {
                        Dwarf_Sig8 signature;
                        VERIFY(wrap(dwarf_formsig8, attr, &signature) == DW_DLV_OK);
                        Dwarf_Die target = 0;
                        Dwarf_Bool targ_is_info = false;
                        VERIFY(wrap(dwarf_find_die_given_sig8, dbg, &signature, &target, &targ_is_info) == DW_DLV_OK);
                        return die_object(dbg, target);
                    }
                default:
                    PANIC(stringf("unknown form for attribute %d %d\n", attr_num, form));
            }
        }

        Dwarf_Unsigned get_ranges_offset(Dwarf_Attribute attr) const {
            Dwarf_Unsigned off = 0;
            Dwarf_Half form = 0;
            VERIFY(wrap(dwarf_whatform, attr, &form) == DW_DLV_OK);
            if (form == DW_FORM_rnglistx) {
                VERIFY(wrap(dwarf_formudata, attr, &off) == DW_DLV_OK);
            } else {
                VERIFY(wrap(dwarf_global_formref, attr, &off) == DW_DLV_OK);
            }
            return off;
        }

        template<typename F>
        void dwarf5_ranges(F callback) const {
            Dwarf_Attribute attr = 0;
            VERIFY(wrap(dwarf_attr, die, DW_AT_ranges, &attr) == DW_DLV_OK);
            auto attrwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            Dwarf_Unsigned offset = get_ranges_offset(attr);
            Dwarf_Half form = 0;
            VERIFY(wrap(dwarf_whatform, attr, &form) == DW_DLV_OK);
            // get .debug_rnglists info
            Dwarf_Rnglists_Head head = 0;
            Dwarf_Unsigned rnglists_entries = 0;
            Dwarf_Unsigned dw_global_offset_of_rle_set = 0;
            int res = wrap(
                dwarf_rnglists_get_rle_head,
                attr,
                form,
                offset,
                &head,
                &rnglists_entries,
                &dw_global_offset_of_rle_set
            );
            auto headwrapper = raii_wrap(head, [] (Dwarf_Rnglists_Head head) { dwarf_dealloc_rnglists_head(head); });
            if(res == DW_DLV_NO_ENTRY) {
                return;
            }
            VERIFY(res == DW_DLV_OK);
            for(size_t i = 0 ; i < rnglists_entries; i++) {
                unsigned entrylen = 0;
                unsigned rle_value_out = 0;
                Dwarf_Unsigned raw1 = 0;
                Dwarf_Unsigned raw2 = 0;
                Dwarf_Bool unavailable = 0;
                Dwarf_Unsigned cooked1 = 0;
                Dwarf_Unsigned cooked2 = 0;
                res = wrap(
                    dwarf_get_rnglists_entry_fields_a,
                    head,
                    i,
                    &entrylen,
                    &rle_value_out,
                    &raw1,
                    &raw2,
                    &unavailable,
                    &cooked1,
                    &cooked2
                );
                if(res == DW_DLV_NO_ENTRY) {
                    continue;
                }
                VERIFY(res == DW_DLV_OK);
                if(unavailable) {
                    continue;
                }
                switch(rle_value_out) {
                    // Following the same scheme from libdwarf-addr2line
                    case DW_RLE_end_of_list:
                    case DW_RLE_base_address:
                    case DW_RLE_base_addressx:
                        // Already handled
                        break;
                    case DW_RLE_offset_pair:
                    case DW_RLE_startx_endx:
                    case DW_RLE_start_end:
                    case DW_RLE_startx_length:
                    case DW_RLE_start_length:
                        if(!callback(cooked1, cooked2)) {
                            return;
                        }
                        break;
                    default:
                        PANIC("Something is wrong");
                        break;
                }
            }
        }

        template<typename F>
        void dwarf4_ranges(Dwarf_Addr lowpc, F callback) const {
            Dwarf_Attribute attr = 0;
            if(wrap(dwarf_attr, die, DW_AT_ranges, &attr) != DW_DLV_OK) {
                return;
            }
            auto attrwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            Dwarf_Unsigned offset;
            if(wrap(dwarf_global_formref, attr, &offset) != DW_DLV_OK) {
                return;
            }
            Dwarf_Addr baseaddr = 0;
            if(lowpc != std::numeric_limits<Dwarf_Addr>::max()) {
                baseaddr = lowpc;
            }
            Dwarf_Ranges* ranges = 0;
            Dwarf_Signed count = 0;
            VERIFY(
                wrap(
                    dwarf_get_ranges_b,
                    dbg,
                    offset,
                    die,
                    nullptr,
                    &ranges,
                    &count,
                    nullptr
                ) == DW_DLV_OK
            );
            auto rangeswrapper = raii_wrap(
                ranges,
                [this, count] (Dwarf_Ranges* ranges) { dwarf_dealloc_ranges(dbg, ranges, count); }
            );
            for(int i = 0; i < count; i++) {
                if(ranges[i].dwr_type == DW_RANGES_ENTRY) {
                    if(!callback(baseaddr + ranges[i].dwr_addr1, baseaddr + ranges[i].dwr_addr2)) {
                        return;
                    }
                } else if(ranges[i].dwr_type == DW_RANGES_ADDRESS_SELECTION) {
                    baseaddr = ranges[i].dwr_addr2;
                } else {
                    ASSERT(ranges[i].dwr_type == DW_RANGES_END);
                    baseaddr = lowpc;
                }
            }
        }

        template<typename F>
        void dwarf_ranges(int version, optional<Dwarf_Addr> pc, F callback) const {
            Dwarf_Addr lowpc = std::numeric_limits<Dwarf_Addr>::max();
            if(wrap(dwarf_lowpc, die, &lowpc) == DW_DLV_OK) {
                if(pc.has_value() && pc.unwrap() == lowpc) {
                    callback(lowpc, lowpc + 1);
                    return;
                }
                Dwarf_Addr highpc = 0;
                enum Dwarf_Form_Class return_class;
                if(wrap(dwarf_highpc_b, die, &highpc, nullptr, &return_class) == DW_DLV_OK) {
                    if(return_class == DW_FORM_CLASS_CONSTANT) {
                        highpc += lowpc;
                    }
                    if(!callback(lowpc, highpc)) {
                        return;
                    }
                }
            }
            if(version >= 5) {
                dwarf5_ranges(callback);
            } else {
                dwarf4_ranges(lowpc, callback);
            }
        }

        std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>> get_rangelist_entries(int version) const {
            std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>> vec;
            dwarf_ranges(version, nullopt, [&vec] (Dwarf_Addr low, Dwarf_Addr high) {
                vec.push_back({low, high});
                return true;
            });
            return vec;
        }

        Dwarf_Bool pc_in_die(int version, Dwarf_Addr pc) const {
            bool found = false;
            dwarf_ranges(version, pc, [&found, pc] (Dwarf_Addr low, Dwarf_Addr high) {
                if(pc >= low && pc < high) {
                    found = true;
                    return false;
                }
                return true;
            });
            return found;
        }

        void print() const {
            fprintf(
                stderr,
                "%08llx %s %s\n",
                to_ull(get_global_offset()),
                get_tag_name(),
                get_name().c_str()
            );
        }
    };

    bool is_mangled_name(const std::string& name) {
        return name.find("_Z") || name.find("?h@@");
    }

    struct line_context {
        Dwarf_Unsigned version;
        Dwarf_Line_Context line_context;
    };

    struct subprogram_entry {
        die_object die;
        Dwarf_Addr low;
        Dwarf_Addr high;
    };

    struct dwarf_resolver {
        std::string obj_path;
        Dwarf_Debug dbg;
        bool ok = false;
        std::unordered_map<Dwarf_Off, line_context> line_contexts;
        std::unordered_map<Dwarf_Off, std::vector<subprogram_entry>> subprograms_cache;

        // Exists only for cleaning up an awful mach-o hack
        std::string tmp_object_path;

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
            Dwarf_Error error = 0;
            int ret = f(std::forward<Args2>(args)..., &error);
            if(ret == DW_DLV_ERROR) {
                handle_error(dbg, error);
            }
            return ret;
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        dwarf_resolver(const std::string& object_path) {
            obj_path = object_path;
            #if IS_APPLE
            if(directory_exists(obj_path + ".dSYM")) {
                obj_path += ".dSYM/Contents/Resources/DWARF/" + basename(object_path);
            }
            if(macho_is_fat(obj_path)) {
                // If the object is fat, we'll copy out the mach-o object we care about
                // Awful hack until libdwarf supports fat mach
                auto sub_object = get_fat_macho_information(obj_path);
                char tmp_template[] = "/tmp/tmp.cpptrace.XXXXXX";
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                VERIFY(mktemp(tmp_template) != nullptr);
                #pragma GCC diagnostic pop
                std::string tmp_path = tmp_template;
                auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
                auto tmp = raii_wrap(fopen(tmp_path.c_str(), "wb"), file_deleter);
                VERIFY(file != nullptr);
                VERIFY(tmp != nullptr);
                std::unique_ptr<char[]> buffer(new char[sub_object.size]);
                VERIFY(fseek(file, sub_object.offset, SEEK_SET) == 0);
                VERIFY(fread(buffer.get(), 1, sub_object.size, file) == sub_object.size);
                VERIFY(fwrite(buffer.get(), 1, sub_object.size, tmp) == sub_object.size);
                obj_path = tmp_path;
                tmp_object_path = std::move(tmp_path);
            }
            #endif

            // Giving libdwarf a buffer for a true output path is needed for its automatic resolution of debuglink and
            // dSYM files. We don't utilize the dSYM logic here, we just care about debuglink.
            std::unique_ptr<char[]> buffer(new char[CPPTRACE_MAX_PATH]);
            auto ret = wrap(
                dwarf_init_path,
                obj_path.c_str(),
                buffer.get(),
                CPPTRACE_MAX_PATH,
                DW_GROUPNUMBER_ANY,
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
            dwarf_finish(dbg);
            // cleanup awful mach-o hack
            if(!tmp_object_path.empty()) {
                unlink(tmp_object_path.c_str());
            }
        }

        // walk die list, callback is called on each die and should return true to
        // continue traversal
        void walk_die_list(
            const die_object& die,
            std::function<bool(const die_object&)> fn
        ) {
            // TODO: Refactor so there is only one fn call
            if(fn(die)) {
                die_object current = die.get_sibling();
                while(current) {
                    if(fn(current)) {
                        current = current.get_sibling();
                    } else {
                        break;
                    }
                }
            }
            if(dump_dwarf) {
                fprintf(stderr, "End walk_die_list\n");
            }
        }

        // walk die list, recursing into children, callback is called on each die
        // and should return true to continue traversal
        // returns true if traversal should continue
        bool walk_die_list_recursive(
            const die_object& die,
            std::function<bool(const die_object&)> fn
        ) {
            bool continue_traversal = true;
            walk_die_list(
                die,
                [this, &fn, &continue_traversal](const die_object& die) {
                    auto child = die.get_child();
                    if(child) {
                        if(!walk_die_list_recursive(child, fn)) {
                            continue_traversal = false;
                            return false;
                        }
                    }
                    return fn(die);
                }
            );
            return continue_traversal;
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
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void retrieve_symbol(
            const die_object& cu_die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame
        ) {
            auto off = cu_die.get_global_offset();
            auto it = subprograms_cache.find(off);
            if(it == subprograms_cache.end()) {
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
                VERIFY(table_count >= 0 && table_count <= 2, "Unknown dwarf line table count");
                if(ret == DW_DLV_NO_ENTRY) {
                    // TODO: Failing silently for now
                    return;
                }
                VERIFY(ret == DW_DLV_OK);
                line_contexts.insert({off, {version, line_context}});
            }
            Dwarf_Line* line_buffer = 0;
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
            Dwarf_Line last_line = 0;
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
                        last_line = 0;
                    } else {
                        last_lineaddr = lineaddr;
                        last_line = line;
                    }
                }
            }
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void walk_compilation_units(Dwarf_Addr pc, stacktrace_frame& frame) {
            // 0 passed as the die to the first call of dwarf_siblingof_b immediately after dwarf_next_cu_header_d
            // to fetch the cu die
            die_object cu_die(dbg, nullptr);
            cu_die = cu_die.get_sibling();
            if(!cu_die) {
                if(dump_dwarf) {
                    fprintf(stderr, "End walk_compilation_units\n");
                }
                return;
            }
            walk_die_list(
                cu_die,
                [this, &frame, pc] (const die_object& cu_die) {
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
                }
            );
        }

        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void walk_dbg(Dwarf_Addr pc, stacktrace_frame& frame) {
            // libdwarf keeps track of where it is in the file, dwarf_next_cu_header_d is statefull
            Dwarf_Unsigned next_cu_header;
            Dwarf_Half header_cu_type;
            //fprintf(stderr, "-----------------\n");
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
                walk_compilation_units(pc, frame);
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
            Dwarf_Arange* aranges;
            Dwarf_Signed arange_count;
            if(wrap(dwarf_get_aranges, dbg, &aranges, &arange_count) == DW_DLV_OK) {
                auto aranges_wrapper = raii_wrap(
                    aranges,
                    [this] (Dwarf_Arange* aranges) { dwarf_dealloc(dbg, aranges, DW_DLA_LIST); }
                );
                // Try to find pc in aranges
                Dwarf_Arange arange;
                if(wrap(dwarf_get_arange, aranges, arange_count, pc, &arange) == DW_DLV_OK) {
                    auto arange_wrapper = raii_wrap(
                        arange,
                        [this] (Dwarf_Arange arange) { dwarf_dealloc(dbg, arange, DW_DLA_ARANGE); }
                    );
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
                walk_dbg(pc, frame);
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
        // Locking around all libdwarf interaction per https://github.com/davea42/libdwarf-code/discussions/184
        const std::lock_guard<std::mutex> lock(mutex);
        for(const auto& obj_entry : collate_frames(frames, trace)) {
            try {
                const auto& obj_name = obj_entry.first;
                dwarf_resolver resolver(obj_name);
                // If there's no debug information it'll mark itself as not ok
                if(resolver.ok) {
                    for(const auto& entry : obj_entry.second) {
                        try {
                            const auto& dlframe = entry.first.get();
                            auto& frame = entry.second.get();
                            frame = resolver.resolve_frame(dlframe);
                        } catch(...) {
                            if(!should_absorb_trace_exceptions()) {
                                throw;
                            }
                        }
                    }
                }
            } catch(...) {
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
