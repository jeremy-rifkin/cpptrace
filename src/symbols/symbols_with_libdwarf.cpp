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

// some stuff is based on https://github.com/davea42/libdwarf-addr2line/blob/master/addr2line.c, mainly line handling
// then much expanded for symbols and efficiency
// dwarf5_ranges and dwarf4_ranges utility functions are taken from there directly, also pc_in_die

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
            Dwarf_Off off = get_global_offset();
            Dwarf_Bool is_info = dwarf_get_die_infotypes_flag(die);
            Dwarf_Die die_copy = 0;
            VERIFY(wrap(dwarf_offdie_b, dbg, off, is_info, &die_copy) == DW_DLV_OK);
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
            std::string str;
            if(ret != DW_DLV_NO_ENTRY) {
                str = name;
                dwarf_dealloc(dbg, name, DW_DLA_STRING);
            }
            return name;
        }

        optional<std::string> get_string_attribute(Dwarf_Half dw_attrnum) const {
            Dwarf_Attribute attr;
            if(wrap(dwarf_attr, die, dw_attrnum, &attr) == DW_DLV_OK) {
                char* raw_str;
                std::string str;
                VERIFY(wrap(dwarf_formstring, attr, &raw_str) == DW_DLV_OK);
                str = raw_str;
                dwarf_dealloc(dbg, raw_str, DW_DLA_STRING);
                dwarf_dealloc_attribute(attr);
                return str;
            } else {
                return nullopt;
            }
        }

        bool has_attr(Dwarf_Half dw_attrnum) const {
            Dwarf_Bool present = false;
            VERIFY(wrap(dwarf_hasattr, die, dw_attrnum, &present) == DW_DLV_OK);
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

        die_object resolve_reference_attribute(Dwarf_Half dw_attrnum) const {
            Dwarf_Attribute attr;
            VERIFY(dwarf_attr(die, dw_attrnum, &attr, nullptr) == DW_DLV_OK);
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
                        Dwarf_Off goff = 0;
                        VERIFY(wrap(dwarf_convert_to_global_offset, attr, off, &goff) == DW_DLV_OK);
                        Dwarf_Die targ_die_a = 0;
                        VERIFY(wrap(dwarf_offdie_b, dbg, goff, is_info, &targ_die_a) == DW_DLV_OK);
                        dwarf_dealloc_attribute(attr);
                        return die_object(dbg, targ_die_a);
                    }
                case DW_FORM_ref_addr:
                    {
                        Dwarf_Off off;
                        VERIFY(wrap(dwarf_global_formref, attr, &off) == DW_DLV_OK);
                        int is_info_a = dwarf_get_die_infotypes_flag(die);
                        Dwarf_Die targ_die_a = 0;
                        VERIFY(wrap(dwarf_offdie_b, dbg, off, is_info_a, &targ_die_a) == DW_DLV_OK);
                        dwarf_dealloc_attribute(attr);
                        return die_object(dbg, targ_die_a);
                    }
                case DW_FORM_ref_sig8:
                    {
                        Dwarf_Sig8 signature;
                        VERIFY(wrap(dwarf_formsig8, attr, &signature) == DW_DLV_OK);
                        Dwarf_Die  targdie = 0;
                        Dwarf_Bool targ_is_info = false;
                        VERIFY(wrap(dwarf_find_die_given_sig8, dbg, &signature, &targdie, &targ_is_info) == DW_DLV_OK);
                        dwarf_dealloc_attribute(attr);
                        return die_object(dbg, targdie);
                    }
                default:
                    fprintf(stderr, "unknown form for attribute %d %d\n", dw_attrnum, form);
                    exit(1);
            }
        }

        Dwarf_Unsigned get_ranges_offset(Dwarf_Attribute attr) const {
            Dwarf_Unsigned off = 0;
            Dwarf_Half attrform = 0;
            VERIFY(wrap(dwarf_whatform, attr, &attrform) == DW_DLV_OK);
            if (attrform == DW_FORM_rnglistx) {
                VERIFY(wrap(dwarf_formudata, attr, &off) == DW_DLV_OK);
            } else {
                VERIFY(wrap(dwarf_global_formref, attr, &off) == DW_DLV_OK);
            }
            return off;
        }

        // ranges code based on libdwarf-addr2line
        int dwarf5_ranges(
            Dwarf_Die cu_die,
            Dwarf_Addr *lowest,
            Dwarf_Addr *highest,
            std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>>* ranges_vec // TODO: Super hacky
        ) const {
            Dwarf_Unsigned offset = 0;
            Dwarf_Attribute attr = 0;
            Dwarf_Half attrform = 0;
            Dwarf_Unsigned i = 0;
            int res = 0;

            res = wrap(dwarf_attr, cu_die, DW_AT_ranges, &attr);
            if(res != DW_DLV_OK) {
                return res;
            }
            offset = get_ranges_offset(attr);
            if(true) {
                Dwarf_Unsigned rlesetoffset = 0;
                Dwarf_Unsigned rnglists_count = 0;
                Dwarf_Rnglists_Head head = 0;

                VERIFY(wrap(dwarf_whatform, attr, &attrform) == DW_DLV_OK);
                // offset is in .debug_rnglists
                res = wrap(
                    dwarf_rnglists_get_rle_head,
                    attr,
                    attrform,
                    offset,
                    &head,
                    &rnglists_count,
                    &rlesetoffset
                );
                VERIFY(res == DW_DLV_OK);
                if(res != DW_DLV_OK) {
                    /* ASSERT: is DW_DLV_NO_ENTRY */
                    dwarf_dealloc_attribute(attr);
                    return res;
                }
                for( ; i < rnglists_count; ++i) {
                    unsigned entrylen = 0;
                    unsigned rle_val = 0;
                    Dwarf_Unsigned raw1 = 0;
                    Dwarf_Unsigned raw2 = 0;
                    Dwarf_Bool unavail = 0;
                    Dwarf_Unsigned cooked1 = 0;
                    Dwarf_Unsigned cooked2 = 0;

                    res = wrap(
                        dwarf_get_rnglists_entry_fields_a,
                        head,
                        i,
                        &entrylen,
                        &rle_val,
                        &raw1,
                        &raw2,
                        &unavail,
                        &cooked1,
                        &cooked2
                    );
                    if(res != DW_DLV_OK) {
                        ASSERT(res == DW_DLV_NO_ENTRY);
                        continue;
                    }
                    if(unavail) {
                        continue;
                    }
                    switch(rle_val) {
                    case DW_RLE_end_of_list:
                    case DW_RLE_base_address:
                    case DW_RLE_base_addressx:
                        /* These are accounted for already */
                        break;
                    case DW_RLE_offset_pair:
                    case DW_RLE_startx_endx:
                    case DW_RLE_start_end:
                    case DW_RLE_startx_length:
                    case DW_RLE_start_length:
                        if(ranges_vec) {
                            ranges_vec->push_back({cooked1, cooked2});
                        }
                        if(cooked1 < *lowest) {
                            *lowest = cooked1;
                        }
                        if(cooked2 > *highest) {
                            *highest = cooked2;
                        }
                        break;
                    default:
                        PANIC("Something is wrong");
                        break;
                    }
                }
                dwarf_dealloc_rnglists_head(head);
            }
            dwarf_dealloc_attribute(attr);
            return DW_DLV_OK;
        }

        // ranges code based on libdwarf-addr2line
        int dwarf4_ranges(
            Dwarf_Die cu_die,
            Dwarf_Addr cu_lowpc,
            Dwarf_Addr *lowest,
            Dwarf_Addr *highest,
            std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>>* ranges_vec // TODO: Super hacky
        ) const {
            Dwarf_Unsigned offset;
            Dwarf_Attribute attr = 0;
            int res = 0;

            res = wrap(dwarf_attr, cu_die, DW_AT_ranges, &attr);
            if(res != DW_DLV_OK) {
                return res;
            }
            if(wrap(dwarf_global_formref, attr, &offset) == DW_DLV_OK) {
                Dwarf_Signed count = 0;
                Dwarf_Ranges *ranges = 0;
                Dwarf_Addr baseaddr = 0;
                if(cu_lowpc != 0xffffffffffffffff) {
                    baseaddr = cu_lowpc;
                }
                VERIFY(
                    wrap(
                        dwarf_get_ranges_b,
                        dbg,
                        offset,
                        cu_die,
                        nullptr,
                        &ranges,
                        &count,
                        nullptr
                    ) == DW_DLV_OK
                );
                for(int i = 0; i < count; i++) {
                    Dwarf_Ranges *cur = ranges + i;

                    if(cur->dwr_type == DW_RANGES_ENTRY) {
                        Dwarf_Addr rng_lowpc, rng_highpc;
                        rng_lowpc = baseaddr + cur->dwr_addr1;
                        rng_highpc = baseaddr + cur->dwr_addr2;
                        if(ranges_vec) {
                            ranges_vec->push_back({rng_lowpc, rng_highpc});
                        }
                        if(rng_lowpc < *lowest) {
                            *lowest = rng_lowpc;
                        }
                        if(rng_highpc > *highest) {
                            *highest = rng_highpc;
                        }
                    } else if(cur->dwr_type ==
                        DW_RANGES_ADDRESS_SELECTION) {
                        baseaddr = cur->dwr_addr2;
                    } else {  // DW_RANGES_END
                        baseaddr = cu_lowpc;
                    }
                }
                dwarf_dealloc_ranges(dbg, ranges, count);
            }
            dwarf_dealloc_attribute(attr);
            return DW_DLV_OK;
        }

        // pc_in_die code based on libdwarf-addr2line
        // TODO: Super hacky. And ugly code duplication.
        std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>> get_pc_range(int version) const {
            int ret;
            Dwarf_Addr cu_lowpc = 0xffffffffffffffff;
            Dwarf_Addr cu_highpc = 0;
            enum Dwarf_Form_Class highpc_cls;
            Dwarf_Addr lowest = 0xffffffffffffffff;
            Dwarf_Addr highest = 0;

            ret = wrap(dwarf_lowpc, die, &cu_lowpc);
            if(ret == DW_DLV_OK) {
                ret = wrap(dwarf_highpc_b, die, &cu_highpc, nullptr, &highpc_cls);
                if(ret == DW_DLV_OK) {
                    if(highpc_cls == DW_FORM_CLASS_CONSTANT) {
                        cu_highpc += cu_lowpc;
                    }
                    return {{cu_lowpc, cu_highpc}};
                }

            }
            std::vector<std::pair<Dwarf_Addr, Dwarf_Addr>> ranges;
            if(version >= 5) {
                ret = dwarf5_ranges(die, &lowest, &highest, &ranges);
            } else {
                ret = dwarf4_ranges(die, cu_lowpc, &lowest, &highest, &ranges);
            }
            return ranges;
            //return {lowest, highest};
        }

        Dwarf_Bool pc_in_die(int version, Dwarf_Addr pc) const {
            int ret;
            Dwarf_Addr cu_lowpc = 0xffffffffffffffff;
            Dwarf_Addr cu_highpc = 0;
            enum Dwarf_Form_Class highpc_cls;
            Dwarf_Addr lowest = 0xffffffffffffffff;
            Dwarf_Addr highest = 0;

            ret = wrap(dwarf_lowpc, die, &cu_lowpc);
            if(ret == DW_DLV_OK) {
                if(pc == cu_lowpc) {
                    return true;
                }
                ret = wrap(dwarf_highpc_b, die, &cu_highpc, nullptr, &highpc_cls);
                if(ret == DW_DLV_OK) {
                    if(highpc_cls == DW_FORM_CLASS_CONSTANT) {
                        cu_highpc += cu_lowpc;
                    }
                    if(pc >= cu_lowpc && pc < cu_highpc) {
                        return true;
                    }
                }
            }
            if(version >= 5) {
                ret = dwarf5_ranges(die, &lowest, &highest, nullptr);
            } else {
                ret = dwarf4_ranges(die, cu_lowpc, &lowest, &highest, nullptr);
            }
            if(pc >= lowest && pc < highest) {
                return true;
            }
            return false;
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
        Dwarf_Small table_count;
        Dwarf_Line_Context ctx;
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
                dwarf_srclines_dealloc_b(entry.second.ctx);
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
            Dwarf_Addr pc,
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
                    // TODO: Passing pc here is misleading
                    return retrieve_symbol_for_subprogram(spec, pc, dwversion, frame);
                } else if(die.has_attr(DW_AT_abstract_origin)) {
                    die_object spec = die.resolve_reference_attribute(DW_AT_abstract_origin);
                    // TODO: Passing pc here is misleading
                    return retrieve_symbol_for_subprogram(spec, pc, dwversion, frame);
                }
            }
            // TODO: Disabled for now
            // TODO: Handle namespaces
            /*std::string name = die.get_name();
            std::vector<std::string> params;
            auto child = die.get_child();
            if(child) {
                walk_die_list_recursive(
                    dbg,
                    child,
                    [pc, dwversion, &frame, &params] (Dwarf_Debug dbg, const die_object& die) {
                        if(die.get_tag() == DW_TAG_formal_parameter) {
                            // TODO: Ignore DW_AT_artificial
                            params.push_back(resolve_type(dbg, get_type_die(dbg, die)));
                        }
                    }
                );
            } else {
                fprintf(stderr, "no child %s\n", name.c_str());
            }
            frame.symbol = name + "(" + join(params, ", ") + ")";*/
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
                            retrieve_symbol_for_subprogram(die, pc, dwversion, frame);
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
                    //die.print();
                    switch(die.get_tag()) {
                        case DW_TAG_subprogram:
                            {
                                auto ranges_vec = die.get_pc_range(dwversion);
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
            //retrieve_symbol_walk(std::move(cu_die), pc, dwversion, frame);
            //return;
            auto off = cu_die.get_global_offset();
            auto it = subprograms_cache.find(off);
            if(it == subprograms_cache.end()) {
                std::vector<subprogram_entry> vec;
                preprocess_subprograms(cu_die, dwversion, vec);
                std::sort(vec.begin(), vec.end(), [] (const subprogram_entry& a, const subprogram_entry& b) {
                    return a.low < b.low;
                });
                //for(const auto& entry : vec) {
                //    fprintf(stderr, "vec -> %llx %llx %s\n", entry.low, entry.high, entry.die.get_name().c_str());
                //}
                subprograms_cache.emplace(off, std::move(vec));
                it = subprograms_cache.find(off);
            }
            auto& vec = it->second;
            auto vec_it = std::lower_bound(
                vec.begin(),
                vec.end(),
                pc,
                [] (const subprogram_entry& entry, Dwarf_Addr pc) {
                    //fprintf(stderr, "%llx %llx\n", entry.low, pc);
                    return entry.low < pc;
                }
            );
            //fprintf(stderr, "retrieve_symbol %llx\n", pc);
            // vec_it is first >= pc
            // we want first <= pc
            if(vec_it != vec.begin()) {
                vec_it--;
            }
            // If the vector has been empty this can happen
            if(vec_it != vec.end()) {
                //vec_it->die.print();
                if(vec_it->die.pc_in_die(dwversion, pc)) {
                    retrieve_symbol_for_subprogram(vec_it->die, pc, dwversion, frame);
                }
            } else {
                ASSERT(vec.size() == 0, "Vec should be empty?");
            }
        }

        void handle_line(Dwarf_Line line, stacktrace_frame& frame) {
            char what[] = "??";
            char *         linesrc = what;
            Dwarf_Unsigned lineno = 0;

            if(line) {
                VERIFY(wrap(dwarf_linesrc, line, &linesrc) == DW_DLV_OK);
                VERIFY(wrap(dwarf_lineno, line, &lineno) == DW_DLV_OK);
            }
            if(dump_dwarf) {
                printf("%s:%u\n", linesrc, to<unsigned>(lineno));
            }
            frame.line = static_cast<uint_least32_t>(lineno);
            frame.filename = linesrc;
            if(line) {
                dwarf_dealloc(dbg, linesrc, DW_DLA_STRING);
            }
        }

        // retrieve_line_info code based on libdwarf-addr2line
        CPPTRACE_FORCE_NO_INLINE_FOR_PROFILING
        void retrieve_line_info(
            const die_object& die,
            Dwarf_Addr pc,
            Dwarf_Half dwversion,
            stacktrace_frame& frame
        ) {
            Dwarf_Unsigned version;
            Dwarf_Small table_count;
            Dwarf_Line_Context ctxt;
            (void)dwversion;
            auto off = die.get_global_offset();
            auto it = line_contexts.find(off);
            if(it != line_contexts.end()) {
                auto& entry = it->second;
                version = entry.version;
                table_count = entry.table_count;
                ctxt = entry.ctx;
            } else {
                int ret = wrap(
                    dwarf_srclines_b,
                    die.get(),
                    &version,
                    &table_count,
                    &ctxt
                );
                if(ret == DW_DLV_NO_ENTRY) {
                    // TODO: Failing silently for now
                    return;
                }
                line_contexts.insert({off, {version, table_count, ctxt}});
            }
            if(table_count == 1) {
                Dwarf_Line* linebuf = 0;
                Dwarf_Signed linecount = 0;
                Dwarf_Addr prev_lineaddr = 0;
                VERIFY(
                    wrap(
                        dwarf_srclines_from_linecontext,
                        ctxt,
                        &linebuf,
                        &linecount
                    ) == DW_DLV_OK
                );
                Dwarf_Line prev_line = 0;
                for(int i = 0; i < linecount; i++) {
                    Dwarf_Line line = linebuf[i];
                    Dwarf_Addr lineaddr = 0;
                    VERIFY(wrap(dwarf_lineaddr, line, &lineaddr) == DW_DLV_OK);
                    if(pc == lineaddr) {
                        // Find the last line entry containing current pc
                        Dwarf_Line last_pc_line = line;
                        for(int j = i + 1; j < linecount; j++) {
                            Dwarf_Line j_line = linebuf[j];
                            VERIFY(wrap(dwarf_lineaddr, j_line, &lineaddr) == DW_DLV_OK);
                            if(pc == lineaddr) {
                                last_pc_line = j_line;
                            }
                        }
                        handle_line(last_pc_line, frame);
                        break;
                    } else if(prev_line && pc > prev_lineaddr && pc < lineaddr) {
                        handle_line(prev_line, frame);
                        break;
                    }
                    Dwarf_Bool is_lne;
                    VERIFY(wrap(dwarf_lineendsequence, line, &is_lne) == DW_DLV_OK);
                    if(is_lne) {
                        prev_line = 0;
                    } else {
                        prev_lineaddr = lineaddr;
                        prev_line = line;
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
                        retrieve_line_info(cu_die, pc, dwversion, frame); // no offset for line info
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
            Dwarf_Arange *aranges;
            Dwarf_Signed arange_count;
            if(wrap(dwarf_get_aranges, dbg, &aranges, &arange_count) == DW_DLV_OK) {
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
                    retrieve_line_info(cu_die, pc, dwversion, frame); // no offset for line info
                    retrieve_symbol(cu_die, pc, dwversion, frame);
                    dwarf_dealloc(dbg, arange, DW_DLA_ARANGE);
                }
                dwarf_dealloc(dbg, aranges, DW_DLA_LIST);
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

    // Currently commented out. Need to decide if this is worth it. The mangled name is easier and maybe more
    // trustworthy. This is a lot of effort for something that would only be relevant for extern "C" methods.
    /*die_object get_type_die(Dwarf_Debug dbg, const die_object& die) {
        Dwarf_Off type_offset;
        Dwarf_Bool is_info;
        int ret = dwarf_dietype_offset(die.get(), &type_offset, &is_info, nullptr);
        if(ret == DW_DLV_OK) {
            Dwarf_Die type_die;
            ret = dwarf_offdie_b(
                dbg,
                type_offset,
                is_info,
                &type_die,
                nullptr
            );
            if(ret == DW_DLV_OK) {
                return die_object(dbg, type_die);
            } else {
                fprintf(stderr, "Error\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "no type offset??\n");
        }
        return die_object(dbg, nullptr);
    }

    bool has_type(Dwarf_Debug dbg, const die_object& die) {
        Dwarf_Attribute attr;
        int ret = dwarf_attr(die.get(), DW_AT_type, &attr, nullptr);
        if(ret == DW_DLV_NO_ENTRY) {
            return false;
        } else if(ret == DW_DLV_OK) {
            dwarf_dealloc_attribute(attr);
            return true;
        } else {
            fprintf(stderr, "Error\n");
            exit(1);
        }
    }

    struct type_result {
        std::string base;
        std::string extent;

        std::string get_type() {
            return base + extent;
        }
    };

    // TODO: ::*, namespace lookup, arrays
    // DW_TAG_namespace
    const char* tag_to_keyword(Dwarf_Half tag) {
        switch(tag) {
            case DW_TAG_atomic_type:
                return "_Atomic";
            case DW_TAG_const_type:
                return "const";
            case DW_TAG_volatile_type:
                return "volatile";
            case DW_TAG_restrict_type:
                return "restrict";
            default:
                {
                    const char* tag_name = nullptr;
                    dwarf_get_TAG_name(tag, &tag_name);
                    fprintf(stderr, "tag_to_keyword unknown tag %s\n", tag_name);
                    exit(1);
                }
        }
    }
    const char* tag_to_ptr_ref(Dwarf_Half tag) {
        switch(tag) {
            case DW_TAG_pointer_type:
                return "*";
            case DW_TAG_ptr_to_member_type:
                return "::*"; // TODO
            case DW_TAG_reference_type:
                return "&";
            case DW_TAG_rvalue_reference_type:
                return "&&";
            default:
                {
                    const char* tag_name = nullptr;
                    dwarf_get_TAG_name(tag, &tag_name);
                    fprintf(stderr, "tag_to_ptr_ref unknown tag %s\n", tag_name);
                    exit(1);
                }
        }
    }

    std::string resolve_type(Dwarf_Debug dbg, const die_object& die, std::string build = "");

    std::string get_array_extents(Dwarf_Debug dbg, const die_object& die) {
        VERIFY(die.get_tag() == DW_TAG_array_type);
        std::string extents = "";
        walk_die_list(dbg, die.get_child(), [&extents](Dwarf_Debug dbg, const die_object& subrange) {
            if(subrange.get_tag() == DW_TAG_subrange_type) {
                Dwarf_Attribute attr = 0;
                int res = 0;
                res = dwarf_attr(subrange.get(), DW_AT_upper_bound, &attr, nullptr);
                if(res != DW_DLV_OK) {
                    fprintf(stderr, "Error\n");
                    return;
                }
                Dwarf_Half form;
                res = dwarf_whatform(attr, &form, nullptr);
                if(res != DW_DLV_OK) {
                    fprintf(stderr, "Error\n");
                    return;
                }
                //fprintf(stderr, "form: %d\n", form);
                Dwarf_Unsigned val;
                res = dwarf_formudata(attr, &val, nullptr);
                if(res != DW_DLV_OK) {
                    fprintf(stderr, "Error\n");
                    return;
                }
                extents += "[" + std::to_string(val + 1) + "]";
                dwarf_dealloc_attribute(attr);
            } else {
                fprintf(stderr, "unknown tag %s\n", subrange.get_tag_name());
            }
        });
        return extents;
    }

    std::string get_parameters(Dwarf_Debug dbg, const die_object& die) {
        VERIFY(die.get_tag() == DW_TAG_subroutine_type);
        std::vector<std::string> params;
        walk_die_list(dbg, die.get_child(), [&params](Dwarf_Debug dbg, const die_object& die) {
            if(die.get_tag() == DW_TAG_formal_parameter) {
                // TODO: Ignore DW_AT_artificial
                params.push_back(resolve_type(dbg, get_type_die(dbg, die)));
            }
        });
        return "(" + join(params, ", ") + ")";
    }

    std::string resolve_type(Dwarf_Debug dbg, const die_object& die, std::string build) {
        switch(auto tag = die.get_tag()) {
            case DW_TAG_base_type:
            case DW_TAG_class_type:
            case DW_TAG_structure_type:
            case DW_TAG_union_type:
            case DW_TAG_enumeration_type:
                return die.get_name() + build;
            case DW_TAG_typedef:
                return resolve_type(dbg, get_type_die(dbg, die));
            //case DW_TAG_subroutine_type:
            //    {
            //        // If there's no DW_AT_type then it's a void
            //        std::vector<std::string> params;
            //        // TODO: Code duplication with retrieve_symbol_for_subprogram?
            //        walk_die_list(dbg, die.get_child(), [&params] (Dwarf_Debug dbg, const die_object& die) {
            //            if(die.get_tag() == DW_TAG_formal_parameter) {
            //                // TODO: Ignore DW_AT_artificial
            //                params.push_back(resolve_type(dbg, get_type_die(dbg, die)));
            //            }
            //        });
            //        if(!has_type(dbg, die)) {
            //            return "void" + (build.empty() ? "" : "(" + build + ")") + "(" + join(params, ", ") + ")";
            //        } else {
            //            // resolving return type, building on build
            //            return resolve_type(
            //                dbg, get_type_die(dbg, die),
            //                (build.empty() ? "" : "(" + build + ")")
            //                    + "("
            //                    + join(params, ", ")
            //                    + ")"
            //            );
            //        }
            //    }
            //case DW_TAG_array_type:
            //    return resolve_type(dbg, get_type_die(dbg, die), (build.empty() ? "" : "(" + build + ")") + "[" + "x" + "]");
            case DW_TAG_pointer_type:
            case DW_TAG_reference_type:
            case DW_TAG_rvalue_reference_type:
            case DW_TAG_ptr_to_member_type:
                {
                    const auto child = get_type_die(dbg, die); // AST child, rather than dwarf child
                    const auto child_tag = child.get_tag();
                    switch(child_tag) {
                        case DW_TAG_subroutine_type:
                            if(!has_type(dbg, child)) {
                                return "void(" + std::string(tag_to_ptr_ref(tag)) + build + ")" + get_parameters(dbg, child);
                            } else {
                                return resolve_type(
                                    dbg,
                                    get_type_die(dbg, child),
                                    "(" + std::string(tag_to_ptr_ref(tag)) + build + ")" + get_parameters(dbg, child)
                                );
                            }
                        case DW_TAG_array_type:
                            return resolve_type(
                                dbg,
                                get_type_die(dbg, child),
                                "(" + std::string(tag_to_ptr_ref(tag)) + build + ")" + get_array_extents(dbg, child)
                            );
                        default:
                            if(build.empty()) {
                                return resolve_type(dbg, get_type_die(dbg, die), tag_to_ptr_ref(tag));
                            } else {
                                return resolve_type(
                                    dbg,
                                    get_type_die(dbg, die),
                                    std::string(tag_to_ptr_ref(tag)) + " " + build
                                );
                            }
                    }
                }
            case DW_TAG_const_type:
            case DW_TAG_atomic_type:
            case DW_TAG_volatile_type:
            case DW_TAG_restrict_type:
                {
                    const auto child = get_type_die(dbg, die); // AST child, rather than dwarf child
                    const auto child_tag = child.get_tag();
                    switch(child_tag) {
                        case DW_TAG_base_type:
                        case DW_TAG_class_type:
                        case DW_TAG_typedef:
                            return std::string(tag_to_keyword(tag))
                                    + " "
                                    + resolve_type(dbg, get_type_die(dbg, die), build);
                        default:
                            return resolve_type(
                                dbg,
                                get_type_die(dbg, die),
                                std::string(tag_to_keyword(tag)) + " " + build
                            );
                    }
                }
            default:
                {
                    fprintf(stderr, "unknown tag %s\n", die.get_tag_name());
                    exit(1);
                }
        }
        return {"<unknown>", "<unknown>"};
    }*/
}
}
}

#endif
