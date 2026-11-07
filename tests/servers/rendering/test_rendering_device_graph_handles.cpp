/**************************************************************************/
/*  test_rendering_device_graph_handles.cpp                               */
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

#include "servers/rendering/rendering_device_graph.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_rendering_device_graph_handles)

TEST_CASE("[R02][Graph] append returns distinct original graph handles") {
	RenderingDeviceGraph graph;
	TransferTestDriverBase driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	graph.add_compute_list_begin();
	const auto first = graph.add_compute_list_dispatch(1, 2, 3);
	const auto second = graph.add_compute_list_dispatch(4, 5, 6);
	CHECK(first.is_valid());
	CHECK(second.is_valid());
	CHECK(first.kind == 2);
	CHECK(second.kind == 2);
	CHECK(first.graph_frame == second.graph_frame);
	CHECK(first.index == second.index);
	CHECK(first.offset < second.offset);
	CHECK(graph.contains_record(first));
	CHECK(graph.contains_record(second));
	graph.add_compute_list_end();
	CHECK(graph.contains_record(first));
	CHECK(graph.contains_record(second));
	RenderingDeviceGraph foreign;
	foreign.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	foreign.begin();
	foreign.add_compute_list_begin();
	const auto foreign_record = foreign.add_compute_list_dispatch(1, 2, 3);
	CHECK_FALSE(foreign.contains_record(first));
	CHECK_FALSE(graph.contains_record(foreign_record));
	graph.begin();
	CHECK_FALSE(graph.contains_record(first));
	graph.add_compute_list_begin();
	const auto next_frame = graph.add_compute_list_dispatch(1, 2, 3);
	CHECK(next_frame.is_valid());
	CHECK(graph.contains_record(next_frame));
	CHECK(next_frame.graph_frame != first.graph_frame);
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto upload = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	CHECK(upload.is_valid());
	CHECK(upload.kind == 1);
	CHECK(graph.contains_record(upload));
	CHECK_FALSE(foreign.contains_record(upload));
}

TEST_CASE("[R02][Graph] texture update and clear return graph command handles") {
	RenderingDeviceGraph graph;
	TransferTestDriverBase driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	RenderingDeviceGraph::ResourceTracker source;
	source.buffer_driver_id = RenderingDeviceDriver::BufferID(71);
	RenderingDeviceGraph::ResourceTracker destination;
	destination.texture_driver_id = RenderingDeviceDriver::TextureID(72);
	RenderingDeviceGraph::RecordedBufferToTextureCopy transfer;
	transfer.from_buffer = RenderingDeviceDriver::BufferID(71);
	transfer.region = {};
	RenderingDeviceGraph::ResourceTracker *source_trackers[] = { &source };
	const auto update = graph.add_texture_update(RenderingDeviceDriver::TextureID(72), &destination, VectorView(&transfer, 1), VectorView(source_trackers, 1));
	CHECK(update.is_valid());
	CHECK(graph.contains_record(update));
	CHECK(update.kind == 1);
	const auto clear = graph.add_texture_clear_color(RenderingDeviceDriver::TextureID(72), &destination, Color(0, 0, 0, 1), {});
	CHECK(clear.is_valid());
	CHECK(graph.contains_record(clear));
	CHECK(clear.kind == 1);
}

TEST_CASE("[R02][Graph] dependency path reports actual adjacency length") {
	RenderingDeviceGraph graph;
	TransferTestDriverBase driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto before = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	graph.add_synchronization();
	const auto after = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	uint32_t path_length = 0;
	CHECK(graph.has_dependency_path(before, after, path_length));
	CHECK(path_length > 0);
	CHECK_FALSE(graph.has_dependency_path(after, before, path_length));
	auto foreign = before;
	foreign.graph_instance++;
	CHECK_FALSE(graph.has_dependency_path(foreign, after, path_length));
}

class RecordSlotTestDriver : public TransferTestDriverBase {
public:
	bool begin_result = true;
	uint32_t copy_calls = 0;
	uint32_t failed_buffer_barriers = 0;
	void command_pipeline_barrier(CommandBufferID p_buffer, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {
		if (p_buffer == CommandBufferID(12)) {
			failed_buffer_barriers++;
		}
	}
	void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override { copy_calls++; }
	void command_compute_dispatch(CommandBufferID, uint32_t, uint32_t, uint32_t) override {}
	CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(12); }
	bool command_buffer_begin(CommandBufferID) override { return begin_result; }
	void command_buffer_end(CommandBufferID) override {}
	SemaphoreID semaphore_create() override { return SemaphoreID(13); }
	void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
};

