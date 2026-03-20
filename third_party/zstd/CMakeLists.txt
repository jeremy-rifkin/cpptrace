if(CPPTRACE_USE_EXTERNAL_ZSTD)
  find_package(zstd)
else()
  # libdwarf depends on zstd, so it's a logic error for libdwarf to already be in our declaration
  # list.
  declaration_is_unique(libdwarf)

  cmake_policy(SET CMP0074 NEW)
  set(ZSTD_BUILD_PROGRAMS OFF)
  set(ZSTD_BUILD_CONTRIB OFF)
  set(ZSTD_BUILD_TESTS OFF)
  set(ZSTD_BUILD_STATIC ON)
  set(ZSTD_BUILD_SHARED OFF)
  set(ZSTD_LEGACY_SUPPORT OFF)
  set(package_name "zstd")
  declaration_is_unique("${package_name}")
  FetchContent_Declare(
    "${package_name}"
    SOURCE_SUBDIR build/cmake
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL "${CPPTRACE_ZSTD_URL}"
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND CPPTRACE_THIRD_PARTY_DECLARATIONS zstd)
endif()
