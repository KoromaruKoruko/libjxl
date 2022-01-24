# Quick Overview:
#   this just creates 2 executable compile endpoints with the main source file being wasm_extras/{dec/enc}_extras.cc
#   this is to allow emscripten to properly bind and preserve the exports
#   while keeping the usual libjxl compiles independent

# ensure that CXX_NO_RTTI_SUPPORTED is not true here, should be disabled before processing this file, but just in case..
if (CXX_NO_RTTI_SUPPORTED)
  message( FATAL_ERROR, "-fno-rtti SET FOR WASM BUILD")
endif()


add_executable(client_wasm_dec wasm_extras/dec_extras.cc)
target_compile_options(client_wasm_dec PRIVATE --bind)
set_target_properties(client_wasm_dec PROPERTIES LINK_FLAGS "--bind -s ALLOW_MEMORY_GROWTH=1")
target_link_libraries(client_wasm_dec jxl_dec jxl_threads)
target_include_directories(client_wasm_dec PUBLIC
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")

set_target_properties(client_wasm_dec
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
)


# currently there is no code for the wasm_extras encoder
#add_executable(client_wasm_enc wasm_extras/enc_extras.cc)
#set_target_properties(client_wasm_enc PROPERTIES LINK_FLAGS "--bind -s ALLOW_MEMORY_GROWTH=1")
#target_compile_options(client_wasm_enc PRIVATE --bind)
#target_link_libraries(client_wasm_enc jxl jxl_threads)
#target_include_directories(client_wasm_enc PUBLIC
#  "${PROJECT_SOURCE_DIR}"
#  "${CMAKE_CURRENT_SOURCE_DIR}/include"
#  "${CMAKE_CURRENT_BINARY_DIR}/include")
#set_target_properties(client_wasm_enc
#  PROPERTIES
#  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
#  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
#  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
#)