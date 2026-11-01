/**************************************************************************/
/*  test_graph_dependency_scratch.cpp                                     */
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

#include "core/os/memory_allocation_test_hooks.h"
#include "servers/rendering/rendering_device_graph.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_graph_dependency_scratch)

namespace TestGraphDependencyScratch {

using Result = RenderingDeviceGraph::DependencyPathResult;
using Scratch = RenderingDeviceGraph::DependencyPathScratch;

struct Fixture {
#ifndef GRAPH_SCRATCH_STANDALONE
	TransferTestDriverBase driver;
#endif
	RenderingDeviceGraph graph;
	RenderingDeviceGraph::ResourceTracker destination;
	RenderingDeviceGraph::RecordedBufferCopy copy;

	Fixture() {
#ifdef GRAPH_SCRATCH_STANDALONE
		// CPU fixture only: establish the frame slot without an Object/driver host.
		graph.frames.resize(1);
#else
		graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
#endif
		graph.begin();
		destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
		copy.source = RenderingDeviceDriver::BufferID(71);
		copy.region = { 0, 0, 32 };
	}

	RenderingDeviceGraph::RecordHandle append() {
		return graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	}
};

TEST_CASE("[R02][GraphScratch] exact preallocated storage queries the original dependency") {
	Fixture fixture;
	const auto first = fixture.append();
	const auto middle = fixture.append();
	const auto last = fixture.append();
	REQUIRE(fixture.graph.qualification_command_count() == 3);
	uint32_t distances[5] = { 101, 0, 0, 0, 102 };
	uint32_t pending[5] = { 201, 0, 0, 0, 202 };
	Scratch scratch{ distances + 1, pending + 1, 3 };
	uint32_t length = 99;
	const bool previous_observation = MemoryAllocationTest::enabled;
	const uint64_t previous_attempts = MemoryAllocationTest::attempts;
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const Result outcome = fixture.graph.query_dependency_path(first, last, scratch, length);
	const uint64_t attempts = MemoryAllocationTest::attempts;
	MemoryAllocationTest::enabled = previous_observation;
	MemoryAllocationTest::attempts = previous_attempts;
	CHECK(outcome == Result::Found);
	CHECK(attempts == 0);
	CHECK(length == 2);
	uint32_t legacy_length = 0;
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const bool legacy_found = fixture.graph.has_dependency_path(first, last, legacy_length);
	const uint64_t legacy_attempts = MemoryAllocationTest::attempts;
	MemoryAllocationTest::enabled = previous_observation;
	MemoryAllocationTest::attempts = previous_attempts;
	CHECK(legacy_found);
	CHECK(legacy_attempts > 0); // Positive control for the allocation observer.
	CHECK(legacy_length == length);
	CHECK(fixture.graph.query_dependency_path(first, middle, scratch, length) == Result::Found);
	CHECK(length == 1);
	CHECK(distances[0] == 101);
	CHECK(distances[4] == 102);
	CHECK(pending[0] == 201);
	CHECK(pending[4] == 202);
}

TEST_CASE("[R02][GraphScratch] absent capacity refuses without touching admitted storage") {
	Fixture fixture;
	const auto first = fixture.append();
	const auto second = fixture.append();
	uint32_t distances[3] = { 11, 12, 13 };
	uint32_t pending[3] = { 21, 22, 23 };
	uint32_t length = 99;
	Scratch scratch{ distances, pending, 1 };
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, length) == Result::InsufficientScratch);
	CHECK(length == 0);
	CHECK(distances[0] == 11);
	CHECK(pending[0] == 21);
	scratch.capacity = 2;
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, length) == Result::Found);
	CHECK(length == 1);
	const auto third = fixture.append();
	distances[0] = 31;
	pending[0] = 41;
	CHECK(fixture.graph.query_dependency_path(first, third, scratch, length) == Result::InsufficientScratch);
	CHECK(length == 0);
	CHECK(distances[0] == 31);
	CHECK(pending[0] == 41);
}

TEST_CASE("[R02][GraphScratch] invalid identity and storage cannot become ordering facts") {
	Fixture fixture;
	const auto first = fixture.append();
	const auto second = fixture.append();
	uint32_t distances[4] = {};
	uint32_t pending[4] = {};
	Scratch scratch{ distances, pending, 4 };
	uint32_t length = 99;
	auto foreign = first;
	foreign.graph_instance++;
	CHECK(fixture.graph.query_dependency_path(foreign, second, scratch, length) == Result::Invalid);
	CHECK(length == 0);
	CHECK(fixture.graph.query_dependency_path(second, first, scratch, length) == Result::NotFound);
	CHECK(length == 0);
	CHECK(fixture.graph.query_dependency_path(first, first, scratch, length) == Result::NotFound);
	CHECK(length == 0);
	Scratch missing{ nullptr, pending, 4 };
	CHECK(fixture.graph.query_dependency_path(first, second, missing, length) == Result::Invalid);
	Scratch overlap{ distances, distances + 1, 4 };
	CHECK(fixture.graph.query_dependency_path(first, second, overlap, length) == Result::Invalid);
	alignas(uint32_t) uint8_t misaligned[20] = {};
	Scratch unaligned{ reinterpret_cast<uint32_t *>(misaligned + 1), pending, 4 };
	CHECK(fixture.graph.query_dependency_path(first, second, unaligned, length) == Result::Invalid);
	fixture.graph.begin();
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, length) == Result::Invalid);
	CHECK(length == 0);
}

TEST_CASE("[R02][GraphScratch] command index order alone is not a dependency") {
	Fixture fixture;
	const auto first = fixture.append();
	RenderingDeviceGraph::ResourceTracker independent;
	independent.buffer_driver_id = RenderingDeviceDriver::BufferID(73);
	const auto second = fixture.graph.add_buffer_update(RenderingDeviceDriver::BufferID(73), &independent, VectorView(&fixture.copy, 1));
	uint32_t distances[2] = {};
	uint32_t pending[2] = {};
	Scratch scratch{ distances, pending, 2 };
	uint32_t length = 99;
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, length) == Result::NotFound);
	CHECK(length == 0);
}

TEST_CASE("[R02][GraphScratch] result scalar must not overlap workspace") {
	Fixture fixture;
	const auto first = fixture.append();
	const auto second = fixture.append();
	uint32_t distances[2] = {};
	uint32_t pending[2] = {};
	Scratch scratch{ distances, pending, 2 };
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, distances[0]) == Result::Invalid);
	CHECK(distances[0] == 0);
	CHECK(fixture.graph.query_dependency_path(first, second, scratch, pending[1]) == Result::Invalid);
	CHECK(pending[1] == 0);
}

} // namespace TestGraphDependencyScratch
