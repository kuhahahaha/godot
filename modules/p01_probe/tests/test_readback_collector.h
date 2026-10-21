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

}
