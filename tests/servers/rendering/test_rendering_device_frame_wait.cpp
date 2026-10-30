/**************************************************************************/
/*  test_rendering_device_frame_wait.cpp                                  */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_rendering_device_frame_wait)

#include "core/object/callable_mp.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#ifdef MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_image_collector.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_readback_collector.h"
#endif

class TestFrameWaitAccess {
public:
	static void attach_native_source(RenderingDevice &device, RenderingDeviceDriver &driver) {
		device.driver = &driver;
		device.frames.resize(1);
		RenderingDeviceDriver::QueueCallReport report;
		RenderingDeviceDriver::NativeCallFact fact;
		fact.serial = fact.completion_serial = 17;
		report.calls.push_back(fact);
		device.frames[0].chain_reports.push_back(report);
	}
	static bool native_source_returned(RenderingDevice &device) { return device.frames[0].chain_reports[0].calls[0].source_owner_returned; }

	static void attach(RenderingDevice &device, RenderingDeviceDriver &driver, const Callable &callback) {
		device.driver = &driver;
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.fence = RenderingDeviceDriver::FenceID(41);
		frame.fence_signaled = true;
		frame.primary_command_buffer_begun = true;
		RenderingDevice::BufferGetDataRequest request;
		request.size = 4;
		request.frame_local_count = 1;
		request.callback = callback;
		request.result.instantiate();
		request.result->bytes.resize(4);
		frame.download_buffer_get_data_requests.push_back(request);
		frame.download_buffer_staging_buffers.push_back(RenderingDeviceDriver::BufferID(51));
		RenderingDeviceDriver::BufferCopyRegion region;
		region.size = 4;
		frame.download_buffer_copy_regions.push_back(region);
		RenderingDevice::Buffer retained;
		retained.driver_id = RenderingDeviceDriver::BufferID(61);
		frame.buffers_to_dispose_of.push_back(retained);
		device.upload_staging_buffers.blocks.resize(1);
		device.upload_staging_buffers.blocks.write[0].frame_used = 9;
		device.upload_staging_buffers.blocks.write[0].fill_amount = 16;
	}
	static bool signaled(const RenderingDevice &device) { return device.frames[0].fence_signaled; }
	static uint32_t pending(const RenderingDevice &device) { return device.frames[0].download_buffer_get_data_requests.size(); }
	static bool disposal_pending(const RenderingDevice &device) { return device.frames[0].buffers_to_dispose_of.front() != nullptr; }
	static Error stage(RenderingDevice &device, bool flush) {
		return device._staging_buffer_execute_required_action(device.upload_staging_buffers,
				flush ? RenderingDevice::STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL : RenderingDevice::STAGING_REQUIRED_ACTION_STALL_PREVIOUS);
	}
	static bool staging_retained(const RenderingDevice &device) {
		return device.upload_staging_buffers.blocks[0].frame_used == 9 && device.upload_staging_buffers.blocks[0].fill_amount == 16;
	}
	static void age_staging(RenderingDevice &device) {
		device.frames_drawn = 20;
		device.upload_staging_buffers.block_size = 64;
		device.upload_staging_buffers.max_size = 64;
	}
	static Error allocate_staging(RenderingDevice &device) {
		uint32_t offset = 0;
		uint32_t size = 0;
		RenderingDevice::StagingRequiredAction action;
		return device._staging_buffer_allocate(device.upload_staging_buffers, 8, 4, offset, size, action);
	}
	static int frame_index(const RenderingDevice &device) { return device.frame; }
	static void previous_slot_fixture(RenderingDevice &device) {
		device.frames.resize(2);
		device.frame = 1;
		device.frames[1].primary_command_buffer_begun = true;
	}
	static Error retry_previous(RenderingDevice &device) { return device._stall_for_previous_frames(); }
	static void use_texture_request(RenderingDevice &device, const Callable &callback) {
		auto &frame = device.frames[0];
		frame.download_buffer_get_data_requests.clear();
		frame.download_buffer_staging_buffers.clear();
		frame.download_buffer_copy_regions.clear();
		RenderingDevice::TextureGetDataRequest request;
		request.format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
		request.width = request.height = request.depth = request.mipmaps = 1;
		request.frame_local_count = 1;
		request.callback = callback;
		frame.download_texture_get_data_requests.push_back(request);
		frame.download_texture_staging_buffers.push_back(RenderingDeviceDriver::BufferID(51));
		RenderingDeviceDriver::BufferTextureCopyRegion region;
		region.row_pitch = 4;
		frame.download_buffer_texture_copy_regions.push_back(region);
		frame.download_texture_mipmap_offsets.push_back(0);
		device.texture_download_region_size_px = 1;
	}
	static uint32_t pending_texture(const RenderingDevice &device) { return device.frames[0].download_texture_get_data_requests.size(); }
	static auto prepare_issue(RenderingDevice &device, bool primary_began = true, uint32_t size = 4) {
		auto &frame = device.frames[0];
		frame.fence_signaled = false;
		frame.command_buffer = RenderingDeviceDriver::CommandBufferID(11);
		device.draw_graph.initialize(device.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin(primary_began);
		auto result = frame.download_buffer_get_data_requests[0].result;
		frame.download_buffer_get_data_requests[0].size = size;
		frame.download_buffer_copy_regions[0].size = size;
		const auto handle = device.draw_graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(71), nullptr, RenderingDeviceDriver::BufferID(51), { 0, 0, size });
		result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), RenderingDeviceDriver::BufferID(51), { 0, 0, size }, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, {}, false, OK, {} });
		return result;
	}
	static void add_idle_slot(RenderingDevice &device) { device.frames.resize(2); }
	static void invalidate_graph(RenderingDevice &device) { device.draw_graph.begin(); }
	static void record_next_graph(RenderingDevice &device) {
		auto &frame = device.frames[0];
		frame.fence_signaled = false;
		frame.primary_command_buffer_begun = true;
		device.draw_graph.begin();
		auto &request = frame.download_buffer_get_data_requests[0];
		const auto region = frame.download_buffer_copy_regions[0];
		const auto handle = device.draw_graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(71), nullptr, RenderingDeviceDriver::BufferID(51), region);
		request.result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), RenderingDeviceDriver::BufferID(51), region, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, {}, false, OK, {} });
	}
	static void separate_present(RenderingDevice &device) {
		device.main_queue = RenderingDeviceDriver::CommandQueueID(1);
		device.present_queue = RenderingDeviceDriver::CommandQueueID(2);
		device.frames[0].semaphore = RenderingDeviceDriver::SemaphoreID(42);
		device.frames[0].swap_chains_to_present.push_back(RenderingDeviceDriver::SwapChainID(3));
	}
	static const auto &present_report(const RenderingDevice &device) { return device.frames[0].present_report; }
	static void seal_split_readbacks(RenderingDevice &device) {
		auto &frame = device.frames[0];
		auto &request = frame.download_buffer_get_data_requests[0];
		request.size = 12;
		request.frame_local_count = 3;
		request.result->bytes.resize(12);
		for (uint32_t i = 1; i < 3; i++) {
			device.draw_graph.add_draw_list_begin(RenderingDeviceDriver::RenderPassID(), RenderingDeviceDriver::FramebufferID(), Rect2i(), {}, {}, {}, 0, true);
			device.draw_graph.add_draw_list_end();
			const RenderingDeviceDriver::BufferID destination(51 + i);
			const RenderingDeviceDriver::BufferCopyRegion region{ i * 4, 0, 4 };
			const auto handle = device.draw_graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(71), nullptr, destination, region);
			request.result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), destination, region, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, {}, false, OK, {} });
			frame.download_buffer_staging_buffers.push_back(destination);
			frame.download_buffer_copy_regions.push_back(region);
		}
		auto command = frame.command_buffer;
		device.draw_graph.end(false, false, command, frame.command_buffer_pool);
		device._seal_buffer_readback_records();
	}
	static auto split_first_segment(RenderingDevice &device) {
		auto &request = device.frames[0].download_buffer_get_data_requests[0];
		request.size = 2;
		request.final_segment = false;
		device.frames[0].download_buffer_copy_regions[0].size = 2;
		return request.result;
	}
	static void next_segment(RenderingDevice &device, const Ref<RenderingDevice::BufferGetDataResult> &result, const Callable &callback, uint32_t offset = 2, uint32_t size = 2) {
		auto &frame = device.frames[0];
		frame.fence_signaled = true;
		RenderingDevice::BufferGetDataRequest request;
		request.size = size;
		request.result_offset = offset;
		request.frame_local_count = 1;
		request.callback = callback;
		request.result = result;
		frame.download_buffer_get_data_requests.push_back(request);
		frame.download_buffer_staging_buffers.push_back(RenderingDeviceDriver::BufferID(51));
		RenderingDeviceDriver::BufferCopyRegion region;
		region.src_offset = offset;
		region.dst_offset = 2;
		region.size = size;
		frame.download_buffer_copy_regions.push_back(region);
	}
	static void detach(RenderingDevice &device) {
		// These handles belong to the fake fixture, not a native driver.
		device.frames.clear();
		device.upload_staging_buffers.blocks.clear();
		device.main_queue = RenderingDeviceDriver::CommandQueueID();
		device.present_queue = RenderingDeviceDriver::CommandQueueID();
		device.driver = nullptr;
	}
};

