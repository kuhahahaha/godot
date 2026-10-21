#pragma once
#include "modules/p01_probe/p01_native_session.h"
#include "modules/p01_probe/p01_phase_observer.h"
#include "modules/p01_probe/p01_observer.h"
#include "tests/test_macros.h"
#include "servers/rendering/rendering_server.h"
#include <thread>

class P01NativeSessionTestAccess {
public:
	static Error prepare(P01NativeSession &p_session, const std::vector<P01SessionRequest> &p_requests, Callable &r_delivery) { return p_session.prepare_delivery(p_requests, r_delivery); }
	static void maintain(P01NativeSession &p_session) { p_session.maintain(); }
	static void phase(P01NativeSession &p_session, uint64_t p_frame, bool p_after_views) { p_session.phase(p_frame, p_after_views); }
	static void remove_terminal() {
		// Public pump remains owner-thread checked; dummy server runs on test main.
		P01NativeSession::service_retirement();
	}
};
TEST_CASE("[P01] Native session bounds immutable requests and closes without inventing GPU work") {
	p01_hook_reset();
	auto session = P01NativeSession::create(1, 9001, false);
	REQUIRE(session != nullptr);
	CHECK(P01NativeSession::create(1, 9002, false) == nullptr);
	P01SessionRequest request;
	CHECK(session->enqueue(request) == ERR_INVALID_PARAMETER);
	request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	for (uint64_t i = 1; i <= 8; i++) { request.request = i; CHECK(session->enqueue(request) == OK); }
	request.request = 9; CHECK(session->enqueue(request) == ERR_BUSY);
	session->close();
	CHECK(session->enqueue(request) == ERR_UNAVAILABLE);
	P01NativeSessionTestAccess::maintain(*session);
	const auto status = session->status();
	CHECK(status.results.size() == 8);
	for (const auto &result : status.results) { CHECK(result.canceled); CHECK(result.error == ERR_SKIP); CHECK_FALSE(result.recorded); }
	CHECK(status.closing); CHECK(status.no_native_work); CHECK_FALSE(status.pool_retired); CHECK_FALSE(status.initialized);
	P01NativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native session cleanup survives poisoned phase evidence and absent RD") {
	p01_hook_reset();
	auto session = P01NativeSession::create(2, 9002, false);
	REQUIRE(session != nullptr);
	P01SessionRequest request;
	request.request = 1; request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	CHECK(session->enqueue(request) == OK);
	P01NativeSessionTestAccess::phase(*session, 77, false);
	CHECK(session->status().error == ERR_UNAVAILABLE);
	p01_hook_begin(0); // Invalid epoch poisons evidence independently of phases.
	CHECK(p01_hook_is_poisoned());
	P01NativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	CHECK_FALSE(session->status().pool_retired);
	p01_hook_reset();
	P01NativeSessionTestAccess::remove_terminal();
}

TEST_CASE("[P01] Acquired session transport failure closes without blocking no-work cleanup") {
	auto session = P01NativeSession::create(3, 9003, false, true);
	REQUIRE(session != nullptr);
	P01NativeSessionTestAccess::maintain(*session);
	CHECK(session->status().error == ERR_UNAVAILABLE);
	CHECK(session->status().no_native_work);
	CHECK_FALSE(session->status().pool_retired);
	P01NativeSessionTestAccess::remove_terminal();
}

