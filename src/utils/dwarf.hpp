#ifndef DWARF_HPP
#define DWARF_HPP

#include <cpptrace/cpptrace.hpp>
#include "../utils/error.hpp"
#include "../utils/utils.hpp"

#include <functional>
#include <stdexcept>
#include <type_traits>

#ifdef CPPTRACE_USE_NESTED_LIBDWARF_HEADER_PATH
 #include <libdwarf/libdwarf.h>
 #include <libdwarf/dwarf.h>
#else
 #include <libdwarf.h>
 #include <dwarf.h>
#endif

#define CONCAT_IMPL( x, y ) x##y
#define CONCAT( x, y ) CONCAT_IMPL( x, y )

namespace cpptrace {
namespace detail {
namespace libdwarf {
    static_assert(std::is_pointer<Dwarf_Die>::value, "Dwarf_Die not a pointer");
    static_assert(std::is_pointer<Dwarf_Debug>::value, "Dwarf_Debug not a pointer");

    static NODISCARD ErrorResult<internal_error> handle_dwarf_error(Dwarf_Debug, Dwarf_Error error) {
        Dwarf_Unsigned ev = dwarf_errno(error);
        char* msg = dwarf_errmsg(error);
        return {internal_error("Cpptrace dwarf error {} {}", ev, msg)};
    }

    #define CHECK_OK_IMPL(res_var, expr) \
        Result<int, internal_error> res_var = (expr); \
        if(!res_var) { \
            return std::move(res_var).unwrap_error(); \
        } else if(res_var.unwrap_value() != DW_DLV_OK) { \
            return Error( \
                internal_error( \
                    "dwarf error: {} didn't evaluate to DW_DLV_OK, instead got {}", \
                    #expr, \
                    res_var.unwrap_value() \
                ) \
            ); \
        }

    // Check if the expression is an error or not DW_DLV_OK and error if either
    #define CHECK_OK(expr) CHECK_OK_IMPL(CONCAT(res, __COUNTER__), (expr))

    #define PROP_ASSIGN_IMPL(var, res_var, expr) \
        auto res_var = (expr); \
        if(!res_var) { \
            return std::move(res_var).unwrap_error(); \
        } \
        auto var = res_var.unwrap_value();

    // If the expression is an error, return the error. Otherwise assign the value to the variable.
    #define PROP_ASSIGN(var, expr) PROP_ASSIGN_IMPL(var, CONCAT(res, __COUNTER__), (expr))

    #define PROP_IMPL(res_var, expr) \
        auto res_var = (expr); \
        if(!res_var) { \
            return std::move(res_var).unwrap_error(); \
        }

    // If the expression is an error, return the error. Otherwise assign the value to the variable.
    #define PROP(expr) PROP_IMPL(CONCAT(res, __COUNTER__), (expr))

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
        Result<int, internal_error> wrap(int (*f)(Args...), Args2&&... args) const {
            Dwarf_Error error = nullptr;
            int ret = f(std::forward<Args2>(args)..., &error);
            if(ret == DW_DLV_ERROR) {
                return handle_dwarf_error(dbg, error);
            }
            return Ok(ret);
        }

    public:
        die_object(Dwarf_Debug dbg, Dwarf_Die die) : dbg(dbg), die(die) {
            ASSERT(dbg != nullptr);
        }

    // public:
        // Result<die_object, internal_error> create(Dwarf_Debug dbg, Dwarf_Die die) {
        //     if(die == nullptr) {
        //         return internal_error("Error creating dwarf die object: Nullptr");
        //     } else {
        //         return die_object(dbg, die);
        //     }
        // }

        ~die_object() {
            if(die) {
                dwarf_dealloc_die(die);
            }
        }

        die_object(const die_object&) = delete;

        die_object& operator=(const die_object&) = delete;

        die_object(die_object&& other) noexcept : dbg(other.dbg), die(other.die) {
            // done for finding mistakes, attempts to use the die_object after this should segfault
            // a valid use otherwise would be moved_from.get_sibling() which would get the next CU
            other.dbg = nullptr;
            other.die = nullptr;
        }