TEST_CASE("[K03][FrameWait] ordinary native source retirement waits while typed Busy retains source") {
	class NativeDriver : public TransferTestDriverBase {
	public:
		int bounded = 0, blocking = 0;
		bool native_call_observation_supported() const override { return true; }
		FencePollResult native_call_poll(uint64_t serial) override {
			FencePollResult r;
			r.completed = true;
			r.completed_serial = serial;
			return r;
		}
		FencePollResult native_call_wait(uint64_t serial) override { return native_call_poll(serial); }
		Error native_call_recycle(uint64_t serial) override {
			CHECK(serial == 17);
			++bounded;
			return ERR_BUSY;
		}
		// Ordinary retirement uses the blocking backend route.
		Error native_call_recycle_wait(uint64_t serial) override {
			CHECK(serial == 17);
			++blocking;
			return OK;
		}
	} driver;
	RenderingDevice device;
	TestFrameWaitAccess::attach_native_source(device, driver);
	CHECK(device._stall_for_frame(0, true) == ERR_BUSY);
	CHECK_FALSE(TestFrameWaitAccess::native_source_returned(device));
	CHECK(device._stall_for_frame(0, false) == OK);
	CHECK(TestFrameWaitAccess::native_source_returned(device));
	CHECK(driver.bounded == 1);
	CHECK(driver.blocking == 1);
	TestFrameWaitAccess::detach(device);
}

