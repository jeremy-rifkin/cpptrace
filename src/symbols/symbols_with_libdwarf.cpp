#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/program_name.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <libdwarf.h>
#include <dwarf.h>

#include "../platform/object.hpp"

// some stuff is based on https://github.com/davea42/libdwarf-addr2line/blob/master/addr2line.c, mainly line handling
// then much expanded for symbols and efficiency
// dwarf5_ranges and dwarf4_ranges utility functions are taken from there directly, also pc_in_die

// TODO
// Inlined calls
// Memoizing / lazy loading
// More utils to clean this up, some wrapper for unique_ptr
// Ensure memory is being cleaned up properly
// Efficiency tricks
// Implementation cleanup
// Properly get the image base

#define DW_PR_DUx "llx"
#define DW_PR_DUu "llu"

static int dwarf5_ranges(Dwarf_Die cu_die, Dwarf_Addr *lowest, Dwarf_Addr *highest) {
    Dwarf_Unsigned offset = 0;
    Dwarf_Attribute attr = 0;
    Dwarf_Half attrform = 0;
    Dwarf_Unsigned i = 0;
    int res = 0;

    res = dwarf_attr(cu_die, DW_AT_ranges, &attr, nullptr);
    if(res != DW_DLV_OK) {
        return res;
    }
    if(dwarf_global_formref(attr, &offset, nullptr) == DW_DLV_OK) {
        Dwarf_Unsigned rlesetoffset = 0;
        Dwarf_Unsigned rnglists_count = 0;
        Dwarf_Rnglists_Head head = 0;

        dwarf_whatform(attr, &attrform, nullptr);
        /* offset is in .debug_rnglists */
        res = dwarf_rnglists_get_rle_head(
            attr,
            attrform,offset,
            &head,
            &rnglists_count,
            &rlesetoffset,
            nullptr
        );
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

            res = dwarf_get_rnglists_entry_fields_a(
                head,
                i,
                &entrylen,
                &rle_val,
                &raw1,
                &raw2,
                &unavail,
                &cooked1,
                &cooked2,
                nullptr
            );
            if(res != DW_DLV_OK) {
                /* ASSERT: is DW_DLV_NO_ENTRY */
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
                if(cooked1 < *lowest) {
                    *lowest = cooked1;
                }
                if(cooked2 > *highest) {
                    *highest = cooked2;
                }
            default:
                /* Something is wrong. */
                break;

            }
        }
        dwarf_dealloc_rnglists_head(head);
    }
    dwarf_dealloc_attribute(attr);
    return DW_DLV_OK;
}

