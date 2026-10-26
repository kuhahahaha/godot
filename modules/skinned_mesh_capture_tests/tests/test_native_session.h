/**************************************************************************/
/*  test_native_session.h                                                 */
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
#include "servers/rendering/rendering_server.h"
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_native_session.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_phase_observer.h"

#include <thread>

class SkinnedMeshCaptureTestNativeSessionTestAccess {
public:
	static Error prepare(SkinnedMeshCaptureTestNativeSession &p_session, const std::vector<SkinnedMeshCaptureTestSessionRequest> &p_requests, Callable &r_delivery) { return p_session.prepare_delivery(p_requests, r_delivery); }
	static void maintain(SkinnedMeshCaptureTestNativeSession &p_session) { p_session.maintain(); }
	static void phase(SkinnedMeshCaptureTestNativeSession &p_session, uint64_t p_frame, bool p_after_views) { p_session.phase(p_frame, p_after_views); }
	static Error prepare_completed_image(SkinnedMeshCaptureTestNativeSession &p_session, Callable &r_callback) {
		const Error error = p_session.images.reserve(0, r_callback);
		if (error != OK) {
			return error;
		}
		SkinnedMeshCaptureTestNativeSession::Job job;
		job.started = true;
		job.enqueued = 2;
		job.result.request = 1;
		job.result.completed = true;
		p_session.jobs.push_back(job);
		// Exercise the completed-job skip without allocating an RD pool.
		p_session.initialized = true;
		return OK;
	}
	static void remove_terminal() {
		// Public pump remains owner-thread checked; dummy server runs on test main.
		SkinnedMeshCaptureTestNativeSession::service_retirement();
	}
};
TEST_CASE("[P01] Native session bounds immutable requests and closes without inventing GPU work") {
	bool dual_view = false;
	SUBCASE("single-view capacity") {}
	SUBCASE("dual-view capacity") {
		dual_view = true;
	}
	const uint64_t capacity = dual_view ? 18 : 8;
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(1, 9001, dual_view);
	REQUIRE(session != nullptr);
	CHECK(SkinnedMeshCaptureTestNativeSession::create(1, 9002, false) == nullptr);
	SkinnedMeshCaptureTestSessionRequest request;
	CHECK(session->enqueue(request) == ERR_INVALID_PARAMETER);
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	for (uint64_t i = 1; i <= capacity; i++) {
		request.request = i;
		CHECK(session->enqueue(request) == OK);
	}
	request.request = capacity + 1;
	CHECK(session->enqueue(request) == ERR_BUSY);
	session->close();
	CHECK(session->enqueue(request) == ERR_UNAVAILABLE);
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	const auto status = session->status();
	CHECK(status.results.size() == capacity);
	for (const auto &result : status.results) {
		CHECK(result.canceled);
		CHECK(result.error == ERR_SKIP);
		CHECK_FALSE(result.recorded);
	}
	CHECK(status.closing);
	CHECK(status.no_native_work);
	CHECK_FALSE(status.pool_retired);
	CHECK_FALSE(status.initialized);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native session cleanup survives poisoned phase evidence and absent RD") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(2, 9002, false);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestSessionRequest request;
	request.request = 1;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	CHECK(session->enqueue(request) == OK);
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 77, false);
	CHECK(session->status().error == ERR_UNAVAILABLE);
	skinned_mesh_capture_test_hook_begin(0); // Invalid epoch poisons evidence independently of phases.
	CHECK(skinned_mesh_capture_test_hook_is_poisoned());
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	CHECK_FALSE(session->status().pool_retired);
	skinned_mesh_capture_test_hook_reset();
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}

TEST_CASE("[P01] Acquired session transport failure closes without blocking no-work cleanup") {
	auto session = SkinnedMeshCaptureTestNativeSession::create(3, 9003, false, true);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().error == ERR_UNAVAILABLE);
	CHECK(session->status().no_native_work);
	CHECK_FALSE(session->status().pool_retired);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}