namespace TestRenderingDeviceFrameWait {

class Driver : public TransferTestDriverBase {
public:
	Error result = ERR_TIMEOUT;
	int waits = 0;
	int maps = 0;
	int unmaps = 0;
	int resets = 0;
	bool map_failure = false;
	bool reset_result = true;
	bool begin_result = true;
	int begins = 0;
	Error queue_result = ERR_OUT_OF_MEMORY;
	bool native_issued = false;
	int queue_calls = 0;
	bool fail_present = false;
	int fail_call = 0;
	uint64_t next_command = 12;
	uint8_t bytes[4] = { 1, 2, 3, 4 };
	Error fence_wait(FenceID) override {
		waits++;
		return result;
	}
	uint8_t *buffer_map(BufferID) override {
		maps++;
		return map_failure ? nullptr : bytes;
	}
	void buffer_unmap(BufferID) override { unmaps++; }
	bool command_pool_reset(CommandPoolID) override {
		resets++;
		return reset_result;
	}
	void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override {}
	void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
	void command_buffer_end(CommandBufferID) override {}
	CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(next_command++); }
	bool command_buffer_begin(CommandBufferID) override { return begin_result; }
	void begin_segment(uint32_t, uint32_t) override { begins++; }
	SemaphoreID semaphore_create() override { return SemaphoreID(13); }
	void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
	void end_segment() override {}
	Error command_queue_execute_and_present(CommandQueueID queue, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
		queue_calls++;
		const Error error = fail_present && queue == CommandQueueID(2) ? ERR_UNAVAILABLE : (fail_call == queue_calls ? ERR_OUT_OF_MEMORY : queue_result);
		QueueCallReport report;
		report.issued = native_issued;
		report.error = error;
		report.native_code = error != OK ? -731 : 0;
		report.submission_serial = native_issued ? 22 + queue_calls : 0;
		NativeCallFact call;
		call.serial = queue_calls;
		call.issue = native_issued ? IssueFact::IssuedUnknown : IssueFact::NotIssued;
		call.error = error;
		call.native_code = report.native_code;
		report.calls.push_back(call);
		record_queue_call_report(report);
		return error;
	}
};

