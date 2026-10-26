/**************************************************************************/
/*  skinned_mesh_capture_test_native_source.h                             */
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
#include "skinned_mesh_capture_test_rd_pool.h"

#include <vector>

// Read-only original-owner metadata. No borrowed pointer or buffer RID leaves
// capture_current; the actual source is copied in the same list-closed RT call.
// Caller must separately prove t0 readback and the phase/generation window.
struct SkinnedMeshCaptureTestNativeSourceObservation {
	uint64_t frame = 0, draw_sequence = 0, viewport = 0;
	bool selected_previous = false;
	bool after_views = false;
	uint64_t scene = 0, mesh = 0, mesh_instance = 0, skeleton = 0;
	uint64_t device = 0, allocation = 0, buffer = 0;
	uint64_t dispatch_call_count = 0;
	uint64_t skeleton_version = 0, mesh_skeleton_version = 0, last_change = 0;
	uint32_t current = 0, previous = 0;
	bool queued = false;
};

struct SkinnedMeshCaptureTestDrawObservation {
	uint64_t epoch = 0, frame = 0, sequence = 0, mesh_instance = 0, vertex_array = 0;
	bool previous_bound = false;
	uint64_t viewport = 0;
	uint64_t device = 0, buffers[2] = {}, allocations[2] = {};
	uint32_t surface = 0, indices[2] = {}, renderer = 0, pass = 0;
};

class SkinnedMeshCaptureTestNativeSource {
	friend class SkinnedMeshCaptureTestDrawSelectionTestAccess;
	static Error select_matching_draw(const std::vector<SkinnedMeshCaptureTestDrawObservation> &p_draws, uint64_t p_mesh_instance, uint64_t p_epoch, uint64_t p_frame, uint64_t p_viewport, uint64_t p_sequence, uint64_t &r_sequence);
	static Error capture(RID p_scene, RID p_mesh, RID p_skeleton, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation, const SkinnedMeshCaptureTestDrawObservation *p_draw, bool p_previous);

public:
	static uint64_t draw_record_count();
	static Error select_draw(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_epoch, uint64_t p_frame, RID p_viewport, uint64_t p_sequence, uint64_t &r_sequence);
	static bool observe_draw(RID p_mesh_instance, uint32_t p_surface, RID p_vertex_array, uint64_t p_before_count, uint32_t p_renderer, uint32_t p_pass);
	static std::vector<SkinnedMeshCaptureTestDrawObservation> draw_observations();
	static Error capture_selected(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_draw_sequence, bool p_previous, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation);
	static Error capture_current(RID p_scene, RID p_mesh, RID p_skeleton, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation);
};

// Stack scope at the original viewport draw entry, including early returns.
