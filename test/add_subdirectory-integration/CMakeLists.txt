cmake_minimum_required(VERSION 3.8)

project(demo_project VERSION 0.0.1 LANGUAGES CXX)

add_executable(main main.cpp)

add_subdirectory(cpptrace)
target_link_libraries(main cpptrace)
