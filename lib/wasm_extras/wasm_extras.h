#ifndef _WASM_EXTRAS_H_
#define _WASM_EXTRAS_H_

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include <emscripten/wire.h>
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "dec_extras.h"

extern thread_local const emscripten::val JsFloat32Array;
extern thread_local const emscripten::val JsUInt8Array;
extern thread_local const emscripten::val JsUInt16Array;
extern thread_local const emscripten::val JsUInt32Array;
extern thread_local const emscripten::val JsImageData;

extern const std::map<JxlExtraChannelType, const char*> JxlExtraChannelTypeStrings;
extern const std::map<JxlBlendMode, const char*> JxlBlendModeStrings;
extern const std::map<JxlOrientation, const char*> JxlOrientationStrings;

emscripten::val JxlOrientationToJs(JxlOrientation value);
emscripten::val JxlPreviewHeaderToJs(JxlPreviewHeader value);
emscripten::val JxlIntrinsicSizeHeaderToJs(JxlIntrinsicSizeHeader value);
emscripten::val JxlAnimationHeaderToJs(JxlAnimationHeader value);
emscripten::val JxlBasicInfoToJs(JxlBasicInfo value);
emscripten::val JxlExtraChannelInfoToJs(JxlExtraChannelInfo value);
emscripten::val JxlHeaderExtensionsToJs(JxlHeaderExtensions value);
emscripten::val JxlBlendInfoToJs(JxlBlendInfo value);
emscripten::val JxlLayerInfoToJs(JxlLayerInfo value);
emscripten::val JxlFrameHeaderToJs(JxlFrameHeader value);

#endif