# Quick Overview:
#   this just creates 2 executable compile endpoints with the main source file being wasm_extras/{dec/enc}_extras.cc
#   this is to allow emscripten to properly bind and preserve the exports
#   while keeping the usual libjxl compiles independent

# ensure that CXX_NO_RTTI_SUPPORTED is not true here, should be disabled before processing this file, but just in case..
if (CXX_NO_RTTI_SUPPORTED)
  message( FATAL_ERROR, "-fno-rtti SET FOR WASM BUILD")
endif()

#current configuration:
# Decoder and Encoder will contain Common Files
# Encoder will contain Decoder

#source files
set(WASM_EXTRAS_FILES
  wasm_extras/wasm_extras.cc
)
set(WASM_EXTRAS_DECODER_FILES
  ${WASM_EXTRAS_FILES}
  wasm_extras/dec_extras.cc
)
set(WASM_EXTRAS_ENCODER_FILES
  ${WASM_EXTRAS_DECODER_FILES}
  wasm_extras/enc_extras.cc
)

#definitions
set(WASM_EXTRAS_DEFINITIONS
  "WASM_EXTRAS"
)
set(WASM_EXTRAS_DECODER_DEFINITIONS
  ${WASM_EXTRAS_DEFINITIONS}
  "WASM_EXTRAS_DECODER"
)
set(WASM_EXTRAS_ENCODER_DEFINITIONS
  ${WASM_EXTRAS_DECODER_DEFINITIONS}
  "WASM_EXTRAS_ENCODER"
)


# decoder
add_executable(client_wasm_dec ${WASM_EXTRAS_DECODER_FILES})
target_compile_options(client_wasm_dec PRIVATE "--bind")
target_compile_definitions(client_wasm_dec PRIVATE ${WASM_EXTRAS_DECODER_DEFINITIONS})
set_target_properties(client_wasm_dec PROPERTIES LINK_FLAGS "--bind -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1")
target_link_libraries(client_wasm_dec jxl_dec jxl_threads)
target_include_directories(client_wasm_dec PUBLIC
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
set_target_properties(client_wasm_dec PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
)

# encoder 
if(false) # currently disabled, as i am testing strictly the decoder!
add_executable(client_wasm_enc ${WASM_EXTRAS_ENCODER_FILES})
target_compile_definitions(client_wasm_enc PRIVATE ${WASM_EXTRAS_ENCODER_DEFINITIONS})
set_target_properties(client_wasm_enc PROPERTIES LINK_FLAGS "--bind -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1")
target_compile_options(client_wasm_enc PRIVATE "--bind -DWASM_EXTRAS_DECODER -DWASM_EXTRAS_ENCODER")
target_link_libraries(client_wasm_enc jxl jxl_threads)
target_include_directories(client_wasm_enc PUBLIC
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")
set_target_properties(client_wasm_enc PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm_extras/"
)
endif()