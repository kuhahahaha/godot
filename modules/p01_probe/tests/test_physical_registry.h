#pragma once
#include "modules/p01_probe/p01_physical_registry.h"
#include "tests/test_macros.h"
#include <thread>
namespace TestP01PhysicalRegistry {
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
	CHECK(registry.allocate(10, 22) == 0);
	CHECK(registry.dispose(10, serial));
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
