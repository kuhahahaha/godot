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
#include "core/os/thread.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#ifdef MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_image_collector.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_readback_collector.h"
#endif

class TestFrameWaitAccess {
public:
	struct FrameErrors {
		uint32_t count = 0;
		static void capture(void *p_data, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType p_type) {
			if (p_type == ERR_HANDLER_ERROR) {
				++static_cast<FrameErrors *>(p_data)->count;
			}
		}
		ErrorHandlerList handler{ &FrameErrors::capture, this };
		FrameErrors() { add_error_handler(&handler); }
		~FrameErrors() { remove_error_handler(&handler); }
	};
	static inline uint32_t pending_draw_callbacks = 0;
	static void pending_frame_drawn() { ++pending_draw_callbacks; }
	static void busy_ordinary_advance_retries_same_slot(bool p_server_draw = false, uint32_t p_failure = 0) {
		auto *server = p_server_draw ? Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton()) : nullptr;
		auto *compositor = p_server_draw ? RendererCompositor::get_singleton() : nullptr;
		if (p_server_draw) {
			REQUIRE(server != nullptr);
			REQUIRE(compositor != nullptr);
			REQUIRE(RenderingDevice::get_singleton() == nullptr);
			if (!server || !compositor || RenderingDevice::get_singleton()) {
				return;
			}
			server->sync();
		}
		// Feed original RD frame paths distinct observations; this fixture does
		// not certify a native dispatch or replace the separate Metal witness.
		struct BusyDriver : TransferTestDriverBase {
			LocalVector<uint64_t> waited, recycled;
			bool ready = false, native_failure = false;
			uint32_t begins = 0, resets = 0, submissions = 0;
			bool native_call_observation_supported() const override { return true; }
			FencePollResult native_call_wait(uint64_t serial) override {
				waited.push_back(serial);
				FencePollResult result;
				if (native_failure) {
					result.native_error = true;
					result.native_code = 77;
					return result;
				}
				if (ready) {
					result.completed = true;
					result.completed_serial = serial;
				}
				return result;
			}
			Error native_call_recycle_wait(uint64_t serial) override {
				recycled.push_back(serial);
				return OK;
			}
			Error fence_wait(FenceID) override { return OK; }
			bool command_pool_reset(CommandPoolID) override {
				++resets;
				return true;
			}
			bool command_buffer_begin(CommandBufferID) override { return true; }
			void begin_segment(uint32_t, uint32_t) override { ++begins; }
			void command_buffer_end(CommandBufferID) override {}
			void end_segment() override {}
			Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
				++submissions;
				QueueCallReport report;
				report.issued = true;
				record_queue_call_report(report);
				return OK;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.is_main_instance = p_server_draw;
		device.frames.resize(2);
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 2, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		for (uint32_t i = 0; i < 2; ++i) {
			auto &frame = device.frames[i];
			frame.fence_signaled = frame.primary_command_buffer_begun = true;
			frame.command_buffer = RenderingDeviceDriver::CommandBufferID(31 + i);
			frame.fence = RenderingDeviceDriver::FenceID(41 + i);
			RenderingDeviceDriver::QueueCallReport report;
			RenderingDeviceDriver::NativeCallFact fact;
			fact.serial = fact.completion_serial = 17 + i;
			report.calls.push_back(fact);
			frame.chain_reports.push_back(report);
		}
		FrameErrors errors;
		ERR_PRINT_OFF;
		device.swap_buffers(false);
		ERR_PRINT_ON;
		CHECK(errors.count == 0); // Healthy early Busy is not a rendering error.
		CHECK(device.frame == 1);
		CHECK(device.frame_wait_error == OK); // Early native Busy has no fence error.
		CHECK(device.qualification_frame_advance_pending);
		CHECK(device.buffer_clear(RID(), 0, 4) == ERR_BUSY);
		const uint64_t compositor_frame = compositor ? compositor->get_frame_number() : 0;
		if (p_server_draw) {
			pending_draw_callbacks = 0;
			server->request_frame_drawn_callback(callable_mp_static(&TestFrameWaitAccess::pending_frame_drawn));
			if (p_failure == 1) {
				device.frame_issue_error = FAILED;
			} else if (p_failure == 2) {
				device.frame_wait_error = FAILED;
			} else if (p_failure == 3) {
				driver.native_failure = true;
			}
		}
		errors.count = 0;
		ERR_PRINT_OFF;
		if (p_server_draw) {
			server->draw(false, 0.01);
		} else {
			device.swap_buffers(false);
		}
		ERR_PRINT_ON;
		if (p_failure) {
			CHECK(errors.count > 0); // A real error remains observable.
		} else {
			CHECK(errors.count == 0);
		}
		if (p_server_draw) {
			CHECK(pending_draw_callbacks == 0);
			CHECK(compositor->get_frame_number() == compositor_frame);
			CHECK(server->has_changed());
		}
		if (p_failure) {
			CHECK(device.frame == 1);
			CHECK(device.qualification_frame_advance_pending);
			CHECK(driver.begins == 0);
			CHECK(driver.resets == 0);
			CHECK(driver.submissions == 0);
			if (p_failure == 1) {
				CHECK(device.frame_issue_error == FAILED);
			} else if (p_failure == 2) {
				CHECK(device.frame_wait_error == FAILED);
			} else {
				CHECK(device.frame_wait_native_code_valid);
				CHECK(device.frame_wait_native_code == 77);
			}
			// Restore only the controlled driver failure to drain the registered
			// callback normally before this fixture's RD singleton is destroyed.
			device.frame_issue_error = device.frame_wait_error = OK;
			device.frame_wait_native_code_valid = false;
			driver.native_failure = false;
		}
		CHECK(device.frame == 1);
		if (!p_failure) {
			CHECK(driver.waited.size() == 2);
			if (driver.waited.size() == 2) {
				CHECK(driver.waited[0] == 18);
				CHECK(driver.waited[1] == 18);
			}
		}
		CHECK_FALSE(device.frames[1].chain_reports[0].calls[0].source_owner_returned);
		CHECK(device.get_frames_drawn() == 0);
		CHECK(driver.recycled.is_empty());
		driver.ready = true;
		if (p_server_draw) {
			server->draw(false, 0.01);
			CHECK(pending_draw_callbacks == 1);
			CHECK(compositor->get_frame_number() == compositor_frame + 1);
		} else {
			device.swap_buffers(false);
		}
		CHECK(device.frame == 1);
		CHECK_FALSE(device.qualification_frame_advance_pending);
		CHECK(device._recording_error() == OK);
		CHECK(device.frames[1].chain_reports[0].calls[0].source_owner_returned);
		CHECK(device.get_frames_drawn() == 1);
		CHECK(driver.begins == 1);
		CHECK(driver.resets == 1);
		CHECK(driver.submissions == 0);
		REQUIRE(driver.recycled.size() == 1);
		if (driver.recycled.size() == 1) {
			CHECK(driver.recycled[0] == 18);
		}
		device.swap_buffers(false);
		CHECK(device.frame == 0);
		CHECK_FALSE(device.qualification_frame_advance_pending);
		CHECK(device.get_frames_drawn() == 2);
		CHECK(driver.begins == 2);
		CHECK(driver.resets == 2);
		CHECK(driver.submissions == 1);
		REQUIRE(driver.recycled.size() == 2);
		if (driver.recycled.size() == 2) {
			CHECK(driver.recycled[1] == 17);
		}
		device.frames.clear();
		device.driver = nullptr;
	}
	static void ordinary_fence_busy_reports_error() {
		struct Driver : TransferTestDriverBase {
			Error wait_error = ERR_BUSY;
			uint32_t waits = 0, resets = 0;
			Error fence_wait(FenceID) override {
				++waits;
				return wait_error;
			}
			bool command_pool_reset(CommandPoolID) override {
				++resets;
				return true;
			}
			bool command_buffer_begin(CommandBufferID) override { return true; }
			void begin_segment(uint32_t, uint32_t) override {}
			void command_buffer_end(CommandBufferID) override {}
			void end_segment() override {}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.frames.resize(2);
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 2, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.frames[0].command_buffer = RenderingDeviceDriver::CommandBufferID(31);
		device.frames[1].command_buffer = RenderingDeviceDriver::CommandBufferID(32);
		device.frames[1].fence = RenderingDeviceDriver::FenceID(41);
		// Enter the same ordinary retry state produced after a submitted frame
		// fence reports BUSY.  This keeps the fixture on _stall_for_frame and
		// avoids issuing a synthetic command queue submission.
		device.frames[1].fence_signaled = true;
		device.frame = 1;
		device.qualification_frame_advance_pending = true;
		device.frame_wait_error = ERR_BUSY;
		device.failed_wait_frame = 1;
		FrameErrors errors;
		for (uint32_t attempt = 0; attempt < 2; ++attempt) {
			errors.count = 0;
			ERR_PRINT_OFF;
			const Error begin_error = device._begin_frame(true);
			ERR_PRINT_ON;
			CHECK(begin_error == ERR_BUSY);
			CHECK(errors.count == 0);
			CHECK(device.frame_wait_error == ERR_BUSY);
			CHECK(device.failed_wait_frame == 1);
			CHECK(device.frame == 1);
			CHECK(device.qualification_frame_advance_pending);
			CHECK(driver.waits == attempt + 1);
			CHECK(driver.resets == 0);
		}
		driver.wait_error = OK;
		CHECK(device._begin_frame(true) == OK);
		CHECK(device.frame_wait_error == OK);
		CHECK_FALSE(device.qualification_frame_advance_pending);
		CHECK(driver.resets == 1);
		device.frames.clear();
		device.driver = nullptr;
	}
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
		request.result.instantiate();
		request.result->bytes.resize(4);
		request.result->issued_regions = 1;
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
		result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), RenderingDeviceDriver::BufferID(51), { 0, 0, size }, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, RenderingDeviceDriver::CommandBufferID(), false, OK, {} });
		return result;
	}
	static void add_idle_slot(RenderingDevice &device) { device.frames.resize(2); }
	static void invalidate_graph(RenderingDevice &device) { device.draw_graph.begin(); }
	static void record_next_graph(RenderingDevice &device) {
		auto &frame = device.frames[0];
		frame.fence_signaled = false;
		// The RD recording epoch was opened by _begin_frame(), not just graph.begin().
		auto &request = frame.download_buffer_get_data_requests[0];
		const auto region = frame.download_buffer_copy_regions[0];
		const auto handle = device.draw_graph.add_buffer_get_data(RenderingDeviceDriver::BufferID(71), nullptr, RenderingDeviceDriver::BufferID(51), region);
		request.result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), RenderingDeviceDriver::BufferID(51), region, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, RenderingDeviceDriver::CommandBufferID(), false, OK, {} });
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
			request.result->copies.push_back({ handle, RenderingDeviceDriver::BufferID(71), destination, region, false, RenderingDeviceGraph::RecordMappingState::Pending, UINT32_MAX, RenderingDeviceDriver::CommandBufferID(), false, OK, {} });
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
	// Only native objects and allocator capacity are supplied by this fixture.
	// The original _begin_frame opens recording; the public API creates every request.
	static Error prepare_texture_api(RenderingDevice &device, RenderingDeviceDriver &driver) {
		device.driver = &driver;
		device.main_queue_family = driver.command_queue_family_get(RenderingDeviceDriver::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);
		device.main_queue = driver.command_queue_create(device.main_queue_family, true);
		device.present_queue = device.main_queue;
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.command_pool = driver.command_pool_create(device.main_queue_family, RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY);
		frame.command_buffer = driver.command_buffer_create(frame.command_pool);
		frame.command_buffer_pool.pool = frame.command_pool;
		frame.semaphore = driver.semaphore_create();
		frame.fence = driver.fence_create();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, device.main_queue_family, 0);
		device.texture_download_region_size_px = 4;
		device.download_staging_buffers.block_size = 32;
		device.download_staging_buffers.max_size = 32;
		device.download_staging_buffers.usage_bits = RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_TO_BIT;
		const Error allocated = device._insert_staging_block(device.download_staging_buffers);
		return allocated == OK ? device._begin_frame() : allocated;
	}
	static void texture_api_max_staging(RenderingDevice &device, uint64_t bytes) { device.download_staging_buffers.max_size = bytes; }
	static Vector3i texture_api_staged_rows(const RenderingDevice &device) {
		const auto &frame = device.frames[device.frame];
		return Vector3i(frame.download_texture_staging_buffers.size(), frame.download_buffer_texture_copy_regions.size(), frame.download_texture_mipmap_offsets.size());
	}
	static uint32_t texture_api_pending(const RenderingDevice &device) { return device.frames[device.frame].download_texture_get_data_requests.size(); }
	static Error finish_texture_api(RenderingDevice &device) { return device._flush_and_stall_for_all_frames(); }
	static Error repeat_texture_drain(RenderingDevice &device) { return device._stall_for_frame(device.frame); }
	static void detach_texture_api(RenderingDevice &device) {
		// Test teardown only: no native work exists behind this CPU boundary.
		device._free_pending_resources(device.frame);
		device.download_staging_buffers.blocks.clear();
		detach(device);
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

// This mock implements only the native boundary. Requests, tile layout, staging
// reuse, graph issue, and callback draining all run through the original RD API.
class TextureAPIDriver : public Driver {
public:
	HashMap<uint64_t, Vector<uint8_t>> buffers;
	HashMap<uint64_t, uint32_t> texture_widths;
	uint64_t next_buffer = 100;
	uint64_t next_texture = 200;
	int copies = 0;
	int fail_map_at = 0;
	int invalid_texture_copies = 0;
	bool fail_next_buffer_create = false;
	int failed_buffer_creates = 0;
	RenderingDevice *allocation_observer = nullptr;
	Vector3i rows_at_failed_create;

	TextureAPIDriver() { result = OK; queue_result = OK; native_issued = true; }
	CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits>, RenderingContextDriver::SurfaceID = 0) override { return CommandQueueFamilyID(1); }
	CommandQueueID command_queue_create(CommandQueueFamilyID family, bool) override {
		CHECK(family == CommandQueueFamilyID(1));
		return CommandQueueID(2);
	}
	CommandPoolID command_pool_create(CommandQueueFamilyID family, CommandBufferType type) override {
		CHECK(family == CommandQueueFamilyID(1));
		CHECK(type == COMMAND_BUFFER_TYPE_PRIMARY);
		return CommandPoolID(3);
	}
	FenceID fence_create() override { return FenceID(41); }
	uint64_t api_trait_get(ApiTrait trait) override {
		if (trait == API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT) {
			return 32;
		}
		return RenderingDeviceDriver::api_trait_get(trait);
	}
	uint64_t limit_get(Limit) override { return 1024; }
	void set_object_name(ObjectType, ID, const String &) override {}
	TextureID texture_create(const TextureFormat &format, const TextureView &) override {
		const TextureID id(next_texture++);
		texture_widths[id.id] = format.width;
		return id;
	}
	uint64_t texture_get_allocation_size(TextureID id) override { return texture_widths[id.id] * texture_widths[id.id]; }
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat, bool) override { return TEXTURE_USAGE_CAN_COPY_FROM_BIT; }
	void texture_free(TextureID id) override { texture_widths.erase(id.id); }
	BufferID buffer_create(uint64_t size, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		if (fail_next_buffer_create) {
			fail_next_buffer_create = false;
			++failed_buffer_creates;
			if (allocation_observer) {
				rows_at_failed_create = TestFrameWaitAccess::texture_api_staged_rows(*allocation_observer);
			}
			return {};
		}
		const BufferID id(next_buffer++);
		buffers[id.id].resize(size);
		buffers[id.id].fill(0xee);
		return id;
	}
	void buffer_free(BufferID id) override { buffers.erase(id.id); }
	uint8_t *buffer_map(BufferID id) override {
		++maps;
		return fail_map_at == maps ? nullptr : buffers[id.id].ptrw();
	}
	void command_copy_texture_to_buffer(CommandBufferID, TextureID source, TextureLayout, BufferID destination, VectorView<BufferTextureCopyRegion> regions) override {
		for (uint32_t region_index = 0; region_index < regions.size(); ++region_index) {
			const auto &region = regions[region_index];
			++copies;
			REQUIRE(region.texture_subresource.mipmap == 0);
			REQUIRE(region.texture_subresource.layer == 0);
			const uint32_t *source_width = texture_widths.getptr(source.id);
			if (!source_width) {
				++invalid_texture_copies;
				continue;
			}
			const uint32_t width = *source_width;
			for (int y = 0; y < region.texture_region_size.y; ++y) {
				for (int x = 0; x < region.texture_region_size.x; ++x) {
					const uint32_t offset = region.buffer_offset + y * region.row_pitch + x;
					REQUIRE(offset < uint32_t(buffers[destination.id].size()));
					buffers[destination.id].ptrw()[offset] = 1 + region.texture_offset.x + x + (region.texture_offset.y + y) * width;
				}
			}
		}
	}
};

