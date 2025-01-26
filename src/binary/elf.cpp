#include "binary/elf.hpp"

#if IS_LINUX

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include <elf.h>

namespace cpptrace {
namespace detail {
    elf::elf(
        file_wrapper file,
        const std::string& object_path,
        bool is_little_endian,
        bool is_64
    ) : file(std::move(file)), object_path(object_path), is_little_endian(is_little_endian), is_64(is_64) {}

    Result<elf, internal_error> elf::open_elf(const std::string& object_path) {
        auto file = raii_wrap(std::fopen(object_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            return internal_error("Unable to read object file {}", object_path);
        }
        // Initial checks/metadata
        auto magic = load_bytes<std::array<char, 4>>(file, 0);
        if(magic.is_error()) {
            return std::move(magic).unwrap_error();
        }
        if(magic.unwrap_value() != (std::array<char, 4>{0x7F, 'E', 'L', 'F'})) {
            return internal_error("File is not ELF " + object_path);
        }
        auto ei_class = load_bytes<std::uint8_t>(file, 4);
        if(ei_class.is_error()) {
            return std::move(ei_class).unwrap_error();
        }
        bool is_64 = ei_class.unwrap_value() == 2;
        auto ei_data = load_bytes<std::uint8_t>(file, 5);
        if(ei_data.is_error()) {
            return std::move(ei_data).unwrap_error();
        }
        bool is_little_endian = ei_data.unwrap_value() == 1;
        auto ei_version = load_bytes<std::uint8_t>(file, 6);
        if(ei_version.is_error()) {
            return std::move(ei_version).unwrap_error();
        }
        if(ei_version.unwrap_value() != 1) {
            return internal_error("Unexpected ELF version " + object_path);
        }
        return elf(std::move(file), object_path, is_little_endian, is_64);
    }

    Result<std::uintptr_t, internal_error> elf::get_module_image_base() {
        // get image base
        if(is_64) {
            return get_module_image_base_impl<64>();
        } else {
            return get_module_image_base_impl<32>();
        }
    }

    template<std::size_t Bits>
    Result<std::uintptr_t, internal_error> elf::get_module_image_base_impl() {
        static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
        using PHeader = typename std::conditional<Bits == 32, Elf32_Phdr, Elf64_Phdr>::type;
        auto header = get_header_info();
        if(header.is_error()) {
            return std::move(header).unwrap_error();
        }
        const auto& header_info = header.unwrap_value();
        // PT_PHDR will occur at most once
        // Should be somewhat reliable https://stackoverflow.com/q/61568612/15675011
        // It should occur at the beginning but may as well loop just in case
        for(unsigned i = 0; i < header_info.e_phnum; i++) {
            auto loaded_ph = load_bytes<PHeader>(file, header_info.e_phoff + header_info.e_phentsize * i);
            if(loaded_ph.is_error()) {
                return std::move(loaded_ph).unwrap_error();
            }
            const PHeader& program_header = loaded_ph.unwrap_value();
            if(byteswap_if_needed(program_header.p_type, is_little_endian) == PT_PHDR) {
                return byteswap_if_needed(program_header.p_vaddr, is_little_endian) -
                    byteswap_if_needed(program_header.p_offset, is_little_endian);
            }
        }
        // Apparently some objects like shared objects can end up missing this header. 0 as a base seems correct.
        return 0;
    }

    template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type>
    T elf::byteswap_if_needed(T value, bool elf_is_little) {
        if(cpptrace::detail::is_little_endian() == elf_is_little) {
            return value;
        } else {
            return byteswap(value);
        }
    }

    Result<elf::header_info, internal_error> elf::get_header_info() {
        if(header) {
            return header.unwrap();
        }
        if(is_64) {
            return get_header_info_impl<64>();
        } else {
            return get_header_info_impl<32>();
        }
    }

    template<std::size_t Bits>
    Result<elf::header_info, internal_error> elf::get_header_info_impl() {
        static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
        using Header = typename std::conditional<Bits == 32, Elf32_Ehdr, Elf64_Ehdr>::type;
        auto loaded_header = load_bytes<Header>(file, 0);
        if(loaded_header.is_error()) {
            return std::move(loaded_header).unwrap_error();
        }
        const Header& file_header = loaded_header.unwrap_value();
        if(file_header.e_ehsize != sizeof(Header)) {
            return internal_error("ELF file header size mismatch" + object_path);
        }
        header_info info;
        info.e_phoff = file_header.e_phoff;
        info.e_phnum = file_header.e_phnum;
        info.e_phentsize = file_header.e_phentsize;
        info.e_shoff = file_header.e_shoff;
        info.e_shnum = file_header.e_shnum;
        info.e_shentsize = file_header.e_shentsize;
        header = info;
        return header.unwrap();
    }
}
}

#endif
