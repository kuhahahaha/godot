/**************************************************************************/
/*  height_frame_resources.cpp                                             */
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

#include "height_frame_resources.h"

#include "servers/rendering/rendering_device.h"

namespace RendererRD {

ParticlesStorage::HeightFrameResources::HeightFrameResources(RenderingDevice &p_device, uint32_t p_width, uint32_t p_height) :
		device(p_device), width(p_width), height(p_height) {
}

Error ParticlesStorage::HeightFrameResources::allocate(RenderingDevice &p_device, uint32_t p_width, uint32_t p_height, Ref<HeightFrameResources> &r_owner) {
	ERR_FAIL_COND_V(r_owner.is_valid(), ERR_INVALID_PARAMETER);
	r_owner = Ref<HeightFrameResources>(memnew(HeightFrameResources(p_device, p_width, p_height)));
	HeightFrameResources &owner = **r_owner;
	owner.allocation_error = p_device._native_disposal_depth_texture_create(p_width, p_height, owner.depth, owner.depth_witness);
	if (owner.allocation_error != OK) {
		// No resource was published by the factory. No native command prefix
		// exists, and there is nothing to retire.
		owner.state = State::RELEASED;
		return owner.allocation_error;
	}
	owner.allocation_error = p_device._native_disposal_depth_framebuffer_create(owner.depth_witness, owner.framebuffer, owner.framebuffer_witness);
	// Do not roll back or discard the depth prefix on framebuffer failure.
	return owner.allocation_error;
}

Error ParticlesStorage::HeightFrameResources::allocate_quoted(RenderingDevice &p_original_main_rd, const NativeDepthAllocationQuote &p_original_quote, Ref<HeightFrameResources> &r_original_owner, NativeDepthAllocationQuote::CreateOutcome &r_outcome) {
	r_outcome = NativeDepthAllocationQuote::CreateOutcome::NotCreated;
	ERR_FAIL_COND_V(r_original_owner.is_valid(), ERR_INVALID_PARAMETER);
	NativeDepthAllocationQuote::Report quote;
	const Error resolved = p_original_main_rd._native_disposal_depth_allocation_quote_observe(p_original_quote, quote);
	if (resolved != OK) {
		return resolved;
	}
	r_original_owner = Ref<HeightFrameResources>(memnew(HeightFrameResources(p_original_main_rd, quote.width, quote.height)));
	HeightFrameResources &owner = **r_original_owner;
	owner.allocation_quote = p_original_quote;
	owner.allocation_error = p_original_main_rd._native_disposal_depth_texture_create_quoted(p_original_quote, owner.depth, owner.depth_witness, owner.depth_projection, owner.creation_outcome);
	r_outcome = owner.creation_outcome;
	if (owner.creation_outcome == NativeDepthAllocationQuote::CreateOutcome::NotCreated) {
		owner.state = State::RELEASED;
		return owner.allocation_error;
	}
	// The quoted factory may publish depth before detecting an actual-size
	// failure. Its original owner and witness survive every such failure.
	NativeDepthAllocationProjection::Report depth_observation;
	owner.depth_projection_captured = p_original_main_rd._native_disposal_depth_allocation_observe(owner.depth_projection, depth_observation) == OK;
	if (owner.allocation_error != OK) {
		return owner.allocation_error;
	}
	owner.allocation_error = p_original_main_rd._native_disposal_depth_framebuffer_create(owner.depth_witness, owner.framebuffer, owner.framebuffer_witness);
	if (owner.allocation_error != OK) {
		return owner.allocation_error;
	}
	owner.allocation_error = p_original_main_rd._native_disposal_depth_allocation_capture(owner.depth_witness, owner.framebuffer_witness, owner.pair_projection);
	owner.pair_projection_captured = owner.allocation_error == OK;
	return owner.allocation_error;
}

Error ParticlesStorage::HeightFrameResources::observe_allocation(NativeDepthAllocationProjection::Report &r_original_observation) const {
	r_original_observation = {};
	if (pair_projection_captured) {
		return device._native_disposal_depth_allocation_observe(pair_projection, r_original_observation);
	}
	Error projection_error = OK;
	if (depth_projection_captured) {
		projection_error = device._native_disposal_depth_allocation_observe(depth_projection, r_original_observation);
	}
	// A failed pair capture must not hide its real framebuffer. When no
	// snapshot exists, original witnesses still expose identity and return;
	// all byte fields remain typed UnknownNotExposed.
	Observation observation;
	const Error observed = observe(observation);
	r_original_observation.depth = observation.depth;
	r_original_observation.framebuffer = observation.framebuffer;
	r_original_observation.framebuffer_created = framebuffer.is_valid();
	if (framebuffer.is_valid()) {
		r_original_observation.shared_attachment_depth = observation.depth;
	}
	r_original_observation.all_created_children_returned = observed == OK && projection_error == OK && depth.is_valid() &&
			observation.depth.state == NativeDisposalWitness::State::RETURNED &&
			(!framebuffer.is_valid() || observation.framebuffer.state == NativeDisposalWitness::State::RETURNED);
	return projection_error != OK ? projection_error : observed;
}

Error ParticlesStorage::HeightFrameResources::retire_step() {
	if (state == State::RELEASED) {
		return OK;
	}
	if (get_reference_count() > 1) {
		return ERR_BUSY;
	}
	if (state == State::ALLOCATED) {
		state = State::RETIRING;
		retire_target = framebuffer.is_valid() ? RetireTarget::FRAMEBUFFER : RetireTarget::DEPTH;
	}
	const NativeDisposalWitness &witness = retire_target == RetireTarget::FRAMEBUFFER ? framebuffer_witness : depth_witness;
	NativeDisposalWitness::Report report;
	const Error observed = device._native_disposal_observe(witness, report);
	if (observed != OK) {
		return observed;
	}
	switch (report.state) {
		case NativeDisposalWitness::State::LIVE: {
			const Error requested = device._native_disposal_free(witness);
			// Request acceptance is not physical return, even if the factory
			// happens to dispose synchronously. Observe on the next pump step.
			return requested == OK ? ERR_BUSY : requested;
		}
		case NativeDisposalWitness::State::ENQUEUED:
			return ERR_BUSY;
		case NativeDisposalWitness::State::RETURNED:
			if (retire_target == RetireTarget::FRAMEBUFFER) {
				retire_target = RetireTarget::DEPTH;
				return ERR_BUSY;
			}
			state = State::RELEASED;
			return OK;
		case NativeDisposalWitness::State::EMPTY:
		case NativeDisposalWitness::State::UNKNOWN:
			return ERR_INVALID_DATA;
	}
	return ERR_BUG;
}

Error ParticlesStorage::HeightFrameResources::observe(Observation &r_observation) const {
	r_observation = Observation();
	r_observation.state = state;
	r_observation.allocation_error = allocation_error;
	r_observation.creation_outcome = creation_outcome;
	Error depth_error = OK;
	Error framebuffer_error = OK;
	if (depth.is_valid()) {
		depth_error = device._native_disposal_observe(depth_witness, r_observation.depth);
	}
	if (framebuffer.is_valid()) {
		framebuffer_error = device._native_disposal_observe(framebuffer_witness, r_observation.framebuffer);
	}
	return depth_error != OK ? depth_error : framebuffer_error;
}

bool ParticlesStorage::HeightFrameResources::is_quoted_verified() const {
	if (state != State::ALLOCATED || allocation_error != OK || creation_outcome != NativeDepthAllocationQuote::CreateOutcome::CreatedVerified) {
		return false;
	}
	NativeDepthAllocationQuote::Report quote;
	return device._native_disposal_depth_allocation_quote_observe(allocation_quote, quote) == OK;
}

RID ParticlesStorage::HeightFrameResources::get_depth() const {
	ERR_FAIL_COND_V(state != State::ALLOCATED || allocation_error != OK, RID());
	return depth;
}

RID ParticlesStorage::HeightFrameResources::get_framebuffer() const {
	ERR_FAIL_COND_V(state != State::ALLOCATED || allocation_error != OK, RID());
	return framebuffer;
}

ParticlesStorage::HeightFrameResources::~HeightFrameResources() {
	if (state == State::RELEASED) {
		return;
	}
	// A final original-RD disposal can return both witnesses before the next
	// owner poll. Accept that exact proof, never a guessed device-close flag.
	Observation observation;
	const Error observed = observe(observation);
	CRASH_COND_MSG(observed != OK ||
					(depth.is_valid() && observation.depth.state != NativeDisposalWitness::State::RETURNED) ||
					(framebuffer.is_valid() && observation.framebuffer.state != NativeDisposalWitness::State::RETURNED),
			"Scene height resources must remain retained until original RD physical return.");
}

} // namespace RendererRD
