/**************************************************************************/
/*  test_readback_collector.h                                             */
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
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_gpu_payload.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_readback_collector.h"

namespace TestSkinnedMeshCaptureTestReadbackCollector {
static SkinnedMeshCaptureTestReadbackIdentity identity(uint32_t p_slot = 0, bool p_consumer = false) {
	return { 1, 2, 3 + p_slot * 2 + uint64_t(p_consumer), 40 + p_slot * 2 + uint64_t(p_consumer), 5, 6, 7, 8, 9, p_slot, p_consumer };
}
TEST_CASE("[P01] Collector preserves exact identity copied bytes and late close completion") {
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
	Callable callback;
	const auto id = identity();
	REQUIRE(collector.reserve(id, callback) == OK);
	CHECK(collector.mark_enqueued(id) == OK);
	CHECK(collector.mark_enqueued(id) == ERR_INVALID_PARAMETER);
	CHECK(collector.poisoned());
	CHECK(collector.cancel_unqueued(id) == ERR_INVALID_PARAMETER);
	Callable duplicate;
	CHECK(collector.reserve(id, duplicate) != OK);
	PackedByteArray bytes = skinned_mesh_capture_test_initial_guarded_buffer();
	PackedByteArray output;
	CHECK(collector.copy_completed(id, output) == ERR_BUSY);
	collector.close();
	CHECK_FALSE(collector.drained());
	callback.call(bytes);
	// Receipt retires pending work, but the caller-held Callable is still a
	// live engine callback holder. Drain requires both facts.
	CHECK_FALSE(collector.drained());
	Callable retained = callback;
	callback = Callable();
	CHECK_FALSE(collector.drained());
	CHECK(collector.cancel_unqueued(id) == ERR_INVALID_PARAMETER);
	REQUIRE(collector.copy_completed(id, output) == OK);
	bytes.set(0, 0);
	if (output.size() == 64) {
		CHECK(output[0] == 0x21);
	} else {
		CHECK(output.size() == 64);
	}
	auto stale = id;
	stale.alias++;
	CHECK(collector.copy_completed(stale, output) == ERR_INVALID_PARAMETER);
	retained.call(bytes);
	CHECK(collector.poisoned());
	retained = Callable();
	CHECK(collector.drained());
}
TEST_CASE("[P01] Collector callback survives owner and bounded pools reject overflow") {
	Callable surviving;
	{
		SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
		REQUIRE(collector.reserve(identity(), surviving) == OK);
		collector.close();
	}
	surviving.call(skinned_mesh_capture_test_initial_guarded_buffer());
	for (bool dual : { false, true }) {
		SkinnedMeshCaptureTestReadbackCollector collector(1, 2, dual);
		const uint32_t capacity = dual ? 18 : 8;
		for (uint32_t slot = 0; slot < capacity; slot++) {
			for (bool consumer : { false, true }) {
				Callable callback;
				REQUIRE(collector.reserve(identity(slot, consumer), callback) == OK);
				callback.call(skinned_mesh_capture_test_initial_guarded_buffer());
			}
		}
		Callable overflow;
		CHECK(collector.reserve(identity(capacity), overflow) == ERR_INVALID_PARAMETER);
		collector.close();
		CHECK(collector.drained());
	}
}
TEST_CASE("[P01] Collector rejects malformed requests and malformed completed bytes") {
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
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
	third.call(skinned_mesh_capture_test_initial_guarded_buffer());
	CHECK(collector.cancel_unqueued(bad) == OK);
	CHECK(collector.cancel_unqueued(bad) == ERR_INVALID_PARAMETER);
	CHECK(collector.poisoned());
	PackedByteArray wrong_size;
	wrong_size.resize(63);
	callback.call(wrong_size);
	CHECK(collector.poisoned());
	PackedByteArray output;
	CHECK(collector.copy_completed(identity(), output) == ERR_INVALID_DATA);
	collector.close();
	CHECK_FALSE(collector.drained());
	callback = Callable();
	other = Callable();
	third = Callable();
	CHECK(collector.drained());
	CHECK(collector.reserve(identity(1), other) == ERR_UNAVAILABLE);
}

TEST_CASE("[P01] S02 a14 requires its own callbacks even when a12 bytes match") {
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
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
	const auto bytes = skinned_mesh_capture_test_initial_guarded_buffer();
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
	CHECK_FALSE(collector.drained());
	old_callback = Callable();
	snapshot_callback = Callable();
	consumer_callback = Callable();
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
}

TEST_CASE("[P01] Readback pair rejects guard damage and bitwise transport mismatch") {
	for (int corruption : { -1, 0, 36, 63 }) {
		SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
		Callable snapshot_callback, consumer_callback;
		const auto snapshot_id = identity();
		const auto consumer_id = identity(0, true);
		CHECK(collector.reserve(snapshot_id, snapshot_callback) == OK);
		CHECK(collector.reserve(consumer_id, consumer_callback) == OK);
		CHECK(collector.mark_enqueued(snapshot_id) == OK);
		CHECK(collector.mark_enqueued(consumer_id) == OK);
		CHECK(collector.validate_pair(snapshot_id, consumer_id) == ERR_BUSY);
		auto snapshot = skinned_mesh_capture_test_initial_guarded_buffer();
		auto consumer = skinned_mesh_capture_test_initial_guarded_buffer();
		if (corruption >= 0) {
			consumer.set(corruption, consumer[corruption] ^ 1);
		}
		snapshot_callback.call(snapshot);
		consumer_callback.call(consumer);
		CHECK(collector.validate_pair(snapshot_id, consumer_id) == (corruption < 0 ? OK : ERR_INVALID_DATA));
		CHECK(collector.validate_pair(consumer_id, snapshot_id) == ERR_INVALID_PARAMETER);
		auto wrong = consumer_id;
		wrong.slot = 1;
		CHECK(collector.validate_pair(snapshot_id, wrong) == ERR_INVALID_PARAMETER);
	}
}

} //namespace TestSkinnedMeshCaptureTestReadbackCollector

