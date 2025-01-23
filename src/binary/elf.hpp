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

        elf(file_wrapper file, const std::string& object_path, bool is_little_endian, bool is_64);

    public:
        static NODISCARD Result<elf, internal_error> open_elf(const std::string& object_path);

        Result<std::uintptr_t, internal_error> get_module_image_base();

    private:
        template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        T byteswap_if_needed(T value, bool elf_is_little);

        template<std::size_t Bits>
        Result<std::uintptr_t, internal_error> get_module_image_base_from_program_table();
    };
}
}

#endif

#endif
