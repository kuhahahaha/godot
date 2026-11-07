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

#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"

namespace RendererRD {

ParticlesStorage::HeightFrame::HeightFrame(const Snapshot &p_snapshot, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity) :
		snapshot(p_snapshot), resources(p_resources), prefix(p_resources, p_native_capacity, p_transfer_capacity) {
}

Error ParticlesStorage::HeightFrame::create_candidate(ParticlesStorage &p_storage, RID p_instance, uint64_t p_original_occurrence, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity, Ref<HeightFrame> &r_frame) {
	ERR_FAIL_COND_V(r_frame.is_valid() || p_resources.is_null() || !p_original_occurrence, ERR_INVALID_PARAMETER);
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
	capture_error = p_renderer.render_particle_collider_heightfield_checked(resources->get_framebuffer(), snapshot.transform, snapshot.extents, p_geometry, prefix);
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
		}
		state = State::FailedRetiring;
		return capture_error;
	}
	state = State::CaptureRecorded;
	return OK;
}

ParticlesStorage::HeightFrame::~HeightFrame() {
	// The original renderer must explicitly close/unwind. Unknown native state
	// remains a session-owner quarantine responsibility, never implicit cleanup.
	CRASH_COND_MSG(prefix.has_open_list(), "Scene height candidate destroyed with an unresolved native draw list.");
}

} // namespace RendererRD
