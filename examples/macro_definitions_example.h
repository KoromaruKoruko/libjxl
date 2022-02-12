#define EXTERN_C(code...) \
    std::cout << "BEGIN extern \"C\"" << std::endl; \
    code \
    std::cout << "END extern \"C\"" << std::endl;
#define Private \
    std::cout << "private: ";
#define Public \
    std::cout << "public: ";
#define Export \
    std::cout << "JXL_EXPORT";
#define DEPRECATED \
    std::cout << "JXL_DEPRECATED";
#define Threads_Export \
    std::cout << "JXL_THREADS_EXPORT";
#define Inline \
    std::cout << "inline"
#define Static \
    std::cout << "static"


#define Delegate(return_valuetype, name, args) \
    std::cout << #return_valuetype " (*" #name ")" #args << std::endl;

#define Struct(name, body...) \
    std::cout << "BEGIN struct " #name << std::endl; \
    body \
    std::cout << "END struct " #name << std::endl;

#define StructDef(name, body...) \
    std::cout << "BEGIN struct " #name << std::endl; \
    body \
    std::cout << "END struct " #name << std::endl;

#define StructDef2(typedef_name, struct_name, body...) \
    std::cout << "BEGIN struct " #struct_name " as " #typedef_name << std::endl; \
    body \
    std::cout << "END struct " #struct_name " as " #typedef_name << std::endl;

#define InlineStruct(body...) \
    std::cout << "BEGIN struct" << std::endl; \
    body \
    std::cout << "END struct" << std::endl;

#define NamedInlineStruct(name, body...) \
    std::cout << "BEGIN struct " #name << std::endl; \
    body \
    std::cout << "END struct " #name << std::endl;

#define Type(name, value...) \
    std::count << "DEFINE " #name " = " #value << std::endl;

#define TypeDef(name, value...) \
    std::count << "DEFINE " #name " = " #value << std::endl

#define MemberWithValue(valuetype, name, value...) \
    std::cout << #valuetype " " #name " = " #value ";" << std::endl;

#define Member(valuetype, name) \
    std::cout << #valuetype " " #name ";" << std::endl;

#define FixedArray(valuetype, name, size) \
    std::cout << #valuetype " " #name "[" #size "]" << std::endl;

#define Method(return_valuetype, name, args) \
    std::cout << #return_valuetype " " #name #args << std::endl;

#define BodyMethod(return_valuetype, name, args, code...) \
    std::cout << #return_valuetype " " #name #args << std::endl << #code;

#define Enum(name, body...) \
    std::cout << "BEGIN enum " #name << std::endl; \
    body \
    std::cout << "END enum " #name << std::endl;

#define EnumDef(name, body...) \
    std::cout << "BEGIN enum " #name << std::endl; \
    body \
    std::cout << "END enum " #name << std::endl;

#define Value(name) \
    std::cout << #name << std::endl;

#define DefinedValue(name, value...) \
    std::cout << #name " = " #value << std::endl;

#define RawCode(code...) \
    std::cout << "RAWCODE = \"\"\"" #code "\"\"\"" << std::endl;
