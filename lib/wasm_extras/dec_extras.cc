// just to get around my stupid vscode
#define __EMSCRIPTEN__ 1

#ifdef __EMSCRIPTEN__
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "wasm_extras.h"

thread_local const emscripten::val Float32Array = emscripten::val::global("Float32Array");
thread_local const emscripten::val UInt8Array = emscripten::val::global("Uint8ClampedArray");
thread_local const emscripten::val UInt16Array = emscripten::val::global("Uint16Array");
thread_local const emscripten::val UInt32Array = emscripten::val::global("Uint32Array");
thread_local const emscripten::val ImageData = emscripten::val::global("ImageData");

template<typename TValueType, const JxlDataType TJXL_TYPE>
inline emscripten::val decode_oneshot(std::string data, const emscripten::val ArrayType) {
  emscripten::val ret = emscripten::val::object();
  std::unique_ptr<JxlDecoder, std::integral_constant<decltype(&JxlDecoderDestroy), JxlDecoderDestroy>> dec(JxlDecoderCreate(nullptr));

  const int PIXEL_COMPONENTS = 4;
  JxlBasicInfo info;
  JxlPixelFormat format = {PIXEL_COMPONENTS, TJXL_TYPE, JXL_NATIVE_ENDIAN, 0};

  std::vector<TValueType> pixels = std::vector<TValueType>();
  std::vector<uint8_t> icc = std::vector<uint8_t>();
  uint32_t xsize = -1;
  uint32_t ysize = -1;

  if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO /*| JXL_DEC_COLOR_ENCODING*/ | JXL_DEC_FULL_IMAGE)) {
    ret.set("error", 1);
    ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderSubscribeEvents");
    return ret;
  }

  JxlDecoderSetInput(dec.get(), (const uint8_t*)data.c_str(), data.length());
  JxlDecoderCloseInput(dec.get());

  while(true)
  {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    switch (status) {
      case JXL_DEC_ERROR:
        ret.set("error", 2);
        ret.set("message", "recived JXL_DEC_ERROR from JxlDecoderProcessInput");
        return ret;

      case JXL_DEC_NEED_MORE_INPUT:
        ret.set("error", 3);
        ret.set("message", "recived JXL_DEC_NEED_MORE_INPUT from JxlDecoderProcessInput, you must provide the full file in bulk!");
        return ret;

      case JXL_DEC_BASIC_INFO:
        status = JxlDecoderGetBasicInfo(dec.get(), &info);
        if (status != JXL_DEC_SUCCESS) {
          ret.set("error", 4);
          ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderGetBasicInfo");
          return ret;
        }
        xsize = info.xsize;
        ysize = info.ysize;
        break;

      case JXL_DEC_COLOR_ENCODING:
        size_t icc_size;
        status = JxlDecoderGetICCProfileSize(dec.get(), &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size);
        if(status != JXL_DEC_SUCCESS)
        {
          ret.set("error", 5);
          ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderGetICCProfileSize");
          return ret;
        }
        icc.resize(icc_size);
        status = JxlDecoderGetColorAsICCProfile(dec.get(), &format, JXL_COLOR_PROFILE_TARGET_DATA, icc.data(), icc.size());
        if(status != JXL_DEC_SUCCESS)
        {
          ret.set("error", 6);
          ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderGetColorAsICCProfile");
          return ret;
        }
      break;

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
        size_t buffer_size;
        if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
          ret.set("error", 7);
          ret.set("message", "did not recive JXL_DEC_SUCCESS from  JxlDecoderImageOutBufferSize");
          return ret;
        }
        if (buffer_size != xsize * ysize * PIXEL_COMPONENTS * sizeof(TValueType))
        {
          ret.set("error", 8);
          ret.set("message", "invalid buffer size from JxlDecoderImageOutBufferSize");
          return ret;
        }
        pixels.resize(xsize * ysize * PIXEL_COMPONENTS);
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, (void*)pixels.data(), pixels.size() * sizeof(TValueType))) {
          ret.set("error", 9);
          ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderSetImageOutBuffer");
          return ret;
        }
        break;

      case JXL_DEC_FULL_IMAGE:
        // do nothing
        break;

      case JXL_DEC_SUCCESS:
        ret.set("error", 0);
        ret.set("data", ArrayType.new_(emscripten::typed_memory_view(pixels.size(), pixels.data())));
        ret.set("sizeX", xsize);
        ret.set("sizeY", ysize);
        ret.set("icc", UInt8Array.new_(emscripten::typed_memory_view(icc.size(), icc.data())));
        return ret;
        break;

      default:
        ret.set("error", 10);
        ret.set("message", "unknown status code recived by JxlDecoderProcessInput");
        return ret;
    }
  }
}


//emscripten::val decode_oneshot_boolean(std::string data) { return decode_oneshot<bool, JXL_TYPE_BOOLEAN>(data, BooleanArray); }
//emscripten::val decode_oneshot_float16(std::string data) { return decode_oneshot<float16_t, JXL_TYPE_FLOAT16>(data, Float16Array); }
emscripten::val decode_oneshot_float32(std::string data) { return decode_oneshot<float, JXL_TYPE_FLOAT>(data, Float32Array); }
emscripten::val decode_oneshot_uint8(std::string data) { return decode_oneshot<uint8_t, JXL_TYPE_UINT8>(data, UInt8Array); }
emscripten::val decode_oneshot_uint16(std::string data) { return decode_oneshot<uint16_t, JXL_TYPE_UINT16>(data, UInt16Array); }
emscripten::val decode_oneshot_uint32(std::string data) { return decode_oneshot<uint32_t, JXL_TYPE_UINT32>(data, UInt32Array); }

EMSCRIPTEN_BINDINGS(my_module) {

  // default decode should be uint8, most images will decode perfectly well using this
  emscripten::function("decode_oneshot", &decode_oneshot_uint8);
  //emscripten::function("decode_oneshot_boolean", &decode_oneshot_boolean);
  //emscripten::function("decode_oneshot_float16", &decode_oneshot_float16);
  emscripten::function("decode_oneshot_float32", &decode_oneshot_float32);
  emscripten::function("decode_oneshot_uint8", &decode_oneshot_uint8);
  emscripten::function("decode_oneshot_uint16", &decode_oneshot_uint16);
  emscripten::function("decode_oneshot_uint32", &decode_oneshot_uint32);


  emscripten_run_script("let v=async function(){if(typeof libjxl!=='undefined'){if(typeof libjxl.on_load==='function'){let x=libjxl.on_load;libjxl=Module;libjxl.loaded=true;x();}else{libjxl=Module;libjxl.loaded=true;}}else{libjxl=Module;libjxl.loaded=true;}};v()");
}

#endif