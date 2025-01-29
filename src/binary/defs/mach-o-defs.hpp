#ifndef MACH_O_DEFS_HPP
#define MACH_O_DEFS_HPP

#include <cstdint>

// This file contains definitons from various apple headers licensed under APSL
// https://opensource.apple.com/apsl/

typedef int32_t integer_t; // https://developer.apple.com/documentation/driverkit/integer_t
typedef integer_t cpu_type_t; // https://developer.apple.com/documentation/kernel/cpu_type_t
typedef integer_t cpu_subtype_t; // https://developer.apple.com/documentation/kernel/cpu_subtype_t

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/osfmk/mach/vm_prot.h#L75
typedef int vm_prot_t; // https://developer.apple.com/documentation/kernel/vm_prot_t

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/loader.h#L65
#define MH_MAGIC    0xfeedface    /* the mach magic number */
#define MH_CIGAM    0xcefaedfe    /* NXSwapInt(MH_MAGIC) */

struct mach_header {
    uint32_t      magic;        /* mach magic number identifier */
    cpu_type_t    cputype;      /* cpu specifier */
    cpu_subtype_t cpusubtype;   /* machine specifier */
    uint32_t      filetype;     /* type of file */
    uint32_t      ncmds;        /* number of load commands */
    uint32_t      sizeofcmds;   /* the size of all the load commands */
    uint32_t      flags;        /* flags */
};

/* Constant for the magic field of the mach_header_64 (64-bit architectures) */
#define MH_MAGIC_64 0xfeedfacf /* the 64-bit mach magic number */
#define MH_CIGAM_64 0xcffaedfe /* NXSwapInt(MH_MAGIC_64) */

