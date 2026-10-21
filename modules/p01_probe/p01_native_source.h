#pragma once
#include "p01_rd_pool.h"

// Read-only original-owner metadata. No borrowed pointer or buffer RID leaves
// capture_current; the actual source is copied in the same list-closed RT call.
// Caller must separately prove t0 readback and the phase/generation window.
struct P01NativeSourceObservation {
	uint64_t scene = 0, mesh = 0, mesh_instance = 0, skeleton = 0;
	uint64_t device = 0, allocation = 0, buffer = 0;
	uint64_t dispatch_call_count = 0;
	uint64_t skeleton_version = 0, mesh_skeleton_version = 0, last_change = 0;
	uint32_t current = 0, previous = 0;
	bool queued = false;
};

class P01NativeSource {
public:
	static Error capture_current(RID p_scene, RID p_mesh, RID p_skeleton, P01RDPool &p_pool, uint32_t p_slot, P01NativeSourceObservation &r_observation);
};
