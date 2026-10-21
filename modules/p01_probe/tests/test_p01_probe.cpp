#include "tests/test_macros.h"

#include "modules/p01_probe/p01_fixture.h"
#include "modules/p01_probe/p01_capture.h"
#include "modules/p01_probe/p01_kernels.h"
#include "modules/p01_probe/p01_transport.h"
#include <atomic>

TEST_FORCE_LINK(test_p01_probe_fixture)

namespace TestP01ProbeFixture {

TEST_CASE("[P01] Capture full trace permits completion but poisons omitted evidence") {
	P01Capture capture;
	capture.begin(1);
	P01CaptureEvent event{0, 1, 1, 1, 2, 3, 4, 5, 9, 0, 4, 0};
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
	P01CaptureEvent second = event;
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
	P01Capture capture;
	capture.begin(1);
	P01CaptureEvent event{0, 1, 1, 1, 2, 3, 4, 5, 9, 0, 4, 0};
	REQUIRE(capture.append(event));
	CHECK_FALSE(capture.append(event));
	CHECK(capture.is_poisoned());
	// Poison still permits the already admitted completion to drain.
	event.kind = 2;
	CHECK(capture.append(event));
	CHECK(capture.drain_complete());

	P01Capture reused;
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
	P01Capture capture;
	capture.begin(3);
	P01CaptureEvent first{0, 1, 1, 3, 2, 3, 4, 5, 9, 0, 4, 0};
	REQUIRE(capture.append(first));
	first.kind = 2;
	REQUIRE(capture.append(first));
	P01CaptureEvent next = first;
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
	P01Capture capture;
	capture.begin(4);
	P01CaptureEvent first{0, 1, 7, 4, 2, 3, 4, 5, 11, 0, 4, 0};
	P01CaptureEvent second = first;
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
	P01TransportQueue queue;
	P01CaptureEvent event{};
	event.token = 31;
	REQUIRE(queue.push(event));
	P01CaptureEvent output{};
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
	P01TransportQueue queue;
	for (uint64_t token = 1; token <= 128; token++) {
		P01CaptureEvent event{};
		event.token = token;
		REQUIRE(queue.push(event));
	}
	CHECK_FALSE(queue.push({}));
	CHECK(queue.is_poisoned());
	CHECK(queue.size() == 128);
	for (uint64_t token = 1; token <= 128; token++) {
		P01CaptureEvent event{};
		REQUIRE(queue.pop(event));
		CHECK(event.token == token);
	}
	CHECK(queue.size() == 0);
	CHECK_FALSE(queue.push({}));
}

TEST_CASE("[P01] Transport competing initial owners poison without mutation") {
	for (int round = 0; round < 200; round++) {
		P01TransportQueue queue;
		std::atomic<int> ready{0};
		std::atomic<bool> go{false};
		std::thread workers[2];
		for (auto &worker : workers) {
			worker = std::thread([&] {
				P01CaptureEvent event{};
				ready.fetch_add(1);
				while (!go.load()) { std::this_thread::yield(); }
				queue.pop(event);
			});
		}
		while (ready.load() != 2) { std::this_thread::yield(); }
		go.store(true);
		for (auto &worker : workers) { worker.join(); }
		CHECK(queue.is_poisoned());
		CHECK(queue.size() == 0);
	}
}

TEST_CASE("[P01] Transport rejects foreign writes and permits owner drain") {
	P01TransportQueue queue;
	P01CaptureEvent event{};
	event.token = 71;
	REQUIRE(queue.push(event));
	bool accepted = true;
	std::thread worker([&] { accepted = queue.push(event); });
	worker.join();
	CHECK_FALSE(accepted);
	CHECK(queue.is_poisoned());
	CHECK(queue.size() == 1);
	CHECK_FALSE(queue.push(event));
	P01CaptureEvent result{};
	REQUIRE(queue.pop(result));
	CHECK(result.token == 71);
	CHECK_FALSE(queue.pop(result));
}

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
