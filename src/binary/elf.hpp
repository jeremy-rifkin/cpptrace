#ifndef ELF_HPP
#define ELF_HPP

#include "utils/common.hpp"
#include "utils/utils.hpp"

#if IS_LINUX

#include <cstdint>
#include <string>

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
        optional<header_info> header;

        elf(file_wrapper file, const std::string& object_path, bool is_little_endian, bool is_64);

    public:
        static NODISCARD Result<elf, internal_error> open_elf(const std::string& object_path);

    public:
        Result<std::uintptr_t, internal_error> get_module_image_base();
    private:
        template<std::size_t Bits>
        Result<std::uintptr_t, internal_error> get_module_image_base_impl();

    private:
        template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        T byteswap_if_needed(T value, bool elf_is_little);

        Result<header_info, internal_error> get_header_info();
        template<std::size_t Bits>
        Result<header_info, internal_error> get_header_info_impl();
    };
}
}

#endif

#endif