        die_object& operator=(die_object&& other) noexcept {
            std::swap(dbg, other.dbg);
            std::swap(die, other.die);
            return *this;
        }

        Result<die_object, internal_error> clone() const {
            auto global_offset = get_global_offset();
            if(!global_offset) {
                return global_offset.unwrap_error();
            }
            Dwarf_Bool is_info = dwarf_get_die_infotypes_flag(die);
            Dwarf_Die die_copy = nullptr;
            CHECK_OK(wrap(dwarf_offdie_b, dbg, global_offset.unwrap_value(), is_info, &die_copy));
            return die_object(dbg, die_copy);
        }

        Result<die_object, internal_error> get_child() const {
            Dwarf_Die child = nullptr;
            PROP_ASSIGN(ret, wrap(dwarf_child, die, &child));
            if(ret == DW_DLV_OK) {
                return die_object(dbg, child);
            } else if(ret == DW_DLV_NO_ENTRY) {
                return die_object(dbg, nullptr);
            } else {
                return internal_error("dwarf error: Unexpected return from dwarf_child {}", ret);
            }
        }

        Result<die_object, internal_error> get_sibling() const {
            Dwarf_Die sibling = nullptr;
            PROP_ASSIGN(ret, wrap(dwarf_siblingof_b, dbg, die, true, &sibling));
            if(ret == DW_DLV_OK) {
                return die_object(dbg, sibling);
            } else if(ret == DW_DLV_NO_ENTRY) {
                return die_object(dbg, nullptr);
            } else {
                return internal_error("dwarf error: Unexpected return from dwarf_siblingof_b {}", ret);
            }
        }

        explicit operator bool() const {
            return die != nullptr;
        }

        Dwarf_Die get() const {
            return die;
        }

        Result<std::string, internal_error> get_name() const {
            char empty[] = "";
            char* name = empty;
            PROP_ASSIGN(ret, wrap(dwarf_diename, die, &name));
            auto wrapper = raii_wrap(name, [this] (char* str) { dwarf_dealloc(dbg, str, DW_DLA_STRING); });
            std::string str;
            if(ret != DW_DLV_NO_ENTRY) {
                str = name;
            }
            return std::string(name);
        }

        Result<optional<std::string>, internal_error> get_string_attribute(Dwarf_Half attr_num) const {
            Dwarf_Attribute attr;
            auto ret = wrap(dwarf_attr, die, attr_num, &attr);
            if(ret.is_error()) {
                ret.drop_error();
            } else if(ret.unwrap_value() == DW_DLV_OK) {
                auto attwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
                char* raw_str;
                CHECK_OK(wrap(dwarf_formstring, attr, &raw_str));
                auto strwrapper = raii_wrap(raw_str, [this] (char* str) { dwarf_dealloc(dbg, str, DW_DLA_STRING); });
                std::string str = raw_str;
                return Ok(str);
            }
            return Ok(nullopt);
        }

        Result<optional<Dwarf_Unsigned>, internal_error> get_unsigned_attribute(Dwarf_Half attr_num) const {
            Dwarf_Attribute attr;
            auto ret = wrap(dwarf_attr, die, attr_num, &attr);
            if (ret.is_error()) {
                ret.drop_error();
            } else if(ret.unwrap_value() == DW_DLV_OK) {
                auto attwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
                Dwarf_Unsigned val;
                CHECK_OK(wrap(dwarf_formudata, attr, &val));
                return Ok(val);
            }
            return Ok(nullopt);
        }

        Result<bool, internal_error> has_attr(Dwarf_Half attr_num) const {
            Dwarf_Bool present = false;
            CHECK_OK(wrap(dwarf_hasattr, die, attr_num, &present));
            return present;
        }

        Result<Dwarf_Half, internal_error> get_tag() const {
            Dwarf_Half tag = 0;
            CHECK_OK(wrap(dwarf_tag, die, &tag));
            return tag;
        }

