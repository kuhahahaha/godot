/**************************************************************************/
/*  test_physical_registry.h                                              */
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
#pragma once
#include "servers/rendering/render_capture_test_tracking.h"
#include "tests/test_macros.h"

#include <thread>
namespace TestSkinnedMeshCaptureTestPhysicalRegistry {
TEST_CASE("[P01] Device native escape covers existing future and reused allocations") {
	RenderCaptureTestAllocationRegistry registry;
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
	RenderCaptureTestAllocationRegistry fresh_device;
	const auto clean = fresh_device.allocate(10, 20);
	REQUIRE(clean != 0);
	CHECK_FALSE(fresh_device.access_escaped(10, clean));
}
TEST_CASE("[P01] Physical allocation outlives owners and reused handles reject stale disposal") {
	RenderCaptureTestAllocationRegistry registry;
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
	RenderCaptureTestAllocationRegistry registry;
	const auto serial = registry.allocate(10, 20);
	CHECK(serial != 0);
	for (uint64_t rid = 21; rid < 28; rid++) {
		CHECK(registry.add_alias(10, serial, rid));
	}
	CHECK_FALSE(registry.add_alias(10, serial, 28));
	CHECK_FALSE(registry.add_alias(10, serial, 20));
	for (uint64_t rid = 20; rid < 28; rid++) {
		CHECK(registry.release_owner(10, serial, rid));
	}
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
	RenderCaptureTestAllocationRegistry registry;
	std::array<uint64_t, 64> serials{};
	std::array<std::thread, 4> workers;
	for (int worker = 0; worker < 4; worker++) {
		workers[worker] = std::thread([&, worker]() {
			for (int i = worker; i < 64; i += 4) {
				serials[i] = registry.allocate(i + 1, i + 1000);
			}
		});
	}
	for (auto &worker : workers) {
		worker.join();
	}
	for (int i = 0; i < 64; i++) {
		CHECK(serials[i] != 0);
		CHECK(registry.resolve(i + 1, i + 1000) == serials[i]);
		for (int j = 0; j < i; j++) {
			CHECK(serials[i] != serials[j]);
		}
	}
}

TEST_CASE("[P01] Escaped native access persists until exact physical disposal") {
	RenderCaptureTestAllocationRegistry registry;
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

} //namespace TestSkinnedMeshCaptureTestPhysicalRegistry
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_write_census.h"
namespace TestSkinnedMeshCaptureTestWriteCensus {
TEST_CASE("[P01] Shared writable event log rejects unknown identities and bounds events") {
	skinned_mesh_capture_test_hook_reset();
	RenderCaptureTestHooks::write_census_record(1, 2, 3, 11, RenderCaptureTestHooks::DRAW);
	CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
	CHECK(skinned_mesh_capture_test_hook_events().empty());
	skinned_mesh_capture_test_hook_begin(1);
	RenderCaptureTestHooks::write_census_record(0, 2, 3, 11, RenderCaptureTestHooks::DRAW);
	CHECK(skinned_mesh_capture_test_hook_is_poisoned());
	RenderCaptureTestHooks::write_census_record(1, 2, 3, 11, RenderCaptureTestHooks::DRAW);
	CHECK(skinned_mesh_capture_test_hook_events().empty());
	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(2);
	for (uint32_t i = 0; i < 4096; i++) {
		RenderCaptureTestHooks::write_census_record(1, 2, 3, 11, RenderCaptureTestHooks::DRAW);
	}
	CHECK_FALSE(skinned_mesh_capture_test_hook_is_poisoned());
	const auto events = skinned_mesh_capture_test_write_census_events();
	REQUIRE(events.size() == 4096);
	CHECK(events.back().sequence == 4096);
	CHECK(events.back().epoch == 2);
	RenderCaptureTestHooks::write_census_record(1, 2, 3, 11, RenderCaptureTestHooks::DRAW);
	CHECK(skinned_mesh_capture_test_hook_is_poisoned());
	CHECK(skinned_mesh_capture_test_hook_events().size() == 4096);
	skinned_mesh_capture_test_hook_reset();
}
} //namespace TestSkinnedMeshCaptureTestWriteCensus

TEST_CASE("[P01] Pool native retirement requires each actual free and preserves handle reuse") {
	RenderCaptureTestResourceRegistry registry;
	const auto shader = registry.track(RenderCaptureTestResourceKind::Shader, 11);
	const auto pipeline = registry.track(RenderCaptureTestResourceKind::ComputePipeline, 11);
	const auto set = registry.track(RenderCaptureTestResourceKind::UniformSet, 12);
	REQUIRE(shader != 0);
	REQUIRE(pipeline != 0);
	REQUIRE(set != 0);
	CHECK(shader != pipeline);
	CHECK(registry.track(RenderCaptureTestResourceKind::Shader, 11) == 0);
	CHECK_FALSE(registry.acknowledge(shader));
	registry.observe_free(RenderCaptureTestResourceKind::ComputePipeline, 11);
	CHECK(registry.status(shader) == RenderCaptureTestResourceRegistry::Status::Live);
	CHECK(registry.status(pipeline) == RenderCaptureTestResourceRegistry::Status::Released);
	CHECK_FALSE(registry.observe_free(RenderCaptureTestResourceKind::ComputePipeline, 11));
	registry.observe_free(RenderCaptureTestResourceKind::Shader, 11);
	const auto reused = registry.track(RenderCaptureTestResourceKind::Shader, 11);
	CHECK(reused != shader);
	CHECK(reused != 0);
	CHECK(registry.acknowledge(shader));
	CHECK(registry.status(reused) == RenderCaptureTestResourceRegistry::Status::Live);
	CHECK(registry.status(set) == RenderCaptureTestResourceRegistry::Status::Live);
	CHECK(registry.status(shader) == RenderCaptureTestResourceRegistry::Status::Unknown);
}
TEST_CASE("[P01] Pool resource exhaustion retains released evidence until acknowledgment") {
	RenderCaptureTestResourceRegistry registry;
	std::array<uint64_t, 128> tokens;
	for (uint64_t i = 0; i < tokens.size(); ++i) {
		tokens[i] = registry.track(RenderCaptureTestResourceKind::UniformSet, i + 1);
		REQUIRE(tokens[i] != 0);
	}
	CHECK(registry.track(RenderCaptureTestResourceKind::Shader, 200) == 0);
	CHECK(registry.observe_free(RenderCaptureTestResourceKind::UniformSet, 1));
	CHECK(registry.track(RenderCaptureTestResourceKind::Shader, 200) == 0);
	CHECK(registry.status(tokens[0]) == RenderCaptureTestResourceRegistry::Status::Released);
	CHECK(registry.acknowledge(tokens[0]));
	const auto next = registry.track(RenderCaptureTestResourceKind::Shader, 200);
	CHECK(next > tokens.back());
	CHECK_FALSE(registry.acknowledge(tokens[0]));
	CHECK(registry.status(next) == RenderCaptureTestResourceRegistry::Status::Live);
}

TEST_CASE("[P01] Image texture retirement cannot be satisfied by a shader with the same driver id") {
	RenderCaptureTestResourceRegistry registry;
	const auto texture_kind = static_cast<RenderCaptureTestResourceKind>(3);
	const auto texture = registry.track(texture_kind, 77);
	const auto shader = registry.track(RenderCaptureTestResourceKind::Shader, 77);
	REQUIRE(texture != 0);
	REQUIRE(shader != 0);
	CHECK(registry.observe_free(RenderCaptureTestResourceKind::Shader, 77));
	CHECK(registry.status(texture) == RenderCaptureTestResourceRegistry::Status::Live);
	CHECK(registry.observe_free(texture_kind, 77));
	CHECK(registry.status(texture) == RenderCaptureTestResourceRegistry::Status::Released);
}
