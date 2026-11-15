/**************************************************************************/
/*  test_wrapper_r1_gates_2d.cpp                                          */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_wrapper_r1_gates_2d)

#if defined(THREADS_ENABLED) && !defined(PHYSICS_2D_DISABLED)

#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "servers/physics_2d/physics_server_2d_dummy.h"
#include "servers/physics_2d/physics_server_2d_wrap_mt.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

class TestPhysicsWrapper2DR1Access {
public:
	using Point = PhysicsServer2DWrapMT::TestPoint;
	struct Snapshot {
		uint64_t requested;
		uint64_t acknowledged;
		uint64_t publishers;
		bool parked;
		bool stopping;
	};
	static Snapshot snapshot(PhysicsServer2DWrapMT *p_wrapper) {
		MutexLock lock(p_wrapper->control_mutex);
		return { p_wrapper->requested_serial, p_wrapper->acknowledged_serial, p_wrapper->publishers, p_wrapper->parked, p_wrapper->stop_state != PhysicsServer2DWrapMT::StopState::OPEN };
	}
	static void hook(PhysicsServer2DWrapMT *p_wrapper, void (*p_callback)(void *, Point), void *p_context) {
		p_wrapper->test_context = p_context;
		p_wrapper->test_callback = p_callback;
	}
	static void force_early_park(PhysicsServer2DWrapMT *p_wrapper) { p_wrapper->_acquire_park(); }
	static void force_early_ack(PhysicsServer2DWrapMT *p_wrapper) {
		MutexLock lock(p_wrapper->control_mutex);
		p_wrapper->parked = true;
		p_wrapper->acknowledged_serial = p_wrapper->requested_serial;
	}
	template <typename T>
	static void enqueue_end_sync_tail(PhysicsServer2DWrapMT *p_wrapper, T *p_backend, void (T::*p_method)()) {
		PhysicsServer2DWrapMT::Scope release(p_wrapper, PhysicsServer2DWrapMT::ScopeKind::RELEASE);
		if (release) {
			p_wrapper->command_queue.push(p_backend, p_method);
		}
	}
};

namespace TestPhysicsWrapper2DR1Gates {

using Access = TestPhysicsWrapper2DR1Access;

[[noreturn]] static void trap(int p_code, const char *p_marker) {
	std::fprintf(stderr, "%s\n", p_marker);
	std::fflush(stderr);
	std::_Exit(p_code);
}

// Gate expiration is infrastructure failure (87), never the target RED (86).
static void wait_gate(Semaphore &p_gate) {
	const uint64_t deadline = OS::get_singleton()->get_ticks_usec() + 10000000;
	while (!p_gate.try_wait()) {
		if (OS::get_singleton()->get_ticks_usec() >= deadline) {
			trap(87, "PHYSICS2D_R1_FIXTURE_GATE_UNREACHED");
		}
		OS::get_singleton()->delay_usec(100);
	}
}

class Backend : public PhysicsServer2DDummy {
public:
	PhysicsServer2DWrapMT *wrapper = nullptr;
	std::atomic<int> sync_count{ 0 };
	std::atomic<int> end_count{ 0 };
	std::atomic<int> finish_count{ 0 };
	std::atomic<int> setter_count{ 0 };
	std::atomic<int> free_count{ 0 };
	std::atomic<int> sequence{ 0 };
	int setter_order = 0;
	int free_order = 0;
	int tail_order = 0;
	int finish_order = 0;
	bool finish_from_step = false;
	bool stage_end_tail = false;
	bool finish_cleanup = false;
	std::atomic<bool> step_returned_before_finish{ false };
	std::atomic<int> finish_count_before_step_return{ -1 };
	Semaphore step_returned;
	Thread::ID init_thread = Thread::UNASSIGNED_ID;
	Thread::ID finish_thread = Thread::UNASSIGNED_ID;
	WorkerThreadPool::TaskID init_task = WorkerThreadPool::INVALID_TASK_ID;
	WorkerThreadPool::TaskID finish_task = WorkerThreadPool::INVALID_TASK_ID;

