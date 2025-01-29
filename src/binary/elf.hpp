#ifndef ELF_HPP
#define ELF_HPP

#include "utils/common.hpp"
#include "utils/utils.hpp"

#if IS_LINUX

#include <cstdint>
#include <string>
#include <unordered_map>

namespace cpptrace {
namespace detail {
    class elf {
        file_wrapper file;
        std::string object_path;
        bool is_little_endian;
        bool is_64;

        struct header_info {
            uint64_t e_phoff;
            uint32_t e_phnum;
            uint32_t e_phentsize;
            uint64_t e_shoff;
            uint32_t e_shnum;
            uint32_t e_shentsize;
        };
        bool tried_to_load_header = false;
        optional<header_info> header;

        struct section_info {
            uint32_t sh_type;
            uint64_t sh_addr;
            uint64_t sh_offset;
            uint64_t sh_size;
            uint64_t sh_entsize;
            uint32_t sh_link;
        };
        bool tried_to_load_sections = false;
        bool did_load_sections = false;
        std::vector<section_info> sections;

        struct strtab_entry {
            bool tried_to_load_strtab = false;
            bool did_load_strtab = false;
            std::vector<char> data;
        };
        std::unordered_map<std::size_t, strtab_entry> strtab_entries;

        struct symtab_entry {
            uint32_t st_name;
            unsigned char st_info;
            unsigned char st_other;
            uint16_t st_shndx;
            uint64_t st_value;
            uint64_t st_size;
        };
        struct symtab_info {
            std::vector<symtab_entry> entries;
            std::size_t strtab_link = 0;
        };
        bool tried_to_load_symtab = false;
        bool did_load_symtab = false;
        optional<symtab_info> symtab;

        elf(file_wrapper file, const std::string& object_path, bool is_little_endian, bool is_64);

    public:
        static NODISCARD Result<elf, internal_error> open_elf(const std::string& object_path);

    public:
        Result<std::uintptr_t, internal_error> get_module_image_base();
    private:
        template<std::size_t Bits>
        Result<std::uintptr_t, internal_error> get_module_image_base_impl();

    public:
        std::string lookup_symbol(frame_ptr pc);

    private:
        template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        T byteswap_if_needed(T value);

        Result<const header_info&, internal_error> get_header_info();
        template<std::size_t Bits>
        Result<const header_info&, internal_error> get_header_info_impl();

        Result<const std::vector<section_info>&, internal_error> get_sections();
        template<std::size_t Bits>
        Result<const std::vector<section_info>&, internal_error> get_sections_impl();

        Result<const std::vector<char>&, internal_error> get_strtab(std::size_t index);

        Result<const optional<symtab_info>&, internal_error> get_symtab();
        template<std::size_t Bits>
        Result<const optional<symtab_info>&, internal_error> get_symtab_impl();
    };
}
}

#endif

#endif
