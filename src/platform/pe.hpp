#ifndef PE_HPP
#define PE_HPP

#include "common.hpp"

#if IS_WINDOWS
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
T pe_byteswap_if_needed(T value) {
    // PE header values are little endian
    if(!is_little_endian()) {
        return byteswap(value);
    } else {
        return value;
    }
}

static uintptr_t pe_get_module_image_base(const std::string& obj_path) {
    FILE* file = fopen(obj_path.c_str(), "rb");
    auto magic = load_bytes<std::array<char, 2>>(file, 0);
    internal_verify(memcmp(magic.data(), "MZ", 2) == 0);
    DWORD e_lfanew = pe_byteswap_if_needed(load_bytes<DWORD>(file, 0x3c)); // dos header + 0x3c
    long nt_header_offset = e_lfanew;
    auto signature = load_bytes<std::array<char, 4>>(file, nt_header_offset); // nt header + 0
    internal_verify(memcmp(signature.data(), "PE\0\0", 4) == 0);
    WORD size_of_optional_header = pe_byteswap_if_needed(
        load_bytes<WORD>(file, nt_header_offset + 4 + 0x10) // file header + 0x10
    );
    internal_verify(size_of_optional_header != 0);
    WORD optional_header_magic = pe_byteswap_if_needed(
        load_bytes<WORD>(file, nt_header_offset + 0x18) // optional header + 0x0
    );
    internal_verify(optional_header_magic == IMAGE_NT_OPTIONAL_HDR_MAGIC);
    uintptr_t image_base;
    if(optional_header_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        // 32 bit
        image_base = pe_byteswap_if_needed(
            load_bytes<DWORD>(file, nt_header_offset + 0x18 + 0x1c) // optional header + 0x1c
        );
    } else {
        // 64 bit
        // I get an "error: 'QWORD' was not declared in this scope" for some reason when using QWORD
        image_base = pe_byteswap_if_needed(
            load_bytes<unsigned __int64>(file, nt_header_offset + 0x18 + 0x18) // optional header + 0x18
        );
    }
    fclose(file);
    return image_base;
}
#endif

#endif
