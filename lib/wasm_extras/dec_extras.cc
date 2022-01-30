#ifdef __EMSCRIPTEN__

#ifndef WASM_EXTRAS_DECODER
#error WASM_EXTRAS_DECODER NOT SET BUT BUILDING WASM_EXTRAS_DECODER!
#endif

#include "dec_extras.h"
#include "wasm_extras_internals.h"

#pragma region enums

#define DefineEnumString(name) std::make_pair(name, #name)
const std::map<JxlDecoderStatus, const char*> JXLDecoderStatusStrings
{
    DefineEnumString(JXL_DEC_ERROR),
    DefineEnumString(JXL_DEC_EXTENSIONS),
    DefineEnumString(JXL_DEC_FRAME),
    DefineEnumString(JXL_DEC_FRAME_PROGRESSION),
    DefineEnumString(JXL_DEC_FULL_IMAGE),
    DefineEnumString(JXL_DEC_JPEG_NEED_MORE_OUTPUT),
    DefineEnumString(JXL_DEC_JPEG_RECONSTRUCTION),
    DefineEnumString(JXL_DEC_NEED_DC_OUT_BUFFER),
    DefineEnumString(JXL_DEC_NEED_IMAGE_OUT_BUFFER),
    DefineEnumString(JXL_DEC_NEED_MORE_INPUT),
    DefineEnumString(JXL_DEC_NEED_PREVIEW_OUT_BUFFER),
    DefineEnumString(JXL_DEC_PREVIEW_IMAGE),
    DefineEnumString(JXL_DEC_SUCCESS)
};
#undef DefineEnumString

template<const JxlDecoderStatus status>
WasmJxlDecoderStatus GetWasmJxlDecoderStatus()
{
	return WasmJxlDecoderStatus(status);
}

#pragma endregion

#pragma region decode_oneshot
template<typename TValueType, const JxlDataType TJXL_TYPE>
inline emscripten::val decode_oneshot(std::string data, const emscripten::val ArrayType) {
  	emscripten::val ret = emscripten::val::object();
  //std::unique_ptr<JxlDecoder, std::integral_constant<decltype(&JxlDecoderDestroy), JxlDecoderDestroy>> dec(JxlDecoderCreate(nullptr));
	JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  	const int PIXEL_COMPONENTS = 4;
  	JxlBasicInfo info;
	JxlPixelFormat format = {PIXEL_COMPONENTS, TJXL_TYPE, JXL_NATIVE_ENDIAN, 0};

	std::vector<TValueType> pixels = std::vector<TValueType>();
	std::vector<uint8_t> icc = std::vector<uint8_t>();
	uint32_t xsize = -1;
	uint32_t ysize = -1;

	if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE)) {
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
			ret.set("icc", JsUInt8Array.new_(emscripten::typed_memory_view(icc.size(), icc.data())));
			return ret;
			break;

		default:
			ret.set("error", 10);
			ret.set("message", "unknown status code recived by JxlDecoderProcessInput");
			return ret;
		}
	}
}
//emscripten::val decode_oneshot_boolean(std::string data) { return decode_oneshot<bool, JXL_TYPE_BOOLEAN>(data, JsBooleanArray); }
//emscripten::val decode_oneshot_float16(std::string data) { return decode_oneshot<float16_t, JXL_TYPE_FLOAT16>(data, JsFloat16Array); }
emscripten::val decode_oneshot_float32(std::string data) { return decode_oneshot<float, JXL_TYPE_FLOAT>(data, JsFloat32Array); }
emscripten::val decode_oneshot_uint8(std::string data) { return decode_oneshot<uint8_t, JXL_TYPE_UINT8>(data, JsUInt8Array); }
emscripten::val decode_oneshot_uint16(std::string data) { return decode_oneshot<uint16_t, JXL_TYPE_UINT16>(data, JsUInt16Array); }
//emscripten::val decode_oneshot_uint32(std::string data) { return decode_oneshot<uint32_t, JXL_TYPE_UINT32>(data, JsUInt32Array); }
#pragma endregion

#pragma region WasmJxlDecoderStatus
WasmJxlDecoderStatus::WasmJxlDecoderStatus()
{
	this->status = static_cast<JxlDecoderStatus>(-51);
	this->message = "uninitialized";
}
WasmJxlDecoderStatus::WasmJxlDecoderStatus(JxlDecoderStatus status)
{
	this->status = status;
	this->message = JXLDecoderStatusStrings.at(status);
}
WasmJxlDecoderStatus::WasmJxlDecoderStatus(std::string message)
{
	this->status = static_cast<JxlDecoderStatus>(-50);
	this->message = message;
}

