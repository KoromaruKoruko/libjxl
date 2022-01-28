#ifndef _DEC_EXTRAS_H_
#define _DEC_EXTRAS_H_

#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "wasm_extras.h"

const extern std::map<JxlDecoderStatus, const char*> JXLDecoderStatusStrings;

//emscripten::val decode_oneshot_boolean(std::string data);
//emscripten::val decode_oneshot_float16(std::string data);
emscripten::val decode_oneshot_float32(std::string data);
emscripten::val decode_oneshot_uint8(std::string data);
emscripten::val decode_oneshot_uint16(std::string data);
//emscripten::val decode_oneshot_uint32(std::string data);

typedef struct
{

} WasmJxlDecoder;

#endif