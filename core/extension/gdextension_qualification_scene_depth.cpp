/**************************************************************************/
/*  gdextension_qualification_scene_depth.cpp                             */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_qualification_internal.h"

#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error e, uint32_t reason = 0) {
	return { int32_t(e), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool separate(const void *a, size_t as, const void *b, size_t bs) {
	const auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
bool abi(const GDExtensionDeviceExitAbi &a) {
	return a.abi_version == 1 && a.struct_size == sizeof(a) && !a.reserved;
}
GDExtensionStorageSceneDepthBytesV1 bytes(const NativeDepthAllocationProjection::Bytes &value) {
	const bool known = value.knowledge == NativeDepthAllocationProjection::ByteKnowledge::Known;
	return { known ? value.bytes : 0, known ? 1U : 0U, 0 };
}
bool origin(const GDExtensionStorageSceneDepthQuoteRequestV1 &q, uint32_t slot, uint32_t part, uint64_t generation) {
	return q.allocation_slot == slot && q.part == part && q.allocation_generation == generation;
}
} //namespace

Error Registry::qualification_scene_depth_locked(QualificationBridge *bridge, GDExtensionStorageSceneDepthRefV1 facet, uint32_t &index) {
	index = UINT32_MAX;
	if (facet.registry_instance != instance || !facet.handle_serial) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < QualificationBridge::CHILD_CAPACITY; ++i) {
		const auto *child = bridge->scene_depth[i];
		if (child && child->serial == facet.handle_serial && !child->acknowledged) {
			index = i;
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_quote(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionStorageSceneDepthQuoteRequestV1 *request, GDExtensionStorageSceneDepthQuoteV1 *out) {
	if (!aligned(request) || !aligned(out) || !separate(request, sizeof(*request), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(request->abi) || !abi(request->descriptor.abi) || request->descriptor.reserved || !request->descriptor.width || !request->descriptor.height || !request->allocation_generation || request->quote_attempt != 1) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationBridge::SceneDepth *child = nullptr;
	{
		Lock lock(*this);
		Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
		if (error != OK) {
			return result(error, 16);
		}
		bridge->collect_retired_children_locked();
		for (auto *existing : bridge->scene_depth) {
			if (!existing || !origin(existing->origin, request->allocation_slot, request->part, request->allocation_generation)) {
				continue;
			}
			if (memcmp(&existing->origin, request, sizeof(*request))) {
				return result(ERR_INVALID_DATA, 16);
			}
			if (existing->acknowledged) {
				return result(ERR_DOES_NOT_EXIST, 17);
			}
			if (existing->quoting) {
				return result(ERR_BUSY, 16);
			}
			const bool accepted = qualification_accepted_call_locked(bridge, 21);
			*out = existing->quote;
			if (accepted && !bridge->rt->scene_depth_serial) {
				if (existing->accepted_rt_refs == UINT32_MAX) {
					*out = {};
					return result(ERR_PARAMETER_RANGE_ERROR, 19);
				}
				bridge->rt->scene_depth_serial = existing->serial;
				++existing->accepted_rt_refs;
			}
			return result(OK);
		}
		if (const auto *old = bridge->scene_depth_origins.find_retired([&](const auto &q) { return origin(q, request->allocation_slot, request->part, request->allocation_generation); })) {
			return result(memcmp(&old->origin, request, sizeof(*request)) ? ERR_INVALID_DATA : ERR_DOES_NOT_EXIST, 17);
		}
		// Exact retries above are observations of an admitted transaction. New
		// work must use every original gate, including metadata/cleanup denial
		// and guard enrollment. Consume the copied-call exception only here.
		error = qualification_bridge_locked(ref, true, true, bridge, rd, 21);
		if (error != OK) {
			return result(error, 16);
		}
		const bool accepted = active_qualification_bridge == bridge && bridge->rt && bridge->rt->occupied && bridge->rt->executing &&
				bridge->rt->serial == active_qualification_serial && bridge->rt->header.kind == 21 && bridge->rt->admission_consumed;
		uint32_t slot = 0;
		for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->scene_depth[slot]; ++slot) {
		}
		const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
		if (slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(4)) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		uint32_t history_index = UINT32_MAX;
		error = bridge->scene_depth_origins.prepare(history_index);
		if (error != OK) {
			return result(error, 19);
		}
		if (!bridge->cleanup_refs_reserved) {
			bridge->cleanup_refs_reserved = true;
			records[bridge->library].private_refs += 2;
		}
		child = memnew(QualificationBridge::SceneDepth);
		qualification_serials.take(child->serial);
		qualification_serials.take(child->retire_serial);
		qualification_serials.take(child->dependency_serial);
		child->cleanup_serial_reserved = true;
		++qualification_cleanup_serial_reservations;
		child->origin = *request;
		child->history_index = history_index;
		bridge->scene_depth_origins.begin(history_index, *request, child->serial);
		bridge->scene_depth[slot] = child;
		++bridge->children;
		++records[bridge->library].private_refs;
		if (accepted) {
			bridge->rt->scene_depth_serial = child->serial;
			++child->accepted_rt_refs;
		}
	}
	// Only the original quote runs here. Registry never allocates a native pair.
	Error error = rd->_native_disposal_depth_allocation_quote(request->descriptor.width, request->descriptor.height, child->native_quote);
	NativeDepthAllocationQuote::Report observed;
	if (error == OK) {
		error = rd->_native_disposal_depth_allocation_quote_observe(child->native_quote, observed);
	}
	Lock lock(*this);
	child->quote.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	child->quote.facet = { instance, child->serial };
	child->quote.descriptor = request->descriptor;
	child->quote.quote_error = error;
	if (error == OK) {
		child->quote.texture_native_requirement = bytes(observed.texture_native_requirement);
		child->quote.texture_owner_reservation = bytes(observed.texture_owner_reservation);
		child->quote.framebuffer_owner_reservation = bytes(observed.framebuffer_owner_reservation);
		child->quote.framebuffer_driver_backing = bytes(observed.framebuffer_driver_backing);
		child->quote.opaque_driver_overhead = bytes(observed.opaque_driver_overhead);
		child->quote.alignment = observed.alignment;
		child->quote.policy = uint32_t(observed.policy);
		child->quote.allocations = observed.allocations;
		child->quote.parts = observed.parts;
		child->quote.shared_attachment_references = observed.shared_attachment_references;
	}
	child->report.abi = child->quote.abi;
	child->report.facet = child->quote.facet;
	child->report.allocation_slot = request->allocation_slot;
	child->report.part = request->part;
	child->report.allocation_generation = request->allocation_generation;
	child->report.revision = 1;
	child->report.state = error == OK ? 1 : 8;
	child->report.custody = 1;
	child->quoting = false;
	*out = child->quote;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_create(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionStorageSceneDepthCreateRequestV1 *request, GDExtensionStorageSceneDepthReportV1 *out) {
	if (!aligned(request) || !aligned(out) || !separate(request, sizeof(*request), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(request->abi) || request->reserved || !request->allocation_generation || request->create_attempt != 2) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
	if (error != OK) {
		return result(error, 16);
	}
	uint32_t index;
	error = qualification_scene_depth_locked(bridge, request->facet, index);
	if (error != OK) {
		return result(error, 17);
	}
	auto *child = bridge->scene_depth[index];
	if (!origin(child->origin, request->allocation_slot, request->part, request->allocation_generation)) {
		return result(ERR_INVALID_DATA, 16);
	}
	if (child->create_attempted) {
		if (memcmp(&child->creation, request, sizeof(*request))) {
			return result(ERR_INVALID_DATA, 16);
		}
		qualification_accepted_call_locked(bridge, 22);
		*out = child->create_report;
		return result(OK);
	}
	if (child->quoting || child->retiring || child->quote.quote_error != OK) {
		return result(ERR_BUSY, 16);
	}
	error = qualification_bridge_locked(ref, true, true, bridge, rd, 22);
	if (error != OK) {
		return result(error, 16);
	}
	if (child->report.revision == UINT64_MAX) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	// The current renderer lacks the original same-Ref owner adapter. Retain
	// this exact facet and quote; never allocate a replacement pair in Core.
	child->creation = *request;
	child->create_attempted = true;
	child->report.create_attempt = 2;
	child->report.creation_error = ERR_UNAVAILABLE;
	child->report.state = 8;
	++child->report.revision;
	child->create_report = child->report;
	*out = child->report;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_retire(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionStorageSceneDepthRetireRequestV1 *request, GDExtensionStorageSceneDepthReportV1 *out) {
	if (!aligned(request) || !aligned(out) || !separate(request, sizeof(*request), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(request->abi) || request->reserved || !request->allocation_generation || request->retire_attempt != 3) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
	if (error != OK) {
		return result(error, 16);
	}
	qualification_accepted_call_locked(bridge, 23);
	uint32_t index;
	error = qualification_scene_depth_locked(bridge, request->facet, index);
	if (error != OK) {
		return result(error, 17);
	}
	auto *child = bridge->scene_depth[index];
	if (!origin(child->origin, request->allocation_slot, request->part, request->allocation_generation)) {
		return result(ERR_INVALID_DATA, 16);
	}
	if (child->retiring) {
		if (memcmp(&child->retirement, request, sizeof(*request))) {
			return result(ERR_INVALID_DATA, 16);
		}
		*out = child->report;
		return result(OK);
	}
	if (child->quoting) {
		return result(ERR_BUSY, 16);
	}
	if (child->report.revision == UINT64_MAX) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	child->retirement = *request;
	child->retiring = true;
	child->report.retire_attempt = 3;
	child->report.state = 9; // Original quote-only/adapter-unavailable no-effect proof.
	++child->report.revision;
	*out = child->report;
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_observe(GDExtensionQualificationBridgeRefV1 ref, GDExtensionStorageSceneDepthRefV1 facet, const GDExtensionQualificationBudgetV1 *budget, GDExtensionStorageSceneDepthReportV1 *out) {
	if (!aligned(budget) || !aligned(out) || !separate(budget, sizeof(*budget), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(budget->abi) || budget->reserved || budget->max_passes < 1 || budget->max_passes > 32 || budget->max_micros < 1 || budget->max_micros > 5000) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, 17);
	}
	uint32_t index;
	error = qualification_scene_depth_locked(bridge, facet, index);
	if (error != OK) {
		return result(error, 17);
	}
	if (bridge->scene_depth[index]->quoting) {
		return result(ERR_BUSY, 16);
	}
	*out = bridge->scene_depth[index]->report; // No native access on owner thread.
	return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_release(GDExtensionQualificationBridgeRefV1 ref, GDExtensionStorageSceneDepthRefV1 facet) {
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, 17);
	}
	uint32_t index;
	error = qualification_scene_depth_locked(bridge, facet, index);
	if (error != OK) {
		return result(error, 17);
	}
	auto *child = bridge->scene_depth[index];
	if (child->quoting || !child->retiring || child->report.state != 9 || child->report.maintenance_error != OK || child->report.known_created_mask || child->accepted_rt_refs || (bridge->rt && bridge->rt->occupied && bridge->rt->scene_depth_serial == child->serial)) {
		return result(ERR_BUSY, 16);
	}
	if (child->report.revision == UINT64_MAX) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	if (child->cleanup_serial_reserved) {
		CRASH_COND(!qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		child->cleanup_serial_reserved = false;
	}
	child->acknowledged = true;
	--bridge->children;
	--records[bridge->library].private_refs;
	bridge->collect_retired_children_locked();
	qualification_trim_cleanup_ref_credits_locked(bridge);
	return result(OK);
}

namespace {
static GDExtensionDeviceSessionResultV1 quote(GDExtensionQualificationBridgeRefV1 b, const GDExtensionStorageSceneDepthQuoteRequestV1 *q, GDExtensionStorageSceneDepthQuoteV1 *out) {
	return Registry::get_singleton().qualification_scene_depth_quote(b, q, out);
}
static GDExtensionDeviceSessionResultV1 create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionStorageSceneDepthCreateRequestV1 *q, GDExtensionStorageSceneDepthReportV1 *out) {
	return Registry::get_singleton().qualification_scene_depth_create(b, q, out);
}
static GDExtensionDeviceSessionResultV1 retire(GDExtensionQualificationBridgeRefV1 b, const GDExtensionStorageSceneDepthRetireRequestV1 *q, GDExtensionStorageSceneDepthReportV1 *out) {
	return Registry::get_singleton().qualification_scene_depth_retire(b, q, out);
}
static GDExtensionDeviceSessionResultV1 observe(GDExtensionQualificationBridgeRefV1 b, GDExtensionStorageSceneDepthRefV1 facet, const GDExtensionQualificationBudgetV1 *budget, GDExtensionStorageSceneDepthReportV1 *out) {
	return Registry::get_singleton().qualification_scene_depth_observe(b, facet, budget, out);
}
static GDExtensionDeviceSessionResultV1 release(GDExtensionQualificationBridgeRefV1 b, GDExtensionStorageSceneDepthRefV1 facet) {
	return Registry::get_singleton().qualification_scene_depth_release(b, facet);
}
static_assert(std::is_same_v<decltype(&quote), GDExtensionInterfaceDeviceSessionQualificationSceneDepthQuoteV1>);
static_assert(std::is_same_v<decltype(&create), GDExtensionInterfaceDeviceSessionQualificationSceneDepthCreateV1>);
static_assert(std::is_same_v<decltype(&retire), GDExtensionInterfaceDeviceSessionQualificationSceneDepthRetireV1>);
static_assert(std::is_same_v<decltype(&observe), GDExtensionInterfaceDeviceSessionQualificationSceneDepthObserveV1>);
static_assert(std::is_same_v<decltype(&release), GDExtensionInterfaceDeviceSessionQualificationSceneDepthReleaseV1>);
} //namespace
void register_qualification_scene_depth_interfaces() {
	GDExtension::register_interface_function("device_session_qualification_scene_depth_quote_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&quote));
	GDExtension::register_interface_function("device_session_qualification_scene_depth_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&create));
	GDExtension::register_interface_function("device_session_qualification_scene_depth_retire_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&retire));
	GDExtension::register_interface_function("device_session_qualification_scene_depth_observe_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&observe));
	GDExtension::register_interface_function("device_session_qualification_scene_depth_release_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&release));
}
} //namespace GDExtensionDeviceExit