static int callbacks = 0;
static PackedByteArray received;
static RenderingDevice *reentrant_device = nullptr;
static Error reentrant_result = OK;
static Error reentrant_begin_result = OK;
static void receive(PackedByteArray data) {
	callbacks++;
	received = data;
	if (reentrant_device) {
		ERR_PRINT_OFF;
		reentrant_device->swap_buffers(false);
		reentrant_device->sync();
		reentrant_device->draw_command_begin_label(Span<char>(), Color());
		reentrant_device->draw_command_end_label();
		CHECK_FALSE(reentrant_device->storage_buffer_create(4).is_valid());
		ERR_PRINT_ON;
		reentrant_begin_result = reentrant_device->_begin_frame();
		reentrant_result = reentrant_device->buffer_get_data_async(RID(), callable_mp_static(receive));
	}
}

TEST_CASE("[R01][FrameWait] failed fence preserves requests and retry delivers once") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	received = PackedByteArray();
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	CHECK(device._stall_for_frame(0) == ERR_TIMEOUT);
	device._execute_frame(false);
	CHECK(TestFrameWaitAccess::signaled(device));
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	CHECK(driver.maps == 0);
	CHECK(driver.unmaps == 0);
	CHECK(callbacks == 0);
	driver.result = OK;
	CHECK(device._stall_for_frame(0) == OK);
	CHECK_FALSE(TestFrameWaitAccess::signaled(device));
	CHECK(TestFrameWaitAccess::pending(device) == 0);
	CHECK(driver.maps == 1);
	CHECK(driver.unmaps == 1);
	CHECK(callbacks == 1);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	CHECK(driver.waits == 2);
	TestFrameWaitAccess::detach(device);
	REQUIRE(received.size() == 4);
	CHECK(received[0] == 1);
	CHECK(received[3] == 4);
}

