/**************************************************************************/
/*  test_record_source.cpp                                                */
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

#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <type_traits>
TEST_FORCE_LINK(test_record_source)

// Controlled native observations exercise original source machinery. They do
// not issue production scope/record authority or certify a physical GPU path.
class TestRecordSourceAccess {
	using Origin = GDExtensionDeviceExit::NativeSourceOrigin;
	using Member = GDExtensionDeviceExit::NativeSourceMember;
	using Observation = GDExtensionDeviceExit::NativeSourceObservation;
	struct Driver : TransferTestDriverBase {
		uint32_t calls = 0, retains = 0, releases = 0, polls = 0, waits = 0;
		Error retain_error = OK, release_error = OK;
		FencePollResult observed;
		bool unknown_issue = false;
		bool honors_barriers = true;
		uint32_t expected_uniform_set = 51;
		uint32_t push_calls = 0, prepare_calls = 0, set_calls = 0, dispatch_calls = 0;
		uint32_t pushed_words[4] = {};
		CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(61); }
		bool command_buffer_begin(CommandBufferID) override { return true; }
		void command_buffer_end(CommandBufferID) override {}
		SemaphoreID semaphore_create() override { return SemaphoreID(62); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		void command_bind_compute_pipeline(CommandBufferID, PipelineID p_pipeline) override { CHECK(p_pipeline == PipelineID(8)); }
		void command_bind_push_constants(CommandBufferID, ShaderID p_shader, uint32_t p_first, VectorView<uint32_t> p_data) override {
			CHECK(p_shader == ShaderID(7));
			CHECK(p_first == 0);
			REQUIRE(p_data.size() == 4);
			++push_calls;
			for (uint32_t i = 0; i < 4; ++i) {
				pushed_words[i] = p_data[i];
			}
		}
		void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID p_set, ShaderID p_shader, uint32_t p_index) override {
			CHECK(p_set == UniformSetID(9));
			CHECK(p_shader == ShaderID(7));
			CHECK(p_index == 0);
			++prepare_calls;
		}
		void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID> p_sets, ShaderID p_shader, uint32_t p_first, uint32_t p_count, uint32_t) override {
			CHECK(p_shader == ShaderID(7));
			CHECK(p_first == 0);
			CHECK(p_count == 1);
			REQUIRE(p_sets.size() == 1);
			CHECK(p_sets[0] == UniformSetID(9));
			++set_calls;
		}
		void command_compute_dispatch(CommandBufferID, uint32_t p_x, uint32_t p_y, uint32_t p_z) override {
			CHECK(p_x == 1);
			CHECK(p_y == 2);
			CHECK(p_z == 3);
			++dispatch_calls;
		}

		uint32_t record_native_call_bound(uint32_t slots) const override { return slots; }
		bool native_call_observation_supported() const override { return true; }
		Error native_call_retain(uint64_t serial) override {
			CHECK(serial == 17);
			++retains;
			return retain_error;
		}
		Error native_call_release(uint64_t serial) override {
			CHECK(serial == 17);
			++releases;
			return release_error;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			CHECK(serial == 17);
			++polls;
			return observed;
		}
		FencePollResult native_call_wait(uint64_t) override {
			++waits;
			return {};
		}
		uint64_t limit_get(Limit) override { return 64; }
		uint64_t api_trait_get(ApiTrait trait) override { return trait == API_TRAIT_HONORS_PIPELINE_BARRIERS && honors_barriers ? 1 : 0; }
		uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> sets, ShaderID, uint32_t first, uint32_t count) const override {
			CHECK(sets.size() == 1);
			CHECK(sets[0] == UniformSetID(expected_uniform_set));
			CHECK(first == 0);
			CHECK(count == 1);
			return 0; // The controlled set has no dynamic descriptors.
		}
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID> swaps) override {
			CHECK(swaps.size() == 0);
			uint32_t index = UINT32_MAX;
			if (!claim_native_fact(index)) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			++calls; // This fixture's actual issue frontier.
			NativeCallFact fact;
			fact.serial = 17;
			fact.completion_serial = 73;
			fact.queue_index = 1;
			fact.issue = unknown_issue ? IssueFact::IssuedUnknown : IssueFact::KnownAccepted;
			fact.error = unknown_issue ? FAILED : OK;
			fact.native_code = unknown_issue ? -9 : 0;
			fact.completion = CompletionCapability::Fence;
			finish_native_fact(index, fact, 91);
			return fact.error;
		}
	};
	struct DeviceCleanup {
		RenderingDevice &device;
		~DeviceCleanup() {
			device.frames.clear();
			device.driver = nullptr;
		}
	};
	static Member member() {
		Member result;
		result.source.serial = 17;
		result.source.completion_serial = 73;
		result.source.queue_index = 1;
		result.source.issue = RDD::IssueFact::KnownAccepted;
		result.source.completion = RDD::CompletionCapability::Fence;
		return result;
	}

