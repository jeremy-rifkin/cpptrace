#ifdef CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF

#include <cpptrace/cpptrace.hpp>
#include "symbols.hpp"
#include "../platform/program_name.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
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

        void walk_die(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr pc, Dwarf_Half dwversion, bool walk_siblings, stacktrace_frame& frame) {
            int ret;
            Dwarf_Half tag = 0;
            dwarf_tag(die, &tag, nullptr);
            if(dump_dwarf) {
                char* name;
                ret = dwarf_diename(die, &name, nullptr);
                if(ret == DW_DLV_NO_ENTRY) {
                    name = nullptr;
                }
                const char* tag_name;
                dwarf_get_TAG_name(tag, &tag_name);
                fprintf(
                    stderr,
                    "-------------> %d %s %s\n",
                    dwversion,
                    tag_name,
                    name ? name : ""
                );
                dwarf_dealloc(dbg, name, DW_DLA_STRING);
            }

            if(!pc_in_die(dbg, die, dwversion, pc)) {
                if(dump_dwarf) {
                    fprintf(stderr, "pc not in die\n");
                }
            } else {
                if(dump_dwarf) {
                    fprintf(stderr, "pc in die <-----------------------------------\n");
                }
                if(tag == DW_TAG_subprogram) {
                    Dwarf_Attribute attr;
                    ret = dwarf_attr(die, DW_AT_linkage_name, &attr, nullptr);
                    if(ret != DW_DLV_OK) {
                        ret = dwarf_attr(die, DW_AT_MIPS_linkage_name, &attr, nullptr);
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
                    }
                }
                Dwarf_Die child = 0;
                ret = dwarf_child(
                    die,
                    &child,
                    nullptr
                );
                if(ret == DW_DLV_OK) {
                    walk_die(dbg, child, pc, dwversion, true, frame);
                } else {
                    if(dump_dwarf) {
                        fprintf(stderr, "(no child)\n");
                    }
                }
            }

            if(walk_siblings) {
                while(true) {
                    ret = dwarf_siblingof_b(dbg, die, true, &die, nullptr);
                    if(ret == DW_DLV_NO_ENTRY) {
                        if(dump_dwarf) {
                            fprintf(stderr, "End walk_die\n");
                        }
                        return;
                    }
                    if(ret != DW_DLV_OK) {
                        fprintf(stderr, "Error\n");
                        return;
                    }
                    walk_die(dbg, die, pc, dwversion, false, frame);
                }
            }
        }

        void retrieve_line_info(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr pc, Dwarf_Half dwversion, stacktrace_frame& frame) {
            Dwarf_Unsigned version;
            Dwarf_Small table_count;
            Dwarf_Line_Context ctxt;
            Dwarf_Bool is_found = false;
            (void)dwversion;
            int ret = dwarf_srclines_b(
                die,
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
            // 0 passed to the first call of dwarf_siblingof_b immediately after dwarf_next_cu_header_d to fetch the cu
            // die
            Dwarf_Die cu_die = 0;
            while(true) {
                Dwarf_Die new_die = 0;
                int ret = dwarf_siblingof_b(dbg, cu_die, true, &new_die, nullptr);
                dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
                cu_die = new_die;
                if(ret == DW_DLV_NO_ENTRY) {
                    if(dump_dwarf) {
                        fprintf(stderr, "End walk_compilation_units\n");
                    }
                    return;
                }
                if(ret != DW_DLV_OK) {
                    fprintf(stderr, "Error\n");
                    return;
                }
                Dwarf_Half offset_size = 0;
                Dwarf_Half dwversion = 0;
                dwarf_get_version_of_die(cu_die, &dwversion, &offset_size);
                walk_die(dbg, cu_die, pc, dwversion, false, frame);
                if(pc_in_die(dbg, cu_die, dwversion, pc)) {
                    retrieve_line_info(dbg, cu_die, pc, dwversion, frame);
                }
            }
        }

        void walk_dbg(Dwarf_Debug dbg, Dwarf_Addr pc, stacktrace_frame& frame) {
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
