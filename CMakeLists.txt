cmake_minimum_required(VERSION 3.14)

include(cmake/PreventInSourceBuilds.cmake)

# ---- Initialize Project ----

# Used to support find_package
set(package_name "cpptrace")

project(
  cpptrace
  VERSION 0.3.1
  DESCRIPTION "Simple, portable, and self-contained stacktrace library for C++11 and newer "
  HOMEPAGE_URL "https://github.com/jeremy-rifkin/cpptrace"
  LANGUAGES C CXX
)

# Don't change include order, OptionVariables checks if project is top level
include(cmake/ProjectIsTopLevel.cmake)
include(cmake/OptionVariables.cmake)

include(GNUInstallDirs)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
  endif()
endif()

if(CPPTRACE_SANITIZER_BUILD)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

if(NOT "${CPPTRACE_BACKTRACE_PATH}" STREQUAL "")
  # quotes used over <> because of a macro substitution issue where
  # </usr/lib/gcc/x86_64-linux-gnu/12/include/backtrace.h>
  # is expanded to
  # </usr/lib/gcc/x86_64-1-gnu/12/include/backtrace.h>
  string(CONCAT CPPTRACE_BACKTRACE_PATH "\"" ${CPPTRACE_BACKTRACE_PATH})
  string(CONCAT CPPTRACE_BACKTRACE_PATH ${CPPTRACE_BACKTRACE_PATH} "\"")
  #message(STATUS ${CPPTRACE_BACKTRACE_PATH})
  string(CONCAT CPPTRACE_BACKTRACE_PATH_DEFINITION "-DCPPTRACE_BACKTRACE_PATH=" ${CPPTRACE_BACKTRACE_PATH})
  #message(STATUS ${CPPTRACE_BACKTRACE_PATH_DEFINITION})
else()
  set(CPPTRACE_BACKTRACE_PATH_DEFINITION "")
endif()

# =============================================== Platform Support ===============================================
function(check_support var source includes libraries definitions)
  set(CMAKE_REQUIRED_INCLUDES "${includes}")
  list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  set(CMAKE_REQUIRED_LIBRARIES "${libraries}")
  set(CMAKE_REQUIRED_DEFINITIONS "${definitions}")
  string(CONCAT full_source "#include \"${source}\"" ${nonce})
  check_cxx_source_compiles(${full_source} ${var})
  set(${var} ${${var}} PARENT_SCOPE)
endfunction()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  check_support(HAS_CXXABI has_cxxabi.cpp "" "" "")
endif()

if(NOT WIN32)
  check_support(HAS_UNWIND has_unwind.cpp "" "" "")
  check_support(HAS_EXECINFO has_execinfo.cpp "" "" "")
  check_support(HAS_BACKTRACE has_backtrace.cpp "" "backtrace" "${CPPTRACE_BACKTRACE_PATH_DEFINITION}")
  set(STACKTRACE_LINK_LIB "stdc++_libbacktrace")
else()
  check_support(HAS_STACKWALK has_stackwalk.cpp "" "dbghelp" "")
endif()

if(NOT WIN32 OR MINGW)
  check_support(HAS_CXX_EXCEPTION_TYPE has_cxx_exception_type.cpp "" "" "")
endif()

if(UNIX AND NOT APPLE)
  check_support(HAS_DL_FIND_OBJECT has_dl_find_object.cpp "" "" "")
endif()

# =============================================== Autoconfig unwinding ===============================================
# Unwind back-ends
if(
  NOT (
    CPPTRACE_UNWIND_WITH_UNWIND OR
    CPPTRACE_UNWIND_WITH_LIBUNWIND OR
    CPPTRACE_UNWIND_WITH_EXECINFO OR
    CPPTRACE_UNWIND_WITH_WINAPI OR
    CPPTRACE_UNWIND_WITH_DBGHELP OR
    CPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(UNIX)
    if(HAS_UNWIND)
      set(CPPTRACE_UNWIND_WITH_UNWIND On)
      message(STATUS "Cpptrace auto config: Using libgcc unwind for unwinding")
    elseif(HAS_EXECINFO)
      set(CPPTRACE_UNWIND_WITH_EXECINFO On)
      message(STATUS "Cpptrace auto config: Using execinfo.h for unwinding")
    else()
      set(CPPTRACE_UNWIND_WITH_NOTHING On)
      message(FATAL_ERROR "Cpptrace auto config: No unwinding back-end seems to be supported, stack tracing will not work. To compile anyway set CPPTRACE_UNWIND_WITH_NOTHING.")
    endif()
  elseif(MINGW OR WIN32)
    if(HAS_STACKWALK)
      set(CPPTRACE_UNWIND_WITH_DBGHELP On)
      message(STATUS "Cpptrace auto config: Using dbghelp for unwinding")
    else()
      set(CPPTRACE_UNWIND_WITH_WINAPI On)
      message(STATUS "Cpptrace auto config: Using winapi for unwinding")
    endif()
  endif()
else()
  #message(STATUS "MANUAL CONFIG SPECIFIED")
endif()

# =============================================== Autoconfig symbols ===============================================
if(
  NOT (
    CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBDL OR
    CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF OR
    CPPTRACE_GET_SYMBOLS_WITH_DBGHELP OR
    CPPTRACE_GET_SYMBOLS_WITH_NOTHING
  )
)
  if(UNIX)
    message(STATUS "Cpptrace auto config: Using libdwarf for symbols")
    set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF On)
  elseif(MINGW)
    message(STATUS "Cpptrace auto config: Using libdwarf + dbghelp for symbols")
    # Use both dbghelp and libdwarf under mingw: Some files may use pdb symbols, e.g. system dlls like KERNEL32.dll and
    # ntdll.dll at the very least, but also other libraries linked with may have pdb symbols.
    set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF On)
    set(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP On)
  else()
    message(STATUS "Cpptrace auto config: Using dbghelp for symbols")
    set(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP On)
  endif()
