#if __EMSCRIPTEN__

#ifndef WASM_EXTRAS
#error WASM_EXTRAS NOT SET BUT BUILDING WASM_EXTRAS!
#endif

#include "wasm_extras.h"
#include "wasm_extras_internals.h"

thread_local const emscripten::val JsFloat32Array = emscripten::val::global("Float32Array");
thread_local const emscripten::val JsUInt8Array = emscripten::val::global("Uint8ClampedArray");
thread_local const emscripten::val JsUInt16Array = emscripten::val::global("Uint16Array");
thread_local const emscripten::val JsUInt32Array = emscripten::val::global("Uint32Array");
thread_local const emscripten::val JsImageData = emscripten::val::global("ImageData");


#define DefineEnumString(name) std::make_pair(name, #name)
const std::map<JxlExtraChannelType, const char*> JxlExtraChannelTypeStrings
{
    DefineEnumString(JXL_CHANNEL_ALPHA),
    DefineEnumString(JXL_CHANNEL_BLACK),
    DefineEnumString(JXL_CHANNEL_CFA),
    DefineEnumString(JXL_CHANNEL_DEPTH),
    DefineEnumString(JXL_CHANNEL_OPTIONAL),
    DefineEnumString(JXL_CHANNEL_RESERVED0),
    DefineEnumString(JXL_CHANNEL_RESERVED1),
    DefineEnumString(JXL_CHANNEL_RESERVED2),
    DefineEnumString(JXL_CHANNEL_RESERVED3),
    DefineEnumString(JXL_CHANNEL_RESERVED4),
    DefineEnumString(JXL_CHANNEL_RESERVED5),
    DefineEnumString(JXL_CHANNEL_RESERVED6),
    DefineEnumString(JXL_CHANNEL_RESERVED7),
    DefineEnumString(JXL_CHANNEL_SELECTION_MASK),
    DefineEnumString(JXL_CHANNEL_SPOT_COLOR),
    DefineEnumString(JXL_CHANNEL_THERMAL),
    DefineEnumString(JXL_CHANNEL_UNKNOWN)
};
const std::map<JxlBlendMode, const char*> JxlBlendModeStrings
{
    DefineEnumString(JXL_BLEND_ADD),
    DefineEnumString(JXL_BLEND_BLEND),
    DefineEnumString(JXL_BLEND_MUL),
    DefineEnumString(JXL_BLEND_MULADD),
    DefineEnumString(JXL_BLEND_REPLACE)
};
const std::map<JxlOrientation, const char*> JxlOrientationStrings
{
    DefineEnumString(JXL_ORIENT_ANTI_TRANSPOSE),
    DefineEnumString(JXL_ORIENT_FLIP_HORIZONTAL),
    DefineEnumString(JXL_ORIENT_FLIP_VERTICAL),
    DefineEnumString(JXL_ORIENT_IDENTITY),
    DefineEnumString(JXL_ORIENT_ROTATE_180),
    DefineEnumString(JXL_ORIENT_ROTATE_90_CCW),
    DefineEnumString(JXL_ORIENT_ROTATE_90_CW),
    DefineEnumString(JXL_ORIENT_TRANSPOSE)
};
const std::map<JxlDataType, const char*> JxlDataTypeStrings
{
    DefineEnumString(JXL_TYPE_FLOAT),
    DefineEnumString(JXL_TYPE_BOOLEAN),
    DefineEnumString(JXL_TYPE_UINT8),
    DefineEnumString(JXL_TYPE_UINT16),
    DefineEnumString(JXL_TYPE_UINT32),
    DefineEnumString(JXL_TYPE_FLOAT16)
};
const std::map<JxlEndianness, const char*> JxlEndiannessStrings
{
    DefineEnumString(JXL_NATIVE_ENDIAN),
    DefineEnumString(JXL_LITTLE_ENDIAN),
    DefineEnumString(JXL_BIG_ENDIAN)
};
#undef DefineEnumString