TEST_CASE("[P01] Native FIFO receive gates an atomic phase batch and holder retirement") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(4, 9004, false);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestSessionRequest early;
	early.request = 1;
	early.scene = RID::from_uint64(1);
	early.mesh = RID::from_uint64(2);
	early.skeleton = RID::from_uint64(3);
	auto late = early;
	late.request = 2;
	late.after_views = true;
	Callable delivery;
	CHECK(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { early, early }, delivery) == ERR_INVALID_PARAMETER);
	CHECK(delivery.is_null());
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { early, late }, delivery) == OK);
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 88, false);
	CHECK(session->status().results.empty());
	CHECK_FALSE(session->status().initialized);
	CHECK(session->status().error == OK);
	delivery.call();
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 88, true);
	CHECK(session->status().results.empty());
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 89, false);
	auto state = session->status();
	REQUIRE(state.results.size() == 2);
	CHECK(state.results[0].frame == 89);
	CHECK(state.results[1].frame == 89);
	CHECK(state.results[0].publication == state.results[1].publication);
	CHECK(state.results[0].received);
	CHECK(state.results[1].received);
	CHECK(state.error == ERR_UNAVAILABLE); // Dummy RD, not a GPU claim.
	session->close();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK_FALSE(session->status().no_native_work); // Callable still retained.
	delivery = Callable();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native FIFO holder survives fixture owner release and undelivered cancellation") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(5, 9005, false);
	REQUIRE(session != nullptr);
	std::weak_ptr<SkinnedMeshCaptureTestNativeSession> lifetime = session;
	SkinnedMeshCaptureTestSessionRequest request;
	request.request = 1;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	session->close();
	session.reset();
	CHECK_FALSE(lifetime.expired());
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
	CHECK_FALSE(lifetime.expired());
	CHECK_FALSE(lifetime.lock()->status().no_native_work);
	delivery = Callable(); // Original holder released without executing.
	session = lifetime.lock();
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	CHECK(state.no_native_work);
	CHECK_FALSE(state.pool_retired);
	CHECK(state.error == ERR_INVALID_DATA);
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].canceled);
	CHECK_FALSE(state.results[0].received);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
	session.reset();
	CHECK(lifetime.expired());
}
TEST_CASE("[P01] Late FIFO receive cannot revive canceled requests or bypass copied holders") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(6, 9006, false);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestSessionRequest request;
	request.request = 1;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	Callable copied_holder = delivery;
	session->close();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	delivery.call();
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 91, false);
	delivery = Callable();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	auto state = session->status();
	CHECK_FALSE(state.no_native_work);
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].received);
	CHECK(state.results[0].canceled);
	CHECK_FALSE(state.results[0].recorded);
	CHECK(state.results[0].frame == 0);
	copied_holder = Callable();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	CHECK(session->status().error == OK);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Rejected FIFO batch consumes neither capacity nor publication identity") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(7, 9007, false);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestSessionRequest request;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	std::vector<SkinnedMeshCaptureTestSessionRequest> batch;
	for (uint64_t i = 1; i <= 7; ++i) {
		request.request = i;
		batch.push_back(request);
	}
	Callable first, second;
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, batch, first) == OK);
	request.request = 8;
	auto overflow = request;
	overflow.request = 9;
	CHECK(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { request, overflow }, second) == ERR_BUSY);
	CHECK(second.is_null());
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { request }, second) == OK);
	first.call();
	second.call();
	first = Callable();
	second = Callable();
	session->close();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	REQUIRE(state.results.size() == 8);
	CHECK(state.results.front().publication == 1);
	CHECK(state.results.back().publication == 2);
	CHECK(state.results.back().request == 8);
	CHECK(state.no_native_work);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native delivery traverses the original RS cross-thread command queue") {
	skinned_mesh_capture_test_hook_reset();
	auto *server = RenderingServer::get_singleton();
	REQUIRE(server != nullptr);
	REQUIRE(server->is_on_render_thread()); // Headless test main owns dummy RT.
	auto session = SkinnedMeshCaptureTestNativeSession::create(8, 9008, false);
	REQUIRE(session != nullptr);
	SkinnedMeshCaptureTestSessionRequest request;
	request.request = 1;
	request.scene = RID::from_uint64(1);
	request.mesh = RID::from_uint64(2);
	request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	// Exercise the actual non-owner queue path without replacing the live
	// server or claiming a separate-render-thread/GPU qualification.
	std::thread publisher([&]() { server->call_on_render_thread(delivery); });
	publisher.join();
	delivery = Callable();
	SkinnedMeshCaptureTestNativeSessionTestAccess::phase(*session, 92, false);
	CHECK(session->status().results.empty());
	CHECK(session->status().error == OK);
	server->sync(); // Test-only normal CPU queue flush; production never syncs.
	session->close();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].received);
	CHECK(state.results[0].canceled);
	CHECK(state.no_native_work);
	CHECK(state.error == OK);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}

TEST_CASE("[P01] Native session detects late duplicate image after completed job") {
	skinned_mesh_capture_test_hook_reset();
	auto session = SkinnedMeshCaptureTestNativeSession::create(30, 9030, false);
	REQUIRE(session != nullptr);
	Callable callback;
	REQUIRE(SkinnedMeshCaptureTestNativeSessionTestAccess::prepare_completed_image(*session, callback) == OK);
	PackedByteArray pixels;
	pixels.resize(128 * 64 * 4);
	pixels.fill(37);
	callback.call(pixels);
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().error == OK);
	callback.call(pixels);
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().error == ERR_INVALID_DATA);
	CHECK(session->status().closing);
	CHECK_FALSE(session->status().no_native_work);
	callback = Callable();
	SkinnedMeshCaptureTestNativeSessionTestAccess::maintain(*session);
	CHECK(session->status().error == ERR_INVALID_DATA);
	CHECK(session->status().no_native_work);
	CHECK_FALSE(session->status().pool_retired);
	SkinnedMeshCaptureTestNativeSessionTestAccess::remove_terminal();
}