class GraphBindHandleTestDriver : public RecordSlotTestDriver {
public:
	Vector<uint64_t> payload;
	uint64_t buffer_get_dynamic_offsets(Span<BufferID> p_buffers) override {
		CHECK(p_buffers.size() == 3);
		return 5;
	}
	void command_begin_render_pass(CommandBufferID p_command_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType, const Rect2i &, VectorView<RenderPassClearValue>) override {
		CHECK(p_command_buffer == CommandBufferID(11));
		CHECK(p_render_pass == RenderPassID(21));
		CHECK(p_framebuffer == FramebufferID(22));
		payload.push_back(10);
	}
	void command_end_render_pass(CommandBufferID) override { payload.push_back(20); }
	void command_render_bind_vertex_buffers(CommandBufferID p_command_buffer, uint32_t p_count, const BufferID *p_buffers, const uint64_t *p_offsets, uint64_t p_dynamic_offsets) override {
		payload.push_back(1);
		payload.push_back(p_command_buffer.id);
		payload.push_back(p_count);
		for (uint32_t i = 0; i < p_count; i++) {
			payload.push_back(p_buffers[i].id);
			payload.push_back(p_offsets[i]);
		}
		payload.push_back(p_dynamic_offsets);
	}
	void command_render_bind_index_buffer(CommandBufferID p_command_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override {
		payload.push_back(2);
		payload.push_back(p_command_buffer.id);
		payload.push_back(p_buffer.id);
		payload.push_back(p_format);
		payload.push_back(p_offset);
	}
};

TEST_CASE("[R02][Graph] vertex and index bind handles retain original payload and executed mapping") {
	using Driver = RenderingDeviceDriver;
	using Graph = RenderingDeviceGraph;
	for (int discard_return = 0; discard_return < 2; discard_return++) {
		CAPTURE(discard_return);
		Graph graph;
		GraphBindHandleTestDriver driver;
		graph.initialize(&driver, [](Driver *, VectorView<Driver::AttachmentLoadOp>, VectorView<Driver::AttachmentStoreOp>, void *) { return Driver::RenderPassID(); }, 1, Driver::CommandQueueFamilyID(1), 0);
		graph.begin();
		graph.add_draw_list_begin(Driver::RenderPassID(21), Driver::FramebufferID(22), Rect2i(), {}, {}, {});
		Driver::BufferID buffers[] = { Driver::BufferID(31), Driver::BufferID(), Driver::BufferID(33) };
		uint64_t offsets[] = { 4, 8, uint64_t(1) << 35 };
		Graph::RecordHandle records[3];
		if (discard_return) {
			// Existing void-style callers still record exactly the same instructions.
			graph.add_draw_list_bind_vertex_buffers(buffers, offsets);
			graph.add_draw_list_bind_index_buffer(Driver::BufferID(71), Driver::INDEX_BUFFER_FORMAT_UINT32, 0xabc);
			graph.add_draw_list_bind_index_buffer(Driver::BufferID(72), Driver::INDEX_BUFFER_FORMAT_UINT16, 6);
		} else {
			records[0] = graph.add_draw_list_bind_vertex_buffers(buffers, offsets);
			records[1] = graph.add_draw_list_bind_index_buffer(Driver::BufferID(71), Driver::INDEX_BUFFER_FORMAT_UINT32, 0xabc);
			records[2] = graph.add_draw_list_bind_index_buffer(Driver::BufferID(72), Driver::INDEX_BUFFER_FORMAT_UINT16, 6);
			CHECK(records[0].offset < records[1].offset);
			CHECK(records[1].offset < records[2].offset);
			for (const auto &record : records) {
				CHECK(record.kind == 3);
				CHECK(record.index == records[0].index);
				CHECK(record.graph_instance == records[0].graph_instance);
				CHECK(record.graph_frame == records[0].graph_frame);
				CHECK(graph.contains_record(record));
				CHECK(graph.record_mapping_state(record) == Graph::RecordMappingState::Pending);
				uint32_t slot = UINT32_MAX;
				CHECK_FALSE(graph.resolve_record_command_buffer_slot(record, slot));
				CHECK(slot == UINT32_MAX);
				auto invalid = record;
				invalid.graph_instance++;
				CHECK_FALSE(graph.contains_record(invalid));
				CHECK_FALSE(graph.resolve_record_command_buffer_slot(invalid, slot));
				invalid = record;
				invalid.offset++;
				CHECK_FALSE(graph.contains_record(invalid));
				invalid.offset = UINT32_MAX;
				CHECK_FALSE(graph.contains_record(invalid));
			}
		}
		// Caller-owned arrays are copied into the original instruction storage.
		buffers[0] = Driver::BufferID(99);
		offsets[0] = 999;
		const auto end = graph.add_draw_list_end();
		if (!discard_return) {
			for (const auto &record : records) {
				CHECK(graph.contains_record(record));
				CHECK(graph.record_mapping_state(record) == Graph::RecordMappingState::Pending);
				uint32_t slot = UINT32_MAX;
				CHECK_FALSE(graph.resolve_record_command_buffer_slot(record, slot));
				CHECK(slot == UINT32_MAX);
			}
		}
		CHECK(driver.payload.is_empty()); // Recording is not native command execution.
		Graph::CommandBufferPool pool;
		Driver::CommandBufferID primary(11);
		graph.end(false, false, primary, pool);
		const uint64_t expected[] = { 10, 1, 11, 3, 31, 4, 0, 8, 33, uint64_t(1) << 35, 5,
			2, 11, 71, Driver::INDEX_BUFFER_FORMAT_UINT32, 0xabc,
			2, 11, 72, Driver::INDEX_BUFFER_FORMAT_UINT16, 6, 20 };
		REQUIRE(driver.payload.size() == int(sizeof(expected) / sizeof(expected[0])));
		for (int i = 0; i < driver.payload.size(); i++) {
			CHECK(driver.payload[i] == expected[i]);
		}
		if (!discard_return) {
			for (const auto &record : records) {
				CHECK(graph.contains_record(record));
				CHECK(graph.record_mapping_state(record) == Graph::RecordMappingState::Recorded);
				uint32_t slot = UINT32_MAX;
				CHECK(graph.resolve_record_command_buffer_slot(record, slot));
				CHECK(slot == 0);
				Driver::CommandBufferID actual;
				CHECK(graph.resolve_record_command_buffer(record, actual));
				CHECK(actual == primary);
				for (int invalid_part = 0; invalid_part < 3; invalid_part++) {
					auto invalid = record;
					if (invalid_part == 0) {
						invalid.graph_instance++;
					} else if (invalid_part == 1) {
						invalid.offset++;
					} else {
						invalid.index++;
					}
					CHECK_FALSE(graph.contains_record(invalid));
					CHECK(graph.record_mapping_state(invalid) == Graph::RecordMappingState::Invalid);
					slot = UINT32_MAX;
					CHECK_FALSE(graph.resolve_record_command_buffer_slot(invalid, slot));
					CHECK(slot == UINT32_MAX);
				}
			}
			CHECK(graph.contains_record(end));
			graph.begin();
			for (const auto &record : records) {
				CHECK_FALSE(graph.contains_record(record));
				CHECK(graph.record_mapping_state(record) == Graph::RecordMappingState::Invalid);
				uint32_t slot = UINT32_MAX;
				CHECK_FALSE(graph.resolve_record_command_buffer_slot(record, slot));
				CHECK(slot == UINT32_MAX);
			}
		}
	}
}

TEST_CASE("[R02][Graph] rejected primary begin cannot execute or resolve an upload") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin(false);
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto upload = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	CHECK(graph.contains_record(upload));
	CHECK(graph.record_mapping_state(upload) == RenderingDeviceGraph::RecordMappingState::Pending);
	RenderingDeviceGraph::CommandBufferPool pool;
	RenderingDeviceDriver::CommandBufferID primary(11);
	graph.end(false, false, primary, pool);
	RenderingDeviceDriver::CommandBufferID actual;
	uint32_t slot = UINT32_MAX;
	CHECK_FALSE(graph.resolve_record_command_buffer(upload, actual));
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(upload, slot));
	CHECK(graph.record_mapping_state(upload) == RenderingDeviceGraph::RecordMappingState::Failed);
	CHECK(driver.copy_calls == 0);
}

