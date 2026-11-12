/**************************************************************************/
/*  test_capture_observation.cpp                                          */
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
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/capture_lifecycle_model.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_capture.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_kernels.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_transport.h"

#include <atomic>

TEST_FORCE_LINK(test_skinned_mesh_capture_test_probe_fixture)

namespace TestSkinnedMeshCaptureTestProbeFixture {

TEST_CASE("[P01] Inactive observation leaves counters and events unchanged") {
	skinned_mesh_capture_test_hook_reset();
	RenderCaptureTestHooks::observe_buffer_write(1, 0, 4, true);
	RenderCaptureTestHooks::observe_buffer_write(1, 4, 4, false);
	RenderCaptureTestHooks::observe_readback_queued(1, 4);
	RenderCaptureTestHooks::observe_readback_completed(1, 4);
	RenderCaptureTestHooks::observe_buffer_allocation(1, 2, 3);
	RenderCaptureTestHooks::observe_buffer_free(1);
	RenderCaptureTestHooks::observe_buffer_clear(1, 0, 4);
	RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 4);
	const auto snapshot = skinned_mesh_capture_test_hook_snapshot();
	CHECK(snapshot.write_begin_count == 0);
	CHECK(snapshot.write_bytes == 0);
	CHECK(snapshot.write_failure_count == 0);
	CHECK(snapshot.readback_queued_count == 0);
	CHECK(snapshot.readback_completed_count == 0);
	CHECK(skinned_mesh_capture_test_hook_events().empty());
	CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
}

