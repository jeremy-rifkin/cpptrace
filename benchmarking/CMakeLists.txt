include(CTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(
  warning_options
  ${warning_options} $<$<CXX_COMPILER_ID:GNU>:-Wno-infinite-recursion>
)

add_executable(benchmark_unwinding unwinding.cpp)
target_compile_features(benchmark_unwinding PRIVATE cxx_std_20)
target_link_libraries(benchmark_unwinding PRIVATE ${target_name} benchmark::benchmark)
