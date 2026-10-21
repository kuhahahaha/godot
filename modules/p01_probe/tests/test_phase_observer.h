#pragma once
#include "modules/p01_probe/p01_phase_observer.h"
#include "tests/test_macros.h"
#include <thread>
namespace TestP01PhaseObserver {
TEST_CASE("[P01] Phase observation retains actual frame and requires ordered paired phases") {
	CHECK(p01_phase_begin(100));
	p01_observe_phase(500, false);
	p01_observe_phase(500, true);
	p01_observe_phase(503, false);
	p01_observe_phase(503, true);
	p01_phase_end();
	const auto events = p01_phase_events();
	CHECK(events.size() == 4);
	if (events.size() == 4) {
		CHECK(events[0].epoch == 100);
		CHECK(events[0].frame == 500);
		CHECK(events[2].frame == 503);
		CHECK(events[3].sequence == 4);
		CHECK(events[3].after_views);
	}
	CHECK_FALSE(p01_phase_poisoned());
	p01_observe_phase(504, false);
	CHECK(p01_phase_events().size() == 4);
	CHECK(p01_phase_begin(101));
	p01_observe_phase(600, true);
	CHECK(p01_phase_poisoned());
	p01_phase_end();
}
TEST_CASE("[P01] Phase coverage rejects overflow unfinished frames and foreign thread") {
	CHECK(p01_phase_begin(200));
	for (uint64_t frame = 1; frame <= 64; frame++) {
		p01_observe_phase(frame, false);
		p01_observe_phase(frame, true);
	}
	CHECK(p01_phase_events().size() == 128);
	CHECK_FALSE(p01_phase_poisoned());
	p01_observe_phase(65, false);
	CHECK(p01_phase_poisoned());
	p01_phase_end();
	CHECK(p01_phase_begin(201));
	p01_observe_phase(1, false);
	p01_phase_end();
	CHECK(p01_phase_poisoned());
	CHECK(p01_phase_begin(202));
	p01_observe_phase(1, false);
	std::thread other([] { p01_observe_phase(1, true); });
	other.join();
	CHECK(p01_phase_poisoned());
	p01_phase_end();
}

}

TEST_CASE("[P01] Capture window is never retained after a phase hook returns") {
	uint64_t frame = 999;
	bool views = true;
	CHECK_FALSE(p01_phase_capture_window(frame, views));
	CHECK(frame == 999);
	CHECK(views);
}

#include "modules/p01_probe/p01_phase_capture.h"
TEST_CASE("[P01] Scheduled capture executes only its exact native phase and rejects missed windows") {
	P01PhaseCaptureRequest request;
	CHECK(p01_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	request.epoch = 1000; request.request = 1; request.frame = 10;
	request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
	request.pool = std::make_shared<P01RDPool>();
	CHECK(p01_phase_capture_enqueue(request) == OK);
	CHECK(p01_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
	CHECK(p01_phase_begin(1000));
	p01_observe_phase(10, false);
	auto result = p01_phase_capture_results();
	REQUIRE(result.size() == 1);
	CHECK(result[0].request == 1); CHECK(result[0].frame == 10);
	CHECK_FALSE(result[0].after_views);
	// Dummy renderer has no RD. A real phase does not fabricate native success.
	CHECK(result[0].error == ERR_UNAVAILABLE);
	request.request = 2;
	CHECK(p01_phase_capture_enqueue(request) == OK);
	p01_observe_phase(10, true);
	result = p01_phase_capture_results();
	REQUIRE(result.size() == 2);
	CHECK(result[1].error == ERR_INVALID_DATA);
	p01_phase_end();
	p01_phase_capture_cancel(1000);
	request.request = 3;
	CHECK(p01_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
}

TEST_CASE("[P01] Previous phase requests require an after-views draw selection") {
 P01PhaseCaptureRequest request;
 request.epoch = 1001; request.request = 1; request.frame = 10;
 request.scene = RID::from_uint64(1); request.mesh = RID::from_uint64(2); request.skeleton = RID::from_uint64(3);
 request.pool = std::make_shared<P01RDPool>(); request.previous = true;
 CHECK(p01_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
 request.after_views = true; request.request++;
 CHECK(p01_phase_capture_enqueue(request) == ERR_INVALID_PARAMETER);
 request.selected_viewport = RID::from_uint64(4); request.request++;
 CHECK(p01_phase_capture_enqueue(request) == OK);
 p01_phase_capture_cancel(1001);
}
