#pragma once
#include "modules/p01_probe/p01_physical_registry.h"
#include "tests/test_macros.h"
#include <thread>
namespace TestP01PhysicalRegistry {
TEST_CASE("[P01] Device native escape covers existing future and reused allocations") {
	P01PhysicalRegistry registry;
	const auto first = registry.allocate(10, 20);
	REQUIRE(first != 0);
	CHECK_FALSE(registry.access_escaped(10, first));
	registry.mark_device_access_escaped();
	CHECK(registry.access_escaped(10, first));
	CHECK(registry.release_owner(10, first, 20));
	CHECK(registry.dispose(10, first));
	const auto reused = registry.allocate(10, 21);
	REQUIRE(reused > first);
	CHECK(registry.access_escaped(10, reused));
	const auto future = registry.allocate(11, 22);
	REQUIRE(future != 0);
	CHECK(registry.access_escaped(11, future));
	P01PhysicalRegistry fresh_device;
	const auto clean = fresh_device.allocate(10, 20);
	REQUIRE(clean != 0);
	CHECK_FALSE(fresh_device.access_escaped(10, clean));
}
TEST_CASE("[P01] Physical allocation outlives owners and reused handles reject stale disposal") {
	P01PhysicalRegistry registry;
	const uint64_t serial = registry.allocate(10, 20);
	CHECK(serial != 0);
	CHECK(registry.add_alias(10, serial, 21));
	CHECK(registry.resolve(10, 21) == serial);
	CHECK_FALSE(registry.dispose(10, serial));
	CHECK(registry.release_owner(10, serial, 20));
	CHECK(registry.release_owner(10, serial, 21));
	CHECK(registry.resolve(10, 20) == 0);
	CHECK(registry.physically_live(10, serial));
	CHECK(registry.resolve_driver(10) == serial);
	CHECK(registry.allocate(10, 22) == 0);
	CHECK(registry.dispose(10, serial));
	CHECK(registry.resolve_driver(10) == 0);
	const uint64_t replacement = registry.allocate(10, 22);
	CHECK(replacement > serial);
	CHECK_FALSE(registry.dispose(10, serial));
	CHECK(registry.resolve(10, 22) == replacement);
}
TEST_CASE("[P01] Physical registry owner and allocation exhaustion preserve existing identities") {
	P01PhysicalRegistry registry;
	const auto serial = registry.allocate(10, 20);
	CHECK(serial != 0);
	for (uint64_t rid = 21; rid < 28; rid++) { CHECK(registry.add_alias(10, serial, rid)); }
	CHECK_FALSE(registry.add_alias(10, serial, 28));
	CHECK_FALSE(registry.add_alias(10, serial, 20));
	for (uint64_t rid = 20; rid < 28; rid++) { CHECK(registry.release_owner(10, serial, rid)); }
	CHECK(registry.dispose(10, serial));
	for (uint64_t i = 1; i <= 128; i++) {
		const auto allocated = registry.allocate(i, i + 1000);
		CHECK(allocated != 0);
		CHECK(registry.release_owner(i, allocated, i + 1000));
		CHECK(registry.physically_live(i, allocated));
	}
	CHECK(registry.allocate(129, 2000) == 0);
}
TEST_CASE("[P01] Concurrent physical allocations preserve unique serials and live mappings") {
	P01PhysicalRegistry registry;
	std::array<uint64_t, 64> serials{};
	std::array<std::thread, 4> workers;
	for (int worker = 0; worker < 4; worker++) {
		workers[worker] = std::thread([&, worker]() {
			for (int i = worker; i < 64; i += 4) { serials[i] = registry.allocate(i + 1, i + 1000); }
		});
	}
	for (auto &worker : workers) { worker.join(); }
	for (int i = 0; i < 64; i++) {
		CHECK(serials[i] != 0);
		CHECK(registry.resolve(i + 1, i + 1000) == serials[i]);
		for (int j = 0; j < i; j++) { CHECK(serials[i] != serials[j]); }
	}
}

TEST_CASE("[P01] Escaped native access persists until exact physical disposal") {
	P01PhysicalRegistry registry;
	CHECK(registry.access_escaped(10, 0));
	const auto serial = registry.allocate(10, 20);
	REQUIRE(serial != 0);
	CHECK_FALSE(registry.access_escaped(10, serial));
	CHECK_FALSE(registry.mark_access_escaped(10, serial + 1));
	CHECK(registry.add_alias(10, serial, 21));
	CHECK(registry.mark_access_escaped(10, serial));
	CHECK(registry.access_escaped(10, registry.resolve(10, 21)));
	CHECK(registry.release_owner(10, serial, 20));
	CHECK(registry.release_owner(10, serial, 21));
	CHECK(registry.access_escaped(10, serial));
	CHECK(registry.dispose(10, serial));
	CHECK(registry.access_escaped(10, serial));
	const auto replacement = registry.allocate(10, 22);
	REQUIRE(replacement > serial);
	CHECK_FALSE(registry.mark_access_escaped(10, serial));
	CHECK_FALSE(registry.access_escaped(10, replacement));
}

}
#include "modules/p01_probe/p01_write_census.h"
#include "modules/p01_probe/p01_observer.h"
namespace TestP01WriteCensus {
TEST_CASE("[P01] Shared writable event log rejects unknown identities and bounds events") {
	p01_hook_reset();
	p01_write_census_record(1, 2, 3, 11, P01_DRAW);
	CHECK_FALSE(p01_hook_is_poisoned());
	CHECK(p01_hook_events().empty());
	p01_hook_begin(1);
	p01_write_census_record(0, 2, 3, 11, P01_DRAW);
	CHECK(p01_hook_is_poisoned());
	p01_write_census_record(1, 2, 3, 11, P01_DRAW);
	CHECK(p01_hook_events().empty());
	p01_hook_reset(); p01_hook_begin(2);
	for (uint32_t i = 0; i < 4096; i++) p01_write_census_record(1, 2, 3, 11, P01_DRAW);
	CHECK_FALSE(p01_hook_is_poisoned());
	const auto events = p01_write_census_events();
	REQUIRE(events.size() == 4096);
	CHECK(events.back().sequence == 4096);
	CHECK(events.back().epoch == 2);
	p01_write_census_record(1, 2, 3, 11, P01_DRAW);
	CHECK(p01_hook_is_poisoned());
	CHECK(p01_hook_events().size() == 4096);
	p01_hook_reset();
}
}

