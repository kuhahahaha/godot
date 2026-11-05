/**************************************************************************/
/*  test_qualification_ticket.cpp                                         */
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
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_server.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <thread>
#if defined(VULKAN_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#endif
TEST_FORCE_LINK(test_qualification_ticket)

// These controls feed deliberately distinct source observations into the
// original ticket reducer. They do not issue Registry authority or certify a
// native dispatch; the actual Metal control remains separately required.
class TestQualificationTicketAccess {
public:
	static void checked_depth_draw_records() {
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin(false);
		// Minimal original RD owners, not a new production injection endpoint.
		RDG::ResourceTracker tracker;
		tracker.texture_driver_id = RDD::TextureID(72);
		RenderingDevice::Texture texture;
		texture.format = RenderingDevice::DATA_FORMAT_D32_SFLOAT;
		texture.usage_flags = RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		texture.draw_tracker = &tracker;
		const RID target = device.texture_owner.make_rid(texture);
		RenderingDevice::FramebufferFormatKey key;
		key.passes.push_back(RenderingDevice::FramebufferPass());
		key.passes.write[0].depth_attachment = 0;
		RenderingDevice::FramebufferFormat format;
		format.E = device.framebuffer_format_cache.insert(key, 1);
		device.framebuffer_formats[1] = format;
		RDG::FramebufferCache cache;
		cache.trackers.push_back(&tracker);
		RenderingDevice::Framebuffer framebuffer;
		framebuffer.format_id = 1;
		framebuffer.texture_ids.push_back(target);
		framebuffer.framebuffer_cache = &cache;
		framebuffer.size = Size2(8, 4);
		framebuffer.view_count = 1;
		const RID framebuffer_rid = device.framebuffer_owner.make_rid(framebuffer);
		RenderingDevice::RenderPipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(5);
#ifdef DEBUG_ENABLED
		pipeline.validation.framebuffer_format = 1;
		pipeline.validation.vertex_format = RenderingDevice::INVALID_ID;
		pipeline.validation.primitive_minimum = pipeline.validation.primitive_divisor = 3;
#endif
		const RID pipeline_rid = device.render_pipeline_owner.make_rid(pipeline);
		const auto begin = device._draw_list_begin_recorded(framebuffer_rid, RenderingDevice::DRAW_CLEAR_DEPTH, {}, 0.0f, 0, Rect2(), 0);
		CHECK(begin.error == OK);
		CHECK(begin.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(begin.capture_complete);
		CHECK(begin.preparation_count == 0);
		CHECK(begin.prefix_count == 2);
		CHECK(device.draw_graph.contains_record(begin.begin_handle));
		CHECK(device.draw_graph.contains_record(begin.clear_handle));
		const auto bind = device._draw_list_bind_render_pipeline_recorded(begin.list, pipeline_rid);
		CHECK(bind.error == OK);
		CHECK(bind.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(bind.capture_complete);
		const auto scissor = device._draw_list_enable_scissor_recorded(begin.list, Rect2(0, 0, 4, 4));
		CHECK(scissor.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(scissor.capture_complete);
		const auto empty = device._draw_list_enable_scissor_recorded(begin.list, Rect2(9, 9, 1, 1));
		CHECK(empty.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(empty.capture_complete);
		CHECK_FALSE(empty.draw_handle.is_valid());
		const auto draw = device._draw_list_draw_recorded(begin.list, false, 1, 3);
		CHECK(draw.error == OK);
		CHECK(draw.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(draw.capture_complete);
		ERR_PRINT_OFF;
		const auto refused = device._draw_list_draw_recorded(begin.list, false, 2, 3);
		ERR_PRINT_ON;
		CHECK(refused.error == ERR_UNAVAILABLE);
		CHECK(refused.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(refused.capture_complete);
		CHECK_FALSE(refused.draw_handle.is_valid());
		const auto end = device._draw_list_end_recorded();
		CHECK(end.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(end.capture_complete);
		CHECK_FALSE(device.draw_list.active);
		CHECK_FALSE(device.texture_owner.get_or_null(target)->bound);
		const RDG::RecordHandle records[] = { begin.begin_handle, begin.clear_handle, begin.prefix_handles[0], begin.prefix_handles[1], bind.draw_handle, scissor.draw_handle, draw.draw_handle, end.draw_handle };
		for (const auto &record : records) {
			CHECK(device.draw_graph.contains_record(record));
			CHECK(device.draw_graph.record_mapping_state(record) == RDG::RecordMappingState::Pending);
		}
		// Actual graph records exist even though no native command buffer ran.
		// Prefix retention here is not native issue/completion or GPU acceptance.
		device.draw_graph.begin();
		for (const auto &record : records) {
			CHECK_FALSE(device.draw_graph.contains_record(record));
		}
		device.render_pipeline_owner.free(pipeline_rid);
		device.framebuffer_owner.free(framebuffer_rid);
		device.texture_owner.free(target);
		device.framebuffer_formats.clear();
		device.framebuffer_format_cache.clear();
		device.driver = nullptr;
	}
	static Error original_upload_capacity(RenderingDeviceDriver &driver, uint32_t result_bytes, uint32_t workers) {
		RenderingDevice device;
		device.driver = &driver;
		device.transfer_worker_pool_max_size = workers;
		GDExtensionDeviceExit::QualificationTicket ticket;
		ticket.preparation.expected_result_bytes = result_bytes;
		const Error status = GDExtensionDeviceExit::Registry::get_singleton().qualification_typed_upload_capacity(&ticket, &device, 0);
		CHECK(device.draw_graph.qualification_command_count() == 0);
		CHECK(ticket.facts.is_empty());
		device.driver = nullptr;
		return status;
	}
	static GDExtensionQualificationRecordV2 range_request() {
		GDExtensionQualificationRecordV2 q = {};
		q.abi = { 2, sizeof(GDExtensionDeviceExitAbi), 0 };
		q.record_generation = q.request_nonce = q.library_generation = q.qualification_session_serial = 1;
		q.attempt = 1;
		q.range_end = q.sample_count = 4;
		q.body_groups_x = 1;
		q.capture_set_count = q.body_set_count = 2;
		q.capture_set1 = 21;
		q.body_set1 = 23;
		q.readback_count = 3;
		q.readback_size = 44;
		q.readbacks[0] = { 31, 41, 51, 0, 24, 0, GDEXTENSION_QUALIFICATION_READBACK_METHOD_RAW_RESULTS_V1 };
		q.readbacks[1] = { 32, 42, 52, 0, 16, 24, GDEXTENSION_QUALIFICATION_READBACK_METHOD_FAULT_PLANE_V1 };
		q.readbacks[2] = { 33, 43, 53, 0, 4, 40, GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1 };
		return q;
	}
	static void range_descriptors() {
		using namespace GDExtensionDeviceExit;
		auto q = range_request();
		CHECK(validate_qualification_record_v2(q) == OK);
		for (uint32_t mode = 0; mode < 17; ++mode) {
			auto bad = q;
			CAPTURE(mode);
			switch (mode) {
				case 0:
					bad.abi.abi_version = 1;
					break;
				case 1:
					bad.readback_count = 2;
					break;
				case 2:
					bad.readback_count = 4;
					break;
				case 3:
					bad.readback_size = 43;
					break;
				case 4:
					bad.readbacks[1].result_offset = 25;
					break;
				case 5:
					bad.readbacks[1].result_offset = 23;
					break;
				case 6:
					bad.readbacks[0].readback_size = 0;
					break;
				case 7:
					bad.readbacks[0].readback_offset = UINT64_MAX;
					break;
				case 8:
					bad.readbacks[0].observed_backing_serial = 0;
					break;
				case 9:
					bad.readbacks[0].source_allocation_generation = 0;
					break;
				case 10:
					bad.readbacks[1].role = GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1;
					break;
				case 11:
					bad.readbacks[1].readback_buffer = bad.readbacks[0].readback_buffer;
					break;
				case 12:
					bad.readbacks[2].readback_size = 8;
					bad.readback_size += 4;
					break;
				case 13:
					bad.capture_set_count = 1;
					break;
				case 14:
					bad.body_set_count = 3;
					break;
				case 15:
					bad.body_set1 = 0;
					break;
				case 16:
					bad.readback_size = 16385;
					break;
			}
			CHECK(validate_qualification_record_v2(bad) != OK);
		}
		q.readbacks[0].role = GDEXTENSION_QUALIFICATION_READBACK_METHOD_DESTINATION_V1;
		CHECK(validate_qualification_record_v2(q) == OK);
		q = range_request();
		q.readback_count = 1;
		q.readback_size = 24;
		q.readbacks[0].role = GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1;
		q.readbacks[1] = {};
		q.readbacks[2] = {};
		q.capture_set_count = q.body_set_count = 1;
		q.capture_set1 = q.body_set1 = 0;
		CHECK(validate_qualification_record_v2(q) == OK);
		q.readbacks[2].readback_buffer = 7;
		CHECK(validate_qualification_record_v2(q) == ERR_INVALID_PARAMETER);
	}

	static void empty_range_dispatch() {
		using namespace GDExtensionDeviceExit;
		auto request = range_request();
		request.sample_count = 0;
		request.range_begin = request.range_end = 0;
		request.body_groups_x = 1;
		CHECK(validate_qualification_record_v2(request) == OK);
		auto invalid = request;
		invalid.body_groups_x = 0;
		CHECK(validate_qualification_record_v2(invalid) == ERR_INVALID_PARAMETER);
		invalid = request;
		invalid.range_begin = invalid.range_end = 1;
		CHECK(validate_qualification_record_v2(invalid) == ERR_INVALID_PARAMETER);
		const auto legacy = qualification_record_v1(request);
		auto *registry = memnew(Registry);
		registry->records[0].snapshot.library_generation = 1;
		Registry::QualificationBridge bridge;
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		LocalVector<RID> resources;
		// This unsupported driver deliberately stops after the wire/empty-domain
		// checks, before any fake resource or native dispatch could be admitted.
		CHECK(registry->qualification_record_preflight(&bridge, &device, legacy, resources) == ERR_INVALID_PARAMETER);
		CHECK(registry->qualification_record_preflight(&bridge, &device, legacy, resources, request.capture_set1, request.body_set1, true) == ERR_UNAVAILABLE);
		CHECK(resources.is_empty());
		CHECK(device.draw_graph.qualification_command_count() == 0);
		device.driver = nullptr;
		memdelete(registry);
	}
	static void range_completion(uint32_t mode) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = registry->private_limit;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		auto &native = registry->device_records[0];
		native.object = &device;
		native.live = true;
		native.epoch = 1;
		native.execution_thread = Thread::get_caller_id();
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->prepared = ticket->multi_readback = ticket->recorded = ticket->ordering_closed = true;
		ticket->range_origin = range_request();
		ticket->report.revision = 1;
		ticket->frame_refs = 1;
		bridge->tickets[0] = ticket;
		uint8_t expected[44];
		for (uint32_t i = 0; i < 44; ++i) {
			expected[i] = uint8_t(i + 7);
		}
		for (uint32_t i = 0; i < 3; ++i) {
			const auto range = ticket->readback_range(i);
			auto &owned = ticket->readback_result(i);
			owned.instantiate();
			owned->qualification_owned = true;
			owned->request_serial = 101 + i;
			if (i) {
				ticket->additional_readback_serials[i - 1] = 101 + i;
			} else {
				ticket->original_readback_serial = 101;
			}
			owned->source_backing_serial = range.observed_backing_serial;
			owned->source_offset = range.readback_offset;
			owned->source_size = range.readback_size;
			owned->bytes.resize(range.readback_size);
			memcpy(owned->bytes.ptrw(), expected + range.result_offset, range.readback_size);
			owned->issued_size = range.readback_size;
			GDExtensionQualificationFactV1 fact = {};
			fact.semantic_kind = 3;
			fact.command_slot = 0;
			ticket->graphs.push_back({ {}, ticket->facts.size(), 0, 0, 1, true, false });
			ticket->facts.push_back(fact);
		}
		RDD::NativeCallFact source;
		source.serial = 17;
		source.completion_serial = 73;
		source.queue_index = 1;
		GDExtensionQualificationFactV1 fact = {};
		fact.issue = 1;
		fact.completion = 2;
		ticket->natives.push_back({ source, ticket->facts.size(), 0, 0, 1, false, false });
		ticket->facts.push_back(fact);
		registry->qualification_ticket_publish(ticket, &device);
		CHECK(ticket->report.state != 8);
		CHECK_FALSE(ticket->physically_retired.load());
		uint8_t actual[44];
		memset(actual, 0xa5, sizeof(actual));
		uint32_t written = 99;
		CHECK(registry->qualification_copy({ 71, 11 }, { 71, 23 }, ticket->report.revision, actual, sizeof(actual), &written).native_error == ERR_BUSY);
		CHECK(written == 0);
		CHECK(actual[0] == 0xa5);
		for (uint32_t i = 0; i < 3; ++i) {
			auto &owned = ticket->readback_result(i);
			owned->completed_size = owned->bytes.size();
			owned->delivered = true;
			registry->qualification_ticket_publish(ticket, &device);
			CHECK(ticket->report.state != 8);
			CHECK(ticket->report.owned_size == 0);
		}
		if (mode == 1) {
			ticket->readback_result(1)->error = ERR_CANT_ACQUIRE_RESOURCE;
			ticket->readback_result(1)->native_code = -55;
			registry->qualification_ticket_publish(ticket, &device);
			ticket->readback_result(2)->error = FAILED;
			ticket->readback_result(2)->native_code = -77;
		}
		if (mode == 2) {
			CHECK(registry->qualification_cancel({ 71, 11 }, { 71, 23 }).native_error == OK);
			CHECK(ticket->admission_closed.load());
		}
		for (auto &graph : ticket->graphs) {
			graph.returned = true;
		}
		ticket->frame_refs = 0;
		registry->qualification_ticket_publish(ticket, &device);
		GDExtensionQualificationReportV2 report = {};
		registry->qualification_report_v2(ticket, report);
		CHECK(report.abi.abi_version == 2);
		CHECK(report.readback_count == 3);
		CHECK(report.readback_size == 44);
		for (uint32_t i = 0; i < 3; ++i) {
			CHECK(report.readbacks[i].readback_request == 101 + i);
			CHECK(memcmp(&report.readbacks[i].source, &ticket->range_origin.readbacks[i], sizeof(GDExtensionQualificationReadbackRangeV1)) == 0);
		}
		CHECK(ticket->physically_retired.load());
		written = 99;
		auto copy = registry->qualification_copy({ 71, 11 }, { 71, 23 }, report.revision, actual, sizeof(actual), &written);
		if (mode == 0) {
			CHECK(report.state == 8);
			CHECK(report.owned_size == 44);
			CHECK(copy.native_error == OK);
			CHECK(written == 44);
			CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
			auto &later = ticket->readback_result(1);
			const uint64_t original_request = later->request_serial;
			later->request_serial = original_request + 1;
			memset(actual, 0xa5, sizeof(actual));
			uint8_t untouched[sizeof(actual)];
			memset(untouched, 0xa5, sizeof(untouched));
			written = 99;
			CHECK(registry->qualification_copy({ 71, 11 }, { 71, 23 }, report.revision, actual, sizeof(actual), &written).native_error == ERR_BUSY);
			CHECK(written == 0);
			CHECK(memcmp(actual, untouched, sizeof(actual)) == 0);
			later->request_serial = original_request;
			const uint64_t original_backing = later->source_backing_serial;
			later->source_backing_serial = original_backing + 1;
			written = 99;
			CHECK(registry->qualification_copy({ 71, 11 }, { 71, 23 }, report.revision, actual, sizeof(actual), &written).native_error == ERR_BUSY);
			CHECK(written == 0);
			CHECK(memcmp(actual, untouched, sizeof(actual)) == 0);
			later->source_backing_serial = original_backing;
			written = 99;
			CHECK(registry->qualification_copy({ 71, 11 }, { 71, 23 }, report.revision, actual, sizeof(actual), &written).native_error == OK);
			CHECK(written == 44);
			CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
			written = 99;
			CHECK(registry->qualification_copy({ 71, 11 }, { 71, 23 }, report.revision - 1, actual, sizeof(actual), &written).native_error == ERR_BUSY);
			CHECK(written == 0);
		} else {
			CHECK(report.state != 8);
			CHECK(report.owned_size == 0);
			CHECK(copy.native_error == ERR_BUSY);
			CHECK(written == 0);
			if (mode == 1) {
				CHECK(report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
				CHECK(report.native_code == -55);
			}
		}
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		device.driver = nullptr;
		memdelete(ticket);
		memdelete(bridge);
		memdelete(registry);
	}
	static void body_source_scope() {
		using namespace GDExtensionDeviceExit;
		QualificationTicket ticket;
		GDExtensionQualificationFactV1 body = {};
		body.semantic_kind = 2;
		body.command_slot = 3;
		body.issue = 1;
		body.completion = 2;
		ticket.facts.push_back(body);
		ticket.graphs.push_back({ {}, 0, 0, 0, 1, true, true });
		RDD::NativeCallFact source;
		GDExtensionQualificationFactV1 known = {};
		known.issue = 1;
		known.completion = 2;
		ticket.natives.push_back({ source, 1, 0, 3, 1, false, false });
		ticket.facts.push_back(known);
		CHECK(ticket.body_returned_successfully());
		GDExtensionQualificationFactV1 pending = {};
		pending.completion = 1;
		ticket.natives.push_back({ source, 2, 1, UINT32_MAX, 2, false, false });
		ticket.facts.push_back(pending);
		CHECK(ticket.body_returned_successfully());
		ticket.natives[1].frame = 0;
		ticket.natives[1].epoch = 1;
		CHECK_FALSE(ticket.body_returned_successfully());
		ticket.facts[2].issue = 1;
		ticket.facts[2].completion = 2;
		CHECK(ticket.body_returned_successfully());
		ticket.graphs[0].returned = false;
		CHECK_FALSE(ticket.body_returned_successfully());
	}

#ifdef THREADS_ENABLED
	static void replay_under_contention(bool p_prepared = false) {
		using namespace GDExtensionDeviceExit;
		// Seed only original Registry cells; this exercises replay, not issuance.
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 7;
		auto &native = registry->device_records[0];
		native.object = &device;
		native.live = true;
		native.epoch = 1;
		native.execution_thread = Thread::get_caller_id();
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->origin.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		ticket->origin.record_generation = 37;
		ticket->origin.request_nonce = 39;
		ticket->report.revision = 5;
		ticket->report.state = 4;
		ticket->prepared = p_prepared;
		ticket->preparation.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		ticket->preparation.record_generation = 37;
		ticket->preparation.request_nonce = 39;
		ticket->preparation.library_generation = 1;
		ticket->preparation.qualification_session_serial = 7;
		ticket->preparation.parent_serial = 31;
		ticket->preparation.reservation_serial = 97;
		ticket->preparation.attempt = 1;
		ticket->preparation.upload_count = 2;
		ticket->preparation.max_segments = 2;
		ticket->preparation.expected_result_bytes = 16;
		ticket->uploads.resize(2);
		for (uint32_t i = 0; i < 2; ++i) {
			auto &plan = ticket->uploads[i].plan;
			plan.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
			plan.upload_index = i;
			plan.role = i;
			plan.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1;
			plan.byte_size = 32;
		}
		bridge->tickets[0] = ticket;
		Semaphore locked, release;
		std::thread holder_thread([&] { device._thread_safe_.lock(); locked.post(); release.wait(); device._thread_safe_.unlock(); });
		locked.wait();
		GDExtensionQualificationTicketRefV1 out = {};
		GDExtensionQualificationReportV1 report = {};
		auto request = ticket->origin;
		auto prepare = ticket->preparation;
		GDExtensionQualificationUploadPlanV1 plans[] = { ticket->uploads[1].plan, ticket->uploads[0].plan };
		auto invoke = [&] { return p_prepared ? registry->qualification_prepare_uploads({ 71, 11 }, &prepare, plans, 2, &out, &report) : registry->qualification_record({ 71, 11 }, &request, &out, &report); };
		auto status = invoke();
		CHECK(status.native_error == OK);
		CHECK(out.registry_instance == 71);
		CHECK(out.handle_serial == 23);
		CHECK(memcmp(&report, &ticket->report, sizeof(report)) == 0);
		out = {};
		request.body_groups_x++;
		plans[0].role++;
		CHECK(invoke().native_error == ERR_INVALID_DATA);
		CHECK(out.handle_serial == 0);
		request = ticket->origin;
		plans[0] = ticket->uploads[1].plan;
		ticket->acknowledged = true;
		CHECK(invoke().native_error == ERR_DOES_NOT_EXIST);
		ticket->acknowledged = false;
		request.request_nonce++;
		prepare.request_nonce++;
		CHECK(invoke().native_error == ERR_BUSY);
		CHECK(bridge->children == 1);
		CHECK(record.private_refs == 7);
		CHECK(device.draw_graph.qualification_command_count() == 0);
		release.post();
		holder_thread.join();
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(ticket);
		memdelete(bridge);
		memdelete(registry);
	}
#endif
	static void snapshot_output_overlap() {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		GDExtensionQualificationBridgeRefV1 bridge = {};
		GDExtensionQualificationTicketRefV1 ticket = {};
		GDExtensionQualificationFactV1 fact = {};
		fact.graph_kind = 0x12345678;
		const GDExtensionQualificationFactV1 original = fact;
		auto result = registry->qualification_facts(bridge, ticket, 1, 0, 1, &fact, &fact.graph_kind);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&fact, &original, sizeof(fact)) == 0);

		uint32_t destination[] = { 0x12345678, 0x87654321, 0xdeadbeef, 0xa5a5a5a5 };
		const uint32_t original_bytes[] = { 0x12345678, 0x87654321, 0xdeadbeef, 0xa5a5a5a5 };
		result = registry->qualification_copy(bridge, ticket, 1, destination, sizeof(destination), &destination[1]);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(destination, original_bytes, sizeof(destination)) == 0);

		struct alignas(GDExtensionQualificationFactV1) {
			uint32_t written = 0x76543210;
			uint8_t rest[sizeof(GDExtensionQualificationFactV1) - sizeof(uint32_t)] = {};
		} zero_capacity_fact;
		const auto original_zero_capacity_fact = zero_capacity_fact;
		result = registry->qualification_facts(bridge, ticket, 1, 0, 0, reinterpret_cast<GDExtensionQualificationFactV1 *>(&zero_capacity_fact), &zero_capacity_fact.written);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&zero_capacity_fact, &original_zero_capacity_fact, sizeof(zero_capacity_fact)) == 0);
		result = registry->qualification_copy(bridge, ticket, 1, &destination[1], 0, &destination[1]);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(destination, original_bytes, sizeof(destination)) == 0);

		uint32_t written = 99;
		result = registry->qualification_facts(bridge, ticket, 1, 0, 1, &fact, &written);
		CHECK(result.native_error != OK);
		CHECK(written == 0);
		written = 99;
		result = registry->qualification_copy(bridge, ticket, 1, destination, sizeof(destination), &written);
		CHECK(result.native_error != OK);
		CHECK(written == 0);
		written = 99;
		result = registry->qualification_facts(bridge, ticket, 1, UINT32_MAX, 1, &fact, &written);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(written == 0);
		written = 99;
		result = registry->qualification_copy(bridge, ticket, 1, nullptr, sizeof(destination), &written);
		CHECK(result.native_error == ERR_INVALID_PARAMETER);
		CHECK(written == 0);
		memdelete(registry);
	}
	static void v2_prepare_output_overlap() {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		GDExtensionQualificationUploadPrepareV1 request = {};
		request.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		request.record_generation = 37;
		GDExtensionQualificationUploadPlanV1 plan = {};
		plan.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		plan.role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_OUTPUT_V1;
		GDExtensionQualificationTicketRefV1 ticket = {};
		alignas(GDExtensionQualificationReportV2) uint8_t request_storage[sizeof(GDExtensionQualificationReportV2)] = {};
		memcpy(request_storage, &request, sizeof(request));
		auto *overlapping_request = reinterpret_cast<GDExtensionQualificationUploadPrepareV1 *>(request_storage);
		auto *overlapping_report = reinterpret_cast<GDExtensionQualificationReportV2 *>(request_storage);
		CHECK(registry->qualification_prepare_uploads_v2({}, overlapping_request, &plan, 1, &ticket, overlapping_report).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(request_storage, &request, sizeof(request)) == 0);
		alignas(GDExtensionQualificationReportV2) uint8_t plan_storage[sizeof(GDExtensionQualificationReportV2)] = {};
		memcpy(plan_storage, &plan, sizeof(plan));
		auto *overlapping_plan = reinterpret_cast<GDExtensionQualificationUploadPlanV1 *>(plan_storage);
		overlapping_report = reinterpret_cast<GDExtensionQualificationReportV2 *>(plan_storage);
		CHECK(registry->qualification_prepare_uploads_v2({}, &request, overlapping_plan, 1, &ticket, overlapping_report).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(plan_storage, &plan, sizeof(plan)) == 0);
		memdelete(registry);
	}
	static void snapshot_after_overlap() {
		using namespace GDExtensionDeviceExit;
		// Seed CPU snapshots only. This does not issue a native ticket or
		// establish GPU completion; the Metal control proves that separately.
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		registry->records[0].owner_thread = Thread::get_caller_id();
		registry->records[0].snapshot.library_generation = 1;
		auto &native = registry->device_records[0];
		native.object = &device;
		native.live = true;
		native.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->report.revision = 5;
		ticket->report.state = 8;
		ticket->report.owned_size = 16;
		ticket->physically_retired.store(true);
		ticket->result.instantiate();
		ticket->result->delivered = true;
		const uint32_t expected_bytes[] = { 7, 8, 9, 10 };
		ticket->result->bytes.resize(sizeof(expected_bytes));
		memcpy(ticket->result->bytes.ptrw(), expected_bytes, sizeof(expected_bytes));
		CHECK_FALSE(ticket->readback_complete(0));
		// This CPU-only completed snapshot includes its original request and
		// backing association; delivery and byte values alone are insufficient.
		ticket->original_readback_serial = ticket->result->request_serial = 101;
		ticket->origin.observed_backing_serial = ticket->result->source_backing_serial = 51;
		ticket->origin.readback_size = ticket->result->source_size = sizeof(expected_bytes);
		ticket->result->issued_size = ticket->result->completed_size = sizeof(expected_bytes);
		CHECK(ticket->readback_complete(0));
		GDExtensionQualificationFactV1 expected_facts[2] = {};
		expected_facts[0].graph_kind = 1;
		expected_facts[0].graph_index = 17;
		expected_facts[1].graph_kind = 2;
		expected_facts[1].graph_index = 29;
		for (const auto &fact : expected_facts) {
			ticket->facts.push_back(fact);
		}
		bridge->tickets[0] = ticket;
		const GDExtensionQualificationBridgeRefV1 bridge_ref = { 71, 11 };
		const GDExtensionQualificationTicketRefV1 ticket_ref = { 71, 23 };
		GDExtensionQualificationFactV1 fact = {};
		fact.graph_kind = 0x12345678;
		const auto original_fact = fact;
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 0, 1, &fact, &fact.graph_kind).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&fact, &original_fact, sizeof(fact)) == 0);
		uint32_t written = 99;
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 1, 1, &fact, &written).native_error == OK);
		CHECK(written == 1);
		CHECK(memcmp(&fact, &expected_facts[1], sizeof(fact)) == 0);
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 0, 1, &fact, &written).native_error == OK);
		CHECK(written == 1);
		CHECK(memcmp(&fact, &expected_facts[0], sizeof(fact)) == 0);
		uint32_t destination[] = { 0x12345678, 0x87654321, 0xdeadbeef, 0xa5a5a5a5 };
		const auto original_first = destination[0];
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &destination[0]).native_error == ERR_INVALID_PARAMETER);
		CHECK(destination[0] == original_first);
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &written).native_error == OK);
		CHECK(written == sizeof(destination));
		CHECK(memcmp(destination, expected_bytes, sizeof(destination)) == 0);
		memset(destination, 0, sizeof(destination));
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &written).native_error == OK);
		CHECK(written == sizeof(destination));
		CHECK(memcmp(destination, expected_bytes, sizeof(destination)) == 0);
		CHECK(ticket->report.revision == 5);
		CHECK(!ticket->acknowledged);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(ticket);
		memdelete(bridge);
		memdelete(registry);
	}
	static void readback_map_error(uint32_t p_failure_index, bool p_observed = true) {
		using namespace GDExtensionDeviceExit;
		// Deterministic backend outcomes feed the original frame readback and
		// ticket reducer; these seeded sources do not certify a native dispatch.
		struct MapDriver : TransferTestDriverBase {
			uint32_t failure_index = 0;
			const uint32_t *qualification_users = nullptr;
			uint32_t words[2] = { 7, 8 };
			Error fence_wait(FenceID) override { return OK; }
			uint8_t *buffer_map(BufferID p_buffer) override {
				CHECK(*qualification_users == 1);
				const uint32_t index = p_buffer.id - 51;
				return index == failure_index ? nullptr : reinterpret_cast<uint8_t *>(&words[index]);
			}
			void buffer_unmap(BufferID p_buffer) override {
				CRASH_COND(p_buffer.id - 51 == failure_index);
			}
		} legacy;
		struct ObservedMapDriver : MapDriver {
			uint8_t *buffer_map_observed(BufferID p_buffer, int64_t &r_native_code) override {
				auto *data = buffer_map(p_buffer);
				r_native_code = data ? 0 : -5;
				return data;
			}
		} observed;
		legacy.failure_index = observed.failure_index = p_failure_index;
		RenderingDevice device;
		device.driver = p_observed ? &observed : &legacy;
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.fence_signaled = true;
		QualificationTicket ticket;
		ticket.recorded = ticket.ordering_closed = true;
		ticket.origin.readback_size = 8;
		ticket.report.revision = 1;
		ticket.report.native_code = p_observed ? 0 : -77;
		ticket.report.source_backing_serial = 11;
		ticket.report.source_offset = 4;
		ticket.report.source_size = 8;
		ticket.result.instantiate();
		ticket.result->qualification_owned = true;
		ticket.result->bytes.resize(8);
		ticket.result->issued_size = 8;
		ticket.result->request_serial = ticket.original_readback_serial = 101;
		ticket.origin.observed_backing_serial = 11;
		ticket.origin.readback_offset = 4;
		ticket.result->source_backing_serial = 11;
		ticket.result->source_offset = 4;
		ticket.result->source_size = 8;
		RenderingDevice::BufferGetDataRequest request;
		request.size = 8;
		request.frame_local_count = 2;
		request.result = ticket.result;
		frame.download_buffer_get_data_requests.push_back(request);
		for (uint32_t i = 0; i < 2; ++i) {
			frame.download_buffer_staging_buffers.push_back(RDD::BufferID(51 + i));
			frame.download_buffer_copy_regions.push_back({ 0, 0, 4 });
		}
		RenderingDevice::Buffer buffer;
		buffer.qualification_users = 1;
		const RID rid = device.storage_buffer_owner.make_rid(buffer);
		legacy.qualification_users = observed.qualification_users = &device.storage_buffer_owner.get_or_null(rid)->qualification_users;
		ticket.resource_pins.push_back(rid);
		GDExtensionQualificationFactV1 graph = {};
		graph.semantic_kind = 3;
		ticket.facts.push_back(graph);
		ticket.graphs.push_back({ {}, 0, 0, 0, 1, true, false });
		ticket.frame_refs = 1;
		RDD::NativeCallFact source;
		source.serial = 17;
		source.completion_serial = 73;
		source.queue_index = 1;
		GDExtensionQualificationFactV1 fact = {};
		fact.issue = 1;
		fact.completion = 2;
		ticket.facts.push_back(fact);
		ticket.natives.push_back({ source, 1, 0, 0, 1, false, false });
		ERR_PRINT_OFF;
		CHECK(device._stall_for_frame(0) == OK);
		ERR_PRINT_ON;
		auto &registry = Registry::get_singleton();
		registry.qualification_ticket_publish(&ticket, &device);
		const bool failed = p_failure_index < 2;
		CHECK(ticket.report.native_error == (failed ? ERR_CANT_ACQUIRE_RESOURCE : OK));
		CHECK(ticket.report.native_code == (failed && p_observed ? -5 : 0));
		CHECK(ticket.result->delivered == !failed);
		CHECK(ticket.report.readback_request == 101);
		CHECK(ticket.report.source_backing_serial == 11);
		CHECK(ticket.report.source_offset == 4);
		CHECK(ticket.report.source_size == 8);
		CHECK(ticket.report.owned_size == 0);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		ticket.graphs[0].returned = true;
		ticket.frame_refs = 0;
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
		CHECK(ticket.report.state == (failed ? 5 : 8));
		CHECK(ticket.report.owned_size == (failed ? 0 : 8));
		CHECK(ticket.admission_closed.load() == failed);
		if (!failed) {
			const uint32_t expected[] = { 7, 8 };
			CHECK(memcmp(ticket.result->bytes.ptr(), expected, sizeof(expected)) == 0);
		}
		device.storage_buffer_owner.free(rid);
		device.frames.clear();
		device.driver = nullptr;
	}
	static void frame_poll_error_retains_native_code() {
		using namespace GDExtensionDeviceExit;
		struct PollDriver : TransferTestDriverBase {
			uint32_t polls = 0;
			FencePollResult fence_poll(FenceID, uint64_t p_serial) override {
				++polls;
				FencePollResult result;
				result.completed_serial = p_serial;
				result.native_error = true;
				result.native_code = -13;
				return result;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(1);
		device.frames[0].fence = RDD::FenceID(71);
		device.frames[0].fence_signaled = true;
		RDD::QueueCallReport report;
		report.submission_serial = 99;
		device.frames[0].chain_reports.push_back(report);
		CHECK(device._stall_for_frame(0, true) == FAILED);
		CHECK(driver.polls == 1);
		CHECK(device.frame_wait_native_code.load() == -13);
		device.frame_wait_error = OK;
		device.failed_wait_frame = -1;
		device.frames.clear();
		device.driver = nullptr;
	}
	static void ticket_poll_error_retains_native_code() {
		using namespace GDExtensionDeviceExit;
		struct PollDriver : TransferTestDriverBase {
			bool native_call_observation_supported() const override { return true; }
			Error native_call_retain(uint64_t) override { return OK; }
			FencePollResult native_call_poll(uint64_t p_serial) override {
				FencePollResult result;
				result.completed_serial = p_serial;
				result.native_error = true;
				result.native_code = -7;
				return result;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(1);
		device.frames[0].fence_signaled = true;
		QualificationTicket ticket;
		ticket.recorded = true;
		ticket.frame_refs = 1;
		ticket.report.revision = 1;
		RDD::NativeCallFact source;
		source.serial = source.completion_serial = 17;
		source.queue_index = 1;
		source.completion = RDD::CompletionCapability::Fence;
		source.issue = RDD::IssueFact::IssuedUnknown;
		source.native_code = -7;
		GDExtensionQualificationFactV1 fact = {};
		fact.issue = uint32_t(source.issue);
		fact.completion = 1;
		ticket.facts.push_back(fact);
		ticket.natives.push_back({ source, 0, 0, 0, 1, false, false });
		device.frames[0].qualification_tickets.push_back(&ticket);
		Registry::get_singleton().qualification_ticket_step(&ticket, &device);
		CHECK(ticket.append_failed);
		CHECK(ticket.report.native_error == FAILED);
		CHECK(ticket.report.native_code == -7);
		CHECK(ticket.facts[0].native_code == -7);
		device.frames.clear();
		device.driver = nullptr;
	}
	static void chain_poll_error_retains_native_code() {
		struct PollDriver : TransferTestDriverBase {
			bool native_call_observation_supported() const override { return true; }
			FencePollResult native_call_poll(uint64_t p_serial) override {
				FencePollResult result;
				result.completed_serial = p_serial;
				result.native_error = true;
				result.native_code = -21;
				return result;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(1);
		RDD::QueueCallReport report;
		RDD::NativeCallFact call;
		call.serial = call.completion_serial = 31;
		report.calls.push_back(call);
		device.frames[0].chain_reports.push_back(report);
		CHECK(device._stall_for_frame(0, true) == FAILED);
		CHECK(device.frame_wait_native_code.load() == -21);
		device.frames.clear();
		device.driver = nullptr;
	}

	static void continuation_profile_capacity() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			mutable uint32_t calls = 0, main_slots = 0, worker_slots = 0, frames = 0;
			uint32_t bound = 3952;
			uint32_t qualification_native_call_bound_for_profile(uint32_t main, uint32_t workers, uint32_t frame_count) const override {
				++calls;
				main_slots = main;
				worker_slots = workers;
				frames = frame_count;
				return bound;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		struct Cleanup {
			RenderingDevice &device;
			~Cleanup() { device.driver = nullptr; }
		} cleanup{ device };
		device.transfer_worker_pool_max_size = 11;
		QualificationTicket ticket;
		ticket.preparation.expected_result_bytes = 2080;
		const uint32_t plan_bytes[] = { 4, 2064, 4, 4, 4, 4, 4, 12, 4, 4, 24 };
		ticket.uploads.resize(11);
		for (uint32_t i = 0; i < 11; ++i) {
			ticket.uploads[i].plan.byte_size = plan_bytes[i];
		} // The original R12 eleven plans require 75 admitted segments.
		auto &registry = Registry::get_singleton();
		CHECK(registry.qualification_typed_upload_capacity(&ticket, &device, 0) == OK);
		CHECK(driver.main_slots == 145);
		CHECK(driver.worker_slots == 1595);
		CHECK(driver.frames == 145);
		driver.bound = 3953; // G144 + native3953 exceeds the unchanged 4096 store.
		CHECK(registry.qualification_typed_upload_capacity(&ticket, &device, 0) == ERR_PARAMETER_RANGE_ERROR);
		driver.bound = 0;
		CHECK(registry.qualification_typed_upload_capacity(&ticket, &device, 0) == ERR_UNAVAILABLE);
		const uint32_t calls = driver.calls;
		device.transfer_worker_pool_max_size = UINT32_MAX;
		CHECK(registry.qualification_typed_upload_capacity(&ticket, &device, 0) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(driver.calls == calls);
		CHECK(device.draw_graph.qualification_command_count() == 0);
		CHECK(ticket.facts.is_empty());
	}
	static void continuation_capacity() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			mutable uint32_t last_slots = 0;
			bool native_call_observation_supported() const override { return true; }
			uint32_t qualification_native_call_bound(uint32_t slots) const override {
				last_slots = slots;
				return slots;
			}
			uint64_t limit_get(Limit) override { return 65535; }
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		auto *registry = memnew(Registry);
		registry->records[0].snapshot.library_generation = 1;
		Registry::QualificationBridge bridge;
		GDExtensionQualificationRecordV1 request = {};
		request.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		request.record_generation = request.request_nonce = request.library_generation = request.qualification_session_serial = 1;
		request.attempt = 1;
		request.sample_count = request.range_end = 4;
		request.body_groups_x = 1;
		request.source_allocation_generation = 1;
		const uint32_t push[4] = { 0, 4, 0, 0 };
		memcpy(request.body_push, push, 16);
		memcpy(request.capture_push, push, 16);
		struct Case {
			uint32_t bytes, workers, expected_slots;
			Error result;
		};
		const Case cases[] = {
			{ 4, 2, 12, ERR_INVALID_DATA },
			{ 4, 1022, 4092, ERR_INVALID_DATA },
			{ 4, 1023, 0, ERR_PARAMETER_RANGE_ERROR },
			{ 4, UINT32_MAX, 0, ERR_PARAMETER_RANGE_ERROR },
			{ 16384, 5, 3090, ERR_INVALID_DATA },
			{ 16384, 6, 3605, ERR_PARAMETER_RANGE_ERROR },
		};
		for (const auto &c : cases) {
			CAPTURE(c.bytes);
			CAPTURE(c.workers);
			device.transfer_worker_pool_max_size = c.workers;
			request.readback_size = c.bytes;
			driver.last_slots = 0;
			LocalVector<RID> resources;
			CHECK(registry->qualification_record_preflight(&bridge, &device, request, resources) == c.result);
			if (c.expected_slots) {
				CHECK(driver.last_slots == c.expected_slots);
			}
			CHECK(resources.is_empty());
			CHECK(device.draw_graph.qualification_command_count() == 0);
		}
		// Invalid RID is deliberately later than capacity: no fake pipeline or
		// production grant is needed to observe preappend slot-budget admission.
		device.driver = nullptr;
		memdelete(registry);
	}

	static void unknown_native_error(uint32_t mode = 0, bool p_depth = false) {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			bool pending = false, finished = false;
			Error native_call_retain(uint64_t serial) override {
				CHECK(serial == 17);
				return OK;
			}
			FencePollResult native_call_poll(uint64_t serial) override {
				CHECK(serial == 17);
				FencePollResult out;
				out.completed_serial = serial;
				out.native_error = !pending;
				out.native_code = pending ? 0 : -1;
				out.completed = pending && finished;
				if (out.completed) {
					out.resolved_issue = IssueFact::KnownAccepted; // Exact void-commit completion source.
				}
				return out;
			}
		} driver;
		driver.pending = mode == 2;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(1);
		QualificationTicket ticket;
		ticket.report.revision = 1;
		ticket.frame_refs = 1;
		// These are explicit synthetic native inputs; only polling and the public
		// depth projection are exercised here, never GPU acceptance.
		auto *registry = p_depth ? memnew(Registry) : nullptr;
		auto *previous = RenderingDevice::singleton;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		Registry::QualificationBridge *bridge = nullptr;
		if (p_depth) {
			RenderingDevice::singleton = &device;
			device.is_main_instance = true;
			registry->instance = 71;
			auto &record = registry->records[0];
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = holder.generation = 1;
			// Saturation makes observe schedule return without queue publication.
			record.private_refs = registry->private_limit;
			auto &native = registry->device_records[0];
			native.object = &device;
			native.live = true;
			native.epoch = 1;
			bridge = memnew(Registry::QualificationBridge);
			bridge->serial = 11;
			bridge->epoch = 1;
			bridge->holder = &holder;
			bridge->children = 1;
			registry->qualification_bridges[0] = bridge;
			ticket.serial = 23;
			ticket.bridge_serial = 11;
			ticket.multi_readback = ticket.depth_bound = true;
			ticket.depth_report.state = 1;
			bridge->tickets[0] = &ticket;
		}
		auto observe_depth = [&](RID p_rid) {
			if (!p_depth) {
				return;
			}
			GDExtensionQualificationBudgetV1 budget = {};
			budget.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
			budget.max_passes = budget.max_micros = 1;
			GDExtensionQualificationReportV3 report = {};
			CHECK(registry->qualification_observe_v3({ 71, 11 }, { 71, 23 }, &budget, &report).native_error == OK);
			CHECK(report.abi.abi_version == 3);
			CHECK(report.record.state == ticket.report.state);
			CHECK(report.depth.report_revision == ticket.report.revision);
			const uint32_t expected = ticket.report.state == 6 ? 3 : ticket.report.state == 7 ? 2
																							  : 1;
			CHECK(report.depth.state == expected);
			CHECK_FALSE(ticket.queued);
			CHECK(bridge->children == 1);
			CHECK(registry->records[0].private_refs == registry->private_limit);
			CHECK(device.storage_buffer_owner.get_or_null(p_rid)->qualification_users == 1);
			CHECK_FALSE(ticket.natives[0].owner_released);
			CHECK_FALSE(ticket.physically_retired.load());
			if (expected != 1) {
				CHECK(report.depth.native_error == ticket.report.native_error);
				CHECK(report.depth.phase == 3);
			}
		};
		RDD::NativeCallFact source;
		source.serial = source.completion_serial = 17;
		source.queue_index = 3;
		source.completion = RDD::CompletionCapability::Fence;
		source.issue = mode == 1 ? RDD::IssueFact::KnownAccepted : RDD::IssueFact::IssuedUnknown;
		source.error = mode == 2 ? OK : FAILED;
		source.native_code = mode == 2 ? 0 : -1;
		GDExtensionQualificationFactV1 fact = {};
		fact.issue = uint32_t(source.issue);
		fact.completion = 1;
		fact.native_serial = fact.completion_serial = 17;
		fact.backend_queue = 3;
		ticket.facts.push_back(fact);
		ticket.natives.push_back({ source, 0, 0, 0, 1, false, false });
		if (mode == 3) {
			auto failed_source = source;
			failed_source.serial = failed_source.completion_serial = 18;
			failed_source.issue = RDD::IssueFact::KnownAccepted;
			auto failed_fact = fact;
			failed_fact.issue = 1;
			failed_fact.completion = 3;
			failed_fact.error = FAILED;
			ticket.facts.push_back(failed_fact);
			ticket.natives.push_back({ failed_source, 1, 0, 0, 1, true, false });
		}
		device.frames[0].qualification_tickets.push_back(&ticket);
		RenderingDevice::Buffer buffer;
		buffer.qualification_users = 1;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		ticket.resource_pins.push_back(rid);
		CHECK(device._qualification_poll_tickets(0) == (mode == 2 ? ERR_BUSY : FAILED));
		CHECK(ticket.facts[0].issue == (mode == 1 ? 1 : 2));
		CHECK(ticket.facts[0].completion == (mode == 2 ? 1 : mode == 1 ? 3
																	   : 4));
		CHECK(ticket.facts[0].native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.possible_count == (mode == 1 ? 0 : 1));
		CHECK(ticket.report.known_count == (mode == 1 || mode == 3 ? 1 : 0));
		CHECK(ticket.report.state == (mode == 1 ? 7 : mode == 2 ? 4
																: 6));
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		CHECK_FALSE(ticket.natives[0].owner_released);
		observe_depth(rid);
		if (mode == 2) {
			driver.finished = true;
			CHECK(device._qualification_poll_tickets(0) == OK);
			CHECK(ticket.facts[0].issue == 1);
			CHECK(ticket.facts[0].completion == 2);
			CHECK(ticket.report.possible_count == 0);
			CHECK(ticket.report.known_count == 1);
			observe_depth(rid);
			CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
			CHECK_FALSE(ticket.natives[0].owner_released);
			CHECK_FALSE(ticket.physically_retired.load());
		}
		// Synthetic reducer inputs own no GPU work. This is fixture cleanup,
		// never an acknowledgment or proof that an Unknown may retire.
		device.frames.clear();
		device.storage_buffer_owner.free(rid);
		if (p_depth) {
			registry->qualification_bridges[0] = nullptr;
			RenderingDevice::singleton = previous;
			device.is_main_instance = false;
			memdelete(bridge);
			memdelete(registry);
		}
		device.driver = nullptr;
	}

	static void no_effect_reacquires_resource_pin(bool p_attempted = false) {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		QualificationTicket ticket;
		ticket.prepared = true;
		ticket.report.revision = 1;
		ticket.resource_pins.reserve(128);
		const auto reserved_capacity = ticket.resource_pins.get_capacity();
		RenderingDevice::Buffer buffer;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		LocalVector<RID> resources;
		resources.push_back(rid);
		REQUIRE(device._qualification_pin_resources(&ticket, resources) == OK);
		CHECK(device._qualification_resource_busy(rid));
		auto &registry = Registry::get_singleton();
		if (p_attempted) {
			ticket.uploads.resize(1);
			ticket.uploads[0].attempted = true;
		}
		// The original reducer returns a zero-effect upload's physical pins
		// while its prepared ticket can still admit the same upload later.
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK(ticket.physically_retired.load() == !p_attempted);
		CHECK_FALSE(ticket.admission_closed.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == (p_attempted ? 1 : 0));
		CHECK(device._qualification_resource_busy(rid) == p_attempted);
		if (p_attempted) {
			ticket.admission_closed = true;
			registry.qualification_ticket_publish(&ticket, &device);
			CHECK(ticket.physically_retired.load());
			CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
		}
		if (p_attempted) {
			device.storage_buffer_owner.free(rid);
			device.driver = nullptr;
			return;
		}
		CHECK_FALSE(device._qualification_resource_busy(rid));
		REQUIRE(device._qualification_pin_resources(&ticket, resources) == OK);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		CHECK(device._qualification_resource_busy(rid));
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(ticket.resource_pins.get_capacity() == reserved_capacity);
		// Avoid invoking a known unbalanced release in the RED control.
		if (device.storage_buffer_owner.get_or_null(rid)->qualification_users) {
			registry.qualification_ticket_publish(&ticket, &device);
			registry.qualification_ticket_publish(&ticket, &device);
			CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
			CHECK_FALSE(device._qualification_resource_busy(rid));
			CHECK(ticket.resource_pins.get_capacity() == reserved_capacity);
		}
		device.storage_buffer_owner.free(rid);
		device.driver = nullptr;
	}
#ifdef THREADS_ENABLED
	static void contended_continuation() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			int waits = 0;
			Error fence_wait(FenceID) override {
				++waits;
				return ERR_TIMEOUT;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(2);
		device.frames_drawn = 10;
		device.frames[0].qualification_epoch = 1;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 2, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		auto &staging = device.download_staging_buffers;
		staging.blocks.resize(1);
		staging.block_size = staging.max_size = 64;
		staging.blocks.write[0].driver_id = RenderingDeviceDriver::BufferID(51);
		staging.blocks.write[0].frame_used = 10;
		staging.blocks.write[0].fill_amount = 32;
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(71);
		buffer.size = 64;
		buffer.backing_serial = 11;
		buffer.draw_tracker = RenderingDeviceGraph::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		QualificationTicket ticket;
		ticket.recorded = true;
		ticket.report.revision = 1;
		ticket.origin.readback_buffer = rid.get_id();
		ticket.origin.readback_size = 64;
		ticket.result.instantiate();
		ticket.result->qualification_owned = true;
		ticket.result->request_serial = ticket.original_readback_serial = 101;
		ticket.result->bytes.resize(64);
		auto &registry = Registry::get_singleton();
		std::atomic<bool> locked = false, release = false;
		std::thread holder([&] {device._thread_safe_.lock();locked=true;while(!release.load()){std::this_thread::yield();
}device._thread_safe_.unlock(); });
		while (!locked.load()) {
			std::this_thread::yield();
		}
		registry.qualification_ticket_step(&ticket, &device);
		CHECK(ticket.result->issued_size == 0);
		CHECK(ticket.result->copies.is_empty());
		CHECK(device.frames[0].download_buffer_get_data_requests.is_empty());
		CHECK(ticket.facts.is_empty());
		CHECK(ticket.frame_refs == 0);
		release = true;
		holder.join();
		registry.qualification_ticket_step(&ticket, &device);
		CHECK(ticket.result->issued_size == 32);
		CHECK(ticket.result->copies.size() == 1);
		CHECK(ticket.frame_refs == 1);
		CHECK_FALSE(ticket.physically_retired.load());
		staging.blocks.write[0].frame_used = 0;
		registry.qualification_ticket_step(&ticket, &device);
		CHECK(ticket.result->issued_size == 64);
		CHECK(ticket.result->copies.size() == 2);
		CHECK(ticket.frame_refs == 1);
		CHECK(ticket.facts.size() == 2);
		CHECK(driver.waits == 0);
		if (ticket.result->copies.size() == 2) {
			CHECK(ticket.result->copies[0].region.src_offset == 0);
			CHECK(ticket.result->copies[1].region.src_offset == 32);
			CHECK(ticket.result->copies[1].region.size == 32);
		}
		RenderingDeviceGraph::resource_tracker_free(buffer.draw_tracker);
		device.storage_buffer_owner.free(rid);
		device.frames.clear();
		device.download_staging_buffers.blocks.clear();
		device.driver = nullptr;
	}
#endif
	static void reduction(bool copy_failure) {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		QualificationTicket ticket;
		ticket.report.revision = 1;
		ticket.origin.readback_size = 4;
		ticket.recorded = true;
		ticket.ordering_closed = true;
		ticket.result.instantiate();
		ticket.result->qualification_owned = true;
		ticket.result->bytes.resize(4);
		ticket.result->issued_size = 4;
		ticket.result->request_serial = ticket.original_readback_serial = 101;
		ticket.origin.observed_backing_serial = ticket.result->source_backing_serial = 11;
		ticket.result->source_size = 4;
		RenderingDevice::Buffer buffer;
		buffer.qualification_users = 1;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		ticket.resource_pins.push_back(rid);
		GDExtensionQualificationFactV1 graph = {};
		graph.semantic_kind = 3;
		graph.command_slot = 0;
		ticket.facts.push_back(graph);
		ticket.graphs.push_back({ {}, 0, 0, 0, 1, true, false });
		ticket.frame_refs = 1;
		for (uint32_t i = 0; i < 2; ++i) {
			RenderingDeviceDriver::NativeCallFact source;
			source.serial = 17 + i;
			source.completion_serial = 73 + i;
			source.queue_index = 1;
			GDExtensionQualificationFactV1 fact = {};
			fact.issue = 1;
			fact.completion = i ? 1 : 2;
			ticket.natives.push_back({ source, ticket.facts.size(), 0, 0, 1, true, false });
			ticket.facts.push_back(fact);
		}
		auto &registry = Registry::get_singleton();
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK(ticket.facts[0].native_serial == 17);
		CHECK(ticket.facts[0].completion_serial == 73);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		ticket.facts[2].completion = 2;
		ticket.observation_dirty = true;
		if (copy_failure) {
			ticket.result->error = ERR_CANT_ACQUIRE_RESOURCE;
		} else {
			ticket.result->completed_size = 4;
			ticket.result->delivered = true;
		}
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(ticket.report.owned_size == 0);
		ticket.graphs[0].returned = true;
		ticket.frame_refs = 0;
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
		if (copy_failure) {
			CHECK(ticket.report.state == 5);
			CHECK(ticket.report.owned_size == 0);
			CHECK(ticket.report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
			CHECK(ticket.admission_closed.load());
		} else {
			CHECK(ticket.report.state == 8);
			CHECK(ticket.report.owned_size == 4);
		}
		device.storage_buffer_owner.free(rid);
		device.driver = nullptr;
	}
	static void owner_stop() {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = registry->private_limit;
		registry->device_records[0].object = &device;
		registry->device_records[0].live = true;
		registry->device_records[0].epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		};
		Holder holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->report.revision = 1;
		ticket->physically_retired = true;
		bridge->tickets[0] = ticket;
		auto ordinary = registry->qualification_cancel({ 71, 11 }, { 71, 23 });
		CHECK(ordinary.native_error == ERR_BUSY);
		CHECK(ordinary.reason == 16);
		CHECK_FALSE(ticket->stop_requested);
		ticket->prepared = true;
		ticket->report.state = 8;
		auto complete = registry->qualification_cancel({ 71, 11 }, { 71, 23 });
		CHECK(complete.native_error == ERR_BUSY);
		CHECK(complete.reason == 16);
		CHECK(ticket->report.state == 8);
		CHECK_FALSE(ticket->stop_requested);
		ticket->report.state = 0;
#ifdef THREADS_ENABLED
		GDExtensionDeviceSessionResultV1 foreign = {};
		std::thread other([&] { foreign = registry->qualification_cancel({ 71, 11 }, { 71, 23 }); });
		other.join();
		CHECK(foreign.native_error == ERR_UNAUTHORIZED);
		CHECK(foreign.reason == 15);
		CHECK_FALSE(ticket->stop_requested);
#endif
		auto stale = registry->qualification_cancel({ 71, 11 }, {});
		CHECK(stale.native_error == ERR_DOES_NOT_EXIST);
		CHECK(stale.reason == 17);
		auto accepted = registry->qualification_cancel({ 71, 11 }, { 71, 23 });
		CHECK(accepted.native_error == OK);
		CHECK(ticket->stop_requested);
		CHECK(ticket->admission_closed.load());
		CHECK_FALSE(ticket->physically_retired.load());
		CHECK_FALSE(ticket->queued);
		CHECK(bridge->children == 1);
		CHECK(record.private_refs == registry->private_limit);
		CHECK(registry->qualification_cancel({ 71, 11 }, { 71, 23 }).native_error == OK);
		registry->qualification_ticket_publish(ticket, &device);
		CHECK(ticket->report.state == 7);
		CHECK(ticket->report.phase == 6);
		CHECK(ticket->report.owned_size == 0);
		CHECK(ticket->report.required_count == 0);
		CHECK(ticket->physically_retired.load());
		const uint64_t revision = ticket->report.revision;
		CHECK(registry->qualification_cancel({ 71, 11 }, { 71, 23 }).native_error == OK);
		CHECK(ticket->report.revision == revision);
		CHECK(registry->qualification_release({ 71, 11 }, { 71, 23 }).native_error == OK);
		CHECK(bridge->children == 0);
		CHECK(record.private_refs == registry->private_limit - 1);
		CHECK(registry->qualification_cancel({ 71, 11 }, { 71, 23 }).reason == 17);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(ticket);
		memdelete(bridge);
		memdelete(registry);
	}
	static void unknown_keeps_pin() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		QualificationTicket ticket;
		ticket.report.revision = 1;
		ticket.append_failed = true;
		ticket.mapping_failed = true;
		RenderingDevice::Buffer buffer;
		buffer.qualification_users = 1;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		ticket.resource_pins.push_back(rid);
		Registry::get_singleton().qualification_ticket_publish(&ticket, &device);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(ticket.report.state == 6);
		CHECK(ticket.report.owned_size == 0);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		device.storage_buffer_owner.free(rid);
		device.driver = nullptr;
	}
	static void transitive_graph_order() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
			void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override {}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 1;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDeviceGraph::ResourceTracker destination;
		destination.buffer_driver_id = RenderingDeviceDriver::BufferID(72);
		RenderingDeviceGraph::RecordedBufferCopy copy;
		copy.source = RenderingDeviceDriver::BufferID(71);
		copy.region = { 0, 0, 32 };
		const auto source = device.draw_graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
		const auto capture = device.draw_graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
		device.draw_graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
		const auto body = device.draw_graph.add_buffer_update(RenderingDeviceDriver::BufferID(72), &destination, VectorView(&copy, 1));
		uint32_t path_length = 0;
		CHECK_FALSE(device.draw_graph.has_synchronization_edge(capture, body));
		CHECK(device.draw_graph.has_dependency_path(capture, body, path_length));
		CHECK(path_length == 2);
		RenderingDeviceGraph::CommandBufferPool pool;
		RenderingDeviceDriver::CommandBufferID primary(11);
		device.draw_graph.end(true, false, primary, pool);
		QualificationTicket ticket;
		ticket.capture = capture;
		ticket.body = body;
		ticket.prepared = true;
		GDExtensionQualificationFactV1 source_fact = {};
		source_fact.semantic_kind = 6;
		source_fact.command_slot = UINT32_MAX;
		ticket.facts.push_back(source_fact);
		ticket.graphs.push_back({ source, 0, 0, 0, 1, false, false });
		ticket.frame_refs = 1;
		ticket.report.revision = 1;
		device.frames[0].qualification_tickets.push_back(&ticket);
		device._qualification_seal_tickets();
		CHECK(ticket.ordering_closed);
		CHECK(ticket.ordering_path_length == 2);
		CHECK(ticket.graphs[0].path_length == 1);
		CHECK_FALSE(ticket.mapping_failed);
		device.frames.clear();
		device.draw_graph.finalize();
		device.driver = nullptr;
	}
};
TEST_CASE("[Qualification] overlapping snapshot outputs are rejected without writing destination") {
	TestQualificationTicketAccess::snapshot_output_overlap();
}
TEST_CASE("[Qualification][MultiReadback] V2 prepare rejects report overlap without modifying request or plan") {
	TestQualificationTicketAccess::v2_prepare_output_overlap();
}
TEST_CASE("[Qualification] same ticket snapshots remain readable after overlap refusal") {
	TestQualificationTicketAccess::snapshot_after_overlap();
}
TEST_CASE("[Qualification] original async map failure retains raw code and physical obligations") {
	TestQualificationTicketAccess::readback_map_error(0);
	TestQualificationTicketAccess::readback_map_error(1);
}
TEST_CASE("[Qualification] async map success and legacy failure preserve original outcome") {
	TestQualificationTicketAccess::readback_map_error(2);
	TestQualificationTicketAccess::readback_map_error(0, false);
}
TEST_CASE("[Qualification] frame poll failure retains original native code") {
	TestQualificationTicketAccess::frame_poll_error_retains_native_code();
}
TEST_CASE("[Qualification] ticket poll failure retains original native code") {
	TestQualificationTicketAccess::ticket_poll_error_retains_native_code();
}
TEST_CASE("[Qualification] chain poll failure retains original native code") {
	TestQualificationTicketAccess::chain_poll_error_retains_native_code();
}
TEST_CASE("[Qualification] ticket keeps distinct native and completion identities until every source and frame returns") {
	TestQualificationTicketAccess::reduction(false);
}

TEST_CASE("[Qualification] sealing accepts an actual transitive graph path") {
	TestQualificationTicketAccess::transitive_graph_order();
}

TEST_CASE("[Qualification] zero-effect prepared ticket reacquires original resource pins before retry") {
	TestQualificationTicketAccess::no_effect_reacquires_resource_pin();
}
TEST_CASE("[Qualification] attempted upload keeps original writer pin across zero-effect busy publish") {
	TestQualificationTicketAccess::no_effect_reacquires_resource_pin(true);
}
TEST_CASE("[Qualification] settled native work releases resource pins after a logical readback copy failure") {
	TestQualificationTicketAccess::reduction(true);
}
TEST_CASE("[Qualification] unresolved source mapping keeps failed ticket resource pins") {
	TestQualificationTicketAccess::unknown_keeps_pin();
}

TEST_CASE("[Qualification] owner stop is opt in exact ticket monotone and preserves immutable Complete") {
	TestQualificationTicketAccess::owner_stop();
}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] exact record replay returns original ticket despite RD contention") {
	TestQualificationTicketAccess::replay_under_contention();
}
TEST_CASE("[Qualification] exact prepared replay returns canonical original plans despite RD contention") {
	TestQualificationTicketAccess::replay_under_contention(true);
}
TEST_CASE("[Qualification] ticket continuation refuses contended RD before graph and resumes the same suffix") {
	TestQualificationTicketAccess::contended_continuation();
}
#endif

