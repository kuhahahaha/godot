/**************************************************************************/
/*  test_graph_selected_buffer_source.cpp                               */
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

TEST_FORCE_LINK(test_graph_selected_buffer_source)

namespace TestGraphSelectedBufferSource {
using RDG = RenderingDeviceGraph;
using RDD = RenderingDeviceDriver;

struct Driver : TransferTestDriverBase {
	bool transitions = false;
	uint64_t api_trait_get(ApiTrait p_trait) override {
		return p_trait == API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS && transitions;
	}
};

struct Fixture {
	Driver driver;
	RDG graph;
	RDG::ResourceTracker buffer;
	RDG::ResourceTracker other;
	explicit Fixture(bool p_transitions = false) {
		driver.transitions = p_transitions;
		graph.initialize(&driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		graph.begin();
		buffer.buffer_driver_id = RDD::BufferID(71);
		other.buffer_driver_id = RDD::BufferID(72);
	}
	RDG::RecordHandle begin(RDG::ResourceTracker &p_tracker, RDG::ResourceUsage p_usage) {
		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&p_tracker, p_usage);
		return graph.add_compute_list_dispatch(1, 1, 1);
	}
	RDG::RecordHandle append(RDG::ResourceTracker &p_tracker, RDG::ResourceUsage p_usage) {
		begin(p_tracker, p_usage);
		return graph.add_compute_list_end();
	}
};

// RED: accepting any earlier writer, or substituting path reachability for the
// latest writer check, must fail the overwrite controls below.
TEST_CASE("[R02][SelectedBufferSource] original closed producer and readonly reader have an exact tracker edge") {
	Fixture f;
	const auto instruction = f.begin(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, instruction));
	const auto producer = f.graph.add_compute_list_end();
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, instruction));
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	const auto pending_reader = f.begin(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, producer, pending_reader));
	const auto reader = f.graph.add_compute_list_end();
	CHECK(f.graph.has_buffer_read_dependency(&f.buffer, producer, reader));
	CHECK(f.graph.has_buffer_read_dependency(&f.buffer, instruction, pending_reader));
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.other, producer, reader));
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, reader, producer));
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	CHECK(f.graph.qualification_command_count() == 2);
}

TEST_CASE("[R02][SelectedBufferSource] transitive dependency survives overwrite but selected contents do not") {
	Fixture f;
	const auto selected = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	RDG::ResourceTracker *alias = &f.buffer; // Another slot resolves the original Buffer's same tracker.
	const auto overwrite = f.append(*alias, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, selected));
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, overwrite));
	const auto reader = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	uint32_t distances[3] = {}, pending[3] = {}, length = 0;
	CHECK(f.graph.query_dependency_path(selected, reader, { distances, pending, 3 }, length) == RDG::DependencyPathResult::Found);
	CHECK(length == 2); // selected -> overwrite -> reader; this path is not freshness.
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, selected, reader));
	CHECK(f.graph.has_buffer_read_dependency(&f.buffer, overwrite, reader));
}

TEST_CASE("[R02][SelectedBufferSource] disjoint later copy and writable graph reader refuse conservatively") {
	Fixture f;
	const auto selected = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	RDG::RecordedBufferCopy copy;
	copy.source = RDD::BufferID(73);
	copy.region = { 0, 32, 4 }; // Even a write outside a selected [0,4) source invalidates whole-buffer freshness.
	f.graph.add_buffer_update(f.buffer.buffer_driver_id, &f.buffer, VectorView(&copy, 1));
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, selected));
	const auto reader = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, selected, reader));
	const auto new_source = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	const auto writable_reader = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, new_source, writable_reader));
}

TEST_CASE("[R02][SelectedBufferSource] global synchronization to unrelated reader is not a source read") {
	Fixture f;
	const auto producer = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	f.graph.add_synchronization();
	const auto unrelated = f.append(f.other, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	CHECK(f.graph.has_synchronization_edge(producer, unrelated));
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, producer, unrelated));
}

