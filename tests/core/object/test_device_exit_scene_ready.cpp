#include "core/extension/gdextension.h"
#include "core/extension/gdextension_interface.gen.h"
#include "tests/test_macros.h"

#include <type_traits>

TEST_FORCE_LINK(test_device_exit_scene_ready)

GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *p_name);
GDExtensionInterfaceFunctionPtr gdextension_get_guarded_proc_address(const char *p_name);

static_assert(sizeof(GDExtensionDeviceExitSceneReadyContextRef) == 16);
static_assert(alignof(GDExtensionDeviceExitSceneReadyContextRef) == alignof(uint64_t));
static_assert(GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 == (UINT64_C(1) << 11));
static_assert(std::is_same_v<GDExtensionDeviceExitSceneReadyCallbackV1, int32_t (*)(void *, GDExtensionDeviceExitSceneReadyContextRef)>);

TEST_CASE("[DeviceExit][SceneReady] four scene-ready procedures are guarded-only") {
 SUBCASE("SI2_01_ABI_and_atomic_lookup") {
  CHECK(gdextension_get_proc_address("device_exit_register_scene_ready_v2") == nullptr);
  CHECK(gdextension_get_guarded_proc_address("device_exit_register_scene_ready_v2") != nullptr);
 }
	for (const char *name : { "device_exit_register_scene_ready_v1", "device_exit_scene_ready_schedule_object_cleanup_v1", "device_session_bind_scene_ready_v1", "device_exit_scene_ready_abort_v1" }) {
		CHECK(gdextension_get_proc_address(name) == nullptr);
		CHECK(gdextension_get_guarded_proc_address(name) != nullptr);
	}
}


static int v2_ready_effects=0;
static int32_t v2_never_enter(void *,GDExtensionDeviceExitSceneReadyContextRef,const GDExtensionSceneInstallPlanV2 *){++v2_ready_effects;return OK;}

TEST_CASE("[DeviceExit][SceneReady] malformed ABI never reaches ready effects") {
 SUBCASE("SI2_01_requirements_before_effects") {
 auto install=reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterSceneReadyV2>(gdextension_get_guarded_proc_address("device_exit_register_scene_ready_v2"));
 REQUIRE(install!=nullptr);if(!install)return;
 v2_ready_effects=0;
 const GDExtensionSceneInstallRequirementsV2 valid{{1,sizeof(GDExtensionSceneInstallRequirementsV2),0},1,2,0};
 CHECK(install(nullptr,{},v2_never_enter,nullptr)==ERR_INVALID_PARAMETER);
 CHECK(install(&valid,{},nullptr,nullptr)==ERR_INVALID_PARAMETER);
 alignas(GDExtensionSceneInstallRequirementsV2) unsigned char bytes[sizeof(valid)+alignof(GDExtensionSceneInstallRequirementsV2)]={};
 CHECK(install(reinterpret_cast<const GDExtensionSceneInstallRequirementsV2 *>(bytes+1),{},v2_never_enter,nullptr)==ERR_INVALID_PARAMETER);
 for(int variant=0;variant<13;++variant){auto q=valid;switch(variant){
  case 0:q.abi.abi_version=2;break;case 1:q.abi.struct_size=16;break;case 2:q.abi.reserved=1;break;case 3:q.reserved=1;break;
  case 4:q.required_features=0;q.optional_features=0;break;case 5:q.optional_features=0;break;case 6:q.required_features=0;break;case 7:q.required_features=3;break;case 8:q.optional_features=1;break;case 9:q.required_features=7;q.optional_features=0;break;case 10:q.optional_features=4;break;case 11:q.required_features=2;q.optional_features=1;break;case 12:q.required_features=UINT32_MAX;q.optional_features=0;break;
 }CHECK(install(&q,{},v2_never_enter,nullptr)==ERR_INVALID_PARAMETER);CHECK(v2_ready_effects==0);}
 CHECK(install(&valid,{},v2_never_enter,nullptr)!=OK);CHECK(v2_ready_effects==0);
 }
	const auto registration = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterSceneReadyV1>(gdextension_get_guarded_proc_address("device_exit_register_scene_ready_v1"));
	const auto schedule = reinterpret_cast<GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1>(gdextension_get_guarded_proc_address("device_exit_scene_ready_schedule_object_cleanup_v1"));
	const auto bind = reinterpret_cast<GDExtensionInterfaceDeviceSessionBindSceneReadyV1>(gdextension_get_guarded_proc_address("device_session_bind_scene_ready_v1"));
	const auto abort_ready = reinterpret_cast<GDExtensionInterfaceDeviceExitSceneReadyAbortV1>(gdextension_get_guarded_proc_address("device_exit_scene_ready_abort_v1"));
	REQUIRE(registration != nullptr);
	REQUIRE(schedule != nullptr);
	REQUIRE(bind != nullptr);
	REQUIRE(abort_ready != nullptr);
	CHECK(registration(nullptr, {}, nullptr, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(schedule(nullptr, {}, 0, 0) == ERR_INVALID_PARAMETER);
	GDExtensionDeviceRegistryContextRef context{};
	GDExtensionOriginalDeviceSessionRef session{};
	CHECK(bind(nullptr, {}, nullptr, &context, &session).native_error == ERR_INVALID_PARAMETER);
	CHECK(context.handle_serial == 0);
	CHECK(session.handle_serial == 0);
	CHECK(abort_ready(nullptr, {}, ERR_INVALID_DATA, GDEXTENSION_DEVICE_EXIT_SCENE_READY_ABORT_UNSCHEDULED_PAIR_V1, 0) == ERR_INVALID_PARAMETER);
}