TEST_CASE("[R02][Graph] actual graph execution maps recorded commands to the native command-buffer slot") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto upload = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	graph.add_compute_list_begin();
	const auto dispatch = graph.add_compute_list_dispatch(1, 1, 1);
	graph.add_compute_list_end();
	uint32_t slot = UINT32_MAX;
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(upload, slot));
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(dispatch, slot));
	CHECK(graph.record_mapping_state(upload) == RenderingDeviceGraph::RecordMappingState::Pending);
	CHECK(slot == UINT32_MAX);
	RenderingDeviceGraph::CommandBufferPool pool;
	RenderingDeviceDriver::CommandBufferID primary(11);
	RenderingDeviceDriver::CommandBufferID actual;
	CHECK_FALSE(graph.resolve_record_command_buffer(upload, actual));
	graph.end(true, false, primary, pool);
	CHECK(graph.resolve_record_command_buffer(upload, actual));
	CHECK(actual == primary);
	CHECK(graph.resolve_record_command_buffer(dispatch, actual));
	CHECK(actual == primary);
	CHECK(graph.resolve_record_command_buffer_slot(upload, slot));
	CHECK(slot == 0);
	CHECK(graph.resolve_record_command_buffer_slot(dispatch, slot));
	CHECK(slot == 0);
	CHECK(graph.record_mapping_state(upload) == RenderingDeviceGraph::RecordMappingState::Recorded);
	CHECK(graph.record_mapping_state(dispatch) == RenderingDeviceGraph::RecordMappingState::Recorded);
	RenderingDeviceGraph foreign;
	foreign.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	foreign.begin();
	CHECK_FALSE(foreign.resolve_record_command_buffer_slot(dispatch, slot));
	graph.begin();
	CHECK(graph.record_mapping_state(upload) == RenderingDeviceGraph::RecordMappingState::Invalid);
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(upload, slot));
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(dispatch, slot));
}