emscripten::val JxlPreviewHeaderToJs(JxlPreviewHeader value)
{
    emscripten::val header = emscripten::val::object();

    header.set("xsize", value.xsize);
    header.set("ysize", value.ysize);

    return header;
}
emscripten::val JxlIntrinsicSizeHeaderToJs(JxlIntrinsicSizeHeader value)
{
    emscripten::val header = emscripten::val::object();

    header.set("xsize", value.xsize);
    header.set("ysize", value.ysize);

    return header;
}
emscripten::val JxlAnimationHeaderToJs(JxlAnimationHeader value)
{
    emscripten::val header = emscripten::val::object();

    header.set("have_timecodes", value.have_timecodes != 0);
    header.set("num_loops", value.num_loops);
    header.set("tps_denominator", value.tps_denominator);
    header.set("tps_numerator", value.tps_numerator);

    return header;
}
emscripten::val JxlBasicInfoToJs(JxlBasicInfo value)
{
    emscripten::val info = emscripten::val::object();

    info.set("alpha_bits", value.alpha_bits);
    info.set("alpha_exponent_bits", value.alpha_exponent_bits);
    info.set("alpha_premultiplied", value.alpha_premultiplied != 0);
    info.set("animation", JxlAnimationHeaderToJs(value.animation));
    info.set("bits_per_sample", value.bits_per_sample);
    info.set("exponent_bits_per_sample", value.exponent_bits_per_sample);
    info.set("have_animation", value.have_animation != 0);
    info.set("have_container", value.have_container != 0);
    info.set("have_preview", value.have_preview != 0);
    info.set("intensity_target", value.intensity_target);
    info.set("intrinsic_xsize", value.intrinsic_xsize);
    info.set("intrinsic_ysize", value.intrinsic_ysize);
    info.set("linear_below", value.linear_below);
    info.set("min_nits", value.min_nits);
    info.set("num_color_channels", value.num_color_channels);
    info.set("num_extra_channels", value.num_extra_channels);
    info.set("orientation", value.orientation);
    //info.set("padding", value.padding); //
    info.set("preview", JxlPreviewHeaderToJs(value.preview));
    info.set("relative_to_max_display", value.relative_to_max_display != 0);
    info.set("uses_original_profile", value.uses_original_profile != 0);
    info.set("xsize", value.xsize);
    info.set("ysize", value.ysize);

    return info;
}
emscripten::val JxlExtraChannelInfoToJs(JxlExtraChannelInfo value)
{
    emscripten::val info = emscripten::val::object();

    info.set("alpha_premultiplied", value.alpha_premultiplied != 0);
    info.set("bits_per_sample", value.bits_per_sample);
    info.set("cfa_channel", value.cfa_channel);
    info.set("dim_shift", value.dim_shift);
    info.set("exponent_bits_per_sample", value.exponent_bits_per_sample);
    info.set("name_length", value.name_length);
    info.set("spot_color", JsFloat32Array.new_(emscripten::typed_memory_view(4, value.spot_color)));
    info.set("type", value.type);

    return info;

}
emscripten::val JxlHeaderExtensionsToJs(JxlHeaderExtensions value)
{
    emscripten::val extension = emscripten::val::object();

    extension.set("extensions", value.extensions);

    return extension;
}
emscripten::val JxlBlendInfoToJs(JxlBlendInfo value)
{
    emscripten::val info = emscripten::val::object();

    info.set("alpha", value.alpha);
    info.set("blendmode", value.blendmode);
    info.set("clamp", value.clamp);
    info.set("source", value.source);

    return info;
}
emscripten::val JxlLayerInfoToJs(JxlLayerInfo value)
{
    emscripten::val info = emscripten::val::object();

    info.set("blend_info", JxlBlendInfoToJs(value.blend_info));
    info.set("crop_x0", value.crop_x0);
    info.set("crop_y0", value.crop_y0);
    info.set("have_crop", value.have_crop != 0);
    info.set("save_as_reference", value.save_as_reference);
    info.set("xsize", value.xsize);
    info.set("ysize", value.ysize);

    return info;

}
emscripten::val JxlFrameHeaderToJs(JxlFrameHeader value)
{
    emscripten::val header = emscripten::val::object();

    header.set("duration", value.duration);
    header.set("is_last", value.is_last != 0);
    header.set("layer_info", JxlLayerInfoToJs(value.layer_info));
    header.set("name_length", value.name_length);
    header.set("timecode", value.timecode);

    return header;
}

