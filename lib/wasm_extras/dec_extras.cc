// just to get around my stupid vscode
#define __EMSCRIPTEN__ 1

#ifdef __EMSCRIPTEN__
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "wasm_extras.h"

thread_local const emscripten::val UInt8Array =
    emscripten::val::global("Uint8ClampedArray");
thread_local const emscripten::val ImageData =
    emscripten::val::global("ImageData");
thread_local const auto JxlRunner = JxlResizableParallelRunnerMake(nullptr);


emscripten::val decode_oneshot_uint8(std::string data) {
  emscripten::val ret = emscripten::val::object();
  std::unique_ptr<JxlDecoder, std::integral_constant<decltype(&JxlDecoderDestroy), JxlDecoderDestroy>> dec(JxlDecoderCreate(nullptr));

  const int PIXEL_COMPONENTS = 4;
  JxlBasicInfo info;
  JxlPixelFormat format = {PIXEL_COMPONENTS, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  std::vector<uint8_t> pixels = std::vector<uint8_t>();
  std::vector<uint8_t> icc_profile = std::vector<uint8_t>();
  uint32_t xsize = -1;
  uint32_t ysize = -1;

  if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO /*| JXL_DEC_COLOR_ENCODING*/ | JXL_DEC_FULL_IMAGE)) {
    ret.set("error", 1);
    ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderSubscribeEvents");
    return ret;
  }
  if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, JxlRunner.get()))
  {
    ret.set("error", 1);
    ret.set("message", "did not recive JXL_DEC_SUCCESS from JxlDecoderSetParallelRunner");
    return ret;
  }

  JxlDecoderSetInput(dec.get(), (const uint8_t*)data.c_str(), data.length());

  while(true)
  {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    switch (status) {
      case JXL_DEC_ERROR:
        ret.set("error", 1);
        ret.set("message", "recived JXL_DEC_ERROR from JxlDecoderProcessInput");
        JxlDecoderCloseInput(dec.get());
        return ret;

      case JXL_DEC_NEED_MORE_INPUT:
        ret.set("error", 2);
        ret.set("message",
                "recived JXL_DEC_NEED_MORE_INPUT from JxlDecoderProcessInput, "
                "you must provide the full file in bulk!");
        JxlDecoderCloseInput(dec.get());
        return ret;

      case JXL_DEC_BASIC_INFO:
        status = JxlDecoderGetBasicInfo(dec.get(), &info);
        if (status != JXL_DEC_SUCCESS) {
          ret.set("error", 3);
          ret.set("message",
                  "did not recive JXL_DEC_SUCCESS from "
                  "JxlDecoderGetBasicInfo");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        xsize = info.xsize;
        ysize = info.ysize;
        JxlResizableParallelRunnerSetThreads( JxlRunner.get(), JxlResizableParallelRunnerSuggestThreads(xsize, ysize));
        break;

      case JXL_DEC_COLOR_ENCODING:
        size_t icc_size;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetICCProfileSize(
                dec.get(), &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
          ret.set("error", 4);
          ret.set("message",
                  "did not recive JXL_DEC_SUCCESS from "
                  "JxlDecoderGetICCProfileSize");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        icc_profile.resize(icc_size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   dec.get(), &format,
                                   JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile.data(), icc_profile.size())) {
          ret.set("error", 5);
          ret.set("message",
                  "did not recive JXL_DEC_SUCCESS from "
                  "JxlDecoderGetColorAsICCProfile");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        break;

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
        size_t buffer_size;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
          ret.set("error", 6);
          ret.set("message",
                  "did not recive JXL_DEC_SUCCESS from "
                  "JxlDecoderImageOutBufferSize");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        if (buffer_size != xsize * ysize * PIXEL_COMPONENTS * sizeof(uint8_t))
        {
          ret.set("error", 7);
          ret.set("message", "invalid buffer size from JxlDecoderImageOutBufferSize");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        pixels.resize(xsize * ysize * PIXEL_COMPONENTS);
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, (void*)pixels.data(), pixels.size() * sizeof(uint8_t))) {
          ret.set("error", 7);
          ret.set("message",
                  "did not recive JXL_DEC_SUCCESS from "
                  "JxlDecoderSetImageOutBuffer");
          JxlDecoderCloseInput(dec.get());
          return ret;
        }
        break;

      case JXL_DEC_FULL_IMAGE:
        // do nothing
        break;

      case JXL_DEC_SUCCESS:
        ret.set("error", 0);
        ret.set("data", ImageData.new_(UInt8Array.new_(emscripten::typed_memory_view(pixels.size(), pixels.data())), xsize, ysize));
        JxlDecoderCloseInput(dec.get());
        return ret;
        break;

      default:
        ret.set("error", 8);
        ret.set("message", "unknown status code recived by JxlDecoderProcessInput");
        JxlDecoderCloseInput(dec.get());
        return ret;
    }
  }
}

EMSCRIPTEN_BINDINGS(my_module) {
  // debug line to ensure the module loads
  // default decode should be uint8, most images will decode perfectly well using this
  //  with the added benefit of extra speed by skiping a transform step, we also appear to not need the icc data
  emscripten::function("decode_oneshot", &decode_oneshot_uint8);
  emscripten::function("decode_oneshot_uint8", &decode_oneshot_uint8);
  emscripten_run_script("let v=async function(){if(typeof libjxl!=='undefined'){if(typeof libjxl.on_load==='function'){let x=libjxl.on_load;libjxl=Module;libjxl.loaded=true;x();}else{libjxl=Module;libjxl.loaded=true;}}else{libjxl=Module;libjxl.loaded=true;}};v()");
}

#endif