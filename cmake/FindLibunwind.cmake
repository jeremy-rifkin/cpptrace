# Find module for libunwind, providing Libunwind::Libunwind

if(APPLE)
  # On Apple, libunwind is built into the system. Headers are available via the SDK, no library is needed.
  set(Libunwind_FOUND TRUE)
  if(NOT TARGET Libunwind::Libunwind)
    add_library(Libunwind::Libunwind INTERFACE IMPORTED)
  endif()
else()
  # Use pkg-config for hints
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(_LIBUNWIND QUIET libunwind)
  endif()

  find_path(Libunwind_INCLUDE_DIR NAMES libunwind.h HINTS ${_LIBUNWIND_INCLUDE_DIRS})
  find_library(Libunwind_LIBRARY NAMES unwind libunwind HINTS ${_LIBUNWIND_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Libunwind DEFAULT_MSG
    Libunwind_LIBRARY Libunwind_INCLUDE_DIR
  )

  if(Libunwind_FOUND)
    message(STATUS "Found Libunwind: ${Libunwind_LIBRARY}")
  endif()

  mark_as_advanced(Libunwind_INCLUDE_DIR Libunwind_LIBRARY)

  if(Libunwind_FOUND AND NOT TARGET Libunwind::Libunwind)
    add_library(Libunwind::Libunwind UNKNOWN IMPORTED)
    set_target_properties(
      Libunwind::Libunwind
      PROPERTIES
      IMPORTED_LOCATION "${Libunwind_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Libunwind_INCLUDE_DIR}"
    )
  endif()
endif()
