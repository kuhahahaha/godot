#include <dlfcn.h>
#include "thirdparty/spirv-reflect/spirv_reflect.h"

#include "core/error/error_list.h"

#include <CommonCrypto/CommonDigest.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/guarded_scene_tree.hpp>
#include <godot_cpp/godot.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#ifndef PROBE_TAG
#define PROBE_TAG "one"
#endif
#ifndef PROBE_ERROR
#define PROBE_ERROR 0
#endif
#ifndef PROBE_SKIP_READY
#define PROBE_SKIP_READY 0
#endif

namespace {
struct State {
	uint32_t calls = 0;
} state;
GDExtensionClassLibraryPtr library = nullptr;
GDExtensionDeviceExitGuardToken guard{};
const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
GDExtensionInterfaceDeviceExitRegisterGuardV1 enroll = nullptr;
GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1 borrow_init = nullptr;
GDExtensionInterfaceDeviceExitAfterBindingInitializeV1 after_init = nullptr;
GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1 borrow_deinit = nullptr;
GDExtensionInterfaceDeviceExitBeginUserTerminatorV1 begin_user = nullptr;
GDExtensionInterfaceDeviceExitAfterUserTerminatorV1 after_user = nullptr;
GDExtensionInterfaceDeviceExitAfterBindingDeinitV1 after_binding = nullptr;
GDExtensionInterfaceDeviceExitPublishExitResultV1 publish = nullptr;
GDExtensionInterfaceDeviceExitRegisterSceneReadyV1 install_ready = nullptr;
GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1 schedule_ready = nullptr;
GDExtensionInterfaceDeviceSessionBindSceneReadyV1 bind_ready = nullptr;
GDExtensionInterfaceDeviceExitSceneReadyAbortV1 abort_ready = nullptr;

GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1 borrow_library;
GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1 borrow_cleanup;
GDExtensionInterfaceDeviceExitAcquireCodeHoldV1 acquire_hold;
GDExtensionInterfaceDeviceExitReleaseCodeHoldV1 release_hold;
GDExtensionInterfaceDeviceSessionBindMainSceneReadyV1 bind_main;
GDExtensionInterfaceDeviceSessionQualificationOpenV1 open_bridge;
GDExtensionInterfaceDeviceSessionQualificationCloseV1 close_bridge;
GDExtensionInterfaceDeviceSessionReleaseContextV1 release_context;
GDExtensionInterfaceDeviceSessionReleaseSessionV1 release_session;
GDExtensionDeviceExitLibraryCodeHoldRef code_hold{};
GDExtensionInterfaceObjectGetInstanceFromId object_from_id;
GDExtensionDeviceExitSceneReadyContextRef expired_ready{};

GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1 storage_create;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1 storage_retire;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1 storage_observe;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1 storage_release;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1 unit_create;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1 unit_retire;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1 unit_observe;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1 unit_release;
GDExtensionInterfaceDeviceExitBootstrapLibraryV1 bootstrap_library;
GDExtensionInterfaceDeviceExitRetainLibraryViewV1 retain_library_view;
GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 copy_library_snapshot;
GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 release_library_view;
uint64_t library_generation = 0;
void require(int32_t error, const char *where) {
	if (!error) {
		return;
	}
	std::fprintf(stderr, "PHS1_FIXTURE_FATAL tag=%s where=%s error=%d\n", PROBE_TAG, where, error);
	std::_Exit(91);
}

GDExtensionInterfaceDeviceSessionQualificationRecordV1 record_ticket;
GDExtensionInterfaceDeviceSessionQualificationObserveV1 observe_ticket;
GDExtensionInterfaceDeviceSessionQualificationFactsV1 facts_ticket;
GDExtensionInterfaceDeviceSessionQualificationCopyV1 copy_ticket;
GDExtensionInterfaceDeviceSessionQualificationReleaseV1 release_ticket;
GDExtensionInterfaceStringNameNewWithUtf8Chars name_new;
GDExtensionInterfaceVariantGetPtrDestructor type_destroy;
GDExtensionInterfaceVariantGetPtrConstructor type_construct;
GDExtensionInterfaceClassdbConstructObject2 construct_object;
GDExtensionInterfaceClassdbGetMethodBind get_method;
GDExtensionInterfaceObjectMethodBindPtrcall call_method;
GDExtensionInterfaceGetVariantFromTypeConstructor from_type;
GDExtensionInterfaceVariantNewNil variant_nil;
GDExtensionInterfaceVariantDestroy variant_destroy;
GDExtensionInterfaceArraySetTyped set_array_type;
GDExtensionInterfaceVariantGetPtrBuiltinMethod get_builtin;
struct alignas(8) Opaque {
	unsigned char bytes[64] = {};
};
void method(const char *klass, const char *name, uint64_t hash, void *object, const GDExtensionConstTypePtr *args, void *out = nullptr) {
	Opaque k, n;
	name_new(&k, klass);
	name_new(&n, name);
	auto m = get_method(&k, &n, hash);
	if (!m) {
		std::_Exit(130);
	}
	call_method(m, object, args, out);
	type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&k);
	type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&n);
}
uint64_t set_create(void *rd, uint64_t shader, const std::array<GDExtensionStorageBackingV1, 8> &buffers) {
	Opaque klass;
	name_new(&klass, "RDUniform");
	Opaque array, nil;
	type_construct(GDEXTENSION_VARIANT_TYPE_ARRAY, 0)(&array, nullptr);
	variant_nil(&nil);
	set_array_type(&array, GDEXTENSION_VARIANT_TYPE_OBJECT, &klass, &nil);
	Opaque append;
	name_new(&append, "append");
	auto fn = get_builtin(GDEXTENSION_VARIANT_TYPE_ARRAY, &append, 3316032543);
	if (!fn) {
		std::_Exit(132);
	}
	for (uint32_t i = 0; i < 8; ++i) {
		void *uniform = construct_object(&klass);
		if (!uniform) {
			std::_Exit(131);
		}
		int64_t kind = 8, binding = i;
		const void *args[] = { &kind };
		method("RDUniform", "set_uniform_type", 1664894931, uniform, args);
		args[0] = &binding;
		method("RDUniform", "set_binding", 1286410249, uniform, args);
		args[0] = &buffers[i].rid;
		method("RDUniform", "add_id", 2722037293, uniform, args);
		Opaque object_variant;
		from_type(GDEXTENSION_VARIANT_TYPE_OBJECT)(&object_variant, &uniform);
		args[0] = &object_variant;
		fn(&array, args, nullptr, 1);
		variant_destroy(&object_variant);
	}
	int64_t index = 0;
	uint64_t set = 0;
	const void *args[] = { &array, &shader, &index };
	method("RenderingDevice", "uniform_set_create", 2280795797, rd, args, &set);
	variant_destroy(&nil);
	type_destroy(GDEXTENSION_VARIANT_TYPE_ARRAY)(&array);
	type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&klass);
	type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&append);
	if (!set) {
		std::_Exit(133);
	}
	return set;
}
GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1 rt_configure;
GDExtensionInterfaceDeviceSessionQualificationRtReserveV1 rt_reserve;
GDExtensionInterfaceDeviceSessionQualificationRtActivateV1 rt_activate;
GDExtensionInterfaceDeviceSessionQualificationRtObserveV1 rt_observe;
GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1 rt_release;
uint64_t rt_operation = 1;
bool rt_configured = false, rt_abort_tested = false;
GDExtensionDeviceSessionResultV1 invoke_rt(GDExtensionQualificationBridgeRefV1 bridge, uint32_t kind, const void *request, uint32_t request_size, const void *payload, uint64_t payload_size, void *out, uint32_t out_size, GDExtensionQualificationUploadSegmentV1 *segments = nullptr, uint32_t segment_capacity = 0) {
	memset(out, 0, out_size);
	if (segment_capacity) {
		memset(segments, 0, segment_capacity * sizeof(*segments));
	}
	if (!rt_configured) {
		GDExtensionQualificationRtConfigV1 config{ { 1, sizeof(config), 0 }, 1, 312, 200, 128, 65536 };
		require(rt_configure(bridge, &config).native_error, "rt_configure");
		rt_configured = true;
	}
	GDExtensionQualificationRtRequestV1 header{ { 1, sizeof(header), 0 }, kind, 0, rt_operation++, payload_size, request_size, out_size, segment_capacity, 0 };
	GDExtensionQualificationRtRefV1 ref{}, again{};
	auto accepted = rt_reserve(bridge, &header, request, payload, &ref);
	if (accepted.native_error) {
		return accepted;
	}
	require(rt_activate(bridge, ref).native_error, "rt_activate");
	GDExtensionQualificationRtObservationV1 observation{};
	for (unsigned pass = 0; pass < 5000; ++pass) {
		require(rt_observe(bridge, ref, &observation, out, out_size, segments, segment_capacity).native_error, "rt_observe");
		if (observation.state != 1) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (observation.state != 2 || observation.kind != kind || observation.operation_serial != header.operation_serial || observation.slot_generation != ref.handle_serial || observation.result_bytes != out_size || observation.abi.struct_size != sizeof(observation)) {
		std::_Exit(174);
	}
	again = {};
	require(rt_reserve(bridge, &header, request, payload, &again).native_error, "rt_returned_reserve_retry");
	if (memcmp(&ref, &again, sizeof(ref))) {
		std::_Exit(175);
	}
	require(rt_release(bridge, ref).native_error, "rt_owner_ack");
	if (!rt_release(bridge, ref).native_error) {
		std::_Exit(176);
	}
	std::fprintf(stderr, "QRT_NATIVE_RETURN kind=%u operation=%llu slot=%llu result=%u segments=%u error=%d reply_ack=1\n", kind, (unsigned long long)header.operation_serial, (unsigned long long)ref.handle_serial, observation.result_bytes, observation.segment_count, observation.call_result.native_error);
	return observation.call_result;
}
GDExtensionDeviceSessionResultV1 wrapped_storage_create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionStorageCreateRequestV1 *q, GDExtensionStorageBackingV1 *o) {
	auto r = invoke_rt(b, 2, q, sizeof(*q), nullptr, 0, o, sizeof(*o));
	if (!r.native_error) {
		GDExtensionQualificationBackingV1 backing{};
		auto observed = invoke_rt(b, 1, &o->rid, 8, nullptr, 0, &backing, sizeof(backing));
		require(observed.native_error, "rt_backing");
		if (backing.rid != o->rid || backing.backing_serial != o->backing_serial) {
			std::_Exit(177);
		}
	}
	return r;
}
GDExtensionDeviceSessionResultV1 wrapped_storage_retire(GDExtensionQualificationBridgeRefV1 b, const GDExtensionStorageRetireRequestV1 *q, GDExtensionStorageRetireRefV1 *r, GDExtensionStorageRetireReportV1 *o) {
	struct Pair {
		GDExtensionStorageRetireRefV1 ref;
		GDExtensionStorageRetireReportV1 report;
	} pair{};
	static_assert(sizeof(pair) == 136);
	auto result = invoke_rt(b, 3, q, sizeof(*q), nullptr, 0, &pair, sizeof(pair));
	*r = pair.ref;
	*o = pair.report;
	return result;
}
GDExtensionDeviceSessionResultV1 wrapped_unit_create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionNativeUnitCreateRequestV1 *q, const void *p, uint64_t n, GDExtensionNativeUnitBackingV1 *o) {
	return invoke_rt(b, 4, q, sizeof(*q), p, n, o, sizeof(*o));
}
GDExtensionDeviceSessionResultV1 wrapped_unit_retire(GDExtensionQualificationBridgeRefV1 b, const GDExtensionNativeUnitRetireRequestV1 *q, GDExtensionNativeUnitRetireRefV1 *r, GDExtensionNativeUnitRetireReportV1 *o) {
	struct Pair {
		GDExtensionNativeUnitRetireRefV1 ref;
		GDExtensionNativeUnitRetireReportV1 report;
	} pair{};
	static_assert(sizeof(pair) == 184);
	auto result = invoke_rt(b, 5, q, sizeof(*q), nullptr, 0, &pair, sizeof(pair));
	*r = pair.ref;
	*o = pair.report;
	return result;
}

