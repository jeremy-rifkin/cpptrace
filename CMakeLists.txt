cmake_minimum_required(VERSION 3.4)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  cpptrace
  VERSION 0.1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

option(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE "" OFF)
option(CPPTRACE_FULL_TRACE_WITH_STACKTRACE "" OFF)

option(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_LIBDL "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_NOTHING "" OFF)

option(CPPTRACE_UNWIND_WITH_UNWIND "" OFF)
option(CPPTRACE_UNWIND_WITH_EXECINFO "" OFF)
option(CPPTRACE_UNWIND_WITH_WINAPI "" OFF)
option(CPPTRACE_UNWIND_WITH_NOTHING "" OFF)

option(CPPTRACE_DEMANGLE_WITH_CXXABI "" OFF)
option(CPPTRACE_DEMANGLE_WITH_NOTHING "" OFF)

set(CPPTRACE_BACKTRACE_PATH "" CACHE STRING "Path to backtrace.h, if the compiler doesn't already know it. Check /usr/lib/gcc/x86_64-linux-gnu/*/include.")
set(CPPTRACE_HARD_MAX_FRAMES "" CACHE STRING "Hard limit on unwinding depth. Default is 100.")
set(CPPTRACE_ADDR2LINE_PATH "" CACHE STRING "Absolute path to the addr2line executable you want to use.")
option(CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH "" OFF)

option(CPPTRACE_BUILD_TEST "" OFF)
option(CPPTRACE_BUILD_DEMO "" OFF)
option(CPPTRACE_BUILD_TEST_RDYNAMIC "" OFF)

option(CPPTRACE_BUILD_SPEEDTEST "" OFF)
option(CPPTRACE_BUILD_SPEEDTEST_DWARF4 "" OFF)
option(CPPTRACE_BUILD_SPEEDTEST_DWARF5 "" OFF)

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

if(MINGW OR NOT WIN32) # No need to bother checking in msvc, but do check in minngw
 check_support(HAS_UNWIND has_unwind.cpp "" "" "")
 check_support(HAS_EXECINFO has_execinfo.cpp "" "" "")
 check_support(HAS_BACKTRACE has_backtrace.cpp "" "backtrace" "${CPPTRACE_BACKTRACE_PATH_DEFINITION}")
 check_support(HAS_DL has_dl.cpp "" "dl" "")
 check_support(HAS_CXXABI has_cxxabi.cpp "" "" "")
 if(NOT MSVC)
   set(STACKTRACE_LINK_LIB "stdc++_libbacktrace")
 else()
   set(STACKTRACE_LINK_LIB "")
 endif()
 check_support(HAS_STACKTRACE has_stacktrace.cpp "" "${STACKTRACE_LINK_LIB}" "")
 if(APPLE)
    find_program(ADDR2LINE_PATH atos PATHS ENV PATH)
  else()
    find_program(ADDR2LINE_PATH addr2line PATHS ENV PATH)
  endif()
  if("${ADDR2LINE_PATH}" STREQUAL "ADDR2LINE_PATH-NOTFOUND")
    set(HAS_ADDR2LINE FALSE)
  else()
    set(HAS_ADDR2LINE TRUE)
  endif()
endif()

# =============================================== Autoconfig full dump ===============================================
# If nothing is specified, attempt to use libbacktrace's full dump
if(
  NOT (
    CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    CPPTRACE_FULL_TRACE_WITH_STACKTRACE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBDL OR
    CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE OR
    CPPTRACE_GET_SYMBOLS_WITH_DBGHELP OR
    CPPTRACE_GET_SYMBOLS_WITH_NOTHING OR
    CPPTRACE_UNWIND_WITH_UNWIND OR
    CPPTRACE_UNWIND_WITH_EXECINFO OR
    CPPTRACE_UNWIND_WITH_WINAPI OR
    CPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(MINGW OR NOT WIN32) # Our trace is better than msvc's <stacktrace>
    if(HAS_STACKTRACE)
      set(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
      message(STATUS "Cpptrace auto config: Using C++23 <stacktrace> for the full trace")
    elseif(HAS_BACKTRACE AND NOT WIN32) # Mingw libbacktrace doesn't seem to be working
      set(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
      message(STATUS "Cpptrace auto config: Using libbacktrace for the full trace")
    endif()
  endif()
endif()

# =============================================== Autoconfig unwinding ===============================================
# Unwind back-ends (If not doing CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
if(
  NOT (
    CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    CPPTRACE_FULL_TRACE_WITH_STACKTRACE OR
    CPPTRACE_UNWIND_WITH_UNWIND OR
    CPPTRACE_UNWIND_WITH_EXECINFO OR
    CPPTRACE_UNWIND_WITH_WINAPI OR
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
  elseif(MINGW)
    if(HAS_UNWIND)
      set(CPPTRACE_UNWIND_WITH_UNWIND On)
      message(STATUS "Cpptrace auto config: Using libgcc unwind for unwinding")
    elseif(HAS_EXECINFO)
      set(CPPTRACE_UNWIND_WITH_EXECINFO On)
      message(STATUS "Cpptrace auto config: Using execinfo.h for unwinding")
    else()
      set(CPPTRACE_UNWIND_WITH_WINAPI On)
      message(STATUS "Cpptrace auto config: Using winapi for unwinding")
    endif()
  elseif(WIN32)
    set(CPPTRACE_UNWIND_WITH_WINAPI On)
    message(STATUS "Cpptrace auto config: Using winapi for unwinding")
  endif()
else()
  #message(STATUS "MANUAL CONFIG SPECIFIED")
endif()

# =============================================== Autoconfig symbols ===============================================
# Symbol back-ends (If not doing CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
if(
  NOT (
    CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    CPPTRACE_FULL_TRACE_WITH_STACKTRACE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE OR
    CPPTRACE_GET_SYMBOLS_WITH_LIBDL OR
    CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE OR
    CPPTRACE_GET_SYMBOLS_WITH_DBGHELP OR
    CPPTRACE_GET_SYMBOLS_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(UNIX OR MINGW)
    if(HAS_BACKTRACE AND NOT MINGW) # not working on mingw at the moment
      set(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE On)
      message(STATUS "Cpptrace auto config: Using libbacktrace for symbols")
    elseif(HAS_ADDR2LINE)
      set(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE On)
      message(STATUS "Cpptrace auto config: Using addr2line for symbols")
    elseif(HAS_DL)
      set(CPPTRACE_GET_SYMBOLS_WITH_LIBDL On)
      message(STATUS "Cpptrace auto config: Using libdl for symbols")
    else()
      message(FATAL_ERROR "Cpptrace auto config: No symbol back-end could be automatically configured. To compile anyway set CPPTRACE_GET_SYMBOLS_WITH_NOTHING.")
    endif()
  elseif(WIN32)
    set(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP On)
    message(STATUS "Cpptrace auto config: Using dbghelp for symbols")
  endif()
else()
  #message(STATUS "MANUAL CONFIG SPECIFIED")
endif()

# =============================================== Autoconfig demangling ===============================================
# Handle demangle configuration
if(
  NOT (
    CPPTRACE_DEMANGLE_WITH_CXXABI OR
    CPPTRACE_DEMANGLE_WITH_NOTHING
  )
)
  if(HAS_CXXABI)
    set(CPPTRACE_DEMANGLE_WITH_CXXABI On)
  else()
    set(CPPTRACE_DEMANGLE_WITH_NOTHING On)
  endif()
else()
  #message(STATUS "Manual demangling back-end specified")
endif()

# =============================================== Now define the library ===============================================

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(
  sources
  src/cpptrace.cpp
  src/demangle/demangle_with_cxxabi.cpp
  src/demangle/demangle_with_nothing.cpp
  src/full/full_trace_with_libbacktrace.cpp
  src/full/full_trace_with_stacktrace.cpp
  src/symbols/symbols_with_addr2line.cpp
  src/symbols/symbols_with_dbghelp.cpp
  src/symbols/symbols_with_dl.cpp
  src/symbols/symbols_with_libbacktrace.cpp
  src/symbols/symbols_with_nothing.cpp
  src/unwind/unwind_with_execinfo.cpp
  src/unwind/unwind_with_nothing.cpp
  src/unwind/unwind_with_unwind.cpp
  src/unwind/unwind_with_winapi.cpp
)

# TODO: This feels like a hack.
if(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  add_library(cpptrace SHARED ${sources} include/cpptrace/cpptrace.hpp)
else()
  add_library(cpptrace ${sources} include/cpptrace/cpptrace.hpp)
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
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow -Wundef>
  $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast -Wnonnull-compare>
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

# Full
if(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
  check_backtrace_error()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
  target_link_libraries(cpptrace PRIVATE backtrace)
endif()

if(CPPTRACE_FULL_TRACE_WITH_STACKTRACE)
  if(NOT HAS_STACKTRACE)
    message(WARNING "Cpptrace: CPPTRACE_FULL_TRACE_WITH_STACKTRACE specified but <stacktrace> doesn't seem to be available.")
  endif()
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_FULL_TRACE_WITH_STACKTRACE)
  target_link_libraries(cpptrace PRIVATE "${STACKTRACE_LINK_LIB}")
endif()

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

if(CMAKE_BUILD_TYPE STREQUAL "")
  message(FATAL_ERROR "Setting CMAKE_BUILD_TYPE is required")
endif()

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(CMakePackageConfigHelpers)

  install(
    TARGETS cpptrace
    EXPORT cpptrace_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

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
  target_link_libraries(test PRIVATE cpptrace)
  # Clang has been fast to adopt dwarf 5, other tools (e.g. addr2line from binutils) have not
  check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
  if(HAS_DWARF4)
    target_compile_options(test PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
  endif()
  if(CPPTRACE_BUILD_TEST_RDYNAMIC)
    set_property(TARGET test PROPERTY ENABLE_EXPORTS ON)
  endif()
endif()

if(CPPTRACE_BUILD_DEMO)
  add_executable(demo test/demo.cpp)
  target_link_libraries(demo PRIVATE cpptrace)
  # Clang has been fast to adopt dwarf 5, other tools (e.g. addr2line from binutils) have not
  check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
  if(HAS_DWARF4)
    target_compile_options(demo PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
  endif()
  if(CPPTRACE_BUILD_TEST_RDYNAMIC)
    set_property(TARGET demo PROPERTY ENABLE_EXPORTS ON)
  endif()
endif()

if(CPPTRACE_BUILD_SPEEDTEST)
  if(CPPTRACE_BUILD_SPEEDTEST_DWARF4)
    check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
    if(HAS_DWARF4)
      add_compile_options("$<$<CONFIG:Debug>:-gdwarf-4>")
      #target_compile_options(speedtest PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
      #target_compile_options(googletest INTERFACE "$<$<CONFIG:Debug>:-gdwarf-4>")
    endif()
  endif()
  if(CPPTRACE_BUILD_SPEEDTEST_DWARF5)
    check_cxx_compiler_flag("-gdwarf-5" HAS_DWARF5)
    if(HAS_DWARF5)
      add_compile_options("$<$<CONFIG:Debug>:-gdwarf-5>")
      #target_compile_options(speedtest PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
      #target_compile_options(googletest INTERFACE "$<$<CONFIG:Debug>:-gdwarf-4>")
    endif()
  endif()

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(speedtest test/speedtest.cpp)
  target_link_libraries(
    speedtest
    PRIVATE
    GTest::gtest_main
    cpptrace
  )

  if(WIN32)
    add_custom_command(
        TARGET speedtest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:speedtest> $<TARGET_FILE_DIR:speedtest>
        COMMAND_EXPAND_LISTS
    )
  endif()
endif()
