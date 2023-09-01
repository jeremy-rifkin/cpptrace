
/* Define if building universal (internal helper macro) */
#cmakedefine AC_APPLE_UNIVERSAL_BUILD 1

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
#cmakedefine CRAY_STACKSEG_END 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#cmakedefine HAVE_DLFCN_H 1

/* Define to 1 if you have the <libelf.h> header file. */
#cmakedefine HAVE_INTTYPES_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#cmakedefine HAVE_FCNTL_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#cmakedefine HAVE_MALLOC_H 1

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H 1

/* Set to 1 if big endian . */
#cmakedefine WORDS_BIGENDIAN 1

/* Define to 1 if you have the <sgidefs.h> header file. */
#cmakedefine HAVE_SGIDEFS_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H 1

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H 1

/* Define to HAVE_UINTPTR_T 1 if the system has the type `uintptr_t'. */
#cmakedefine HAVE_UINTPTR_T 1
/* Define to 1 if the system has the type `intptr_t'. */
#cmakedefine HAVE_INTPTR_T


/*  Define to the uintptr_t to the type of an unsigned integer 
    type wide enough to hold a pointer
    if the system does not define it. */
#cmakedefine uintptr_t  ${uintptr_t}
#cmakedefine intptr_t  ${intptr_t}

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H 1

/* Set to 1 if __attribute__ ((unused)) is available. */
#cmakedefine HAVE_UNUSED_ATTRIBUTE 1

/* Set to 1 if zlib decompression is available. */
#cmakedefine HAVE_ZLIB 1

/* Define to 1 if you have the <zlib.h> header file. */
#cmakedefine HAVE_ZLIB_H 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#cmakedefine LT_OBJDIR 1

/* Name of package */
#cmakedefine PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#cmakedefine PACKAGE_NAME libdwarf

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING "${PACKAGE_NAME}  ${VERSION}"

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME

/* Define to the home page for this package. */
#cmakedefine PACKAGE_URL "${tarname}" )


/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
#cmakedefine STACK_DIRECTION

/* Define to 1 if you have the ANSI C header files. */
#cmakedefine STDC_HEADERS 1

/* Define to the version of this package. */
#cmakedefine PACKAGE_VERSION ${PACKAGE_VERSION}

/* Version number of package */
#cmakedefine VERSION   ${VERSION} 

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  cmakedefine WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

