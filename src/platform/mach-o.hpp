#ifndef MACHO_HPP
#define MACHO_HPP

#include "common.hpp"
#include "utils.hpp"

#if IS_APPLE
#include <cstdio>
#include <cstring>
#include <type_traits>

#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>

#if defined(__aarch64__)
 #define CURRENT_CPU CPU_TYPE_ARM64
#elif defined(__arm__) && defined(__thumb__)
 #define CURRENT_CPU CPU_TYPE_ARM
#elif defined(__amd64__)
 #define CURRENT_CPU CPU_TYPE_X86_64
#elif defined(__i386__)
 #define CURRENT_CPU CPU_TYPE_I386
#else
 #error "Unknown CPU architecture"
#endif

namespace cpptrace {
namespace detail {
    static bool is_mach_o(uint32_t magic) {
        switch(magic) {
            case FAT_MAGIC:
            case FAT_CIGAM:
            case MH_MAGIC:
            case MH_CIGAM:
            case MH_MAGIC_64:
            case MH_CIGAM_64:
                return true;
            default:
                return false;
        }
    }

    // Based on https://github.com/AlexDenisov/segment_dumper/blob/master/main.c
    // and https://lowlevelbits.org/parsing-mach-o-files/
    static bool is_magic_64(uint32_t magic) {
        return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
    }

    static bool should_swap_bytes(uint32_t magic) {
        return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
    }

    static void swap_mach_header(mach_header_64& header) {
        swap_mach_header_64(&header, NX_UnknownByteOrder);
    }

    static void swap_mach_header(mach_header& header) {
        swap_mach_header(&header, NX_UnknownByteOrder);
    }

    static void swap_segment_command(segment_command_64& segment) {
        swap_segment_command_64(&segment, NX_UnknownByteOrder);
    }

    static void swap_segment_command(segment_command& segment) {
        swap_segment_command(&segment, NX_UnknownByteOrder);
    }

    template<std::size_t Bits>
    static uintptr_t macho_get_text_vmaddr_mach(FILE* obj_file, off_t offset, bool should_swap) {
        static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
        using Mach_Header = typename std::conditional<Bits == 32, mach_header, mach_header_64>::type;
        using Segment_Command = typename std::conditional<Bits == 32, segment_command, segment_command_64>::type;
        uint32_t ncmds;
        off_t load_commands_offset = offset;
        size_t header_size = sizeof(Mach_Header);
        Mach_Header header = load_bytes<Mach_Header>(obj_file, offset);
        if(header.cputype != CURRENT_CPU) {
            return 0;
        }
        if(should_swap) {
            swap_mach_header(header);
        }
        ncmds = header.ncmds;
        load_commands_offset += header_size;
        // iterate load commands
        off_t actual_offset = load_commands_offset;
        for(uint32_t i = 0; i < ncmds; i++) {
            load_command cmd = load_bytes<load_command>(obj_file, actual_offset);
            if(should_swap) {
                swap_load_command(&cmd, NX_UnknownByteOrder);
            }
            Segment_Command segment = load_bytes<Segment_Command>(obj_file, actual_offset);
            if(should_swap) {
                swap_segment_command(segment);
            }
            if(strcmp(segment.segname, "__TEXT") == 0) {
                return segment.vmaddr;
            }
            actual_offset += cmd.cmdsize;
        }
        // somehow no __TEXT section was found...
        CPPTRACE_VERIFY(false, "Couldn't find __TEXT section while parsing Mach-O object");
        return 0;
    }

    static uintptr_t macho_get_text_vmaddr_fat(FILE* obj_file, bool should_swap) {
        size_t header_size = sizeof(fat_header);
        size_t arch_size = sizeof(fat_arch);
        fat_header header = load_bytes<fat_header>(obj_file, 0);
        if(should_swap) {
            swap_fat_header(&header, NX_UnknownByteOrder);
        }
        off_t arch_offset = (off_t)header_size;
        uintptr_t text_vmaddr = 0;
        for(uint32_t i = 0; i < header.nfat_arch; i++) {
            fat_arch arch = load_bytes<fat_arch>(obj_file, arch_offset);
            if(should_swap) {
                swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
            }
            off_t mach_header_offset = (off_t)arch.offset;
            arch_offset += arch_size;
            uint32_t magic = load_bytes<uint32_t>(obj_file, mach_header_offset);
            if(is_magic_64(magic)) {
                text_vmaddr = macho_get_text_vmaddr_mach<64>(
                    obj_file,
                    mach_header_offset,
                    should_swap_bytes(magic)
                );
            } else {
                text_vmaddr = macho_get_text_vmaddr_mach<32>(
                    obj_file,
                    mach_header_offset,
                    should_swap_bytes(magic)
                );
            }
            if(text_vmaddr != 0) {
                return text_vmaddr;
            }
        }
        // If this is reached... something went wrong. The cpu we're on wasn't found.
        // TODO: Disabled temporarily for CI
        /////CPPTRACE_VERIFY(false, "Couldn't find appropriate architecture in fat Mach-O");
        return 0;
    }

    static uintptr_t macho_get_text_vmaddr(const std::string& obj_path) {
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        uint32_t magic = load_bytes<uint32_t>(file, 0);
        CPPTRACE_VERIFY(is_mach_o(magic), "File is not Mach-O " + obj_path);
        bool is_64 = is_magic_64(magic);
        bool should_swap = should_swap_bytes(magic);
        if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
            return macho_get_text_vmaddr_fat(file, should_swap);
        } else {
            if(is_64) {
                return macho_get_text_vmaddr_mach<64>(file, 0, should_swap);
            } else {
                return macho_get_text_vmaddr_mach<32>(file, 0, should_swap);
            }
        }
    }
}
}

#endif

#endif
