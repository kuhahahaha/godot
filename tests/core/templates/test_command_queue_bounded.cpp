/**************************************************************************/
/*  test_command_queue_bounded.cpp                                        */
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

#include "core/templates/command_queue_mt.h"
#include "tests/test_macros.h"

#include <atomic>
#include <chrono>
#include <limits>
#include <thread>

TEST_FORCE_LINK(test_command_queue_bounded)

namespace TestCommandQueueBounded {
class Access {
public:
	static void lock(CommandQueueMT &queue) { queue.mutex.lock(); }
	static void unlock(CommandQueueMT &queue) { queue.mutex.unlock(); }
	static uint64_t capacity(const CommandQueueMT &queue) { return queue.command_mem.get_capacity(); }
	static uint64_t used(const CommandQueueMT &queue) { return queue.command_mem.size(); }
	static bool pending(const CommandQueueMT &queue) { return queue.pending.load(); }
};
struct Target {
	uint64_t values[4] = {};
	uint32_t count = 0;
	void call(uint64_t value) {
		if (count < 4) {
			values[count++] = value;
		}
	}
};
TEST_CASE("[CommandQueue][Qualification] full bounded enqueue has no effect and retry preserves FIFO") {
	CommandQueueMT queue;
	Target target;
	CHECK_FALSE(queue.try_push_bounded(0, &target, &Target::call, uint64_t(77)));
	CHECK(Access::used(queue) == 0);
	CHECK_FALSE(Access::pending(queue));
	queue.flush_all();
	CHECK(target.count == 0);
	REQUIRE(queue.try_push_bounded(64, &target, &Target::call, uint64_t(11)));
	const uint64_t used = Access::used(queue);
	CHECK_FALSE(queue.try_push_bounded(64, &target, &Target::call, uint64_t(22)));
	CHECK(Access::used(queue) == used);
	CHECK(Access::pending(queue));
	queue.flush_all();
	REQUIRE(target.count == 1);
	CHECK(target.values[0] == 11);
	REQUIRE(queue.try_push_bounded(64, &target, &Target::call, uint64_t(22)));
	queue.push(&target, &Target::call, uint64_t(33));
	queue.flush_all();
	REQUIRE(target.count == 3);
	CHECK(target.values[1] == 22);
	CHECK(target.values[2] == 33);
}

struct SequenceTarget {
	uint64_t count = 0;
	bool ordered = true;
	void call(uint64_t value) {
		ordered = ordered && value == count;
		count++;
	}
};

TEST_CASE("[CommandQueue][Qualification] bounded enqueue cannot grow reserved storage with a larger caller limit") {
	CommandQueueMT queue;
	SequenceTarget target;
	const uint64_t capacity = Access::capacity(queue);
	const uint64_t limit = std::numeric_limits<uint64_t>::max();
	REQUIRE(queue.try_push_bounded(limit, &target, &SequenceTarget::call, uint64_t(0)));
	const uint64_t command_bytes = Access::used(queue);
	const uint64_t accepted = capacity / command_bytes;
	for (uint64_t i = 1; i < accepted; i++) {
		REQUIRE(queue.try_push_bounded(limit, &target, &SequenceTarget::call, i));
	}
	const uint64_t used = Access::used(queue);
	CHECK_FALSE(queue.try_push_bounded(limit, &target, &SequenceTarget::call, accepted));
	CHECK(Access::capacity(queue) == capacity);
	CHECK(Access::used(queue) == used);
	CHECK(Access::pending(queue));
	queue.flush_all();
	CHECK(target.ordered);
	CHECK(target.count == accepted);
	CHECK(Access::capacity(queue) == capacity);
	CHECK_FALSE(Access::pending(queue));
	REQUIRE(queue.try_push_bounded(command_bytes, &target, &SequenceTarget::call, accepted));
	queue.flush_all();
	CHECK(target.ordered);
	CHECK(target.count == accepted + 1);
}

TEST_CASE("[CommandQueue][Qualification] ordinary enqueue can still grow storage after bounded refusal") {
	CommandQueueMT queue;
	SequenceTarget target;
	const uint64_t capacity = Access::capacity(queue);
	REQUIRE(queue.try_push_bounded(capacity, &target, &SequenceTarget::call, uint64_t(0)));
	const uint64_t command_bytes = Access::used(queue);
	const uint64_t accepted = capacity / command_bytes;
	for (uint64_t i = 1; i < accepted; i++) {
		REQUIRE(queue.try_push_bounded(capacity, &target, &SequenceTarget::call, i));
	}
	CHECK_FALSE(queue.try_push_bounded(capacity, &target, &SequenceTarget::call, accepted));
	queue.push(&target, &SequenceTarget::call, accepted);
	CHECK(Access::capacity(queue) > capacity);
	queue.flush_all();
	CHECK(target.ordered);
	CHECK(target.count == accepted + 1);
}

#ifdef THREADS_ENABLED
TEST_CASE("[CommandQueue][Qualification] contended bounded enqueue refuses without waiting or publication") {
	CommandQueueMT queue;
	Target target;
	std::atomic<bool> returned = false;
	bool accepted = true;
	Access::lock(queue);
	std::thread producer([&] { accepted=queue.try_push_bounded(64,&target,&Target::call,uint64_t(42));returned.store(true); });
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	while (!returned.load() && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::yield();
	}
	const bool refused_before_unlock = returned.load();
	Access::unlock(queue);
	producer.join();
	CHECK(refused_before_unlock);
	CHECK_FALSE(accepted);
	CHECK(Access::used(queue) == 0);
	CHECK_FALSE(Access::pending(queue));
	queue.flush_all();
	CHECK(target.count == 0);
}
#endif
} //namespace TestCommandQueueBounded
