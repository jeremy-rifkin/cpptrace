foreach(package IN LISTS CPPTRACE_THIRD_PARTY_DECLARATIONS)
  FetchContent_MakeAvailable(${package})
endforeach()
