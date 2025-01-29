#ifndef ELF_DEFS_HPP
#define ELF_DEFS_HPP

#include <cstdint>

namespace cpptrace {
namespace detail {

    // https://man7.org/linux/man-pages/man5/elf.5.html
    // https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h

    /* 32-bit ELF base types. */
    typedef std::uint32_t Elf32_Addr;
    typedef std::uint16_t Elf32_Half;
    typedef std::uint32_t Elf32_Off;
    typedef std::int32_t  Elf32_Sword;
    typedef std::uint32_t Elf32_Word;

    /* 64-bit ELF base types. */
    typedef std::uint64_t Elf64_Addr;
    typedef std::uint16_t Elf64_Half;
    typedef std::int16_t  Elf64_SHalf;
    typedef std::uint64_t Elf64_Off;
    typedef std::int32_t  Elf64_Sword;
    typedef std::uint32_t Elf64_Word;
    typedef std::uint64_t Elf64_Xword;
    typedef std::int64_t  Elf64_Sxword;

    #define PT_PHDR 6
    #define EI_NIDENT 16
    #define SHT_SYMTAB 2
    #define SHT_STRTAB 3
    #define SHT_DYNAMIC 6

    typedef struct {
        std::uint32_t p_type;
        Elf32_Off     p_offset;
        Elf32_Addr    p_vaddr;
        Elf32_Addr    p_paddr;
        std::uint32_t p_filesz;
        std::uint32_t p_memsz;
        std::uint32_t p_flags;
        std::uint32_t p_align;
    } Elf32_Phdr;

    typedef struct {
        std::uint32_t p_type;
        std::uint32_t p_flags;
        Elf64_Off     p_offset;
        Elf64_Addr    p_vaddr;
        Elf64_Addr    p_paddr;
        std::uint64_t p_filesz;
        std::uint64_t p_memsz;
        std::uint64_t p_align;
    } Elf64_Phdr;

    typedef struct elf32_hdr {
        unsigned char e_ident[EI_NIDENT];
        Elf32_Half    e_type;
        Elf32_Half    e_machine;
        Elf32_Word    e_version;
        Elf32_Addr    e_entry; /* Entry point */
        Elf32_Off     e_phoff;
        Elf32_Off     e_shoff;
        Elf32_Word    e_flags;
        Elf32_Half    e_ehsize;
        Elf32_Half    e_phentsize;
        Elf32_Half    e_phnum;
        Elf32_Half    e_shentsize;
        Elf32_Half    e_shnum;
        Elf32_Half    e_shstrndx;
    } Elf32_Ehdr;

    typedef struct elf64_hdr {
        unsigned char e_ident[EI_NIDENT]; /* ELF "magic number" */
        Elf64_Half    e_type;
        Elf64_Half    e_machine;
        Elf64_Word    e_version;
        Elf64_Addr    e_entry; /* Entry point virtual address */
        Elf64_Off     e_phoff; /* Program header table file offset */
        Elf64_Off     e_shoff; /* Section header table file offset */
        Elf64_Word    e_flags;
        Elf64_Half    e_ehsize;
        Elf64_Half    e_phentsize;
        Elf64_Half    e_phnum;
        Elf64_Half    e_shentsize;
        Elf64_Half    e_shnum;
        Elf64_Half    e_shstrndx;
    } Elf64_Ehdr;

    typedef struct elf32_shdr {
        Elf32_Word sh_name;
        Elf32_Word sh_type;
        Elf32_Word sh_flags;
        Elf32_Addr sh_addr;
        Elf32_Off sh_offset;
        Elf32_Word sh_size;
        Elf32_Word sh_link;
        Elf32_Word sh_info;
        Elf32_Word sh_addralign;
        Elf32_Word sh_entsize;
    } Elf32_Shdr;

    typedef struct elf64_shdr {
        Elf64_Word  sh_name;      /* Section name, index in string tbl */
        Elf64_Word  sh_type;      /* Type of section */
        Elf64_Xword sh_flags;     /* Miscellaneous section attributes */
        Elf64_Addr  sh_addr;      /* Section virtual addr at execution */
        Elf64_Off   sh_offset;    /* Section file offset */
        Elf64_Xword sh_size;      /* Size of section in bytes */
        Elf64_Word  sh_link;      /* Index of another section */
        Elf64_Word  sh_info;      /* Additional section information */
        Elf64_Xword sh_addralign; /* Section alignment */
        Elf64_Xword sh_entsize;   /* Entry size if section holds table */
    } Elf64_Shdr;

    typedef struct elf32_sym {
        Elf32_Word st_name;
        Elf32_Addr st_value;
        Elf32_Word st_size;
        unsigned char st_info;
        unsigned char st_other;
        Elf32_Half st_shndx;
    } Elf32_Sym;

    typedef struct elf64_sym {
        Elf64_Word    st_name;  /* Symbol name, index in string tbl */
        unsigned char st_info;  /* Type and binding attributes */
        unsigned char st_other; /* No defined meaning, 0 */
        Elf64_Half    st_shndx; /* Associated section index */
        Elf64_Addr    st_value; /* Value of the symbol */
        Elf64_Xword   st_size;  /* Associated symbol size */
    } Elf64_Sym;

}
}

#endif
