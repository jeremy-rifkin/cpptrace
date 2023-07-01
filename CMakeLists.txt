cmake_minimum_required(VERSION 3.8...3.23)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  libcpptrace
  VERSION 1.0.0
  LANGUAGES CXX
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CheckCXXSourceCompiles)

file(GLOB_RECURSE sources src/*.cpp)
#message(STATUS "Sources" ${sources})
add_library(cpptrace ${sources} include/cpptrace/cpptrace.hpp)

target_include_directories(
  cpptrace
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cpptrace/cpptrace>
)

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
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

function(check_support var source includes libraries definitions)
  set(CMAKE_REQUIRED_INCLUDES "${includes}")
  list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  set(CMAKE_REQUIRED_LIBRARIES "${libraries}")
  set(CMAKE_REQUIRED_DEFINITIONS "${definitions}")
  check_cxx_source_compiles("#include \"${source}\"" ${var})
  set(${var} ${${var}} PARENT_SCOPE)
endfunction()

option(LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE "" OFF)

option(LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE "" OFF)
option(LIBCPPTRACE_GET_SYMBOLS_WITH_LIBDL "" OFF)
option(LIBCPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE "" OFF)
option(LIBCPPTRACE_GET_SYMBOLS_WITH_DBGHELP "" OFF)
option(LIBCPPTRACE_GET_SYMBOLS_WITH_NOTHING "" OFF)

option(LIBCPPTRACE_UNWIND_WITH_EXECINFO "" OFF)
option(LIBCPPTRACE_UNWIND_WITH_WINAPI "" OFF)
option(LIBCPPTRACE_UNWIND_WITH_NOTHING "" OFF)

option(LIBCPPTRACE_DEMANGLE_WITH_CXXABI "" OFF)
option(LIBCPPTRACE_DEMANGLE_WITH_NOTHING "" OFF)

# =============================================== Autoconfig full dump ===============================================
# If nothing is specified, attempt to use libbacktrace's full dump
if(
  NOT (
    LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE OR
    LIBCPPTRACE_GET_SYMBOLS_WITH_NOTHING OR
    LIBCPPTRACE_UNWIND_WITH_EXECINFO OR
    LIBCPPTRACE_UNWIND_WITH_WINAPI OR
    LIBCPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  check_support(HAS_BACKTRACE has_backtrace.cpp "" "backtrace" "")

  if(HAS_BACKTRACE)
    set(LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
    message(STATUS "Cpptrace auto config: Using libbacktrace for the full trace")
  endif()
endif()

# =============================================== Autoconfig unwinding ===============================================
# Unwind back-ends (If not doing LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
if(
  NOT (
    LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    LIBCPPTRACE_UNWIND_WITH_EXECINFO OR
    LIBCPPTRACE_UNWIND_WITH_WINAPI OR
    LIBCPPTRACE_UNWIND_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(UNIX)
    check_support(HAS_EXECINFO has_execinfo.cpp "" "" "")

    if(HAS_EXECINFO)
      set(LIBCPPTRACE_UNWIND_WITH_EXECINFO On)
      message(STATUS "Cpptrace auto config: Using execinfo.h for unwinding")
    else()
      set(LIBCPPTRACE_UNWIND_WITH_NOTHING On)
      message(WARNING "Cpptrace auto config: <execinfo.h> doesn't seem to be supported, stack tracing will not work.")
    endif()
  elseif(WIN32)
    set(LIBCPPTRACE_UNWIND_WITH_WINAPI On)
    message(STATUS "Cpptrace auto config: Using winapi for unwinding")
  endif()
else()
  #message(STATUS "MANUAL CONFIG SPECIFIED")
endif()

# =============================================== Autoconfig symbols ===============================================
# Symbol back-ends (If not doing LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
if(
  NOT (
    LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE OR
    LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE OR
    LIBCPPTRACE_GET_SYMBOLS_WITH_NOTHING
  )
)
  # Attempt to auto-config
  if(UNIX)
    check_support(HAS_EXECINFO has_execinfo.cpp "" "" "")

    if(HAS_EXECINFO)
      set(LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE On)
      message(STATUS "Cpptrace auto config: Using libbacktrace for symbols")
    else()
      message(WARNING "Cpptrace auto config: No symbol back end could be automatically configured")
    endif()
  elseif(WIN32)
    #set(LIBCPPTRACE_UNWIND_WITH_WINAPI On)
    #message(STATUS "Cpptrace auto config: Using winapi for unwinding")
  endif()
else()
  #message(STATUS "MANUAL CONFIG SPECIFIED")
endif()

# =============================================== Autoconfig demangling ===============================================
# Handle demangle configuration
if(
  NOT (
    LIBCPPTRACE_DEMANGLE_WITH_CXXABI OR
    LIBCPPTRACE_DEMANGLE_WITH_NOTHING
  )
)
  check_support(HAS_CXXABI has_cxxabi.cpp "" "" "")
  if(HAS_CXXABI)
    set(LIBCPPTRACE_DEMANGLE_WITH_CXXABI On)
  else()
    set(LIBCPPTRACE_DEMANGLE_WITH_NOTHING On)
  endif()
else()
  #message(STATUS "Manual demangling back-end specified")
endif()

# =============================================== Apply options to build ===============================================

# Full
if(LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE)
  target_link_libraries(cpptrace PRIVATE backtrace)
endif()

# Symbols
if(LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE)
endif()

# Unwinding
if(LIBCPPTRACE_UNWIND_WITH_EXECINFO)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_UNWIND_WITH_EXECINFO)
endif()

if(LIBCPPTRACE_UNWIND_WITH_WINAPI)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_UNWIND_WITH_WINAPI)
endif()

# Demangling
if(LIBCPPTRACE_DEMANGLE_WITH_CXXABI)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_DEMANGLE_WITH_CXXABI)
endif()

if(LIBCPPTRACE_DEMANGLE_WITH_NOTHING)
  target_compile_definitions(cpptrace PUBLIC LIBCPPTRACE_DEMANGLE_WITH_NOTHING)
endif()

target_link_libraries(
  cpptrace
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:dbghelp>
  ${CMAKE_DL_LIBS}
)

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
    include/cpptrace.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpptrace/cpptrace
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