TEST_CASE("[P01] Typed observations preserve allocation and copy identity") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(42, 99);
	RenderCaptureTestHooks::observe_buffer_allocation(7, 123, 456, 77);
	RenderCaptureTestHooks::observe_buffer_copy(7, 8, 12, 20, 36, 123, 77, 88);
	const auto events = skinned_mesh_capture_test_hook_events();
	CHECK(events.size() == 2);
	if (events.size() == 2) {
		CHECK(events[0].epoch == 42);
		CHECK(events[0].device == 123);
		CHECK(events[0].driver == 456);
		CHECK(events[0].allocation_serial == 77);
		CHECK(events[0].session_serial_hint == 99);
		CHECK(events[1].source_rid == 7);
		CHECK(events[1].rid == 8);
		CHECK(events[1].source_offset == 12);
		CHECK(events[1].offset == 20);
		CHECK(events[1].size == 36);
		CHECK(events[1].device == 123);
		CHECK(events[1].source_allocation_serial == 77);
		CHECK(events[1].allocation_serial == 88);
	}
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Write clear logical free and physical dispose preserve actual identity") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(7, 999);
	RenderCaptureTestHooks::observe_buffer_write(1, 0, 36, true, 2, 3, 4);
	RenderCaptureTestHooks::observe_buffer_clear(1, 0, 36, 2, 3, 4);
	RenderCaptureTestHooks::observe_buffer_free(1, 2, 3, 4);
	RenderCaptureTestHooks::observe_buffer_dispose(2, 3, 4);
	const auto events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 4);
	for (const auto &event : events) {
		CHECK(event.device == 2);
		CHECK(event.driver == 3);
		CHECK(event.allocation_serial == 4);
		CHECK(event.session_serial_hint == 999);
	}
	CHECK(events[2].rid == 1);
	CHECK(events[3].rid == 0);
	CHECK(events[3].kind != events[2].kind);
	CHECK(events[3].sequence == events[2].sequence + 1);
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Failed update prefix is independent of absolute destination offset") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(1);
	RenderCaptureTestHooks::observe_buffer_write(1, 128, 64, false, 2, 3, 4, false);
	RenderCaptureTestHooks::observe_buffer_write(1, 192, 64, false, 2, 3, 4, true);
	RenderCaptureTestHooks::observe_buffer_write(1, 0, 64, true, 2, 3, 4, true);
	const auto events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 3);
	CHECK_FALSE(events[0].failed_prefix);
	CHECK(events[1].failed_prefix);
	CHECK_FALSE(events[2].failed_prefix);
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Skeleton producer events preserve setter upload and version order") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(3);
	RenderCaptureTestHooks::observe_skeleton_setter(10, 7);
	RenderCaptureTestHooks::observe_skeleton_upload(10, 20, 48, OK);
	RenderCaptureTestHooks::observe_skeleton_version(10, uint64_t(1) << 40);
	const auto events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 3);
	CHECK(events[0].kind == 10);
	CHECK(events[1].kind == 11);
	CHECK(events[2].kind == 12);
	CHECK(events[1].rid == 10);
	CHECK(events[1].source_rid == 20);
	CHECK(events[1].size == 48);
	CHECK(events[1].operation_error == OK);
	RenderCaptureTestHooks::observe_skeleton_upload(10, 20, 48, ERR_OUT_OF_MEMORY);
	const auto failure = skinned_mesh_capture_test_hook_events().back();
	CHECK_FALSE(failure.success);
	CHECK_FALSE(failure.failed_prefix);
	CHECK(failure.operation_error == ERR_OUT_OF_MEMORY);
	CHECK(events[2].content_version == (uint64_t(1) << 40));
	CHECK(events[2].sequence == 3);
	skinned_mesh_capture_test_hook_reset();
}
TEST_CASE("[P01] Capture full trace permits completion but poisons omitted evidence") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(1);
	SkinnedMeshCaptureTestCaptureEvent event{ 0, 1, 1, 1, 2, 3, 4, 5, 9, 0, 4, 0 };
	for (uint64_t token = 1; token <= 2047; token++) {
		event.token = token;
		event.queue_sequence = token + 8;
		event.kind = 1;
		REQUIRE(capture.append(event));
		event.kind = 2;
		REQUIRE(capture.append(event));
	}
	event.token = 2048;
	event.queue_sequence = 2056;
	event.kind = 1;
	REQUIRE(capture.append(event));
	SkinnedMeshCaptureTestCaptureEvent second = event;
	second.token++;
	second.queue_sequence++;
	REQUIRE(capture.append(second));
	REQUIRE(capture.get_events().size() == 4096);
	CHECK_FALSE(capture.is_poisoned());
	event.kind = 2;
	second.kind = 2;
	CHECK(capture.append(event));
	CHECK(capture.is_poisoned());
	CHECK_FALSE(capture.drain_complete());
	CHECK(capture.append(second));
	CHECK(capture.drain_complete());
	CHECK(capture.get_events().size() == 4096);
	CHECK(capture.dispose_physical());
	CHECK_FALSE(capture.seal());
}

TEST_CASE("[P01] Capture rejects duplicate and reused request identity") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(1);
	SkinnedMeshCaptureTestCaptureEvent event{ 0, 1, 1, 1, 2, 3, 4, 5, 9, 0, 4, 0 };
	REQUIRE(capture.append(event));
	CHECK_FALSE(capture.append(event));
	CHECK(capture.is_poisoned());
	// Poison still permits the already admitted completion to drain.
	event.kind = 2;
	CHECK(capture.append(event));
	CHECK(capture.drain_complete());

	SkinnedMeshCaptureTestCapture reused;
	reused.begin(2);
	event.epoch = 2;
	event.kind = 1;
	REQUIRE(reused.append(event));
	event.kind = 2;
	REQUIRE(reused.append(event));
	event.kind = 1;
	CHECK_FALSE(reused.append(event));
	CHECK(reused.is_poisoned());
	CHECK(reused.drain_complete());
	event.kind = 2;
	CHECK_FALSE(reused.append(event));
	CHECK(reused.drain_complete());
}