TEST_CASE("[P01] Pool native retirement requires each actual free and preserves handle reuse") {
	P01PoolResourceRegistry registry;
	const auto shader = registry.track(P01PoolResourceKind::Shader, 11);
	const auto pipeline = registry.track(P01PoolResourceKind::ComputePipeline, 11);
	const auto set = registry.track(P01PoolResourceKind::UniformSet, 12);
	REQUIRE(shader != 0); REQUIRE(pipeline != 0); REQUIRE(set != 0);
	CHECK(shader != pipeline);
	CHECK(registry.track(P01PoolResourceKind::Shader, 11) == 0);
	CHECK_FALSE(registry.acknowledge(shader));
	registry.observe_free(P01PoolResourceKind::ComputePipeline, 11);
	CHECK(registry.status(shader) == P01PoolResourceRegistry::Status::Live);
	CHECK(registry.status(pipeline) == P01PoolResourceRegistry::Status::Released);
	registry.observe_free(P01PoolResourceKind::Shader, 11);
	const auto reused = registry.track(P01PoolResourceKind::Shader, 11);
	CHECK(reused != shader); CHECK(reused != 0);
	CHECK(registry.acknowledge(shader));
	CHECK(registry.status(reused) == P01PoolResourceRegistry::Status::Live);
	CHECK(registry.status(set) == P01PoolResourceRegistry::Status::Live);
	CHECK(registry.status(shader) == P01PoolResourceRegistry::Status::Unknown);
}
TEST_CASE("[P01] Pool resource exhaustion retains released evidence until acknowledgment") {
	P01PoolResourceRegistry registry;
	std::array<uint64_t, 128> tokens;
	for (uint64_t i = 0; i < tokens.size(); ++i) {
		tokens[i] = registry.track(P01PoolResourceKind::UniformSet, i + 1);
		REQUIRE(tokens[i] != 0);
	}
	CHECK(registry.track(P01PoolResourceKind::Shader, 200) == 0);
	CHECK(registry.observe_free(P01PoolResourceKind::UniformSet, 1));
	CHECK(registry.track(P01PoolResourceKind::Shader, 200) == 0);
	CHECK(registry.status(tokens[0]) == P01PoolResourceRegistry::Status::Released);
	CHECK(registry.acknowledge(tokens[0]));
	const auto next = registry.track(P01PoolResourceKind::Shader, 200);
	CHECK(next > tokens.back());
	CHECK_FALSE(registry.acknowledge(tokens[0]));
	CHECK(registry.status(next) == P01PoolResourceRegistry::Status::Live);
}