TEST_CASE("[R02][Graph] rejected split-buffer begin does not resolve a later upload") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto before = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	graph.add_draw_list_begin(RenderingDeviceDriver::RenderPassID(), RenderingDeviceDriver::FramebufferID(), Rect2i(), {}, {}, {}, 0, true);
	graph.add_draw_list_end();
	const auto after = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	RenderingDeviceGraph::CommandBufferPool pool;
	RenderingDeviceDriver::CommandBufferID primary(11);
	driver.begin_result = false;
	graph.end(false, false, primary, pool);
	RenderingDeviceDriver::CommandBufferID actual;
	CHECK(graph.resolve_record_command_buffer(before, actual));
	CHECK(actual == RenderingDeviceDriver::CommandBufferID(11));
	CHECK_FALSE(graph.resolve_record_command_buffer(after, actual));
	uint32_t slot = UINT32_MAX;
	CHECK_FALSE(graph.resolve_record_command_buffer_slot(after, slot));
	CHECK(graph.record_mapping_state(before) == RenderingDeviceGraph::RecordMappingState::Recorded);
	CHECK(graph.record_mapping_state(after) == RenderingDeviceGraph::RecordMappingState::Failed);
	CHECK(driver.copy_calls == 1);
	CHECK(driver.failed_buffer_barriers == 0);
}

TEST_CASE("[R02][Graph] split draw command puts later upload in the next native slot") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin();
	RenderingDeviceGraph::ResourceTracker destination;
	destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
	RenderingDeviceGraph::RecordedBufferCopy copy;
	copy.source = RenderingDeviceDriver::BufferID(71);
	copy.region = { 0, 0, 32 };
	const auto before = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	graph.add_draw_list_begin(RenderingDeviceDriver::RenderPassID(), RenderingDeviceDriver::FramebufferID(), Rect2i(), {}, {}, {}, 0, true);
	graph.add_draw_list_end();
	const auto after = graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
	RenderingDeviceGraph::CommandBufferPool pool;
	RenderingDeviceDriver::CommandBufferID primary(11);
	RenderingDeviceDriver::CommandBufferID current = primary;
	graph.end(false, false, current, pool);
	RenderingDeviceDriver::CommandBufferID actual;
	CHECK(graph.resolve_record_command_buffer(before, actual));
	CHECK(actual == primary);
	CHECK(graph.resolve_record_command_buffer(after, actual));
	CHECK(actual == pool.buffers[0]);
	CHECK(actual != primary);
	CHECK(current == actual);
	RenderingDeviceGraph foreign;
	foreign.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	foreign.begin();
	CHECK_FALSE(foreign.resolve_record_command_buffer(after, actual));
	uint32_t slot = UINT32_MAX;
	CHECK(graph.resolve_record_command_buffer_slot(before, slot));
	CHECK(slot == 0);
	CHECK(graph.resolve_record_command_buffer_slot(after, slot));
	CHECK(slot == 1);
	CHECK(pool.buffers_used == 1);
}

