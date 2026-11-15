/**************************************************************************/
/*  test_rd_pool.h                                                        */
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

#include "servers/rendering/render_capture_test_hooks.h"

#include "modules/skinned_mesh_capture_tests/render_capture_test_access.h"
#pragma once
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_gpu_payload.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_rd_pool.h"

#include <thread>

namespace TestSkinnedMeshCaptureTestRDPool {
TEST_CASE("[P01] Main RD pool rejects absent device without admitting recordings") {
	// doctest uses the dummy renderer. This exercises rejection only, never GPU.
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	SkinnedMeshCaptureTestRDPool pool;
	CHECK(pool.physical_retirement_status() == ERR_UNCONFIGURED);
	CHECK(pool.initialize(false) == ERR_UNAVAILABLE);
	CHECK(pool.initialize(true) == ERR_UNAVAILABLE);
	CHECK(pool.record_public(0, SkinnedMeshCaptureTestPublicSnapshot()) == ERR_UNCONFIGURED);
	CHECK(pool.record_native(0, RID(), 0) == ERR_UNCONFIGURED);
	RID image;
	CHECK(pool.record_image(0, RID(), image) == ERR_UNCONFIGURED);
	CHECK(image.is_null());
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
	CHECK(pool.enqueue_readback(SkinnedMeshCaptureTestReadbackIdentity(), collector) == ERR_UNCONFIGURED);
	CHECK(pool.release() == OK);
	CHECK(pool.release() == OK);
}
} //namespace TestSkinnedMeshCaptureTestRDPool

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_write_census.h"
class SkinnedMeshCaptureTestWriteCensusTestAccess {
public:
	static void check_scanner() {
		RenderingDevice rd;
		RenderingDeviceGraph::ResourceTracker tracker;
		tracker.buffer_driver_id.id = 100;
		const auto serial = RenderCaptureTestAccess::capture_test_allocation_registry(&rd).allocate(100, 101);
		REQUIRE(serial != 0);
		RenderCaptureTestAccess::UniformSet set;
		set.draw_trackers.push_back(&tracker);
		set.draw_trackers_usage.push_back(RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		const RID rid = RenderCaptureTestAccess::uniform_set_owner(&rd).make_rid(set);
		skinned_mesh_capture_test_hook_reset();
		skinned_mesh_capture_test_hook_begin(11);
		RenderCaptureTestAccess::record_writable_set(&rd, rid, RenderCaptureTestHooks::COMPUTE);
		CHECK(skinned_mesh_capture_test_write_census_events().empty());
		RenderCaptureTestAccess::uniform_set_owner(&rd).get_or_null(rid)->draw_trackers_usage.write[0] = RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE;
		for (uint32_t operation : { RenderCaptureTestHooks::DRAW, RenderCaptureTestHooks::DRAW_INDIRECT, RenderCaptureTestHooks::COMPUTE, RenderCaptureTestHooks::COMPUTE_INDIRECT, RenderCaptureTestHooks::RAYTRACE }) {
			RenderCaptureTestAccess::record_writable_set(&rd, rid, operation);
			RenderCaptureTestAccess::record_writable_set(&rd, rid, operation);
		}
		const auto events = skinned_mesh_capture_test_write_census_events();
		CHECK(events.size() == 10);
		for (const auto &event : events) {
			CHECK(event.allocation == serial);
			CHECK(event.device == uint64_t(rd.get_instance_id()));
			CHECK(event.driver == 100);
			CHECK(event.epoch == 11);
		}
		CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
		RenderCaptureTestHooks::observe_buffer_copy(1, 2, 0, 0, 36, uint64_t(rd.get_instance_id()), serial, serial);
		RenderCaptureTestAccess::record_writable_set(&rd, rid, RenderCaptureTestHooks::COMPUTE);
		const auto ordered = skinned_mesh_capture_test_hook_events();
		REQUIRE(ordered.size() == 12);
		CHECK(ordered[10].kind == 7);
		CHECK(ordered[11].sequence == ordered[10].sequence + 1);
		CHECK(skinned_mesh_capture_test_write_census_events().back().sequence == ordered[11].sequence);
		tracker.buffer_driver_id.id = 200;
		RenderCaptureTestAccess::record_writable_set(&rd, rid, RenderCaptureTestHooks::COMPUTE);
		CHECK(skinned_mesh_capture_test_hook_is_poisoned());
		CHECK(skinned_mesh_capture_test_write_census_events().size() == 11);
		for (auto usage : { RenderingDeviceGraph::RESOURCE_USAGE_RESOLVE_TO, RenderingDeviceGraph::RESOURCE_USAGE_MAX }) {
			skinned_mesh_capture_test_hook_reset();
			skinned_mesh_capture_test_hook_begin(12);
			RenderCaptureTestAccess::record_buffer_usage(&rd, 100, usage, RenderCaptureTestHooks::CALLBACK);
			CHECK(skinned_mesh_capture_test_hook_is_poisoned());
			CHECK(skinned_mesh_capture_test_write_census_events().empty());
		}
		skinned_mesh_capture_test_hook_reset();
		skinned_mesh_capture_test_hook_begin(13);
		RenderCaptureTestAccess::record_buffer_usage(&rd, 100, RenderingDeviceGraph::RESOURCE_USAGE_COPY_FROM, RenderCaptureTestHooks::CALLBACK);
		CHECK(skinned_mesh_capture_test_write_census_events().empty());
		RenderCaptureTestAccess::record_buffer_usage(&rd, 100, RenderingDeviceGraph::RESOURCE_USAGE_GENERAL, RenderCaptureTestHooks::CALLBACK);
		CHECK(skinned_mesh_capture_test_write_census_events().size() == 1);
		CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
		RenderCaptureTestAccess::uniform_set_owner(&rd).free(rid);
		skinned_mesh_capture_test_hook_reset();
		RenderCaptureTestAccess::record_writable_set(&rd, RID(), RenderCaptureTestHooks::COMPUTE);
		CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
	}
};
TEST_CASE("[P01] Actual RD writable scanner records repeated uses and rejects unknown allocation") {
	SkinnedMeshCaptureTestWriteCensusTestAccess::check_scanner();
}

class SkinnedMeshCaptureTestRDPoolTestAccess {
public:
	static void check_buffer_qualification() {
		RenderingDevice device;
		SkinnedMeshCaptureTestRDPool pool;
		pool.rd = &device;
		RenderCaptureTestAccess::Buffer buffer;
		buffer.driver_id.id = 701;
		buffer.size = 64;
		const RID rid = RenderCaptureTestAccess::storage_buffer_owner(&device).make_rid(buffer);
		auto *actual = RenderCaptureTestAccess::storage_buffer_owner(&device).get_or_null(rid);
		actual->capture_test_allocation_serial = RenderCaptureTestAccess::capture_test_allocation_registry(&device).allocate(701, rid.get_id());
		REQUIRE(actual->capture_test_allocation_serial != 0);
		CHECK(pool.qualify_buffer(rid) == OK);
		skinned_mesh_capture_test_hook_reset();
		skinned_mesh_capture_test_hook_begin(998);
		uint64_t sequence = 0, epoch = 0;
		CHECK(pool.record_boundary(rid, 0, sequence, epoch) == ERR_INVALID_DATA);
		SkinnedMeshCaptureTestRecordingScope operation;
		RenderCaptureTestHooks::observe_buffer_copy(3, rid.get_id(), 0, 0, 36, uint64_t(device.get_instance_id()), 8, actual->capture_test_allocation_serial);
		CHECK(pool.record_boundary(rid, operation.identity(), sequence, epoch) == OK);
		CHECK(sequence == 1);
		CHECK(epoch == 998);
		CHECK(pool.record_boundary(rid, operation.identity() + 1, sequence, epoch) == ERR_INVALID_DATA);
		{
			SkinnedMeshCaptureTestRecordingScope unrelated;
			std::thread publisher([&] { RenderCaptureTestHooks::observe_buffer_copy(3, rid.get_id(), 0, 0, 36, uint64_t(device.get_instance_id()), 8, actual->capture_test_allocation_serial); });
			publisher.join();
			CHECK(pool.record_boundary(rid, unrelated.identity(), sequence, epoch) == ERR_INVALID_DATA);
		}
		skinned_mesh_capture_test_hook_reset();

		pool.capacity = 1;
		SkinnedMeshCaptureTestReadbackCollector original(1, 2, false), replacement(1, 2, false);
		const SkinnedMeshCaptureTestReadbackIdentity snapshot_id{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, false };
		auto consumer_id = snapshot_id;
		consumer_id.consumer = true;
		consumer_id.token++;
		consumer_id.queue++;
		Callable pending_snapshot, pending_consumer, substitute_snapshot, substitute_consumer;
		REQUIRE(original.reserve(snapshot_id, pending_snapshot) == OK);
		REQUIRE(original.reserve(consumer_id, pending_consumer) == OK);
		REQUIRE(original.mark_enqueued(snapshot_id) == OK);
		REQUIRE(original.mark_enqueued(consumer_id) == OK);
		REQUIRE(replacement.reserve(snapshot_id, substitute_snapshot) == OK);
		REQUIRE(replacement.reserve(consumer_id, substitute_consumer) == OK);
		REQUIRE(replacement.mark_enqueued(snapshot_id) == OK);
		REQUIRE(replacement.mark_enqueued(consumer_id) == OK);
		substitute_snapshot.call(skinned_mesh_capture_test_initial_guarded_buffer());
		substitute_consumer.call(skinned_mesh_capture_test_initial_guarded_buffer());
		CHECK(replacement.validate_pair(snapshot_id, consumer_id) == OK);
		CHECK(original.validate_pair(snapshot_id, consumer_id) == ERR_BUSY);
		pool.queued_collector = std::make_unique<SkinnedMeshCaptureTestReadbackCollector>(original);
		pool.slots[0].readback_attempted[0] = true;
		pool.slots[0].readback_attempted[1] = true;
		CHECK(pool.qualify_collector(0, replacement) == ERR_INVALID_DATA);
		CHECK(pool.qualify_collector(0, original) == OK);
		pool.slots[0].readback_attempted[1] = false;
		CHECK(pool.qualify_collector(0, original) == ERR_INVALID_DATA);

		pool.slots[0].snapshot = rid;
		SkinnedMeshCaptureTestReadbackIdentity identity;
		identity.device = uint64_t(device.get_instance_id());
		identity.rid = rid.get_id();
		identity.allocation = actual->capture_test_allocation_serial;
		CHECK(pool.qualify_readback(identity) == OK);
		SkinnedMeshCaptureTestReadbackIdentity resolved;
		pool.slots[0].transported = true;
		pool.slots[0].observation_epoch = 2;
		CHECK(pool.resolve_readback_identity(0, false, 1, 2, 3, resolved) == OK);
		CHECK(resolved.run == 1);
		CHECK(resolved.epoch == 2);
		CHECK(resolved.token == 6);
		CHECK(resolved.queue == 3);
		CHECK(resolved.device == uint64_t(device.get_instance_id()));
		CHECK(resolved.rid == rid.get_id());
		CHECK(resolved.owner == rid.get_id());
		CHECK(resolved.alias == 701);
		CHECK(resolved.allocation == actual->capture_test_allocation_serial);
		const auto saved = resolved;
		CHECK(pool.resolve_readback_identity(0, false, 1, 2, UINT64_MAX, resolved) == ERR_INVALID_PARAMETER);
		CHECK(resolved == saved);
		CHECK(pool.resolve_readback_identity(0, false, 1, 3, 3, resolved) == ERR_INVALID_DATA);
		CHECK(pool.resolve_readback_identity(0, true, 1, 2, 3, resolved) == ERR_INVALID_DATA);

		identity.allocation++;
		CHECK(pool.qualify_readback(identity) == ERR_INVALID_DATA);
		identity.allocation--;
		identity.device++;
		CHECK(pool.qualify_readback(identity) == ERR_INVALID_DATA);
		identity.device--;
		identity.consumer = true;
		CHECK(pool.qualify_readback(identity) == ERR_INVALID_DATA);

		CHECK(pool.qualify_buffer(RID()) == ERR_INVALID_DATA);
		CHECK(RenderCaptureTestAccess::capture_test_allocation_registry(&device).mark_access_escaped(701, actual->capture_test_allocation_serial));
		CHECK(pool.qualify_buffer(rid) == ERR_UNAVAILABLE);
		RenderCaptureTestAccess::storage_buffer_owner(&device).free(rid);
		pool.rd = nullptr;
	}
};
TEST_CASE("[P01] Pool qualification rejects escaped actual allocations") {
	SkinnedMeshCaptureTestRDPoolTestAccess::check_buffer_qualification();
}