TEST_CASE("[P01] Stale completion cannot consume a distinct current tuple") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(3);
	SkinnedMeshCaptureTestCaptureEvent first{ 0, 1, 1, 3, 2, 3, 4, 5, 9, 0, 4, 0 };
	REQUIRE(capture.append(first));
	first.kind = 2;
	REQUIRE(capture.append(first));
	SkinnedMeshCaptureTestCaptureEvent next = first;
	next.kind = 1;
	next.token = 2;
	next.queue_sequence = 10;
	REQUIRE(capture.append(next));
	CHECK_FALSE(capture.append(first));
	CHECK_FALSE(capture.drain_complete());
	CHECK_FALSE(capture.dispose_physical());
	next.kind = 2;
	CHECK(capture.append(next));
	CHECK(capture.drain_complete());
	CHECK(capture.dispose_physical());
	CHECK_FALSE(capture.seal());
}

TEST_CASE("[P01] Token and queue sequence remain independent identities") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(4);
	SkinnedMeshCaptureTestCaptureEvent first{ 0, 1, 7, 4, 2, 3, 4, 5, 11, 0, 4, 0 };
	SkinnedMeshCaptureTestCaptureEvent second = first;
	second.queue_sequence = 12;
	REQUIRE(capture.append(first));
	REQUIRE(capture.append(second));
	CHECK_FALSE(capture.is_poisoned());
	first.kind = 2;
	second.kind = 2;
	CHECK(capture.append(first));
	CHECK(capture.append(second));
	CHECK(capture.drain_complete());
	CHECK(capture.dispose_physical());
	CHECK(capture.seal());
}

TEST_CASE("[P01] Transport foreign pop cannot consume owner data") {
	SkinnedMeshCaptureTestTransportQueue queue;
	SkinnedMeshCaptureTestCaptureEvent event{};
	event.token = 31;
	REQUIRE(queue.push(event));
	SkinnedMeshCaptureTestCaptureEvent output{};
	output.token = 99;
	bool accepted = true;
	std::thread worker([&] { accepted = queue.pop(output); });
	worker.join();
	CHECK_FALSE(accepted);
	CHECK(output.token == 99);
	CHECK(queue.size() == 1);
	CHECK(queue.is_poisoned());
	REQUIRE(queue.pop(output));
	CHECK(output.token == 31);
}

TEST_CASE("[P01] Transport capacity preserves FIFO during poisoned drain") {
	SkinnedMeshCaptureTestTransportQueue queue;
	for (uint64_t token = 1; token <= 128; token++) {
		SkinnedMeshCaptureTestCaptureEvent event{};
		event.token = token;
		REQUIRE(queue.push(event));
	}
	CHECK_FALSE(queue.push({}));
	CHECK(queue.is_poisoned());
	CHECK(queue.size() == 128);
	for (uint64_t token = 1; token <= 128; token++) {
		SkinnedMeshCaptureTestCaptureEvent event{};
		REQUIRE(queue.pop(event));
		CHECK(event.token == token);
	}
	CHECK(queue.size() == 0);
	CHECK_FALSE(queue.push({}));
}

TEST_CASE("[P01] Transport competing initial owners poison without mutation") {
	for (int round = 0; round < 200; round++) {
		SkinnedMeshCaptureTestTransportQueue queue;
		std::atomic<int> ready{ 0 };
		std::atomic<bool> go{ false };
		std::thread workers[2];
		for (auto &worker : workers) {
			worker = std::thread([&] {
				SkinnedMeshCaptureTestCaptureEvent event{};
				ready.fetch_add(1);
				while (!go.load()) {
					std::this_thread::yield();
				}
				queue.pop(event);
			});
		}
		while (ready.load() != 2) {
			std::this_thread::yield();
		}
		go.store(true);
		for (auto &worker : workers) {
			worker.join();
		}
		CHECK(queue.is_poisoned());
		CHECK(queue.size() == 0);
	}
}

