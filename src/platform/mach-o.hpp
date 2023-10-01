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

    struct load_command_entry {
        uint32_t file_offset;
        uint32_t cmd;
        uint32_t cmdsize;
    };

    class mach_o {
        FILE* file = nullptr;
        std::string obj_path;
        uint32_t magic;
        cpu_type_t cputype;
        cpu_subtype_t cpusubtype;
        uint32_t filetype;
        uint32_t n_load_commands;
        uint32_t sizeof_load_commands;
        uint32_t flags;

        size_t load_base = 0;
        size_t fat_index = std::numeric_limits<size_t>::max();

        std::vector<load_command_entry> load_commands;

    public:
        mach_o(const std::string& obj_path) : obj_path(obj_path) {
            file = fopen(obj_path.c_str(), "rb");
            if(file == nullptr) {
                throw file_error("Unable to read object file " + obj_path);
            }
            magic = load_bytes<uint32_t>(file, 0);
            VERIFY(is_mach_o(magic), "File is not Mach-O " + obj_path);
            if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
                load_fat_mach();
            } else {
                fat_index = 0;
                if(is_magic_64(magic)) {
                    load_mach<64>(false);
                } else {
                    load_mach<32>(false);
                }
            }
        }

        ~mach_o() {
            if(file) {
                fclose(file);
            }
        }

        uintptr_t get_text_vmaddr() {
            for(const auto& command : load_commands) {
                if(command.cmd == LC_SEGMENT_64 || command.cmd == LC_SEGMENT) {
                    auto segment = command.cmd == LC_SEGMENT_64
                                        ? load_segment_command<64>(command.file_offset)
                                        : load_segment_command<32>(command.file_offset);
                    if(strcmp(segment.segname, "__TEXT") == 0) {
                        return segment.vmaddr;
                    }
                }
            }
            // somehow no __TEXT section was found...
            PANIC("Couldn't find __TEXT section while parsing Mach-O object");
            return 0;
        }

        size_t get_fat_index() const {
            VERIFY(fat_index != std::numeric_limits<size_t>::max());
            return fat_index;
        }

        void print_segments() const {
            int i = 0;
            for(const auto& command : load_commands) {
                if(command.cmd == LC_SEGMENT_64 || command.cmd == LC_SEGMENT) {
                    auto segment = command.cmd == LC_SEGMENT_64
                                        ? load_segment_command<64>(command.file_offset)
                                        : load_segment_command<32>(command.file_offset);
                    fprintf(stderr, "Load command %d\n", i);
                    fprintf(stderr, "         cmd %u\n", segment.cmd);
                    fprintf(stderr, "     cmdsize %u\n", segment.cmdsize);
                    fprintf(stderr, "     segname %s\n", segment.segname);
                    fprintf(stderr, "      vmaddr 0x%llx\n", segment.vmaddr);
                    fprintf(stderr, "      vmsize 0x%llx\n", segment.vmsize);
                    fprintf(stderr, "         off 0x%llx\n", segment.fileoff);
                    fprintf(stderr, "    filesize %llu\n", segment.filesize);
                    fprintf(stderr, "      nsects %u\n", segment.nsects);
                }
                i++;
            }
        }

    private:
        template<std::size_t Bits>
        void load_mach(
            bool allow_arch_mismatch
        ) {
            static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
            using Mach_Header = typename std::conditional<Bits == 32, mach_header, mach_header_64>::type;
            size_t header_size = sizeof(Mach_Header);
            Mach_Header header = load_bytes<Mach_Header>(file, load_base);
            magic = header.magic;
            if(should_swap()) {
                swap_mach_header(header);
            }
            thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
            if(
                header.cputype != mhp->cputype ||
                static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK) != header.cpusubtype
            ) {
                if(allow_arch_mismatch) {
                    return;
                } else {
                    PANIC("Mach-O file cpu type and subtype do not match current machine " + obj_path);
                }
            }
            cputype = header.cputype;
            cpusubtype = header.cpusubtype;
            filetype = header.filetype;
            n_load_commands = header.ncmds;
            sizeof_load_commands = header.sizeofcmds;
            flags = header.flags;
            // handle load commands
            uint32_t ncmds = header.ncmds;
            uint32_t load_commands_offset = load_base + header_size;
            // iterate load commands
            uint32_t actual_offset = load_commands_offset;
            for(uint32_t i = 0; i < ncmds; i++) {
                load_command cmd = load_bytes<load_command>(file, actual_offset);
                if(should_swap()) {
                    swap_load_command(&cmd, NX_UnknownByteOrder);
                }
                load_commands.push_back({ actual_offset, cmd.cmd, cmd.cmdsize });
                actual_offset += cmd.cmdsize;
            }
        }

        void load_fat_mach() {
            size_t header_size = sizeof(fat_header);
            size_t arch_size = sizeof(fat_arch);
            fat_header header = load_bytes<fat_header>(file, 0);
            if(should_swap()) {
                swap_fat_header(&header, NX_UnknownByteOrder);
            }
            thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
            off_t arch_offset = (off_t)header_size;
            for(size_t i = 0; i < header.nfat_arch; i++) {
                fat_arch arch = load_bytes<fat_arch>(file, arch_offset);
                if(should_swap()) {
                    swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
                }
                off_t mach_header_offset = (off_t)arch.offset;
                arch_offset += arch_size;
                uint32_t magic = load_bytes<uint32_t>(file, mach_header_offset);
                if(
                    arch.cputype == mhp->cputype &&
                    static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK) == arch.cpusubtype
                ) {
                    load_base = mach_header_offset;
                    fat_index = i;
                    if(is_magic_64(magic)) {
                        load_mach<64>(true);
                    } else {
                        load_mach<32>(true);
                    }
                    return;
                }
            }
            // If this is reached... something went wrong. The cpu we're on wasn't found.
            PANIC("Couldn't find appropriate architecture in fat Mach-O");
        }

        template<std::size_t Bits>
        segment_command_64 load_segment_command(uint32_t offset) const {
            using Segment_Command = typename std::conditional<Bits == 32, segment_command, segment_command_64>::type;
            Segment_Command segment = load_bytes<Segment_Command>(file, offset);
            ASSERT(segment.cmd == LC_SEGMENT_64 || segment.cmd == LC_SEGMENT);
            if(should_swap()) {
               swap_segment_command(segment);
            }
            // fields match just u64 instead of u32
            segment_command_64 common;
            common.cmd = segment.cmd;
            common.cmdsize = segment.cmdsize;
            static_assert(sizeof common.segname == 16 && sizeof segment.segname == 16, "xx");
            memcpy(common.segname, segment.segname, 16);
            common.vmaddr = segment.vmaddr;
            common.vmsize = segment.vmsize;
            common.fileoff = segment.fileoff;
            common.filesize = segment.filesize;
            common.maxprot = segment.maxprot;
            common.initprot = segment.initprot;
            common.nsects = segment.nsects;
            common.flags = segment.flags;
            return common;
        }

        bool should_swap() const {
            return should_swap_bytes(magic);
        }
    };

    inline bool macho_is_fat(const std::string& obj_path) {
        auto file = raii_wrap(fopen(obj_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + obj_path);
        }
        uint32_t magic = load_bytes<uint32_t>(file, 0);
        return is_fat_magic(magic);
    }
}
}

#pragma GCC diagnostic pop

#endif

#endif
