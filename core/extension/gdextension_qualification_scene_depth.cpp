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
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"

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
// Projection only: original K observations supply every identity/state/byte.
// A created bit remains set after physical return and failed creation.
void allocation_report(const NativeDepthAllocationProjection::Report &p, GDExtensionStorageSceneDepthReportV1 &r) {
	r.depth_rid = p.depth.rid;
	r.depth_serial = p.depth.serial;
	r.framebuffer_rid = p.framebuffer.rid;
	r.framebuffer_serial = p.framebuffer.serial;
	r.depth_state = uint32_t(p.depth.state);
	r.framebuffer_state = uint32_t(p.framebuffer.state);
	r.known_created_mask |= (p.depth.rid ? 1U : 0U) | (p.framebuffer.rid ? 2U : 0U);
	r.depth_driver_backing = bytes(p.depth_driver_backing);
	r.framebuffer_driver_backing = bytes(p.framebuffer_driver_backing);
	r.framebuffer_object = bytes(p.framebuffer_object);
	r.attachment_reference = bytes(p.attachment_reference);
	r.opaque_driver_overhead = bytes(p.opaque_driver_overhead);
	r.all_created_children_returned = p.all_created_children_returned ? 1U : 0U;
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

Error Registry::scene_height_stage_target(GDExtensionQualificationBridgeRefV1 ref, GDExtensionStorageSceneDepthRefV1 facet, RID source_instance, uint32_t native_capacity, uint32_t transfer_capacity) {
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
	if (error != OK) return error;
	if (bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started || rd->production_finalization_started.load()) return ERR_UNAVAILABLE;
	uint32_t index = UINT32_MAX;
	error = qualification_scene_depth_locked(bridge, facet, index);
	if (error != OK) return error;
	auto &depth = *bridge->scene_depth[index];
	if (!depth.create_attempted || depth.native_operation || depth.retiring || depth.native_owner.is_null()) return ERR_BUSY;
	if (native_capacity < 2) return ERR_INVALID_PARAMETER;
	auto *scene = RendererSceneCull::singleton;
	auto *source = scene ? scene->instance_owner.get_or_null(source_instance) : nullptr;
	if (!source || source->base_type != RSE::INSTANCE_PARTICLES_COLLISION || !source->base_data || !source->scenario || !source->visible) return ERR_INVALID_PARAMETER;
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage->particles_collision_is_heightfield(source->base)) return ERR_INVALID_PARAMETER;
	const auto *collision = static_cast<const RendererSceneCull::InstanceParticlesCollisionData *>(source->base_data);
	auto *pci = storage->particles_collision_instance_owner.get_or_null(collision->instance);
	if (!pci || pci->pending_frame.is_valid()) return ERR_BUSY;
	if (depth.native_scene_source.is_valid() && depth.native_scene_source != source_instance) return ERR_INVALID_DATA;
	if (pci->staged_height_resources.is_valid()) {
		return pci->staged_height_resources == depth.native_owner &&
				pci->staged_height_bridge.registry_instance == ref.registry_instance && pci->staged_height_bridge.handle_serial == ref.handle_serial &&
				pci->staged_height_facet.registry_instance == facet.registry_instance && pci->staged_height_facet.handle_serial == facet.handle_serial &&
				pci->staged_height_native_capacity == native_capacity && pci->staged_height_transfer_capacity == transfer_capacity ? OK : ERR_BUSY;
	}
	if (depth.native_frame.is_valid()) {
		const auto &old = depth.native_frame;
		const auto &range = old->native_range;
		// Only the original C row and PCI may own this logical frame. Actual
		// readers must have physically completed/returned before in-place reuse.
		if (pci->current_frame != old || old->get_reference_count() != 2 || old->state != RendererRD::ParticlesStorage::HeightFrame::State::Current ||
				old->resources != depth.native_owner || old->snapshot.instance != collision->instance || old->snapshot.source_generation != pci->scene_source_key ||
				old->snapshot.width != depth.native_owner->width || old->snapshot.height != depth.native_owner->height || range.is_null() || !range->native_complete ||
				range->active_readers || range->owner_requested_return || range->observers_returned || range->uncaptured_prefix ||
				range->append_error != OK || range->mapping_error != OK || range->issue_error != OK || range->retention_error != OK ||
				range->completion_error != OK || range->return_error != OK) return ERR_BUSY;
	} else if (pci->current_frame.is_valid() && pci->current_frame->resources == depth.native_owner) {
		return ERR_INVALID_DATA;
	}
	depth.native_scene_source = source_instance;
	pci->scene_managed = true;
	pci->staged_height_bridge = ref;
	pci->staged_height_facet = facet;
	pci->staged_height_resources = depth.native_owner;
	pci->staged_height_native_capacity = native_capacity;
	pci->staged_height_transfer_capacity = transfer_capacity;
	// The original SceneCull source list performs the actual capture, after its
	// normal dirty flush. Do not change the node's WHEN_MOVED setting.
	scene->heightfield_particle_colliders_update_list.insert(source);
	return OK;
}