// Always detach the fake native boundary, including when a regression assertion
// aborts the test body. Cleanup does not submit or manufacture readback requests.
struct TextureAPIScope {
	RenderingDevice &device;
	Vector<RID> textures;
	explicit TextureAPIScope(RenderingDevice &p_device) : device(p_device) {}
	RID keep(RID texture) {
		textures.push_back(texture);
		return texture;
	}
	~TextureAPIScope() {
		for (RID texture : textures) {
			if (device.texture_is_valid(texture)) {
				device.free_rid(texture);
			}
		}
		TestFrameWaitAccess::detach_texture_api(device);
	}
};

static RID create_api_texture(RenderingDevice &device, uint32_t size = 8) {
	RenderingDevice::TextureFormat format;
	format.format = RenderingDevice::DATA_FORMAT_R8_UNORM;
	format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
	format.width = format.height = size;
	format.depth = format.array_layers = format.mipmaps = 1;
	format.usage_bits = RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	return device.texture_create(format, RenderingDevice::TextureView());
}

TEST_CASE("[R01][TextureAsyncAPI] staging flush preserves every tile and delivers once") {
	TextureAPIDriver driver;
	RenderingDevice device;
	TextureAPIScope scope(device);
	callbacks = 0;
	received.clear();
	REQUIRE(TestFrameWaitAccess::prepare_texture_api(device, driver) == OK);
	CHECK(driver.begins == 1);
	const RID texture = scope.keep(create_api_texture(device));
	REQUIRE(texture.is_valid());
	REQUIRE(device.texture_get_data_async(texture, 0, callable_mp_static(receive)) == OK);
	CHECK(callbacks == 0);
	CHECK(driver.queue_calls == 3);
	CHECK(driver.copies == 3);
	REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
	CHECK(driver.queue_calls == 4);
	CHECK(driver.copies == 4);
	CHECK(driver.waits == 4);
	CHECK(driver.invalid_texture_copies == 0);
	CHECK(callbacks == 1);
	REQUIRE(received.size() == 64);
	for (int i = 0; i < 64; ++i) {
		CHECK(received[i] == i + 1);
	}
	CHECK(TestFrameWaitAccess::repeat_texture_drain(device) == OK);
	CHECK(callbacks == 1);
}

