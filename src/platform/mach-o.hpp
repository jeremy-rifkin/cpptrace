#ifndef MACHO_HPP
#define MACHO_HPP

#include "common.hpp"
#include "utils.hpp"

#if IS_APPLE

// A number of mach-o functions are deprecated as of macos 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <cstdio>
#include <cstring>
#include <type_traits>

#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>
#include <crt_externs.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>

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

    static bool is_fat_magic(uint32_t magic) {
        return magic == FAT_MAGIC || magic == FAT_CIGAM;
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

    #ifdef __LP64__
     #define LP(x) x##_64
    #else
     #define LP(x) x
    #endif

    template<std::size_t Bits>
    static optional<uintptr_t> macho_get_text_vmaddr_mach(
        FILE* obj_file,
        const std::string& obj_path,
        off_t offset,
        bool should_swap,
        bool allow_arch_mismatch
    ) {
        static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
        using Mach_Header = typename std::conditional<Bits == 32, mach_header, mach_header_64>::type;
        using Segment_Command = typename std::conditional<Bits == 32, segment_command, segment_command_64>::type;
        uint32_t ncmds;
        off_t load_commands_offset = offset;
        size_t header_size = sizeof(Mach_Header);
        Mach_Header header = load_bytes<Mach_Header>(obj_file, offset);
        if(should_swap) {
            swap_mach_header(header);
        }
        thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
        //fprintf(
        //    stderr,
        //    "----> %d %d; %d %d\n",
        //    header.cputype,
        //    mhp->cputype,
        //    static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK),
        //    header.cpusubtype
        //);
        if(
            header.cputype != mhp->cputype ||
            static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK) != header.cpusubtype
        ) {
            if(allow_arch_mismatch) {
                return nullopt;
            } else {
                PANIC("Mach-O file cpu type and subtype do not match current machine " + obj_path);
            }
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
            // TODO: This is a mistake? Need to check cmd.cmd == LC_SEGMENT_64 / cmd.cmd == LC_SEGMENT
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
        PANIC("Couldn't find __TEXT section while parsing Mach-O object");
        return 0;
    }

    static uintptr_t macho_get_text_vmaddr_fat(FILE* obj_file, const std::string& obj_path, bool should_swap) {
        size_t header_size = sizeof(fat_header);
        size_t arch_size = sizeof(fat_arch);
        fat_header header = load_bytes<fat_header>(obj_file, 0);
        if(should_swap) {
            swap_fat_header(&header, NX_UnknownByteOrder);
        }
        off_t arch_offset = (off_t)header_size;
        optional<uintptr_t> text_vmaddr;
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
                    obj_path,
                    mach_header_offset,
                    should_swap_bytes(magic),
                    true
                );
            } else {
                text_vmaddr = macho_get_text_vmaddr_mach<32>(
                    obj_file,
                    obj_path,
                    mach_header_offset,
                    should_swap_bytes(magic),
                    true
                );
            }
            if(text_vmaddr.has_value()) {
                return text_vmaddr.unwrap();
            }
        }
        // If this is reached... something went wrong. The cpu we're on wasn't found.
        PANIC("Couldn't find appropriate architecture in fat Mach-O");
        return 0;
    }

    static uintptr_t macho_get_text_vmaddr(const std::string& obj_path) {
        //fprintf(stderr, "--%s--\n", obj_path.c_str());
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        uint32_t magic = load_bytes<uint32_t>(file, 0);
        VERIFY(is_mach_o(magic), "File is not Mach-O " + obj_path);
        bool is_64 = is_magic_64(magic);
        bool should_swap = should_swap_bytes(magic);
        if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
            return macho_get_text_vmaddr_fat(file, obj_path, should_swap);
        } else {
            if(is_64) {
                return macho_get_text_vmaddr_mach<64>(file, obj_path, 0, should_swap, false).unwrap();
            } else {
                return macho_get_text_vmaddr_mach<32>(file, obj_path, 0, should_swap, false).unwrap();
            }
        }
    }

    inline bool macho_is_fat(const std::string& obj_path) {
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        uint32_t magic = load_bytes<uint32_t>(file, 0);
        return is_fat_magic(magic);
    }

    // returns index of the appropriate mach-o binary in the universal binary
    // TODO: Code duplication with macho_get_text_vmaddr_fat
    inline unsigned get_fat_macho_index(const std::string& obj_path) {
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        uint32_t magic = load_bytes<uint32_t>(file, 0);
        VERIFY(is_fat_magic(magic));
        bool should_swap = should_swap_bytes(magic);
        size_t header_size = sizeof(fat_header);
        size_t arch_size = sizeof(fat_arch);
        fat_header header = load_bytes<fat_header>(file, 0);
        if(should_swap) {
            swap_fat_header(&header, NX_UnknownByteOrder);
        }
        off_t arch_offset = (off_t)header_size;
        thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
        for(uint32_t i = 0; i < header.nfat_arch; i++) {
            fat_arch arch = load_bytes<fat_arch>(file, arch_offset);
            if(should_swap) {
                swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
            }
            arch_offset += arch_size;
            if(
                arch.cputype == mhp->cputype &&
                static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK) == arch.cpusubtype
            ) {
                return i;
            }
        }
        // If this is reached... something went wrong. The cpu we're on wasn't found.
        PANIC("Couldn't find appropriate architecture in fat Mach-O");
    }
}
}

#pragma GCC diagnostic pop

#endif

#endif
