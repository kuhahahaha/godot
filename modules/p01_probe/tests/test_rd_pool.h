#pragma once
#include "modules/p01_probe/p01_rd_pool.h"
#include "modules/p01_probe/p01_gpu_payload.h"
#include <thread>
#include "tests/test_macros.h"

namespace TestP01RDPool {
TEST_CASE("[P01] Main RD pool rejects absent device without admitting recordings") {
	// doctest uses the dummy renderer. This exercises rejection only, never GPU.
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	P01RDPool pool;
	CHECK(pool.physical_retirement_status() == ERR_UNCONFIGURED);
	CHECK(pool.initialize(false) == ERR_UNAVAILABLE);
	CHECK(pool.initialize(true) == ERR_UNAVAILABLE);
	CHECK(pool.record_public(0, P01PublicSnapshot()) == ERR_UNCONFIGURED);
	CHECK(pool.record_native(0, RID(), 0) == ERR_UNCONFIGURED);
	P01ReadbackCollector collector(1, 2, false);
	CHECK(pool.enqueue_readback(P01ReadbackIdentity(), collector) == ERR_UNCONFIGURED);
	CHECK(pool.release() == OK);
	CHECK(pool.release() == OK);
}
}

#include "modules/p01_probe/p01_write_census.h"
#include "modules/p01_probe/p01_observer.h"
class P01WriteCensusTestAccess {
public:
	static void check_scanner() {
		RenderingDevice rd;
		RenderingDeviceGraph::ResourceTracker tracker;
		tracker.buffer_driver_id.id = 100;
		const auto serial = rd.p01_physical_registry.allocate(100, 101);
		REQUIRE(serial != 0);
		RenderingDevice::UniformSet set;
		set.draw_trackers.push_back(&tracker);
		set.draw_trackers_usage.push_back(RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		const RID rid = rd.uniform_set_owner.make_rid(set);
		p01_hook_reset(); p01_hook_begin(11);
		rd._p01_record_writable_set(rid, P01_COMPUTE);
		CHECK(p01_write_census_events().empty());
		rd.uniform_set_owner.get_or_null(rid)->draw_trackers_usage.write[0] = RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE;
		for (uint32_t operation : { P01_DRAW, P01_DRAW_INDIRECT, P01_COMPUTE, P01_COMPUTE_INDIRECT, P01_RAYTRACE }) {
			rd._p01_record_writable_set(rid, operation);
			rd._p01_record_writable_set(rid, operation);
		}
		const auto events = p01_write_census_events();
		CHECK(events.size() == 10);
		for (const auto &event : events) {
			CHECK(event.allocation == serial);
			CHECK(event.device == uint64_t(rd.get_instance_id()));
			CHECK(event.driver == 100);
			CHECK(event.epoch == 11);
		}
		CHECK_FALSE(p01_hook_is_poisoned());
		p01_observe_buffer_copy(1, 2, 0, 0, 36, uint64_t(rd.get_instance_id()), serial, serial);
		rd._p01_record_writable_set(rid, P01_COMPUTE);
		const auto ordered = p01_hook_events();
		REQUIRE(ordered.size() == 12);
		CHECK(ordered[10].kind == 7);
		CHECK(ordered[11].sequence == ordered[10].sequence + 1);
		CHECK(p01_write_census_events().back().sequence == ordered[11].sequence);
		tracker.buffer_driver_id.id = 200;
		rd._p01_record_writable_set(rid, P01_COMPUTE);
		CHECK(p01_hook_is_poisoned());
		CHECK(p01_write_census_events().size() == 11);
		for (auto usage : { RenderingDeviceGraph::RESOURCE_USAGE_RESOLVE_TO, RenderingDeviceGraph::RESOURCE_USAGE_MAX }) {
			p01_hook_reset(); p01_hook_begin(12);
			rd._p01_record_buffer_usage(100, usage, P01_CALLBACK);
			CHECK(p01_hook_is_poisoned());
			CHECK(p01_write_census_events().empty());
		}
		p01_hook_reset(); p01_hook_begin(13);
		rd._p01_record_buffer_usage(100, RenderingDeviceGraph::RESOURCE_USAGE_COPY_FROM, P01_CALLBACK);
		CHECK(p01_write_census_events().empty());
		rd._p01_record_buffer_usage(100, RenderingDeviceGraph::RESOURCE_USAGE_GENERAL, P01_CALLBACK);
		CHECK(p01_write_census_events().size() == 1);
		CHECK_FALSE(p01_hook_is_poisoned());
		rd.uniform_set_owner.free(rid);
		p01_hook_reset();
		rd._p01_record_writable_set(RID(), P01_COMPUTE);
		CHECK_FALSE(p01_hook_is_poisoned());
	}
};
TEST_CASE("[P01] Actual RD writable scanner records repeated uses and rejects unknown allocation") {
	P01WriteCensusTestAccess::check_scanner();
}

class P01RDPoolTestAccess {
public:
	static void check_buffer_qualification() {
		RenderingDevice device;
		P01RDPool pool;
		pool.rd = &device;
		RenderingDevice::Buffer buffer;
		buffer.driver_id.id = 701;
		buffer.size = 64;
		const RID rid = device.storage_buffer_owner.make_rid(buffer);
		auto *actual = device.storage_buffer_owner.get_or_null(rid);
		actual->p01_allocation_serial = device.p01_physical_registry.allocate(701, rid.get_id());
		REQUIRE(actual->p01_allocation_serial != 0);
		CHECK(pool.qualify_buffer(rid) == OK);
		p01_hook_reset(); p01_hook_begin(998);
		uint64_t sequence = 0, epoch = 0;
		CHECK(pool.record_boundary(rid, 0, sequence, epoch) == ERR_INVALID_DATA);
		P01RecordingScope operation;
		p01_observe_buffer_copy(3, rid.get_id(), 0, 0, 36, uint64_t(device.get_instance_id()), 8, actual->p01_allocation_serial);
		CHECK(pool.record_boundary(rid, operation.identity(), sequence, epoch) == OK);
		CHECK(sequence == 1); CHECK(epoch == 998);
		CHECK(pool.record_boundary(rid, operation.identity() + 1, sequence, epoch) == ERR_INVALID_DATA);
		{
			P01RecordingScope unrelated;
			std::thread publisher([&] { p01_observe_buffer_copy(3, rid.get_id(), 0, 0, 36, uint64_t(device.get_instance_id()), 8, actual->p01_allocation_serial); });
			publisher.join();
			CHECK(pool.record_boundary(rid, unrelated.identity(), sequence, epoch) == ERR_INVALID_DATA);
		}
		p01_hook_reset();

		pool.capacity = 1;
		P01ReadbackCollector original(1, 2, false), replacement(1, 2, false);
		const P01ReadbackIdentity snapshot_id{1, 2, 3, 4, 5, 6, 7, 8, 9, 0, false};
		auto consumer_id = snapshot_id; consumer_id.consumer = true; consumer_id.token++; consumer_id.queue++;
		Callable pending_snapshot, pending_consumer, substitute_snapshot, substitute_consumer;
		REQUIRE(original.reserve(snapshot_id, pending_snapshot) == OK);
		REQUIRE(original.reserve(consumer_id, pending_consumer) == OK);
		REQUIRE(original.mark_enqueued(snapshot_id) == OK);
		REQUIRE(original.mark_enqueued(consumer_id) == OK);
		REQUIRE(replacement.reserve(snapshot_id, substitute_snapshot) == OK);
		REQUIRE(replacement.reserve(consumer_id, substitute_consumer) == OK);
		REQUIRE(replacement.mark_enqueued(snapshot_id) == OK);
		REQUIRE(replacement.mark_enqueued(consumer_id) == OK);
		substitute_snapshot.call(p01_initial_guarded_buffer());
		substitute_consumer.call(p01_initial_guarded_buffer());
		CHECK(replacement.validate_pair(snapshot_id, consumer_id) == OK);
		CHECK(original.validate_pair(snapshot_id, consumer_id) == ERR_BUSY);
		pool.queued_collector = std::make_unique<P01ReadbackCollector>(original);
		pool.slots[0].readback_attempted[0] = true;
		pool.slots[0].readback_attempted[1] = true;
		CHECK(pool.qualify_collector(0, replacement) == ERR_INVALID_DATA);
		CHECK(pool.qualify_collector(0, original) == OK);
		pool.slots[0].readback_attempted[1] = false;
		CHECK(pool.qualify_collector(0, original) == ERR_INVALID_DATA);

		pool.slots[0].snapshot = rid;
		P01ReadbackIdentity identity;
		identity.device = uint64_t(device.get_instance_id());
		identity.rid = rid.get_id(); identity.allocation = actual->p01_allocation_serial;
		CHECK(pool.qualify_readback(identity) == OK);
		P01ReadbackIdentity resolved;
		pool.slots[0].transported = true;
		pool.slots[0].observation_epoch = 2;
		CHECK(pool.resolve_readback_identity(0, false, 1, 2, 3, resolved) == OK);
		CHECK(resolved.run == 1); CHECK(resolved.epoch == 2); CHECK(resolved.token == 6); CHECK(resolved.queue == 3);
		CHECK(resolved.device == uint64_t(device.get_instance_id()));
		CHECK(resolved.rid == rid.get_id()); CHECK(resolved.owner == rid.get_id());
		CHECK(resolved.alias == 701); CHECK(resolved.allocation == actual->p01_allocation_serial);
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
		CHECK(device.p01_physical_registry.mark_access_escaped(701, actual->p01_allocation_serial));
		CHECK(pool.qualify_buffer(rid) == ERR_UNAVAILABLE);
		device.storage_buffer_owner.free(rid);
		pool.rd = nullptr;
	}
};
TEST_CASE("[P01] Pool qualification rejects escaped actual allocations") {
	P01RDPoolTestAccess::check_buffer_qualification();
}
