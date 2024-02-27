#ifndef MACHO_HPP
#define MACHO_HPP

#include "../utils/common.hpp"
#include "../utils/utils.hpp"

#if IS_APPLE

// A number of mach-o functions are deprecated as of macos 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <iostream>
#include <iomanip>

#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>
#include <crt_externs.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/arch.h>

namespace cpptrace {
namespace detail {
    inline bool is_mach_o(std::uint32_t magic) {
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

    inline bool file_is_mach_o(const std::string& object_path) noexcept {
        try {
            FILE* file = std::fopen(object_path.c_str(), "rb");
            if(file == nullptr) {
                return false;
            }
            auto magic = load_bytes<std::uint32_t>(file, 0);
            return is_mach_o(magic);
        } catch(...) {
            return false;
        }
    }

    inline bool is_fat_magic(std::uint32_t magic) {
        return magic == FAT_MAGIC || magic == FAT_CIGAM;
    }

    // Based on https://github.com/AlexDenisov/segment_dumper/blob/master/main.c
    // and https://lowlevelbits.org/parsing-mach-o-files/
    inline bool is_magic_64(std::uint32_t magic) {
        return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
    }

    inline bool should_swap_bytes(std::uint32_t magic) {
        return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
    }

    inline void swap_mach_header(mach_header_64& header) {
        swap_mach_header_64(&header, NX_UnknownByteOrder);
    }

    inline void swap_mach_header(mach_header& header) {
        swap_mach_header(&header, NX_UnknownByteOrder);
    }

    inline void swap_segment_command(segment_command_64& segment) {
        swap_segment_command_64(&segment, NX_UnknownByteOrder);
    }

    inline void swap_segment_command(segment_command& segment) {
        swap_segment_command(&segment, NX_UnknownByteOrder);
    }

    inline void swap_nlist(struct nlist& entry) {
        swap_nlist(&entry, 1, NX_UnknownByteOrder);
    }

    inline void swap_nlist(struct nlist_64& entry) {
        swap_nlist_64(&entry, 1, NX_UnknownByteOrder);
    }

    #ifdef __LP64__
     #define LP(x) x##_64
    #else
     #define LP(x) x
    #endif

    struct load_command_entry {
        std::uint32_t file_offset;
        std::uint32_t cmd;
        std::uint32_t cmdsize;
    };

    class mach_o {
        std::FILE* file = nullptr;
        std::string object_path;
        std::uint32_t magic;
        cpu_type_t cputype;
        cpu_subtype_t cpusubtype;
        std::uint32_t filetype;
        std::uint32_t n_load_commands;
        std::uint32_t sizeof_load_commands;
        std::uint32_t flags;
        std::size_t bits = 0; // 32 or 64 once load_mach is called

        std::size_t load_base = 0;
        std::size_t fat_index = std::numeric_limits<std::size_t>::max();

        std::vector<load_command_entry> load_commands;

        struct symtab_info_data {
            symtab_command symtab;
            std::unique_ptr<char[]> stringtab;
            const char* get_string(std::size_t index) const {
                if(stringtab && index < symtab.strsize) {
                    return stringtab.get() + index;
                } else {
                    throw std::runtime_error("can't retrieve symbol from symtab");
                }
            }
        };

        bool tried_to_load_symtab = false;
        optional<symtab_info_data> symtab_info;

    public:
        mach_o(const std::string& object_path) : object_path(object_path) {
            file = std::fopen(object_path.c_str(), "rb");
            if(file == nullptr) {
                throw file_error("Unable to read object file " + object_path);
            }
            magic = load_bytes<std::uint32_t>(file, 0);
            VERIFY(is_mach_o(magic), "File is not Mach-O " + object_path);
            if(magic == FAT_MAGIC || magic == FAT_CIGAM) {
                load_fat_mach();
            } else {
                fat_index = 0;
                if(is_magic_64(magic)) {
                    load_mach<64>();
                } else {
                    load_mach<32>();
                }
            }
        }

        ~mach_o() {
            if(file) {
                std::fclose(file);
            }
        }

        std::uintptr_t get_text_vmaddr() {
            for(const auto& command : load_commands) {
                if(command.cmd == LC_SEGMENT_64 || command.cmd == LC_SEGMENT) {
                    auto segment = command.cmd == LC_SEGMENT_64
                                        ? load_segment_command<64>(command.file_offset)
                                        : load_segment_command<32>(command.file_offset);
                    if(std::strcmp(segment.segname, "__TEXT") == 0) {
                        return segment.vmaddr;
                    }
                }
            }
            // somehow no __TEXT section was found...
            throw std::runtime_error("Couldn't find __TEXT section while parsing Mach-O object");
        }

        std::size_t get_fat_index() const {
            VERIFY(fat_index != std::numeric_limits<std::size_t>::max());
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

        optional<symtab_info_data>& get_symtab_info() {
            if(!symtab_info.has_value() && !tried_to_load_symtab) {
                // don't try to load the symtab again if for some reason loading here fails
                tried_to_load_symtab = true;
                for(const auto& command : load_commands) {
                    if(command.cmd == LC_SYMTAB) {
                        symtab_info_data info;
                        info.symtab = load_symbol_table_command(command.file_offset);
                        info.stringtab = load_string_table(info.symtab.stroff, info.symtab.strsize);
                        symtab_info = std::move(info);
                        break;
                    }
                }
            }
            return symtab_info;
        }

        void print_symbol_table_entry(
            const nlist_64& entry,
            const std::unique_ptr<char[]>& stringtab,
            std::size_t stringsize,
            std::size_t j
        ) const {
            const char* type = "";
            if(entry.n_type & N_STAB) {
                switch(entry.n_type) {
                    case N_SO: type = "N_SO"; break;
                    case N_OSO: type = "N_OSO"; break;
                    case N_BNSYM: type = "N_BNSYM"; break;
                    case N_ENSYM: type = "N_ENSYM"; break;
                    case N_FUN: type = "N_FUN"; break;
                }
            } else if((entry.n_type & N_TYPE) == N_SECT) {
                type = "N_SECT";
            }
            fprintf(
                stderr,
                "%5llu %8llx %2llx %7s %2llu %4llx %16llx %s\n",
                to_ull(j),
                to_ull(entry.n_un.n_strx),
                to_ull(entry.n_type),
                type,
                to_ull(entry.n_sect),
                to_ull(entry.n_desc),
                to_ull(entry.n_value),
                stringtab == nullptr
                    ? "Stringtab error"
                    : entry.n_un.n_strx < stringsize
                        ? stringtab.get() + entry.n_un.n_strx
                        : "String index out of bounds"
            );
        }

        void print_symbol_table() {
            int i = 0;
            for(const auto& command : load_commands) {
                if(command.cmd == LC_SYMTAB) {
                    auto symtab = load_symbol_table_command(command.file_offset);
                    fprintf(stderr, "Load command %d\n", i);
                    fprintf(stderr, "         cmd %llu\n", to_ull(symtab.cmd));
                    fprintf(stderr, "     cmdsize %llu\n", to_ull(symtab.cmdsize));
                    fprintf(stderr, "      symoff 0x%llu\n", to_ull(symtab.symoff));
                    fprintf(stderr, "       nsyms %llu\n", to_ull(symtab.nsyms));
                    fprintf(stderr, "      stroff 0x%llu\n", to_ull(symtab.stroff));
                    fprintf(stderr, "     strsize %llu\n", to_ull(symtab.strsize));
                    auto stringtab = load_string_table(symtab.stroff, symtab.strsize);
                    for(std::size_t j = 0; j < symtab.nsyms; j++) {
                        nlist_64 entry = bits == 32
                                        ? load_symtab_entry<32>(symtab.symoff, j)
                                        : load_symtab_entry<64>(symtab.symoff, j);
                        print_symbol_table_entry(entry, stringtab, symtab.strsize, j);
                    }
                }
                i++;
            }
        }

        struct debug_map_entry {
            uint64_t source_address;
            uint64_t size;
            std::string name;
        };

        struct symbol_entry {
            uint64_t address;
            std::string name;
        };

        // map from object file to a vector of symbols to resolve
        using debug_map = std::unordered_map<std::string, std::vector<debug_map_entry>>;

        // produce information similar to dsymutil -dump-debug-map
        debug_map get_debug_map() {
            // we have a bunch of symbols in our binary we need to pair up with symbols from various .o files
            // first collect symbols and the objects they come from
            debug_map debug_map;
            const auto& symtab_info = get_symtab_info().unwrap();
            const auto& symtab = symtab_info.symtab;
            // TODO: Take timestamp into account?
            std::string current_module;
            optional<debug_map_entry> current_function;
            for(std::size_t j = 0; j < symtab.nsyms; j++) {
                nlist_64 entry = bits == 32
                                ? load_symtab_entry<32>(symtab.symoff, j)
                                : load_symtab_entry<64>(symtab.symoff, j);
                // entry.n_type & N_STAB indicates symbolic debug info
                if(!(entry.n_type & N_STAB)) {
                    continue;
                }
                switch(entry.n_type) {
                    case N_SO:
                        // pass - these encode path and filename for the module, if applicable
                        break;
                    case N_OSO:
                        // sets the module
                        current_module = symtab_info.get_string(entry.n_un.n_strx);
                        break;
                    case N_BNSYM: break; // pass
                    case N_ENSYM: break; // pass
                    case N_FUN:
                        {
                            const char* str = symtab_info.get_string(entry.n_un.n_strx);
                            if(str[0] == 0) {
                                // end of function scope
                                if(!current_function) { /**/ }
                                current_function.unwrap().size = entry.n_value;
                                debug_map[current_module].push_back(std::move(current_function).unwrap());
                            } else {
                                current_function = debug_map_entry{};
                                current_function.unwrap().source_address = entry.n_value;
                                current_function.unwrap().name = str;
                            }
                        }
                        break;
                }
            }
            return debug_map;
        }

        std::vector<symbol_entry> symbol_table() {
            // we have a bunch of symbols in our binary we need to pair up with symbols from various .o files
            // first collect symbols and the objects they come from
            std::vector<symbol_entry> symbols;
            const auto& symtab_info = get_symtab_info().unwrap();
            const auto& symtab = symtab_info.symtab;
            // TODO: Take timestamp into account?
            for(std::size_t j = 0; j < symtab.nsyms; j++) {
                nlist_64 entry = bits == 32
                                ? load_symtab_entry<32>(symtab.symoff, j)
                                : load_symtab_entry<64>(symtab.symoff, j);
                if(entry.n_type & N_STAB) {
                    continue;
                }
                if((entry.n_type & N_TYPE) == N_SECT) {
                    symbols.push_back({
                        entry.n_value,
                        symtab_info.get_string(entry.n_un.n_strx)
                    });
                }
            }
            return symbols;
        }

        // produce information similar to dsymutil -dump-debug-map
        static void print_debug_map(const debug_map& debug_map) {
            for(const auto& entry : debug_map) {
                std::cout<<entry.first<<": "<< '\n';
                for(const auto& symbol : entry.second) {
                    std::cerr
                        << "    "
                        << symbol.name
                        << " "
                        << std::hex
                        << symbol.source_address
                        << " "
                        << symbol.size
                        << std::dec
                        << '\n';
                }
            }
        }

    private:
        template<std::size_t Bits>
        void load_mach() {
            static_assert(Bits == 32 || Bits == 64, "Unexpected Bits argument");
            bits = Bits;
            using Mach_Header = typename std::conditional<Bits == 32, mach_header, mach_header_64>::type;
            std::size_t header_size = sizeof(Mach_Header);
            Mach_Header header = load_bytes<Mach_Header>(file, load_base);
            magic = header.magic;
            if(should_swap()) {
                swap_mach_header(header);
            }
            cputype = header.cputype;
            cpusubtype = header.cpusubtype;
            filetype = header.filetype;
            n_load_commands = header.ncmds;
            sizeof_load_commands = header.sizeofcmds;
            flags = header.flags;
            // handle load commands
            std::uint32_t ncmds = header.ncmds;
            std::uint32_t load_commands_offset = load_base + header_size;
            // iterate load commands
            std::uint32_t actual_offset = load_commands_offset;
            for(std::uint32_t i = 0; i < ncmds; i++) {
                load_command cmd = load_bytes<load_command>(file, actual_offset);
                if(should_swap()) {
                    swap_load_command(&cmd, NX_UnknownByteOrder);
                }
                load_commands.push_back({ actual_offset, cmd.cmd, cmd.cmdsize });
                actual_offset += cmd.cmdsize;
            }
        }

        void load_fat_mach() {
            std::size_t header_size = sizeof(fat_header);
            std::size_t arch_size = sizeof(fat_arch);
            fat_header header = load_bytes<fat_header>(file, 0);
            if(should_swap()) {
                swap_fat_header(&header, NX_UnknownByteOrder);
            }
            // thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
            // off_t arch_offset = (off_t)header_size;
            // for(std::size_t i = 0; i < header.nfat_arch; i++) {
            //     fat_arch arch = load_bytes<fat_arch>(file, arch_offset);
            //     if(should_swap()) {
            //         swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
            //     }
            //     off_t mach_header_offset = (off_t)arch.offset;
            //     arch_offset += arch_size;
            //     std::uint32_t magic = load_bytes<std::uint32_t>(file, mach_header_offset);
            //     std::cerr<<"xxx: "<<arch.cputype<<" : "<<mhp->cputype<<std::endl;
            //     std::cerr<<"     "<<arch.cpusubtype<<" : "<<static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK)<<std::endl;
            //     if(
            //         arch.cputype == mhp->cputype &&
            //         static_cast<cpu_subtype_t>(mhp->cpusubtype & ~CPU_SUBTYPE_MASK) == arch.cpusubtype
            //     ) {
            //         load_base = mach_header_offset;
            //         fat_index = i;
            //         if(is_magic_64(magic)) {
            //             load_mach<64>(true);
            //         } else {
            //             load_mach<32>(true);
            //         }
            //         return;
            //     }
            // }
            std::vector<fat_arch> fat_arches;
            fat_arches.reserve(header.nfat_arch);
            off_t arch_offset = (off_t)header_size;
            for(std::size_t i = 0; i < header.nfat_arch; i++) {
                fat_arch arch = load_bytes<fat_arch>(file, arch_offset);
                if(should_swap()) {
                    swap_fat_arch(&arch, 1, NX_UnknownByteOrder);
                }
                fat_arches.push_back(arch);
                arch_offset += arch_size;
            }
            thread_local static struct LP(mach_header)* mhp = _NSGetMachExecuteHeader();
            fat_arch* best = NXFindBestFatArch(
                mhp->cputype,
                mhp->cpusubtype,
                fat_arches.data(),
                header.nfat_arch
            );
            if(best) {
                off_t mach_header_offset = (off_t)best->offset;
                std::uint32_t magic = load_bytes<std::uint32_t>(file, mach_header_offset);
                load_base = mach_header_offset;
                fat_index = best - fat_arches.data();
                if(is_magic_64(magic)) {
                    load_mach<64>();
                } else {
                    load_mach<32>();
                }
                return;
            }
            // If this is reached... something went wrong. The cpu we're on wasn't found.
            throw std::runtime_error("Couldn't find appropriate architecture in fat Mach-O");
        }

        template<std::size_t Bits>
        segment_command_64 load_segment_command(std::uint32_t offset) const {
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

        symtab_command load_symbol_table_command(std::uint32_t offset) const {
            symtab_command symtab = load_bytes<symtab_command>(file, offset);
            ASSERT(symtab.cmd == LC_SYMTAB);
            if(should_swap()) {
               swap_symtab_command(&symtab, NX_UnknownByteOrder);
            }
            return symtab;
        }

        template<std::size_t Bits>
        nlist_64 load_symtab_entry(std::uint32_t symbol_base, std::size_t index) const {
            using Nlist = typename std::conditional<Bits == 32, struct nlist, struct nlist_64>::type;
            uint32_t offset = load_base + symbol_base + index * sizeof(Nlist);
            Nlist entry = load_bytes<Nlist>(file, offset);
            if(should_swap()) {
               swap_nlist(entry);
            }
            // fields match just u64 instead of u32
            nlist_64 common;
            common.n_un.n_strx = entry.n_un.n_strx;
            common.n_type = entry.n_type;
            common.n_sect = entry.n_sect;
            common.n_desc = entry.n_desc;
            common.n_value = entry.n_value;
            return common;
        }

        std::unique_ptr<char[]> load_string_table(std::uint32_t offset, std::uint32_t byte_count) const {
            std::unique_ptr<char[]> buffer(new char[byte_count + 1]);
            VERIFY(std::fseek(file, load_base + offset, SEEK_SET) == 0, "fseek error");
            VERIFY(std::fread(buffer.get(), sizeof(char), byte_count, file) == byte_count, "fread error");
            buffer[byte_count] = 0; // just out of an abundance of caution
            return buffer;
        }

        bool should_swap() const {
            return should_swap_bytes(magic);
        }
    };

    inline bool macho_is_fat(const std::string& object_path) {
        auto file = raii_wrap(std::fopen(object_path.c_str(), "rb"), file_deleter);
        if(file == nullptr) {
            throw file_error("Unable to read object file " + object_path);
        }
        std::uint32_t magic = load_bytes<std::uint32_t>(file, 0);
        return is_fat_magic(magic);
    }
}
}

#pragma GCC diagnostic pop

#endif

#endif