endif()

# =============================================== Autoconfig demangling ===============================================
# Handle demangle configuration
if(
  NOT (
    CPPTRACE_DEMANGLE_WITH_CXXABI OR
    CPPTRACE_DEMANGLE_WITH_WINAPI OR
    CPPTRACE_DEMANGLE_WITH_NOTHING
  )
)
  if(HAS_CXXABI)
    message(STATUS "Cpptrace auto config: Using cxxabi for demangling")
    set(CPPTRACE_DEMANGLE_WITH_CXXABI On)
  elseif(WIN32 AND NOT MINGW)
    message(STATUS "Cpptrace auto config: Using dbghelp for demangling")
    set(CPPTRACE_DEMANGLE_WITH_WINAPI On)
  else()
    set(CPPTRACE_DEMANGLE_WITH_NOTHING On)
  endif()
else()
  #message(STATUS "Manual demangling back-end specified")
endif()

# =============================================== Now define the library ===============================================

# Target that we can modify (can't modify ALIAS targets)
# Target name should not be the same as ${PROJECT_NAME}, causes add_subdirectory issues
set(target_name "cpptrace-lib")
add_library(${target_name} ${build_type})

# Alias to cause error at configuration time instead of link time if target is missing
add_library(cpptrace::cpptrace ALIAS ${target_name})

# Add /include files to target
# This is solely for IDE benefit, doesn't affect building
target_sources(
    ${target_name} PRIVATE
    include/cpptrace/cpptrace.hpp
)

# add /src files to target
target_sources(
    ${target_name} PRIVATE
    # src
    src/cpptrace.cpp
    src/demangle/demangle_with_cxxabi.cpp
    src/demangle/demangle_with_winapi.cpp
    src/demangle/demangle_with_nothing.cpp
    src/symbols/symbols_with_addr2line.cpp
    src/symbols/symbols_with_dbghelp.cpp
    src/symbols/symbols_with_dl.cpp
    src/symbols/symbols_with_libbacktrace.cpp
    src/symbols/symbols_with_libdwarf.cpp
    src/symbols/symbols_with_nothing.cpp
    src/symbols/symbols_core.cpp
    src/unwind/unwind_with_execinfo.cpp
    src/unwind/unwind_with_libunwind.cpp
    src/unwind/unwind_with_nothing.cpp
    src/unwind/unwind_with_unwind.cpp
    src/unwind/unwind_with_winapi.cpp
    src/unwind/unwind_with_dbghelp.cpp
)

target_include_directories(
  ${target_name}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cpptrace/>
)

target_compile_options(
  ${target_name}
  PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wundef>
  $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

# ---- Generate Build Info Headers ----

# used in export header generated below
if(NOT CPPTRACE_BUILD_SHARED)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_STATIC_DEFINE)
endif()

# generate header file with export macros prefixed with BASE_NAME
include(GenerateExportHeader)
generate_export_header(
  ${target_name}
  BASE_NAME cpptrace
  EXPORT_FILE_NAME include/cpptrace/cpptrace_export.hpp
)

# ---- Library Properties ----

# Hide all symbols by default
# Use SameMajorVersion versioning for shared library runtime linker lookup
set_target_properties(
    ${target_name} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME "cpptrace"
    OUTPUT_NAME "cpptrace"
)

# Header files generated by CMake
target_include_directories(
    ${target_name} SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
)

