#pragma once
#include "modules/p01_probe/p01_readback_collector.h"
#include "modules/p01_probe/p01_gpu_payload.h"
#include "tests/test_macros.h"

namespace TestP01ReadbackCollector {
static P01ReadbackIdentity identity(uint32_t p_slot = 0, bool p_consumer = false) {
	return { 1, 2, 3 + p_slot * 2 + uint64_t(p_consumer), 40 + p_slot * 2 + uint64_t(p_consumer), 5, 6, 7, 8, 9, p_slot, p_consumer };
}
TEST_CASE("[P01] Collector preserves exact identity copied bytes and late close completion") {
	P01ReadbackCollector collector(1, 2, false);
	Callable callback;
	const auto id = identity();
	REQUIRE(collector.reserve(id, callback) == OK);
	CHECK(collector.mark_enqueued(id) == OK);
	CHECK(collector.cancel_unqueued(id) == ERR_INVALID_PARAMETER);
	Callable duplicate;
	CHECK(collector.reserve(id, duplicate) != OK);
	PackedByteArray bytes = p01_initial_guarded_buffer();
	PackedByteArray output;
	CHECK(collector.copy_completed(id, output) == ERR_BUSY);
	collector.close();
	CHECK_FALSE(collector.drained());
	callback.call(bytes);
	CHECK(collector.drained());
	CHECK(collector.cancel_unqueued(id) == ERR_INVALID_PARAMETER);
	REQUIRE(collector.copy_completed(id, output) == OK);
	bytes.set(0, 0);
	if (output.size() == 64) { CHECK(output[0] == 0x21); } else { CHECK(output.size() == 64); }
	auto stale = id;
	stale.alias++;
	CHECK(collector.copy_completed(stale, output) == ERR_INVALID_PARAMETER);
	callback.call(bytes);
	CHECK(collector.poisoned());
}
TEST_CASE("[P01] Collector callback survives owner and bounded pools reject overflow") {
	Callable surviving;
	{
		P01ReadbackCollector collector(1, 2, false);
		REQUIRE(collector.reserve(identity(), surviving) == OK);
		collector.close();
	}
	surviving.call(p01_initial_guarded_buffer());
	for (bool dual : { false, true }) {
		P01ReadbackCollector collector(1, 2, dual);
		const uint32_t capacity = dual ? 18 : 8;
		for (uint32_t slot = 0; slot < capacity; slot++) {
			for (bool consumer : { false, true }) {
				Callable callback;
				REQUIRE(collector.reserve(identity(slot, consumer), callback) == OK);
				callback.call(p01_initial_guarded_buffer());
			}
		}
		Callable overflow;
		CHECK(collector.reserve(identity(capacity), overflow) == ERR_INVALID_PARAMETER);
		collector.close();
		CHECK(collector.drained());
	}
}
TEST_CASE("[P01] Collector rejects malformed requests and malformed completed bytes") {
	P01ReadbackCollector collector(1, 2, false);
	Callable callback;
	auto bad = identity();
	bad.run++;
	CHECK(collector.reserve(bad, callback) == ERR_INVALID_PARAMETER);
	bad = identity();
	bad.epoch++;
	CHECK(collector.reserve(bad, callback) == ERR_INVALID_PARAMETER);
	bad = identity();
	bad.device = 0;
	CHECK(collector.reserve(bad, callback) == ERR_INVALID_PARAMETER);
	CHECK(collector.reserve(identity(), callback) == OK);
	bad = identity(1);
	bad.queue = identity().queue;
	Callable other;
	CHECK(collector.reserve(bad, other) == OK);
	auto same_token = identity(2);
	same_token.token = identity().token;
	Callable third;
	CHECK(collector.reserve(same_token, third) == OK);
	third.call(p01_initial_guarded_buffer());
	CHECK(collector.cancel_unqueued(bad) == OK);
	CHECK(collector.cancel_unqueued(bad) == ERR_INVALID_PARAMETER);
	PackedByteArray wrong_size;
	wrong_size.resize(63);
	callback.call(wrong_size);
	CHECK(collector.poisoned());
	PackedByteArray output;
	CHECK(collector.copy_completed(identity(), output) == ERR_INVALID_DATA);
	collector.close();
	CHECK(collector.drained());
	CHECK(collector.reserve(identity(1), other) == ERR_UNAVAILABLE);
}

TEST_CASE("[P01] S02 a14 requires its own callbacks even when a12 bytes match") {
	P01ReadbackCollector collector(1, 2, false);
	const auto a12_snapshot = identity(0);
	const auto a14_snapshot = identity(1);
	const auto a14_consumer = identity(1, true);
	Callable old_callback, snapshot_callback, consumer_callback;
	CHECK(collector.reserve(a12_snapshot, old_callback) == OK);
	CHECK(collector.reserve(a14_snapshot, snapshot_callback) == OK);
	CHECK(collector.reserve(a14_consumer, consumer_callback) == OK);
	CHECK(collector.mark_enqueued(a12_snapshot) == OK);
	CHECK(collector.mark_enqueued(a14_snapshot) == OK);
	CHECK(collector.mark_enqueued(a14_consumer) == OK);
	const auto bytes = p01_initial_guarded_buffer();
	old_callback.call(bytes);
	collector.close();
	PackedByteArray output;
	CHECK(collector.copy_completed(a12_snapshot, output) == OK);
	CHECK(collector.copy_completed(a14_snapshot, output) == ERR_BUSY);
	CHECK(collector.copy_completed(a14_consumer, output) == ERR_BUSY);
	CHECK_FALSE(collector.drained());
	snapshot_callback.call(bytes);
	CHECK(collector.copy_completed(a14_snapshot, output) == OK);
	CHECK(collector.copy_completed(a14_consumer, output) == ERR_BUSY);
	CHECK_FALSE(collector.drained());
	consumer_callback.call(bytes);
	CHECK(collector.copy_completed(a14_consumer, output) == OK);
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
}

TEST_CASE("[P01] Readback pair rejects guard damage and bitwise transport mismatch") {
	for (int corruption : { -1, 0, 36, 63 }) {
		P01ReadbackCollector collector(1, 2, false);
		Callable snapshot_callback, consumer_callback;
		const auto snapshot_id = identity();
		const auto consumer_id = identity(0, true);
		CHECK(collector.reserve(snapshot_id, snapshot_callback) == OK);
		CHECK(collector.reserve(consumer_id, consumer_callback) == OK);
		CHECK(collector.mark_enqueued(snapshot_id) == OK);
		CHECK(collector.mark_enqueued(consumer_id) == OK);
		CHECK(collector.validate_pair(snapshot_id, consumer_id) == ERR_BUSY);
		auto snapshot = p01_initial_guarded_buffer();
		auto consumer = p01_initial_guarded_buffer();
		if (corruption >= 0) { consumer.set(corruption, consumer[corruption] ^ 1); }
		snapshot_callback.call(snapshot);
		consumer_callback.call(consumer);
		CHECK(collector.validate_pair(snapshot_id, consumer_id) == (corruption < 0 ? OK : ERR_INVALID_DATA));
		CHECK(collector.validate_pair(consumer_id, snapshot_id) == ERR_INVALID_PARAMETER);
		auto wrong = consumer_id;
		wrong.slot = 1;
		CHECK(collector.validate_pair(snapshot_id, wrong) == ERR_INVALID_PARAMETER);
	}
}

}

TEST_CASE("[P01] Poisoned collector cannot certify previously completed bytes") {
	P01ReadbackCollector collector(1, 2, false);
	const auto snapshot_id = TestP01ReadbackCollector::identity();
	const auto consumer_id = TestP01ReadbackCollector::identity(0, true);
	Callable snapshot, consumer;
	REQUIRE(collector.reserve(snapshot_id, snapshot) == OK);
	REQUIRE(collector.reserve(consumer_id, consumer) == OK);
	CHECK(collector.mark_enqueued(snapshot_id) == OK);
	CHECK(collector.mark_enqueued(consumer_id) == OK);
	snapshot.call(p01_initial_guarded_buffer());
	consumer.call(p01_initial_guarded_buffer());
	CHECK(collector.validate_pair(snapshot_id, consumer_id) == OK);
	snapshot.call(p01_initial_guarded_buffer());
	CHECK(collector.poisoned());
	CHECK(collector.validate_pair(snapshot_id, consumer_id) == ERR_INVALID_DATA);
	PackedByteArray output;
	CHECK(collector.copy_completed(snapshot_id, output) == ERR_INVALID_DATA);
	collector.close();
	CHECK(collector.drained());
}