static RenderingDevice *free_texture_device = nullptr;
static RID free_texture_target;
static int free_texture_callbacks = 0;
static void free_target_texture(PackedByteArray) {
	++free_texture_callbacks;
	free_texture_device->free_rid(free_texture_target);
}

TEST_CASE("[R01][TextureAsyncAPI] another callback freeing target refuses texture continuation") {
	TextureAPIDriver driver;
	RenderingDevice device;
	TextureAPIScope scope(device);
	callbacks = free_texture_callbacks = 0;
	REQUIRE(TestFrameWaitAccess::prepare_texture_api(device, driver) == OK);
	const RID trigger = scope.keep(create_api_texture(device, 4));
	const RID target = scope.keep(create_api_texture(device));
	REQUIRE(trigger.is_valid());
	REQUIRE(target.is_valid());
	free_texture_device = &device;
	free_texture_target = target;
	REQUIRE(device.texture_get_data_async(trigger, 0, callable_mp_static(free_target_texture)) == OK);
	ERR_PRINT_OFF;
	const Error result = device.texture_get_data_async(target, 0, callable_mp_static(receive));
	ERR_PRINT_ON;
	free_texture_device = nullptr;
	free_texture_target = RID();
	CHECK(result == ERR_DOES_NOT_EXIST);
	CHECK(free_texture_callbacks == 1);
	CHECK_FALSE(device.texture_is_valid(target));
	CHECK(callbacks == 0);
	REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
	CHECK(callbacks == 0);
	CHECK(driver.invalid_texture_copies == 0);
}