TEST_CASE("[R01][FrameWait] failed wait stops frame reset and flush") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	CHECK(device._begin_frame() == ERR_TIMEOUT);
	CHECK(driver.resets == 0);
	CHECK(device._flush_and_stall_for_all_frames() == ERR_TIMEOUT);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	CHECK(TestFrameWaitAccess::signaled(device));
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	CHECK(TestFrameWaitAccess::disposal_pending(device));
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] failed staging wait preserves charged blocks") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	CHECK(TestFrameWaitAccess::stage(device, true) == ERR_TIMEOUT);
	CHECK(TestFrameWaitAccess::staging_retained(device));
	CHECK(TestFrameWaitAccess::stage(device, false) == ERR_TIMEOUT);
	CHECK(TestFrameWaitAccess::staging_retained(device));
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	CHECK(TestFrameWaitAccess::disposal_pending(device));
	CHECK(callbacks == 0);
	CHECK(driver.maps == 0);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] failed epoch rejects new readbacks and staging reuse") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	CHECK(device._stall_for_frame(0) == ERR_TIMEOUT);
	TestFrameWaitAccess::age_staging(device);
	CHECK(TestFrameWaitAccess::allocate_staging(device) == ERR_TIMEOUT);
	CHECK(TestFrameWaitAccess::staging_retained(device));
	CHECK(device.buffer_get_data_async(RID(), callable_mp_static(receive)) == ERR_TIMEOUT);
	CHECK(device.texture_get_data_async(RID(), 0, callable_mp_static(receive)) == ERR_TIMEOUT);
	CHECK(device.buffer_clear(RID(), 0, 4) == ERR_TIMEOUT);
	CHECK(device.buffer_copy(RID(), RID(), 0, 0, 4) == ERR_TIMEOUT);
	const auto count = device.get_frames_drawn();
	const auto frame = TestFrameWaitAccess::frame_index(device);
	ERR_PRINT_OFF;
	device.swap_buffers(false);
	device.swap_buffers(false);
	ERR_PRINT_ON;
	CHECK(device.get_frames_drawn() == count);
	CHECK(TestFrameWaitAccess::frame_index(device) == frame);
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	driver.result = OK;
	reentrant_device = &device;
	reentrant_result = OK;
	CHECK(device._stall_for_frame(0) == OK);
	reentrant_device = nullptr;
	CHECK(reentrant_result == ERR_TIMEOUT);
	CHECK(reentrant_begin_result == ERR_BUSY);
	CHECK(callbacks == 1);
	CHECK(TestFrameWaitAccess::pending(device) == 0);
	// Observing the old fence alone does not reopen recording before begin_frame.
	CHECK(device.buffer_get_data_async(RID(), callable_mp_static(receive)) == ERR_TIMEOUT);
	CHECK(device.texture_get_data_async(RID(), 0, callable_mp_static(receive)) == ERR_TIMEOUT);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] healthy callback cannot reopen its draining frame") {
	Driver driver;
	driver.result = OK;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	reentrant_device = &device;
	reentrant_result = OK;
	reentrant_begin_result = OK;
	CHECK(device._stall_for_frame(0) == OK);
	reentrant_device = nullptr;
	CHECK(reentrant_result == ERR_BUSY);
	CHECK(reentrant_begin_result == ERR_BUSY);
	CHECK(driver.resets == 0);
	CHECK(callbacks == 1);
	CHECK(TestFrameWaitAccess::pending(device) == 0);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] previous slot retry preserves the active recording slot") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	TestFrameWaitAccess::previous_slot_fixture(device);
	CHECK(TestFrameWaitAccess::retry_previous(device) == ERR_TIMEOUT);
	const auto count = device.get_frames_drawn();
	ERR_PRINT_OFF;
	device.swap_buffers(false);
	device.swap_buffers(false);
	ERR_PRINT_ON;
	CHECK(TestFrameWaitAccess::frame_index(device) == 1);
	CHECK(device.get_frames_drawn() == count);
	CHECK(driver.resets == 0);
	CHECK(callbacks == 0);
	driver.result = OK;
	CHECK(TestFrameWaitAccess::retry_previous(device) == OK);
	CHECK(callbacks == 1);
	CHECK(TestFrameWaitAccess::frame_index(device) == 1);
	CHECK(device.get_frames_drawn() == count);
	CHECK(TestFrameWaitAccess::retry_previous(device) == OK);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] texture wait error retains its exact callback batch") {
	Driver driver;
	RenderingDevice device;
	callbacks = 0;
	received = PackedByteArray();
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	TestFrameWaitAccess::use_texture_request(device, callable_mp_static(receive));
	CHECK(device._stall_for_frame(0) == ERR_TIMEOUT);
	CHECK(TestFrameWaitAccess::pending_texture(device) == 1);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	driver.result = OK;
	reentrant_device = &device;
	CHECK(device._stall_for_frame(0) == OK);
	reentrant_device = nullptr;
	CHECK(reentrant_result == ERR_TIMEOUT);
	CHECK(reentrant_begin_result == ERR_BUSY);
	CHECK(TestFrameWaitAccess::pending_texture(device) == 0);
	CHECK(driver.maps == 1);
	CHECK(driver.unmaps == 1);
	CHECK(callbacks == 1);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::detach(device);
	REQUIRE(received.size() == 4);
	CHECK(received[0] == 1);
	CHECK(received[3] == 4);
}

#ifdef MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED
TEST_CASE("[P01][R01] Image collector rejects a texture map failure after RD releases its callback") {
	Driver driver;
	driver.result = OK;
	driver.map_failure = true;
	RenderingDevice device;
	SkinnedMeshCaptureTestImageCollector collector;
	Callable callback;
	REQUIRE(collector.reserve(0, callback) == OK);
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	TestFrameWaitAccess::use_texture_request(device, callback);
	callback = Callable();
	collector.close();
	CHECK_FALSE(collector.drained());
	ERR_PRINT_OFF;
	CHECK(device._stall_for_frame(0) == OK);
	ERR_PRINT_ON;
	CHECK(driver.maps == 1);
	CHECK(driver.unmaps == 0);
	CHECK(TestFrameWaitAccess::pending_texture(device) == 0);
	PackedByteArray output;
	CHECK(collector.copy(0, output) == ERR_UNAVAILABLE);
	CHECK(collector.drained());
	CHECK_FALSE(collector.poisoned());
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[P01][R01] Readback collector rejects a buffer map failure after RD releases its callback") {
	Driver driver;
	driver.result = OK;
	driver.map_failure = true;
	RenderingDevice device;
	SkinnedMeshCaptureTestReadbackCollector collector(1, 2, false);
	const SkinnedMeshCaptureTestReadbackIdentity id{ 1, 2, 3, 40, 5, 6, 7, 8, 9, 0, false };
	Callable callback;
	REQUIRE(collector.reserve(id, callback) == OK);
	REQUIRE(collector.mark_enqueued(id) == OK);
	TestFrameWaitAccess::attach(device, driver, callback);
	callback = Callable();
	collector.close();
	CHECK_FALSE(collector.drained());
	ERR_PRINT_OFF;
	CHECK(device._stall_for_frame(0) == OK);
	ERR_PRINT_ON;
	CHECK(driver.maps == 1);
	CHECK(driver.unmaps == 0);
	CHECK(TestFrameWaitAccess::pending(device) == 0);
	PackedByteArray output;
	CHECK(collector.copy_completed(id, output) == ERR_INVALID_DATA);
	CHECK(collector.poisoned());
	CHECK(collector.drained());
	TestFrameWaitAccess::detach(device);
}
#endif

