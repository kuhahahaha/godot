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

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_resource_use.h"
#include "core/extension/gdextension_record_source.h"
#include "servers/rendering/native_disposal_witness.h"

class TestProductionResourceUseAccess;
namespace TestProductionNativeObserve {
struct Access;
}
namespace TestProductionRequest {
struct Access;
}
namespace GDExtensionDeviceExit {
// Production candidate DTO headers describe the containing DTO. The installed
// common DeviceExit header and qualification DTO rules are separate contracts.
template <class T>
constexpr GDExtensionDeviceExitAbi production_dto_abi() {
	return { 1, sizeof(T), 0 };
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
struct ProductionReadbackRows {
	const GDExtensionProductionReadbackV1 *legacy = nullptr;
	const GDExtensionProductionReadbackV2 *ordinal = nullptr;
	uint64_t canonical_begin = 0;
	bool aligned() const {
		if (legacy) {
			return uintptr_t(legacy) % alignof(GDExtensionProductionReadbackV1) == 0;
		}
		return ordinal && uintptr_t(ordinal) % alignof(GDExtensionProductionReadbackV2) == 0;
	}
	GDExtensionProductionReadbackV2 operator[](uint32_t index) const {
		if (ordinal) {
			return ordinal[index];
		}
		const auto &v = legacy[index];
		return { v.resource_slot, v.reserved, v.canonical_producer >= canonical_begin ? v.canonical_producer - canonical_begin : UINT64_MAX, v.canonical_copy >= canonical_begin ? v.canonical_copy - canonical_begin : UINT64_MAX, v.byte_offset, v.byte_size };
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
	uint32_t transfer_worker = UINT32_MAX;
	uint64_t transfer_operation = 0, transfer_action_ordinal = 0;
	bool transfer_pending = false, transfer_staged = false;
	QualificationPhysicalReturn owned_physical;
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
	Error handoff_error = OK;
};
class ProductionActionCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionActionCell() = default;
	ProductionRequest *request = nullptr;
	GDExtensionProductionActionV2 declaration = {};
	GDExtensionProductionDispatchV2 dispatch = {};
	GDExtensionProductionCopyV2 copy = {};
	GDExtensionProductionUploadV2 upload = {};
	uint64_t payload_offset = 0, payload_bytes = 0;
	uint32_t recorded_bytes = 0;
	uint32_t readback_slot = UINT32_MAX;
	bool prepared = false;
};
class ProductionUseCell {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
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
	uint32_t output_prev = UINT32_MAX, output_next = UINT32_MAX;
	ProductionDisposalUse disposal;
	uint64_t disposal_group_serial = 0, disposal_group_pin_serial = 0;
	uint32_t disposal_group_count = 0, disposal_ordinal = UINT32_MAX;
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
	GDExtensionProductionReadbackV2 declaration = {};
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
	ProductionRequest *request = nullptr;
	RDG::RecordHandle handle;
	BufferUploadSourceSegment captured;
	GDExtensionProductionFactV2 fact = {};
	uint32_t device_frame = UINT32_MAX;
	uint64_t device_epoch = 0;
	bool pending = false;
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
};
class ProductionRequest {
	friend class Registry;
	friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionRequest() = default;
	ProductionAttachment *attachment = nullptr;
	uint64_t serial = 0;
	GDExtensionProductionOperationV1 operation = {};
	uint32_t contract_version = 1;
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
	bool source_device_lost = false, release_started = false;
	uint32_t capture_inflight = 0;
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
	bool opened = false;
	bool closing = false;
	// Open must acquire the original device holder and operation-window receiver.
	// Metadata reservations do not satisfy those two obligations.
};
} // namespace GDExtensionDeviceExit
