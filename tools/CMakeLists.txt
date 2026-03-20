set(
  COMMON_LIBS
  cpptrace::cpptrace
  bfg::lyra
  fmt::fmt
)

function(binary TARGET)
  cmake_parse_arguments(BINARY "" "" "SOURCES;LIBS;FLAGS;DEFS" ${ARGN})
  add_executable(${TARGET} main.cpp)
  if(BINARY_SOURCES)
    add_library(${TARGET}_OBJ OBJECT ${BINARY_SOURCES})
    target_link_libraries(${TARGET}_OBJ PUBLIC ${COMMON_LIBS})
  endif()
  target_link_libraries(${TARGET} PUBLIC ${COMMON_LIBS})
  target_link_libraries(${TARGET} PUBLIC ${BINARY_LIBS})
  target_compile_definitions(${TARGET} PUBLIC ${BINARY_DEFS})
  target_compile_options(${TARGET} PUBLIC ${BINARY_FLAGS} ${debug} ${warning_options} $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>)
  target_include_directories(${TARGET} PUBLIC "${PROJECT_SOURCE_DIR}/src")
  target_compile_features(${TARGET} PRIVATE cxx_std_20)
  set_target_properties(
    ${TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endfunction()

add_subdirectory(dwarfdump)
add_subdirectory(symbol_tables)
add_subdirectory(resolver)
