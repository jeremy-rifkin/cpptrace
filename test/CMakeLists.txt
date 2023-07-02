cmake_minimum_required(VERSION 3.8...3.23)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  test-libcpptrace
  VERSION 1.0.0
  LANGUAGES CXX
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")

add_executable(test test.cpp)
target_link_libraries(test PRIVATE cpptrace)
target_include_directories(test PRIVATE ../include)
target_link_directories(test PRIVATE ../build)

file(GLOB_RECURSE CPPTRACE_PATHS ../build/cpptrace.dll ../build/libcpptrace.so ../build/**/cpptrace.dll ../build/**/libcpptrace.so)
list(POP_BACK CPPTRACE_PATHS CPPTRACE_PATH)
#find_library(CPPTRACE_PATH NAMES "cpptrace.dll" "libcpptrace.so" PATHS ../build PATH_SUFFIXES Debug Release)
message(STATUS "Copying lib from " ${CPPTRACE_PATH})
file(COPY ${CPPTRACE_PATH} DESTINATION .)