TEST_CASE("[R02][SelectedBufferSource] pending overwrite and ambiguous multi-dispatch lists cannot certify a source") {
	Fixture f;
	const auto producer = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	f.begin(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	f.graph.add_compute_list_dispatch(1, 1, 1);
	const auto ambiguous = f.graph.add_compute_list_end();
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, ambiguous));
	const auto reader = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	CHECK_FALSE(f.graph.has_buffer_read_dependency(&f.buffer, ambiguous, reader));
}

TEST_CASE("[R02][SelectedBufferSource] current graph identity canonical tracker and supported backend trait are required") {
	Fixture f;
	const auto producer = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	auto foreign = producer;
	++foreign.graph_instance;
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, foreign));
	CHECK_FALSE(f.graph.is_latest_buffer_writer(nullptr, producer));
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.other, producer));
	f.graph.begin();
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	const auto next = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, next));
	Fixture transitions(true);
	const auto transitioning = transitions.append(transitions.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(transitions.graph.is_latest_buffer_writer(&transitions.buffer, transitioning));
}

TEST_CASE("[R02][SelectedBufferSource] used wrong tracker malformed handles and dispatch cardinality refuse") {
	Fixture identity;
	const auto producer = identity.append(identity.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	identity.append(identity.other, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK_FALSE(identity.graph.is_latest_buffer_writer(&identity.other, producer));
	auto malformed = producer;
	malformed.kind = 0;
	CHECK_FALSE(identity.graph.is_latest_buffer_writer(&identity.buffer, malformed));
	malformed = producer;
	malformed.offset = 1;
	CHECK_FALSE(identity.graph.is_latest_buffer_writer(&identity.buffer, malformed));

	Fixture empty;
	empty.graph.add_compute_list_begin();
	empty.graph.add_compute_list_usage(&empty.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	const auto zero_dispatches = empty.graph.add_compute_list_end();
	CHECK_FALSE(empty.graph.is_latest_buffer_writer(&empty.buffer, zero_dispatches));

	Fixture multiple_reader;
	const auto selected = multiple_reader.append(multiple_reader.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	const auto reader_instruction = multiple_reader.begin(multiple_reader.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	multiple_reader.graph.add_compute_list_dispatch(1, 1, 1);
	const auto reader = multiple_reader.graph.add_compute_list_end();
	CHECK(multiple_reader.graph.is_latest_buffer_writer(&multiple_reader.buffer, selected));
	CHECK_FALSE(multiple_reader.graph.has_buffer_read_dependency(&multiple_reader.buffer, selected, reader));
	CHECK_FALSE(multiple_reader.graph.has_buffer_read_dependency(&multiple_reader.buffer, selected, reader_instruction));
}

TEST_CASE("[R02][SelectedBufferSource] opaque native callback refuses before reader effects") {
	Fixture f;
	const auto producer = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	CHECK(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	f.graph.add_driver_callback(+[](RDD *, RDD::CommandBufferID, void *) {}, nullptr, {}, {});
	CHECK_FALSE(f.graph.is_latest_buffer_writer(&f.buffer, producer));
	CHECK(f.graph.qualification_command_count() == 2);
}

TEST_CASE("[R02][SelectedBufferSource] observations allocate nothing and do not advance graph state") {
	Fixture f;
	const auto producer = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
	const auto reader = f.append(f.buffer, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	const bool saved_enabled = MemoryAllocationTest::enabled;
	const uint64_t saved_attempts = MemoryAllocationTest::attempts;
	MemoryAllocationTest::attempts = 0;
	MemoryAllocationTest::enabled = true;
	const bool fresh = f.graph.is_latest_buffer_writer(&f.buffer, producer);
	const bool edge = f.graph.has_buffer_read_dependency(&f.buffer, producer, reader);
	const uint64_t attempts = MemoryAllocationTest::attempts;
	MemoryAllocationTest::enabled = saved_enabled;
	MemoryAllocationTest::attempts = saved_attempts;
	CHECK(fresh);
	CHECK(edge);
	CHECK(attempts == 0);
	CHECK(f.graph.qualification_command_count() == 2);
	CHECK(f.buffer.command_index == 1);
	CHECK(f.buffer.write_command_or_list_index == 0);
	CHECK(f.buffer.usage == RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
}
} // namespace TestGraphSelectedBufferSource
