/**************************************************************************/
/*  skinned_mesh_capture_test_readback_collector.h                        */
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
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#include <array>
#include <memory>
#include <mutex>

// Immutable identity copied into each engine-owned callback. Physical identity
// must be resolved by the caller at actual enqueue, never inferred from a RID.
struct SkinnedMeshCaptureTestReadbackIdentity {
	uint64_t run = 0, epoch = 0, token = 0, queue = 0;
	uint64_t device = 0, allocation = 0, rid = 0, owner = 0, alias = 0;
	uint32_t slot = 0;
	bool consumer = false;
	bool operator==(const SkinnedMeshCaptureTestReadbackIdentity &p_other) const;
};

class SkinnedMeshCaptureTestReadbackCollector {
	struct State;
	class Callback;
	std::shared_ptr<State> state;
	Error copy_completed_locked(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, PackedByteArray &r_bytes) const;

public:
	SkinnedMeshCaptureTestReadbackCollector(uint64_t p_run, uint64_t p_epoch, bool p_dual_view);
	bool shares_state(const SkinnedMeshCaptureTestReadbackCollector &p_other) const { return state == p_other.state; }
	Error reserve(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, Callable &r_callback);
	// Call after the actual RD enqueue accepts the callback.
	Error mark_enqueued(const SkinnedMeshCaptureTestReadbackIdentity &p_identity);
	// Only after RD enqueue returns non-OK without retaining the callback.
	// Never cancel accepted work or use this for timeout cleanup.
	Error cancel_unqueued(const SkinnedMeshCaptureTestReadbackIdentity &p_identity);
	Error copy_completed(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, PackedByteArray &r_bytes) const;
	// Requires both original role requests; checks complete64B guards/transport.
	Error validate_pair(const SkinnedMeshCaptureTestReadbackIdentity &p_snapshot, const SkinnedMeshCaptureTestReadbackIdentity &p_consumer) const;
	void close();
	bool drained() const;
	bool poisoned() const;
};
