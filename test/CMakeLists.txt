cmake_minimum_required(VERSION 3.8...3.23)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  test-libcpptrace
  VERSION 1.0.0
  LANGUAGES CXX
)

add_executable(test test.cpp)
target_link_libraries(test PRIVATE cpptrace)
target_include_directories(test PRIVATE ../include)
target_link_directories(test PRIVATE ../build)
