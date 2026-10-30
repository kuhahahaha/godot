/**************************************************************************/
/*  test_job_system_retirement.h                                          */
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

#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "tests/test_macros.h"

#include "modules/jolt_physics/spaces/jolt_job_system.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

class TestJoltJobSystemAccess {
public:
	static void set_hooks(void (*p_after_execute)(void *), void (*p_after_release)(void *), void (*p_before_destroy)(void *), void (*p_before_shutdown_wait)(void *), void *p_userdata) {
		JoltJobSystem::test_after_execute = p_after_execute;
		JoltJobSystem::test_after_release = p_after_release;
		JoltJobSystem::test_before_destroy = p_before_destroy;
		JoltJobSystem::test_before_shutdown_wait = p_before_shutdown_wait;
		JoltJobSystem::test_userdata = p_userdata;
	}

	static bool has_completed(JoltJobSystem *p_system) {
		p_system->completed_lock.lock();
		const bool has_completed = p_system->completed_head != nullptr;
		p_system->completed_lock.unlock();
		return has_completed;
	}
};

namespace TestJoltJobSystemRetirement {

#ifdef THREADS_ENABLED
struct Witness {
	Semaphore worker_entered;
	Semaphore release_worker;
	Semaphore worker_released;
	Semaphore shutdown_waiting;
	std::atomic<bool> released{ false };
};

static void after_execute(void *p_userdata) {
	Witness *witness = static_cast<Witness *>(p_userdata);
	witness->worker_entered.post();
	witness->release_worker.wait();
}

static void after_release(void *p_userdata) {
	Witness *witness = static_cast<Witness *>(p_userdata);
	witness->released.store(true, std::memory_order_release);
	witness->worker_released.post();
}

static void before_destroy(void *p_userdata) {
	Witness *witness = static_cast<Witness *>(p_userdata);
	if (!witness->released.load(std::memory_order_acquire)) {
		std::fputs("JOLT_JOB_RETIREMENT_PRE_FREE_BLOCKED\n", stderr);
		std::fflush(stderr);
		std::_Exit(86);
	}
}

static void before_shutdown_wait(void *p_userdata) {
	static_cast<Witness *>(p_userdata)->shutdown_waiting.post();
}

TEST_CASE("[JoltJobSystem] shutdown retains a worker through its final release") {
	const bool race = OS::get_singleton()->get_environment("GODOT_TEST_JOLT_JOB_RETIREMENT") == "race";
	Witness witness;
	TestJoltJobSystemAccess::set_hooks(after_execute, after_release, before_destroy, before_shutdown_wait, &witness);
	JoltJobSystem *system = new JoltJobSystem;
	JPH::JobSystem *base = system;
	JPH::JobSystem::Barrier *barrier = base->CreateBarrier();
	CHECK(barrier != nullptr);
	if (!barrier) {
		TestJoltJobSystemAccess::set_hooks(nullptr, nullptr, nullptr, nullptr, nullptr);
		delete system;
		return;
	}
	JPH::JobHandle job = base->CreateJob("Retirement witness", JPH::Color::sWhite, []() {}, 1);
	barrier->AddJob(job);
	job.RemoveDependency();
	witness.worker_entered.wait();
	base->WaitForJobs(barrier);
	base->DestroyBarrier(barrier);
	job = JPH::JobHandle();
	system->post_step();

	if (race) {
		std::thread controller([&]() {
			witness.shutdown_waiting.wait();
			witness.release_worker.post();
		});
		delete system;
		controller.join();
	} else {
		witness.release_worker.post();
		witness.worker_released.wait();
		system->post_step();
		delete system;
	}
	CHECK(witness.released.load(std::memory_order_acquire));
	TestJoltJobSystemAccess::set_hooks(nullptr, nullptr, nullptr, nullptr, nullptr);
}
#endif // THREADS_ENABLED

TEST_CASE("[JoltJobSystem] completed jobs remain with their original owner") {
	JoltJobSystem *first = new JoltJobSystem;
	JoltJobSystem *second = new JoltJobSystem;
	JPH::JobSystem *first_api = first;
	JPH::JobHandle job = first_api->CreateJob("Owner witness", JPH::Color::sWhite, []() {}, 1);
	job = JPH::JobHandle();
	CHECK(TestJoltJobSystemAccess::has_completed(first));
	const bool second_has_completed = TestJoltJobSystemAccess::has_completed(second);
	CHECK_FALSE(second_has_completed);
	// Avoid asking a different owner to reclaim the job if the invariant is broken.
	if (!second_has_completed) {
		second->post_step();
	}
	CHECK(TestJoltJobSystemAccess::has_completed(first));
	first->post_step();
	CHECK_FALSE(TestJoltJobSystemAccess::has_completed(first));
	delete second;
	delete first;
}

} // namespace TestJoltJobSystemRetirement