const int WasmJxlDecoderStatus::getStatusInt()
{
  return static_cast<int>(this->status);
}
std::string WasmJxlDecoderStatus::getStatusString()
{
  return this->message;
}

bool WasmJxlDecoderStatus::operator ==(WasmJxlDecoderStatus other)
{
	return this->status == other.status || this->message == other.message;
}
bool WasmJxlDecoderStatus::operator ==(JxlDecoderStatus other)
{
	return this->status == other;
}

#pragma endregion

#pragma region WasmJxlDecoder

//TODO: Implement remaining Decoder Functions
//TODO: Implement JxlPixelFormat
//TODO: Implement JxlColorProfileTarget

WasmJxlDecoder::WasmJxlDecoder()
{
	this->decoder = JxlDecoderMake(nullptr);
	this->last_status = WasmJxlDecoderStatus(JxlDecoderStatus::JXL_DEC_SUCCESS);
}
WasmJxlDecoderStatus WasmJxlDecoder::getStatus()
{
	return this->last_status;
}
void WasmJxlDecoder::processInput()
{
	this->last_status = JxlDecoderProcessInput(this->decoder.get());
}

void WasmJxlDecoder::subscribeEvents(emscripten::val events)
{
	if(!events.isArray())
	{
		this->last_status = WasmJxlDecoderStatus("Invalid Input, expected array of JxlDecoderStatus");
		return;
	}
	int subscriberflags=0;
	int size = events["length"]().as<int>();
	for(int x = 0; x < size; x++)
		subscriberflags |= events[x].as<WasmJxlDecoderStatus>().getStatusInt(); // i have 0 clue how to make this type safe
	
	this->last_status = WasmJxlDecoderStatus(JxlDecoderSubscribeEvents(this->decoder.get(), subscriberflags));
}
void WasmJxlDecoder::setInput(std::string buffer)
{
	this->last_status = JxlDecoderSetInput(this->decoder.get(), (const uint8_t*)buffer.c_str(), buffer.length());
}
void WasmJxlDecoder::closeInput()
{
	JxlDecoderCloseInput(this->decoder.get());
	this->last_status = WasmJxlDecoderStatus(JxlDecoderStatus::JXL_DEC_SUCCESS);
}
emscripten::val WasmJxlDecoder::getBasicInfo()
{
	JxlBasicInfo info;
	this->last_status = JxlDecoderGetBasicInfo(this->decoder.get(), &info);
	if(this->last_status == JXL_DEC_SUCCESS)
	{
		return JxlBasicInfoToJs(info);
	}
	else
	{
		return emscripten::val::null();
	}
}
// NOT WORKING
emscripten::val WasmJxlDecoder::getColorAsICCProfile(JxlPixelFormat format, JxlColorProfileTarget target)
{
	size_t size;
	this->last_status = JxlDecoderGetICCProfileSize(this->decoder.get(), &format, target, &size);
	if(this->last_status == JXL_DEC_SUCCESS)
	{
		uint8_t data[size];
		this->last_status = JxlDecoderGetColorAsICCProfile(this->decoder.get(), &format, target, data, size);
		return JsUInt8Array.new_(emscripten::typed_memory_view(size, data));
	}
	else
	{
		return emscripten::val::null();
	}
}
// NOT WORKING
emscripten::val WasmJxlDecoder::getPixelData(JxlPixelFormat format)
{
	size_t size;
	this->last_status = JxlDecoderImageOutBufferSize(this->decoder.get(), &format, &size);
	if(this->last_status == JXL_DEC_SUCCESS)
	{
		uint8_t data[size];
		this->last_status = JxlDecoderSetImageOutBuffer(this->decoder.get(), &format, data, size);
		switch (format.data_type)
		{
			//case JXL_TYPE_BOOLEAN: return JsBooleanArray.new_(emscripten::typed_memory_view(size / sizeof(bool), (bool*)data));
			//case JXL_TYPE_FLOAT16: return JsFloat16Array.new_(emscripten::typed_memory_view(size / sizeof(float16_t), (float16_t*)data));
			case JXL_TYPE_FLOAT: return JsFloat32Array.new_(emscripten::typed_memory_view(size / sizeof(float), (float*)data));
			case JXL_TYPE_UINT8: goto return_uint8;
			case JXL_TYPE_UINT16: return JsUInt16Array.new_(emscripten::typed_memory_view(size / sizeof(uint16_t), (uint16_t*)data));
			//case JXL_TYPE_UINT32: return JsUInt32Array.new_(emscripten::typed_memory_view(size / sizeof(uint32_t), (uint32_t*)data));
			default:
				return_uint8:
				return JsUInt8Array.new_(emscripten::typed_memory_view(size / sizeof(uint8_t), (uint8_t*)data));
		}
	}
	else
	{
		return emscripten::val::null();
	}
}