TEST_CASE("[R01][TextureAsyncAPI] prefix map failure never publishes partial success") {
	TextureAPIDriver driver;
	RenderingDevice device;
	TextureAPIScope scope(device);
	callbacks = 0;
	REQUIRE(TestFrameWaitAccess::prepare_texture_api(device, driver) == OK);
	const RID texture = scope.keep(create_api_texture(device));
	REQUIRE(texture.is_valid());
	driver.fail_map_at = driver.maps + 1;
	ERR_PRINT_OFF;
	const Error result = device.texture_get_data_async(texture, 0, callable_mp_static(receive));
	ERR_PRINT_ON;
	CHECK(result == ERR_CANT_ACQUIRE_RESOURCE);
	CHECK(callbacks == 0);
	driver.fail_map_at = 0;
	REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
	CHECK(callbacks == 0);
}

// Real allocator refusal after the first staged tile must not leak anonymous
// frame rows across successful ordinary frame recycling. A later healthy API
// request also proves failed operations never acquire a late success callback.
TEST_CASE("[R01][TextureAsyncAPI][TextureAsyncUnpublishedTail] allocator failure rolls back only unpublished texture rows across frames") {
	TextureAPIDriver driver;
	RenderingDevice device;
	TextureAPIScope scope(device);
	callbacks = 0;
	received.clear();
	REQUIRE(TestFrameWaitAccess::prepare_texture_api(device, driver) == OK);
	const RID texture = scope.keep(create_api_texture(device));
	REQUIRE(texture.is_valid());
	TestFrameWaitAccess::texture_api_max_staging(device, 64);
	driver.allocation_observer = &device;
	for (int attempt = 0; attempt < 3; ++attempt) {
		driver.fail_next_buffer_create = true;
		ERR_PRINT_OFF;
		const Error error = device.texture_get_data_async(texture, 0, callable_mp_static(receive));
		ERR_PRINT_ON;
		CHECK(error == ERR_CANT_CREATE);
		CHECK(driver.failed_buffer_creates == attempt + 1);
		CHECK(driver.rows_at_failed_create == Vector3i(1, 1, 1)); // Original API staged one tile before actual growth refusal.
		CHECK(TestFrameWaitAccess::texture_api_staged_rows(device) == Vector3i());
		CHECK(TestFrameWaitAccess::texture_api_pending(device) == 0);
		CHECK(driver.copies == 0); // These rows were never published as graph copies.
		CHECK(callbacks == 0);
		REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
		CHECK(TestFrameWaitAccess::texture_api_staged_rows(device) == Vector3i());
		CHECK(callbacks == 0);
	}
	driver.allocation_observer = nullptr;
	TestFrameWaitAccess::texture_api_max_staging(device, 32);
	REQUIRE(device.texture_get_data_async(texture, 0, callable_mp_static(receive)) == OK);
	CHECK(callbacks == 0);
	REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
	CHECK(callbacks == 1);
	REQUIRE(received.size() == 64);
	for (int i = 0; i < 64; ++i) {
		CHECK(received[i] == i + 1);
	}
	CHECK(driver.copies == 4);
	CHECK(TestFrameWaitAccess::texture_api_staged_rows(device) == Vector3i());
	CHECK(TestFrameWaitAccess::texture_api_pending(device) == 0);
	CHECK(TestFrameWaitAccess::repeat_texture_drain(device) == OK);
	CHECK(callbacks == 1);
}