Error Registry::scene_height_capture_source(RID instance, RendererSceneRenderRD &renderer, const PagedArray<RenderGeometryInstance *> &geometry) {
	using Storage = RendererRD::ParticlesStorage;
	using Frame = Storage::HeightFrame;
	Lock lock(*this);
	auto *storage = Storage::get_singleton();
	auto *pci = storage->particles_collision_instance_owner.get_or_null(instance);
	if (!pci || pci->staged_height_resources.is_null() || pci->pending_frame.is_valid()) return ERR_INVALID_DATA;
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(pci->staged_height_bridge, true, false, bridge, rd);
	if (error != OK) return error;
	if (bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started || rd->production_finalization_started.load()) return ERR_UNAVAILABLE;
	uint32_t index = UINT32_MAX;
	error = qualification_scene_depth_locked(bridge, pci->staged_height_facet, index);
	if (error != OK) return error;
	auto &depth = *bridge->scene_depth[index];
	if (depth.native_operation || depth.retiring || depth.native_owner != pci->staged_height_resources) return ERR_BUSY;
	if (!pci->next_scene_content_ordinal || pci->next_scene_content_ordinal == UINT64_MAX) return ERR_PARAMETER_RANGE_ERROR;
	// A reader or sample may have appeared after staging, so admission is
	// rechecked on the actual source occurrence before any native candidate.
	if (depth.native_frame.is_valid()) {
		const auto &old = depth.native_frame;
		const auto &range = old->native_range;
		if (pci->current_frame != old || old->get_reference_count() != 2 || old->state != Frame::State::Current ||
				old->resources != depth.native_owner || old->snapshot.instance != instance || old->snapshot.source_generation != pci->scene_source_key ||
				old->snapshot.width != depth.native_owner->width || old->snapshot.height != depth.native_owner->height || range.is_null() || !range->native_complete ||
				range->active_readers || range->owner_requested_return || range->observers_returned || range->uncaptured_prefix ||
				range->append_error != OK || range->mapping_error != OK || range->issue_error != OK || range->retention_error != OK ||
				range->completion_error != OK || range->return_error != OK) return ERR_BUSY;
	}
	Ref<Frame> old = depth.native_frame;
	Ref<Frame> candidate;
	// Registry lock is held. Burn failed candidate identities; never recycle.
	uint64_t original_k_frame_serial = 0;
	if (!serials.take(original_k_frame_serial)) return ERR_PARAMETER_RANGE_ERROR;
	error = Frame::create_candidate(*storage, instance, 0, depth.native_owner, pci->staged_height_native_capacity, pci->staged_height_transfer_capacity, depth.origin.allocation_generation, original_k_frame_serial, candidate);
	if (error != OK) return error;
	// This same original C row owns the complete failure prefix before any
	// native effect, and cannot retire/free while the synchronous capture runs.
	depth.native_frame = candidate;
	depth.native_operation = true;
	pci->pending_frame = candidate;
	pci->staged_height_resources.unref();
	pci->staged_height_bridge = {};
	pci->staged_height_facet = {};
	unlock_mutex_for_call();
	// The existing storage wrapper installs and clears its modifying-begin
	// hook around this exact pending candidate; no sink fields escape it.
	storage->particles_collision_capture_scene_candidate(renderer, instance, geometry);
	error = candidate->capture_error;
	relock_mutex_after_call();
	if (old.is_valid() && old->state == Frame::State::Invalidated && old->native_range.is_valid()) {
		// The original C owner still retains old throughout synchronous capture.
		// Hand its completed prefix to the original RD cursor before dropping it.
		old->native_range->owner_requested_return = true;
	}
	depth.native_operation = false;
	pci = storage->particles_collision_instance_owner.get_or_null(instance);
	if (pci && pci->pending_frame == candidate) pci->pending_frame.unref();
	if (candidate->prefix.is_restoration_unknown() || candidate->prefix.has_open_list() ||
			(candidate->native_range.is_valid() && candidate->native_range->uncaptured_prefix)) {
		// The original native owner must quarantine an unproved close/prefix.
		// The same C row retains every possible native use through device close.
		candidate->capture_error = error != OK ? error : ERR_INVALID_DATA;
		candidate->state = Frame::State::FailedRetiring;
		return rd->_fail_frame_issue(candidate->capture_error);
	}
	if (error != OK) {
		if (old.is_valid() && old->state == Frame::State::Current) {
			// No modifying begin was entered: preserve the original readable
			// Current and C custody. The empty failed range stays in the original
			// finite RD directory until its original observers are returned.
			depth.native_frame = old;
			if (candidate->native_range.is_valid()) candidate->native_range->owner_requested_return = true;
		}
		return error;
	}
	const auto &range = candidate->native_range;
	// The original RD close_checked_capture already authenticated every actual
	// prefix save, raw/checked worker tuple and complete sink/capture counters.
	// Reuse that original closure; do not re-derive a parallel receipt ledger.
	error = rd->_native_height_range_validate(range);
	if (error == OK && (range->capture_sink != &candidate->prefix || !range->recording_closed || range->uncaptured_prefix ||
			range->capture_frame != uint32_t(rd->frame) || range->capture_epoch != rd->frames[rd->frame].qualification_epoch ||
			range->producer_resources.ptr() != candidate->resources.ptr() || range->producer_depth.cell != candidate->resources->depth_witness.cell ||
			range->append_error != OK || range->mapping_error != OK || range->issue_error != OK || range->retention_error != OK ||
			range->completion_error != OK || range->return_error != OK || range->observers_returned)) error = ERR_INVALID_DATA;
	if (error == OK && (!pci || pci->scene_source_key != candidate->snapshot.source_generation || bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started)) error = ERR_UNAVAILABLE;
	if (error != OK) {
		candidate->capture_error = error;
		candidate->state = Frame::State::FailedRetiring;
		return error;
	}
	// Only this successful source capture enters the post-update notification.
	// Keep the original C operation pin until original dispatch consumes it.
	// No occurrence, Current, ordinal, phase borrow or E09 permit is issued here.
	depth.native_operation = true;
	depth.publication = { bridge, scene_height_recorded_head, index };
	scene_height_recorded_head = &depth.publication;
	return OK;
}

