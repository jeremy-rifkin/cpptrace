#ifndef ELF_HPP
#define ELF_HPP

#include "common.hpp"
#include "utils.hpp"

#if IS_LINUX
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include <elf.h>

namespace cpptrace {
namespace detail {
    template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    T elf_byteswap_if_needed(T value, bool elf_is_little) {
        if(is_little_endian() == elf_is_little) {
            return value;
        } else {
            return byteswap(value);
        }
    }

    template<std::size_t Bits>
    static uintptr_t elf_get_module_image_base_from_program_table(
        const std::string& obj_path,
        FILE* file,
        bool is_little_endian
    ) {
        static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
        using Header = typename std::conditional<Bits == 32, Elf32_Ehdr, Elf64_Ehdr>::type;
        using PHeader = typename std::conditional<Bits == 32, Elf32_Phdr, Elf64_Phdr>::type;
        Header file_header = load_bytes<Header>(file, 0);
        VERIFY(file_header.e_ehsize == sizeof(Header), "ELF file header size mismatch" + obj_path);
        // PT_PHDR will occur at most once
        // Should be somewhat reliable https://stackoverflow.com/q/61568612/15675011
        // It should occur at the beginning but may as well loop just in case
        for(int i = 0; i < file_header.e_phnum; i++) {
            PHeader program_header = load_bytes<PHeader>(file, file_header.e_phoff + file_header.e_phentsize * i);
            if(elf_byteswap_if_needed(program_header.p_type, is_little_endian) == PT_PHDR) {
                return elf_byteswap_if_needed(program_header.p_vaddr, is_little_endian) -
                       elf_byteswap_if_needed(program_header.p_offset, is_little_endian);
            }
        }
        // Apparently some objects like shared objects can end up missing this file. 0 as a base seems correct.
        return 0;
    }

    static uintptr_t elf_get_module_image_base(const std::string& obj_path) {
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        // Initial checks/metadata
        auto magic = load_bytes<std::array<char, 4>>(file, 0);
        VERIFY(magic == (std::array<char, 4>{0x7F, 'E', 'L', 'F'}), "File is not ELF " + obj_path);
        bool is_64 = load_bytes<uint8_t>(file, 4) == 2;
        bool is_little_endian = load_bytes<uint8_t>(file, 5) == 1;
        VERIFY(load_bytes<uint8_t>(file, 6) == 1, "Unexpected ELF endianness " + obj_path);
        // get image base
        if(is_64) {
            return elf_get_module_image_base_from_program_table<64>(obj_path, file, is_little_endian);
        } else {
            return elf_get_module_image_base_from_program_table<32>(obj_path, file, is_little_endian);
        }
    }
}
}

#endif

#endif