// The first request fills the original staging block. The second request
// flushes it before staging any local row, so its old nonzero index must rebase.
TEST_CASE("[R01][TextureAsyncAPI][TextureAsyncZeroPrefixFlush] foreign request flush before first local tile rebases the actual segment") {
	TextureAPIDriver driver;
	RenderingDevice device;
	TextureAPIScope scope(device);
	callbacks = 0;
	received.clear();
	REQUIRE(TestFrameWaitAccess::prepare_texture_api(device, driver) == OK);
	const RID first = scope.keep(create_api_texture(device, 4));
	const RID second = scope.keep(create_api_texture(device, 8));
	REQUIRE(first.is_valid());
	REQUIRE(second.is_valid());
	REQUIRE(device.texture_get_data_async(first, 0, callable_mp_static(receive)) == OK);
	CHECK(callbacks == 0);
	REQUIRE(device.texture_get_data_async(second, 0, callable_mp_static(receive)) == OK);
	CHECK(callbacks == 1);
	REQUIRE(received.size() == 16);
	for (int i = 0; i < 16; ++i) {
		CHECK(received[i] == i + 1);
	}
	CHECK(driver.invalid_texture_copies == 0);
	REQUIRE(TestFrameWaitAccess::finish_texture_api(device) == OK);
	CHECK(callbacks == 2);
	REQUIRE(received.size() == 64);
	for (int i = 0; i < 64; ++i) {
		CHECK(received[i] == i + 1);
	}
	CHECK(driver.copies == 5);
	CHECK(driver.invalid_texture_copies == 0);
	CHECK(TestFrameWaitAccess::texture_api_staged_rows(device) == Vector3i());
	CHECK(TestFrameWaitAccess::texture_api_pending(device) == 0);
	CHECK(TestFrameWaitAccess::repeat_texture_drain(device) == OK);
	CHECK(callbacks == 2);
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
	struct ReuseDriver : Driver {
		void buffer_free(BufferID p_buffer) override {
			CHECK(p_buffer == BufferID(61));
		}
	} driver;
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
	// Reuse must open a fresh RD epoch, including its command issue cursor.
	REQUIRE(device._begin_frame() == OK);
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

TEST_CASE("[R01][FrameWait] ordinary advance retries an early native wait before rotating again") {
	TestFrameWaitAccess::busy_ordinary_advance_retries_same_slot();
}

TEST_CASE("[SceneTree][R01][FrameWait] ordinary draw retries its pending frame before advancing renderer state") {
	TestFrameWaitAccess::busy_ordinary_advance_retries_same_slot(true);
}

TEST_CASE("[SceneTree][R01][FrameWait] pending ordinary draw preserves real issue wait and native errors") {
	SUBCASE("Issue failure") { TestFrameWaitAccess::busy_ordinary_advance_retries_same_slot(true, 1); }
	SUBCASE("Fence wait failure") { TestFrameWaitAccess::busy_ordinary_advance_retries_same_slot(true, 2); }
	SUBCASE("Native completion failure") { TestFrameWaitAccess::busy_ordinary_advance_retries_same_slot(true, 3); }
}

TEST_CASE("[R01][FrameWait] actual fence Busy remains an error during ordinary frame advance") {
	TestFrameWaitAccess::ordinary_fence_busy_reports_error();
}
