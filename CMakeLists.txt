cmake_minimum_required(VERSION 3.5)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  cpptrace
  VERSION 0.2.1
  LANGUAGES C CXX
)

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

option(CPPTRACE_STATIC "" OFF)

option(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_LIBDL "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_NOTHING "" OFF)

option(CPPTRACE_UNWIND_WITH_UNWIND "" OFF)
option(CPPTRACE_UNWIND_WITH_EXECINFO "" OFF)
option(CPPTRACE_UNWIND_WITH_WINAPI "" OFF)
option(CPPTRACE_UNWIND_WITH_DBGHELP "" OFF)
option(CPPTRACE_UNWIND_WITH_NOTHING "" OFF)

option(CPPTRACE_DEMANGLE_WITH_CXXABI "" OFF)
option(CPPTRACE_DEMANGLE_WITH_WINAPI "" OFF)
option(CPPTRACE_DEMANGLE_WITH_NOTHING "" OFF)

set(CPPTRACE_BACKTRACE_PATH "" CACHE STRING "Path to backtrace.h, if the compiler doesn't already know it. Check /usr/lib/gcc/x86_64-linux-gnu/*/include.")
set(CPPTRACE_HARD_MAX_FRAMES "" CACHE STRING "Hard limit on unwinding depth. Default is 100.")
set(CPPTRACE_ADDR2LINE_PATH "" CACHE STRING "Absolute path to the addr2line executable you want to use.")
option(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH "" OFF)

option(CPPTRACE_BUILD_TEST "" OFF)
option(CPPTRACE_BUILD_DEMO "" OFF)
option(CPPTRACE_BUILD_TEST_RDYNAMIC "" OFF)

option(CPPTRACE_USE_SYSTEM_LIBDWARF "" OFF)
option(CPPTRACE_SANITIZER_BUILD "" OFF)

mark_as_advanced(
  CPPTRACE_BACKTRACE_PATH
  CPPTRACE_ADDR2LINE_PATH
  CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH
  CPPTRACE_BUILD_TEST
  CPPTRACE_BUILD_DEMO
  CPPTRACE_BUILD_TEST_RDYNAMIC
  CPPTRACE_USE_SYSTEM_LIBDWARF
  CPPTRACE_SANITIZER_BUILD
)

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

# =============================================== Autoconfig unwinding ===============================================
# Unwind back-ends
if(
  NOT (
    CPPTRACE_UNWIND_WITH_UNWIND OR
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

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(
  sources
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
  src/unwind/unwind_with_nothing.cpp
  src/unwind/unwind_with_unwind.cpp
  src/unwind/unwind_with_winapi.cpp
  src/unwind/unwind_with_dbghelp.cpp
)

if(CPPTRACE_STATIC)
  add_library(cpptrace STATIC ${sources} include/cpptrace/cpptrace.hpp)
else()
  add_library(cpptrace SHARED ${sources} include/cpptrace/cpptrace.hpp)
  set_property(TARGET cpptrace PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(
  cpptrace
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cpptrace/>
)

# TODO
target_compile_features(cpptrace PRIVATE cxx_range_for cxx_constexpr cxx_nullptr cxx_static_assert)

set_target_properties(
  cpptrace
  PROPERTIES
  CXX_STANDARD_REQUIRED TRUE
  CXX_EXTENSIONS OFF
)

target_compile_options(
  cpptrace
  PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wundef>
  $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

# =============================================== Apply options to build ===============================================

function(check_backtrace_error)
  if(NOT HAS_BACKTRACE)
    if(NOT "${CPPTRACE_BACKTRACE_PATH}" STREQUAL "")
      message(WARNING "Cpptrace: CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE specified but libbacktrace doesn't appear installed or configured properly.")
    else()
      message(WARNING "Cpptrace: CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE specified but libbacktrace doesn't appear installed or configured properly. You may need to specify CPPTRACE_BACKTRACE_PATH.")
    endif()
  endif()
endfunction()

# Symbols
if(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  check_backtrace_error()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  target_link_libraries(cpptrace PRIVATE backtrace)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_LIBDL)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBDL)
  target_link_libraries(cpptrace PRIVATE dl)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
  # set(CPPTRACE_ADDR2LINE_PATH "" CACHE STRING "Absolute path to the addr2line executable you want to use.")
  # option(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH "" OFF)
  if(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH)
    target_compile_definitions(cpptrace PUBLIC CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH)
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
    target_compile_definitions(cpptrace PUBLIC CPPTRACE_ADDR2LINE_PATH="${CPPTRACE_ADDR2LINE_PATH_FINAL}")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
  if(UNIX)
    target_link_libraries(cpptrace PRIVATE dl)
  endif()
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF)
  if(CPPTRACE_USE_SYSTEM_LIBDWARF)
    find_package(libdwarf REQUIRED)
    target_link_libraries(cpptrace PRIVATE libdwarf)
  else()
    add_subdirectory(bundled/libdwarf)
    target_link_libraries(cpptrace PRIVATE dwarf)
  endif()
  if(UNIX)
    target_link_libraries(cpptrace PRIVATE dl)
  endif()
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_link_libraries(cpptrace PRIVATE dbghelp)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
endif()

# Unwinding
if(CPPTRACE_UNWIND_WITH_UNWIND)
  if(NOT HAS_UNWIND)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_UNWIND specified but libgcc unwind doesn't seem to be available.")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_UNWIND_WITH_UNWIND)
endif()

if(CPPTRACE_UNWIND_WITH_EXECINFO)
  if(NOT HAS_EXECINFO)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_EXECINFO specified but execinfo.h doesn't seem to be available.")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_UNWIND_WITH_EXECINFO)
endif()

if(CPPTRACE_UNWIND_WITH_WINAPI)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_UNWIND_WITH_WINAPI)
endif()

if(CPPTRACE_UNWIND_WITH_DBGHELP)
  if(NOT HAS_STACKWALK)
    message(WARNING "Cpptrace: CPPTRACE_UNWIND_WITH_DBGHELP specified but dbghelp stackwalk64 doesn't seem to be available.")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_UNWIND_WITH_DBGHELP)
  target_link_libraries(cpptrace PRIVATE dbghelp)
endif()

if(CPPTRACE_UNWIND_WITH_NOTHING)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_UNWIND_WITH_NOTHING)
endif()

