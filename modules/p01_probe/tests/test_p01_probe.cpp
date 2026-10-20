#include "tests/test_macros.h"

#include "modules/p01_probe/p01_fixture.h"
#include "modules/p01_probe/p01_capture.h"
#include "modules/p01_probe/p01_kernels.h"

TEST_FORCE_LINK(test_p01_probe_fixture)

namespace TestP01ProbeFixture {

TEST_CASE("[P01] Fixture lifecycle") {
	P01Fixture fixture;
	CHECK(fixture.begin(1));
	CHECK(fixture.advance_action(42));
	const P01FixtureResult result = fixture.take_result();
	CHECK(result.epoch == 1);
	CHECK(result.action_count == 1);
	CHECK(result.last_value == 42);
	CHECK_FALSE(result.capture_poisoned);
	CHECK(fixture.close());
	CHECK(fixture.take_result().disposed);
	CHECK_FALSE(fixture.advance_action(7));
}

TEST_CASE("[P01] Fixture rejects duplicate begin") {
	P01Fixture fixture;
	CHECK(fixture.begin(1));
	CHECK_FALSE(fixture.begin(2));
	CHECK(fixture.advance_action(7));
	CHECK(fixture.close());
}

TEST_CASE("[P01] Capture requires physical disposal before seal") {
	P01Capture capture;
	capture.begin(7);
	P01CaptureEvent queued{0, 1, 1, 7, 2, 3, 5, 6, 9, 0, 4, 0};
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	CHECK_FALSE(capture.seal());
	CHECK(capture.dispose_physical());
	CHECK(capture.seal());
	CHECK(capture.is_sealed());
}

TEST_CASE("[P01] Capture rejects transport before seal and short writes") {
	P01Capture capture;
	capture.begin(8);
	P01CaptureEvent queued{0, 1, 1, 8, 2, 3, 5, 6, 9, 0, 4, 0};
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	uint8_t byte = 0;
	CHECK_FALSE(capture.write_transport(&byte, 1, 1));
	CHECK(capture.has_transport_failure());
}

TEST_CASE("[P01] Capture overflow poisons publication but permits pending drain") {
	P01Capture capture;
	capture.begin(9);
	for (uint64_t i = 1; i <= 64; i++) {
		CHECK(capture.append({0, 1, i, 9, 2, 3, 5, 6, i, 0, 4, 0}));
	}
	CHECK_FALSE(capture.append({0, 1, 65, 9, 2, 3, 5, 6, 65, 0, 4, 0}));
	CHECK(capture.is_poisoned());
	for (uint64_t i = 1; i <= 64; i++) {
		CHECK(capture.append({0, 2, i, 9, 2, 3, 5, 6, i, 0, 4, 0}));
	}
	CHECK(capture.drain_complete());
	CHECK(capture.dispose_physical());
	CHECK_FALSE(capture.seal());
}

TEST_CASE("[P01] Capture publishes only after disposal and seal") {
	P01Capture capture;
	capture.begin(10);
	P01CaptureEvent queued{0, 1, 1, 10, 2, 3, 5, 6, 1, 0, 4, 0};
	CHECK(capture.append(queued));
	queued.kind = 2;
	CHECK(capture.append(queued));
	uint8_t byte = 0;
	CHECK_FALSE(capture.write_transport(&byte, 1, 1));
	P01Capture valid;
	valid.begin(11);
	CHECK(valid.append({0, 1, 1, 11, 2, 3, 5, 6, 1, 0, 4, 0}));
	CHECK(valid.append({0, 2, 1, 11, 2, 3, 5, 6, 1, 0, 4, 42}));
	CHECK(valid.dispose_physical());
	CHECK(valid.seal());
	CHECK(valid.write_transport(&byte, 1, 1));
}

TEST_CASE("[P01] Kernel fixture is deterministic") {
	CHECK(p01_kernel_identity({42}) == 42);
}

TEST_CASE("[P01] Capture event inspection is a synchronized snapshot") {
	P01Capture capture;
	capture.begin(12);
	CHECK(capture.append({0, 1, 1, 12, 2, 3, 5, 6, 1, 0, 4, 0}));
	const std::vector<P01CaptureEvent> snapshot = capture.get_events();
	REQUIRE(snapshot.size() == 1);
	CHECK(snapshot[0].sequence == 1);
}

} // namespace TestP01ProbeFixture