GDExtensionDeviceSessionResultV1 wrapped_record(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationRecordV1 *q, GDExtensionQualificationTicketRefV1 *t, GDExtensionQualificationReportV1 *o) {
	struct Pair {
		GDExtensionQualificationTicketRefV1 ref;
		GDExtensionQualificationReportV1 report;
	} pair{};
	static_assert(sizeof(pair) == 200);
	auto result = invoke_rt(b, 6, q, sizeof(*q), nullptr, 0, &pair, sizeof(pair));
	*t = pair.ref;
	*o = pair.report;
	return result;
}
GDExtensionDeviceSessionResultV1 wrapped_prepare(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationUploadPrepareV1 *q, const GDExtensionQualificationUploadPlanV1 *p, uint32_t n, GDExtensionQualificationTicketRefV1 *t, GDExtensionQualificationReportV1 *o) {
	struct Pair {
		GDExtensionQualificationTicketRefV1 ref;
		GDExtensionQualificationReportV1 report;
	} pair{};
	auto result = invoke_rt(b, 7, q, sizeof(*q), p, uint64_t(n) * sizeof(*p), &pair, sizeof(pair));
	*t = pair.ref;
	*o = pair.report;
	return result;
}
GDExtensionDeviceSessionResultV1 wrapped_upload(GDExtensionQualificationBridgeRefV1 b, GDExtensionQualificationTicketRefV1 t, const GDExtensionQualificationUploadRequestV1 *q, const void *p, uint32_t n, GDExtensionQualificationUploadSegmentV1 *s, uint32_t c, GDExtensionQualificationUploadReportV1 *o) {
	std::vector<uint8_t> payload(16 + size_t(n));
	memcpy(payload.data(), &t, 16);
	memcpy(payload.data() + 16, p, n);
	return invoke_rt(b, 8, q, sizeof(*q), payload.data(), payload.size(), o, sizeof(*o), s, c);
}
GDExtensionDeviceSessionResultV1 wrapped_prepared(GDExtensionQualificationBridgeRefV1 b, GDExtensionQualificationTicketRefV1 t, const GDExtensionQualificationRecordV1 *q, GDExtensionQualificationReportV1 *o) {
	return invoke_rt(b, 9, q, sizeof(*q), &t, sizeof(t), o, sizeof(*o));
}
GDExtensionInterfaceDeviceSessionQualificationCancelV1 cancel_ticket;
std::vector<uint8_t> file_bytes(const std::filesystem::path &p, uint64_t limit = 67108864) {
	std::ifstream f(p, std::ios::binary | std::ios::ate);
	if (!f) {
		std::_Exit(181);
	}
	auto end = f.tellg();
	if (end < 0 || uint64_t(end) > limit) {
		std::_Exit(181);
	}
	std::vector<uint8_t> bytes(static_cast<size_t>(end));
	f.seekg(0);
	if (!bytes.empty()) {
		f.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	}
	if (!f) {
		std::_Exit(181);
	}
	return bytes;
}
bool matched_hash(const std::vector<uint8_t> &bytes, const std::array<uint8_t, 32> &expected) {
	std::array<uint8_t, 32> actual{};
	CC_SHA256(bytes.data(), bytes.size(), actual.data());
	return actual == expected;
}
uint32_t read_word(const std::vector<uint8_t> &b, size_t offset) {
	if (offset + 4 > b.size()) {
		std::_Exit(182);
	}
	return uint32_t(b[offset]) | uint32_t(b[offset + 1]) << 8 | uint32_t(b[offset + 2]) << 16 | uint32_t(b[offset + 3]) << 24;
}
int32_t ready(GDExtensionDeviceExitSceneReadyContextRef context, const GDExtensionSceneInstallPlanV2 &scene_plan) {
 if(++state.calls!=1)return ERR_ALREADY_IN_USE;
 expired_ready=context;
 auto *tree=godot::SceneTree::get_singleton();
 if(!tree || tree->get_instance_id()!=scene_plan.tree_id)return ERR_INVALID_DATA;
 godot::internal::retain_guarded_scene_binding(tree,godot::internal::GuardedSceneBinding::Tree,godot::GDExtensionBinding::active_guarded_user()->exit_api,context);
 auto create=reinterpret_cast<int(*)()>(dlsym(RTLD_DEFAULT,"k05_private_fixture_create"));
 if(!create)return ERR_UNAVAILABLE;
 return create();
}
void initialize(godot::ModuleInitializationLevel level) {
	if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	auto *data = godot::GDExtensionBinding::active_guarded_user();
	GDExtensionDeviceExitInitializationLevelContextRef frame{};
	require(data->exit_api.borrow_initialize_level(&abi, library, GDEXTENSION_INITIALIZATION_SCENE, &frame), "sdk_original_initialize_frame");
	GDExtensionDeviceExitGuardedLibraryRef lib{};
	GDExtensionDeviceExitRegisteredCleanupRef terminal{};
	require(borrow_library(&abi, library, &lib), "sdk_borrow_library");
	require(borrow_cleanup(&abi, frame, 1, &terminal), "sdk_binding_terminal");
	require(acquire_hold(&abi, lib, terminal, &code_hold), "sdk_code_hold");

	(void)godot::Engine::get_singleton();
}
godot::ModuleInitializationLevel terminating_level = godot::MODULE_INITIALIZATION_LEVEL_MAX;
void terminate(godot::ModuleInitializationLevel level) {
if(level==godot::MODULE_INITIALIZATION_LEVEL_SCENE){
  auto cleanup=reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT,"k05_private_fixture_cleanup"));if(cleanup)cleanup();
 }
	terminating_level = level;
	std::printf("ORIGINAL_SDK_USER_TERMINATOR level=%u\n", unsigned(level));
	std::fflush(stdout);
}
int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef frame, void *) {
	auto *data = godot::GDExtensionBinding::guarded_claimant;
	const GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(GDExtensionDeviceExitExitDiagnosticV1), 0 }, 0, UINT32_MAX, 0 };
	return data->exit_api.publish_exit_result(&abi, data->guard, frame, &diagnostic);
}
GDExtensionInterfaceGetProcAddress original_get;
int32_t binding_terminal(const GDExtensionDeviceExitAbi *request, GDExtensionDeviceExitDeinitLevelContextRef frame) {
	// Observe the SDK's real post-ClassDB binding terminal, before the original
	// registered acknowledgement. Do not mint a frame, stage, or API return.
	if (terminating_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE && code_hold.handle_serial) {
		auto error = release_hold(&abi, code_hold);
		if (error) {
			return error;
		}
		code_hold = {};
		std::puts("ORIGINAL_CODE_HOLD_BINDING_RETURN stage=2");
		std::fflush(stdout);
	}
	return after_binding(request, frame);
}
GDExtensionInterfaceFunctionPtr lookup(const char *name) {
	if (std::strcmp(name, "device_exit_after_binding_deinit_v1") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&binding_terminal);
	}
	return original_get(name);
}
template <class T>
bool load(GDExtensionInterfaceGetProcAddress get, const char *name, T &out) {
	out = reinterpret_cast<T>(get(name));
	return out != nullptr;
}
} //namespace
extern "C" GDExtensionBool GDE_EXPORT numeric_control_init(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *result) {
#define LOAD(m_name, m_out) \
	if (!load(get, m_name, m_out)) { \
		std::fprintf(stderr, "ORIGINAL_MISSING_PROC %s\n", m_name); \
		return false; \
	}
	LOAD("device_session_qualification_rt_configure_v1", rt_configure)
	LOAD("device_session_qualification_rt_reserve_v1", rt_reserve)
	LOAD("device_session_qualification_rt_activate_v1", rt_activate)
	LOAD("device_session_qualification_rt_observe_v1", rt_observe)
	LOAD("device_session_qualification_rt_release_v1", rt_release)
	LOAD("device_session_qualification_observe_v1", observe_ticket)
	LOAD("device_session_qualification_facts_v1", facts_ticket)
	LOAD("device_session_qualification_copy_v1", copy_ticket)
	LOAD("device_session_qualification_release_v1", release_ticket)
	LOAD("device_session_qualification_cancel_v1", cancel_ticket)
	LOAD("device_session_qualification_storage_retire_observe_v1", storage_observe)
	LOAD("device_session_qualification_storage_retire_release_v1", storage_release)
	LOAD("device_session_qualification_native_unit_retire_observe_v1", unit_observe)
	LOAD("device_session_qualification_native_unit_retire_release_v1", unit_release)
	LOAD("device_session_bind_main_scene_ready_v1", bind_main)
	LOAD("device_session_qualification_open_v1", open_bridge)
	LOAD("device_session_qualification_close_v1", close_bridge)
	LOAD("device_session_release_context_v1", release_context)
	LOAD("device_session_release_session_v1", release_session)
	LOAD("device_exit_borrow_guarded_library_v1", borrow_library)
	LOAD("device_exit_borrow_level_cleanup_v1", borrow_cleanup)
	LOAD("device_exit_acquire_code_hold_v1", acquire_hold)
	LOAD("device_exit_release_code_hold_v1", release_hold)
	LOAD("device_exit_after_binding_deinit_v1", after_binding)
	LOAD("device_exit_bootstrap_library_v1", bootstrap_library)
	LOAD("device_exit_retain_library_view_v1", retain_library_view)
	LOAD("device_exit_copy_library_snapshot_v1", copy_library_snapshot)
	LOAD("device_exit_release_library_view_v1", release_library_view)
	LOAD("object_get_instance_from_id", object_from_id)
	LOAD("string_name_new_with_utf8_chars", name_new)
	LOAD("variant_get_ptr_destructor", type_destroy)
	LOAD("variant_get_ptr_constructor", type_construct)
	LOAD("classdb_construct_object2", construct_object)
	LOAD("classdb_get_method_bind", get_method)
	LOAD("object_method_bind_ptrcall", call_method)
	LOAD("get_variant_from_type_constructor", from_type)
	LOAD("variant_new_nil", variant_nil)
	LOAD("variant_destroy", variant_destroy)
	LOAD("array_set_typed", set_array_type)
	LOAD("variant_get_ptr_builtin_method", get_builtin)
#undef LOAD
	storage_create = &wrapped_storage_create;
	storage_retire = &wrapped_storage_retire;
	unit_create = &wrapped_unit_create;
	unit_retire = &wrapped_unit_retire;
	library = owner;
	GDExtensionDeviceExitLibraryCapability capability{};
	GDExtensionDeviceExitLibraryViewRef view{};
	require(bootstrap_library(&abi, library, &capability), "sdk_bootstrap_original");
	require(retain_library_view(&abi, capability, &view), "sdk_retain_original_view");
	GDExtensionDeviceExitLibrarySnapshot snapshot{};
	snapshot.abi = { 1, sizeof(snapshot), 0 };
	require(copy_library_snapshot(view, &snapshot), "sdk_original_snapshot");
	library_generation = snapshot.library_generation;
	require(release_library_view(&abi, view), "sdk_release_original_view");
	original_get = get;
	godot::GDExtensionBinding::InitObject object(lookup, owner, result);
	object.register_initializer(initialize);
	object.register_terminator(terminate);
	object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	const GDExtensionSceneInstallRequirementsV2 requirements{ { 1, sizeof(GDExtensionSceneInstallRequirementsV2), 0 }, GDEXTENSION_SCENE_INSTALL_BASE_V2 | GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2, 0, 0 };
	object.register_scene_ready_callback_v2(ready, requirements);
	object.require_main_rd_qualification();
	GDExtensionDeviceExitGuardRegistrationV1 config{};
	config.level_bitmap = config.binding_cleanup_bitmap = config.user_cleanup_bitmap = 12;
	config.entry_classes = (1u << 0) | (1u << 1) | (0x3fu << 2) | (1u << 8);
	config.needs[GDEXTENSION_INITIALIZATION_SCENE] = { (1u << 1) | (1u << 5) | (1u << 7), 1u << 7 };
	config.needs[GDEXTENSION_INITIALIZATION_EDITOR] = { 0, 1u << 7 };
	config.preflight = preflight;
	object.set_guard_configuration(config);
	return object.init();
}
__attribute__((destructor)) static void unloaded() {
	std::puts("ORIGINAL_NUMERIC_DSO_UNLOAD");
	std::fflush(stdout);
}