struct mach_header_64 {
    uint32_t      magic;        /* mach magic number identifier */
    cpu_type_t    cputype;      /* cpu specifier */
    cpu_subtype_t cpusubtype;   /* machine specifier */
    uint32_t      filetype;     /* type of file */
    uint32_t      ncmds;        /* number of load commands */
    uint32_t      sizeofcmds;   /* the size of all the load commands */
    uint32_t      flags;        /* flags */
    uint32_t      reserved;     /* reserved */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/loader.h#L247C1-L250C3
struct load_command {
	uint32_t cmd;		/* type of load command */
	uint32_t cmdsize;	/* total size of command in bytes */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/fat.h#L48
#define FAT_MAGIC    0xcafebabe
#define FAT_CIGAM    0xbebafeca    /* NXSwapLong(FAT_MAGIC) */
#define FAT_MAGIC_64 0xcafebabf
#define FAT_CIGAM_64 0xbfbafeca      /* NXSwapLong(FAT_MAGIC_64) */

struct fat_header {
	uint32_t	magic;		/* FAT_MAGIC */
	uint32_t	nfat_arch;	/* number of structs that follow */
};

struct fat_arch {
	cpu_type_t	cputype;	/* cpu specifier (int) */
	cpu_subtype_t	cpusubtype;	/* machine specifier (int) */
	uint32_t	offset;		/* file offset to this object file */
	uint32_t	size;		/* size of this object file */
	uint32_t	align;		/* alignment as a power of 2 */
};

struct fat_arch_64 {
    cpu_type_t      cputype;        /* cpu specifier (int) */
    cpu_subtype_t   cpusubtype;     /* machine specifier (int) */
    uint64_t        offset;         /* file offset to this object file */
    uint64_t        size;           /* size of this object file */
    uint32_t        align;          /* alignment as a power of 2 */
    uint32_t        reserved;       /* reserved */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/loader.h#L355
struct segment_command { /* for 32-bit architectures */
    uint32_t    cmd;          /* LC_SEGMENT */
    uint32_t    cmdsize;      /* includes sizeof section structs */
    char        segname[16];  /* segment name */
    uint32_t    vmaddr;       /* memory address of this segment */
    uint32_t    vmsize;       /* memory size of this segment */
    uint32_t    fileoff;      /* file offset of this segment */
    uint32_t    filesize;     /* amount to map from the file */
    vm_prot_t   maxprot;      /* maximum VM protection */
    vm_prot_t   initprot;     /* initial VM protection */
    uint32_t    nsects;       /* number of sections in segment */
    uint32_t    flags;        /* flags */
};

struct segment_command_64 { /* for 64-bit architectures */
    uint32_t    cmd;          /* LC_SEGMENT_64 */
    uint32_t    cmdsize;      /* includes sizeof section_64 structs */
    char        segname[16];  /* segment name */
    uint64_t    vmaddr;       /* memory address of this segment */
    uint64_t    vmsize;       /* memory size of this segment */
    uint64_t    fileoff;      /* file offset of this segment */
    uint64_t    filesize;     /* amount to map from the file */
    vm_prot_t   maxprot;      /* maximum VM protection */
    vm_prot_t   initprot;     /* initial VM protection */
    uint32_t    nsects;       /* number of sections in segment */
    uint32_t    flags;        /* flags */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/loader.h#L868
struct symtab_command {
    uint32_t    cmd;        /* LC_SYMTAB */
    uint32_t    cmdsize;    /* sizeof(struct symtab_command) */
    uint32_t    symoff;     /* symbol table offset */
    uint32_t    nsyms;      /* number of symbol table entries */
    uint32_t    stroff;     /* string table offset */
    uint32_t    strsize;    /* string table size in bytes */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/nlist.h#L92
struct nlist {
    union {
// #ifndef __LP64__
//         char *n_name;   /* for use when in-core */
// #endif
        uint32_t n_strx;   /* index into the string table */
    } n_un;
    uint8_t n_type;        /* type flag, see below */
    uint8_t n_sect;        /* section number or NO_SECT */
    int16_t n_desc;        /* see <mach-o/stab.h> */
    uint32_t n_value;      /* value of this symbol (or stab offset) */
};

struct nlist_64 {
    union {
        uint32_t  n_strx;  /* index into the string table */
    } n_un;
    uint8_t n_type;        /* type flag, see below */
    uint8_t n_sect;        /* section number or NO_SECT */
    uint16_t n_desc;       /* see <mach-o/stab.h> */
    uint64_t n_value;      /* value of this symbol (or stab offset) */
};

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/loader.h#L263
/* Constants for the cmd field of all load commands, the type */
#define	LC_SEGMENT	0x1	/* segment of this file to be mapped */
#define	LC_SYMTAB	0x2	/* link-edit stab symbol table info */
#define	LC_SYMSEG	0x3	/* link-edit gdb symbol table info (obsolete) */
#define	LC_THREAD	0x4	/* thread */
#define	LC_UNIXTHREAD	0x5	/* unix thread (includes a stack) */
#define	LC_LOADFVMLIB	0x6	/* load a specified fixed VM shared library */
#define	LC_IDFVMLIB	0x7	/* fixed VM shared library identification */
#define	LC_IDENT	0x8	/* object identification info (obsolete) */
#define LC_FVMFILE	0x9	/* fixed VM file inclusion (internal use) */
#define LC_PREPAGE      0xa     /* prepage command (internal use) */
#define	LC_DYSYMTAB	0xb	/* dynamic link-edit symbol table info */
#define	LC_LOAD_DYLIB	0xc	/* load a dynamically linked shared library */
#define	LC_ID_DYLIB	0xd	/* dynamically linked shared lib ident */
#define LC_LOAD_DYLINKER 0xe	/* load a dynamic linker */
#define LC_ID_DYLINKER	0xf	/* dynamic linker identification */
#define	LC_PREBOUND_DYLIB 0x10	/* modules prebound for a dynamically */
				/*  linked shared library */
#define	LC_ROUTINES	0x11	/* image routines */
#define	LC_SUB_FRAMEWORK 0x12	/* sub framework */
#define	LC_SUB_UMBRELLA 0x13	/* sub umbrella */
#define	LC_SUB_CLIENT	0x14	/* sub client */
#define	LC_SUB_LIBRARY  0x15	/* sub library */
#define	LC_TWOLEVEL_HINTS 0x16	/* two-level namespace lookup hints */
#define	LC_PREBIND_CKSUM  0x17	/* prebind checksum */

#define	LC_SEGMENT_64	0x19	/* 64-bit segment of this file to be */
#define	LC_ROUTINES_64	0x1a	/* 64-bit image routines */
#define LC_UUID		0x1b	/* the uuid */

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/nlist.h#L117
#define	N_STAB	0xe0  /* if any of these bits set, a symbolic debugging entry */
#define	N_PEXT	0x10  /* private external symbol bit */
#define	N_TYPE	0x0e  /* mask for the type bits */
#define	N_EXT	0x01  /* external symbol bit, set for external symbols */

#define	N_UNDF	0x0		/* undefined, n_sect == NO_SECT */
#define	N_ABS	0x2		/* absolute, n_sect == NO_SECT */
#define	N_SECT	0xe		/* defined in section number n_sect */
#define	N_PBUD	0xc		/* prebound undefined (defined in a dylib) */
#define N_INDR	0xa		/* indirect */

// https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/EXTERNAL_HEADERS/mach-o/stab.h#L87C1-L116C68
#define	N_GSYM	0x20	/* global symbol: name,,NO_SECT,type,0 */
#define	N_FNAME	0x22	/* procedure name (f77 kludge): name,,NO_SECT,0,0 */
#define	N_FUN	0x24	/* procedure: name,,n_sect,linenumber,address */
#define	N_STSYM	0x26	/* static symbol: name,,n_sect,type,address */
#define	N_LCSYM	0x28	/* .lcomm symbol: name,,n_sect,type,address */
#define N_BNSYM 0x2e	/* begin nsect sym: 0,,n_sect,0,address */
#define N_AST	0x32	/* AST file path: name,,NO_SECT,0,0 */
#define N_OPT	0x3c	/* emitted with gcc2_compiled and in gcc source */
#define	N_RSYM	0x40	/* register sym: name,,NO_SECT,type,register */
#define	N_SLINE	0x44	/* src line: 0,,n_sect,linenumber,address */
#define N_ENSYM 0x4e	/* end nsect sym: 0,,n_sect,0,address */
#define	N_SSYM	0x60	/* structure elt: name,,NO_SECT,type,struct_offset */
#define	N_SO	0x64	/* source file name: name,,n_sect,0,address */
#define	N_OSO	0x66	/* object file name: name,,0,0,st_mtime */
#define	N_LSYM	0x80	/* local sym: name,,NO_SECT,type,offset */
#define N_BINCL	0x82	/* include file beginning: name,,NO_SECT,0,sum */
#define	N_SOL	0x84	/* #included file name: name,,n_sect,0,address */
#define	N_PARAMS  0x86	/* compiler parameters: name,,NO_SECT,0,0 */
#define	N_VERSION 0x88	/* compiler version: name,,NO_SECT,0,0 */
#define	N_OLEVEL  0x8A	/* compiler -O level: name,,NO_SECT,0,0 */
#define	N_PSYM	0xa0	/* parameter: name,,NO_SECT,type,offset */
#define N_EINCL	0xa2	/* include file end: name,,NO_SECT,0,0 */
#define	N_ENTRY	0xa4	/* alternate entry: name,,n_sect,linenumber,address */
#define	N_LBRAC	0xc0	/* left bracket: 0,,NO_SECT,nesting level,address */
#define N_EXCL	0xc2	/* deleted include file: name,,NO_SECT,0,sum */
#define	N_RBRAC	0xe0	/* right bracket: 0,,NO_SECT,nesting level,address */
#define	N_BCOMM	0xe2	/* begin common: name,,NO_SECT,0,0 */
#define	N_ECOMM	0xe4	/* end common: name,,n_sect,0,0 */
#define	N_ECOML	0xe8	/* end common (local name): 0,,n_sect,0,address */
#define	N_LENG	0xfe	/* second stab entry with length information */

extern "C" {
// There is exceedingly little evidence that this function actually exists. I think I discovered it through
// https://github.com/ruby/ruby/blob/ff64806ae51c2813f0c6334c0c52082b027c255c/addr2line.c#L2359.
// from MacOSX13.1.sdk/usr/include/crt_externs.h
#ifdef __LP64__
extern struct mach_header_64* _NSGetMachExecuteHeader(void);
#else /* !__LP64__ */
extern struct mach_header* _NSGetMachExecuteHeader(void);
#endif /* __LP64__ */

// MacOSX13.1.sdk/usr/include/mach-o/arch.h
extern struct fat_arch* NXFindBestFatArch(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    struct fat_arch* fat_archs,
    uint32_t nfat_archs
);
extern struct fat_arch_64* NXFindBestFatArch_64(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    struct fat_arch_64* fat_archs64,
    uint32_t nfat_archs
); // __CCTOOLS_DEPRECATED_MSG("use macho_best_slice()")
}

#endif