TEST_CASE("[P01] Transport rejects foreign writes and permits owner drain") {
	SkinnedMeshCaptureTestTransportQueue queue;
	SkinnedMeshCaptureTestCaptureEvent event{};
	event.token = 71;
	REQUIRE(queue.push(event));
	bool accepted = true;
	std::thread worker([&] { accepted = queue.push(event); });
	worker.join();
	CHECK_FALSE(accepted);
	CHECK(queue.is_poisoned());
	CHECK(queue.size() == 1);
	CHECK_FALSE(queue.push(event));
	SkinnedMeshCaptureTestCaptureEvent result{};
	REQUIRE(queue.pop(result));
	CHECK(result.token == 71);
	CHECK_FALSE(queue.pop(result));
}

TEST_CASE("[P01] Fixture lifecycle") {
	CaptureLifecycleTestModel fixture;
	CHECK(fixture.begin(1));
	CHECK(fixture.advance_action(42));
	const CaptureLifecycleTestModelResult result = fixture.take_result();
	CHECK(result.epoch == 1);
	CHECK(result.action_count == 1);
	CHECK(result.last_value == 42);
	CHECK_FALSE(result.capture_poisoned);
	CHECK(fixture.close());
	CHECK(fixture.take_result().disposed);
	CHECK_FALSE(fixture.advance_action(7));
}

TEST_CASE("[P01] Fixture rejects duplicate begin") {
	CaptureLifecycleTestModel fixture;
	CHECK(fixture.begin(1));
	CHECK_FALSE(fixture.begin(2));
	CHECK(fixture.advance_action(7));
	CHECK(fixture.close());
}

TEST_CASE("[P01] Capture requires physical disposal before seal") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(7);
	SkinnedMeshCaptureTestCaptureEvent queued{ 0, 1, 1, 7, 2, 3, 5, 6, 9, 0, 4, 0 };
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	CHECK_FALSE(capture.seal());
	CHECK(capture.dispose_physical());
	CHECK(capture.seal());
	CHECK(capture.is_sealed());
}

TEST_CASE("[P01] Capture rejects transport before seal and short writes") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(8);
	SkinnedMeshCaptureTestCaptureEvent queued{ 0, 1, 1, 8, 2, 3, 5, 6, 9, 0, 4, 0 };
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	uint8_t byte = 0;
	CHECK_FALSE(capture.write_transport(&byte, 1, 1));
	CHECK(capture.has_transport_failure());
}

TEST_CASE("[P01] Capture overflow poisons publication but permits pending drain") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(9);
	for (uint64_t i = 1; i <= 64; i++) {
		CHECK(capture.append({ 0, 1, i, 9, 2, 3, 5, 6, i, 0, 4, 0 }));
	}
	CHECK_FALSE(capture.append({ 0, 1, 65, 9, 2, 3, 5, 6, 65, 0, 4, 0 }));
	CHECK(capture.is_poisoned());
	for (uint64_t i = 1; i <= 64; i++) {
		CHECK(capture.append({ 0, 2, i, 9, 2, 3, 5, 6, i, 0, 4, 0 }));
	}
	CHECK(capture.drain_complete());
	CHECK(capture.dispose_physical());
	CHECK_FALSE(capture.seal());
}

TEST_CASE("[P01] Capture publishes only after disposal and seal") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(10);
	SkinnedMeshCaptureTestCaptureEvent queued{ 0, 1, 1, 10, 2, 3, 5, 6, 1, 0, 4, 0 };
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	uint8_t byte = 0;
	CHECK_FALSE(capture.write_transport(&byte, 1, 1));
	SkinnedMeshCaptureTestCapture valid;
	valid.begin(11);
	CHECK(valid.append({ 0, 1, 1, 11, 2, 3, 5, 6, 1, 0, 4, 0 }));
	CHECK(valid.append({ 0, 2, 1, 11, 2, 3, 5, 6, 1, 0, 4, 42 }));
	CHECK(valid.dispose_physical());
	CHECK(valid.seal());
	CHECK(valid.write_transport(&byte, 1, 1));
}

TEST_CASE("[P01] Kernel fixture is deterministic") {
	CHECK(skinned_mesh_capture_test_kernel_identity({ 42 }) == 42);
}

