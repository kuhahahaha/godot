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
