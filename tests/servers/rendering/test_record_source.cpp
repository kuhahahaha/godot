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
		uint64_t api_trait_get(ApiTrait trait) override { return trait == API_TRAIT_HONORS_PIPELINE_BARRIERS ? 1 : 0; }
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
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1).error == ERR_UNAVAILABLE);
		CHECK(device.draw_graph.qualification_command_count() == before);
		device.compute_pipeline_owner.get_or_null(pipeline_rid)->push_constant_size = 0;
		device.compute_list.state.pipeline_shader_driver_id = shader.driver_id;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = true;
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