TEST_CASE("[R01][FrameWait] segmented result retains prefix until one exact final callback") {
	Driver driver;
	driver.result = OK;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::split_first_segment(device);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 0);
	CHECK(result->completed_size == 2);
	CHECK_FALSE(result->delivered);
	TestFrameWaitAccess::next_segment(device, result, callable_mp_static(receive));
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	CHECK(result->completed_size == 4);
	CHECK(result->delivered);
	CHECK(result->error == OK);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::detach(device);
	REQUIRE(received.size() == 4);
	CHECK(received[0] == 1);
	CHECK(received[3] == 4);
}

TEST_CASE("[R01][FrameWait] incomplete or failed result never publishes a success callback") {
	for (int failure = 0; failure < 4; failure++) {
		Driver driver;
		driver.result = OK;
		RenderingDevice device;
		callbacks = 0;
		TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
		const auto result = TestFrameWaitAccess::split_first_segment(device);
		CHECK(device._stall_for_frame(0) == OK);
		const uint32_t offset = failure == 0 ? 1 : failure == 1 ? 3
																: 2;
		const uint32_t size = failure == 2 ? 1 : 2;
		TestFrameWaitAccess::next_segment(device, result, callable_mp_static(receive), offset, size);
		driver.map_failure = failure == 3;
		ERR_PRINT_OFF;
		CHECK(device._stall_for_frame(0) == OK);
		ERR_PRINT_ON;
		CHECK(result->error == (failure == 3 ? ERR_CANT_ACQUIRE_RESOURCE : ERR_INVALID_DATA));
		CHECK_FALSE(result->delivered);
		CHECK(callbacks == 0);
		CHECK(device._stall_for_frame(0) == OK);
		CHECK(callbacks == 0);
		TestFrameWaitAccess::detach(device);
	}
}

TEST_CASE("[R01][FrameWait] queue failure cannot reset or replay a readback frame") {
	for (bool partial_issue : { false, true }) {
		Driver driver;
		driver.result = OK;
		driver.native_issued = partial_issue;
		RenderingDevice device;
		callbacks = 0;
		TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
		const auto result = TestFrameWaitAccess::prepare_issue(device);
		TestFrameWaitAccess::add_idle_slot(device);
		CHECK(device._end_frame() == OK);
		CHECK(result->copies[0].mapping_sealed);
		CHECK(result->copies[0].mapping == RenderingDeviceGraph::RecordMappingState::Recorded);
		CHECK(result->copies[0].command_slot == 0);
		CHECK(result->copies[0].command_buffer == RenderingDeviceDriver::CommandBufferID(11));
		CHECK_FALSE(result->copies[0].issue_attempted);
		CHECK(device._execute_frame(false) == ERR_OUT_OF_MEMORY);
		CHECK(result->copies[0].issue_attempted);
		CHECK(result->copies[0].queue_error == ERR_OUT_OF_MEMORY);
		CHECK(result->copies[0].issue_report.native_code == -731);
		CHECK(result->copies[0].issue_report.issued == partial_issue);
		CHECK(result->error == ERR_OUT_OF_MEMORY);
		CHECK(device._begin_frame() == ERR_OUT_OF_MEMORY);
		CHECK(device._flush_and_stall_for_all_frames() == ERR_OUT_OF_MEMORY);
		CHECK(device._stall_for_frame(0) == ERR_OUT_OF_MEMORY);
		CHECK(device.buffer_get_data_async(RID(), callable_mp_static(receive)) == ERR_OUT_OF_MEMORY);
		ERR_PRINT_OFF;
		device.swap_buffers(false);
		device.submit();
		ERR_PRINT_ON;
		CHECK(driver.queue_calls == 1);
		CHECK(TestFrameWaitAccess::frame_index(device) == 0);
		CHECK(device.get_frames_drawn() == 0);
		CHECK(driver.resets == 0);
		CHECK(driver.maps == 0);
		CHECK(callbacks == 0);
		CHECK(TestFrameWaitAccess::pending(device) == 1);
		CHECK(TestFrameWaitAccess::disposal_pending(device));
		CHECK_FALSE(TestFrameWaitAccess::signaled(device));
		TestFrameWaitAccess::detach(device);
	}
}

