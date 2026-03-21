include(FetchContent)

set(CPPTRACE_THIRD_PARTY_DECLARATIONS)

function(declaration_is_unique declaration)
  list(FIND "${declaration}" CPPTRACE_THIRD_PARTY_DECLARATIONS declaration_index)
  if(NOT declaration_index EQUAL -1)
    message(FATAL_ERROR "${declaration} was previously added to CPPTRACE_THIRD_PARTY_DECLARATIONS (index ${declaration_index})")
  endif()
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/declare_dependencies.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acquire_dependencies.cmake")
