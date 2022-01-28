#define __EMSCRIPTEN__ 1

#if __EMSCRIPTEN__

#ifndef WASM_EXTRAS
#error WASM_EXTRAS NOT SET BUT BUILDING WASM_EXTRAS!
#endif

#include "wasm_extras.h"
#include "wasm_extras_internals.h"


void post_load()
{
    emscripten_run_script("console.log('post_load')");
    emscripten_run_script("Module.export = {loaded: true};");
    #if WASM_EXTRAS_ENCODER
    wasm_encoder_post_load();
    #endif
    #if WASM_EXTRAS_DECODER
    wasm_decoder_post_load();
    #endif
    emscripten_run_script(
        "var x=function(){};"
        "if(libjxl!==undefined&&typeof libjxl.onload==='function')"
        "{"
            "x=libjxl.on_load();"
        "}"
        "libjxl=Module.export;"
        "Object.freeze(libjxl);" // prevents the libjxl object from being edited
        "x();"
    );
}

// should handle loading the module, actual functions shall be exposed via {enc/dec}_extras.cc
EMSCRIPTEN_BINDINGS(wasm_extras) {
    emscripten_run_script("console.log('wasm_extras')");
    emscripten::constant("loaded", true);
    emscripten::function("onRuntimeInitialized", post_load);
}

#endif