TEST_CASE("[R01][FrameWait] failed graph seal cannot issue or map its queued readback") {
	Driver driver;
	driver.result = OK;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::prepare_issue(device, false);
	CHECK(device._flush_and_stall_for_all_frames() == FAILED);
	CHECK(result->error == FAILED);
	CHECK(result->copies[0].mapping_sealed);
	CHECK(result->copies[0].mapping == RenderingDeviceGraph::RecordMappingState::Failed);
	CHECK_FALSE(result->copies[0].issue_attempted);
	CHECK(device._begin_frame() == FAILED);
	CHECK(driver.queue_calls == 0);
	CHECK(driver.resets == 0);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] successful readback issue remains tied to its original graph") {
	Driver driver;
	driver.result = OK;
	driver.queue_result = OK;
	driver.native_issued = true;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::prepare_issue(device);
	CHECK(device._end_frame() == OK);
	CHECK(device._execute_frame(false) == OK);
	CHECK(result->copies[0].issue_attempted);
	CHECK(result->copies[0].queue_error == OK);
	CHECK(result->copies[0].issue_report.submission_serial == 23);
	CHECK(result->copies[0].command_buffer == RenderingDeviceDriver::CommandBufferID(11));
	CHECK(callbacks == 0);
	// The flush observes the already-issued fence; it must not reissue it.
	CHECK(device._flush_and_stall_for_all_frames(false) == OK);
	CHECK(driver.queue_calls == 1);
	CHECK(callbacks == 1);
	TestFrameWaitAccess::invalidate_graph(device);
	CHECK(result->copies[0].mapping == RenderingDeviceGraph::RecordMappingState::Recorded);
	CHECK(result->copies[0].issue_report.submission_serial == 23);
	CHECK(driver.queue_calls == 1);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] present failure never replaces a command slot report") {
	Driver driver;
	driver.result = OK;
	driver.queue_result = OK;
	driver.native_issued = true;
	driver.fail_present = true;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::prepare_issue(device);
	TestFrameWaitAccess::separate_present(device);
	CHECK(device._end_frame() == OK);
	CHECK(device._execute_frame(true) == ERR_UNAVAILABLE);
	CHECK(result->error == ERR_UNAVAILABLE);
	CHECK(result->copies[0].queue_error == OK);
	CHECK(result->copies[0].issue_report.error == OK);
	CHECK(result->copies[0].issue_report.native_code == 0);
	CHECK(TestFrameWaitAccess::present_report(device).error == ERR_UNAVAILABLE);
	CHECK(TestFrameWaitAccess::present_report(device).native_code == -731);
	CHECK(TestFrameWaitAccess::signaled(device));
	CHECK(device._stall_for_frame(0) == ERR_UNAVAILABLE);
	CHECK(device._begin_frame() == ERR_UNAVAILABLE);
	CHECK(driver.queue_calls == 2);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] split issue preserves earlier call report and unattempted suffix") {
	Driver driver;
	driver.result = OK;
	driver.queue_result = OK;
	driver.native_issued = true;
	driver.fail_call = 2;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::prepare_issue(device);
	TestFrameWaitAccess::seal_split_readbacks(device);
	CHECK(device._execute_frame(false) == ERR_OUT_OF_MEMORY);
	CHECK(driver.queue_calls == 2);
	CHECK(result->copies.size() == 3);
	CHECK(result->copies[0].command_slot == 0);
	CHECK(result->copies[0].issue_attempted);
	CHECK(result->copies[0].queue_error == OK);
	CHECK(result->copies[0].issue_report.native_code == 0);
	CHECK(result->copies[0].issue_report.calls[0].issue == RenderingDeviceDriver::IssueFact::IssuedUnknown);
	CHECK(result->copies[1].command_slot == 1);
	CHECK(result->copies[1].issue_attempted);
	CHECK(result->copies[1].queue_error == ERR_OUT_OF_MEMORY);
	CHECK(result->copies[1].issue_report.native_code == -731);
	CHECK(result->copies[1].issue_report.calls[0].issue == RenderingDeviceDriver::IssueFact::IssuedUnknown);
	CHECK(result->copies[2].command_slot == 2);
	CHECK_FALSE(result->copies[2].issue_attempted);
	CHECK(result->copies[2].issue_report.calls.is_empty());
	CHECK(result->error == ERR_OUT_OF_MEMORY);
	CHECK(device._execute_frame(false) == ERR_OUT_OF_MEMORY);
	CHECK(device._flush_and_stall_for_all_frames(false) == ERR_OUT_OF_MEMORY);
	CHECK(driver.queue_calls == 2);
	CHECK(driver.maps == 0);
	CHECK(callbacks == 0);
	CHECK(TestFrameWaitAccess::pending(device) == 1);
	CHECK(TestFrameWaitAccess::disposal_pending(device));
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] reused native slot cannot overwrite an earlier graph issue") {
	Driver driver;
	driver.result = OK;
	driver.queue_result = OK;
	driver.native_issued = true;
	RenderingDevice device;
	callbacks = 0;
	TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
	const auto result = TestFrameWaitAccess::prepare_issue(device, true, 2);
	TestFrameWaitAccess::split_first_segment(device);
	CHECK(device._end_frame() == OK);
	CHECK(device._execute_frame(false) == OK);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 0);
	CHECK(result->copies[0].issue_report.submission_serial == 23);
	TestFrameWaitAccess::next_segment(device, result, callable_mp_static(receive));
	TestFrameWaitAccess::record_next_graph(device);
	CHECK(device._end_frame() == OK);
	CHECK(device._execute_frame(false) == OK);
	CHECK(result->copies.size() == 2);
	CHECK(result->copies[0].command_buffer == result->copies[1].command_buffer);
	CHECK(result->copies[0].command_slot == result->copies[1].command_slot);
	CHECK(result->copies[0].handle.graph_frame != result->copies[1].handle.graph_frame);
	CHECK(result->copies[0].issue_report.submission_serial == 23);
	CHECK(result->copies[1].issue_report.submission_serial == 24);
	CHECK(device._stall_for_frame(0) == OK);
	CHECK(callbacks == 1);
	CHECK(result->completed_size == 4);
	CHECK(result->error == OK);
	TestFrameWaitAccess::detach(device);
}