static int dwarf4_ranges(
    Dwarf_Debug dbg,
    Dwarf_Die cu_die,
    Dwarf_Addr cu_lowpc,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest
) {
    Dwarf_Unsigned offset;
    Dwarf_Attribute attr = 0;
    int res = 0;

    res = dwarf_attr(cu_die, DW_AT_ranges, &attr, nullptr);
    if(res != DW_DLV_OK) {
        return res;
    }
    if(dwarf_global_formref(attr, &offset, nullptr) == DW_DLV_OK) {
        Dwarf_Signed count = 0;
        Dwarf_Ranges *ranges = 0;
        Dwarf_Addr baseaddr = 0;
        if(cu_lowpc != 0xffffffffffffffff) {
            baseaddr = cu_lowpc;
        }
        res = dwarf_get_ranges_b(
            dbg,
            offset,
            cu_die,
            nullptr,
            &ranges,
            &count,
            nullptr,
            nullptr
        );
        for(int i = 0; i < count; i++) {
            Dwarf_Ranges *cur = ranges + i;

            if(cur->dwr_type == DW_RANGES_ENTRY) {
                Dwarf_Addr rng_lowpc, rng_highpc;
                rng_lowpc = baseaddr + cur->dwr_addr1;
                rng_highpc = baseaddr + cur->dwr_addr2;
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

namespace cpptrace {
    namespace detail {
        // printbugging as we go
        constexpr bool dump_dwarf = false;

        static void err_handler(Dwarf_Error err, Dwarf_Ptr errarg) {
            printf("libdwarf error reading %s: %lu %s\n", "xx", (unsigned long)dwarf_errno(err), dwarf_errmsg(err));
            if(errarg) {
                printf("Error: errarg is nonnull but it should be null\n");
            }
            printf("Giving up");
            exit(1);
        }

        static void print_line(Dwarf_Debug dbg, Dwarf_Line line, Dwarf_Addr pc, stacktrace_frame& frame) {
            char what[] = "??";
            char *         linesrc = what;
            Dwarf_Unsigned lineno = 0;

            (void)pc;

            if(line) {
                /*  These never return DW_DLV_NO_ENTRY */
                dwarf_linesrc(line, &linesrc, nullptr);
                dwarf_lineno(line, &lineno, nullptr);
            }
            if(dump_dwarf) {
                printf("%s:%" DW_PR_DUu "\n", linesrc, lineno);
            }
            frame.line = static_cast<uint_least32_t>(lineno);
            frame.filename = linesrc;
            if(line) {
                dwarf_dealloc(dbg, linesrc, DW_DLA_STRING);
            }
        }

        static Dwarf_Bool pc_in_die(Dwarf_Debug dbg, Dwarf_Die die,int version, Dwarf_Addr pc) {
            int ret;
            Dwarf_Addr cu_lowpc = 0xffffffffffffffff;
            Dwarf_Addr cu_highpc = 0;
            enum Dwarf_Form_Class highpc_cls;
            Dwarf_Addr lowest = 0xffffffffffffffff;
            Dwarf_Addr highest = 0;

            ret = dwarf_lowpc(die, &cu_lowpc, nullptr);
            if(ret == DW_DLV_OK) {
                if(pc == cu_lowpc) {
                    return true;
                }
                ret = dwarf_highpc_b(die, &cu_highpc,
                    nullptr, &highpc_cls, nullptr);
                if(ret == DW_DLV_OK) {
                    if(highpc_cls == DW_FORM_CLASS_CONSTANT) {
                        cu_highpc += cu_lowpc;
                    }
                    //fprintf(stderr, "low: %llx  high: %llx  pc: %llx\n", cu_lowpc, cu_highpc, pc);
                    if(pc >= cu_lowpc && pc < cu_highpc) {
                        return true;
                    }
                }
            }
            if(version >= 5) {
                ret = dwarf5_ranges(die,
                    &lowest,&highest);
            } else {
                ret = dwarf4_ranges(dbg,die,cu_lowpc,
                    &lowest,&highest);
            }
            //fprintf(stderr, "low: %llu  high: %llu\n", lowest, highest);
            if(pc >= lowest && pc < highest) {
                return true;
            }
            return false;
        }

        static_assert(std::is_pointer<Dwarf_Die>::value, "Dwarf_Die not a pointer");
        static_assert(std::is_pointer<Dwarf_Debug>::value, "Dwarf_Debug not a pointer");

        struct die_object {
            Dwarf_Debug dbg = nullptr;
            Dwarf_Die die = nullptr;
            die_object(Dwarf_Debug dbg, Dwarf_Die die) : dbg(dbg), die(die) {}
            ~die_object() {
                if(die) {
                    dwarf_dealloc(dbg, die, DW_DLA_DIE);
                }
            }
            die_object(const die_object&) = delete;
            die_object& operator=(const die_object&) = delete;
            die_object(die_object&& other) : dbg(other.dbg), die(other.die) {
                other.die = nullptr;
            }
            die_object& operator=(die_object&& other) {
                dbg = other.dbg;
                die = other.die;
                other.die = nullptr;
                return *this;
            }
            die_object get_child() const {
                Dwarf_Die child = nullptr;
                int ret = dwarf_child(
                    die,
                    &child,
                    nullptr
                );
                if(ret == DW_DLV_OK) {
                    return die_object(dbg, child);
                } else if(ret == DW_DLV_NO_ENTRY) {
                    return die_object(dbg, 0);
                } else {
                    fprintf(stderr, "Error\n");
                    exit(1);
                }
            }
            die_object get_sibling() const {
                Dwarf_Die sibling = 0;
                int ret = dwarf_siblingof_b(dbg, die, true, &sibling, nullptr);
                if(ret == DW_DLV_OK) {
                    return die_object(dbg, sibling);
                } else if(ret == DW_DLV_NO_ENTRY) {
                    return die_object(dbg, 0);
                } else {
                    fprintf(stderr, "Error\n");
                    exit(1);
                }
            }
            operator bool() const {
                return die != nullptr;
            }
            Dwarf_Die get() const {
                return die;
            }
            std::string get_name() const {
                char* name;
                int ret = dwarf_diename(die, &name, nullptr);
                std::string str;
                if(ret != DW_DLV_NO_ENTRY) {
                    str = name;
                    dwarf_dealloc(dbg, name, DW_DLA_STRING);
                }
                return name;
            }
            Dwarf_Half get_tag() const {
                Dwarf_Half tag = 0;
                dwarf_tag(die, &tag, nullptr);
                return tag;
            }
        };

        void walk_die_list(
            Dwarf_Debug dbg,
            const die_object& die,
            std::function<void(Dwarf_Debug, const die_object&)> fn
        ) {
            fn(dbg, die);
            die_object current = die.get_sibling();
            while(true) {
                if(!current) {
                    if(dump_dwarf) {
                        fprintf(stderr, "End walk_die_list\n");
                    }
                    return;
                }
                fn(dbg, current);
                current = current.get_sibling();
            }
        }

        void walk_die_list_recursive(
            Dwarf_Debug dbg,
            const die_object& die,
            std::function<void(Dwarf_Debug, const die_object&)> fn
        ) {
            walk_die_list(
                dbg,
                die,
                [&fn](Dwarf_Debug dbg, const die_object& die) {
                    auto child = die.get_child();
                    if(child) {
                        walk_die_list_recursive(dbg, child, fn);
                    }
                    fn(dbg, die);
                }
            );
        }

        die_object get_type_die(Dwarf_Debug dbg, const die_object& die) {
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
                dwarf_dealloc(dbg, attr, DW_DLA_ATTR);
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

        // TODO: ::*, namespace lookup, arrays, volatile, restrict, better pointer handling
        type_result resolve_type(Dwarf_Debug dbg, const die_object& die, std::string quantifiers = "") {
            switch(auto tag = die.get_tag()) {
                case DW_TAG_base_type:
                case DW_TAG_class_type:
                    return {(quantifiers.empty() ? "" : quantifiers + " ") + die.get_name(), ""};
                case DW_TAG_typedef:
                    return resolve_type(dbg, get_type_die(dbg, die));
                case DW_TAG_pointer_type:
                    {
                        auto type = resolve_type(dbg, get_type_die(dbg, die));
                        if(type.extent.empty()) {
                            return {type.base + "*", ""};
                        } else {
                            auto q = quantifiers.empty() ? "" : " " + quantifiers;
                            return {type.base + "(*" + q + ")" + type.extent, ""};
                        }
                    }
                case DW_TAG_reference_type:
                    {
                        auto type = resolve_type(dbg, get_type_die(dbg, die));
                        if(type.extent.empty()) {
                            return {type.base + "&", ""};
                        } else {
                            auto q = quantifiers.empty() ? "" : " " + quantifiers;
                            return {type.base + "(&" + q + ")" + type.extent, ""};
                        }
                    }
                case DW_TAG_rvalue_reference_type:
                    {
                        auto type = resolve_type(dbg, get_type_die(dbg, die));
                        if(type.extent.empty()) {
                            return {type.base + "&&", ""};
                        } else {
                            auto q = quantifiers.empty() ? "" : " " + quantifiers;
                            return {type.base + "(&&" + q + ")" + type.extent, ""};
                        }
                    }
                case DW_TAG_subroutine_type:
                    {
                        // If there's no DW_AT_type then it's a void()
                        if(!has_type(dbg, die)) {
                            return {"void", "()"};
                        }
                        auto return_type = resolve_type(dbg, get_type_die(dbg, die));
                        std::vector<std::string> params;
                        walk_die_list(dbg, die.get_child(), [&params] (Dwarf_Debug dbg, const die_object& die) {
                            params.push_back(resolve_type(dbg, die).get_type());
                        });
                        return {return_type.base, join(params, ", ") + return_type.extent};
                    }
                case DW_TAG_const_type:
                    return resolve_type(dbg, get_type_die(dbg, die), "const");
                default:
                    {
                        const char* tag_name = nullptr;
                        dwarf_get_TAG_name(die.get_tag(), &tag_name);
                        fprintf(stderr, "unknown tag %s\n", tag_name);
                        exit(1);
                    }
            }
            return {"<unknown>", "<unknown>"};
        }

        void retrieve_symbol_for_subprogram(Dwarf_Debug dbg, const die_object& die, Dwarf_Addr pc, Dwarf_Half dwversion, stacktrace_frame& frame) {
            /*Dwarf_Attribute attr;
            int ret = dwarf_attr(die.get(), DW_AT_linkage_name, &attr, nullptr);
            if(ret != DW_DLV_OK) {
                ret = dwarf_attr(die.get(), DW_AT_MIPS_linkage_name, &attr, nullptr);
            }
            if(ret == DW_DLV_OK) {
                char* linkage_name;
                if(dwarf_formstring(attr, &linkage_name, nullptr) == DW_DLV_OK) {
                    frame.symbol = linkage_name;
                    if(dump_dwarf) {
                        fprintf(stderr, "name: %s\n", linkage_name);
                    }
                    dwarf_dealloc(dbg, linkage_name, DW_DLA_STRING);
                }
                dwarf_dealloc(dbg, attr, DW_DLA_ATTR);
            }*/
            assert(die.get_tag() == DW_TAG_subprogram);
            std::string name = die.get_name();
            std::vector<std::string> params;
            auto child = die.get_child();
            if(child) {
                walk_die_list_recursive(
                    dbg,
                    child,
                    [pc, dwversion, &frame, &params] (Dwarf_Debug dbg, const die_object& die) {
                        if(die.get_tag() == DW_TAG_formal_parameter) {
                            // TODO: Ignore DW_AT_artificial
                            params.push_back(resolve_type(dbg, get_type_die(dbg, die)).get_type());
                        }
                    }
                );
            } else {
                fprintf(stderr, "no child %s\n", name.c_str());
            }
            frame.symbol = name + "(" + join(params, ", ") + ")";
        }

        void retrieve_symbol(Dwarf_Debug dbg, const die_object& die, Dwarf_Addr pc, Dwarf_Half dwversion, stacktrace_frame& frame) {
            walk_die_list(
                dbg,
                die,
                [pc, dwversion, &frame] (Dwarf_Debug dbg, const die_object& die) {
                    int ret;
                    if(dump_dwarf) {
                        const char* tag_name;
                        dwarf_get_TAG_name(die.get_tag(), &tag_name);
                        fprintf(
                            stderr,
                            "-------------> %d %s %s\n",
                            dwversion,
                            tag_name,
                            die.get_name().c_str()
                        );
                    }

                    if(!pc_in_die(dbg, die.get(), dwversion, pc)) {
                        if(dump_dwarf) {
                            fprintf(stderr, "pc not in die\n");
                        }
                    } else {
                        if(dump_dwarf) {
                            fprintf(stderr, "pc in die <-----------------------------------\n");
                        }
                        if(die.get_tag() == DW_TAG_subprogram) {
                            retrieve_symbol_for_subprogram(dbg, die, pc, dwversion, frame);
                        }
                        auto child = die.get_child();
                        if(child) {
                            retrieve_symbol(dbg, child, pc, dwversion, frame);
                        } else {
                            if(dump_dwarf) {
                                fprintf(stderr, "(no child)\n");
                            }
                        }
                    }
                }
            );
        }

        void retrieve_line_info(Dwarf_Debug dbg, const die_object& die, Dwarf_Addr pc, Dwarf_Half dwversion, stacktrace_frame& frame) {
            Dwarf_Unsigned version;
            Dwarf_Small table_count;
            Dwarf_Line_Context ctxt;
            Dwarf_Bool is_found = false;
            (void)dwversion;
            int ret = dwarf_srclines_b(
                die.get(),
                &version,
                &table_count,
                &ctxt,
                nullptr
            );
            if(ret == DW_DLV_NO_ENTRY) {
                fprintf(stderr, "dwarf_srclines_b error\n");
                return;
            }
            if(table_count == 1) {
                Dwarf_Line *linebuf = 0;
                Dwarf_Signed linecount = 0;
                Dwarf_Addr prev_lineaddr = 0;

                dwarf_srclines_from_linecontext(ctxt, &linebuf,
                    &linecount, nullptr);
                Dwarf_Line prev_line = 0;
                for(int i = 0; i < linecount; i++) {
                    Dwarf_Line line = linebuf[i];
                    Dwarf_Addr lineaddr = 0;

                    dwarf_lineaddr(line, &lineaddr, nullptr);
                    if(pc == lineaddr) {
                        /* Print the last line entry containing current pc. */
                        Dwarf_Line last_pc_line = line;

                        for(int j = i + 1; j < linecount; j++) {
                            Dwarf_Line j_line = linebuf[j];
                            dwarf_lineaddr(j_line, &lineaddr, nullptr);

                            if(pc == lineaddr) {
                                last_pc_line = j_line;
                            }
                        }
                        is_found = true;
                        print_line(dbg, last_pc_line, pc, frame);
                        break;
                    } else if(prev_line && pc > prev_lineaddr &&
                        pc < lineaddr) {
                        is_found = true;
                        print_line(dbg, prev_line, pc, frame);
                        break;
                    }
                    Dwarf_Bool is_lne;
                    dwarf_lineendsequence(line, &is_lne, nullptr);
                    if(is_lne) {
                        prev_line = 0;
                    } else {
                        prev_lineaddr = lineaddr;
                        prev_line = line;
                    }
                }
            }
            dwarf_srclines_dealloc_b(ctxt);
        }

        void walk_compilation_units(Dwarf_Debug dbg, Dwarf_Addr pc, stacktrace_frame& frame) {
            // 0 passed as the dieto the first call of dwarf_siblingof_b immediately after dwarf_next_cu_header_d to
            // fetch the cu die
            die_object cu_die(dbg, nullptr);
            cu_die = cu_die.get_sibling();
            if(!cu_die) {
                if(dump_dwarf) {
                    fprintf(stderr, "End walk_compilation_units\n");
                }
                return;
            }
            walk_die_list(
                dbg,
                cu_die,
                [&frame, pc] (Dwarf_Debug dbg, const die_object& cu_die) {
                    Dwarf_Half offset_size = 0;
                    Dwarf_Half dwversion = 0;
                    dwarf_get_version_of_die(cu_die.get(), &dwversion, &offset_size);
                    /*auto child = cu_die.get_child();
                    if(child) {
                        walk_die_list_recursive(
                            dbg,
                            child,
                            [&frame, pc, dwversion] (Dwarf_Debug dbg, const die_object& cu_die) {

                            }
                        );
                    }*/
                    //walk_die(dbg, cu_die, pc, dwversion, false, frame);
                    if(pc_in_die(dbg, cu_die.get(), dwversion, pc)) {
                        retrieve_line_info(dbg, cu_die, pc, dwversion, frame);
                        retrieve_symbol(dbg, cu_die, pc, dwversion, frame);
                    }
                }
            );
        }

        void walk_dbg(Dwarf_Debug dbg, Dwarf_Addr pc, stacktrace_frame& frame) {
            // libdwarf keeps track of where it is in the file, dwarf_next_cu_header_d is statefull
            Dwarf_Unsigned next_cu_header;
            Dwarf_Half header_cu_type;
            while(true) {
                int ret = dwarf_next_cu_header_d(
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
                    &header_cu_type,
                    nullptr
                );
                if(ret == DW_DLV_NO_ENTRY) {
                    if(dump_dwarf) {
                        fprintf(stderr, "End walk_dbg\n");
                    }
                    return;
                }
                if(ret != DW_DLV_OK) {
                    fprintf(stderr, "Error\n");
                    return;
                }
                walk_compilation_units(dbg, pc, frame);
            }
        }

        void lookup_pc2(
            const char* object,
            Dwarf_Addr pc,
            stacktrace_frame& frame
        ) {
            if(dump_dwarf) {
                fprintf(stderr, "%s\n", object);
                fprintf(stderr, "%llx\n", pc);
            }
            Dwarf_Debug dbg;
            Dwarf_Ptr errarg = 0;
            auto ret = dwarf_init_path(object, nullptr, 0,
                    DW_GROUPNUMBER_ANY, err_handler, errarg, &dbg, nullptr);
            if(ret == DW_DLV_NO_ENTRY) {
                // fail, no debug info
            } else if(ret != DW_DLV_OK) {
                fprintf(stderr, "Error\n");
            } else {
                walk_dbg(dbg, pc, frame);
            }
            dwarf_finish(dbg);
        }

        struct symbolizer::impl {
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            stacktrace_frame resolve_frame(const dlframe& frame_info) {
                stacktrace_frame frame{};
                frame.filename = frame_info.obj_path;
                frame.symbol = frame_info.symbol;
                frame.address = frame_info.raw_address;
                lookup_pc2(
                    frame_info.obj_path.c_str(),
                    frame_info.obj_address,
                    frame
                );
                return frame;
            }
        };

        // NOLINTNEXTLINE(bugprone-unhandled-exception-at-new)
        symbolizer::symbolizer() : pimpl{new impl} {}
        symbolizer::~symbolizer() = default;

        //stacktrace_frame symbolizer::resolve_frame(void* addr) {
        //    return pimpl->resolve_frame(addr);
        //}

        std::vector<stacktrace_frame> symbolizer::resolve_frames(const std::vector<void*>& frames) {
            std::vector<stacktrace_frame> trace;
            trace.reserve(frames.size());
            for(const auto& frame : get_frames_object_info(frames)) {
                trace.push_back(pimpl->resolve_frame(frame));
            }
            return trace;
        }
    }
}

#endif
