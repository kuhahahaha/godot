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
	ProductionReadbackCell() = default;
	ProductionRequest *request = nullptr;
	GDExtensionProductionReadbackV1 declaration = {};
	ReadbackSourceState source;
	uint64_t bytes_offset = 0;
};
class ProductionGraphCell {
	friend class Registry;
	ProductionGraphCell() = default;
	ProductionRequest *request = nullptr;
	RDG::RecordHandle handle;
	GDExtensionProductionFactV1 fact = {};
};
class ProductionNativeCell {
	friend class Registry;
	ProductionNativeCell() = default;
	ProductionRequest *request = nullptr;
	NativeSourceMember member;
};
class ProductionReturnCell {
	friend class Registry;
	ProductionReturnCell() = default;
	ProductionRequest *request = nullptr;
	uint64_t serial = 0;
	bool pending = false;
};
class ProductionCaptureCell {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
#endif
	ProductionCaptureCell() = default;
	ProductionRequest *request = nullptr;
	uint64_t graph_instance = 0, frame_epoch = 0;
	uint32_t command_slot = UINT32_MAX, first_native = UINT32_MAX;
};
class ProductionRequest {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
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
	bool frozen = false, cancelled = false, effects_pending = false;
	// No numeric field or DTO populates original scope/window/K03 authority.
};
class ProductionAttachment {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionRequest::Access;
	friend class ::TestProductionResourceUseAccess;
#endif
	ProductionAttachment() = default;
	uint64_t serial = 0, epoch = 0, producer_serial = 0, library_generation = 0;
	uint32_t library = 0, device = 0;
	HolderRetirement *holder = nullptr;
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
