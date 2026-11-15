/**************************************************************************/
/*  test_pipeline_hash_map_recompile.cpp                                  */
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

#include "core/object/worker_thread_pool.h"
#include "core/os/semaphore.h"
#include "servers/rendering/renderer_rd/pipeline_hash_map_rd.h"
#include "tests/test_macros.h"
#include "tests/test_tools.h"

#include <atomic>

TEST_FORCE_LINK(test_pipeline_hash_map_recompile)

#ifdef THREADS_ENABLED
namespace TestPipelineHashMapRecompile {
struct WorkerProbe {
	using Cache = PipelineHashMapRD<uint32_t, WorkerProbe, void (WorkerProbe::*)(uint32_t)>;
	Cache cache;
	Semaphore entered, release, returned;
	std::atomic<WorkerThreadPool::TaskID> task_id{ WorkerThreadPool::INVALID_TASK_ID };
	std::atomic<uint32_t> generation{ 7 }, observed_generation{ 0 }, calls{ 0 };
	bool publish_null = false;
	WorkerProbe() { cache.set_creation_object_and_function(this, &WorkerProbe::compile); }
	void compile(uint32_t key) {
		task_id.store(WorkerThreadPool::get_singleton()->get_caller_task_id());
		++calls;
		if (publish_null) {
			// A CPU publication marker, never an actual compiled native pipeline.
			cache.add_compiled_pipeline(key, RID());
		}
		entered.post();
		release.wait();
		observed_generation.store(generation.load());
		returned.post();
	}
};

static void require_original_task_returned(WorkerThreadPool::TaskID task) {
	// WorkerThreadPool removes a task only after its actual wait returns. A
	// second wait must reject that real ID; neither time nor a fake RID proves it.
	ErrorDetector errors;
	ERR_PRINT_OFF;
	const Error error = WorkerThreadPool::get_singleton()->wait_for_task_completion(task);
	ERR_PRINT_ON;
	CHECK(error == ERR_INVALID_PARAMETER);
	CHECK(errors.has_error);
}

TEST_CASE("[PipelineHashMapRD] nonwaiting adoption retains the original worker until cache drain") {
	REQUIRE(WorkerThreadPool::get_singleton() != nullptr);
	REQUIRE(WorkerThreadPool::get_singleton()->get_thread_count() > 0);
	WorkerProbe probe;
	probe.publish_null = true;
	WorkerThreadPool::TaskID first = WorkerThreadPool::INVALID_TASK_ID;
	for (uint32_t turn = 0; turn < 2; ++turn) {
		probe.cache.compile_pipeline(13, 13, RSE::PIPELINE_SOURCE_MESH, true);
		probe.entered.wait();
		const auto task = probe.task_id.load();
		CHECK(task != WorkerThreadPool::INVALID_TASK_ID);
		if (turn == 0) {
			first = task;
		} else {
			CHECK(task != first); // Same key, an actual new original worker after clear.
		}
		// This must return while that worker is still blocked. Adopting a
		// published entry cannot silently synchronize asynchronous compilation.
		CHECK(probe.cache.get_pipeline(13, 13, false, RSE::PIPELINE_SOURCE_MESH).is_null());
		CHECK_FALSE(probe.returned.try_wait());
		CHECK_FALSE(WorkerThreadPool::get_singleton()->is_task_completed(task));
		probe.release.post();
		{
			ErrorDetector errors;
			probe.cache.clear_pipelines();
			CHECK_FALSE(errors.has_error); // In particular, do not free a null placeholder.
		}
		probe.generation.store(9);
		probe.returned.wait();
		CHECK(probe.observed_generation.load() == (turn == 0 ? 7u : 9u));
		require_original_task_returned(task);
	}
	CHECK(probe.calls.load() == 2);
	ErrorDetector errors;
	probe.cache.clear_pipelines();
	CHECK_FALSE(errors.has_error);
}

TEST_CASE("[PipelineHashMapRD] failed worker returns and empty placeholders do not leave stale task IDs") {
	REQUIRE(WorkerThreadPool::get_singleton() != nullptr);
	REQUIRE(WorkerThreadPool::get_singleton()->get_thread_count() > 0);
	for (bool wait_for_compilation : { false, true }) {
		WorkerProbe probe; // No publication is the original worker error-return shape.
		WorkerThreadPool::TaskID first = WorkerThreadPool::INVALID_TASK_ID;
		for (uint32_t turn = 0; turn < 2; ++turn) {
			probe.cache.compile_pipeline(17, 17, RSE::PIPELINE_SOURCE_MESH, true);
			probe.entered.wait();
			const auto task = probe.task_id.load();
			CHECK(task != WorkerThreadPool::INVALID_TASK_ID);
			if (turn == 0) {
				first = task;
			} else {
				CHECK(task != first);
			}
			probe.release.post();
			{
				ErrorDetector errors;
				if (wait_for_compilation) {
					CHECK(probe.cache.get_pipeline(17, 17, true, RSE::PIPELINE_SOURCE_MESH).is_null());
				}
				probe.cache.clear_pipelines();
				probe.cache.clear_pipelines();
				CHECK_FALSE(errors.has_error);
			}
			probe.returned.wait();
			require_original_task_returned(task);
		}
		CHECK(probe.calls.load() == 2);
	}
}

TEST_CASE("[PipelineHashMapRD] original destructor waits for its accepted worker return") {
	REQUIRE(WorkerThreadPool::get_singleton() != nullptr);
	REQUIRE(WorkerThreadPool::get_singleton()->get_thread_count() > 0);
	WorkerProbe worker;
	using Cache = WorkerProbe::Cache;
	auto *cache = memnew(Cache);
	cache->set_creation_object_and_function(&worker, &WorkerProbe::compile);
	cache->compile_pipeline(19, 19, RSE::PIPELINE_SOURCE_MESH, true);
	worker.entered.wait();
	const auto task = worker.task_id.load();
	worker.release.post();
	{
		ErrorDetector errors;
		memdelete(cache);
		CHECK_FALSE(errors.has_error);
	}
	worker.returned.wait();
	require_original_task_returned(task);
}
} // namespace TestPipelineHashMapRecompile
#endif
