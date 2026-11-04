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

class TestProductionResourceUseAccess;
namespace TestProductionNativeObserve {
struct Access;
}
namespace TestProductionRequest {
struct Access;
}
namespace GDExtensionDeviceExit {
class ProductionAttachment;
class ProductionRequest;

// These cells are constructed only in the accepted attachment's finite arena.
// A copied DTO or address never enrolls a cell into the Registry.
class ProductionResourceSlot {
	friend class Registry;
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
};
class ProductionActionCell {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionActionCell() = default;
	ProductionRequest *request = nullptr;
	GDExtensionProductionActionV1 declaration = {};
	GDExtensionProductionDispatchV1 dispatch = {};
	GDExtensionProductionCopyV1 copy = {};
	GDExtensionProductionUploadV1 upload = {};
	uint64_t payload_offset = 0, payload_bytes = 0;
	uint32_t readback_slot = UINT32_MAX;
	bool prepared = false;
};
class ProductionUseCell {
	friend class Registry;
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
};
class ProductionReadbackCell {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionReadbackCell() = default;
	ProductionRequest *request = nullptr;
	GDExtensionProductionReadbackV1 declaration = {};
	ReadbackSourceState source;
	uint64_t bytes_offset = 0;
};
class ProductionGraphCell {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
#endif
	ProductionGraphCell() = default;
	ProductionRequest *request = nullptr;
	RDG::RecordHandle handle;
	GDExtensionProductionFactV1 fact = {};
	bool pending = false;
};
class ProductionNativeCell {
	friend class Registry;
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
	GDExtensionProductionFactV1 fact = {};
	NativeSourceObservation observation;
	Error release_error = OK;
};
class ProductionReturnCell {
	friend class Registry;
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
	RELEASE };
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
};
struct ProductionServiceUsage {
	uint32_t controls, receipts, passes;
};
struct ProductionControlOutput {
	uint64_t request_serial;
	uint64_t control_serial;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionReportV1 report;
	ProductionServiceUsage service_usage;
	// Owner reference return is distinct from the immutable RT result.
	GDExtensionDeviceSessionResultV1 owner_return;
	ProductionServiceUsage owner_usage;
};
struct ProductionControlCell {
	ProductionControlInput input;
	ProductionControlOutput output;
	uint64_t serial;
	ProductionControlState state;
};
class ProductionRequest {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend struct ::TestProductionNativeObserve::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionRequest() = default;
	ProductionAttachment *attachment = nullptr;
	uint64_t serial = 0;
	GDExtensionProductionOperationV1 operation = {};
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
	// One fixed mailbox per request, included by sizeof(ProductionRequest) in
	// the checked arena Quote. Never aliases retained payload or return cells.
	ProductionControlCell control = {};
	uint64_t next_control_serial = 1;
	bool frozen = false, cancelled = false, effects_pending = false;
	// No numeric field or DTO populates original scope/window/K03 authority.
};
class ProductionAttachment {
	friend class Registry;
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
	bool closing = false;
	// Open must acquire the original device holder and operation-window receiver.
	// Metadata reservations do not satisfy those two obligations.
};
} // namespace GDExtensionDeviceExit
