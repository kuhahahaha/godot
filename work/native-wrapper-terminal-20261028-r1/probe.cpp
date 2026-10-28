// Actual SDK wrapper/Registry integration control; no production owner grant.
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/guarded_scene_tree.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace godot;
namespace {
GDExtensionInterfaceGetProcAddress original_lookup;
GDExtensionInterfaceObjectFreeInstanceBinding original_free;
GDExtensionInterfaceObjectGetInstanceId original_id;
uint64_t engine_id=0,tree_id=0;
void observe_free(GDExtensionObjectPtr object,void* token) {
 const auto id=original_id(object);
 original_free(object,token);
 std::printf("WRAPPER_FREE_RETURN role=%s id=%llu\n",id==tree_id?"tree":id==engine_id?"engine":"other",(unsigned long long)id);
 std::fflush(stdout);
}
GDExtensionInterfaceFunctionPtr lookup(const char* name) {
 if(std::strcmp(name,"object_free_instance_binding")==0)return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(observe_free);
 return original_lookup(name);
}
void initialize(ModuleInitializationLevel level) {
 if(level!=MODULE_INITIALIZATION_LEVEL_SCENE)return;
 engine_id=Engine::get_singleton()->get_instance_id();
 std::printf("WRAPPER_ENGINE_EARLY id=%llu\n",(unsigned long long)engine_id);
}
int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef frame,void*) {
 auto* b=GDExtensionBinding::guarded_claimant;
 const GDExtensionDeviceExitAbi abi{1,sizeof(GDExtensionDeviceExitAbi),0};
 const GDExtensionDeviceExitExitDiagnosticV1 diagnostic{{1,sizeof(GDExtensionDeviceExitExitDiagnosticV1),0},0,UINT32_MAX,0};
 return b->exit_api.publish_exit_result(&abi,b->guard,frame,&diagnostic);
}
void terminate(ModuleInitializationLevel level) {
 std::printf("WRAPPER_USER_RETURN level=%u\n",unsigned(level));
}
int32_t ready(GDExtensionDeviceExitSceneReadyContextRef frame,const GDExtensionSceneInstallPlanV2& plan) {
 auto* tree=SceneTree::get_singleton();
 if(!tree || tree->get_instance_id()!=plan.tree_id)return 30;
 tree_id=tree->get_instance_id();
#ifndef OMIT_TREE_SCHEDULE
 internal::retain_guarded_scene_binding(tree,internal::GuardedSceneBinding::Tree,
     GDExtensionBinding::active_guarded_user->exit_api,frame);
#endif
 std::printf("WRAPPER_TREE_READY id=%llu\n",(unsigned long long)tree_id);
 return 0;
}
}
extern "C" GDExtensionBool GDE_EXPORT wrapper_probe_init(GDExtensionInterfaceGetProcAddress get,
 GDExtensionClassLibraryPtr library,GDExtensionInitialization* init) {
 original_lookup=get;
 original_free=reinterpret_cast<GDExtensionInterfaceObjectFreeInstanceBinding>(get("object_free_instance_binding"));
 original_id=reinterpret_cast<GDExtensionInterfaceObjectGetInstanceId>(get("object_get_instance_id"));
 GDExtensionBinding::InitObject object(lookup,library,init);
 object.register_initializer(initialize);object.register_terminator(terminate);
 object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
 const GDExtensionSceneInstallRequirementsV2 req{{1,sizeof(GDExtensionSceneInstallRequirementsV2),0},GDEXTENSION_SCENE_INSTALL_BASE_V2|GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2,0,0};
 object.register_scene_ready_callback_v2(ready,req);
 GDExtensionDeviceExitGuardRegistrationV1 config{};
 config.level_bitmap=config.binding_cleanup_bitmap=config.user_cleanup_bitmap=12;
 config.entry_classes=(1u<<0)|(1u<<1)|(0x3fu<<2)|(1u<<8);
 config.needs[GDEXTENSION_INITIALIZATION_SCENE]={(1u<<1)|(1u<<5)|(1u<<7),1u<<7};
 config.needs[GDEXTENSION_INITIALIZATION_EDITOR]={0,1u<<7};
 config.preflight=preflight;
 object.set_guard_configuration(config);
 return object.init();
}

__attribute__((destructor)) static void wrapper_unloaded() { std::puts("WRAPPER_DSO_UNLOAD"); std::fflush(stdout); }