// Original post-update dispatcher consumes only actual successful captures.
// Zero head is zero Height work; this is not another directory or authority.
// p_dispatch_error releases selected operation pins when no occurrence issued.
void Registry::scene_height_publish_recorded_locked(RenderingDevice &device, uint64_t occurrence, Error p_dispatch_error) {
	if (!scene_height_recorded_head) return;
	using Storage = RendererRD::ParticlesStorage;
	using Frame = Storage::HeightFrame;
	auto *storage = Storage::get_singleton();
	while (scene_height_recorded_head) {
		auto *notification = scene_height_recorded_head;
		auto *bridge = notification->bridge;
		const uint32_t index = notification->child_index;
		scene_height_recorded_head = notification->next;
		// Capture admitted this exact row; its operation pin prevents collection,
		// retirement and bridge close before this synchronous RT consumption.
		auto *depth = bridge->scene_depth[index];
		CRASH_COND(!depth || &depth->publication != notification || !depth->native_operation || depth->native_frame.is_null());
		*notification = {};
#ifdef TESTS_ENABLED
		++scene_height_publication_consumed;
#endif
		const Ref<Frame> &frame = depth->native_frame;
		CRASH_COND(frame->state != Frame::State::CaptureRecorded);
		auto &record = records[bridge->library];
		auto &registered_device = device_records[bridge->device];
		Error error = p_dispatch_error;
		if (error == OK && (!storage || !occurrence || occurrence != scene_phase_occurrence ||
				!registered_device.live || registered_device.epoch != bridge->epoch || registered_device.object != &device ||
				!device.is_main_instance || RenderingDevice::get_singleton() != &device ||
				registered_device.execution_thread != Thread::get_caller_id() || device.render_thread_id != Thread::get_caller_id() ||
				!bridge->holder || bridge->holder->pending || bridge->holder->generation != record.snapshot.library_generation ||
				depth->retiring || depth->acknowledged)) error = ERR_UNAVAILABLE;
		const auto &range = frame->native_range;
		auto *pci = storage ? storage->particles_collision_instance_owner.get_or_null(frame->snapshot.instance) : nullptr;
		const auto *base = pci ? storage->particles_collision_owner.get_or_null(pci->collision) : nullptr;
		if (error == OK) error = device._native_height_range_validate(range);
		if (error == OK && (bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started ||
				frame->capture_error != OK || frame->snapshot.producer_occurrence || frame->content_ordinal ||
				frame->resources != depth->native_owner || frame->snapshot.original_allocation_generation != depth->origin.allocation_generation ||
				range->capture_sink != &frame->prefix || !range->recording_closed || range->uncaptured_prefix ||
				range->capture_frame != uint32_t(device.frame) || range->capture_epoch != device.frames[device.frame].qualification_epoch ||
				range->producer_resources.ptr() != frame->resources.ptr() || range->producer_depth.cell != frame->resources->depth_witness.cell ||
				range->append_error != OK || range->mapping_error != OK || range->issue_error != OK || range->retention_error != OK ||
				range->completion_error != OK || range->return_error != OK || range->owner_requested_return || range->observers_returned ||
				!pci || !pci->active || !pci->scene_managed || pci->pending_frame.is_valid() || !base ||
				base->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE || frame->snapshot.base != pci->collision ||
				pci->scene_source_key != frame->snapshot.source_generation || base->scene_shape_revision != frame->snapshot.shape_revision ||
				pci->transform != frame->snapshot.transform || base->extents != frame->snapshot.extents || base->heightfield_resolution != frame->snapshot.resolution)) error = ERR_INVALID_DATA;
		if (error == OK && (!pci->next_scene_content_ordinal || pci->next_scene_content_ordinal == UINT64_MAX)) error = ERR_PARAMETER_RANGE_ERROR;
		depth->native_operation = false;
		if (error != OK) {
			frame->capture_error = error;
			frame->state = Frame::State::FailedRetiring;
			continue;
		}
		// This is the sole zero-to-issued occurrence attachment. Geometry in
		// the const snapshot remains frozen throughout capture/publication.
		frame->snapshot.producer_occurrence = occurrence;
		frame->state = Frame::State::RecordedOrdered;
		frame->content_ordinal = pci->next_scene_content_ordinal++;
		frame->state = Frame::State::Current;
		pci->current_frame = frame;
	}
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
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationBridge::SceneDepth *child = nullptr;
	{
		Lock lock(*this);
		Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
		if (error != OK) {
			return result(error, 16);
		}
		uint32_t index;
		error = qualification_scene_depth_locked(bridge, request->facet, index);
		if (error != OK) {
			return result(error, 17);
		}
		child = bridge->scene_depth[index];
		if (!origin(child->origin, request->allocation_slot, request->part, request->allocation_generation)) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (child->native_operation) {
			return result(ERR_BUSY, 16);
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
		// Original K owns the quoted pair and every partial allocation. Registry
		// retains that exact Ref, never reconstructs the pair or creates RIDs.
		child->creation = *request;
		child->create_attempted = true;
		child->report.create_attempt = 2;
		child->native_operation = true;
	}
	// The original child pins custody across native callbacks, as storage_create.
	// Report publication resumes only after reacquiring Registry::mutex.
	NativeDepthAllocationQuote::CreateOutcome outcome;
	NativeDepthAllocationProjection::Report observed;
	Error observation = OK;
	const Error creation = RendererRD::ParticlesStorage::HeightFrameResources::allocate_quoted(*rd, child->native_quote, child->native_owner, outcome);
	if (child->native_owner.is_valid()) {
		observation = child->native_owner->observe_allocation(observed);
	}
	Lock lock(*this);
	child->native_operation = false;
	child->report.creation_error = creation;
	child->report.create_outcome = uint32_t(outcome);
	child->report.maintenance_error = observation;
	allocation_report(observed, child->report);
	child->report.state = child->report.maintenance_error != OK ? 7 :
			child->report.creation_error == OK ? 3 : child->report.known_created_mask ? 4 : 8;
	child->report.custody = child->report.maintenance_error != OK ? 5 : child->report.known_created_mask ? 2 : 1;
	++child->report.revision;
	child->create_report = child->report;
	*out = child->report;
	return result(OK);
}

// Executed only by the original retained RT cell. This hook services the exact
// recorded source; physical allocation retirement remains its separate episode.
Error Registry::qualification_scene_depth_service_execute_v2(QualificationBridge *bridge, QualificationRtCell *cell, RenderingDevice *rd) {
	using Storage = RendererRD::ParticlesStorage;
	using Frame = Storage::HeightFrame;
	using Owner = Storage::HeightFrameResources;
	if (!bridge || !cell || !rd) return ERR_INVALID_PARAMETER;
	const auto request = cell->height_request;
	const auto grant = cell->height_paid.grant;
	const uint64_t execution = cell->height_paid.execution_serial;
	const uint32_t controls = cell->height_paid.used.controls;
	const uint64_t bridge_serial = bridge->serial, epoch = bridge->epoch, cell_serial = cell->serial;
	QualificationBridge::SceneDepth *child = nullptr;
	Ref<Frame> frame, source;
	Ref<Owner> owner;
	Storage::ParticlesCollisionInstance *pci = nullptr;
	RenderingDevice::NativeDisposalBudget total;
	RenderingDevice::NativeDisposalUsage used;
	bool continuation = false;

	// Every call is under Registry::mutex, including the post-native phase gate.
	auto authenticate = [&]() -> Error {
		QualificationBridge *original = nullptr;
		RenderingDevice *device = nullptr;
		Error error = qualification_bridge_locked({ instance, bridge_serial }, true, false, original, device);
		if (error != OK) return error;
		if (original != bridge || device != rd || bridge->epoch != epoch || bridge->rt != cell || cell->serial != cell_serial ||
				!cell->occupied || !cell->active || !cell->queued || !cell->executing || !cell->height_service_v2 ||
				active_qualification_bridge != bridge || active_qualification_serial != cell_serial ||
				memcmp(&request, &cell->height_request, sizeof(request)) || !cell->height_paid.accepted || !cell->height_paid.executing ||
				!cell->height_paid.valid_usage() || !execution || cell->height_paid.execution_serial != execution || cell->height_paid.used.controls != controls ||
				memcmp(&grant, &cell->height_paid.grant, sizeof(grant)) || cell->height_request.operation_serial != cell->header.operation_serial ||
				cell->scene_depth_serial != request.facet.handle_serial || (request.mode != 1 && request.mode != 2)) return ERR_INVALID_DATA;
		uint32_t index = UINT32_MAX;
		error = qualification_scene_depth_locked(bridge, request.facet, index);
		if (error != OK) return error;
		auto *selected = bridge->scene_depth[index];
		if ((child && child != selected) || selected->quoting || !selected->native_operation || !selected->accepted_rt_refs ||
				selected->origin.allocation_slot != request.allocation_slot || selected->origin.part != request.part ||
				selected->origin.allocation_generation != request.allocation_generation || selected->native_owner.is_null() || selected->native_frame.is_null() ||
				selected->native_frame.ptr() != cell->height_frame_identity || selected->native_owner.ptr() != cell->height_owner_identity ||
				selected->native_frame->native_range.ptr() != cell->height_range_identity || selected->native_frame->resources != selected->native_owner ||
				&selected->native_owner->device != rd || selected->native_frame->snapshot.original_allocation_generation != request.allocation_generation ||
				selected->native_frame->snapshot.original_k_frame_serial != request.expected_frame_version ||
				cell->height_execution_frame_hold.ptr() != selected->native_frame.ptr() || cell->height_execution_owner_hold.ptr() != selected->native_owner.ptr() ||
				cell->height_execution_range_hold != selected->native_frame->native_range) return ERR_INVALID_DATA;
		child = selected;
		if ((frame.is_valid() && (child->native_frame != frame || child->native_owner != owner)) ||
				(source.is_valid() && child->native_frame != source)) return ERR_INVALID_DATA;
		auto *storage = Storage::get_singleton();
		if (!storage) return ERR_UNAVAILABLE;
		const auto &frozen = child->native_frame->snapshot;
		pci = storage->particles_collision_instance_owner.get_or_null(frozen.instance);
		// RETIRE authenticates the retained original physical objects. The live
		// Scene RID may have lost/replaced its base and PCI in the meantime.
		if (request.mode == 1) {
			if (pci && pci->scene_source_key != frozen.source_generation) return ERR_INVALID_DATA;
			auto *scene = RendererSceneCull::singleton;
			auto *scene_source = scene && child->native_scene_source.is_valid() ? scene->instance_owner.get_or_null(child->native_scene_source) : nullptr;
			if (scene_source && (scene_source->base_type != RSE::INSTANCE_PARTICLES_COLLISION || !scene_source->base_data ||
					static_cast<const RendererSceneCull::InstanceParticlesCollisionData *>(scene_source->base_data)->instance != frozen.instance)) return ERR_BUSY;
			const auto *base = pci ? storage->particles_collision_owner.get_or_null(pci->collision) : nullptr;
			if (bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started || rd->production_finalization_started.load() ||
					child->retiring || !scene_source || !scene_source->scenario || !scene_source->visible || !pci || !pci->active || !pci->scene_managed || !base ||
					base->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE || pci->current_frame != child->native_frame ||
					child->native_frame->state != Frame::State::Current || !child->native_frame->content_ordinal || !frozen.producer_occurrence ||
					child->native_frame->capture_error != OK || frozen.base != pci->collision || frozen.shape_revision != base->scene_shape_revision ||
					frozen.transform != pci->transform || frozen.extents != base->extents || frozen.resolution != base->heightfield_resolution) return ERR_BUSY;
		} else if (pci && pci->scene_source_key != frozen.source_generation) {
			pci = nullptr; // Never detach bindings of a replacement source.
		}
		return OK;
	};
	auto no_independent_sample = [&]() {
		// Name each strong hold: child, RT transport, local frame, optional
		// getter source, then only the exact current/pending bindings.
		const uint32_t named = 3 + uint32_t(source.is_valid()) + uint32_t(pci && pci->current_frame == frame) + uint32_t(pci && pci->pending_frame == frame);
		return frame->get_reference_count() == named;
	};
	auto save_usage = [&](const NativeHeightRange::Snapshot &snapshot, bool admitted) {
		// Replace cumulative native7 fields, never add them again. RT alone owns
		// the eighth wire field, controls, which this hook never writes.
		auto &wire = cell->height_paid.used;
		wire.polls = used.polls; wire.frees = used.frees; wire.issues = used.issues;
		wire.retains = used.retains; wire.releases = used.releases; wire.recycles = used.recycles; wire.passes = used.passes;
		if (admitted) { cell->height_snapshot = snapshot; cell->height_row_admitted = true; }
	};
	{
		Lock lock(*this);
		const Error error = authenticate();
		if (error != OK) return error;
		owner = child->native_owner;
		frame = child->native_frame; // Original custody before unlocked getter.
		const auto &wire = cell->height_paid.used;
		used.polls = wire.polls; used.frees = wire.frees; used.issues = wire.issues;
		used.retains = wire.retains; used.releases = wire.releases; used.recycles = wire.recycles; used.passes = wire.passes;
		total = { cell->height_paid.grant.receipts, cell->height_paid.grant.passes, cell->height_paid.grant.deadline_usec };
		continuation = child->retiring;
		if (request.mode == 2 && !no_independent_sample()) return ERR_BUSY;
		cell->height_report = child->report;
	}
	const Error pinned = Frame::pin_native_source(frame, owner, source);
	NativeHeightRange::Snapshot initial;
	if (pinned == OK) rd->_native_height_range_snapshot(source->native_range, initial); // Pure projection outside Registry.
	{
		Lock lock(*this);
		const Error error = authenticate();
		if (error != OK) return error;
		if (pinned != OK) return pinned;
		if (child->retiring != continuation) return ERR_INVALID_DATA;
		if (request.mode == 1 && (initial.owner_requested_return || initial.observers_returned)) return ERR_BUSY;
		if (request.mode == 2) {
			if (!no_independent_sample()) return ERR_BUSY;
			if (!continuation && (initial.owner_requested_return || initial.observers_returned)) return ERR_INVALID_DATA;
			if (continuation) {
				const GDExtensionStorageSceneDepthRetireRequestV1 expected{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, request.facet, request.allocation_slot, request.part, request.allocation_generation, 3, 0 };
				if (memcmp(&expected, &child->retirement, sizeof(expected)) || child->report.retire_attempt != 3) return ERR_INVALID_DATA;
				const bool staged = pci && pci->staged_height_bridge.registry_instance == instance && pci->staged_height_bridge.handle_serial == bridge_serial &&
						pci->staged_height_facet.registry_instance == request.facet.registry_instance && pci->staged_height_facet.handle_serial == request.facet.handle_serial && pci->staged_height_resources == owner;
				if (pci && (pci->current_frame == source || pci->pending_frame == source || staged)) return ERR_BUSY;
			}
		}
	}

	NativeHeightRange::Snapshot snapshot;
	bool admitted = false;
	// CURRENT: one false service, no detach. RETIRE continuation: one true
	// service. Fresh RETIRE always pays a genuine false service first.
	const Error first = Frame::service_native_source(source, owner, request.mode == 2 && continuation, total, used, snapshot, admitted);
	// Physical cleanup may continue, but a later policy wait or service result
	// never replaces the first actual non-Busy cause from this dispatch.
	auto first_cause = [&](Error later) { return first != OK && first != ERR_BUSY ? first : later; };
	{
		Lock lock(*this);
		save_usage(snapshot, admitted); // Every return retains actual spend.
		const Error error = authenticate();
		if (error != OK || child->retiring != continuation) { cell->height_row_admitted = false; return first_cause(error != OK ? error : ERR_INVALID_DATA); }
		if (request.mode == 1 || continuation || !admitted) return first;
		// A new independent sample during phase1 prevents detach. Its paid
		// snapshot/spend and first native cause remain on the original cell.
		if (!no_independent_sample()) return first_cause(ERR_BUSY);
	}
	const Error validated = source->validate_native_source(owner); // Getter outside Registry.
	{
		Lock lock(*this);
		const Error error = authenticate();
		if (error != OK) { cell->height_row_admitted = false; return first_cause(error); }
		if (validated != OK) { cell->height_row_admitted = false; return first_cause(validated); }
		if (!no_independent_sample()) return first_cause(ERR_BUSY);
		if (child->report.revision == UINT64_MAX) return first_cause(ERR_PARAMETER_RANGE_ERROR);
		if (pci) {
			if (pci->staged_height_bridge.registry_instance == instance && pci->staged_height_bridge.handle_serial == bridge_serial &&
					pci->staged_height_facet.registry_instance == request.facet.registry_instance && pci->staged_height_facet.handle_serial == request.facet.handle_serial &&
					pci->staged_height_resources == owner) {
				pci->staged_height_resources.unref(); pci->staged_height_bridge = {}; pci->staged_height_facet = {};
			}
			if (pci->current_frame == source) pci->current_frame.unref();
			if (pci->pending_frame == source) pci->pending_frame.unref();
		}
		child->retirement = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, request.facet, request.allocation_slot, request.part, request.allocation_generation, 3, 0 };
		child->retiring = true;
		child->report.retire_attempt = 3;
		child->report.state = child->report.maintenance_error == OK ? 5 : 7;
		child->report.custody = child->report.maintenance_error == OK ? 3 : 5;
		++child->report.revision;
		cell->height_report = child->report;
	}
	NativeHeightRange::Snapshot returned_snapshot;
	bool return_admitted = false;
	const Error second = Frame::service_native_source(source, owner, true, total, used, returned_snapshot, return_admitted);
	{
		Lock lock(*this);
		// An unadmitted phase2 must not erase phase1's snapshot/admission.
		save_usage(returned_snapshot, return_admitted);
		const Error error = authenticate();
		if (error != OK) { cell->height_row_admitted = false; return first_cause(error); }
		cell->height_report = child->report;
	}
	return first_cause(second);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_scene_depth_retire(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionStorageSceneDepthRetireRequestV1 *request, GDExtensionStorageSceneDepthReportV1 *out) {
	if (!aligned(request) || !aligned(out) || !separate(request, sizeof(*request), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(request->abi) || request->reserved || !request->allocation_generation || request->retire_attempt != 3) {
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
		qualification_accepted_call_locked(bridge, 23);
		uint32_t index;
		error = qualification_scene_depth_locked(bridge, request->facet, index);
		if (error != OK) {
			return result(error, 17);
		}
		child = bridge->scene_depth[index];
		if (!origin(child->origin, request->allocation_slot, request->part, request->allocation_generation)) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (child->native_operation) {
			return result(ERR_BUSY, 16);
		}
		if (child->retiring && memcmp(&child->retirement, request, sizeof(*request))) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (child->retiring && (child->report.state == 6 || child->report.state == 9)) {
			*out = child->report;
			return result(OK);
		}
		if (child->quoting) {
			return result(ERR_BUSY, 16);
		}
		if (child->report.revision == UINT64_MAX) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		// Close future capture/new source borrows on this exact original row.
		// An older COW row must never detach a newer row's current/staging.
		auto *storage = RendererRD::ParticlesStorage::get_singleton();
		auto *scene = RendererSceneCull::singleton;
		auto *source = scene && child->native_scene_source.is_valid() ? scene->instance_owner.get_or_null(child->native_scene_source) : nullptr;
		if (source && source->base_type == RSE::INSTANCE_PARTICLES_COLLISION && source->base_data) {
			const auto *collision = static_cast<const RendererSceneCull::InstanceParticlesCollisionData *>(source->base_data);
			auto *pci = storage->particles_collision_instance_owner.get_or_null(collision->instance);
			if (pci) {
				if (pci->staged_height_bridge.registry_instance == ref.registry_instance && pci->staged_height_bridge.handle_serial == ref.handle_serial &&
						pci->staged_height_facet.registry_instance == request->facet.registry_instance && pci->staged_height_facet.handle_serial == request->facet.handle_serial &&
						pci->staged_height_resources == child->native_owner) {
					pci->staged_height_resources.unref(); pci->staged_height_bridge = {}; pci->staged_height_facet = {};
				}
				if (child->native_frame.is_valid() && pci->current_frame == child->native_frame) pci->current_frame.unref();
				if (child->native_frame.is_valid() && pci->pending_frame == child->native_frame) pci->pending_frame.unref();
			}
		}
		child->retirement = *request;
		child->retiring = true;
		child->report.retire_attempt = 3;
		child->native_operation = true;
	}
	// Each exact RT retry advances at most the original K owner's one
	// bounded observation/free step. Creation failure never prevents cleanup.
	Error maintenance = OK;
	NativeDepthAllocationProjection::Report observed;
	if (child->native_frame.is_valid()) {
		// Only the original row is an owner now; retained independent samples
		// and actual legacy readers must release/physically return first.
		if (child->native_frame->get_reference_count() != 1) {
			maintenance = ERR_BUSY;
		} else if (child->native_frame->native_range.is_valid() && !child->native_frame->native_range->observers_returned) {
			// Original bounded RD return cursor services this same producer range.
			// No second observer, worker wait, poll loop or native budget is minted.
			child->native_frame->native_range->owner_requested_return = true;
			maintenance = ERR_BUSY;
		} else {
			child->native_frame.unref();
		}
	}
	if (child->native_owner.is_valid()) {
		if (maintenance == OK) maintenance = child->native_owner->retire_step();
		const Error observation = child->native_owner->observe_allocation(observed);
		if (observation != OK) {
			maintenance = observation;
		}
	}
	Lock lock(*this);
	child->native_operation = false;
	allocation_report(observed, child->report);
	// Preserve the first original maintenance failure, as other retained
	// qualification children do. Pending retirement is not a failure.
	if (child->report.maintenance_error == OK && maintenance != ERR_BUSY) {
		child->report.maintenance_error = maintenance;
	}
	const bool returned = maintenance == OK && child->report.known_created_mask && child->report.all_created_children_returned &&
			(!(child->report.known_created_mask & 1) || child->report.depth_state == uint32_t(NativeDisposalWitness::State::RETURNED)) &&
			(!(child->report.known_created_mask & 2) || child->report.framebuffer_state == uint32_t(NativeDisposalWitness::State::RETURNED));
	child->report.state = child->report.maintenance_error != OK ? 7 :
			!child->report.known_created_mask ? 9 : returned ? 6 : 5;
	child->report.custody = child->report.maintenance_error != OK ? 5 :
			child->report.state == 6 ? 4 : child->report.state == 9 ? 1 : 3;
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
	if (bridge->scene_depth[index]->quoting || bridge->scene_depth[index]->native_operation) {
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
	const auto &report = child->report;
	const bool no_native = report.state == 9 && !report.known_created_mask && !report.all_created_children_returned && !report.depth_state && !report.framebuffer_state;
	const bool returned = report.state == 6 && report.custody == 4 && report.known_created_mask && report.all_created_children_returned &&
			(!(report.known_created_mask & 1) || report.depth_state == uint32_t(NativeDisposalWitness::State::RETURNED)) &&
			(!(report.known_created_mask & 2) || report.framebuffer_state == uint32_t(NativeDisposalWitness::State::RETURNED));
	if (child->quoting || child->native_operation || child->native_frame.is_valid() || !child->retiring || (!no_native && !returned) || report.maintenance_error != OK || child->accepted_rt_refs || (bridge->rt && bridge->rt->occupied && bridge->rt->scene_depth_serial == child->serial)) {
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
