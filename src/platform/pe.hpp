#ifndef PE_HPP
#define PE_HPP

#include "common.hpp"
#include "error.hpp"
#include "utils.hpp"

#if IS_WINDOWS
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace cpptrace {
namespace detail {
    template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    T pe_byteswap_if_needed(T value) {
        // PE header values are little endian, I think dos e_lfanew should be too
        if(!is_little_endian()) {
            return byteswap(value);
        } else {
            return value;
        }
    }

    inline uintptr_t pe_get_module_image_base(const std::string& obj_path) {
        // https://drive.google.com/file/d/0B3_wGJkuWLytbnIxY1J5WUs4MEk/view?pli=1&resourcekey=0-n5zZ2UW39xVTH8ZSu6C2aQ
        // https://0xrick.github.io/win-internals/pe3/
        // Endianness should always be little for dos and pe headers
        FILE* file_ptr;
        errno_t ret = fopen_s(&file_ptr, obj_path.c_str(), "rb");
        auto file = raii_wrap(std::move(file_ptr), file_deleter);
        if(ret != 0 || file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        auto magic = load_bytes<std::array<char, 2>>(file, 0);
        VERIFY(memcmp(magic.data(), "MZ", 2) == 0, "File is not a PE file " + obj_path);
        DWORD e_lfanew = pe_byteswap_if_needed(load_bytes<DWORD>(file, 0x3c)); // dos header + 0x3c
        DWORD nt_header_offset = e_lfanew;
        auto signature = load_bytes<std::array<char, 4>>(file, nt_header_offset); // nt header + 0
        VERIFY(memcmp(signature.data(), "PE\0\0", 4) == 0, "File is not a PE file " + obj_path);
        WORD size_of_optional_header = pe_byteswap_if_needed(
            load_bytes<WORD>(file, nt_header_offset + 4 + 0x10) // file header + 0x10
        );
        VERIFY(size_of_optional_header != 0);
        WORD optional_header_magic = pe_byteswap_if_needed(
            load_bytes<WORD>(file, nt_header_offset + 0x18) // optional header + 0x0
        );
        VERIFY(
            optional_header_magic == IMAGE_NT_OPTIONAL_HDR_MAGIC,
            "PE file does not match expected bit-mode " + obj_path
        );
        // finally get image base
        if(optional_header_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            // 32 bit
            return to<uintptr_t>(
                pe_byteswap_if_needed(
                    load_bytes<DWORD>(file, nt_header_offset + 0x18 + 0x1c) // optional header + 0x1c
                )
            );
        } else {
            // 64 bit
            // I get an "error: 'QWORD' was not declared in this scope" for some reason when using QWORD
            return to<uintptr_t>(
                pe_byteswap_if_needed(
                    load_bytes<unsigned __int64>(file, nt_header_offset + 0x18 + 0x18) // optional header + 0x18
                )
            );
        }
    }
}
}

#endif

#endif