        const char* get_tag_name() const {
            const char* tag_name;
            auto tag = get_tag();
            if(!tag) {
                tag.drop_error();
            } else if(dwarf_get_TAG_name(tag.unwrap_value(), &tag_name) == DW_DLV_OK) {
                return tag_name;
            }
            return "<unknown tag name>";
        }

        Result<Dwarf_Off, internal_error> get_global_offset() const {
            Dwarf_Off off;
            CHECK_OK(wrap(dwarf_dieoffset, die, &off));
            return off;
        }

        Result<die_object, internal_error> resolve_reference_attribute(Dwarf_Half attr_num) const {
            Dwarf_Attribute attr;
            CHECK_OK(dwarf_attr(die, attr_num, &attr, nullptr));
            auto wrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            Dwarf_Half form = 0;
            CHECK_OK(wrap(dwarf_whatform, attr, &form));
            switch(form) {
                case DW_FORM_ref1:
                case DW_FORM_ref2:
                case DW_FORM_ref4:
                case DW_FORM_ref8:
                case DW_FORM_ref_udata:
                    {
                        Dwarf_Off off = 0;
                        Dwarf_Bool is_info = dwarf_get_die_infotypes_flag(die);
                        CHECK_OK(wrap(dwarf_formref, attr, &off, &is_info));
                        Dwarf_Off global_offset = 0;
                        CHECK_OK(wrap(dwarf_convert_to_global_offset, attr, off, &global_offset));
                        Dwarf_Die target = nullptr;
                        CHECK_OK(wrap(dwarf_offdie_b, dbg, global_offset, is_info, &target));
                        return die_object(dbg, target);
                    }
                case DW_FORM_ref_addr:
                    {
                        Dwarf_Off off;
                        CHECK_OK(wrap(dwarf_global_formref, attr, &off));
                        int is_info = dwarf_get_die_infotypes_flag(die);
                        Dwarf_Die target = nullptr;
                        CHECK_OK(wrap(dwarf_offdie_b, dbg, off, is_info, &target));
                        return die_object(dbg, target);
                    }
                case DW_FORM_ref_sig8:
                    {
                        Dwarf_Sig8 signature;
                        CHECK_OK(wrap(dwarf_formsig8, attr, &signature));
                        Dwarf_Die target = nullptr;
                        Dwarf_Bool targ_is_info = false;
                        CHECK_OK(wrap(dwarf_find_die_given_sig8, dbg, &signature, &target, &targ_is_info));
                        return die_object(dbg, target);
                    }
                default:
                    return internal_error(microfmt::format("unknown form for attribute {} {}", attr_num, form));
            }
        }

        Result<Dwarf_Unsigned, internal_error> get_ranges_offset(Dwarf_Attribute attr) const {
            Dwarf_Unsigned off = 0;
            Dwarf_Half form = 0;
            CHECK_OK(wrap(dwarf_whatform, attr, &form));
            if (form == DW_FORM_rnglistx) {
                CHECK_OK(wrap(dwarf_formudata, attr, &off));
            } else {
                CHECK_OK(wrap(dwarf_global_formref, attr, &off));
            }
            return off;
        }