#pragma endregion

// TODO: Implement    emscripten::val decode_to_png_{type}(std::string data)

void wasm_decoder_post_load()
{
  emscripten_run_script("console.log('wasm_decoder_post_load')");
  // move exports to libjxl.Dec
  emscripten_run_script(
	"Module.JxlDecoderStatus.Enum=Module.JxlDecoderStatus_values;"
	"Module.export.Dec = {"
		"JxlDecoderStatus: Module.JxlDecoderStatus,"
		"JxlDecoder: Module.JxlDecoder,"

		//"decode_oneshot_boolean: Module.decode_oneshot_boolean,"
		//"decode_oneshot_float16: Module.decode_oneshot_float16,"
		"decode_oneshot_float32: Module.decode_oneshot_float32,"
		"decode_oneshot_uint8: Module.decode_oneshot_uint8,"
		"decode_oneshot_uint16: Module.decode_oneshot_uint16,"
		//"decode_oneshot_uint32: Module.decode_oneshot_uint32,"
		"decode_oneshot: Module.decode_oneshot"
	"};"
	"Object.freeze(Module.export.Dec.JxlDecoderStatus.Enum);"
	"Object.freeze(Module.export.Dec.JxlDecoderStatus);"
	"Object.freeze(Module.export.Dec);"
  );
}

EMSCRIPTEN_BINDINGS(dec_extras) {
	emscripten_run_script("console.log('dec_extras')");

	emscripten::function("decode_oneshot", &decode_oneshot_uint8);
	//emscripten::function("decode_oneshot_boolean", &decode_oneshot_boolean);
	//emscripten::function("decode_oneshot_float16", &decode_oneshot_float16);
	emscripten::function("decode_oneshot_float32", &decode_oneshot_float32);
	emscripten::function("decode_oneshot_uint8", &decode_oneshot_uint8);
	emscripten::function("decode_oneshot_uint16", &decode_oneshot_uint16);
	//emscripten::function("decode_oneshot_uint32", &decode_oneshot_uint32);

	auto JxlDecoderStatus_type = emscripten::class_<WasmJxlDecoderStatus>("JxlDecoderStatus");
	JxlDecoderStatus_type.function("getStatusInt", &WasmJxlDecoderStatus::getStatusInt);
	JxlDecoderStatus_type.function("getStatusString", &WasmJxlDecoderStatus::getStatusString);

	#define DefineCommonStatusType(name) JxlDecoderStatus_enum.value(#name, name)
	auto JxlDecoderStatus_enum = emscripten::enum_<JxlDecoderStatus>("JxlDecoderStatus_values");
	JxlDecoderStatus_enum.value("JXL_JS_ERROR", static_cast<JxlDecoderStatus>(-50));
	JxlDecoderStatus_enum.value("JXL_JS_UNINITIALIZED", static_cast<JxlDecoderStatus>(-51));
	DefineCommonStatusType(JXL_DEC_ERROR);
    DefineCommonStatusType(JXL_DEC_EXTENSIONS);
    DefineCommonStatusType(JXL_DEC_FRAME);
    DefineCommonStatusType(JXL_DEC_FRAME_PROGRESSION);
    DefineCommonStatusType(JXL_DEC_FULL_IMAGE);
    DefineCommonStatusType(JXL_DEC_JPEG_NEED_MORE_OUTPUT);
    DefineCommonStatusType(JXL_DEC_JPEG_RECONSTRUCTION);
    DefineCommonStatusType(JXL_DEC_NEED_DC_OUT_BUFFER);
    DefineCommonStatusType(JXL_DEC_NEED_IMAGE_OUT_BUFFER);
    DefineCommonStatusType(JXL_DEC_NEED_MORE_INPUT);
    DefineCommonStatusType(JXL_DEC_NEED_PREVIEW_OUT_BUFFER);
    DefineCommonStatusType(JXL_DEC_PREVIEW_IMAGE);
    DefineCommonStatusType(JXL_DEC_SUCCESS);
	#undef DefineCommonStatusType

	auto JxlDecoder_type = emscripten::class_<WasmJxlDecoder>("JxlDecoder");
	JxlDecoder_type.function("getStatus", &WasmJxlDecoder::getStatus);
	JxlDecoder_type.function("processInput", &WasmJxlDecoder::processInput);
	JxlDecoder_type.constructor();
}

#endif