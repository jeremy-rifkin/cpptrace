#ifndef PE_HPP
#define PE_HPP

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include "common.hpp"

#if IS_WINDOWS
#include <windows.h>

static uintptr_t pe_get_module_image_base(const std::string& obj_path) {
    // PE header values are little endian
    bool do_swap = !is_little_endian();
    FILE* file = fopen(obj_path.c_str(), "rb");
    char magic[2];
    internal_verify(fread(magic, 1, 2, file) == 2); // file + 0x0
    internal_verify(memcmp(magic, "MZ", 2) == 0);
    DWORD e_lfanew;
    internal_verify(fseek(file, 0x3c, SEEK_SET) == 0);
    internal_verify(fread(&e_lfanew, sizeof(DWORD), 1, file) == 1); // file + 0x3c
    if(do_swap) e_lfanew = byteswap(e_lfanew);
    long nt_header_offset = e_lfanew;
    char signature[4];
    internal_verify(fseek(file, nt_header_offset, SEEK_SET) == 0);
    internal_verify(fread(signature, 1, 4, file) == 4); // NT header + 0x0
    internal_verify(memcmp(signature, "PE\0\0", 4) == 0);
    //WORD machine;
    //internal_verify(fseek(file, nt_header_offset + 4, SEEK_SET) == 0); // file header + 0x0
    //internal_verify(fread(&machine, sizeof(WORD), 1, file) == 1);
    WORD size_of_optional_header;
    internal_verify(fseek(file, nt_header_offset + 4 + 0x10, SEEK_SET) == 0); // file header + 0x10
    internal_verify(fread(&size_of_optional_header, sizeof(DWORD), 1, file) == 1);
    if(do_swap) size_of_optional_header = byteswap(size_of_optional_header);
    internal_verify(size_of_optional_header != 0);
    WORD optional_header_magic;
    internal_verify(fseek(file, nt_header_offset + 0x18, SEEK_SET) == 0); // optional header + 0x0
    internal_verify(fread(&optional_header_magic, sizeof(DWORD), 1, file) == 1);
    if(do_swap) optional_header_magic = byteswap(optional_header_magic);
    internal_verify(optional_header_magic == IMAGE_NT_OPTIONAL_HDR_MAGIC);
    uintptr_t image_base;
    if(optional_header_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        // 32 bit
        DWORD base;
        internal_verify(fseek(file, nt_header_offset + 0x18 + 0x1c, SEEK_SET) == 0); // optional header + 0x1c
        internal_verify(fread(&base, sizeof(DWORD), 1, file) == 1);
        if(do_swap) base = byteswap(base);
        image_base = base;
    } else {
        // 64 bit
        // I get an "error: 'QWORD' was not declared in this scope" for some reason when using QWORD
        unsigned __int64 base;
        internal_verify(fseek(file, nt_header_offset + 0x18 + 0x18, SEEK_SET) == 0); // optional header + 0x18
        internal_verify(fread(&base, sizeof(unsigned __int64), 1, file) == 1);
        if(do_swap) base = byteswap(base);
        image_base = base;
    }
    fclose(file);
    return image_base;
}
#endif

#endif