TEST_CASE("[Qualification] unknown submit error preserves original issue possible set and resource obligations") {
	TestQualificationTicketAccess::unknown_native_error();
}

TEST_CASE("[Qualification] known accepted native error stays Failed and retains obligations") {
	TestQualificationTicketAccess::unknown_native_error(1);
}
TEST_CASE("[Qualification] callback pending source stays pending and can become known successful") {
	TestQualificationTicketAccess::unknown_native_error(2);
}
TEST_CASE("[Qualification] mixed Failed and Unknown sources preserve unresolved Unknown") {
	TestQualificationTicketAccess::unknown_native_error(3);
}

TEST_CASE("[Qualification] continuation preflight reserves fresh hidden worker sources per graph frame") {
	TestQualificationTicketAccess::continuation_capacity();
}

TEST_CASE("[Qualification][MultiReadback] Numeric and Method wire rejects malformed range and set partitions") {
	TestQualificationTicketAccess::range_descriptors();
}
TEST_CASE("[Qualification][MultiReadback] every actual range and physical source must return before aggregate copy") {
	TestQualificationTicketAccess::range_completion(0);
}
TEST_CASE("[Qualification][MultiReadback] later range failures preserve the first cause and prohibit aggregate copy") {
	TestQualificationTicketAccess::range_completion(1);
}
TEST_CASE("[Qualification][MultiReadback] cancellation retains the original prefix through physical return") {
	TestQualificationTicketAccess::range_completion(2);
}
TEST_CASE("[Qualification][MultiReadback] Body proof excludes pending sources from subsequent copy frames") {
	TestQualificationTicketAccess::body_source_scope();
}

