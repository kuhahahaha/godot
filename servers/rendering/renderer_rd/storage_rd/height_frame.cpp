/**************************************************************************/
/*  height_frame.cpp                                             */
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

#include "height_frame.h"

#include "core/extension/gdextension_library_exit_guard.h"

#include "servers/rendering/native_height_range.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"

namespace RendererRD {

ParticlesStorage::HeightFrame::HeightFrame(const Snapshot &p_snapshot, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity) :
		snapshot(p_snapshot), resources(p_resources), prefix(p_resources, p_native_capacity, p_transfer_capacity) {
}

Error ParticlesStorage::HeightFrame::create_candidate(ParticlesStorage &p_storage, RID p_instance, uint64_t p_original_occurrence, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity, uint64_t p_original_allocation_generation, uint64_t p_original_k_frame_serial, Ref<HeightFrame> &r_frame) {
	ERR_FAIL_COND_V(r_frame.is_valid() || p_resources.is_null(), ERR_INVALID_PARAMETER);
	const ParticlesCollisionInstance *instance = p_storage.particles_collision_instance_owner.get_or_null(p_instance);
	if (!instance || !instance->active || !instance->scene_source_key) {
		return ERR_DOES_NOT_EXIST;
	}
	const ParticlesCollision *base = p_storage.particles_collision_owner.get_or_null(instance->collision);
	if (!base || base->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_resources->is_quoted_verified()) {
		return ERR_INVALID_DATA;
	}
	HeightFrameResources::Observation allocation;
	const Error observed = p_resources->observe(allocation);
	if (observed != OK) {
		return observed;
	}
	if (allocation.depth.state != NativeDisposalWitness::State::LIVE || allocation.framebuffer.state != NativeDisposalWitness::State::LIVE) {
		return ERR_INVALID_DATA;
	}
	Snapshot frozen;
	frozen.instance = p_instance;
	frozen.base = instance->collision;
	frozen.source_generation = instance->scene_source_key;
	frozen.shape_revision = base->scene_shape_revision;
	frozen.producer_occurrence = p_original_occurrence;
	frozen.original_allocation_generation = p_original_allocation_generation;
	frozen.original_k_frame_serial = p_original_k_frame_serial;
	frozen.shape = base->type;
	frozen.transform = instance->transform;
	frozen.extents = base->extents;
	frozen.bounds = AABB(-base->extents, base->extents * 2);
	frozen.cull_mask = base->cull_mask;
	frozen.heightfield_mask = base->heightfield_mask;
	frozen.resolution = base->heightfield_resolution;
	frozen.width = p_resources->width;
	frozen.height = p_resources->height;
	frozen.depth_allocation = allocation.depth;
	frozen.framebuffer_allocation = allocation.framebuffer;
	r_frame = Ref<HeightFrame>(memnew(HeightFrame(frozen, p_resources, p_native_capacity, p_transfer_capacity)));
	return OK;
}

Error ParticlesStorage::HeightFrame::capture(RendererSceneRenderRD &p_renderer, const PagedArray<RenderGeometryInstance *> &p_geometry) {
	if (state != State::Allocated) {
		return ERR_ALREADY_IN_USE;
	}
	state = State::Recording;
	capture_error = prefix.prepare_native_range(native_range);
	if (capture_error != OK) {
		state = State::FailedRetiring;
		return capture_error;
	}
	capture_error = p_renderer.render_particle_collider_heightfield_checked(resources->get_framebuffer(), snapshot.transform, snapshot.extents, p_geometry, prefix);
	if (capture_error != OK) {
		prefix.reject(capture_error);
		prefix.unwind();
	}
	if (capture_error == OK) {
		capture_error = prefix.get_error();
	}
	bool recorded = capture_error == OK && prefix.is_restored() && !prefix.has_open_list() &&
			!prefix.is_restoration_unknown() && !prefix.has_cleanup_attempt() && prefix.get_business_count() >= 2;
	if (recorded) {
		const auto &begin = prefix.get_business_record(0);
		const auto &end = prefix.get_business_record(prefix.get_business_count() - 1);
		recorded = begin.operation == HeightCaptureCheckedSink::Operation::Begin && begin.resource == resources->get_framebuffer() &&
				begin.begin.effect == HeightCaptureCheckedSink::Effect::Recorded && begin.begin.capture_complete &&
				begin.begin.begin_handle.is_valid() && begin.begin.clear_handle.is_valid() &&
				end.operation == HeightCaptureCheckedSink::Operation::End && end.draw.effect == HeightCaptureCheckedSink::Effect::Recorded &&
				end.draw.capture_complete && end.draw.draw_handle.is_valid();
	}
	if (!recorded) {
		if (capture_error == OK) {
			capture_error = ERR_INVALID_DATA;
			prefix.reject(capture_error);
			prefix.unwind();
		}
		state = State::FailedRetiring;
		return capture_error;
	}
	state = State::CaptureRecorded;
	return OK;
}

Error ParticlesStorage::HeightFrame::validate_native_source(const Ref<HeightFrameResources> &p_original_owner) const {
	if (p_original_owner.is_null() || resources != p_original_owner || prefix.quoted_resources != resources ||
			&prefix.rd != &resources->device || native_range.is_null() || prefix.native_range != native_range ||
			state == State::Allocated || state == State::Recording || !snapshot.instance.is_valid() || !snapshot.base.is_valid() ||
			!snapshot.source_generation || !snapshot.shape_revision || !snapshot.original_allocation_generation || !snapshot.original_k_frame_serial ||
			snapshot.width != resources->width || snapshot.height != resources->height) return ERR_INVALID_DATA;
	NativeHeightRange::Snapshot physical;
	const Error checked = resources->device._native_height_range_validate_checked_source(native_range, prefix, resources->depth_witness, physical);
	if (checked != OK) return checked;
	HeightFrameResources::Observation observed;
	const Error witnessed = resources->observe(observed); // Fixed witness reads; never a native completion poll.
	if (witnessed != OK) return witnessed;
	auto same = [&](const NativeDisposalWitness::Report &p_now, const NativeDisposalWitness::Report &p_original) {
		return p_original.rid && p_original.serial && p_now.rid == p_original.rid && p_now.serial == p_original.serial &&
				(p_now.state == NativeDisposalWitness::State::LIVE || (physical.observers_returned &&
						(p_now.state == NativeDisposalWitness::State::ENQUEUED || p_now.state == NativeDisposalWitness::State::RETURNED)));
	};
	if (!same(observed.depth, snapshot.depth_allocation) || !same(observed.framebuffer, snapshot.framebuffer_allocation) ||
			resources->depth.get_id() != snapshot.depth_allocation.rid || resources->framebuffer.get_id() != snapshot.framebuffer_allocation.rid) return ERR_INVALID_DATA;
	if (prefix.business_count && (prefix.business[0].operation != HeightCaptureCheckedSink::Operation::Begin || prefix.business[0].resource != resources->framebuffer)) return ERR_INVALID_DATA;
	return OK;
}

Error ParticlesStorage::HeightFrame::pin_native_source(const Ref<HeightFrame> &p_original_frame, const Ref<HeightFrameResources> &p_original_owner, Ref<HeightFrame> &r_source) {
	if (r_source.is_valid() || p_original_frame.is_null()) return ERR_INVALID_PARAMETER;
	const Error valid = p_original_frame->validate_native_source(p_original_owner);
	if (valid != OK) return valid;
	r_source = p_original_frame; // The same original Frame, never a wrapper/token.
	return OK;
}

Error ParticlesStorage::HeightFrame::service_native_source(const Ref<HeightFrame> &p_original_source, const Ref<HeightFrameResources> &p_original_owner, bool p_request_return, const RenderingDevice::NativeDisposalBudget &p_total_budget, RenderingDevice::NativeDisposalUsage &r_cumulative_usage, NativeHeightRange::Snapshot &r_snapshot, bool &r_admitted) {
	r_snapshot = {};
	r_admitted = false;
	// Strong local holds survive original RD unlock/return and directory erasure.
	const Ref<HeightFrame> source = p_original_source;
	const Ref<HeightFrameResources> owner = p_original_owner;
	if (source.is_null()) return ERR_INVALID_PARAMETER;
	const Error valid = source->validate_native_source(owner);
	if (valid != OK) return valid;
	const Ref<NativeHeightRange> range = source->native_range;
	const Error serviced = owner->device._native_height_range_service_step(range, p_request_return, p_total_budget, r_cumulative_usage, r_snapshot, r_admitted);
	// Do not erase the admitted flag, actual prefix snapshot or spent usage if
	// a post-call identity failure is found. Scene rechecks its own row on relock.
	if (p_original_source != source || p_original_owner != owner || source->native_range != range) return ERR_INVALID_DATA;
	const Error revalidated = source->validate_native_source(owner);
	return revalidated != OK ? revalidated : serviced;
}

void ParticlesStorage::_heightfield_before_modify(void *p_instance) {
	auto &instance = *static_cast<ParticlesCollisionInstance *>(p_instance);
	// Independent COW content is untouched. Reuse revokes old readability at
	// the first modifying begin, even when the following native call fails.
	if (instance.current_frame.is_valid() && instance.pending_frame.is_valid() &&
			instance.current_frame->resources == instance.pending_frame->resources) {
		instance.current_frame->state = HeightFrame::State::Invalidated;
		instance.current_frame.unref();
	}
}

bool ParticlesStorage::particles_collision_capture_scene_candidate(RendererSceneRenderRD &p_renderer, RID p_instance, const PagedArray<RenderGeometryInstance *> &p_geometry) {
	auto *instance = particles_collision_instance_owner.get_or_null(p_instance);
	if (!instance || !instance->scene_managed) {
		return false;
	}
	// An already managed source must never fall back to an uncharged raw
	// target when no original phase candidate has been admitted.
	if (instance->pending_frame.is_null()) {
		if (instance->staged_height_resources.is_valid()) {
			GDExtensionDeviceExit::Registry::get_singleton().scene_height_capture_source(p_instance, p_renderer, p_geometry);
		}
		return true;
	}
	Ref<HeightFrame> candidate = instance->pending_frame;
	if (candidate->state != HeightFrame::State::Allocated || candidate->snapshot.instance != p_instance) {
		return true;
	}
	candidate->prefix.before_modify = &_heightfield_before_modify;
	candidate->prefix.before_modify_data = instance;
	candidate->capture(p_renderer, p_geometry);
	candidate->prefix.before_modify = nullptr;
	candidate->prefix.before_modify_data = nullptr;
	// Keep the exact complete prefix in the original action-owned candidate.
	// Registry checks accepted producer attachment before any publication.
	return true;
}

ParticlesStorage::HeightFrame::~HeightFrame() {
	// The original renderer must explicitly close/unwind. Unknown native state
	// remains a session-owner quarantine responsibility, never implicit cleanup.
	CRASH_COND_MSG(prefix.has_open_list(), "Scene height candidate destroyed with an unresolved native draw list.");
}

} // namespace RendererRD
