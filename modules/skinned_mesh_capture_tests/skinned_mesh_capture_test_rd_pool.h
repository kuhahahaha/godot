/**************************************************************************/
/*  skinned_mesh_capture_test_rd_pool.h                                   */
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

#include "skinned_mesh_capture_test_public_snapshot.h"
#include "skinned_mesh_capture_test_readback_collector.h"

#include "servers/rendering/rendering_device.h"

// Private RT-owned resources. Releasing these RIDs is logical release only;
// the collector/observer must separately establish physical retirement.
class SkinnedMeshCaptureTestRDPool {
	friend class SkinnedMeshCaptureTestRDPoolTestAccess;
	RenderingDevice *rd = nullptr;
	ObjectID device_identity;
	RID shaders[2];
	RID pipelines[2];
	struct Slot {
		RID snapshot;
		RID consumer;
		RID image;
		RID upload; // At most one immutable 228B upload and one set per slot.
		RID deformation_set;
		RID transport_set;
		uint64_t capture_sequence = 0, transport_sequence = 0, observation_epoch = 0;
		bool consumed = false;
		bool captured = false;
		bool transported = false;
		bool readback_attempted[2] = {};
	};
	Slot slots[18];
	uint32_t capacity = 0;
	struct Retirement {
		uint64_t driver = 0, serial = 0;
	};
	Retirement retirements[54];
	uint32_t retirement_count = 0;
	ObjectID retirement_device;
	bool release_recorded = false;
	bool retirement_invalid = false;
	bool retirement_confirmed = false;
	uint64_t resource_tokens[58] = {};
	uint32_t resource_count = 0;
	bool track_resource(RID p_rid, RenderCaptureTestResourceKind p_kind);
	std::unique_ptr<SkinnedMeshCaptureTestReadbackCollector> queued_collector;
	Error qualify_buffer(RID p_buffer) const;
	Error qualify_collector(uint32_t p_slot, const SkinnedMeshCaptureTestReadbackCollector &p_collector) const;
	Error resolve_readback_identity(uint32_t p_slot, bool p_consumer, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, SkinnedMeshCaptureTestReadbackIdentity &r_identity) const;
	Error record_boundary(RID p_buffer, uint64_t p_operation, uint64_t &r_sequence, uint64_t &r_epoch) const;
	Error qualify_readback(const SkinnedMeshCaptureTestReadbackIdentity &p_identity) const;
	bool on_owner_thread() const;
	bool has_active_list() const;
	RID create_set(RID p_shader, RID p_source, RID p_destination);

public:
	SkinnedMeshCaptureTestRDPool() = default;
	SkinnedMeshCaptureTestRDPool(const SkinnedMeshCaptureTestRDPool &) = delete;
	SkinnedMeshCaptureTestRDPool &operator=(const SkinnedMeshCaptureTestRDPool &) = delete;
	Error initialize(bool p_dual_view);
	Error record_public(uint32_t p_slot, const SkinnedMeshCaptureTestPublicSnapshot &p_snapshot);
	// Caller establishes the actual source owner/generation and 36B borrow window.
	Error record_native(uint32_t p_slot, RID p_source, uint32_t p_offset);
	Error record_transport(uint32_t p_slot);
	Error record_image(uint32_t p_slot, RID p_source, RID &r_image);
	// Identity must come from the actual allocation registry at this RT call.
	Error enqueue_readback(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, SkinnedMeshCaptureTestReadbackCollector &p_collector);
	Error enqueue_pair(uint32_t p_slot, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, SkinnedMeshCaptureTestReadbackCollector &p_collector, SkinnedMeshCaptureTestReadbackIdentity (&r_identities)[2], uint32_t &r_enqueued);
	Error validate_pair(const SkinnedMeshCaptureTestReadbackIdentity &p_snapshot, const SkinnedMeshCaptureTestReadbackIdentity &p_consumer, const SkinnedMeshCaptureTestReadbackCollector &p_collector) const;
	Error release();
	// Only this pool's buffers, sets, shaders and pipelines; never K03/device retirement.
	Error physical_retirement_status();
	bool retirement_required() const { return rd || release_recorded; }
};