	void init() override {
		init_thread = Thread::get_caller_id();
		init_task = WorkerThreadPool::get_singleton()->get_caller_task_id();
	}
	void step(real_t p_step) override {
		if (finish_from_step) {
			wrapper->finish();
			wrapper->finish();
			finish_count_before_step_return.store(finish_count.load());
			step_returned_before_finish.store(finish_count_before_step_return.load() == 0);
			step_returned.post();
		}
	}
	void sync() override { sync_count.fetch_add(1); }
	void end_sync() override {
		end_count.fetch_add(1);
		if (stage_end_tail) {
			// A real accepted release ticket stages internal owner cleanup.
			// It must be consumed by the post-end_sync drain, not FINISH_ONLY.
			Access::enqueue_end_sync_tail(wrapper, this, &Backend::end_sync_tail);
		}
	}
	void end_sync_tail() {
		tail_order = sequence.fetch_add(1) + 1;
		wrapper->free_rid(RID::from_uint64(202));
	}
	void shape_set_data(RID p_shape, const Variant &p_data) override {
		setter_order = sequence.fetch_add(1) + 1;
		setter_count.fetch_add(1);
	}
	void free_rid(RID p_rid) override {
		free_order = sequence.fetch_add(1) + 1;
		free_count.fetch_add(1);
	}
	void finish() override {
		finish_order = sequence.fetch_add(1) + 1;
		finish_count.fetch_add(1);
		finish_thread = Thread::get_caller_id();
		finish_task = WorkerThreadPool::get_singleton()->get_caller_task_id();
		if (finish_cleanup) {
			wrapper->free_rid(RID::from_uint64(303));
			wrapper->finish();
		}
	}
};

TEST_CASE("[PhysicsWrapper2DR1Gates] pump-origin finish returns from step and each Main boundary services it") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	int boundary = 0;
	SUBCASE("sync") {
		boundary = 0;
	}
	SUBCASE("flush_queries") {
		boundary = 1;
	}
	SUBCASE("end_sync") {
		boundary = 2;
	}
	SUBCASE("finish") {
		boundary = 3;
	}
	auto *backend = memnew(Backend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, true));
	backend->wrapper = wrapper;
	wrapper->init();
	backend->finish_from_step = true;
	wrapper->step(1.0 / 60.0);
	wait_gate(backend->step_returned);
	CHECK(backend->step_returned_before_finish.load());
	// The Main step root epilogue may already have serviced and joined the
	// requested stop. Assert the count captured inside the pump callback.
	CHECK(backend->finish_count_before_step_return.load() == 0);
	CHECK(Access::snapshot(wrapper).stopping);
	switch (boundary) {
		case 0:
			wrapper->sync();
			break;
		case 1:
			wrapper->flush_queries();
			break;
		case 2:
			wrapper->end_sync();
			break;
		case 3:
			wrapper->finish();
			break;
	}
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->sync_count.load() == 0);
	CHECK(backend->end_count.load() == 0);
	CHECK(backend->finish_thread == backend->init_thread);
	CHECK(backend->finish_task == backend->init_task);
	CHECK(backend->finish_task != WorkerThreadPool::INVALID_TASK_ID);
	wrapper->finish();
	CHECK(backend->finish_count.load() == 1);
	memdelete(wrapper);
}

struct BarrierContext {
	PhysicsServer2DWrapMT *wrapper;
	Backend *backend;
	Semaphore barrier_release{};
	Semaphore cleanup_entered{};
	Semaphore cleanup_release{};
	Semaphore negative_hold{};
	std::atomic<bool> barrier_entered{ false };
	std::atomic<bool> before_barrier{ false };
	std::atomic<bool> serial_zero_at_barrier{ false };
	bool force_invalid = false;
	bool force_early_ack = false;
	bool stop_did_not_park = false;
	bool ack_waited_for_cleanup = false;
};

class GatedCleanup : public RefCounted {
public:
	BarrierContext *context = nullptr;
	~GatedCleanup() {
		context->cleanup_entered.post();
		wait_gate(context->cleanup_release);
	}
};

static void barrier_hook(void *p_context, Access::Point p_point) {
	auto *context = static_cast<BarrierContext *>(p_context);
	if (p_point == Access::Point::BEFORE_BARRIER) {
		context->before_barrier.store(true);
		if (context->force_invalid) {
			// Negative control uses the real park transfer before the FIFO
			// barrier. The observer traps at this precise invalid state before
			// a parked-pump barrier wait can occur. Default positive does not.
			Access::force_early_park(context->wrapper);
			// Never allow the negative Main path to enqueue its barrier.
			// Only the independent observer exits this controlled process.
			wait_gate(context->negative_hold);
		}
	} else if (p_point == Access::Point::AFTER_BARRIER && context->force_early_ack) {
		// Publish the legal post-barrier request, then keep Main inside
		// this hook even if the observer injects an invalid acknowledgment.
		// Main cannot begin backend sync or a second flush before the trap.
		Access::force_early_park(context->wrapper);
		wait_gate(context->negative_hold);
	} else if (p_point == Access::Point::BARRIER_ENTER) {
		context->serial_zero_at_barrier.store(Access::snapshot(context->wrapper).requested == 0);
		context->barrier_entered.store(true);
		wait_gate(context->barrier_release);
	}
}

