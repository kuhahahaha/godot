/**************************************************************************/
/*  gdextension_scene_ready.cpp                                           */
/**************************************************************************/

#include "gdextension.h"
#include "gdextension_manager.h"

#include "core/config/engine.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include <cstring>
#ifdef TESTS_ENABLED
#include <optional>
#endif
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

namespace GDExtensionDeviceExit {
Error Registry::scene_install_plan_locked(const Record &record,GDExtensionSceneInstallPlanV2 &out,SceneReadyFailureReason &reason) const {
 out={};reason=SceneReadyFailureReason::NO_SCENE_TREE;
 auto *manager=GDExtensionManager::get_singleton();
 if(!manager || !manager->scene_materialization_seam_has_been_reached())return ERR_UNAVAILABLE;
 auto *tree=Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
 if(!tree || tree!=SceneTree::get_singleton())return ERR_UNAVAILABLE;
 reason=SceneReadyFailureReason::NO_ROOT_WORLD;
#ifdef _3D_DISABLED
 return ERR_UNAVAILABLE;
#else
 Window *window=tree->get_root();
 if(!window || window->get_world_3d_instance_id().is_null() || !ObjectDB::get_instance(window->get_world_3d_instance_id()))return ERR_UNAVAILABLE;
 reason=SceneReadyFailureReason::NO_PUBLISHED_RENDERING_SERVER;
 auto *server=RenderingServer::get_singleton();
 if(!server || !Engine::get_singleton()->has_singleton("RenderingServer") || Engine::get_singleton()->get_singleton_object("RenderingServer")!=server)return ERR_UNAVAILABLE;
 reason=SceneReadyFailureReason::ACTUAL_RD_UNSUPPORTED;
 auto *device=server->get_rendering_device();auto *singleton=RenderingDevice::get_singleton();
 GDExtensionSceneInstallPlanV2 plan{};plan.abi={1,sizeof(plan),0};plan.library_generation=record.snapshot.library_generation;
 plan.required_features=record.scene_install_requirements.required_features;plan.optional_features=record.scene_install_requirements.optional_features;
 plan.tree_id=uint64_t(tree->get_instance_id());plan.window_id=uint64_t(window->get_instance_id());plan.world_id=uint64_t(window->get_world_3d_instance_id());plan.rendering_server_id=uint64_t(server->get_instance_id());
 if(!plan.library_generation)return ERR_UNAVAILABLE;
 if(!device && !singleton && record.scene_ready_flavor==Record::SceneReadyFlavor::V2 && plan.required_features==1 && plan.optional_features==2) {
  plan.selected_features=1;plan.omitted_optional_features=2;plan.omission_reason=GDEXTENSION_SCENE_INSTALL_OMISSION_NO_RD_PRESENT_V2;
 } else {
  if(!device || device!=singleton || !device->is_main_instance || ObjectDB::get_instance(device->get_instance_id())!=device)return ERR_UNAVAILABLE;
  for(const auto &registered:device_records)if(registered.object==device && registered.live && registered.epoch && registered.execution_thread && registered.execution_thread==device->render_thread_id){plan.main_rd_id=uint64_t(device->get_instance_id());plan.main_rd_epoch=registered.epoch;break;}
  if(!plan.main_rd_epoch)return ERR_UNAVAILABLE;
  plan.selected_features=3;
 }
 out=plan;return OK;
#endif
}
Error Registry::select_scene_install_plan(const RecordRef &ref,GDExtensionSceneInstallPlanV2 &out,SceneReadyFailureReason &reason) {
 out={};reason=SceneReadyFailureReason::ACTUAL_RD_UNSUPPORTED;
 if(ref.registry!=this)return ERR_INVALID_DATA;
 MutexLock lock(mutex);auto &record=records[ref.slot];
 if(record.owner_thread!=Thread::get_caller_id())return ERR_UNAUTHORIZED;
 if(record.quiescing || record.close_started || record.active_callbacks)return ERR_BUSY;
 if(record.scene_ready_state!=Record::SceneReadyState::PENDING || record.guard_enrollment!=GuardEnrollment::QUALIFIED || !record.attached || !record.bootstrap_tuple_observed || record.scene_ready_flavor==Record::SceneReadyFlavor::NONE)return ERR_UNAVAILABLE;
 GDExtensionSceneInstallPlanV2 selected{};const Error error=scene_install_plan_locked(record,selected,reason);if(error!=OK)return error;
 if(record.scene_install_plan_selected && std::memcmp(&selected,&record.scene_install_plan,sizeof(selected)))return ERR_UNAVAILABLE;
 if(!record.scene_install_plan_selected){record.scene_install_plan=selected;record.scene_install_plan_selected=true;}
 out=record.scene_install_plan;return OK;
}
bool Registry::scene_install_plan_matches_locked(const Record &record,SceneReadyFailureReason *failure) const {
 SceneReadyFailureReason reason=SceneReadyFailureReason::ACTUAL_RD_UNSUPPORTED;
 if(!record.scene_install_plan_selected){if(failure)*failure=reason;return false;}
 GDExtensionSceneInstallPlanV2 current{};
 if(scene_install_plan_locked(record,current,reason)!=OK){if(failure)*failure=reason;return false;}
 if(std::memcmp(&current,&record.scene_install_plan,sizeof(current))==0)return true;
 if(current.tree_id!=record.scene_install_plan.tree_id)reason=SceneReadyFailureReason::NO_SCENE_TREE;
 else if(current.window_id!=record.scene_install_plan.window_id || current.world_id!=record.scene_install_plan.world_id)reason=SceneReadyFailureReason::NO_ROOT_WORLD;
 else if(current.rendering_server_id!=record.scene_install_plan.rendering_server_id)reason=SceneReadyFailureReason::NO_PUBLISHED_RENDERING_SERVER;
 if(failure)*failure=reason;return false;
}

} // namespace GDExtensionDeviceExit

