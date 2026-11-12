/**************************************************************************/
/*  test_draw_selection.h                                                 */
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

#include "servers/rendering/render_capture_test_hooks.h"
#pragma once
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_native_source.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"
TEST_CASE("[P01] Draw observation rejects missing RD and never fabricates a selected role") {
	CHECK(SkinnedMeshCaptureTestNativeSource::draw_record_count() == 0);
	CHECK_FALSE(SkinnedMeshCaptureTestNativeSource::observe_draw(RID(), 0, RID(), 0, 0, 0));
	CHECK(SkinnedMeshCaptureTestNativeSource::draw_observations().empty());
	SkinnedMeshCaptureTestRDPool pool;
	SkinnedMeshCaptureTestNativeSourceObservation observation;
	CHECK(SkinnedMeshCaptureTestNativeSource::capture_selected(RID(), RID(), RID(), 1, true, pool, 0, observation) == ERR_INVALID_DATA);
}

class SkinnedMeshCaptureTestDrawSelectionTestAccess {
public:
	static void check_selection() {
		SkinnedMeshCaptureTestDrawObservation target;
		target.epoch = 1;
		target.frame = 2;
		target.viewport = 3;
		target.mesh_instance = 4;
		target.sequence = 5;
		auto neighbor = target;
		neighbor.mesh_instance = 6;
		neighbor.sequence = 7;
		uint64_t selected = 0;
		CHECK(SkinnedMeshCaptureTestNativeSource::select_matching_draw({ target, neighbor }, 4, 1, 2, 3, 0, selected) == OK);
		CHECK(selected == 5);
		auto second = target;
		second.sequence = 8;
		CHECK(SkinnedMeshCaptureTestNativeSource::select_matching_draw({ target, neighbor, second }, 4, 1, 2, 3, 0, selected) == ERR_INVALID_DATA);
		CHECK(SkinnedMeshCaptureTestNativeSource::select_matching_draw({ target, neighbor, second }, 4, 1, 2, 3, 8, selected) == OK);
		CHECK(selected == 8);
		CHECK(SkinnedMeshCaptureTestNativeSource::select_matching_draw({ neighbor }, 4, 1, 2, 3, 0, selected) == ERR_INVALID_DATA);
		CHECK(SkinnedMeshCaptureTestNativeSource::select_matching_draw({ target }, 4, 1, 2, 99, 0, selected) == ERR_INVALID_DATA);
	}
};
TEST_CASE("[P01] Viewport selection ignores unrelated meshes but rejects ambiguous target draws") {
	SkinnedMeshCaptureTestDrawSelectionTestAccess::check_selection();
}

#include <thread>
TEST_CASE("[P01] Exact recording scopes exclude foreign and nested publishers") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(6000);
	uint64_t first = 0;
	{
		SkinnedMeshCaptureTestRecordingScope operation;
		first = operation.identity();
		REQUIRE(first != 0);
		std::thread other([] { RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5); });
		other.join();
		CHECK(skinned_mesh_capture_test_hook_events().back().recording_token == 0);
		{
			SkinnedMeshCaptureTestRecordingScope nested;
			RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
			CHECK(skinned_mesh_capture_test_hook_events().back().recording_token == nested.identity());
			CHECK(nested.identity() != first);
		}
		const auto marker = skinned_mesh_capture_test_observe_draw_record(3, 4, 5);
		RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
		CHECK(marker.kind == 16);
		CHECK(marker.sequence == 3);
		CHECK(marker.epoch == 6000);
		CHECK(skinned_mesh_capture_test_hook_events().back().recording_token == first);
		CHECK(marker.sequence != skinned_mesh_capture_test_hook_events().back().sequence);
	}
	RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
	CHECK(skinned_mesh_capture_test_hook_events().back().recording_token == 0);
	skinned_mesh_capture_test_hook_reset();
}