TEST_CASE("[PhysicsWrapper2DR1Gates] barrier precedes PARK and acknowledgment waits for trailing cleanup") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(Backend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, true));
	backend->wrapper = wrapper;
	wrapper->init();
	BarrierContext context{ wrapper, backend };
	context.force_invalid = OS::get_singleton()->get_environment("GODOT_TEST_PHYSICS_WRAPPER_2D_R3_CONTROL") == "early-park";
	context.force_early_ack = OS::get_singleton()->get_environment("GODOT_TEST_PHYSICS_WRAPPER_2D_R3_CONTROL") == "early-ack";
	Access::hook(wrapper, barrier_hook, &context);
	Thread controller;
	controller.start([](void *p_context) {
		auto *thread_context = static_cast<BarrierContext *>(p_context);
		const uint64_t deadline = OS::get_singleton()->get_ticks_usec() + 10000000;
		while (!thread_context->barrier_entered.load()) {
			if (thread_context->before_barrier.load() && Access::snapshot(thread_context->wrapper).requested != 0) {
				trap(86, "PHYSICS2D_R3_PARK_BEFORE_BARRIER_PRE_WAIT");
			}
			if (OS::get_singleton()->get_ticks_usec() >= deadline) {
				trap(87, "PHYSICS2D_R1_FIXTURE_BARRIER_UNREACHED");
			}
			OS::get_singleton()->delay_usec(100);
		}
		{
			Ref<GatedCleanup> payload;
			payload.instantiate();
			payload->context = thread_context;
			thread_context->wrapper->shape_set_data(RID(), Variant(payload));
		}
		thread_context->wrapper->finish(); // Foreign request during the held barrier.
		thread_context->stop_did_not_park = Access::snapshot(thread_context->wrapper).requested == 0;
		thread_context->barrier_release.post();
		wait_gate(thread_context->cleanup_entered);
		const uint64_t park_deadline = OS::get_singleton()->get_ticks_usec() + 10000000;
		while (Access::snapshot(thread_context->wrapper).requested == 0) {
			if (OS::get_singleton()->get_ticks_usec() >= park_deadline) {
				trap(87, "PHYSICS2D_R1_FIXTURE_PARK_REQUEST_UNREACHED");
			}
			OS::get_singleton()->delay_usec(100);
		}
		if (thread_context->force_early_ack) {
			Access::force_early_ack(thread_context->wrapper);
		}
		const auto held = Access::snapshot(thread_context->wrapper);
		thread_context->ack_waited_for_cleanup = held.acknowledged < held.requested && !held.parked && thread_context->backend->sync_count.load() == 0 && thread_context->backend->finish_count.load() == 0;
		if (!thread_context->ack_waited_for_cleanup) {
			trap(86, "PHYSICS2D_R3_ACK_BEFORE_CLEANUP_RETURN");
		}
		thread_context->cleanup_release.post();
	},
			&context);
	wrapper->sync();
	controller.wait_to_finish();
	CHECK(context.serial_zero_at_barrier.load());
	CHECK(context.stop_did_not_park);
	CHECK(context.ack_waited_for_cleanup);
	CHECK(backend->setter_count.load() == 1);
	CHECK(backend->sync_count.load() == 1);
	CHECK(backend->end_count.load() == 1);
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->finish_task == backend->init_task);
	CHECK(Access::snapshot(wrapper).requested == 1);
	memdelete(wrapper);
}

struct PublisherContext {
	PhysicsServer2DWrapMT *wrapper;
	Backend *backend;
	Semaphore admitted{};
	Semaphore publish{};
	Semaphore empty_flush{};
	Semaphore resume_drain{};
	Semaphore producer_done{};
	std::atomic<bool> root_gated{ false };
	std::atomic<bool> drain_gated{ false };
	bool empty_flush_still_had_publisher = false;
};

class ReleaseFromCleanup : public RefCounted {
public:
	PhysicsServer2DWrapMT *wrapper = nullptr;
	~ReleaseFromCleanup() {
		wrapper->free_rid(RID::from_uint64(101));
		wrapper->finish();
	}
};

