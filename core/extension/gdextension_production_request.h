/**************************************************************************/
/*  gdextension_production_request.h                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/extension/gdextension_camera_ready.h"
#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_resource_use.h"
#include "core/extension/gdextension_record_source.h"
#include "core/extension/gdextension_scene_phase.h"
#include "servers/rendering/native_disposal_witness.h"
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/rendering_device.h"

#include <atomic>

class TestProductionResourceUseAccess;
class TestNativeDisposalAccess;
namespace TestProductionNativeObserve {
struct Access;
}
namespace TestProductionRequest {
struct Access;
}
namespace GDExtensionDeviceExit {
// The actual native source observer lends this scope to one protected typed
// delivery. Numeric camera identity is only a projection of its retained Ref.
class ProductionCameraReadySourceScope {
	friend class Registry;
	Registry *registry = nullptr;
	GDExtensionProductionAttachmentRefV1 attachment{};
	Ref<RendererViewport::Viewport::CameraCapture> source;
	GDExtensionProductionCameraSampleV1 sample{};
	// The original renderer drain pins this observer for the whole scope.
	const bool *admissions_closed = nullptr;
	ProductionCameraReadySourceScope *previous = nullptr;
	static thread_local ProductionCameraReadySourceScope *current;
	ProductionCameraReadySourceScope(Registry &, GDExtensionProductionAttachmentRefV1, const Ref<RendererViewport::Viewport::CameraCapture> &, const GDExtensionProductionCameraSampleV1 &, const bool *);

public:
	ProductionCameraReadySourceScope(const ProductionCameraReadySourceScope &) = delete;
	ProductionCameraReadySourceScope &operator=(const ProductionCameraReadySourceScope &) = delete;
	~ProductionCameraReadySourceScope();
	static const ProductionCameraReadySourceScope *get() { return current; }
	const Registry *get_registry() const { return registry; }
	GDExtensionProductionAttachmentRefV1 get_attachment() const { return attachment; }
	const Ref<RendererViewport::Viewport::CameraCapture> &get_source() const { return source; }
	const GDExtensionProductionCameraSampleV1 &get_sample() const { return sample; }
};
Error production_camera_sample(const RendererViewport::Viewport::CameraCapture &, GDExtensionProductionCameraSampleV1 &);
Error production_host_transform(const GDExtensionProductionHostTransformInputV1 &, GDExtensionProductionHostTransformV1 &);
// Production candidate DTO headers describe the containing DTO. The installed
// common DeviceExit header and qualification DTO rules are separate contracts.
template <class T>
constexpr GDExtensionDeviceExitAbi production_dto_abi() {
	return { 1, sizeof(T), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionObserveResultV3>() {
	return { 3, sizeof(GDExtensionProductionObserveResultV3), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionDispatchV2>() {
	return { 2, sizeof(GDExtensionProductionDispatchV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionCopyV2>() {
	return { 2, sizeof(GDExtensionProductionCopyV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionUploadV2>() {
	return { 2, sizeof(GDExtensionProductionUploadV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionAppendV2>() {
	return { 2, sizeof(GDExtensionProductionAppendV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionReportV2>() {
	return { 2, sizeof(GDExtensionProductionReportV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionSegmentV2>() {
	return { 2, sizeof(GDExtensionProductionSegmentV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionRecordReportV2>() {
	return { 2, sizeof(GDExtensionProductionRecordReportV2), 0 };
}
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionOutputPredecessorV2>() { return { 2, sizeof(GDExtensionProductionOutputPredecessorV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionOutputPrepareV2>() { return { 2, sizeof(GDExtensionProductionOutputPrepareV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionOutputPlanV2>() { return { 2, sizeof(GDExtensionProductionOutputPlanV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionOutputConsumedSelectorV2>() { return { 2, sizeof(GDExtensionOutputConsumedSelectorV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionOutputConsumedProofRefV2>() { return { 2, sizeof(GDExtensionOutputConsumedProofRefV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionOutputConsumedProofV2>() { return { 2, sizeof(GDExtensionOutputConsumedProofV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionOutputConsumedBorrowResultV2>() { return { 2, sizeof(GDExtensionOutputConsumedBorrowResultV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionPreparedResultRefV2>() { return { 2, sizeof(GDExtensionProductionPreparedResultRefV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionPreparedActionResultV2>() { return { 2, sizeof(GDExtensionProductionPreparedActionResultV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionOutputHistoryStaticV2>() { return { 2, sizeof(GDExtensionProductionOutputHistoryStaticV2), 0 }; }
template <>
constexpr GDExtensionDeviceExitAbi production_dto_abi<GDExtensionProductionOutputHistoryStateV2>() { return { 2, sizeof(GDExtensionProductionOutputHistoryStateV2), 0 }; }
template <class T>
bool production_dto_valid(const T &value) {
	const auto expected = production_dto_abi<T>();
	return value.abi.abi_version == expected.abi_version && value.abi.struct_size == expected.struct_size && value.abi.reserved == 0;
}

// Synchronous input views only. Rows copy into the original accepted arena.
// V1 is explicitly normalized; no ordinal is stored in a V1 canonical field.
struct ProductionActionRows {
	const GDExtensionProductionActionV1 *legacy = nullptr;
	const GDExtensionProductionActionV2 *ordinal = nullptr;
	bool aligned() const {
		if (legacy) {
			return uintptr_t(legacy) % alignof(GDExtensionProductionActionV1) == 0;
		}
		return ordinal && uintptr_t(ordinal) % alignof(GDExtensionProductionActionV2) == 0;
	}
	GDExtensionProductionActionV2 operator[](uint32_t index) const {
		if (ordinal) {
			return ordinal[index];
		}
		const auto &v = legacy[index];
		GDExtensionProductionActionV2 r = {};
		r.action_ordinal = index;
		r.canonical_suboperation = v.canonical_suboperation;
		r.action_kind = v.action_kind;
		r.physical_unit = v.physical_unit;
		r.first_use = v.first_use;
		r.use_count = v.use_count;
		r.list_boundary = v.list_boundary;
		r.readback_slot = v.readback_slot;
		return r;
	}
};
// Core normalization only, never a wire layout. Keep the legacy reserved
// field until validation so adapting an old row cannot erase a bad input.
struct ProductionReadbackDeclaration {
	uint32_t resource_slot = 0, reserved = 0;
	uint64_t producer_action_ordinal = 0, copy_action_ordinal = 0;
	uint64_t byte_offset = 0, byte_size = 0;
	uint32_t source_kind = GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1;
	GDExtensionProductionReadbackIncomingSourceV1 incoming = {};
};
struct ProductionReadbackRows {
	const GDExtensionProductionReadbackV1 *legacy = nullptr;
	const GDExtensionProductionReadbackV2 *ordinal = nullptr;
	uint64_t canonical_begin = 0;
	const GDExtensionProductionReadbackSourceV1 *sources = nullptr;
	bool aligned() const {
		if (sources) {
			return uintptr_t(sources) % alignof(GDExtensionProductionReadbackSourceV1) == 0;
		}
		if (legacy) {
			return uintptr_t(legacy) % alignof(GDExtensionProductionReadbackV1) == 0;
		}
		return ordinal && uintptr_t(ordinal) % alignof(GDExtensionProductionReadbackV2) == 0;
	}
	ProductionReadbackDeclaration operator[](uint32_t index) const {
		if (sources) {
			const auto &v = sources[index];
			return { v.resource_slot, 0, v.writer.producer_action_ordinal, v.copy_action_ordinal, v.byte_offset, v.byte_size, v.source_kind, v.incoming };
		}
		if (ordinal) {
			const auto &v = ordinal[index];
			return { v.resource_slot, v.reserved, v.producer_action_ordinal, v.copy_action_ordinal, v.byte_offset, v.byte_size, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, {} };
		}
		const auto &v = legacy[index];
		return { v.resource_slot, v.reserved, v.canonical_producer >= canonical_begin ? v.canonical_producer - canonical_begin : UINT64_MAX, v.canonical_copy >= canonical_begin ? v.canonical_copy - canonical_begin : UINT64_MAX, v.byte_offset, v.byte_size, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, {} };
	}
};

class ProductionRequest;

// Core-only custody of one original accepted arena cell, never a record grant.
// Explicit last-use return is mandatory; destruction cannot invent it.
class ProductionOutputPin {
	friend class ::RendererRD::ExternalOutputOwner;
	friend class Registry;
	friend class ::RenderingDevice;
	Registry *registry = nullptr;
	ProductionRequest *request = nullptr;
	uint64_t request_serial = 0, serial = 0;
	uint32_t cell_index = UINT32_MAX;

public:
	ProductionOutputPin() = default;
	ProductionOutputPin(const ProductionOutputPin &) = delete;
	ProductionOutputPin &operator=(const ProductionOutputPin &) = delete;
	ProductionOutputPin(ProductionOutputPin &&) noexcept;
	ProductionOutputPin &operator=(ProductionOutputPin &&) noexcept;
	~ProductionOutputPin();
	bool is_empty() const { return registry == nullptr; }
};

// A synchronous native-owner continuation of an originally reserved output
// action. It is neither an extension physical context nor a new reservation.
class ProductionOutputPreparationScope final {
	friend class Registry;
	friend class ::RenderingDevice;
	friend class ::RendererRD::ParticlesStorage;
	friend class ::RendererRD::ExternalPipelinePreparation;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
#endif
	ProductionOutputPreparationScope() = default;
	Registry *registry = nullptr;
	ProductionRequest *request = nullptr;
	ProductionOutputPin *pin = nullptr;
	RenderingDevice *device = nullptr;
	ProductionOutputPreparationScope *previous = nullptr;
	RID *original_root = nullptr;
	::RendererRD::ExternalPipelinePreparation *row = nullptr;
	ProductionResourceSlot *original_source = nullptr;
	const void *original_geometry = nullptr;
	RID original_particles, original_material;
	uint64_t original_generation = 0, original_lease = 0, original_material_generation = 0, source_generation = 0;
	uint32_t original_slot = UINT32_MAX, source_use = UINT32_MAX, first_source_use = UINT32_MAX, source_use_count = 0;
	uint64_t request_serial = 0, root_serial = 0, group = 0, thread = 0;
	uint32_t action_ordinal = UINT32_MAX, surface_ordinal = UINT32_MAX, preparation_index = UINT32_MAX;
	const ::RendererRD::ExternalPacketViewLease *selected_view = nullptr;

public:
	ProductionOutputPreparationScope(const ProductionOutputPreparationScope &) = delete;
	ProductionOutputPreparationScope &operator=(const ProductionOutputPreparationScope &) = delete;
	ProductionOutputPreparationScope(ProductionOutputPreparationScope &&) = delete;
	ProductionOutputPreparationScope &operator=(ProductionOutputPreparationScope &&) = delete;
	~ProductionOutputPreparationScope();
	// Synchronous observation of one exact accepted READ-use row. The original
	// scope retains arena/source custody; the copied values issue no lifetime,
	// semantic role, append permission or producer selected-version authority.
	// Valid only while this original scope remains current. Failure preserves out.
	struct ReadonlySource {
		RID buffer;
		uint64_t request_serial = 0, output_generation = 0, allocation_generation = 0;
		uint64_t byte_offset = 0, byte_size = 0;
		uint32_t action_ordinal = UINT32_MAX, use_index = UINT32_MAX, resource_slot = UINT32_MAX, source_count = 0;
	};
	Error get_readonly_source(uint32_t p_source_index, ReadonlySource &r_source) const;
};

// One actual byte-bounded CPU metadata resource in an original accepted row.
// This lease proves physical allocation/custody only, never logical U08/source
// admission. Explicit return after all readers and the linked root are gone.
class ProductionOutputMetadataLease {
	friend class Registry;
	Registry *registry = nullptr;
	ProductionRequest *request = nullptr;
	uint64_t request_serial = 0;
	uint32_t resource_slot = UINT32_MAX;
	void *data = nullptr;
	uint64_t bytes = 0;

public:
	ProductionOutputMetadataLease() = default;
	ProductionOutputMetadataLease(const ProductionOutputMetadataLease &) = delete;
	ProductionOutputMetadataLease &operator=(const ProductionOutputMetadataLease &) = delete;
	ProductionOutputMetadataLease(ProductionOutputMetadataLease &&) noexcept;
	ProductionOutputMetadataLease &operator=(ProductionOutputMetadataLease &&) noexcept;
	~ProductionOutputMetadataLease();
	bool is_empty() const { return registry == nullptr; }
	void *get_data() const { return data; }
	uint64_t get_size() const { return bytes; }
};

#ifdef TESTS_ENABLED
// Component fixture only, on the genuine original OBSERVE dispatch. The
// original arena capture pin remains held across this callback boundary.
extern thread_local Error (*test_production_before_maintenance)(ProductionRequest &, RenderingDevice &);
#endif

class ProductionAttachment;
class ProductionRequest;

// These cells are constructed only in the accepted attachment's finite arena.
// A copied DTO or address never enrolls a cell into the Registry.
class ProductionResourceSlot {
	friend class ProductionOutputPreparationScope;
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend class ::TestProductionResourceUseAccess;
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	friend class ProductionRequest;
	ProductionResourceSlot() = default;
	ProductionRequest *request = nullptr;
	uint64_t request_serial = 0;
	GDExtensionProductionResourceV1 declaration = {};
	ProductionResourceUse uses[2];
	uint32_t use_count = 0;
	RID primary, secondary;
	uint64_t backing_serial = 0;
	uint32_t imported_unit_slot = UINT32_MAX;
	uint32_t shader_set = UINT32_MAX;
	uint32_t first_binding = UINT32_MAX, binding_count = 0;
	bool imported = false;
	// Nonowning alias of an actual output DISPOSAL_MEMBER. The native group
	// remains the sole free owner; owned_physical is never populated for this.
	NativeDisposalWitness output_witness;
	uint64_t output_action_ordinal = UINT64_MAX;
	uint64_t output_group_serial = 0, output_root_serial = 0;
	uint32_t output_member_ordinal = UINT32_MAX;
	// Readonly predecessor custody; distinct from writable current-slot aliases.
	// The original paid ViewCell pins both its arena and slot incarnation.
	RendererRD::ExternalPacketViewLease output_predecessor_view;
	bool output_predecessor_no_records = false; // Explicit cap0 old-status retention anchor, never a records source.
	NativeDisposalWitness output_predecessor_witness;
	GDExtensionOutputTokenV1 output_predecessor_token = {};
	GDExtensionOutputConsumedProofRefV2 output_predecessor_ref = {};
	GDExtensionOutputAcquireInputV1 output_predecessor_input = {};
	GDExtensionProductionHostTransformV1 output_predecessor_host = {};
	uint32_t output_predecessor_use = UINT32_MAX;
	uint64_t output_predecessor_action = UINT64_MAX;
	// K-owned GuardStatus32, never one of the producer-writable output roles.
	bool output_guard_readonly = false;
	// One prior K status association. All declared physical consumers are checked;
	// no singleton gather, semantic shader tag, or independent view pin.
	bool output_guard_history = false;
	uint32_t output_guard_anchor = UINT32_MAX;
	uint64_t output_guard_consumer = UINT64_MAX, output_guard_finalizer = UINT64_MAX;
	bool output_alias_prepared = false;
	bool output_set_deferred = false;
	uint32_t transfer_worker = UINT32_MAX;
	uint64_t transfer_operation = 0, transfer_action_ordinal = 0;
	bool transfer_pending = false, transfer_staged = false;
	QualificationPhysicalReturn owned_physical;
	// Original request storage; never a borrowed stack address or Output cell.
	RenderingDevice::NativeImmutableHold material_pipeline_shader_hold;
	RID material_pipeline_reservation;
	GDExtensionStorageCreateRequestV1 create_origin = {};
	GDExtensionStorageBackingV1 owned_backing = {};
	GDExtensionStorageRetireRequestV1 retire_origin = {};
	GDExtensionStorageRetireReportV1 retire_report = {};
	uint64_t owned_serial = 0, retire_serial = 0;
	bool creating = false, retiring = false, returned = false;
	// Independent CPU allocation, not a GPU storage backing or return fact.
	ProductionOutputMetadataLease *metadata_consumer = nullptr;
	void *metadata_data = nullptr;
	uint64_t metadata_root_serial = 0;
	bool metadata_created = false, metadata_returned = false;
	uint32_t metadata_kind = 0; // 0: original output allocation; 2: early CPU native selection slab.
	// Set only by successful real GENERATION owner installation. Survives root
	// unpin until the actual placement allocation returns at particles_free.
	bool metadata_generation_bound = false;
	// Exact original terminal consumed by the owner; survives control reuse.
	uint64_t metadata_consumed_control_serial = 0;
	uint64_t metadata_consumed_particles = 0;
	uint64_t metadata_consumed_generation = 0;
	Error handoff_error = OK;
};
// Stack-only original-source borrow for the native guard consumer. Valid only
// while original Registry/RD context is held; it issues no pin or identity.
struct ProductionOutputHistoryBorrow {
	const RendererRD::ExternalPacketViewLease *previous_view = nullptr;
	const GDExtensionProductionHostTransformV1 *current_host = nullptr, *previous_host = nullptr;
	const NativeDisposalWitness *previous_records_witness = nullptr;
	const GDExtensionOutputConsumedProofRefV2 *consumed_proof = nullptr;
	const GDExtensionOutputTokenV1 *consumed_tuple = nullptr;
	RID previous_records, previous_guard_status;
	const NativeDisposalWitness *previous_guard_witness = nullptr;
	uint64_t previous_guard_backing = 0, previous_guard_sequence = 0;
	uint32_t previous_guard_bytes = 0, previous_guard_profile = 0;
	bool previous_no_records = false;
};
// Finite exact native return custody, embedded in original quoted graph/action
// rows. A partial action with progress owns its first new graph row; a terminal
// no-graph action owns its original action row. No renderer allocation or ACK.
struct ProductionNativeBatchCell {
	GDExtensionProductionPreparedActionResultV2 result{};
	GDExtensionProductionSegmentV2 segment{};
	uint64_t next = 0, control = 0;
	bool live = false, read = false, has_segment = false;
};
struct ProductionOutputGuardSource {
	RID buffer;
	const NativeDisposalWitness *witness = nullptr;
	const ProductionRequest *request = nullptr;
	uint64_t root = 0, group = 0, backing = 0, sequence = 0;
	uint32_t member = UINT32_MAX;
};
class ProductionActionCell {
	friend class ProductionRequest;
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionActionCell() = default;
	ProductionNativeBatchCell native_batch;
	ProductionRequest *request = nullptr;
	GDExtensionProductionActionV2 declaration = {};
	GDExtensionProductionDispatchV2 dispatch = {};
	GDExtensionProductionCopyV2 copy = {};
	GDExtensionProductionUploadV2 upload = {};
	uint64_t payload_offset = 0, payload_bytes = 0;
	uint32_t graph_schedule_bound = 0, graph_suffix_bound = 0;
	uint32_t recorded_bytes = 0;
	uint32_t readback_slot = UINT32_MAX;
	// Original checked append fact, stored in its original action row. No once table.
	CheckedRecordAppend original_append;
	ScenePhaseAttempt scene_attempt;
	GDExtensionSceneOperationKeyV1 scene_key = {};
	bool scene_key_bound = false;
	uint64_t original_append_fact_serial = 0;
	bool prepared = false;
	// Same original arena quote pays every result variant; never a DSO payload write.
	GDExtensionProductionPreparedActionResultV2 native_prepared_result = {};
	bool native_prepared_result_valid = false;
	bool native_prepared_record_consumed = false;
	bool native_prepared_result_read = false;
	enum class OutputState : uint8_t { UNBOUND,
		RESERVING,
		BOUND,
		RETIRING,
		RETURNED,
		UNKNOWN };
	struct OutputRole {
		uint32_t role = 0, resource_slot = UINT32_MAX, use_index = UINT32_MAX;
		uint64_t generation = 0, bytes = 0;
	};
	OutputState output_state = OutputState::UNBOUND;
	GDExtensionOutputAcquireInputV1 output_input = {};
	GDExtensionOutputConsumedProofRefV2 consumed_read_ref = {};
	bool consumed_proof_returned = false;
	bool consumed_proof_requested = false;
	// Original existing camera source only; never creates a sample/identity.
	// Both fields are paid by sizeof(ProductionActionCell) in the arena quote.
	Ref<RendererViewport::Viewport::CameraCapture> preinput_camera_source;
	GDExtensionProductionCameraSampleV1 preinput_camera_sample = {};
	bool preinput_camera_returned = false;
	ProductionCameraReadySubscriber camera_ready_subscriber;
	RID camera_ready_viewport;
	// Finite original paid action association. No separate view registry.
	RID host_view_camera;
	RendererViewport::Viewport::ViewPresentationKey host_view_key;
	bool host_view_prepared = false, host_view_received = false;
	GDExtensionProductionHostTransformV1 preinput_host_transform = {};
	bool preinput_host_prepared = false, preinput_host_returned = false;
	// Paid by the existing sizeof(ProductionActionCell) metadata quote.
	// Original E09 retains the actual scene instance through native release.
	GDExtensionProductionOutputSceneInputV1 output_scene = {};
	bool output_scene_prepared = false;
	GDExtensionProductionOutputPlanV1 output_plan = {};
	bool output_plan_prepared = false;
	uint64_t output_acquire_action_ordinal = UINT64_MAX;
	CheckedRecordAppend output_guard_append;
	GDExtensionOutputTokenV1 output_record_token = {}, output_packet_token = {};
	bool output_guard_entered = false, output_guard_complete = false;
	bool staged_predecessor_intent = false;
	uint32_t staged_history_guard = UINT32_MAX;
	uint32_t output_guard_upload_bytes = 0, output_guard_dispatch_cursor = 0, output_guard_graph_begin = 0;
	uint64_t output_guard_record_serial = 0;
	OutputRole output_roles[3];
	ProductionResourceSlot *output_source = nullptr;
	uint32_t output_source_use = UINT32_MAX;
	uint32_t output_first_source_use = UINT32_MAX, output_source_use_count = 0;
	uint64_t output_source_generation = 0;
	RID output_particles;
	uint64_t output_generation = 0, output_lease = 0, output_root = 0;
	uint32_t output_slot = UINT32_MAX;
	uint32_t output_read_capacity = 0, output_read_graph_capacity = 0, output_read_cursor = 0;
	uint64_t output_preparation_borrower_headroom = 0; // Paid simultaneous L, never an issued pin or a read grant.
	bool output_source_prepared = false;
	uint32_t camera_payload_kind = 0; // Explicit staged upload codec: 1 view24, 2 sort18, 3 control3.
	bool output_camera_pending = false; // Only explicit staged preparation may leave camera fields unresolved.
};
struct ProductionOutputDemandSnapshot {
	uint32_t capacity = 0, surfaces = 0, view_capacity = 0, read_capacity = 0, surface_capacity = 0;
};
class ProductionUseCell {
	friend class ProductionOutputPreparationScope;
	friend class ::RendererRD::ExternalOutputOwner;
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend class ::TestNativeDisposalAccess;
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionUseCell() = default;
	ProductionRequest *request = nullptr;
	GDExtensionProductionUseV1 declaration = {};
	GDExtensionProductionSetBindingV1 binding = {};
	ProductionResourceUse dependency;
	// Spare resource-use cells may retain one exact native consumer after
	// immutable preparation freezes. Declaration and binding rows stay intact.
	enum class OutputRole : uint8_t { UNUSED,
		NATIVE_CONSUMER,
		DISPOSAL_GROUP,
		DISPOSAL_MEMBER,
		DISPOSAL_STAGED,
		DISPOSAL_RESERVED };
	OutputRole output_role = OutputRole::UNUSED;
	ProductionOutputPin *output_consumer = nullptr;
	uint64_t output_pin_serial = 0;
	// Exact native owner locator on the ORIGINAL root pin row. Borrow rows
	// remain unlinked. No wrapper/owner address is retained here.
	ProductionOutputOwnerKind output_owner_kind = ProductionOutputOwnerKind::NONE;
	RID output_particles;
	uint64_t output_generation = 0, output_lease = 0;
	uint32_t output_slot = UINT32_MAX;
	// Original generation RT publishes once under Registry after real selection
	// completes. Withdrawal only clears live; no owner field is read by quote.
	ProductionOutputDemandSnapshot output_demand;
	bool output_demand_ready = false;
	std::atomic<bool> output_demand_live{ false };
	uint32_t output_prev = UINT32_MAX, output_next = UINT32_MAX;
	ProductionDisposalUse disposal;
	uint64_t disposal_group_serial = 0, disposal_group_pin_serial = 0;
	uint32_t disposal_group_count = 0, disposal_ordinal = UINT32_MAX;
	uint32_t disposal_group_native_hold_count = 0;
	bool disposal_group_release = false, disposal_group_dropped = false, disposal_group_native_hold = false;
};
class ProductionReadbackCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionReadbackCell() = default;
	ProductionRequest *request = nullptr;
	ProductionReadbackDeclaration declaration = {};
	ReadbackSourceState source;
	BufferReadbackSourceCapture capture;
	uint64_t bytes_offset = 0;
};
class ProductionGraphCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionGraphCell() = default;
	ProductionNativeBatchCell native_batch;
	ProductionRequest *request = nullptr;
	RDG::RecordHandle handle;
	BufferUploadSourceSegment captured;
	GDExtensionProductionFactV2 fact = {};
	uint32_t device_frame = UINT32_MAX;
	uint64_t device_epoch = 0;
	bool pending = false;
	bool output_read = false; // Actual native read graph; no shader recipe credit.
	struct OutputReadTransfer {
		uint32_t worker = UINT32_MAX;
		uint64_t operation = 0;
		bool pending = false;
	};
	OutputReadTransfer output_read_transfers[32];
	uint32_t output_read_transfer_count = 0;
};
class ProductionNativeCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionNativeCell() { fact.membership_parent = UINT32_MAX; }
	ProductionRequest *request = nullptr;
	NativeSourceMember member;
	// Only original source capture populates these rows; no public numeric writer.
	// Aliases retain each membership edge but only the first cell owns a ref.
	uint32_t owner_index = UINT32_MAX;
	GDExtensionProductionFactV2 fact = {};
	NativeSourceObservation observation;
	Error release_error = OK;
};
class ProductionReturnCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionReturnCell() = default;
	ProductionRequest *request = nullptr;
	uint64_t serial = 0;
	bool pending = false;
};
class ProductionCaptureCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionCaptureCell() = default;
	ProductionRequest *request = nullptr;
	uint64_t graph_instance = 0, frame_epoch = 0;
	uint32_t command_slot = UINT32_MAX, first_native = UINT32_MAX;
};
// Core-owned maintenance only; this is not an extension ABI or execution grant.
enum class ProductionControlKind : uint32_t { OBSERVE,
	CANCEL,
	RELEASE,
	STORAGE_CREATE,
	SET_CREATE,
	RASTER_CREATE,
	RECORD };
enum class ProductionControlState : uint32_t { OWNER_CONSUMED,
	PENDING,
	QUEUED,
	EXECUTING,
	RETURNED };
struct ProductionControlInput {
	uint64_t request_serial;
	uint64_t record_generation;
	ProductionControlKind kind;
	GDExtensionProductionServiceBudgetV1 budget;
	uint32_t resource_slot;
	bool release_owner_only = false; // Immutable local owner continuation, never an RT ACK.
	bool predecessor_bind = false, predecessor_staged = false;
	GDExtensionProductionOutputPredecessorV2 predecessor = {};
	GDExtensionProductionOutputPrepareV1 predecessor_prepare = {};
	// Charged with the original fixed control cell; no second queue or owner.
	uint32_t consumed_operation = 0; // 1 borrow, 2 return, both original OBSERVE.
	uint64_t consumed_action_ordinal = UINT64_MAX;
	GDExtensionOutputConsumedProofRefV2 consumed_expected = {};
	bool output_cancel = false; // Same original SLOT cleanup RECORD; never append authority.
};
struct ProductionServiceUsage {
	uint32_t controls, receipts, passes;
};
enum class ProductionRecordPause : uint32_t { NONE,
	SCOPE_RETURN,
	APPEND_LIMIT,
	UPLOAD_PREFIX };
// One immutable native-generation closure per dispatched original owner turn.
// This is charged with ProductionRequest, not a public Poll DTO or E05/U09 ACK.
// Source counters describe only this exact root request, never other requests.
struct ProductionOutputGenerationResult {
	uint64_t registry_instance, request_serial, control_serial, root_serial;
	uint64_t particles, generation;
	GDExtensionProductionReportV2 root_source;
	uint32_t unused_slots, returned_slots;
	bool native_released;
};
struct ProductionControlOutput {
	uint64_t request_serial;
	uint64_t control_serial;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionReportV2 report;
	GDExtensionProductionSegmentV2 segment;
	GDExtensionProductionAppendV2 append;
	ProductionRecordPause record_pause;
	ProductionServiceUsage service_usage;
	ProductionOutputGenerationResult output_generation;
	GDExtensionOutputConsumedProofV2 consumed_proof;
	bool consumed_result = false;
	// Owner reference return is distinct from the immutable RT result.
	GDExtensionDeviceSessionResultV1 owner_return;
	ProductionServiceUsage owner_usage;
};
// Kept in the original fixed control cell, including after owner consumption.
struct ProductionRecordHistory {
	GDExtensionDeviceSessionResultV1 last_schedule_result = {};
	bool schedule_attempted = false;
	bool queue_possible = false;
	bool admitted = false;
	bool schedule_result_valid = false;
	bool control_effect_entered = false;
	bool control_append_entered = false;
};
struct ProductionControlCell {
	ProductionRecordHistory record_history;
	ProductionControlInput input;
	ProductionControlOutput output;
	uint64_t serial;
	ProductionControlState state;
	bool scene_waiting = false;
};
class ProductionRequest {
	friend class ProductionOutputPreparationScope;
	friend class Registry;
	friend class ::RendererRD::ExternalOutputOwner;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend class ::TestNativeDisposalAccess;
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionRequest() = default;
	ProductionAttachment *attachment = nullptr;
	uint64_t serial = 0;
	GDExtensionProductionOperationV1 operation = {};
	uint32_t contract_version = 1;
	bool native_prepared_executor = false;
	bool native_batch_enabled = false;
	uint64_t native_batch_control = 0, native_batch_head = 0, native_batch_tail = 0, native_batch_read_cursor = 0;
	bool native_batch_read_started = false, native_batch_continue = false;
	uint64_t native_batch_pending_ack = 0, native_batch_last_ack_revision = 0;
	bool native_static_sealed = false; // Structural validation and schedule pricing completed before draw.
	uint64_t native_staged_anchor = UINT64_MAX;
	// Exactly one WithoutPrevious fallback, stored in the paid spare action
	// cells [action_count,2*action_count). These are preparation, never actions.
	bool native_history_fallback = false, native_history_selected = false;
	bool native_history_used_fallback = false;
	GDExtensionProductionOutputPlanV1 native_history_fallback_plan{};
	bool native_history_static_prepared = false, native_history_has_tuple = false;
	GDExtensionProductionOutputHistoryStaticV2 native_history_static{};
	GDExtensionProductionOutputHistoryStateV2 native_history_state{};
	GDExtensionOutputCameraV1 native_history_previous_camera{};
	uint64_t native_history_previous_present = 0, native_history_previous_epoch = 0;
	uint64_t native_camera_uploads[3] = { UINT64_MAX, UINT64_MAX, UINT64_MAX };
	uint32_t native_camera_needs_inverse = 0;
	GDExtensionProductionServiceBudgetV1 native_stage_budget = {};
	uint64_t native_stage_control = 0;
	uint32_t native_stage_controls_used = 0, native_stage_passes_used = 0, native_stage_receipts_used = 0;
	uint32_t native_prepared_unread = 0;
	GDExtensionProductionCapacityV1 capacity = {};
	ProductionActionCell *actions = nullptr;
	ProductionResourceSlot *resources = nullptr;
	ProductionUseCell *uses = nullptr;
	ProductionReadbackCell *readbacks = nullptr;
	ProductionGraphCell *graphs = nullptr;
	ProductionNativeCell *natives = nullptr;
	ProductionReturnCell *returns = nullptr;
	ProductionCaptureCell *commands = nullptr, *frames = nullptr;
	uint32_t action_count = 0, resource_count = 0, use_count = 0, readback_count = 0;
	uint32_t binding_used = 0;
	uint64_t payload_offset = 0, readback_offset = 0, payload_used = 0;
	uint32_t command_count = 0, frame_count = 0;
	uint64_t revision = 1;
	uint32_t graph_count = 0, native_count = 0;
	uint32_t observe_cursor = 0, release_cursor = 0;
	// Original request's RT readiness fact, not completion of owner references.
	struct ReleaseReady {
		uint64_t request_serial = 0, control_serial = 0, revision = 0;
		uint64_t record_generation = 0, device_epoch = 0, next_pin_serial = 0;
		uint32_t native_count = 0, graph_count = 0;
	} release_ready;
	bool source_device_lost = false, release_started = false;
	uint32_t capture_inflight = 0;
	uint32_t continuation_inflight = 0;
	// One fixed mailbox per request, included by sizeof(ProductionRequest) in
	// the checked arena Quote. Never aliases retained payload or return cells.
	ProductionControlCell control = {};
	// Only an original inactive-to-active maintenance call owns this cursor.
	uint64_t maintenance_serial = 0;
	uint32_t retire_cursor = 0;
	uint64_t next_control_serial = 1;
	uint64_t next_output_pin_serial = 1;
	uint32_t output_owner_head = UINT32_MAX, output_owner_cursor = UINT32_MAX;
	uint64_t next_disposal_group_serial = 1;
	bool frozen = false, cancelled = false, effects_pending = false;
	bool compute_list_open = false;
	bool draw_list_open = false;
	bool raster_started = false;
	// Monotonic across every control; empty later RECORDs cannot erase effects.
	bool native_effect_entered = false;
	// Resource allocation/retirement alone does not enter business append.
	bool append_effect_entered = false;
	// Independent typed raster declaration retained with the accepted request
	// arena. It does not alias qualification or dispatch ownership.
	GDExtensionProductionRasterActionV1 raster_action = {};
	GDExtensionProductionRasterPrepareV1 raster_prepare_origin = {};
	uint64_t raster_prepare_control = 0;
	uint32_t raster_prepare_phase = 0;
	bool raster_shader_retire_ready = false;
	bool raster_reserved = false;
	bool guarded_schedule_required() const;
	uint32_t next_action = 0;
	uint32_t native_workers_bound = 0;
	GDExtensionProductionSegmentV2 segment = {};
	uint64_t acknowledged_segment = 0;
	uint32_t segment_graph_begin = 0, segment_action_begin = 0;
	// No numeric field or DTO populates original scope/window/K03 authority.
};
class ProductionAttachment {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionAttachment() = default;
	uint64_t serial = 0, epoch = 0, producer_serial = 0, library_generation = 0;
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
	// Intrusive entity edge owned by this same accepted arena. RD destructive
	// finalize and Object destruction consult the reverse link before teardown.
	RenderingDevice *held_device = nullptr;
	ProductionAttachment *entity_previous = nullptr, *entity_next = nullptr;
	uint64_t terminal_action = 0;
	GDExtensionProductionMetadataQuoteV1 quote = {};
	void *arena = nullptr;
	ProductionRequest *requests = nullptr;
	ProductionActionCell *actions = nullptr;
	ProductionResourceSlot *resources = nullptr;
	ProductionUseCell *uses = nullptr;
	ProductionReadbackCell *readbacks = nullptr;
	ProductionGraphCell *graphs = nullptr;
	ProductionNativeCell *natives = nullptr;
	ProductionReturnCell *returns = nullptr;
	ProductionCaptureCell *commands = nullptr, *frames = nullptr;
	uint8_t *payload = nullptr, *readback_bytes = nullptr;
	uint32_t children = 0;
	ProductionRecordReceiver record_receiver = nullptr;
	ProductionMetadataReceiver metadata_receiver = nullptr;
	RecordRef metadata_reference;
	uint64_t metadata_frame = 0;
	bool metadata_active = false, metadata_pending = false;
	void *record_receiver_userdata = nullptr;
	// Paid exclusive payload prefix, initialized before any request reserve.
	SceneConnectionRow *scene_connections = nullptr;
	uint64_t scene_prefix_bytes = 0;
	uint32_t scene_connection_capacity = 0;
	bool scene_owner_initialized = false, request_reserved_once = false;
	NormalRelayToken scene_relay;
	GDExtensionScenePhaseSinkConfigV1 scene_config = {};
	uint64_t scene_registration = 0, scene_occurrence = 0;
	uint32_t scene_active = 0;
	bool scene_closed = false, scene_relay_closed = false;
	NormalRelayToken camera_ready_relay;
	// Exact renderer subscription retained by the attachment. The source Ref is
	// delivered only through the existing post-draw observer drain.
	Ref<RendererViewport::Viewport::CameraSourceObserver> camera_ready_observer;
	RID camera_ready_viewport;
	uint32_t camera_ready_active = 0;
	bool camera_ready_closed = false, camera_ready_relay_closed = false, camera_ready_close_active = false;
	bool opened = false;
	bool closing = false;
	// Open must acquire the original device holder and operation-window receiver.
	// Metadata reservations do not satisfy those two obligations.
};
} // namespace GDExtensionDeviceExit
