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

struct WasmJxlDecoderStatus{
private:
    JxlDecoderStatus status;
    std::string message;

public:
    WasmJxlDecoderStatus();
    WasmJxlDecoderStatus(JxlDecoderStatus status);
    WasmJxlDecoderStatus(std::string message);


    std::string getStatusString();
    const int getStatusInt();

    bool operator ==(WasmJxlDecoderStatus other);
    bool operator ==(JxlDecoderStatus other);
};

struct WasmJxlDecoder
{
private:
    JxlDecoderPtr decoder;
    WasmJxlDecoderStatus last_status;

public:
    WasmJxlDecoder();


    WasmJxlDecoderStatus getStatus();
    void processInput();
    void subscribeEvents(emscripten::val events);
    void setInput(std::string buffer);
    void closeInput();
    emscripten::val getBasicInfo();
    emscripten::val getColorAsICCProfile(JxlPixelFormat format, JxlColorProfileTarget target);
    emscripten::val getPixelData(JxlPixelFormat format);
};

#endif