TEST_CASE("[R02][Graph] readback copy handles map only their actual executed native slots") {
	for (int failure = 0; failure < 3; failure++) {
		RenderingDeviceGraph graph;
		RecordSlotTestDriver driver;
		graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		graph.begin(failure != 2);
		const auto before = graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(71), nullptr, RenderingDeviceDriver::BufferID(81), { 8, 16, 32 });
		graph.add_draw_list_begin(RenderingDeviceDriver::RenderPassID(), RenderingDeviceDriver::FramebufferID(), Rect2i(), {}, {}, {}, 0, true);
		graph.add_draw_list_end();
		const auto after = graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(72), nullptr, RenderingDeviceDriver::BufferID(82), { 40, 48, 64 });
		CHECK(graph.contains_record(before));
		CHECK(graph.contains_record(after));
		CHECK(before.index != after.index);
		CHECK(graph.record_mapping_state(before) == RenderingDeviceGraph::RecordMappingState::Pending);
		RenderingDeviceDriver::CommandBufferID actual;
		CHECK_FALSE(graph.resolve_record_command_buffer(after, actual));
		RenderingDeviceGraph::CommandBufferPool pool;
		RenderingDeviceDriver::CommandBufferID primary(11);
		driver.begin_result = failure != 1;
		graph.end(false, false, primary, pool);
		CHECK(graph.resolve_record_command_buffer(before, actual) == (failure != 2));
		if (failure != 2) {
			CHECK(actual == RenderingDeviceDriver::CommandBufferID(11));
		}
		CHECK(graph.resolve_record_command_buffer(after, actual) == (failure == 0));
		if (failure == 0) {
			CHECK(actual == pool.buffers[0]);
		}
		CHECK(driver.copy_calls == (failure == 0 ? 2 : failure == 1 ? 1
																	: 0));
		graph.begin();
		CHECK_FALSE(graph.contains_record(before));
		CHECK_FALSE(graph.resolve_record_command_buffer(after, actual));
	}
}

TEST_CASE("[D32][Graph] depth draw records keep original storage and reject failed native mapping") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	graph.begin(false);
	RenderingDeviceGraph::ResourceTracker target;
	target.texture_driver_id = RenderingDeviceDriver::TextureID(72);
	RenderingDeviceGraph::FramebufferCache framebuffer;
	framebuffer.trackers.push_back(&target);
	const RenderingDeviceGraph::AttachmentOperation operation = RenderingDeviceGraph::ATTACHMENT_OPERATION_CLEAR;
	RenderingDeviceDriver::RenderPassClearValue clear;
	clear.depth = 1.0f;
	const auto begin = graph.add_draw_list_begin(&framebuffer, Rect2i(0, 0, 8, 8), VectorView(&operation, 1), VectorView(&clear, 1), {}, 0);
	const auto depth_clear = graph.capture_draw_list_clear(0);
	CHECK_FALSE(graph.capture_draw_list_clear(1).is_valid());
	const auto viewport = graph.add_draw_list_set_viewport(Rect2i(0, 0, 8, 8));
	const auto scissor = graph.add_draw_list_set_scissor(Rect2i(0, 0, 4, 4));
	const auto bind = graph.add_draw_list_bind_pipeline(RenderingDeviceDriver::PipelineID(5), {});
	const uint32_t push = 7;
	const auto constant = graph.add_draw_list_set_push_constant(RenderingDeviceDriver::ShaderID(6), &push, sizeof(push));
	const auto draw = graph.add_draw_list_draw(3, 1);
	const RenderingDeviceGraph::RecordHandle records[] = { begin, depth_clear, viewport, scissor, bind, constant, draw };
	for (const auto &record : records) {
		CHECK(graph.contains_record(record));
		CHECK(graph.record_mapping_state(record) == RenderingDeviceGraph::RecordMappingState::Pending);
	}
	const auto end = graph.add_draw_list_end();
	CHECK(end.kind == 7);
	CHECK(graph.contains_record(end));
	for (const auto &record : records) {
		CHECK(graph.contains_record(record));
	}
	RenderingDeviceGraph::CommandBufferPool pool;
	RenderingDeviceDriver::CommandBufferID primary(11);
	graph.end(false, false, primary, pool);
	uint32_t slot = UINT32_MAX;
	for (const auto &record : records) {
		CHECK_FALSE(graph.resolve_record_command_buffer_slot(record, slot));
		CHECK(graph.record_mapping_state(record) == RenderingDeviceGraph::RecordMappingState::Failed);
	}
	CHECK(graph.record_mapping_state(end) == RenderingDeviceGraph::RecordMappingState::Failed);
	graph.begin();
	for (const auto &record : records) {
		CHECK_FALSE(graph.contains_record(record));
	}
	CHECK_FALSE(graph.contains_record(end));
}