TEST_CASE("[Qualification][MultiReadback] V2 empty domain keeps a real guarded group and V1 retains its original nonempty rule") {
	TestQualificationTicketAccess::empty_range_dispatch();
}

#if defined(VULKAN_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[Qualification][VulkanQualification] original gate charges graphs and native submissions together") {
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	// G314 + (G314+1)*(11+1) = 4094 fits the unchanged 4096 facts.
	CHECK(TestQualificationTicketAccess::original_upload_capacity(driver, 9920, 11) == OK);
	// The next graph still has a native bound below 4096, but total facts
	// become 4107. Reject before any graph or source is published.
	CHECK(TestQualificationTicketAccess::original_upload_capacity(driver, 9952, 11) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(TestQualificationTicketAccess::original_upload_capacity(driver, 9920, UINT32_MAX) == ERR_PARAMETER_RANGE_ERROR);
}
#endif

TEST_CASE("[Qualification] continuation capacity preserves main and waitless worker profiles") {
	TestQualificationTicketAccess::continuation_profile_capacity();
}

TEST_CASE("[D32][Qualification] depth observe preserves actual native poll failure and unresolved pins") {
	for (uint32_t mode = 0; mode < 4; ++mode) {
		TestQualificationTicketAccess::unknown_native_error(mode, true);
	}
}

TEST_CASE("[D32][Qualification] checked RD depth draw retains original graph prefix across refused suffix") {
	TestQualificationTicketAccess::checked_depth_draw_records();
}