Error GDExtension::dispatch_scene_ready_if_materialized() {
	auto *manager = GDExtensionManager::get_singleton();
	if (!manager || !manager->scene_materialization_seam_has_been_reached()) { return OK; }
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	if (!registry.scene_ready_pending(library_exit_ref)) { return scene_ready_load_error; }
	using Reason = GDExtensionDeviceExit::SceneReadyFailureReason;
	auto reject = [&](Reason reason) {
		scene_ready_load_error = registry.reject_pending_scene_ready_before_effects(library_exit_ref, reason);
		return scene_ready_load_error;
	};
#ifdef TESTS_ENABLED
 // Negative native controls use actual core objects/state, never fabricated
 // driver handles or a caller-supplied plan. Disabled in ordinary builds/runs.
 std::optional<RenderingDevice> test_invalid_singleton;
 auto *test_server=RenderingServer::get_singleton();
 if(OS::get_singleton()->get_environment("GODOT_SI2_TEST_INVALID_SINGLETON")=="1" && test_server && !test_server->get_rendering_device() && !RenderingDevice::get_singleton()) {
  test_invalid_singleton.emplace();
  print_line(vformat("SI2_TEST_INVALID_SINGLETON object=%d driver_initialized=0",uint64_t(test_invalid_singleton->get_instance_id())));
 }
#endif
 GDExtensionSceneInstallPlanV2 plan{};Reason reason=Reason::ACTUAL_RD_UNSUPPORTED;
 const Error selected=registry.select_scene_install_plan(library_exit_ref,plan,reason);
 if(selected!=OK){if(selected==ERR_UNAVAILABLE)return reject(reason);scene_ready_load_error=selected;return selected;}
#ifdef TESTS_ENABLED
 if(OS::get_singleton()->get_environment("GODOT_SI2_TEST_LOSE_SELECTED_RD")=="1" && plan.main_rd_epoch) {
  registry.device_lost(registry.instance,plan.main_rd_epoch);
  print_line(vformat("SI2_TEST_SELECTED_RD_LOST epoch=%d before_dispatch=1",plan.main_rd_epoch));
 }
 std::optional<Engine::Singleton> test_unpublished_server;
 if(OS::get_singleton()->get_environment("GODOT_SI2_TEST_UNPUBLISH_SERVER")=="1") {
  List<Engine::Singleton> published;Engine::get_singleton()->get_singletons(&published);
  for(const auto &entry:published)if(entry.name==StringName("RenderingServer")){test_unpublished_server=entry;break;}
  if(test_unpublished_server){Engine::get_singleton()->remove_singleton("RenderingServer");print_line("SI2_TEST_SERVER_UNPUBLISHED before_dispatch=1");}
 }
#endif
	scene_ready_load_error = registry.dispatch_scene_ready(library_exit_ref);
#ifdef TESTS_ENABLED
 if(test_unpublished_server)Engine::get_singleton()->add_singleton(*test_unpublished_server);
#endif
	return scene_ready_load_error;
}