public:
	static void bounds() {
		using namespace GDExtensionDeviceExit;
		uint32_t slots = 99, frames = 99;
		CHECK(record_source_slot_bound(2, 3, 5, 4, slots, frames) == OK);
		CHECK(slots == 33);
		CHECK(frames == 5);
		CHECK(record_source_slot_bound(0, 3, 5, 4, slots, frames) == ERR_UNAVAILABLE);
		CHECK(slots == 0);
		CHECK(frames == 0);
		CHECK(record_source_slot_bound(1, UINT64_MAX, 0, 0, slots, frames) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(record_source_slot_bound(1, 1, UINT32_MAX, UINT32_MAX, slots, frames) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(qualification_source_slot_bound(3, 0, 2, 12, slots) == OK);
		CHECK(slots == 12);
		CHECK(qualification_source_slot_bound(3, 0, 2, 11, slots) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(slots == 0);
		CHECK(qualification_source_slot_bound(3, 0, 2, 13, slots) == OK);
		CHECK(slots == 12);
	}
	static void native_source() {
		using namespace GDExtensionDeviceExit;
		static_assert(!std::is_default_constructible_v<Origin>);
		Driver driver;
		auto original = member();
		const auto qualification = Origin::qualification();
		driver.retain_error = ERR_BUSY;
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Pending);
		CHECK_FALSE(original.retained);
		CHECK(driver.polls == 0);
		driver.retain_error = OK;
		driver.observed = { true, false, 72, 0 };
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Pending);
		CHECK(original.retained);
		CHECK(driver.retains == 2);
		driver.observed.completed_serial = 73;
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Complete);
		CHECK(original.source.issue == RDD::IssueFact::KnownAccepted);
		CHECK(driver.retains == 2);
		auto uncertain = member();
		uncertain.source.issue = RDD::IssueFact::IssuedUnknown;
		uncertain.source.error = FAILED;
		uncertain.source.native_code = -9;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Unknown);
		CHECK(uncertain.source.issue == RDD::IssueFact::IssuedUnknown);
		CHECK(uncertain.source.native_code == -9);
		const auto production = Origin::production();
		CHECK(poll_native_source(driver, uncertain, production).state == Observation::State::Unknown);
		// A driver-owned same-command callback may resolve a void commit. The
		// original raw issue remains unchanged, and an actual issue error cannot
		// be laundered by this completion field.
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Unknown);
		uncertain.source.error = OK;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Complete);
		CHECK(uncertain.source.issue == RDD::IssueFact::IssuedUnknown);
		driver.observed = { false, true, 73, -21 };
		auto failed = poll_native_source(driver, original, qualification);
		CHECK(failed.state == Observation::State::NativeError);
		CHECK(failed.error == FAILED);
		CHECK(failed.native_code == -21);
		driver.release_error = ERR_BUSY;
		CHECK(release_native_source(driver, original) == ERR_BUSY);
		CHECK_FALSE(original.owner_released);
		driver.release_error = OK;
		CHECK(release_native_source(driver, original) == OK);
		CHECK(original.owner_released);
		CHECK(release_native_source(driver, original) == OK);
		CHECK(driver.releases == 2);
		CHECK(driver.waits == 0);
	}
	static void bounded_capture() {
		Driver driver;
		RDD::NativeCallFact cells[2];
		cells[1].serial = 123;
		RDD::NativeFactWriteRange range;
		range.data = cells;
		range.capacity = 0;
		CHECK(driver.command_queue_execute_recorded(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), range) == ERR_UNAVAILABLE);
		CHECK(driver.calls == 0);
		CHECK(range.written == 0);
		range.capacity = 1;
		CHECK(driver.command_queue_execute_recorded(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), range) == OK);
		CHECK(driver.calls == 1);
		CHECK(range.written == 1);
		CHECK(range.issued);
		CHECK_FALSE(range.coverage_lost);
		CHECK(cells[0].serial == 17);
		CHECK(cells[0].completion_serial == 73);
		CHECK(cells[1].serial == 123);
		CHECK(range.submission_serial == 91);
		CHECK(driver.get_last_queue_call_report().calls.is_empty());
		RDD::QueueCallReport report;
		driver.unknown_issue = true;
		CHECK(driver.command_queue_execute_and_present_observed(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), {}, report) == FAILED);
		REQUIRE(report.calls.size() == 1);
		CHECK(report.calls[0].issue == RDD::IssueFact::IssuedUnknown);
		CHECK(report.calls[0].native_code == -9);
		CHECK(report.issued);
		CHECK(report.submission_serial == 91);
		CHECK(driver.get_last_queue_call_report().calls[0].serial == 17);
	}
	static void request_identity() {
		using namespace GDExtensionDeviceExit;
		const OriginalOperationIdentity owner{ 11, 12, 13, 14, 0, 4 };
		const OriginalOperationIdentity changed{ 11, 12, 13, 14, 0, 5 };
		CHECK(valid_original_operation_identity(owner));
		CHECK(same_original_operation_identity(owner, owner));
		CHECK_FALSE(same_original_operation_identity(owner, changed));
		CheckedRecordAppend observation;
		CHECK(bind_checked_record_append(observation, owner) == OK);
		CHECK(observation.operation_bound);
		CHECK(same_original_operation_identity(observation.operation, owner));
		CHECK(bind_checked_record_append(observation, changed) == ERR_UNAUTHORIZED);
		const OriginalOperationIdentity invalid{ 11, 12, 13, 14, 4, 4 };
		CHECK_FALSE(valid_original_operation_identity(invalid));
		CHECK(bind_checked_record_append(observation, invalid) == ERR_INVALID_DATA);
		const ReadbackSourceState complete{ 101, 11, 4, 8, 8, 8, 8, OK, true };
		CHECK(same_request_readback_complete(complete, 101, 11, 4, 8));
		for (uint32_t mode = 0; mode < 10; ++mode) {
			auto changed_readback = complete;
			switch (mode) {
				case 0:
					++changed_readback.request_serial;
					break;
				case 1:
					++changed_readback.backing_serial;
					break;
				case 2:
					++changed_readback.offset;
					break;
				case 3:
					++changed_readback.size;
					break;
				case 4:
					--changed_readback.issued;
					break;
				case 5:
					--changed_readback.completed;
					break;
				case 6:
					--changed_readback.owned_bytes;
					break;
				case 7:
					changed_readback.error = ERR_CANT_ACQUIRE_RESOURCE;
					break;
				case 8:
					changed_readback.delivered = false;
					break;
				case 9:
					changed_readback.request_serial = 0;
					break;
			}
			CAPTURE(mode);
			CHECK_FALSE(same_request_readback_complete(changed_readback, 101, 11, 4, 8));
		}
		CHECK_FALSE(same_request_readback_complete(complete, 101, 11, UINT64_MAX, 8));
	}
	static void checked_append() {
		using State = GDExtensionDeviceExit::CheckedRecordAppend::State;
		const GDExtensionDeviceExit::OriginalOperationIdentity identity{ 11, 12, 13, 14, 0, 4 };
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Buffer from, to;
		from.driver_id = RDD::BufferID(5);
		from.size = 32;
		from.backing_serial = 5;
		to.driver_id = RDD::BufferID(6);
		to.size = 32;
		to.backing_serial = 6;
		from.draw_tracker = RDG::resource_tracker_create();
		from.draw_tracker->buffer_driver_id = from.driver_id;
		to.draw_tracker = RDG::resource_tracker_create();
		to.draw_tracker->buffer_driver_id = to.driver_id;
		RID src = device.storage_buffer_owner.make_rid(from), dst = device.storage_buffer_owner.make_rid(to);
		uint32_t before = device.draw_graph.qualification_command_count();
		const GDExtensionDeviceExit::OriginalOperationIdentity missing{};
		auto denied = device._record_source_buffer_copy(missing, src, dst, 0, 0, 16);
		CHECK(denied.state == State::Rejected);
		CHECK(denied.error == ERR_INVALID_DATA);
		CHECK_FALSE(denied.operation_bound);
		CHECK(device.draw_graph.qualification_command_count() == before);
		ERR_PRINT_OFF;
		auto bad = device._record_source_buffer_copy(identity, src, dst, 31, 0, 2);
		CHECK(bad.state == State::Rejected);
		CHECK(bad.error == ERR_INVALID_PARAMETER);
		CHECK(device.draw_graph.qualification_command_count() == before);
		CHECK(device.buffer_copy(src, dst, 31, 0, 2) == ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
		auto appended = device._record_source_buffer_copy(identity, src, dst, 0, 0, 16);
		CHECK(appended.state == State::Appended);
		CHECK(appended.error == OK);
		CHECK(appended.operation_bound);
		CHECK(GDExtensionDeviceExit::same_original_operation_identity(appended.operation, identity));
		CHECK(device.draw_graph.contains_record(appended.handle));
		CHECK(device.draw_graph.qualification_command_count() == before + 1);
		device.storage_buffer_owner.get_or_null(dst)->draw_tracker = nullptr;
		before = device.draw_graph.qualification_command_count();
		CHECK(device._record_source_buffer_copy(identity, src, dst, 0, 0, 16).error == ERR_UNAVAILABLE);
		CHECK(device.draw_graph.qualification_command_count() == before);
		device.compute_list.active = true;
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 0, 1, 1).error == ERR_INVALID_PARAMETER);
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1).error == ERR_INVALID_DATA);
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(7);
		RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(8);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = 16;
		RID pipeline_rid = device.compute_pipeline_owner.make_rid(pipeline);
		device.compute_list.state.pipeline = pipeline_rid;
		device.compute_list.state.pipeline_shader = shader_rid;
		device.compute_list.state.pipeline_shader_driver_id = shader.driver_id;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 16;
