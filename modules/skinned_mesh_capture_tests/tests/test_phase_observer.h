/**************************************************************************/
/*  test_phase_observer.h                                                 */
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

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_phase_observer.h"

#include <thread>
namespace TestSkinnedMeshCaptureTestPhaseObserver {
TEST_CASE("[P01] Phase observation retains actual frame and requires ordered paired phases") {
	CHECK(skinned_mesh_capture_test_phase_begin(100));
	RenderCaptureTestHooks::observe_phase(500, false);
	RenderCaptureTestHooks::observe_phase(500, true);
	RenderCaptureTestHooks::observe_phase(503, false);
	RenderCaptureTestHooks::observe_phase(503, true);
	skinned_mesh_capture_test_phase_end();
	const auto events = skinned_mesh_capture_test_phase_events();
	CHECK(events.size() == 4);
	if (events.size() == 4) {
		CHECK(events[0].epoch == 100);
		CHECK(events[0].frame == 500);
		CHECK(events[2].frame == 503);
		CHECK(events[3].sequence == 4);
		CHECK(events[3].after_views);
	}
	CHECK_FALSE(skinned_mesh_capture_test_phase_poisoned());
	RenderCaptureTestHooks::observe_phase(504, false);
	CHECK(skinned_mesh_capture_test_phase_events().size() == 4);
	CHECK(skinned_mesh_capture_test_phase_begin(101));
	RenderCaptureTestHooks::observe_phase(600, true);
	CHECK(skinned_mesh_capture_test_phase_poisoned());
	skinned_mesh_capture_test_phase_end();
}
TEST_CASE("[P01] Phase coverage rejects overflow unfinished frames and foreign thread") {
	CHECK(skinned_mesh_capture_test_phase_begin(200));
	for (uint64_t frame = 1; frame <= 64; frame++) {
		RenderCaptureTestHooks::observe_phase(frame, false);
		RenderCaptureTestHooks::observe_phase(frame, true);
	}
	CHECK(skinned_mesh_capture_test_phase_events().size() == 128);
	CHECK_FALSE(skinned_mesh_capture_test_phase_poisoned());
	RenderCaptureTestHooks::observe_phase(65, false);
	CHECK(skinned_mesh_capture_test_phase_poisoned());
	skinned_mesh_capture_test_phase_end();
	CHECK(skinned_mesh_capture_test_phase_begin(201));
	RenderCaptureTestHooks::observe_phase(1, false);
	skinned_mesh_capture_test_phase_end();
	CHECK(skinned_mesh_capture_test_phase_poisoned());
	CHECK(skinned_mesh_capture_test_phase_begin(202));
	RenderCaptureTestHooks::observe_phase(1, false);
	std::thread other([] { RenderCaptureTestHooks::observe_phase(1, true); });
	other.join();
	CHECK(skinned_mesh_capture_test_phase_poisoned());
	skinned_mesh_capture_test_phase_end();
}

} //namespace TestSkinnedMeshCaptureTestPhaseObserver

TEST_CASE("[P01] Capture window is never retained after a phase hook returns") {
	uint64_t frame = 999;
	bool views = true;
	CHECK_FALSE(skinned_mesh_capture_test_phase_capture_window(frame, views));
	CHECK(frame == 999);
	CHECK(views);
}

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_phase_capture.h"
TEST_CASE("[P01] Scheduled capture executes only its exact native phase and rejects missed windows") {
	SkinnedMeshCaptureTestPhaseCaptureRequest request;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	request.epoch = 1000;
	request.request = 1;
	request.frame = 10;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	request.pool = std::make_shared<SkinnedMeshCaptureTestRDPool>();
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == OK);
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	CHECK(skinned_mesh_capture_test_phase_begin(1000));
	RenderCaptureTestHooks::observe_phase(10, false);
	auto result = skinned_mesh_capture_test_phase_capture_results();
	REQUIRE(result.size() == 1);
	CHECK(result[0].request == 1);
	CHECK(result[0].frame == 10);
	CHECK_FALSE(result[0].after_views);
	// Dummy renderer has no RD. A real phase does not fabricate native success.
	CHECK(result[0].error == ERR_UNAVAILABLE);
	request.request = 2;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == OK);
	RenderCaptureTestHooks::observe_phase(10, true);
	result = skinned_mesh_capture_test_phase_capture_results();
	REQUIRE(result.size() == 2);
	CHECK(result[1].error == ERR_INVALID_DATA);
	skinned_mesh_capture_test_phase_end();
	skinned_mesh_capture_test_phase_capture_cancel(1000);
	request.request = 3;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
}

TEST_CASE("[P01] Previous phase requests require an after-views draw selection") {
	SkinnedMeshCaptureTestPhaseCaptureRequest request;
	request.epoch = 1001;
	request.request = 1;
	request.frame = 10;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	request.pool = std::make_shared<SkinnedMeshCaptureTestRDPool>();
	request.previous = true;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	request.after_views = true;
	request.request++;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	request.selected_viewport = RID::from_uint64(4);
	request.request++;
	CHECK(skinned_mesh_capture_test_phase_capture_enqueue(request) == OK);
	skinned_mesh_capture_test_phase_capture_cancel(1001);
}
