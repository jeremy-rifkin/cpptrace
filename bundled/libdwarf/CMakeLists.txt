cmake_minimum_required(VERSION 3.5)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(libdwarf C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include_directories( ${PROJECT_BINARY_DIR} )

# used to compile on MSVC upto 2013 where snprintf is not available
macro(msvc_posix target)
    if(MSVC AND ("${MSVC_VERSION}" LESS 1900))
        # under VS 2015
	target_compile_definitions(${target} PUBLIC
	    snprintf=_snprintf)
    endif()
endmacro()

set(LIBDWARF_CRT "MD" CACHE STRING "Either MT or MD, specifies whether to use the static or dynamic MSVCRT.")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Use CMAKE_MSVC_RUNTIME in versions 3.15 and up
  if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(SET CMP0091 NEW)
    if (LIBDWARF_CRT STREQUAL "MT")
      message(STATUS "Using MT runtime by CMAKE_MSVC_RUNTIME_LIBRARY")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(LIBDWARF_CRT STREQUAL "MD")
      message(STATUS "Using MD runtime by CMAKE_MSVC_RUNTIME_LIBRARY")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
      message(FATAL_ERROR "LIBDWARF_CRT must be MT or MD")
    endif()
  # Use regexes in versions before 3.15
  else()
    if (LIBDWARF_CRT STREQUAL "MT")
      message(STATUS "Using MT runtime by compile flag replacement")
      # taken from the CMake FAQ
      foreach(flag_v
      CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if (${flag_v} MATCHES "([\\/\\-]M)D")
          string(REGEX REPLACE "([\\/\\-]M)D" "\\1T"
          ${flag_v} "${${flag_v}}")
        endif()
      endforeach()
    elseif(LIBDWARF_CRT STREQUAL "MD")
      message(STATUS "Using MD runtime by cmake default")
    else()
      message(FATAL_ERROR "LIBDWARF_CRT must be MT or MD")
    endif()
  endif()
endif()

# message("CMake flags are:")
# message("  CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
# message("  CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG}")
# message("  CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}")
# message("  CMAKE_C_FLAGS_MINSIZEREL: ${CMAKE_C_FLAGS_MINSIZEREL}")
# message("  CMAKE_C_FLAGS_RELWITHDEBINFO: ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
# message("  CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
# message("  CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
# message("  CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
# message("  CMAKE_CXX_FLAGS_MINSIZEREL: ${CMAKE_CXX_FLAGS_MINSIZEREL}")
# message("  CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")


# set target folder on IDE
macro(set_folder target folder)
  set_target_properties(${target} PROPERTIES FOLDER ${folder})
endmacro()

# set source groups on IDE
macro(set_source_group list_name group_name)
    set(${list_name} ${ARGN})
    source_group(${group_name} FILES ${ARGN})
endmacro()

# view folders on supported IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# tells cmake to look in 64bit first (cmake
# would probably do this anyway).
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)

enable_testing()

# always include project's folder to includes
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

### begin what was configure.cmake
# cmake macros
include(TestBigEndian)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckSymbolExists)
### Version also appears in configure.ac
set(VERSION 0.7.1)
set(PACKAGE_VERSION "\"${VERSION}\"")
set(PACKAGE_NAME "libdwarf" )
set(PACKAGE_STRING "\"${PACKAGE_NAME} ${VERSION}\"")
string(REGEX REPLACE "[\"]" "" tarname1 "${PACKAGE_STRING}" )
string(REGEX REPLACE "[^a-zA-Z0-9_]" "-" tarname "${tarname1}" )
set(PACKAGE_TARNAME "\"${tarname}\"" )

test_big_endian(isBigEndian)
if (${isBigEndian})
  set ( WORDS_BIGENDIAN 1 )
endif()

