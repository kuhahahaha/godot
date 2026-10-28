#include "expected_artifacts.h"
#include "expected_unit.h"
#include "original_unit.h"
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
GDExtensionQualificationRtRefV1 retained_record_rt{};
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
	if (kind == 9 && observation.call_result.native_error == 0) {
		if (retained_record_rt.handle_serial) {
			std::_Exit(209);
		}
		retained_record_rt = ref;
		std::printf("LATE_RECORD_REPLY_HELD operation=%llu slot=%llu state=%u observed=1 ack=0\n", (unsigned long long)header.operation_serial, (unsigned long long)ref.handle_serial, observation.state);
		return observation.call_result;
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
GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1 direct_storage_retire;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1 direct_unit_retire;
GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV1 direct_prepared;
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
	if (++state.calls != 1) {
		std::_Exit(183);
	}
	expired_ready = context;
	auto *tree = godot::SceneTree::get_singleton();
	if (!tree || tree->get_instance_id() != scene_plan.tree_id) {
		return ERR_INVALID_DATA;
	}
	godot::internal::retain_guarded_scene_binding(tree, godot::internal::GuardedSceneBinding::Tree, godot::GDExtensionBinding::active_guarded_user()->exit_api, context);
	auto built = numeric_control::make_unit();
	if (!built.has_value()) {
		std::fprintf(stderr, "ORIGINAL_FACTORY_ERROR field=%.*s\n", int(built.error().field_id.size()), built.error().field_id.data());
		return ERR_INVALID_DATA;
	}
	// This actual retained C04 unit owns its original recipe/catalog/profile,
	// both source/interface views and source maps through all native operations.
	const auto &unit = built.value();
	const auto &occurrence = unit.descriptor();
	const auto &recipe = unit.recipe();
	if (unit.identity().bytes != expected_unit_digest) {
		return ERR_INVALID_DATA;
	}
	std::printf("ORIGINAL_RETAINED_UNIT ordinal=%u unit=", occurrence.ordinal);
	for (auto b : unit.identity().bytes) {
		std::printf("%02x", unsigned(b));
	}
	std::printf(" flags=%u same_exported_identity=1\n", unit.required_compile_flags());
	const char *directory = std::getenv("NUMERIC_CONTROL_INPUT");
	if (!directory) {
		return ERR_INVALID_PARAMETER;
	}
	const std::filesystem::path input = directory;
	const char *result_directory = std::getenv("NUMERIC_CONTROL_OUTPUT");
	if (!result_directory) {
		return ERR_INVALID_PARAMETER;
	}
	bool cancel_prefix = false; // This package runs only the post-submit case.
	const auto receipt = file_bytes(input.parent_path() / "NATIVE_GENERATION_RECEIPT.json", unit.profile()->data().limits.max_source_bytes_per_unit);
	if (!matched_hash(receipt, expected_generation_receipt)) {
		return ERR_INVALID_DATA;
	}
	auto first_physical = unit.unit(0);
	if (!first_physical.has_value() || first_physical.value().expectation->descriptors.size() != 8) {
		return ERR_INVALID_DATA;
	}
	const auto &physical = first_physical.value();
	std::array<std::vector<uint8_t>, 2> containers;

	std::array<std::vector<uint8_t>, 8> initial;
	for (uint32_t i = 0; i < 8; ++i) {
		initial[i] = file_bytes(input / ("role" + std::to_string(i) + ".bin"), physical.expectation->descriptors[i].byte_extent);
	}
	auto actual_input = ANGE::particle::fixed_numeric_input(recipe, occurrence.ordinal);
	auto actual_output = ANGE::particle::fixed_numeric_output_initial(recipe, occurrence.ordinal);
	if (!actual_input.has_value() || !actual_output.has_value() || initial[6] != actual_input.value() || initial[7] != actual_output.value()) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < 2; ++i) {
		auto view = unit.unit(i);
		if (!view.has_value()) {
			return ERR_INVALID_DATA;
		}
		auto source = file_bytes(input / (i ? "body.comp" : "capture.comp"), view.value().source.size());
		if (!matched_hash(source, expected_sources[i])) {
			return ERR_INVALID_DATA;
		}
		if (source.size() != view.value().source.size() || std::memcmp(source.data(), view.value().source.data(), source.size())) {
			return ERR_INVALID_DATA;
		}
		auto raw = file_bytes(input / (i ? "body.spv" : "capture.spv"), unit.profile()->data().limits.max_spirv_bytes_per_unit);
		if (!matched_hash(raw, expected_spirvs[i])) {
			return ERR_INVALID_DATA;
		}
		containers[i] = file_bytes(input / (i ? "body.bin" : "capture.bin"));
		if (!matched_hash(containers[i], expected_containers[i])) {
			std::fprintf(stderr, "ORIGINAL_ARTIFACT_PRECHECK_FAILURE physical=%u part=container before_bridge=1\n", i);
			return ERR_INVALID_DATA;
		}
		if (raw.empty() || raw.size() % 4) {
			return ERR_INVALID_DATA;
		}
		std::vector<uint32_t> words(raw.size() / 4);
		std::memcpy(words.data(), raw.data(), raw.size());
		SpvReflectAllocationContext account{};
		account.max_live_bytes = original_reflection_bytes[i];
		account.max_work = original_reflection_work[i];
		auto checked = ANGE::particle::validate_spirv_interface(unit, i, words.data(), words.size(), account);
		if (!checked.has_value()) {
			std::fprintf(stderr, "ORIGINAL_INTERFACE_ERROR physical=%u field=%.*s work=%llu\n", i, int(checked.error().field_id.size()), checked.error().field_id.data(), (unsigned long long)account.work);
			std::fprintf(stderr, "REFLECT_ACCOUNT max_bytes=%llu max_work=%llu peak=%llu live=%llu requested=%llu failure=%u ids=%llu elements=%llu\n", (unsigned long long)account.max_live_bytes, (unsigned long long)account.max_work, (unsigned long long)account.peak_live_bytes, (unsigned long long)account.live_bytes, (unsigned long long)account.total_requested_bytes, unsigned(account.failure), (unsigned long long)unit.profile()->data().limits.max_spirv_id_bound, (unsigned long long)unit.profile()->data().limits.max_resources_per_stage);
			return ERR_INVALID_DATA;
		}
		if (account.live_bytes) {
			return ERR_INVALID_DATA;
		}
		std::printf("ORIGINAL_INTERFACE_MATCH physical=%u source_bytes=%zu spv_bytes=%zu flags=%u retained_unit=1 work=%llu\n", i, source.size(), raw.size(), unit.required_compile_flags(), (unsigned long long)account.work);
	}
	GDExtensionDeviceRegistryContextRef device{};
	GDExtensionOriginalDeviceSessionRef session{};
	uint64_t id = 0;
	require(bind_main(&abi, context, &device, &session, &id).native_error, "bind_main");
	GDExtensionQualificationBridgeRefV1 bridge{};
	require(open_bridge(&abi, context, device, session, code_hold, &bridge).native_error, "open_bridge");
	std::array<GDExtensionStorageBackingV1, 8> buffers{};
	for (uint32_t i = 0; i < 8; ++i) {
		const auto extent = physical.expectation->descriptors[i].byte_extent;
		if (extent != initial[i].size()) {
			std::_Exit(184);
		}
		GDExtensionStorageCreateRequestV1 request{ abi, extent, i, 0, 41, 1, 0 };
		require(storage_create(bridge, &request, &buffers[i]).native_error, "original_storage_create");
	}
	GDExtensionNativeUnitBackingV1 natives[2]{};
	for (uint32_t i = 0; i < 2; ++i) {
		const auto &container = containers[i];
		GDExtensionNativeUnitCreateRequestV1 request{};
		request.abi = abi;
		request.library_generation = library_generation;
		request.preparation_generation = 21;
		request.operation_serial = i + 1;
		request.container_byte_count = container.size();
		request.record_slot = 2;
		request.physical_unit = i;
		request.expected_flags = unit.required_compile_flags();
		CC_SHA256(container.data(), container.size(), request.container_digest);
		require(unit_create(bridge, &request, container.data(), container.size(), &natives[i]).native_error, "original_native_unit_create");
		if (natives[i].state != 1 || natives[i].known_mask != 3 || natives[i].possible_mask || natives[i].effective_flags != request.expected_flags) {
			std::_Exit(185);
		}
	}
	void *rd = object_from_id(id);
	const uint64_t sets[] = { set_create(rd, natives[0].shader_rid, buffers), set_create(rd, natives[1].shader_rid, buffers) };
	GDExtensionQualificationRecordV1 request{};
	request.abi = abi;
	request.record_generation = 19;
	request.request_nonce = 23;
	request.library_generation = library_generation;
	request.qualification_session_serial = 7;
	request.range_end = request.sample_count = occurrence.count;
	request.attempt = 1;
	request.body_groups_x = (occurrence.count + 63) / 64;
	request.capture_pipeline = natives[0].pipeline_rid;
	request.capture_set = sets[0];
	request.body_pipeline = natives[1].pipeline_rid;
	request.body_set = sets[1];
	request.readback_buffer = buffers[7].rid;
	request.source_allocation_generation = 41;
	request.observed_backing_serial = buffers[7].backing_serial;
	request.readback_size = occurrence.observed_bytes;
	std::memcpy(request.unit_digest, unit.identity().bytes.data(), 32);
	const uint32_t push[] = { 0, occurrence.count, 0, 0 };
	std::memcpy(request.capture_push, push, 16);
	std::memcpy(request.body_push, push, 16);
	GDExtensionQualificationUploadPrepareV1 preparation{};
	preparation.abi = abi;
	preparation.record_generation = request.record_generation;
	preparation.request_nonce = request.request_nonce;
	preparation.library_generation = library_generation;
	preparation.qualification_session_serial = request.qualification_session_serial;
	preparation.parent_serial = 91;
	preparation.reservation_serial = 97;
	preparation.attempt = 1;
	preparation.upload_count = 8;
	preparation.expected_result_bytes = occurrence.observed_bytes;
	std::memcpy(preparation.unit_digest, request.unit_digest, 32);
	GDExtensionQualificationUploadPlanV1 plans[8]{};
	for (uint32_t i = 0; i < 8; ++i) {
		auto &p = plans[i];
		p.abi = abi;
		p.upload_index = i;
		p.role = i;
		p.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1;
		p.rid = buffers[i].rid;
		p.backing_serial = buffers[i].backing_serial;
		p.byte_size = initial[i].size();
		p.e05_backing = buffers[i].backing;
		CC_SHA256(initial[i].data(), initial[i].size(), p.payload_digest);
		preparation.max_segments += (p.byte_size + 31) / 32;
	}
	GDExtensionQualificationTicketRefV1 ticket{};
	GDExtensionQualificationReportV1 report{};
	require(wrapped_prepare(bridge, &preparation, plans, 8, &ticket, &report).native_error, "prepare_original_roles");
	const GDExtensionQualificationBudgetV1 budget{ abi, 1, 1000, 0 };
	for (uint32_t i = 0; i < (cancel_prefix ? 1u : 8u); ++i) {
		auto &p = plans[i];
		GDExtensionQualificationUploadRequestV1 q{};
		q.abi = abi;
		q.record_generation = request.record_generation;
		q.parent_serial = preparation.parent_serial;
		q.upload_index = i;
		q.rid = p.rid;
		q.backing_serial = p.backing_serial;
		q.byte_size = p.byte_size;
		q.role = p.role;
		q.owner_kind = p.owner_kind;
		q.e05_backing = p.e05_backing;
		std::memcpy(q.payload_digest, p.payload_digest, 32);
		std::vector<GDExtensionQualificationUploadSegmentV1> segments((p.byte_size + 31) / 32);
		GDExtensionQualificationUploadReportV1 uploaded{};
		for (uint32_t pass = 0; pass < 5000; ++pass) {
			q.expected_revision = report.revision;
			require(wrapped_upload(bridge, ticket, &q, initial[i].data(), initial[i].size(), segments.data(), segments.size(), &uploaded).native_error, "upload_original_role");
			require(observe_ticket(bridge, ticket, &budget, &report).native_error, "observe_upload");
			if (uploaded.state == 1) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (uploaded.state != 1 || uploaded.unappended_begin != p.byte_size || uploaded.possible_count) {
			std::_Exit(186);
		}
	}
	if (cancel_prefix) {
		require(cancel_ticket(bridge, ticket).native_error, "cancel_actual_upload_prefix");
		GDExtensionQualificationReportV1 refused{};
		auto after = wrapped_prepared(bridge, ticket, &request, &refused);
		std::printf("ORIGINAL_CANCEL_REFUSAL error=%d reason=%u no_new_record=1\n", after.native_error, after.reason);
		if (after.native_error != ERR_BUSY || after.reason != 16) {
			std::_Exit(187);
		}
	} else {
		auto wrong = request;
		wrong.unit_digest[0] ^= 1;
		GDExtensionQualificationReportV1 rejected{};
		auto identity_refused = wrapped_prepared(bridge, ticket, &wrong, &rejected);
		if (identity_refused.native_error != ERR_INVALID_DATA || retained_record_rt.handle_serial) {
			std::_Exit(224);
		}
		std::printf("LATE_IDENTITY_FAILURE error=%d no_record_reply=1 original_ticket_preserved=1\n", identity_refused.native_error);
		for (uint32_t pass = 0; pass < 5000; ++pass) {
			auto result = wrapped_prepared(bridge, ticket, &request, &report);
			if (!result.native_error) {
				break;
			}
			if (result.native_error != ERR_BUSY || result.reason != 19) {
				require(result.native_error, "record_original_pair");
			}
			require(observe_ticket(bridge, ticket, &budget, &report).native_error, "prior_upload_return");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
#include "late_cut.inc"
	for (uint32_t pass = 0; pass < 5000; ++pass) {
		require(observe_ticket(bridge, ticket, &budget, &report).native_error, "same_request_async_observe");
		if (cancel_prefix ? (report.state == 7 && report.phase == 6) : report.state == 8) {
			break;
		}
		if (report.state == 5 || report.state == 6 || (report.state == 7 && !cancel_prefix)) {
			std::fprintf(stderr, "ORIGINAL_TICKET_ERROR state=%u phase=%u native=%d possible=%u\n", report.state, report.phase, report.native_error, report.possible_count);
			std::_Exit(188);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (cancel_prefix ? (report.state != 7 || report.phase != 6 || report.owned_size) : (report.state != 8 || report.owned_size != occurrence.observed_bytes || !report.readback_request)) {
		std::_Exit(189);
	}
	std::vector<GDExtensionQualificationFactV1> facts(report.fact_count);
	uint32_t written = 0;
	if (facts.empty()) {
		std::_Exit(190);
	}
	for (uint32_t first = 0; first < facts.size();) {
		uint32_t n = 0;
		require(facts_ticket(bridge, ticket, report.revision, first, std::min<uint32_t>(64, facts.size() - first), facts.data() + first, &n).native_error, "exact_native_facts");
		if (!n) {
			std::_Exit(191);
		}
		first += n;
	}
	for (const auto &f : facts) {
		if (f.issue != 1 || f.completion != 2 || !f.native_serial || !f.completion_serial || !f.backend_queue || f.error) {
			std::_Exit(192);
		}
	}
	{
		std::ofstream out(std::filesystem::path(result_directory) / "facts.bin", std::ios::binary);
		out.write(reinterpret_cast<const char *>(facts.data()), facts.size() * sizeof(facts[0]));
		if (!out) {
			std::_Exit(198);
		}
		std::ofstream report_file(std::filesystem::path(result_directory) / "report.bin", std::ios::binary);
		report_file.write(reinterpret_cast<const char *>(&report), sizeof(report));
		if (!report_file) {
			std::_Exit(198);
		}
	}
	for (uint32_t i = 0; i < facts.size(); ++i) {
		const auto &f = facts[i];
		std::printf("ORIGINAL_NATIVE_FACT index=%u semantic=%u graph=%llu:%llu:%u:%u:%u slot=%u queue=%u issue=%u completion=%u error=%u native=%llu completion_source=%llu backing=%llu offset=%llu bytes=%llu\n", i, f.semantic_kind, (unsigned long long)f.graph_instance, (unsigned long long)f.graph_frame, f.graph_kind, f.graph_index, f.graph_offset, f.command_slot, f.backend_queue, f.issue, f.completion, f.error, (unsigned long long)f.native_serial, (unsigned long long)f.completion_serial, (unsigned long long)f.source_backing_serial, (unsigned long long)f.source_offset, (unsigned long long)f.source_size);
	}
	std::printf("ORIGINAL_NATIVE_COMPLETION ticket=%llu revision=%llu readback_request=%llu facts=%zu known=%u possible=%u mode=%s\n", (unsigned long long)ticket.handle_serial, (unsigned long long)report.revision, (unsigned long long)report.readback_request, facts.size(), report.known_count, report.possible_count, cancel_prefix ? "post_submit_cancel" : "positive");
	std::vector<uint8_t> bytes(occurrence.observed_bytes), again(bytes.size());
	if (cancel_prefix) {
		if (!copy_ticket(bridge, ticket, report.revision, bytes.data(), bytes.size(), &written).native_error || written) {
			std::_Exit(193);
		}
	} else {
		require(copy_ticket(bridge, ticket, report.revision, bytes.data(), bytes.size(), &written).native_error, "exact_owned_readback");
		if (written != bytes.size()) {
			std::_Exit(194);
		}
		uint32_t repeated = 0;
		require(copy_ticket(bridge, ticket, report.revision, again.data(), again.size(), &repeated).native_error, "same_owned_readback_repeat");
		if (repeated != written || again != bytes) {
			std::_Exit(195);
		}
		const auto &original = actual_output.value();
		if (std::memcmp(bytes.data(), original.data(), 64) || std::memcmp(bytes.data() + 64 + 8 * occurrence.count, original.data() + 64 + 8 * occurrence.count, 32)) {
			std::_Exit(196);
		}
		for (uint32_t i = 0; i < occurrence.count; ++i) {
			const uint32_t index = occurrence.first_sample + i;
			ANGE::particle::fixed_numeric_data::Expected expected{};
			switch (occurrence.kind) {
				case ANGE::particle::FixedNumericSampleKind::FloatBoundary:
					expected = ANGE::particle::fixed_numeric_data::floats[index].metal;
					break;
				case ANGE::particle::FixedNumericSampleKind::IntegerBoundary:
					expected = ANGE::particle::fixed_numeric_data::integers[index].expected;
					break;
				case ANGE::particle::FixedNumericSampleKind::DivisionCorpus: {
					auto e = ANGE::particle::fixed_numeric_data::division[index];
					expected = { e.fault, 1, { e.bits, 0 } };
				} break;
			}
			const auto fault = read_word(bytes, 64 + 8 * i), bits = read_word(bytes, 68 + 8 * i);
			bool matched = false;
			for (uint32_t k = 0; k < expected.count; ++k) {
				matched |= bits == expected.bits[k];
			}
			if (fault != expected.fault || !matched) {
				std::_Exit(197);
			}
		}
		std::ofstream file(std::filesystem::path(result_directory) / "owned_readback.bin", std::ios::binary);
		file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
		if (!file) {
			std::_Exit(198);
		}
		std::printf("ORIGINAL_EXACT_READBACK bytes=%zu samples=%u recipe_header=1 guard=1 original_fixed_expected=1 repeated_owned_copy=1\n", bytes.size(), occurrence.count);
	}
#include "late_release.inc"
	require(release_ticket(bridge, ticket).native_error, "ticket_release");
	if (!release_ticket(bridge, ticket).native_error) {
		std::_Exit(223);
	}
	for (auto set : sets) {
		const void *args[] = { &set };
		method("RenderingDevice", "free_rid", 2722037293, rd, args);
	}
	for (uint32_t i = 0; i < 2; ++i) {
		GDExtensionNativeUnitRetireRequestV1 q{ abi, natives[i].unit, 3 + i, 0 };
		GDExtensionNativeUnitRetireRefV1 cell{};
		GDExtensionNativeUnitRetireReportV1 observed{};
		require(unit_retire(bridge, &q, &cell, &observed).native_error, "unit_retire");
		if (i == 0 && std::memcmp(&cell, &early_unit_ref, sizeof(cell))) {
			std::_Exit(225);
		}
		for (uint32_t p = 0; p < 5000 && observed.state == 0; ++p) {
			require(unit_observe(bridge, cell, &budget, &observed).native_error, "unit_physical_observe");
			if (!observed.state) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		if (observed.state != 1 || observed.freed_mask != 3 || !observed.shader_free_event_seq || !observed.pipeline_free_event_seq) {
			std::_Exit(199);
		}
		std::printf("ORIGINAL_UNIT_PHYSICAL_RETURN index=%u shader_event=%llu pipeline_event=%llu\n", i, (unsigned long long)observed.shader_free_event_seq, (unsigned long long)observed.pipeline_free_event_seq);
		require(unit_release(bridge, cell).native_error, "unit_ack");
		if (!unit_release(bridge, cell).native_error) {
			std::_Exit(227);
		}
	}
	for (uint32_t i = 0; i < 8; ++i) {
		GDExtensionStorageRetireRequestV1 q{ abi, buffers[i].backing, buffers[i].rid, buffers[i].backing_serial, i, 0, 41, 2, 0 };
		GDExtensionStorageRetireRefV1 cell{};
		GDExtensionStorageRetireReportV1 observed{};
		require(storage_retire(bridge, &q, &cell, &observed).native_error, "storage_retire");
		if (i == 7 && std::memcmp(&cell, &early_storage_ref, sizeof(cell))) {
			std::_Exit(226);
		}
		for (uint32_t p = 0; p < 5000 && observed.state == 0; ++p) {
			require(storage_observe(bridge, cell, &budget, &observed).native_error, "storage_physical_observe");
			if (!observed.state) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		if (observed.state != 1 || !observed.event_seq) {
			std::_Exit(200);
		}
		std::printf("ORIGINAL_STORAGE_PHYSICAL_RETURN role=%u backing=%llu event=%llu\n", i, (unsigned long long)buffers[i].backing_serial, (unsigned long long)observed.event_seq);
		require(storage_release(bridge, cell).native_error, "storage_ack");
		if (!storage_release(bridge, cell).native_error) {
			std::_Exit(228);
		}
	}
	require(close_bridge(bridge).native_error, "bridge_release");
	require(release_session(session).native_error, "session_release");
	require(release_context(device).native_error, "context_release");
	std::printf("ORIGINAL_NATIVE_CONTROL_DONE same_unit=1 same_ticket=1 rt_slots_released=1 physical_returns=1 bridge_closed=1 mode=%s\n", cancel_prefix ? "post_submit_cancel" : "positive");
	std::fflush(stdout);
	return OK;
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
	std::setvbuf(stdout, nullptr, _IONBF, 0);
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
	LOAD("device_session_qualification_storage_retire_v1", direct_storage_retire)
	LOAD("device_session_qualification_native_unit_retire_v1", direct_unit_retire)
	LOAD("device_session_qualification_record_prepared_v1", direct_prepared)
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