# Header files from /include
target_include_directories(
    ${target_name} ${warning_guard} PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

# Require C++11 support
target_compile_features(
    ${target_name}
    PRIVATE cxx_std_11
)

if(APPLE)
  SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
  SET(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

# =============================================== Apply options to build ===============================================

if(HAS_CXX_EXCEPTION_TYPE)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_HAS_CXX_EXCEPTION_TYPE)
endif()

if(HAS_DL_FIND_OBJECT)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_HAS_DL_FIND_OBJECT)
endif()

# Symbols
if(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  if(NOT HAS_BACKTRACE)
    if(NOT "${CPPTRACE_BACKTRACE_PATH}" STREQUAL "")
      message(WARNING "Cpptrace: Using libbacktrace for symbols but libbacktrace doesn't appear installed or configured properly.")
    else()
      message(WARNING "Cpptrace: Using libbacktrace for symbols but libbacktrace doesn't appear installed or configured properly. You may need to specify CPPTRACE_BACKTRACE_PATH.")
    endif()
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  target_link_libraries(${target_name} PRIVATE backtrace ${CMAKE_DL_LIBS})
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_LIBDL)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBDL)
  target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
  # set(CPPTRACE_ADDR2LINE_PATH "" CACHE STRING "Absolute path to the addr2line executable you want to use.")
  # option(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH "" OFF)
  if(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH)
    target_compile_definitions(${target_name} PUBLIC CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH)
  else()
    if("${CPPTRACE_ADDR2LINE_PATH}" STREQUAL "")
      if(APPLE)
        find_program(CPPTRACE_ADDR2LINE_PATH_FINAL atos PATHS ENV PATH REQUIRED)
      else()
        find_program(CPPTRACE_ADDR2LINE_PATH_FINAL addr2line PATHS ENV PATH REQUIRED)
      endif()
    else()
      set(CPPTRACE_ADDR2LINE_PATH_FINAL "${CPPTRACE_ADDR2LINE_PATH}")
    endif()
    message(STATUS "Cpptrace: Using ${CPPTRACE_ADDR2LINE_PATH_FINAL} for addr2line path")
    target_compile_definitions(${target_name} PUBLIC CPPTRACE_ADDR2LINE_PATH="${CPPTRACE_ADDR2LINE_PATH_FINAL}")
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
  if(UNIX)
    target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
  endif()
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF)
  if(CPPTRACE_USE_EXTERNAL_LIBDWARF)
    find_package(libdwarf REQUIRED)
    target_compile_definitions(${target_name} PRIVATE CPPTRACE_USE_EXTERNAL_LIBDWARF)
  else()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(PIC_ALWAYS TRUE)
    set(BUILD_DWARFDUMP FALSE)
    include(FetchContent)
    FetchContent_Declare(
      libdwarf
      # GIT_REPOSITORY https://github.com/jeremy-rifkin/libdwarf-code.git
      # GIT_TAG 6216e185863f41d6f19ab850caabfff7326020d7 # v0.8.0
      # Using a lightweight mirror that's optimized for clone + configure speed
      GIT_REPOSITORY https://github.com/jeremy-rifkin/libdwarf-lite.git
      GIT_TAG c78e984f3abbd20f6e01d6f51819e826b1691f65
      GIT_SHALLOW 1
    )
    FetchContent_MakeAvailable(libdwarf)
  endif()
  if(CPPTRACE_CONAN)
    target_link_libraries(${target_name} PRIVATE libdwarf::libdwarf)
  elseif(CPPTRACE_VCPKG)
    target_link_libraries(${target_name} PRIVATE $<IF:$<TARGET_EXISTS:libdwarf::dwarf-static>,libdwarf::dwarf-static,libdwarf::dwarf-shared>)
  else()
    target_link_libraries(${target_name} PRIVATE libdwarf::dwarf-static)
  endif()
  if(UNIX)
    target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
  endif()
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_link_libraries(${target_name} PRIVATE dbghelp)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
endif()

# Unwinding
if(CPPTRACE_UNWIND_WITH_UNWIND)
  if(NOT HAS_UNWIND)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_UNWIND specified but libgcc unwind doesn't seem to be available.")
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_UNWIND)
endif()

