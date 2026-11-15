/**************************************************************************/
/*  height_frame_resources.h                                              */
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

#include "servers/rendering/native_depth_allocation_projection.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"

#ifdef TESTS_ENABLED
class TestNativeHeightRangeAccess;
#endif

namespace RendererRD {

// Scene height resource custody only. A retained Ref keeps this immutable pair
// alive; it grants neither a Scene sample nor producer/reader completion.
// The original native owner must keep the main RD alive through retirement.
class ParticlesStorage::HeightFrameResources : public RefCounted {
	friend class ParticlesStorage;
	friend class HeightFrame;
	friend class HeightCaptureCheckedSink;
	friend class ::GDExtensionDeviceExit::Registry;
#ifdef TESTS_ENABLED
	friend class ::ParticlesHeightfieldClearTestFixture;
	friend class ::TestNativeHeightRangeAccess;
#endif

	enum class State : uint8_t { ALLOCATED,
		RETIRING,
		RELEASED };
	enum class RetireTarget : uint8_t { FRAMEBUFFER,
		DEPTH };

	struct Observation {
		State state = State::ALLOCATED;
		Error allocation_error = OK;
		NativeDepthAllocationQuote::CreateOutcome creation_outcome = NativeDepthAllocationQuote::CreateOutcome::NotCreated;
		NativeDisposalWitness::Report depth;
		NativeDisposalWitness::Report framebuffer;
	};

	RenderingDevice &device;
	const uint32_t width;
	const uint32_t height;
	RID depth;
	RID framebuffer;
	NativeDisposalWitness depth_witness;
	NativeDisposalWitness framebuffer_witness;
	Error allocation_error = OK;
	NativeDepthAllocationQuote allocation_quote;
	NativeDepthAllocationQuote::CreateOutcome creation_outcome = NativeDepthAllocationQuote::CreateOutcome::NotCreated;
	NativeDepthAllocationProjection depth_projection;
	NativeDepthAllocationProjection pair_projection;
	bool depth_projection_captured = false;
	bool pair_projection_captured = false;
	State state = State::ALLOCATED;
	RetireTarget retire_target = RetireTarget::FRAMEBUFFER;

	HeightFrameResources(RenderingDevice &p_device, uint32_t p_width, uint32_t p_height);
	HeightFrameResources(const HeightFrameResources &) = delete;
	HeightFrameResources &operator=(const HeightFrameResources &) = delete;

	// Always transfers the new owner before issuing native allocation. On FB
	// failure r_owner retains the successfully created depth and its witness.
	static Error allocate(RenderingDevice &p_device, uint32_t p_width, uint32_t p_height, Ref<HeightFrameResources> &r_owner);

	static Error allocate_quoted(RenderingDevice &p_original_main_rd, const NativeDepthAllocationQuote &p_original_quote, Ref<HeightFrameResources> &r_original_owner, NativeDepthAllocationQuote::CreateOutcome &r_outcome);
	Error observe_allocation(NativeDepthAllocationProjection::Report &r_original_observation) const;

	// Called only by the original native owner after its original last-use
	// protocol permits retirement. Refcount is an additional retention gate,
	// never a substitute for native producer/reader completion.
	// One call performs at most one observation and one free request. ERR_BUSY
	// preserves this same owner/cursor; the original control pump resumes it.
	Error retire_step();
	// Two fixed observations; these are physical disposal facts, not Complete.
	Error observe(Observation &r_observation) const;

	bool is_quoted_verified() const;
	RID get_depth() const;
	RID get_framebuffer() const;
	uint32_t get_width() const { return width; }
	uint32_t get_height() const { return height; }

public:
	~HeightFrameResources();
};

} // namespace RendererRD
