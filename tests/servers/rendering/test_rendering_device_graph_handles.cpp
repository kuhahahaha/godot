#include "servers/rendering/rendering_device_graph.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_rendering_device_graph_handles)

TEST_CASE("[R02][Graph] append returns distinct original graph handles") {
	RenderingDeviceGraph graph;
	TransferTestDriverBase driver;
	graph.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	foreign.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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

class RecordSlotTestDriver : public TransferTestDriverBase {
public:
	bool begin_result = true;
	uint32_t copy_calls = 0;
	uint32_t failed_buffer_barriers = 0;
	void command_pipeline_barrier(CommandBufferID p_buffer, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override { if (p_buffer == CommandBufferID(12)) { failed_buffer_barriers++; } }
	void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override { copy_calls++; }
	void command_compute_dispatch(CommandBufferID, uint32_t, uint32_t, uint32_t) override {}
	CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(12); }
	bool command_buffer_begin(CommandBufferID) override { return begin_result; }
	void command_buffer_end(CommandBufferID) override {}
	SemaphoreID semaphore_create() override { return SemaphoreID(13); }
	void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
};

TEST_CASE("[R02][Graph] rejected primary begin cannot execute or resolve an upload") {
	RenderingDeviceGraph graph;
	RecordSlotTestDriver driver;
	graph.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	graph.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	foreign.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	graph.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	graph.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
	foreign.initialize(&driver,
			[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
			1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
		graph.initialize(&driver,
				[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
				1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
		CHECK(driver.copy_calls == (failure == 0 ? 2 : failure == 1 ? 1 : 0));
		graph.begin();
		CHECK_FALSE(graph.contains_record(before));
		CHECK_FALSE(graph.resolve_record_command_buffer(after, actual));
	}
}