TEST_CASE("[R01][FrameWait] native pool reset and primary begin failure close admission immediately") {
	for (bool reset_failure : { false, true }) {
		Driver driver;
		driver.result = OK;
		driver.reset_result = !reset_failure;
		driver.begin_result = reset_failure;
		RenderingDevice device;
		callbacks = 0;
		TestFrameWaitAccess::attach(device, driver, callable_mp_static(receive));
		const auto result = TestFrameWaitAccess::prepare_issue(device);
		ERR_PRINT_OFF;
		CHECK(device._begin_frame() == FAILED);
		ERR_PRINT_ON;
		CHECK(result->error == FAILED);
		const auto count = device.get_frames_drawn();
		CHECK(device._begin_frame() == FAILED);
		CHECK(device._end_frame() == FAILED);
		CHECK(device._execute_frame(false) == FAILED);
		CHECK(device._flush_and_stall_for_all_frames() == FAILED);
		CHECK(device.buffer_get_data_async(RID(), callable_mp_static(receive)) == FAILED);
		CHECK(driver.resets == 1);
		CHECK(driver.begins == (reset_failure ? 0 : 1));
		CHECK(driver.queue_calls == 0);
		CHECK(driver.maps == 0);
		CHECK(callbacks == 0);
		CHECK(device.get_frames_drawn() == count);
		CHECK(TestFrameWaitAccess::pending(device) == 1);
		CHECK(TestFrameWaitAccess::disposal_pending(device));
		TestFrameWaitAccess::detach(device);
	}
}

} //namespace TestRenderingDeviceFrameWait