TEST_CASE("[P01] Capture event inspection is a synchronized snapshot") {
	SkinnedMeshCaptureTestCapture capture;
	capture.begin(12);
	CHECK(capture.append({ 0, 1, 1, 12, 2, 3, 5, 6, 1, 0, 4, 0 }));
	const std::vector<SkinnedMeshCaptureTestCaptureEvent> snapshot = capture.get_events();
	REQUIRE(snapshot.size() == 1);
	CHECK(snapshot[0].sequence == 1);
}

} // namespace TestSkinnedMeshCaptureTestProbeFixture

TEST_CASE("[P01] Main skeleton signal brackets and palette request retain distinct owners") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(8);
	RenderCaptureTestHooks::observe_skeleton_signal(123, false);
	RenderCaptureTestHooks::observe_skeleton_signal(123, true);
	RenderCaptureTestHooks::observe_skeleton_palette_request(123, 456, 1);
	const auto events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 3);
	CHECK(events[0].kind == 13);
	CHECK(events[1].kind == 14);
	CHECK(events[2].kind == 15);
	CHECK(events[0].rid == 123);
	CHECK(events[1].rid == 123);
	CHECK(events[2].rid == 456);
	CHECK(events[2].source_rid == 123);
	CHECK(events[2].offset == 1);
	CHECK(events[0].sequence < events[1].sequence);
	CHECK(events[1].sequence < events[2].sequence);
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Whole allocation window detects alias writes and failed prefixes") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(77);
	RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 9, 10, 11);
	const auto anchor = skinned_mesh_capture_test_hook_events().back().sequence;
	CHECK(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, anchor, 77));
	RenderCaptureTestHooks::observe_buffer_write(200, 0, 4, false, 9, 3, 11, false);
	CHECK(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, anchor, 77));
	RenderCaptureTestHooks::observe_buffer_write(300, 0, 4, true, 9, 4, 12);
	CHECK(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, anchor, 77));
	RenderCaptureTestHooks::observe_buffer_write(999, 0, 4, false, 9, 3, 11, true);
	CHECK_FALSE(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, anchor, 77));
	CHECK_FALSE(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, 0, 77));
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(78);
	RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, 9, 10, 11);
	CHECK_FALSE(skinned_mesh_capture_test_hook_buffer_unchanged(9, 11, anchor, 77));
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Actual driver failure poisons evidence without erasing observations") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(91);
	RenderCaptureTestHooks::observe_driver_result(12, 1, 44, OK);
	CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
	RenderCaptureTestHooks::observe_driver_result(12, 2, 44, ERR_CANT_CREATE);
	CHECK(skinned_mesh_capture_test_hook_is_poisoned());
	const auto events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 2);
	CHECK(events[0].success);
	CHECK_FALSE(events[1].success);
	CHECK(events[1].operation_error == ERR_CANT_CREATE);
	CHECK(events[1].device == 12);
	skinned_mesh_capture_test_hook_reset();
}

TEST_CASE("[P01] Image dispatch witness rejects missing target and unrelated recording") {
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(990);
	SkinnedMeshCaptureTestRecordingScope scope;
	CHECK_FALSE(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	RenderCaptureTestHooks::observe_texture_write(1, 3, 1, RenderCaptureTestHooks::COMPUTE);
	CHECK_FALSE(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	RenderCaptureTestHooks::observe_texture_write(1, 2, 1, RenderCaptureTestHooks::DRAW);
	CHECK_FALSE(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	{
		SkinnedMeshCaptureTestRecordingScope other;
		RenderCaptureTestHooks::observe_texture_write(1, 2, 1, RenderCaptureTestHooks::COMPUTE);
	}
	CHECK_FALSE(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	RenderCaptureTestHooks::observe_texture_write(1, 2, 1, RenderCaptureTestHooks::COMPUTE);
	CHECK(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	RenderCaptureTestHooks::write_census_poison();
	CHECK_FALSE(skinned_mesh_capture_test_image_recorded(1, 2, scope.identity()));
	skinned_mesh_capture_test_hook_reset();
}
