#ifndef CPPTRACE_MACHO_HPP
#define CPPTRACE_MACHO_HPP

#if IS_APPLE
#include <type_traits>
#include <cstdio>

#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>

// Based on https://github.com/AlexDenisov/segment_dumper/blob/master/main.c
// and https://lowlevelbits.org/parsing-mach-o-files/

static uint32_t read_magic(FILE* obj_file, off_t offset) {
    uint32_t magic;
    fseek(obj_file, offset, SEEK_SET);
    fread(&magic, sizeof(uint32_t), 1, obj_file);
    return magic;
}

template<typename T>
T load_bytes(FILE* obj_file, off_t offset, size_t size) {
    static_assert(std::is_pod<T>::value, "Expected POD type");
    T object;
    fseek(obj_file, offset, SEEK_SET);
    fread(&object, size, 1, obj_file);
    return object;
}

static bool is_magic_64(uint32_t magic) {
    return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

static bool should_swap_bytes(uint32_t magic) {
    return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
}

static void dump_segment_commands(FILE* obj_file, off_t offset, bool should_swap, uint32_t ncmds) {
    off_t actual_offset = offset;
    for(uint32_t i = 0; i < ncmds; i++) {
        load_command cmd = load_bytes<load_command>(obj_file, actual_offset, sizeof(load_command));
        if(should_swap) {
            swap_load_command(&cmd, NX_UnknownByteOrder);
        }
        if(cmd.cmd == LC_SEGMENT_64) {
            segment_command_64 segment = load_bytes<segment_command_64>(obj_file, actual_offset, sizeof(segment_command_64));
            if(should_swap) {
                swap_segment_command_64(&segment, NX_UnknownByteOrder);
            }
            printf("segname(64): %s\n", segment.segname);
        } else if(cmd.cmd == LC_SEGMENT) {
            segment_command segment = load_bytes<segment_command>(obj_file, actual_offset, sizeof(segment_command));
            if(should_swap) {
                swap_segment_command(&segment, NX_UnknownByteOrder);
            }
            printf("segname: %s\n", segment.segname);
        }
        actual_offset += cmd.cmdsize;
    }
}

static void dump_mach_header(FILE* obj_file, off_t offset, bool is_64, bool should_swap) {
    uint32_t ncmds;
    off_t load_commands_offset = offset;
    if(is_64) {
        size_t header_size = sizeof(mach_header_64);
        mach_header_64 header = load_bytes<mach_header_64>(obj_file, offset, header_size);
        if(should_swap) {
            swap_mach_header_64(&header, NX_UnknownByteOrder);
        }
        ncmds = header.ncmds;
        load_commands_offset += header_size;
    } else {
        size_t header_size = sizeof(mach_header);
        mach_header header = load_bytes<mach_header>(obj_file, offset, header_size);
        if(should_swap) {
            swap_mach_header(&header, NX_UnknownByteOrder);
        }
        ncmds = header.ncmds;
        load_commands_offset += header_size;
    }
    dump_segment_commands(obj_file, load_commands_offset, should_swap, ncmds);
}

static void dump_fat_header(FILE* obj_file, bool should_swap) {
    size_t header_size = sizeof(fat_header);
    size_t arch_size = sizeof(fat_arch);
    fat_header header = load_bytes<fat_header>(obj_file, 0, header_size);
    if(should_swap) {
        swap_fat_header(&header, NX_UnknownByteOrder);
    }
    off_t arch_offset = (off_t)header_size;
    for(uint32_t i = 0; i < header.nfat_arch; i++) {
        fat_arch arch = load_bytes<fat_arch>(obj_file, arch_offset, arch_size);
        if(should_swap) {
            swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
        }
        off_t mach_header_offset = (off_t)arch.offset;
        arch_offset += arch_size;
        uint32_t magic = read_magic(obj_file, mach_header_offset);
        dump_mach_header(
            obj_file,
            mach_header_offset,
            is_magic_64(magic),
            should_swap_bytes(magic)
        );
    }
}

static void get_text_vmaddr(const char* path) {
    FILE* obj_file = fopen(path, "rb");
    uint32_t magic = read_magic(obj_file, 0);
    bool is_64 = is_magic_64(magic);
    bool should_swap = should_swap_bytes(magic);
    if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
        dump_fat_header(obj_file, should_swap);
    } else {
        dump_mach_header(obj_file, 0, is_64, should_swap);
    }
    fclose(obj_file);
}

#endif

#endif