TEST_CASE("[P01] Native FIFO receive gates an atomic phase batch and holder retirement") {
	p01_hook_reset();
	auto session = P01NativeSession::create(4, 9004, false);
	REQUIRE(session != nullptr);
	P01SessionRequest early;
	early.request = 1; early.scene = RID::from_uint64(1); early.mesh = RID::from_uint64(2); early.skeleton = RID::from_uint64(3);
	auto late = early; late.request = 2; late.after_views = true;
	Callable delivery;
	CHECK(P01NativeSessionTestAccess::prepare(*session, { early, early }, delivery) == ERR_INVALID_PARAMETER);
	CHECK(delivery.is_null());
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, { early, late }, delivery) == OK);
	P01NativeSessionTestAccess::phase(*session, 88, false);
	CHECK(session->status().results.empty());
	CHECK_FALSE(session->status().initialized);
	CHECK(session->status().error == OK);
	delivery.call();
	P01NativeSessionTestAccess::phase(*session, 88, true);
	CHECK(session->status().results.empty());
	P01NativeSessionTestAccess::phase(*session, 89, false);
	auto state = session->status();
	REQUIRE(state.results.size() == 2);
	CHECK(state.results[0].frame == 89);
	CHECK(state.results[1].frame == 89);
	CHECK(state.results[0].publication == state.results[1].publication);
	CHECK(state.results[0].received);
	CHECK(state.results[1].received);
	CHECK(state.error == ERR_UNAVAILABLE); // Dummy RD, not a GPU claim.
	session->close();
	P01NativeSessionTestAccess::maintain(*session);
	CHECK_FALSE(session->status().no_native_work); // Callable still retained.
	delivery = Callable();
	P01NativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	P01NativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native FIFO holder survives fixture owner release and undelivered cancellation") {
	p01_hook_reset();
	auto session = P01NativeSession::create(5, 9005, false);
	REQUIRE(session != nullptr);
	std::weak_ptr<P01NativeSession> lifetime = session;
	P01SessionRequest request;
	request.request = 1; request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	session->close(); session.reset();
	CHECK_FALSE(lifetime.expired());
	P01NativeSessionTestAccess::remove_terminal();
	CHECK_FALSE(lifetime.expired());
	CHECK_FALSE(lifetime.lock()->status().no_native_work);
	delivery = Callable(); // Original holder released without executing.
	session = lifetime.lock();
	REQUIRE(session != nullptr);
	P01NativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	CHECK(state.no_native_work); CHECK_FALSE(state.pool_retired);
	CHECK(state.error == ERR_INVALID_DATA);
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].canceled); CHECK_FALSE(state.results[0].received);
	P01NativeSessionTestAccess::remove_terminal();
	session.reset();
	CHECK(lifetime.expired());
}
TEST_CASE("[P01] Late FIFO receive cannot revive canceled requests or bypass copied holders") {
	p01_hook_reset();
	auto session = P01NativeSession::create(6, 9006, false);
	REQUIRE(session != nullptr);
	P01SessionRequest request;
	request.request = 1; request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	Callable copied_holder = delivery;
	session->close();
	P01NativeSessionTestAccess::maintain(*session);
	delivery.call();
	P01NativeSessionTestAccess::phase(*session, 91, false);
	delivery = Callable();
	P01NativeSessionTestAccess::maintain(*session);
	auto state = session->status();
	CHECK_FALSE(state.no_native_work);
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].received); CHECK(state.results[0].canceled);
	CHECK_FALSE(state.results[0].recorded); CHECK(state.results[0].frame == 0);
	copied_holder = Callable();
	P01NativeSessionTestAccess::maintain(*session);
	CHECK(session->status().no_native_work);
	CHECK(session->status().error == OK);
	P01NativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Rejected FIFO batch consumes neither capacity nor publication identity") {
	p01_hook_reset();
	auto session = P01NativeSession::create(7, 9007, false);
	REQUIRE(session != nullptr);
	P01SessionRequest request;
	request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	std::vector<P01SessionRequest> batch;
	for (uint64_t i = 1; i <= 7; ++i) { request.request = i; batch.push_back(request); }
	Callable first, second;
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, batch, first) == OK);
	request.request = 8;
	auto overflow = request; overflow.request = 9;
	CHECK(P01NativeSessionTestAccess::prepare(*session, { request, overflow }, second) == ERR_BUSY);
	CHECK(second.is_null());
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, { request }, second) == OK);
	first.call(); second.call();
	first = Callable(); second = Callable();
	session->close();
	P01NativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	REQUIRE(state.results.size() == 8);
	CHECK(state.results.front().publication == 1);
	CHECK(state.results.back().publication == 2);
	CHECK(state.results.back().request == 8);
	CHECK(state.no_native_work);
	P01NativeSessionTestAccess::remove_terminal();
}
TEST_CASE("[P01] Native delivery traverses the original RS cross-thread command queue") {
	p01_hook_reset();
	auto *server = RenderingServer::get_singleton();
	REQUIRE(server != nullptr);
	REQUIRE(server->is_on_render_thread()); // Headless test main owns dummy RT.
	auto session = P01NativeSession::create(8, 9008, false);
	REQUIRE(session != nullptr);
	P01SessionRequest request;
	request.request = 1; request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	Callable delivery;
	REQUIRE(P01NativeSessionTestAccess::prepare(*session, { request }, delivery) == OK);
	// Exercise the actual non-owner queue path without replacing the live
	// server or claiming a separate-render-thread/GPU qualification.
	std::thread publisher([&]() { server->call_on_render_thread(delivery); });
	publisher.join();
	delivery = Callable();
	P01NativeSessionTestAccess::phase(*session, 92, false);
	CHECK(session->status().results.empty());
	CHECK(session->status().error == OK);
	server->sync(); // Test-only normal CPU queue flush; production never syncs.
	session->close();
	P01NativeSessionTestAccess::maintain(*session);
	const auto state = session->status();
	REQUIRE(state.results.size() == 1);
	CHECK(state.results[0].received); CHECK(state.results[0].canceled);
	CHECK(state.no_native_work); CHECK(state.error == OK);
	P01NativeSessionTestAccess::remove_terminal();
}