TEST_CASE("[P01] Poisoned collector cannot certify previously completed bytes") {
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
	const auto snapshot_id = TestSkinnedMeshCaptureTestReadbackCollector::identity();
	const auto consumer_id = TestSkinnedMeshCaptureTestReadbackCollector::identity(0, true);
	Callable snapshot, consumer;
	REQUIRE(collector.reserve(snapshot_id, snapshot) == OK);
	REQUIRE(collector.reserve(consumer_id, consumer) == OK);
	CHECK(collector.mark_enqueued(snapshot_id) == OK);
	CHECK(collector.mark_enqueued(consumer_id) == OK);
	snapshot.call(skinned_mesh_capture_test_initial_guarded_buffer());
	consumer.call(skinned_mesh_capture_test_initial_guarded_buffer());
	CHECK(collector.validate_pair(snapshot_id, consumer_id) == OK);
	snapshot.call(skinned_mesh_capture_test_initial_guarded_buffer());
	CHECK(collector.poisoned());
	CHECK(collector.validate_pair(snapshot_id, consumer_id) == ERR_INVALID_DATA);
	PackedByteArray output;
	CHECK(collector.copy_completed(snapshot_id, output) == ERR_INVALID_DATA);
	collector.close();
	CHECK_FALSE(collector.drained());
	snapshot = Callable();
	consumer = Callable();
	CHECK(collector.drained());
}

// CPU-side evidence acquisition rejects identity substitutions even when the
// bytes are identical. Parent route/verdict classification is not duplicated.
TEST_CASE("[P01] Evidence") {
	using namespace TestSkinnedMeshCaptureTestReadbackCollector;
	for (int field = 0; field < 11; field++) {
		SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
		const auto original = identity();
		const auto consumer = identity(0, true);
		Callable snapshot_callback, consumer_callback;
		REQUIRE(collector.reserve(original, snapshot_callback) == OK);
		REQUIRE(collector.reserve(consumer, consumer_callback) == OK);
		REQUIRE(collector.mark_enqueued(original) == OK);
		REQUIRE(collector.mark_enqueued(consumer) == OK);
		PackedByteArray bytes = skinned_mesh_capture_test_initial_guarded_buffer();
		snapshot_callback.call(bytes);
		CHECK(collector.validate_pair(original, consumer) == ERR_BUSY);
		consumer_callback.call(bytes);
		REQUIRE(collector.validate_pair(original, consumer) == OK);
		auto forged = original;
		switch (field) {
			case 0:
				forged.run++;
				break;
			case 1:
				forged.epoch++;
				break;
			case 2:
				forged.token++;
				break;
			case 3:
				forged.queue++;
				break;
			case 4:
				forged.device++;
				break;
			case 5:
				forged.allocation++;
				break;
			case 6:
				forged.rid++;
				break;
			case 7:
				forged.owner++;
				break;
			case 8:
				forged.alias++;
				break;
			case 9:
				forged.slot++;
				break;
			case 10:
				forged.consumer = true;
				break;
		}
		PackedByteArray rejected;
		CHECK(collector.copy_completed(forged, rejected) == ERR_INVALID_PARAMETER);
		CHECK(rejected.is_empty());
		CHECK(collector.validate_pair(forged, consumer) != OK);
		CHECK(collector.validate_pair(original, consumer) == OK);
	}
}

TEST_CASE("[P01] Raw holder drain survives cancellation and missing receipt") {
	using namespace TestSkinnedMeshCaptureTestReadbackCollector;
	for (bool cancel : { false, true }) {
		SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
		Callable callback;
		REQUIRE(collector.reserve(identity(), callback) == OK);
		Callable retained = callback;
		if (cancel) {
			REQUIRE(collector.cancel_unqueued(identity()) == OK);
		}
		collector.close();
		CHECK_FALSE(collector.drained());
		callback = Callable();
		CHECK_FALSE(collector.drained());
		retained = Callable();
		CHECK(collector.drained() == cancel);
	}
}

TEST_CASE("[P01] Raw reserve refuses occupied output without replacing callback") {
	using namespace TestSkinnedMeshCaptureTestReadbackCollector;
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
	Callable callback;
	REQUIRE(collector.reserve(identity(), callback) == OK);
	Callable retained = callback; // Keep destruction outside reserve on the RED path.
	CHECK(collector.reserve(identity(1), callback) == ERR_INVALID_PARAMETER);
	CHECK(callback == retained);
	Callable next;
	CHECK(collector.reserve(identity(1), next) == OK);
}
