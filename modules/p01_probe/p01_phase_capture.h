#pragma once
#include "p01_native_source.h"
#include <memory>
#include <vector>

// Engine-private requests retain their pool through execution/cancellation.
// RIDs are re-resolved at execution; no source pointer is retained or exported.
struct P01PhaseCaptureRequest {
	uint64_t epoch = 0, request = 0, frame = 0;
	bool after_views = false;
	uint64_t draw_sequence = 0;
	RID selected_viewport;
	bool previous = false;
	RID scene, mesh, skeleton;
	uint32_t slot = 0;
	std::shared_ptr<P01RDPool> pool;
};
struct P01PhaseCaptureResult {
	uint64_t epoch = 0, request = 0, frame = 0;
	bool after_views = false;
	Error error = ERR_UNAVAILABLE;
	P01NativeSourceObservation source;
};
Error p01_phase_capture_enqueue(const P01PhaseCaptureRequest &p_request);
std::vector<P01PhaseCaptureResult> p01_phase_capture_results();
// Session cancellation drops only unrecorded jobs, never accepted GPU work.
void p01_phase_capture_cancel(uint64_t p_epoch);
void p01_phase_capture_service(uint64_t p_epoch, uint64_t p_frame, bool p_after_views);
