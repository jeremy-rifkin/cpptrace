#ifndef ELF_HPP
#define ELF_HPP

#include "common.hpp"

#if IS_LINUX
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <elf.h>

template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
T elf_byteswap_if_needed(T value, bool elf_is_little) {
    if(is_little_endian() == elf_is_little) {
        return value;
    } else {
        return byteswap(value);
    }
}

// TODO: Address code duplication here. Do we actually have to care about 32-bit if the library is compiled as 64-bit?
// I think probably not...

// TODO: Re-evaluate use of off_t
// I think we can rely on PT_PHDR https://stackoverflow.com/q/61568612/15675011...
static uintptr_t elf_get_module_image_base_from_program_table(
    FILE* file,
    bool is_64,
    bool is_little_endian,
    off_t e_phoff,
    off_t e_phentsize,
    int e_phnum
) {
    for(int i = 0; i < e_phnum; i++) {
        if(is_64) {
            Elf64_Phdr program_header = load_bytes<Elf64_Phdr>(file, e_phoff + e_phentsize * i);
            if(elf_byteswap_if_needed(program_header.p_type, is_little_endian) == PT_PHDR) {
                return elf_byteswap_if_needed(program_header.p_vaddr, is_little_endian)
                        - elf_byteswap_if_needed(program_header.p_offset, is_little_endian);
            }
        } else {
            Elf32_Phdr program_header = load_bytes<Elf32_Phdr>(file, e_phoff + e_phentsize * i);
            if(elf_byteswap_if_needed(program_header.p_type, is_little_endian) == PT_PHDR) {
                return elf_byteswap_if_needed(program_header.p_vaddr, is_little_endian)
                        - elf_byteswap_if_needed(program_header.p_offset, is_little_endian);
            }
        }
    }
    return 0;
}

static uintptr_t elf_get_module_image_base(const std::string& obj_path) {
    FILE* file = fopen(obj_path.c_str(), "rb");
    // Initial checks/metadata
    auto magic = load_bytes<std::array<char, 4>>(file, 0);
    internal_verify(magic == (std::array<char, 4>{0x7F, 'E', 'L', 'F'}));
    bool is_64 = load_bytes<uint8_t>(file, 4) == 2;
    bool is_little_endian = load_bytes<uint8_t>(file, 5) == 1;
    internal_verify(load_bytes<uint8_t>(file, 6) == 1, "Unexpected ELF version");
    //
    if(is_64) {
        Elf64_Ehdr file_header = load_bytes<Elf64_Ehdr>(file, 0);
        internal_verify(file_header.e_ehsize == sizeof(Elf64_Ehdr));
        return elf_get_module_image_base_from_program_table(
            file,
            is_64,
            is_little_endian,
            elf_byteswap_if_needed(file_header.e_phoff, is_little_endian),
            elf_byteswap_if_needed(file_header.e_phentsize, is_little_endian),
            elf_byteswap_if_needed(file_header.e_phnum, is_little_endian)
        );
    } else {
        Elf32_Ehdr file_header = load_bytes<Elf32_Ehdr>(file, 0);
        internal_verify(file_header.e_ehsize == sizeof(Elf32_Ehdr));
        return elf_get_module_image_base_from_program_table(
            file,
            is_64,
            is_little_endian,
            elf_byteswap_if_needed(file_header.e_phoff, is_little_endian),
            elf_byteswap_if_needed(file_header.e_phentsize, is_little_endian),
            elf_byteswap_if_needed(file_header.e_phnum, is_little_endian)
        );
    }
}

#endif

#endif