static void publisher_hook(void *p_context, Access::Point p_point) {
	auto *context = static_cast<PublisherContext *>(p_context);
	if (p_point == Access::Point::ROOT_ADMITTED && !Thread::is_main_thread() && !context->root_gated.exchange(true)) {
		context->admitted.post();
		wait_gate(context->publish);
	} else if (p_point == Access::Point::DRAIN_AFTER_FLUSH && !context->drain_gated.exchange(true)) {
		context->empty_flush_still_had_publisher = Access::snapshot(context->wrapper).publishers == 1 && context->backend->setter_count.load() == 0;
		context->empty_flush.post();
		wait_gate(context->resume_drain);
	}
}

TEST_CASE("[PhysicsWrapper2DR1Gates] late admitted publisher and end-sync release tail precede sealed pump finish") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(Backend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, true));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->sync();
	backend->stage_end_tail = true;
	backend->finish_cleanup = true;
	PublisherContext context{ wrapper, backend };
	Access::hook(wrapper, publisher_hook, &context);
	Thread producer;
	producer.start([](void *p_context) {
		auto *thread_context = static_cast<PublisherContext *>(p_context);
		{
			Ref<ReleaseFromCleanup> payload;
			payload.instantiate();
			payload->wrapper = thread_context->wrapper;
			thread_context->wrapper->shape_set_data(RID(), Variant(payload));
		}
		thread_context->producer_done.post();
	},
			&context);
	wait_gate(context.admitted);
	Thread controller;
	controller.start([](void *p_context) {
		auto *thread_context = static_cast<PublisherContext *>(p_context);
		wait_gate(thread_context->empty_flush);
		thread_context->publish.post();
		wait_gate(thread_context->producer_done);
		thread_context->resume_drain.post();
	},
			&context);
	wrapper->finish();
	producer.wait_to_finish();
	controller.wait_to_finish();
	CHECK(context.empty_flush_still_had_publisher);
	CHECK(backend->setter_count.load() == 1);
	CHECK(backend->free_count.load() == 2);
	CHECK(backend->end_count.load() == 1);
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->setter_order < backend->tail_order);
	CHECK(backend->tail_order < backend->free_order);
	CHECK(backend->free_order < backend->finish_order);
	CHECK(backend->finish_thread == backend->init_thread);
	CHECK(backend->finish_task == backend->init_task);
	CHECK(Access::snapshot(wrapper).publishers == 0);
	CHECK(Access::snapshot(wrapper).requested == 1); // Existing lease was reused.
	memdelete(wrapper);
}

struct BoundaryContext {
	PhysicsServer2DWrapMT *wrapper;
	Semaphore entered{};
	Semaphore release{};
	std::atomic<bool> gated{ false };
};

static void boundary_hook(void *p_context, Access::Point p_point) {
	auto *context = static_cast<BoundaryContext *>(p_context);
	if (p_point == Access::Point::BOUNDARY_ENTRY && !context->gated.exchange(true)) {
		context->entered.post();
		wait_gate(context->release);
	}
}

TEST_CASE("[PhysicsWrapper2DR1Gates] rejected Main boundary admission still services a raced foreign stop") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	int boundary = 0;
	SUBCASE("sync") {
		boundary = 0;
	}
	SUBCASE("flush_queries") {
		boundary = 1;
	}
	SUBCASE("end_sync") {
		boundary = 2;
	}
	auto *backend = memnew(Backend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, true));
	backend->wrapper = wrapper;
	wrapper->init();
	BoundaryContext context{ wrapper };
	Access::hook(wrapper, boundary_hook, &context);
	Thread controller;
	controller.start([](void *p_context) {
		auto *thread_context = static_cast<BoundaryContext *>(p_context);
		wait_gate(thread_context->entered);
		thread_context->wrapper->finish();
		thread_context->release.post();
	},
			&context);
	switch (boundary) {
		case 0:
			wrapper->sync();
			break;
		case 1:
			wrapper->flush_queries();
			break;
		case 2:
			wrapper->end_sync();
			break;
	}
	controller.wait_to_finish();
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->sync_count.load() == 0);
	CHECK(backend->finish_task == backend->init_task);
	// Assertion precedes any redundant finish: a later call cannot make the
	// boundary-under-test look as though it serviced its own exit request.
	wrapper->finish();
	memdelete(wrapper);
}

} // namespace TestPhysicsWrapper2DR1Gates

#endif // THREADS_ENABLED && !PHYSICS_2D_DISABLED
