/**************************************************************************/
/*  skinned_mesh_capture_test_phase_capture.h                             */
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
#include "skinned_mesh_capture_test_native_source.h"

#include <memory>
#include <vector>

// Engine-private requests retain their pool through execution/cancellation.
// RIDs are re-resolved at execution; no source pointer is retained or exported.
struct SkinnedMeshCaptureTestPhaseCaptureRequest {
	uint64_t epoch = 0, request = 0, frame = 0;
	bool after_views = false;
	uint64_t draw_sequence = 0;
	RID selected_viewport;
	bool previous = false;
	RID scene, mesh, skeleton;
	uint32_t slot = 0;
	std::shared_ptr<SkinnedMeshCaptureTestRDPool> pool;
};
struct SkinnedMeshCaptureTestPhaseCaptureResult {
	uint64_t epoch = 0, request = 0, frame = 0;
	bool after_views = false;
	Error error = ERR_UNAVAILABLE;
	SkinnedMeshCaptureTestNativeSourceObservation source;
};
Error skinned_mesh_capture_test_phase_capture_enqueue(const SkinnedMeshCaptureTestPhaseCaptureRequest &p_request);
std::vector<SkinnedMeshCaptureTestPhaseCaptureResult> skinned_mesh_capture_test_phase_capture_results();
// Session cancellation drops only unrecorded jobs, never accepted GPU work.
void skinned_mesh_capture_test_phase_capture_cancel(uint64_t p_epoch);
void skinned_mesh_capture_test_phase_capture_service(uint64_t p_epoch, uint64_t p_frame, bool p_after_views);
