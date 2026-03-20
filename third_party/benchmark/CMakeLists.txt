if(CPPTRACE_BUILD_BENCHMARKING)
  set(package_name "googlebench")
  declaration_is_unique("${package_name}")
  set(BENCHMARK_ENABLE_TESTING OFF)
  FetchContent_Declare(
    "${package_name}"
    GIT_REPOSITORY "https://github.com/google/benchmark.git"
    GIT_TAG        12235e24652fc7f809373e7c11a5f73c5763fc4c # v1.9.0
    OVERRIDE_FIND_PACKAGE
    SYSTEM
  )

  list(APPEND CPPTRACE_THIRD_PARTY_DECLARATIONS "${package_name}")
endif()
