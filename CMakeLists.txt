cmake_minimum_required(VERSION 3.8...3.23)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  cpptrace
  VERSION 0.0.1
  LANGUAGES CXX
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

file(GLOB_RECURSE sources src/*.cpp)
add_library(cpptrace ${sources} include/cpptrace/cpptrace.hpp)

target_include_directories(
  cpptrace
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cpptrace/cpptrace>
)

# TODO
target_compile_features(
  cpptrace
  PUBLIC
  cxx_std_11
)

set_target_properties(
  cpptrace
  PROPERTIES
  CXX_STANDARD_REQUIRED TRUE
  CXX_EXTENSIONS OFF
)

target_compile_options(
  cpptrace
  PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow -Wundef -Wuseless-cast -Wnonnull-compare>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

option(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE "" OFF)
option(CPPTRACE_FULL_TRACE_WITH_STACKTRACE "" OFF)

option(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_LIBDL "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP "" OFF)
option(CPPTRACE_GET_SYMBOLS_WITH_NOTHING "" OFF)

option(CPPTRACE_UNWIND_WITH_EXECINFO "" OFF)
option(CPPTRACE_UNWIND_WITH_WINAPI "" OFF)
option(CPPTRACE_UNWIND_WITH_NOTHING "" OFF)

option(CPPTRACE_DEMANGLE_WITH_CXXABI "" OFF)
option(CPPTRACE_DEMANGLE_WITH_NOTHING "" OFF)

option(CPPTRACE_BUILD_TEST "" OFF)
option(CPPTRACE_BUILD_TEST_RDYNAMIC "" OFF)

option(CPPTRACE_BUILD_SPEEDTEST "" OFF)
option(CPPTRACE_BUILD_SPEEDTEST_DWARF4 "" OFF)
option(CPPTRACE_BUILD_SPEEDTEST_DWARF5 "" OFF)

option(CPPTRACE_RUN_CLANG_TIDY "" OFF)

set(CPPTRACE_BACKTRACE_PATH "" CACHE STRING "Path to backtrace.h, if the compiler doesn't already know it. Check /usr/lib/gcc/x86_64-linux-gnu/*/include.")
set(CPPTRACE_HARD_MAX_FRAMES "" CACHE STRING "Hard limit on unwinding depth. Default is 100.")

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

check_support(HAS_EXECINFO has_execinfo.cpp "" "" "")
check_support(HAS_BACKTRACE has_backtrace.cpp "" "backtrace" "${CPPTRACE_BACKTRACE_PATH_DEFINITION}")
check_support(HAS_CXXABI has_cxxabi.cpp "" "" "")
if(NOT MSVC)
  set(STACKTRACE_LINK_LIB "stdc++_libbacktrace")
else()
  set(STACKTRACE_LINK_LIB "")
endif()
check_support(HAS_STACKTRACE has_stacktrace.cpp "" "${STACKTRACE_LINK_LIB}" "")

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
    CPPTRACE_UNWIND_WITH_EXECINFO OR
    CPPTRACE_UNWIND_WITH_WINAPI OR
    CPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(HAS_STACKTRACE AND NOT WIN32) # Our trace is better than msvc's <stacktrace>
    set(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
    message(STATUS "Cpptrace auto config: Using C++23 <stacktrace> for the full trace")
  elseif(HAS_BACKTRACE)
    set(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
    message(STATUS "Cpptrace auto config: Using libbacktrace for the full trace")
  endif()
endif()

# =============================================== Autoconfig unwinding ===============================================
# Unwind back-ends (If not doing CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
if(
  NOT (
    CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    CPPTRACE_FULL_TRACE_WITH_STACKTRACE OR
    CPPTRACE_UNWIND_WITH_EXECINFO OR
    CPPTRACE_UNWIND_WITH_WINAPI OR
    CPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(UNIX)
    if(HAS_EXECINFO)
      set(CPPTRACE_UNWIND_WITH_EXECINFO On)
      message(STATUS "Cpptrace auto config: Using execinfo.h for unwinding")
    else()
      set(CPPTRACE_UNWIND_WITH_NOTHING On)
      message(FATAL_ERROR "Cpptrace auto config: <execinfo.h> doesn't seem to be supported, stack tracing will not work. To compile anyway set CPPTRACE_UNWIND_WITH_NOTHING.")
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
  if(APPLE)
    set(CPPTRACE_GET_SYMBOLS_WITH_LIBDL ON)
  elseif(UNIX)
    if(HAS_BACKTRACE)
      set(CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
      message(STATUS "Cpptrace auto config: Using libbacktrace for symbols")
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
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE)
  target_link_libraries(cpptrace PRIVATE dl)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_DBGHELP)
  target_link_libraries(cpptrace PRIVATE dbghelp)
endif()

if(CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
  target_compile_definitions(cpptrace PUBLIC CPPTRACE_GET_SYMBOLS_WITH_NOTHING)
endif()

# Unwinding
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

#target_link_libraries(
#  cpptrace
#  PRIVATE
#  #$<$<CXX_COMPILER_ID:MSVC>:dbghelp>
#  #${CMAKE_DL_LIBS}
#)

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

if(CPPTRACE_RUN_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)
  set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" -extra-arg=-std=c++11)
  set_target_properties(cpptrace PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  target_compile_options(
    cpptrace
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/analyze>
  )
endif()
