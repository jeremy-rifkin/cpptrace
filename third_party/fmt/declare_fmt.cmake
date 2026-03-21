if(CPPTRACE_BUILD_TOOLS)
  set(package_name "fmt")
  declaration_is_unique("${package_name}")
  FetchContent_Declare(
    "${package_name}"
    GIT_SHALLOW    TRUE
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG        "e69e5f977d458f2650bb346dadf2ad30c5320281" # v10.2.1
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND CPPTRACE_THIRD_PARTY_DECLARATIONS "${package_name}")
endif()
