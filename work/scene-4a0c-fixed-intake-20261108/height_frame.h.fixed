/**************************************************************************/
/*  height_frame.h                                             */
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

#include "core/templates/paged_array.h"
#include "servers/rendering/renderer_rd/height_capture_checked_sink.h"
#include "servers/rendering/renderer_rd/storage_rd/height_frame_resources.h"

class RendererSceneRenderRD;
class RenderGeometryInstance;

namespace RendererRD {

// Private Scene candidate. CaptureRecorded means checked native recording only;
// original K01 attachment/dependency is still required for RecordedOrdered and
// publication. This component exposes no Current, sample, or completion grant.
class ParticlesStorage::HeightFrame : public RefCounted {
	friend class ParticlesStorage;
#ifdef TESTS_ENABLED
	friend class ::ParticlesHeightfieldClearTestFixture;
#endif

	enum class State : uint8_t { Allocated,
		Recording,
		CaptureRecorded,
		FailedRetiring };

	struct Snapshot {
		RID instance;
		RID base;
		uint64_t source_generation = 0;
		uint64_t shape_revision = 0;
		uint64_t producer_occurrence = 0;
		RSE::ParticlesCollisionType shape = RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE;
		Transform3D transform;
		Vector3 extents;
		AABB bounds;
		uint32_t cull_mask = 0;
		uint32_t heightfield_mask = 0;
		RSE::ParticlesCollisionHeightfieldResolution resolution = RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024;
		uint32_t width = 0;
		uint32_t height = 0;
		// Exact original allocation identities, not a minted logical version.
		NativeDisposalWitness::Report depth_allocation;
		NativeDisposalWitness::Report framebuffer_allocation;
	};

	const Snapshot snapshot;
	// The original native owner MUST retain its own resource Ref and its other
	// geometry/material leases through actual last-use/retirement. Dropping a
	// frame only releases this custody reference; it never requests disposal.
	Ref<HeightFrameResources> resources;
	HeightCaptureCheckedSink prefix;
	State state = State::Allocated;
	Error capture_error = OK;

	HeightFrame(const Snapshot &p_snapshot, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity);
	HeightFrame(const HeightFrame &) = delete;
	HeightFrame &operator=(const HeightFrame &) = delete;

	// Source fields are frozen from this original storage's live instance/base.
	// The occurrence is supplied by its original owner; this class issues none.
	// No published content ordinal exists before original attachment succeeds.
	static Error create_candidate(ParticlesStorage &p_storage, RID p_instance, uint64_t p_original_occurrence, const Ref<HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity, Ref<HeightFrame> &r_frame);
	// The original checked renderer owns begin(clear0), binds/draws and checked
	// end/unwind. The complete original sink storage remains owned by the frame.
	Error capture(RendererSceneRenderRD &p_renderer, const PagedArray<RenderGeometryInstance *> &p_geometry);

	const Snapshot &get_snapshot() const { return snapshot; }
	State get_state() const { return state; }
	Error get_capture_error() const { return capture_error; }
	const HeightCaptureCheckedSink &get_capture_prefix() const { return prefix; }

public:
	~HeightFrame();
};

} // namespace RendererRD