        template<typename F>
        // callback should return true to keep going
        NODISCARD
        Result<monostate, internal_error> dwarf5_ranges(F callback) const {
            Dwarf_Attribute attr = nullptr;
            auto ranges_res = wrap(dwarf_attr, die, DW_AT_ranges, &attr);
            if(ranges_res.is_error()) {
                return ranges_res.unwrap_error();
            } else if(ranges_res.unwrap_value() != DW_DLV_OK) {
                return internal_error("Unexpected value from dwarf_attr: " + std::to_string(ranges_res.unwrap_value()));
            }
            auto attrwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            PROP_ASSIGN(offset, get_ranges_offset(attr));
            Dwarf_Half form = 0;
            CHECK_OK(wrap(dwarf_whatform, attr, &form));
            // get .debug_rnglists info
            Dwarf_Rnglists_Head head = nullptr;
            Dwarf_Unsigned rnglists_entries = 0;
            Dwarf_Unsigned dw_global_offset_of_rle_set = 0;
            PROP_ASSIGN(
                res,
                wrap(
                    dwarf_rnglists_get_rle_head,
                    attr,
                    form,
                    offset,
                    &head,
                    &rnglists_entries,
                    &dw_global_offset_of_rle_set
                )
            );
            auto headwrapper = raii_wrap(head, [] (Dwarf_Rnglists_Head head) { dwarf_dealloc_rnglists_head(head); });
            if(res == DW_DLV_NO_ENTRY) {
                return monostate{}; // I guess normal
            }
            if(res != DW_DLV_OK) {
                return internal_error("dwarf_rnglists_get_rle_head not ok");
            }
            for(std::size_t i = 0 ; i < rnglists_entries; i++) {
                unsigned entrylen = 0;
                unsigned rle_value_out = 0;
                Dwarf_Unsigned raw1 = 0;
                Dwarf_Unsigned raw2 = 0;
                Dwarf_Bool unavailable = 0;
                Dwarf_Unsigned cooked1 = 0;
                Dwarf_Unsigned cooked2 = 0;
                PROP_ASSIGN(
                    res2,
                    wrap(
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
                    )
                );
                if(res2 == DW_DLV_NO_ENTRY) {
                    continue;
                }
                if(res2 != DW_DLV_OK) {
                    return internal_error("dwarf_get_rnglists_entry_fields_a not ok");
                }
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
                            return monostate{};
                        }
                        break;
                    default:
                        return internal_error("rle_value_out: something is wrong");
                        break;
                }
            }
            return monostate{};
        }

        template<typename F>
        // callback should return true to keep going
        NODISCARD
        Result<monostate, internal_error> dwarf4_ranges(Dwarf_Addr lowpc, F callback) const {
            Dwarf_Attribute attr = nullptr;
            CHECK_OK(wrap(dwarf_attr, die, DW_AT_ranges, &attr));
            auto attrwrapper = raii_wrap(attr, [] (Dwarf_Attribute attr) { dwarf_dealloc_attribute(attr); });
            Dwarf_Unsigned offset;
            CHECK_OK(wrap(dwarf_global_formref, attr, &offset));
            Dwarf_Addr baseaddr = 0;
            if(lowpc != (std::numeric_limits<Dwarf_Addr>::max)()) {
                baseaddr = lowpc;
            }
            Dwarf_Ranges* ranges = nullptr;
            Dwarf_Signed count = 0;
            CHECK_OK(
                wrap(
                    dwarf_get_ranges_b,
                    dbg,
                    offset,
                    die,
                    nullptr,
                    &ranges,
                    &count,
                    nullptr
                )
            );
            auto rangeswrapper = raii_wrap(
                ranges,
                [this, count] (Dwarf_Ranges* ranges) { dwarf_dealloc_ranges(dbg, ranges, count); }
            );
            for(int i = 0; i < count; i++) {
                if(ranges[i].dwr_type == DW_RANGES_ENTRY) {
                    if(!callback(baseaddr + ranges[i].dwr_addr1, baseaddr + ranges[i].dwr_addr2)) {
                        return monostate{};
                    }
                } else if(ranges[i].dwr_type == DW_RANGES_ADDRESS_SELECTION) {
                    baseaddr = ranges[i].dwr_addr2;
                } else {
                    ASSERT(ranges[i].dwr_type == DW_RANGES_END);
                    baseaddr = lowpc;
                }
            }
            return monostate{};
        }

