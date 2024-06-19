include(CTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

macro(add_test_dependencies exec_name)
  target_compile_features(${exec_name} PRIVATE cxx_std_11)
  target_link_libraries(${exec_name} PRIVATE ${target_name})
  target_compile_options(${exec_name} PRIVATE ${warning_options})
  # Clang has been fast to adopt dwarf 5, other tools (e.g. addr2line from binutils) have not
  check_cxx_compiler_flag("-gdwarf-4" HAS_DWARF4)
  if(HAS_DWARF4)
    target_compile_options(${exec_name} PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
  endif()
  # TODO: add debug info for mingw clang?
  if(CPPTRACE_BUILD_TEST_RDYNAMIC)
    set_property(TARGET ${exec_name} PROPERTY ENABLE_EXPORTS ON)
  endif()
endmacro()


add_executable(integration integration.cpp)
add_executable(demo demo.cpp)
add_executable(c_demo ctrace_demo.c)

add_test_dependencies(integration)
add_test_dependencies(demo)
add_test_dependencies(c_demo)

if(UNIX)
  add_executable(signal_demo signal_demo.cpp)
  target_compile_features(signal_demo PRIVATE cxx_std_11)
  target_link_libraries(signal_demo PRIVATE ${target_name})

  add_executable(signal_tracer signal_tracer.cpp)
  target_compile_features(signal_tracer PRIVATE cxx_std_11)
  target_link_libraries(signal_tracer PRIVATE ${target_name})
endif()

# primarily a workaround for github actions issue https://github.com/actions/runner-images/issues/8659
if(NOT CPPTRACE_SKIP_UNIT)
  if(CPPTRACE_USE_EXTERNAL_GTEST)
    find_package(GTest)
  else()
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY "https://github.com/google/googletest.git"
      GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(
    unittest
    unit/raw_trace.cpp
    unit/object_trace.cpp
    unit/stacktrace.cpp
  )
  target_compile_features(unittest PRIVATE cxx_std_20)
  target_link_libraries(unittest PRIVATE ${target_name} GTest::gtest_main GTest::gmock_main)
  target_compile_options(unittest PRIVATE ${warning_options} $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-pedantic -Wno-attributes>)
  add_test(NAME unittest COMMAND unittest)
endif()
