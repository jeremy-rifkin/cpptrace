if(CPPTRACE_SKIP_UNIT)
  return()
endif()

if(CPPTRACE_USE_EXTERNAL_GTEST)
  message(STATUS "Using external GoogleTest")
  find_package(GTest REQUIRED)
else()
  set(package_name "googletest")
  declaration_is_unique("${package_name}")
  FetchContent_Declare(
    "${package_name}"
    GIT_REPOSITORY "https://github.com/google/googletest.git"
    GIT_TAG        tags/release-1.12.1 # last to support C++11
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND CPPTRACE_THIRD_PARTY_DECLARATIONS googletest)
endif()
