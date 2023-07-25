#ifndef MACHO_HPP
#define MACHO_HPP

#include "common.hpp"

#if IS_APPLE
#include <cstdio>
#include <cstring>
#include <type_traits>

#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>

// Based on https://github.com/AlexDenisov/segment_dumper/blob/master/main.c
// and https://lowlevelbits.org/parsing-mach-o-files/

static bool is_magic_64(uint32_t magic) {
    return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

static bool should_swap_bytes(uint32_t magic) {
    return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
}

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

static uintptr_t macho_get_text_vmaddr_from_segments(FILE* obj_file, off_t offset, bool should_swap, uint32_t ncmds) {
    off_t actual_offset = offset;
    for(uint32_t i = 0; i < ncmds; i++) {
        load_command cmd = load_bytes<load_command>(obj_file, actual_offset);
        if(should_swap) {
            swap_load_command(&cmd, NX_UnknownByteOrder);
        }
        if(cmd.cmd == LC_SEGMENT_64) {
            segment_command_64 segment = load_bytes<segment_command_64>(obj_file, actual_offset);
            if(should_swap) {
                swap_segment_command_64(&segment, NX_UnknownByteOrder);
            }
            //printf("segname(64): %s\n", segment.segname);
            //printf("             %d\n", segment.nsects);
            //printf("             %p\n", segment.vmaddr);
            //printf("             %p\n", segment.vmsize);
            if(strcmp(segment.segname, "__TEXT") == 0) {
                return segment.vmaddr;
            }
        } else if(cmd.cmd == LC_SEGMENT) {
            segment_command segment = load_bytes<segment_command>(obj_file, actual_offset);
            if(should_swap) {
                swap_segment_command(&segment, NX_UnknownByteOrder);
            }
            //printf("segname: %s\n", segment.segname);
            if(strcmp(segment.segname, "__TEXT") == 0) {
                return segment.vmaddr;
            }
        }
        actual_offset += cmd.cmdsize;
    }
    // somehow no __TEXT section was found...
    return 0;
}

static uintptr_t macho_get_text_vmaddr_mach(FILE* obj_file, off_t offset, bool is_64, bool should_swap) {
    uint32_t ncmds;
    off_t load_commands_offset = offset;
    if(is_64) {
        size_t header_size = sizeof(mach_header_64);
        mach_header_64 header = load_bytes<mach_header_64>(obj_file, offset);
        //if(offset != 0) { // if fat the offset will be non-zero, if not fat the offset will be zero
            if(header.cputype != CURRENT_CPU) {
                return 0;
            }
        //}
        if(should_swap) {
            swap_mach_header_64(&header, NX_UnknownByteOrder);
        }
        ncmds = header.ncmds;
        load_commands_offset += header_size;
    } else {
        size_t header_size = sizeof(mach_header);
        mach_header header = load_bytes<mach_header>(obj_file, offset);
        //if(offset != 0) { // if fat the offset will be non-zero, if not fat the offset will be zero
            if(header.cputype != CURRENT_CPU) {
                return 0;
            }
        //}
        if(should_swap) {
            swap_mach_header(&header, NX_UnknownByteOrder);
        }
        ncmds = header.ncmds;
        load_commands_offset += header_size;
    }
    return macho_get_text_vmaddr_from_segments(obj_file, load_commands_offset, should_swap, ncmds);
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
        text_vmaddr = macho_get_text_vmaddr_mach(
            obj_file,
            mach_header_offset,
            is_magic_64(magic),
            should_swap_bytes(magic)
        );
        if(text_vmaddr != 0) {
            return text_vmaddr;
        }
    }
    // If this is reached... something went wrong. The cpu we're on wasn't found.
    return text_vmaddr;
}

static uintptr_t macho_get_text_vmaddr(const char* path) {
    FILE* obj_file = fopen(path, "rb");
    uint32_t magic = load_bytes<uint32_t>(obj_file, 0);
    bool is_64 = is_magic_64(magic);
    bool should_swap = should_swap_bytes(magic);
    uintptr_t addr;
    if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
        addr = macho_get_text_vmaddr_fat(obj_file, should_swap);
    } else {
        addr = macho_get_text_vmaddr_mach(obj_file, 0, is_64, should_swap);
    }
    fclose(obj_file);
    return addr;
}

#endif

#endif
