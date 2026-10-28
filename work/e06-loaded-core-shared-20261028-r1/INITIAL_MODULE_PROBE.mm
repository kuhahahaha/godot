// Private read-only libgodot host. Uses original C ABI; no SDK or production actor.
#include "core/extension/libgodot.h"
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct alignas(16) Opaque { unsigned char bytes[64]{}; };
GDExtensionInterfaceGetProcAddress resolve;
GDExtensionInterfaceStringNameNewWithUtf8Chars name_new;
GDExtensionInterfaceVariantGetPtrDestructor type_destroy;
GDExtensionInterfaceVariantDestroy variant_destroy;
GDExtensionInterfaceVariantGetNamed get_named;
GDExtensionInterfaceVariantCall variant_call;
GDExtensionInterfaceGetVariantToTypeConstructor to_native;
GDExtensionInterfaceObjectMethodBindCall method_call;
GDExtensionInterfaceClassdbGetMethodBind method_bind;
GDExtensionInterfaceGlobalGetSingleton singleton;
GDExtensionInterfaceStringToUtf8Chars utf8;
void need(bool ok, const char *where) { if(!ok){std::fprintf(stderr,"E06_SHARED_FIXTURE_ERROR=%s\n",where);std::exit(81);} }
template<class T>T proc(const char *name){auto p=resolve(name);need(p!=nullptr,name);return reinterpret_cast<T>(p);}
void init(void *, GDExtensionInitializationLevel) {}
void deinit(void *, GDExtensionInitializationLevel) {}
GDExtensionBool initialize(GDExtensionInterfaceGetProcAddress p_resolve, GDExtensionClassLibraryPtr, GDExtensionInitialization *out) {
 resolve=p_resolve;
 name_new=proc<GDExtensionInterfaceStringNameNewWithUtf8Chars>("string_name_new_with_utf8_chars");
 type_destroy=proc<GDExtensionInterfaceVariantGetPtrDestructor>("variant_get_ptr_destructor");
 variant_destroy=proc<GDExtensionInterfaceVariantDestroy>("variant_destroy");
 get_named=proc<GDExtensionInterfaceVariantGetNamed>("variant_get_named");
 variant_call=proc<GDExtensionInterfaceVariantCall>("variant_call");
 to_native=proc<GDExtensionInterfaceGetVariantToTypeConstructor>("get_variant_to_type_constructor");
 method_call=proc<GDExtensionInterfaceObjectMethodBindCall>("object_method_bind_call");
 method_bind=proc<GDExtensionInterfaceClassdbGetMethodBind>("classdb_get_method_bind");
 singleton=proc<GDExtensionInterfaceGlobalGetSingleton>("global_get_singleton");
 utf8=proc<GDExtensionInterfaceStringToUtf8Chars>("string_to_utf8_chars");
 out->minimum_initialization_level=GDEXTENSION_INITIALIZATION_CORE;out->userdata=nullptr;out->initialize=init;out->deinitialize=deinit;
 return true;
}
void named(const Opaque &dictionary,const char *key,Opaque &out){Opaque name;name_new(&name,key);GDExtensionBool valid=false;get_named(&dictionary,&name,&out,&valid);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&name);need(valid,key);}
std::string text(const Opaque &value){Opaque s;to_native(GDEXTENSION_VARIANT_TYPE_STRING)(&s,const_cast<Opaque*>(&value));auto n=utf8(&s,nullptr,0);std::vector<char> result(n+1);utf8(&s,result.data(),n);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING)(&s);return {result.data(),size_t(n)};}
std::string field(const Opaque &dictionary,const char *key){Opaque value;named(dictionary,key,value);auto result=text(value);variant_destroy(&value);return result;}
int64_t integer(const Opaque &value){int64_t result=0;to_native(GDEXTENSION_VARIANT_TYPE_INT)(&result,const_cast<Opaque*>(&value));return result;}
Opaque call(const Opaque &value,const char *method){Opaque name,result;name_new(&name,method);GDExtensionCallError error{};variant_call(const_cast<Opaque*>(&value),&name,nullptr,0,&result,&error);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&name);need(error.error==GDEXTENSION_CALL_OK,method);return result;}
}
int main(int argc,char **argv){
 if(argc!=5)return 80;
 @autoreleasepool {
  void *module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!module){std::fprintf(stderr,"E06_SHARED_DLOPEN=%s\n",dlerror());return 82;}
  auto create=reinterpret_cast<decltype(&libgodot_create_godot_instance)>(dlsym(module,"libgodot_create_godot_instance"));
  auto destroy=reinterpret_cast<decltype(&libgodot_destroy_godot_instance)>(dlsym(module,"libgodot_destroy_godot_instance"));need(create&&destroy,"original libgodot exports");
  char flag[]="--headless",pathflag[]="--path";char *engine_args[]={argv[0],flag,pathflag,argv[4]};
  auto instance=create(4,engine_args,initialize);need(instance!=nullptr,"original create GodotInstance");
  Opaque engine_name,method_name;name_new(&engine_name,"Engine");name_new(&method_name,"get_version_info");
  auto engine=singleton(&engine_name);auto binding=method_bind(&engine_name,&method_name,3102165223);need(engine&&binding,"original Engine method");
  type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&method_name);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&engine_name);
  Opaque info,identity;GDExtensionCallError error{};method_call(binding,engine,nullptr,0,&info,&error);need(error.error==GDEXTENSION_CALL_OK,"get_version_info call");named(info,"controlled_build_identity",identity);
  const auto core=field(identity,"core_stamp"),compiler=field(identity,"compiler_stamp");
  bool exact=core==argv[2]&&compiler==argv[3]&&field(identity,"compiler_route")=="BuiltinGlslangV1"&&field(identity,"platform")=="macos"&&field(identity,"architecture")=="arm64"&&field(identity,"target_kind")=="SharedLibrary";
  Opaque schema;named(identity,"schema",schema);exact=exact&&integer(schema)==1;variant_destroy(&schema);
  Opaque count=call(identity,"size");bool closed=integer(count)==7;variant_destroy(&count);
  Opaque readonly=call(identity,"is_read_only");GDExtensionBool is_readonly=false;to_native(GDEXTENSION_VARIANT_TYPE_BOOL)(&is_readonly,&readonly);variant_destroy(&readonly);
  Opaque major;named(info,"major",major);bool original=integer(major)==4;variant_destroy(&major);
  std::printf("E06_SHARED_LOADED_CORE=%s\nE06_SHARED_LOADED_COMPILER=%s\nE06_SHARED_CHECK exact=%u closed=%u readonly=%u original_fields=%u\n",core.c_str(),compiler.c_str(),unsigned(exact),unsigned(closed),unsigned(is_readonly),unsigned(original));
  variant_destroy(&identity);variant_destroy(&info);destroy(instance);std::puts("E06_SHARED_ORIGINAL_DESTROY_RETURNED");int closed_module=dlclose(module);std::printf("E06_SHARED_DLCLOSE=%d\n",closed_module);
  return exact&&closed&&is_readonly&&original&&closed_module==0?0:83;
 }
}