check_include_file( "sys/types.h"     HAVE_SYS_TYPES_H)
check_include_file( "sys/stat.h"      HAVE_SYS_STAT_H )
check_include_file( "inttypes.h"      HAVE_INTTYPES_H   )
check_include_file( "memory.h"        HAVE_MEMORY_H   )
check_include_file( "strings.h"       HAVE_STRINGS_H  )
check_include_file( "stdint.h"        HAVE_STDINT_H   )
check_include_file( "unistd.h"        HAVE_UNISTD_H   )
check_include_file( "sgidefs.h"       HAVE_SGIDEFS_H  )
check_include_file( "stdafx.h"        HAVE_STDAFX_H   )
check_include_file( "fcntl.h"         HAVE_FCNTL_H   )

### cmake provides no way to guarantee uint32_t present.
### configure does guarantee that.
if(HAVE_STDINT_H)
  check_c_source_compiles("
  #include <stdint.h>
  int main()
  {
      uintptr_t i; i = 12;
      return (int)i;
  }" HAVE_UINTPTR_T)
  check_c_source_compiles("
  #include <stdint.h>
  int main()
  {
      intptr_t i; i = 12;
      return (int)i;
  }" HAVE_INTPTR_T)
endif()

if (HAVE_UINTPTR_T)
  if(NOT printed_stati)
    message(STATUS "HAVE_UINTPTR_T 1: uintptr_t defined in stdint.h... YES")
  endif()
else()
  if(NOT printed_stati)
    message(STATUS "uintptr_t defined in stdint.h... NO")
  endif()
  set(uintptr_t "unsigned long long int" )
  if(NOT printed_stati)
    message(STATUS "Setting #define uintptr_t " ${uintptr_t})
  endif()
endif()
if (uintptr_t)
  if(NOT printed_stati)
    message(STATUS "uintptr_t value considered YES ")
  endif()
else()
  if(NOT printed_stati)
    message(STATUS "uintptr_t value considered NO ")
  endif()
endif()
if (HAVE_INTPTR_T)
  if(NOT printed_stati)
    message(STATUS "HAVE_INTPTR_T 1: intptr_t defined in stdint.h... YES")
  endif()
else()
  if(NOT printed_stati)
    message(STATUS "intptr_t defined in stdint.h... NO")
  endif()
  set(uintptr_t "long long int" )
  if(NOT printed_stati)
    message(STATUS "Setting #define intptr_t " ${intptr_t})
  endif()
endif()
if (intptr_t)
  if(NOT printed_stati)
    message(STATUS "intptr_t value considered YES ")
  endif()
else()
  if(NOT printed_stati)
    message(STATUS "intptr_t value considered NO ")
  endif()
endif()

check_c_source_compiles("
  static unsigned foo( unsigned x,
      __attribute__ ((unused)) int y)
  {
      unsigned x2 = x + 1;
      return x2;
  }

  int main(void) {
      unsigned y = 0;
      y = foo(12,y);
      return 0;
  }"    HAVE_UNUSED_ATTRIBUTE)
if(NOT printed_stati)
  message(STATUS "Checking compiler supports __attribute__ unused... ${HAVE_UNUSED_ATTRIBUTE}")
endif()

check_c_source_compiles([=[
  #include "stdafx.h"
  int main()
  {
      int p; p = 27;
      return 0;
  }]=] HAVE_STDAFX_H)
#message(STATUS "Checking have windows stdafx.h... ${HAVE_STDAFX_H}")

option(CMAKE_DWARF_FINDPACKAGE_Z "" OFF)

mark_as_advanced(CMAKE_DWARF_FINDPACKAGE_Z)

if(NOT CMAKE_DWARF_FINDPACKAGE_Z)
  set(CMAKE_REQUIRED_LIBRARIES z)
  check_c_source_compiles( [=[
    #include "zlib.h"
    int main()
    {
        Bytef dest[100];
        uLongf destlen = 100;
        Bytef *src = 0;
        uLong srclen = 3;
        int res = uncompress(dest,&destlen,src,srclen);
        if (res == Z_OK) {
            /* ALL IS WELL */
        }
        return 0;
    } ]=]  HAVE_ZLIB )
  check_c_source_compiles( [=[
    #include "zlib.h"
    int main()
    {
        Bytef dest[100];
        uLongf destlen = 100;
        Bytef *src = 0;
        uLong srclen = 3;
        int res = uncompress(dest,&destlen,src,srclen);
        if (res == Z_OK) {
            /* ALL IS WELL */
        }
        return 0;
    } ]=]  HAVE_ZLIB_H )
  set(CMAKE_REQUIRED_LIBRARIES)
  if (HAVE_ZLIB)
    # For linking in libz
    set(DW_FZLIB "z")
  endif()

  set(CMAKE_REQUIRED_LIBRARIES zstd )
  check_c_source_compiles( [=[
    #include "zstd.h"
    int main()
    {
        char * dest[100];
        size_t destlen = 100;
        char *src = 0;
        size_t srclen = 3;
        size_t res = ZSTD_decompress(dest,destlen,src,srclen);
        if (res == destlen) {
            /* ALL IS WELL */
        }
        return 0;
    } ]=]  HAVE_ZSTD )
  check_c_source_compiles( [=[
    #include "zstd.h"
    int main()
    {
        char * dest[100];
        size_t destlen = 100;
        char *src = 0;
        size_t srclen = 3;
        size_t res = ZSTD_decompress(dest,destlen,src,srclen);
        if (res == destlen) {
            /* ALL IS WELL */
        }
        return 0;
    } ]=]  HAVE_ZSTD_H )
  set(CMAKE_REQUIRED_LIBRARIES)
  if (HAVE_ZSTD)
    # For linking in libzstd
    set(DW_FZSTD "zstd")
  endif()
else()
  find_package(ZLIB REQUIRED)
  set(DW_FZLIB "ZLIB")
  set(HAVE_ZLIB TRUE)
  set(HAVE_ZLIB_H TRUE)
endif()

check_c_source_compiles([=[
#include <stdint.h>
int main()
{
    intptr_t p;
    p = 27;
    return 0;
}]=] HAVE_INTPTR_T)

if(NOT printed_stati)
  message(STATUS "CMAKE_SIZEOF_VOID_P ... " ${CMAKE_SIZEOF_VOID_P} )
endif()

set(printed_stati On CACHE INTERNAL "")

configure_file(cmake/config.h.cmake config.h)

set(
  libdwarf_sources
  dwarf_abbrev.c
  dwarf_alloc.c
  dwarf_crc.c
  dwarf_crc32.c
  dwarf_arange.c
  dwarf_debug_sup.c
  dwarf_debugaddr.c
  dwarf_debuglink.c
  dwarf_die_deliv.c
  dwarf_debugnames.c
  dwarf_dsc.c
  dwarf_elf_load_headers.c
  dwarf_elfread.c
  dwarf_elf_rel_detector.c
  dwarf_error.c
  dwarf_fill_in_attr_form.c
  dwarf_find_sigref.c
  dwarf_fission_to_cu.c
  dwarf_form.c
  dwarf_form_class_names.c
  dwarf_frame.c
  dwarf_frame2.c
  dwarf_gdbindex.c
  dwarf_global.c
  dwarf_gnu_index.c
  dwarf_groups.c
  dwarf_harmless.c
  dwarf_generic_init.c
  dwarf_init_finish.c
  dwarf_leb.c
  dwarf_line.c
  dwarf_loc.c
  dwarf_loclists.c
  dwarf_locationop_read.c
  dwarf_machoread.c
  dwarf_macro.c
  dwarf_macro5.c
  dwarf_memcpy_swap.c
  dwarf_names.c
  dwarf_object_read_common.c
  dwarf_object_detector.c
  dwarf_peread.c
  dwarf_query.c
  dwarf_ranges.c
  dwarf_rnglists.c
  dwarf_safe_arithmetic.c
  dwarf_safe_strcpy.c
  dwarf_secname_ck.c
  dwarf_setup_sections.c
  dwarf_string.h
  dwarf_string.c
  dwarf_stringsection.c
  dwarf_tied.c
  dwarf_str_offsets.c
  dwarf_tsearchhash.c
  dwarf_util.c
  dwarf_xu_index.c
  dwarf_print_lines.c
)

set(
  libdwarf_headers
  dwarf.h
  dwarf_abbrev.h
  dwarf_alloc.h
  dwarf_arange.h
  dwarf_base_types.h
  dwarf_debugaddr.h
  dwarf_debuglink.h
  dwarf_debugnames.h
  dwarf_die_deliv.h
  dwarf_dsc.h
  dwarf_elfread.h
  dwarf_elfstructs.h
  dwarf_elf_access.h
  dwarf_elf_defines.h
  dwarf_elf_rel_detector.h
  dwarf_errmsg_list.h
  dwarf_error.h
  dwarf_frame.h
  dwarf_gdbindex.h
  dwarf_global.h
  dwarf_gnu_index.h
  dwarf_harmless.h
  dwarf_line.h
  dwarf_line_table_reader_common.h
  dwarf_loc.h
  dwarf_machoread.h
  dwarf_macho_loader.h
  dwarf_macro.h
  dwarf_macro5.h
  dwarf_memcpy_swap.h
  dwarf_object_detector.h
  dwarf_object_read_common.h
  dwarf_opaque.h
  dwarf_peread.h
  dwarf_pe_descr.h
  dwarf_reading.h
  dwarf_rnglists.h
  dwarf_safe_arithmetic.h
  dwarf_safe_strcpy.h
  dwarf_secname_ck.h
  dwarf_setup_sections.h
  dwarf_string.h
  dwarf_str_offsets.h
  dwarf_tied_decls.h
  dwarf_tsearch.h
  dwarf_util.h
  dwarf_xu_index.h
  libdwarf.h
  libdwarf_private.h
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(GNUInstallDirs)

add_library(dwarf OBJECT ${libdwarf_sources} ${libdwarf_headers})
set_property(TARGET dwarf PROPERTY POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(dwarf PUBLIC LIBDWARF_BUILD)

target_include_directories(
  dwarf
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(CMAKE_DWARF_FINDPACKAGE_Z)
  target_link_libraries(dwarf PUBLIC ZLIB::ZLIB)
else()
  target_link_libraries(dwarf PUBLIC ${DW_FZLIB} ${DW_FZSTD})
endif()

# TODO: Something will have to be figured out for dynamic linking, but for now there's nothing to install
# if(NOT CMAKE_SKIP_INSTALL_RULES)
#   include(CMakePackageConfigHelpers)

#   install(
#     TARGETS dwarf
#     EXPORT libdwarf_targets
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   )

#   install(
#     FILES
#     ${libdwarf_headers}
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libdwarf
#   )

#   export(
#     EXPORT libdwarf_targets
#     FILE ${CMAKE_CURRENT_BINARY_DIR}/libdwarf/libdwarf_targets.cmake
#     NAMESPACE libdwarf::
#   )

#   configure_package_config_file(
#     cmake/libdwarf-config.cmake.in
#     ${CMAKE_CURRENT_BINARY_DIR}/libdwarf/libdwarf-config.cmake
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libdwarf
#   )

#   write_basic_package_version_file(
#     ${CMAKE_CURRENT_BINARY_DIR}/libdwarf/libdwarf-config-version.cmake
#     VERSION "${VERSION}"
#     COMPATIBILITY SameMajorVersion
#   )

#   install(
#     EXPORT libdwarf_targets
#     FILE libdwarf_targets.cmake
#     NAMESPACE libdwarf::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libdwarf
#   )

#   install(
#     FILES
#     ${CMAKE_CURRENT_BINARY_DIR}/libdwarf/libdwarf-config.cmake
#     ${CMAKE_CURRENT_BINARY_DIR}/libdwarf/libdwarf-config-version.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libdwarf
#   )
# endif()
