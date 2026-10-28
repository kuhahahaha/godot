#include "core/extension/libgodot.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

auto resolve_proc = [](GDExtensionInterfaceGetProcAddress p, const char *name) -> void * { void *out = reinterpret_cast<void *>(p(name)); if (!out) { std::fprintf(stderr, "STATIC_HOST_MISSING=%s\n", name); std::exit(81); } return out; };
struct alignas(16) Opaque { unsigned char bytes[64]{}; };
struct State {
 GDExtensionInterfaceGetProcAddress resolve=nullptr;
 GDExtensionInterfaceStringNameNewWithUtf8Chars name_new=nullptr;
 GDExtensionInterfaceVariantGetPtrDestructor type_destroy=nullptr;
 GDExtensionInterfaceVariantDestroy variant_destroy=nullptr;
 GDExtensionInterfaceVariantGetNamed get_named=nullptr;
 GDExtensionInterfaceGetVariantToTypeConstructor to_native=nullptr;
 GDExtensionInterfaceObjectMethodBindCall method_call=nullptr;
 GDExtensionInterfaceClassdbGetMethodBind method_bind=nullptr;
 GDExtensionInterfaceGlobalGetSingleton singleton=nullptr;
 GDExtensionInterfaceStringToUtf8Chars utf8=nullptr;
} state;
static void init(void *, GDExtensionInitializationLevel) {}
static void deinit(void *, GDExtensionInitializationLevel) {}
static void need(bool x,const char *where){if(!x){std::fprintf(stderr,"STATIC_HOST_FAIL=%s\n",where);std::exit(82);}}
static void name(Opaque &out,const char *text){state.name_new(&out,text);}
static void named(const Opaque &dict,const char *key,Opaque &out){Opaque k;name(k,key);GDExtensionBool valid=false;state.get_named(&dict,&k,&out,&valid);state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&k);need(valid,key);}
static std::string string_value(const Opaque &value){Opaque string;state.to_native(GDEXTENSION_VARIANT_TYPE_STRING)(&string,const_cast<Opaque*>(&value));int64_t n=state.utf8(&string,nullptr,0);std::vector<char> bytes(size_t(n)+1);state.utf8(&string,bytes.data(),n);state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING)(&string);return std::string(bytes.data(),size_t(n));}
static int64_t integer_value(const Opaque &value){int64_t result=0;state.to_native(GDEXTENSION_VARIANT_TYPE_INT)(&result,const_cast<Opaque*>(&value));return result;}
static GDExtensionBool initialize(GDExtensionInterfaceGetProcAddress resolve,GDExtensionClassLibraryPtr owner,GDExtensionInitialization *out){
 state.resolve=resolve;auto load=[&](const char *name){return resolve_proc(resolve,name);};
 state.name_new=(GDExtensionInterfaceStringNameNewWithUtf8Chars)load("string_name_new_with_utf8_chars");state.type_destroy=(GDExtensionInterfaceVariantGetPtrDestructor)load("variant_get_ptr_destructor");state.variant_destroy=(GDExtensionInterfaceVariantDestroy)load("variant_destroy");state.get_named=(GDExtensionInterfaceVariantGetNamed)load("variant_get_named");state.to_native=(GDExtensionInterfaceGetVariantToTypeConstructor)load("get_variant_to_type_constructor");state.method_call=(GDExtensionInterfaceObjectMethodBindCall)load("object_method_bind_call");state.method_bind=(GDExtensionInterfaceClassdbGetMethodBind)load("classdb_get_method_bind");state.singleton=(GDExtensionInterfaceGlobalGetSingleton)load("global_get_singleton");state.utf8=(GDExtensionInterfaceStringToUtf8Chars)load("string_to_utf8_chars");out->minimum_initialization_level=GDEXTENSION_INITIALIZATION_CORE;out->userdata=nullptr;out->initialize=init;out->deinitialize=deinit;return true;}
int main(){char prog[]="static-template-debug-host";char headless[]="--headless";char *args[]={prog,headless};GDExtensionObjectPtr instance=libgodot_create_godot_instance(2,args,initialize);need(instance,"create");Opaque klass,method;name(klass,"GodotInstance");name(method,"start");auto start=state.method_bind(&klass,&method,2240911060);need(start,"start bind");Opaque started;GDExtensionCallError error{};state.method_call(start,instance,nullptr,0,&started,&error);GDExtensionBool started_bool=false;state.to_native(GDEXTENSION_VARIANT_TYPE_BOOL)(&started_bool,&started);state.variant_destroy(&started);need(error.error==GDEXTENSION_CALL_OK&&started_bool,"start");state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&method);name(method,"iteration");auto iteration=state.method_bind(&klass,&method,2240911060);need(iteration,"iteration bind");Opaque iterated;state.method_call(iteration,instance,nullptr,0,&iterated,&error);state.variant_destroy(&iterated);state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&method);state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&klass);need(error.error==GDEXTENSION_CALL_OK,"iteration");Opaque engine_name,method_name;name(engine_name,"Engine");name(method_name,"get_version_info");auto engine=state.singleton(&engine_name);auto binding=state.method_bind(&engine_name,&method_name,3102165223);need(engine&&binding,"engine bind");state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&method_name);state.type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&engine_name);Opaque info;state.method_call(binding,engine,nullptr,0,&info,&error);need(error.error==GDEXTENSION_CALL_OK,"version call");Opaque identity;named(info,"controlled_build_identity",identity);Opaque schema;named(identity,"schema",schema);Opaque core,compiler;named(identity,"core_stamp",core);named(identity,"compiler_stamp",compiler);Opaque route;named(identity,"compiler_route",route);std::printf("STATIC_HOST_CORE=%s\nSTATIC_HOST_COMPILER=%s\nSTATIC_HOST_ROUTE=%s\nSTATIC_HOST_SCHEMA=%lld\n",string_value(core).c_str(),string_value(compiler).c_str(),string_value(route).c_str(),(long long)integer_value(schema));state.variant_destroy(&schema);state.variant_destroy(&core);state.variant_destroy(&compiler);state.variant_destroy(&route);state.variant_destroy(&identity);state.variant_destroy(&info);libgodot_destroy_godot_instance(instance);std::puts("STATIC_HOST_DESTROY_RETURNED");return 0;}