#endif
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1).error == ERR_UNAVAILABLE);
		CHECK(device.draw_graph.qualification_command_count() == before);
		device.compute_pipeline_owner.get_or_null(pipeline_rid)->push_constant_size = 0;
		device.compute_list.state.pipeline_shader_driver_id = shader.driver_id;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 0;
#endif
		device.draw_graph.add_compute_list_begin();
		auto dispatched = device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1);
		CHECK(dispatched.state == State::Appended);
		CHECK(dispatched.error == OK);
		CHECK(GDExtensionDeviceExit::same_original_operation_identity(dispatched.operation, identity));
		CHECK(device.draw_graph.contains_record(dispatched.handle));
		device.draw_graph.add_compute_list_end();
		device.compute_list.active = false;
		device.compute_pipeline_owner.free(pipeline_rid);
		device.shader_owner.free(shader_rid);
		RDG::resource_tracker_free(from.draw_tracker);
		RDG::resource_tracker_free(to.draw_tracker);
		device.storage_buffer_owner.free(src);
		device.storage_buffer_owner.free(dst);
		device.driver = nullptr;
	}
	static void normal_push_dispatch() {
		using namespace GDExtensionDeviceExit;
		using State = CheckedRecordAppend::State;
		const OriginalOperationIdentity identity{ 41, 42, 43, 44, 0, 4 };
		const uint32_t push[4] = { 0x12345678, 0xabcdef01, 0x87654321, 0x10fedcba };
		RenderingDevice device;
		Driver driver;
		driver.honors_barriers = false;
		driver.expected_uniform_set = 9;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(7);
		RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(8);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = sizeof(push);
		pipeline.set_formats.push_back(123);
		RID pipeline_rid = device.compute_pipeline_owner.make_rid(pipeline);
		RenderingDevice::UniformSet set;
		set.driver_id = RDD::UniformSetID(9);
		set.shader_id = shader_rid;
		set.shader_set = 0;
		set.format = 123;
		RID set_rid = device.uniform_set_owner.make_rid(set);
		const auto list = device.compute_list_begin();
		device.compute_list_bind_compute_pipeline(list, pipeline_rid);
		device.compute_list_bind_uniform_set(list, set_rid, 0);
		auto rejected = [&](const void *data, uint32_t size, Error error) {
			const auto result = device._record_source_dispatch(identity, list, 1, 2, 3, data, size);
			CHECK(result.state == State::Rejected);
			CHECK(result.error == error);
			CHECK(result.operation_bound);
			CHECK_FALSE(result.handle.is_valid());
			CHECK(device.compute_list.state.push_constant_size == 0);
			CHECK(device.compute_list.state.dispatch_count == 0);
			CHECK_FALSE(device.compute_list.state.sets[0].bound);
		};
		rejected(nullptr, sizeof(push), ERR_UNAVAILABLE);
		rejected(push, 12, ERR_UNAVAILABLE);
		rejected(push, 132, ERR_UNAVAILABLE);
		QualificationPhysicalReturn physical;
		auto *owned_pipeline = device.compute_pipeline_owner.get_or_null(pipeline_rid);
		auto *owned_shader = device.shader_owner.get_or_null(shader_rid);
		auto *owned_set = device.uniform_set_owner.get_or_null(set_rid);
		owned_pipeline->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_pipeline->qualification_return = nullptr;
		owned_shader->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_shader->qualification_return = nullptr;
		owned_set->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_set->qualification_return = nullptr;
		owned_set->qualification_dependencies.push_back(&physical);
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_set->qualification_dependencies.clear();
		owned_set->format = 124;
		rejected(push, sizeof(push), ERR_UNAVAILABLE);
		owned_set->format = 123;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = false;
		rejected(push, sizeof(push), ERR_INVALID_DATA);
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 12;
		rejected(push, sizeof(push), ERR_INVALID_DATA);
		device.compute_list.validation.pipeline_push_constant_size = sizeof(push);
#endif
		CHECK_FALSE(device.qualification_recording);
		const auto ordinary = device._record_source_dispatch(identity, list, 1, 2, 3, push, sizeof(push));
		CHECK(ordinary.error == OK);
		CHECK(ordinary.state == State::Appended);
		CHECK(same_original_operation_identity(ordinary.operation, identity));
		CHECK(device.draw_graph.contains_record(ordinary.handle));
		CHECK(device.compute_list.state.push_constant_size == sizeof(push));
		CHECK(memcmp(device.compute_list.state.push_constant_data, push, sizeof(push)) == 0);
		CHECK(device.compute_list.state.sets[0].bound);
		CHECK_FALSE(device.qualification_recording);
		// Existing qualification context remains distinct; this is a fake-owner
		// control, not a production admission grant or a new flag setter API.
		device.qualification_recording = true;
		owned_pipeline->qualification_return = &physical;
		owned_shader->qualification_return = &physical;
		owned_set->qualification_return = &physical;
		device.compute_list_bind_uniform_set(list, set_rid, 0);
		const auto qualified = device._record_source_dispatch(identity, list, 1, 2, 3, push, sizeof(push));
		device.qualification_recording = false;
		CHECK(qualified.error == OK);
		CHECK(qualified.state == State::Appended);
		CHECK(device.draw_graph.contains_record(qualified.handle));
		CHECK(ordinary.handle.graph_instance == qualified.handle.graph_instance);
		CHECK(ordinary.handle.graph_frame == qualified.handle.graph_frame);
		CHECK(ordinary.handle.index == qualified.handle.index);
		CHECK(ordinary.handle.offset < qualified.handle.offset);
		device.compute_list_end();
		CHECK(device.draw_graph.contains_record(ordinary.handle));
		CHECK(device.draw_graph.contains_record(qualified.handle));
		RDG::CommandBufferPool pool;
		RDD::CommandBufferID primary(60);
		device.draw_graph.end(true, false, primary, pool);
		CHECK(driver.push_calls == 2);
		CHECK(driver.prepare_calls == 2);
		CHECK(driver.set_calls == 2);
		CHECK(driver.dispatch_calls == 2);
		CHECK(memcmp(driver.pushed_words, push, sizeof(push)) == 0);
		device.uniform_set_owner.free(set_rid);
		device.compute_pipeline_owner.free(pipeline_rid);
		device.shader_owner.free(shader_rid);
		device.driver = nullptr;
	}

	static void checked_indirect_draw() {
		using namespace GDExtensionDeviceExit;
		using State = CheckedRecordAppend::State;
		const OriginalOperationIdentity identity{ 31, 32, 33, 34, 4, 8 };
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.is_main_instance = false; // Same owner-thread semantics as copy/dispatch.
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		device.draw_graph.add_draw_list_begin(RDD::RenderPassID(1), RDD::FramebufferID(2), Rect2i(), {}, {}, {}, 0, false);
		device.draw_list.active = true;
#ifdef DEBUG_ENABLED
		device.draw_list.validation.pipeline_active = true;
		device.draw_list.validation.pipeline_vertex_format = RenderingDevice::INVALID_ID;
#endif
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RDD::BufferID(41);
		buffer.size = 32;
		buffer.usage = RDD::BUFFER_USAGE_INDIRECT_BIT;
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		buffer.transfer_worker_index = 0;
		buffer.transfer_worker_operation = 9;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker->index = 0;
		device.transfer_worker_pool.push_back(worker);
		device.transfer_worker_pool_size = 1;
		device.transfer_worker_operation_used_by_draw.resize(1);
		device.transfer_worker_operation_used_by_draw[0] = 0;
		const auto missing = device._record_source_draw_indirect({}, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 1, 20);
		CHECK(missing.observed.error == ERR_INVALID_DATA);
		CHECK(missing.observed.state == State::Rejected);
		CHECK_FALSE(missing.observed.operation_bound);
		CHECK(missing.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK(missing.native.prefix_count == 0);
		CHECK_FALSE(missing.native.draw_handle.is_valid());
		device.render_thread_id ^= 1;
		const auto wrong_thread = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 1, 20);
		device.render_thread_id = Thread::get_caller_id();
		CHECK(wrong_thread.observed.error == ERR_UNAUTHORIZED);
		CHECK(wrong_thread.observed.state == State::Rejected);
		CHECK(wrong_thread.observed.operation_bound);
		CHECK(same_original_operation_identity(wrong_thread.observed.operation, identity));
		CHECK(wrong_thread.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(wrong_thread.native.draw_handle.is_valid());
		const auto wrong_list = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, false, rid, 0, 1, 20);
		CHECK(wrong_list.observed.error == ERR_INVALID_PARAMETER);
		CHECK(wrong_list.observed.state == State::Rejected);
		CHECK(wrong_list.observed.operation_bound);
		CHECK(wrong_list.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(wrong_list.native.draw_handle.is_valid());
		CHECK(device.draw_list.state.draw_count == 0);
		ERR_PRINT_OFF;
		const auto bad_span = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 2, 20);
		CHECK(bad_span.observed.error == ERR_INVALID_PARAMETER);
		CHECK(bad_span.native.error == bad_span.observed.error);
		CHECK(bad_span.observed.state == State::Rejected);
		CHECK(bad_span.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(bad_span.observed.handle.is_valid());
		CHECK_FALSE(bad_span.native.capture_complete);
		CHECK(bad_span.native.prefix_count == 0);
		CHECK(device.draw_list.state.draw_count == 0);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->transfer_worker_index == 0);
		device.frame_wait_error = ERR_CANT_ACQUIRE_RESOURCE;
		const auto suspended = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 1, 20);
		device.frame_wait_error = OK;
		ERR_PRINT_ON;
		CHECK(suspended.observed.error == ERR_CANT_ACQUIRE_RESOURCE);
		CHECK(suspended.native.error == ERR_CANT_ACQUIRE_RESOURCE);
		CHECK(suspended.observed.state == State::Rejected);
		const auto drawn = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 1, 20);
		CHECK(drawn.observed.error == OK);
		CHECK(drawn.observed.state == State::Appended);
		CHECK(drawn.observed.operation_bound);
		CHECK(same_original_operation_identity(drawn.observed.operation, identity));
		CHECK(device.draw_graph.contains_record(drawn.observed.handle));
		CHECK(drawn.observed.handle.graph_instance == drawn.native.draw_handle.graph_instance);
		CHECK(drawn.observed.handle.graph_frame == drawn.native.draw_handle.graph_frame);
		CHECK(drawn.observed.handle.index == drawn.native.draw_handle.index);
		CHECK(drawn.observed.handle.offset == drawn.native.draw_handle.offset);
		CHECK(drawn.native.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(drawn.native.capture_complete);
		CHECK(drawn.native.has_transfer);
		CHECK(drawn.native.transfer.worker == 0);
		CHECK(drawn.native.transfer.operation == 9);
		CHECK(device.transfer_worker_operation_used_by_draw[0] == 9);
		RenderingDevice::UniformSet set;
		set.format = 1;
		set.driver_id = RDD::UniformSetID(51);
		RID set_rid = device.uniform_set_owner.make_rid(set);
		device.draw_list.state.set_count = 1;
		auto &binding = device.draw_list.state.sets[0];
		binding.uniform_set = set_rid;
		binding.uniform_set_driver_id = set.driver_id;
		binding.pipeline_expected_format = 1;
		binding.uniform_set_format = 1;
		binding.bound = false;
#ifdef DEBUG_ENABLED
		ERR_PRINT_OFF;
		const auto prefix = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, true, rid, 0, 1, 20);
		ERR_PRINT_ON;
		CHECK(prefix.observed.error == ERR_INVALID_PARAMETER);
		CHECK(prefix.native.error == prefix.observed.error);
		CHECK(prefix.observed.state == State::Unknown);
		CHECK(same_original_operation_identity(prefix.observed.operation, identity));
		CHECK(prefix.native.effect == RenderingDevice::NativeAppendEffect::NativePrefixOnly);
		CHECK(prefix.native.capture_complete);
		CHECK(prefix.native.prefix_count == 1);
		CHECK(device.draw_graph.contains_record(prefix.native.prefix_handles[0]));
		CHECK_FALSE(prefix.observed.handle.is_valid());
		CHECK(device.draw_list.state.draw_count == 1);
		device.draw_list.validation.index_array_count = 1;
		binding.bound = false;
