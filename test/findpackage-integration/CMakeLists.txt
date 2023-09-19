cmake_minimum_required(VERSION 3.8)

project(demo_project VERSION 0.0.1 LANGUAGES CXX)

add_executable(main main.cpp)

find_package(cpptrace REQUIRED)
target_link_libraries(main cpptrace::cpptrace)
target_compile_features(main PRIVATE cxx_std_11)
