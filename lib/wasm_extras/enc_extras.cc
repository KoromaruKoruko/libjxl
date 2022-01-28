#define __EMSCRIPTEN__ 1

#ifdef __EMSCRIPTEN__

#ifndef WASM_EXTRAS_ENCODER
#error WASM_EXTRAS_ENCODER NOT SET BUT BUILDING WASM_EXTRAS_ENCODER!
#endif

#include "enc_extras.h"
#include "wasm_extras_internals.h"

// TODO: Implement Wasm_Extras Encoder
//  compilation has been removed, uncomment the target in /lib/jxl_wasm.cmake

void wasm_encoder_post_load()
{
emscripten_run_script("console.log('wasm_encoder_post_load')");
}

EMSCRIPTEN_BINDINGS(enc_extras) {
    emscripten_run_script("console.log('enc_extras')");
    
}
#endif