        template<typename F>
        // callback should return true to keep going
        NODISCARD
        Result<monostate, internal_error> dwarf_ranges(int version, optional<Dwarf_Addr> pc, F callback) const {
            Dwarf_Addr lowpc = (std::numeric_limits<Dwarf_Addr>::max)();
            PROP_ASSIGN(res, wrap(dwarf_lowpc, die, &lowpc));
            if(res == DW_DLV_OK) {
                if(pc.has_value() && pc.unwrap() == lowpc) {
                    callback(lowpc, lowpc + 1);
                    return monostate{};
                }
                Dwarf_Addr highpc = 0;
                enum Dwarf_Form_Class return_class;
                PROP_ASSIGN(res2, wrap(dwarf_highpc_b, die, &highpc, nullptr, &return_class));
                if(res2 == DW_DLV_OK) {
                    if(return_class == DW_FORM_CLASS_CONSTANT) {
                        highpc += lowpc;
                    }
                    if(!callback(lowpc, highpc)) {
                        return monostate{};
                    }
                }
            }
            if(version >= 5) {
                return dwarf5_ranges(callback);
            } else {
                return dwarf4_ranges(lowpc, callback);
            }
        }

        struct address_range {
            Dwarf_Addr low;
            Dwarf_Addr high;
        };

        Result<std::vector<address_range>, internal_error> get_rangelist_entries(int version) const {
            std::vector<address_range> vec;
            PROP(
                dwarf_ranges(version, nullopt, [&vec] (Dwarf_Addr low, Dwarf_Addr high) {
                    // Simple coalescing optimization:
                    // Sometimes the range list entries are really continuous: [100, 200), [200, 300)
                    // Other times there's just one byte of separation [300, 399), [400, 500)
                    // Those are the main two cases I've observed.
                    // This will not catch all cases, presumably, as the range lists aren't sorted.
                    //  But compilers/linkers seem to like to emit the ranges in sorted order.
                    if(!vec.empty() && low - vec.back().high <= 1) {
                        vec.back().high = high;
                    } else {
                        vec.push_back({low, high});
                    }
                    return true;
                })
            );
            return vec;
        }

        Result<Dwarf_Bool, internal_error> pc_in_die(int version, Dwarf_Addr pc) const {
            bool found = false;
            PROP(
                dwarf_ranges(version, pc, [&found, pc] (Dwarf_Addr low, Dwarf_Addr high) {
                    if(pc >= low && pc < high) {
                        found = true;
                        return false;
                    }
                    return true;
                })
            );
            return found;
        }

        void print() const {
            std::fprintf(
                stderr,
                "%08llx %s %s\n",
                to_ull(get_global_offset().value_or(0)),
                get_tag_name(),
                get_name().drop_error().value_or("<name error>").c_str()
            );
        }
    };

    // walk die list, callback is called on each die and should return true to
    // continue traversal
    // returns true if traversal should continue
    bool walk_die_list(
        const die_object& die,
        const std::function<bool(const die_object&)>& fn
    ) {
        // TODO: Refactor so there is only one fn call
        bool continue_traversal = true;
        if(fn(die)) {
            auto sibling = die.get_sibling();
            if(!sibling) {
                sibling.drop_error();
            } else {
                die_object current = std::move(sibling).unwrap_value();
                while(current) {
                    if(fn(current)) {
                        auto sibling = current.get_sibling();
                        if(!sibling) {
                            sibling.drop_error();
                        }
                        current = std::move(sibling).unwrap_value();
                    } else {
                        continue_traversal = false;
                        break;
                    }
                }
            }
        }
        return continue_traversal;
    }

    // walk die list, recursing into children, callback is called on each die
    // and should return true to continue traversal
    // returns true if traversal should continue
    bool walk_die_list_recursive(
        const die_object& die,
        const std::function<bool(const die_object&)>& fn
    ) {
        return walk_die_list(
            die,
            [&fn](const die_object& die) {
                auto child = die.get_child();
                if(child.is_error()) {
                    child.drop_error();
                } else if(child.unwrap_value()) {
                    if(!walk_die_list_recursive(child.unwrap_value(), fn)) {
                        return false;
                    }
                }
                return fn(die);
            }
        );
    }
}
}
}

#endif