# Demangling
if(CPPTRACE_DEMANGLE_WITH_CXXABI)
  if(NOT HAS_CXXABI)
    message(WARNING "Cpptrace: CPPTRACE_DEMANGLE_WITH_CXXABI specified but cxxabi.h doesn't seem to be available.")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_DEMANGLE_WITH_CXXABI)
endif()

if(CPPTRACE_DEMANGLE_WITH_WINAPI)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_DEMANGLE_WITH_WINAPI)
  target_link_libraries(cpptrace PRIVATE dbghelp)
endif()

if(CPPTRACE_DEMANGLE_WITH_NOTHING)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_DEMANGLE_WITH_NOTHING)
endif()

if(NOT "${CPPTRACE_BACKTRACE_PATH}" STREQUAL "")
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_BACKTRACE_PATH=${CPPTRACE_BACKTRACE_PATH})
endif()

if(NOT "${CPPTRACE_HARD_MAX_FRAMES}" STREQUAL "")
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_HARD_MAX_FRAMES=${CPPTRACE_HARD_MAX_FRAMES})
endif()

# ======================================================================================================================

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(CMakePackageConfigHelpers)

  if(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF)
    set(CPPTRACE_STATIC_EXPORT_TARGETS cpptrace dwarf)
  else()
    set(CPPTRACE_STATIC_EXPORT_TARGETS cpptrace)
  endif()

  if(CPPTRACE_STATIC)
    install(
      TARGETS ${CPPTRACE_STATIC_EXPORT_TARGETS}
      EXPORT cpptrace_targets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  else()
    install(
      TARGETS cpptrace
      EXPORT cpptrace_targets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  endif()

  install(
    FILES
    include/cpptrace/cpptrace.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpptrace
  )

  export(
    EXPORT cpptrace_targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/cpptrace/cpptrace_targets.cmake
    NAMESPACE cpptrace::
  )

  configure_package_config_file(
    cmake/cpptrace-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpptrace/cpptrace-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpptrace
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpptrace/cpptrace-config-version.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    EXPORT cpptrace_targets
    FILE cpptrace_targets.cmake
    NAMESPACE cpptrace::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpptrace
  )

  install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpptrace/cpptrace-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpptrace/cpptrace-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpptrace
  )
endif()

if(CPPTRACE_BUILD_TEST)
  add_executable(test test/test.cpp)
  target_compile_features(test PRIVATE cxx_range_for cxx_constexpr cxx_nullptr cxx_static_assert)
  target_link_libraries(test PRIVATE cpptrace)
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
endif()

if(CPPTRACE_BUILD_DEMO)
  add_executable(demo test/demo.cpp)
  target_compile_features(demo PRIVATE cxx_range_for cxx_constexpr cxx_nullptr cxx_static_assert)
  target_link_libraries(demo PRIVATE cpptrace)
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
endif()