#endif
		const auto indexed = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, true, rid, 0, 1, 20);
		CHECK(indexed.observed.error == OK);
		CHECK(indexed.observed.state == State::Appended);
		CHECK(same_original_operation_identity(indexed.observed.operation, identity));
		CHECK(device.draw_graph.contains_record(indexed.observed.handle));
		CHECK(indexed.native.prefix_count == 1);
		CHECK(device.draw_graph.contains_record(indexed.native.prefix_handles[0]));
		const auto zero = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 0, 0);
		CHECK(zero.observed.state == State::Appended);
		CHECK(zero.observed.error == OK);
		CHECK(device.draw_graph.contains_record(zero.native.draw_handle));
		CHECK(device.draw_list.state.draw_count == 3);
		device.draw_graph.add_draw_list_end();
		device.draw_list.active = false;
		RDG::resource_tracker_free(buffer.draw_tracker);
		device.storage_buffer_owner.free(rid);
		device.uniform_set_owner.free(set_rid);
		device.transfer_worker_pool.clear();
		device.transfer_worker_pool_size = 0;
		memdelete(worker);
		device.driver = nullptr;
	}

	static void original_readback_identity() {
		GDExtensionDeviceExit::QualificationTicket ticket;
		ticket.original_readback_serial = 101;
		ticket.origin.observed_backing_serial = 11;
		ticket.origin.readback_offset = 4;
		ticket.origin.readback_size = 8;
		ticket.result.instantiate();
		ticket.result->request_serial = 101;
		ticket.result->source_backing_serial = 11;
		ticket.result->source_offset = 4;
		ticket.result->source_size = 8;
		ticket.result->issued_size = ticket.result->completed_size = 8;
		ticket.result->bytes.resize(8);
		ticket.result->delivered = true;
		CHECK(ticket.readback_complete(0));
		ticket.result->request_serial = 102;
		CHECK_FALSE(ticket.readback_complete(0));
		ticket.result->request_serial = 101;
		ticket.result->source_backing_serial = 12;
		CHECK_FALSE(ticket.readback_complete(0));
		ticket.result->source_backing_serial = 11;
		ticket.result->error = ERR_CANT_ACQUIRE_RESOURCE;
		CHECK_FALSE(ticket.readback_complete(0));
	}
	static void original_ticket_poll() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 1;
		QualificationTicket ticket;
		ticket.report.revision = 1;
		GDExtensionQualificationFactV1 graph{};
		graph.command_slot = 0;
		ticket.facts.push_back(graph);
		ticket.graphs.push_back({ {}, 0, 0, 0, 1, true, false });
		device.frames[0].qualification_tickets.push_back(&ticket);
		auto source = member();
		source.source.issue = RDD::IssueFact::IssuedUnknown;
		source.source.error = FAILED;
		source.source.native_code = 88;
		RDD::QueueCallReport report;
		report.calls.push_back(source.source);
		report.error = FAILED;
		report.native_code = 88;
		report.issued = true;
		device._qualification_issue_tickets(0, report, FAILED);
		REQUIRE(ticket.natives.size() == 1);
		const uint32_t native_fact = ticket.natives[0].fact;
		CHECK(ticket.report.native_error == FAILED);
		CHECK(ticket.report.native_code == 88);
		driver.observed = { true, false, 73, 0 };
		driver.retain_error = ERR_BUSY;
		CHECK(device._qualification_poll_tickets(0) == ERR_BUSY);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.report.native_code == 88);
		CHECK_FALSE(ticket.natives[0].retained);
		driver.retain_error = ERR_DOES_NOT_EXIST;
		CHECK(device._qualification_poll_tickets(0) == FAILED);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.facts[native_fact].error == FAILED);
		CHECK(ticket.report.native_code == 88);
		CHECK(ticket.mapping_failed);
		CHECK_FALSE(ticket.natives[0].retained);
		CHECK(ticket.natives[0].retention_error == ERR_DOES_NOT_EXIST);
		driver.retain_error = OK;
		CHECK(device._qualification_poll_tickets(0) == FAILED);
		CHECK(ticket.facts[native_fact].issue == uint32_t(RDD::IssueFact::IssuedUnknown));
		CHECK(ticket.facts[native_fact].completion == 4);
		CHECK(ticket.report.state == 6);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.facts[native_fact].error == FAILED);
		CHECK(ticket.report.native_code == 88);
		CHECK(ticket.natives[0].source.native_code == 88);
		CHECK(ticket.natives[0].completion_native_code == 0);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(driver.waits == 0);
		QualificationTicket resolved;
		resolved.report.revision = 1;
		auto void_commit = member();
		void_commit.source.issue = RDD::IssueFact::IssuedUnknown;
		resolved.natives.push_back(void_commit);
		resolved.facts.push_back({});
		device.frames[0].qualification_tickets[0] = &resolved;
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(device._qualification_poll_tickets(0) == OK);
		CHECK(resolved.facts[0].issue == uint32_t(RDD::IssueFact::KnownAccepted));
		CHECK(resolved.facts[0].completion == 2);
		CHECK(resolved.natives[0].source.issue == RDD::IssueFact::IssuedUnknown);
		CHECK(resolved.report.state == 4);
		device.frames.clear();
		device.driver = nullptr;
	}
};
TEST_CASE("[RecordSource] bounded admission arithmetic retains qualification policy") {
	TestRecordSourceAccess::bounds();
}
TEST_CASE("[RecordSource] exact native sources distinguish authoritative issue resolution") {
	TestRecordSourceAccess::native_source();
}
TEST_CASE("[RecordSource] original observed queue uses fixed callsite capture") {
	TestRecordSourceAccess::bounded_capture();
}
TEST_CASE("[RecordSource] readback matches the original request backing and range") {
	TestRecordSourceAccess::request_identity();
}
TEST_CASE("[RecordSource] checked original RD copy preserves Error and actual graph frontier") {
	TestRecordSourceAccess::checked_append();
}
TEST_CASE("[RecordSource] original qualification reducer consumes shared native observations") {
	TestRecordSourceAccess::original_ticket_poll();
}

TEST_CASE("[RecordSource] original ticket readback rejects replaced request and backing") {
	TestRecordSourceAccess::original_readback_identity();
}

TEST_CASE("[RecordSource] original indirect draw retains exact identity native prefix and transfer") {
	TestRecordSourceAccess::checked_indirect_draw();
}

TEST_CASE("[RecordSource] ordinary push and backend descriptor preparation preserve original dispatch") {
	TestRecordSourceAccess::normal_push_dispatch();
}
