#pragma once
#include "modules/p01_probe/p01_rd_pool.h"
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
