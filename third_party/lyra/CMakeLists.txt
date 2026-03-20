if(CPPTRACE_BUILD_TOOLS)
  set(package_name "lyra")
  declaration_is_unique("${package_name}")
  FetchContent_Declare(
    "${package_name}"
    GIT_SHALLOW    TRUE
    GIT_REPOSITORY "https://github.com/bfgroup/Lyra.git"
    GIT_TAG        "ee3c076fa6b9d64c9d249a21f5b9b5a8dae92cd8"
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND CPPTRACE_THIRD_PARTY_DECLARATIONS lyra)
endif()