void post_load()
{
    emscripten_run_script("console.log('post_load')");
    emscripten_run_script("Module.export="
        "{"
            "JxlExtraChannelType: Module.JxlExtraChannelType,"
            "JxlBlendMode: Module.JxlBlendMode,"
            "loaded: true"
        "};"
        "Object.freeze(Module.export.JxlExtraChannelType);"
        "Object.freeze(Module.JxlBlendMode);"
    );
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
        "Object.freeze(libjxl);"
        "x();"
    );
}

// should handle loading the module, actual functions shall be exposed via {enc/dec}_extras.cc
EMSCRIPTEN_BINDINGS(wasm_extras) {
    emscripten_run_script("console.log('wasm_extras')");
    emscripten::function("onRuntimeInitialized", post_load);


	#define DefineEnum(enum, name) enum##_enum.value(#name, enum::name)
    auto JxlExtraChannelType_enum = emscripten::enum_<JxlExtraChannelType>("JxlExtraChannelType");
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_ALPHA);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_BLACK);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_CFA);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_DEPTH);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_OPTIONAL);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED0);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED1);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED2);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED3);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED4);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED5);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED6);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_RESERVED7);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_SELECTION_MASK);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_SPOT_COLOR);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_THERMAL);
    DefineEnum(JxlExtraChannelType, JXL_CHANNEL_UNKNOWN);

    auto JxlBlendMode_enum = emscripten::enum_<JxlBlendMode>("JxlBlendMode");
    DefineEnum(JxlBlendMode, JXL_BLEND_ADD);
    DefineEnum(JxlBlendMode, JXL_BLEND_BLEND);
    DefineEnum(JxlBlendMode, JXL_BLEND_MUL);
    DefineEnum(JxlBlendMode, JXL_BLEND_MULADD);
    DefineEnum(JxlBlendMode, JXL_BLEND_REPLACE);

    auto JxlOrientation_enum = emscripten::enum_<JxlOrientation>("JxlOrientation");
    DefineEnum(JxlOrientation, JXL_ORIENT_ANTI_TRANSPOSE);
    DefineEnum(JxlOrientation, JXL_ORIENT_FLIP_HORIZONTAL);
    DefineEnum(JxlOrientation, JXL_ORIENT_FLIP_VERTICAL);
    DefineEnum(JxlOrientation, JXL_ORIENT_IDENTITY);
    DefineEnum(JxlOrientation, JXL_ORIENT_ROTATE_180);
    DefineEnum(JxlOrientation, JXL_ORIENT_ROTATE_90_CCW);
    DefineEnum(JxlOrientation, JXL_ORIENT_ROTATE_90_CW);
    DefineEnum(JxlOrientation, JXL_ORIENT_TRANSPOSE);

    auto JxlDataType_enum = emscripten::enum_<JxlDataType>("JxlDataType");
    DefineEnum(JxlDataType, JXL_TYPE_FLOAT);
    DefineEnum(JxlDataType, JXL_TYPE_BOOLEAN);
    DefineEnum(JxlDataType, JXL_TYPE_UINT8);
    DefineEnum(JxlDataType, JXL_TYPE_UINT16);
    DefineEnum(JxlDataType, JXL_TYPE_UINT32);
    DefineEnum(JxlDataType, JXL_TYPE_FLOAT16);

    auto JxlEndianness_enum = emscripten::enum_<JxlEndianness>("JxlEndianness");
    DefineEnum(JxlEndianness, JXL_NATIVE_ENDIAN);
    DefineEnum(JxlEndianness, JXL_LITTLE_ENDIAN);
    DefineEnum(JxlEndianness, JXL_BIG_ENDIAN);



    #undef DefineEnum
}

#endif