if(CPPTRACE_UNWIND_WITH_LIBUNWIND)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBUNWIND QUIET libunwind)
    if(libunwind_FOUND)
      target_compile_options(${target_name} PRIVATE ${LIBUNWIND_CFLAGS_OTHER})
      target_include_directories(${target_name} PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
      target_link_libraries(${target_name} PRIVATE ${LIBUNWIND_LDFLAGS})
    endif()
  endif()
  if(NOT libunwind_FOUND)
    # set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)
    # set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS ON)
		find_path(LIBUNWIND_INCLUDE_DIR NAMES "libunwind.h")
    find_library(LIBUNWIND NAMES unwind libunwind libunwind8 libunwind.so.8 REQUIRED PATHS "/usr/lib/x86_64-linux-gnu/")
    if(LIBUNWIND)
      set(libunwind_FOUND TRUE)
    endif()
  endif()
  if(NOT libunwind_FOUND)
    # message(FATAL_ERROR "Unable to locate libunwind")
    # Try to link with it if it's where it should be
    # This path can be entered if libunwind was installed via the system package manager, sometimes. I probably messed
    # up the find_library above.
    set(LIBUNWIND_LDFLAGS "-lunwind")
  endif()
  target_compile_options(${target_name} PRIVATE ${LIBUNWIND_CFLAGS_OTHER})
  target_include_directories(${target_name} PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
  target_link_libraries(${target_name} PRIVATE ${LIBUNWIND_LDFLAGS})
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_LIBUNWIND UNW_LOCAL_ONLY)
endif()

if(CPPTRACE_UNWIND_WITH_EXECINFO)
  if(NOT HAS_EXECINFO)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_EXECINFO specified but execinfo.h doesn't seem to be available.")
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_EXECINFO)
endif()

if(CPPTRACE_UNWIND_WITH_WINAPI)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_WINAPI)
endif()

if(CPPTRACE_UNWIND_WITH_DBGHELP)
  if(NOT HAS_STACKWALK)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_DBGHELP specified but dbghelp stackwalk64 doesn't seem to be available.")
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_DBGHELP)
  target_link_libraries(${target_name} PRIVATE dbghelp)
endif()

if(CPPTRACE_UNWIND_WITH_NOTHING)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_UNWIND_WITH_NOTHING)
endif()

# Demangling
if(CPPTRACE_DEMANGLE_WITH_CXXABI)
  if(NOT HAS_CXXABI)
    message(WARNING "Cpptrace: CPPTRACE_DEMANGLE_WITH_CXXABI specified but cxxabi.h doesn't seem to be available.")
  endif()
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_DEMANGLE_WITH_CXXABI)
endif()

if(CPPTRACE_DEMANGLE_WITH_WINAPI)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_DEMANGLE_WITH_WINAPI)
  target_link_libraries(${target_name} PRIVATE dbghelp)
endif()

if(CPPTRACE_DEMANGLE_WITH_NOTHING)
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_DEMANGLE_WITH_NOTHING)
endif()

if(NOT "${CPPTRACE_BACKTRACE_PATH}" STREQUAL "")
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_BACKTRACE_PATH=${CPPTRACE_BACKTRACE_PATH})
endif()

if(NOT "${CPPTRACE_HARD_MAX_FRAMES}" STREQUAL "")
  target_compile_definitions(${target_name} PUBLIC CPPTRACE_HARD_MAX_FRAMES=${CPPTRACE_HARD_MAX_FRAMES})
endif()

# =============================================== Install ===============================================

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/InstallRules.cmake)
endif()

# =============================================== Demo/test ===============================================

if(CPPTRACE_BUILD_TESTING)
  add_executable(test test/test.cpp)
  target_compile_features(test PRIVATE cxx_std_11)
  target_link_libraries(test PRIVATE ${target_name})
  # Clang has been fast to adopt dwarf 5, other tools (e.g. addr2line from binutils) have not
  check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
  if(HAS_DWARF4)
    target_compile_options(test PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
  endif()
  if(CPPTRACE_BUILD_TEST_RDYNAMIC)
    set_property(TARGET test PROPERTY ENABLE_EXPORTS ON)
  endif()
  if(APPLE) # TODO: Temporary
    add_custom_command(
      TARGET test
      POST_BUILD
      COMMAND dsymutil $<TARGET_FILE:test>
    )
  endif()

  add_executable(demo test/demo.cpp)
  target_compile_features(demo PRIVATE cxx_std_11)
  target_link_libraries(demo PRIVATE ${target_name})
  # Clang has been fast to adopt dwarf 5, other tools (e.g. addr2line from binutils) have not
  check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
  if(HAS_DWARF4)
    target_compile_options(demo PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
  endif()
  if(CPPTRACE_BUILD_TEST_RDYNAMIC)
    set_property(TARGET demo PROPERTY ENABLE_EXPORTS ON)
  endif()
  if(APPLE) # TODO: Temporary
    add_custom_command(
      TARGET demo
      POST_BUILD
      COMMAND dsymutil $<TARGET_FILE:demo>
    )
  endif()

  if(UNIX)
    add_executable(signal_demo test/signal_demo.cpp)
    target_compile_features(signal_demo PRIVATE cxx_std_11)
    target_link_libraries(signal_demo PRIVATE ${target_name})

    add_executable(signal_tracer test/signal_tracer.cpp)
    target_compile_features(signal_tracer PRIVATE cxx_std_11)
    target_link_libraries(signal_tracer PRIVATE ${target_name})
  endif()
endif()
