/**************************************************************************/
/*  test_production_request.cpp                                           */
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

#include "core/extension/gdextension_production_request.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_production_request)

namespace TestProductionRequest {
struct Access {
	using Registry = GDExtensionDeviceExit::Registry;
	using Request = GDExtensionDeviceExit::ProductionRequest;
	struct QuoteDriver : TransferTestDriverBase {
		uint32_t qualification_native_call_bound_for_profile(uint32_t main, uint32_t workers, uint32_t frames) const override {
			return frames && frames <= main && workers <= UINT32_MAX - main ? main + workers : 0;
		}
	};
	struct Holder : GDExtensionDeviceExit::HolderRetirement {
		Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
	};
	// Controlled Core enrollment only. Every native driver operation remains
	// the fail-fast TransferTestDriverBase implementation. No GPU is initialized.
	struct Enrollment {
		RenderingDevice *previous = RenderingDevice::get_singleton();
		RenderingDevice rd;
		QuoteDriver driver;
		Registry *registry = memnew(Registry);
		Holder holder;
		GDExtensionDeviceExit::RecordRef reference;
		GDExtensionDeviceExit::RegistrationContext frame;
		static constexpr GDExtensionDeviceExitAbi ABI = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		static constexpr uint64_t INSTANCE = 9101;
		Enrollment() {
			using namespace GDExtensionDeviceExit;
			rd.driver = &driver;
			RenderingDevice::singleton = &rd;
			rd.is_main_instance = true;
			rd.render_thread_id = Thread::get_caller_id();
			rd.frames.resize(1);
			rd.transfer_worker_pool_max_size = 0;
			// One real graph metadata command is the prefix being quoted. It is never
			// issued to a driver and is unrelated to the production request's effects.
			rd.draw_graph.add_capture_timestamp(RDD::QueryPoolID(1), 0);
			registry->instance = INSTANCE;
			auto &record = registry->records[0];
			record.occupied = record.attached = true;
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = 1;
			record.producer_registration_serial = 2;
			record.producer_attachment_identity = this;
			record.producer_code_holder = 3;
			record.guard_enrollment = GuardEnrollment::QUALIFIED;
			record.scene_ready_state = Registry::Record::SceneReadyState::IN_FRAME;
			record.levels[GDEXTENSION_INITIALIZATION_SCENE].binding_action.serial = 4;
			record.private_refs = 1;
			holder.typed_code_hold = true;
			holder.handle_serial = 3;
			holder.generation = 1;
			holder.acquired_thread = record.owner_thread;
			holder.terminal_action = 4;
			record.holders = &holder;
			auto &device = registry->device_records[0];
			device.object = &rd;
			device.live = true;
			device.epoch = 1;
			device.session = 1;
			device.execution_thread = rd.render_thread_id;
			registry->device_refs[0] = { 6, 0, 0, false, false };
			registry->device_refs[1] = { 7, 0, 0, true, false };
			reference = RecordRef(registry, 0);
			frame.enter(reference, true);
			frame.frame_id = 8;
			frame.level_phase = LevelPhase::SCENE_READY;
			registry->views[0] = { 5, 0, Registry::ViewDomain::SCENE_READY, 8, 0 };
			registry->views[1] = { 9, 0, Registry::ViewDomain::PRODUCER_REGISTRATION, 0, 0 };
		}
		~Enrollment() {
			// Also restore TLS/singleton on a REQUIRE exception. These are actual
			// no-effect production returns, not a second allocator or raw arena free.
			for (auto *attachment : registry->production_attachments) {
				if (!attachment) {
					continue;
				}
				for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
					auto &request = attachment->requests[i];
					if (request.serial) {
						registry->production_cancel({ INSTANCE, request.serial });
						registry->production_release({ INSTANCE, request.serial });
					}
				}
				registry->production_close({ INSTANCE, attachment->serial });
			}
			frame.leave();
			reference.reset();
			registry->records[0].holders = nullptr;
			memdelete(registry);
			rd.driver = nullptr;
			rd.frames.clear();
			rd.native_finalization_returned = true;
			RenderingDevice::singleton = previous;
		}
		GDExtensionProductionAttachmentRefV1 accept(const GDExtensionProductionCapacityV1 &capacity) {
			GDExtensionProductionMetadataQuoteV1 quote = {};
			CHECK(rd.draw_graph.qualification_command_count() == 1);
			REQUIRE(registry->production_quote(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, &capacity, &quote).native_error == OK);
			CHECK(quote.graph_slots == 8);
			GDExtensionProductionAttachmentRefV1 attachment = {};
			GDExtensionProductionInstallationReportV1 report = {};
			REQUIRE(registry->production_accept(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, { INSTANCE, 3 }, &quote, &attachment, &report).native_error == OK);
			CHECK(report.state == GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1);
			CHECK(report.allocated_bytes == quote.allocation_bytes);
			CHECK(report.returned_bytes == 0);
			return attachment;
		}
		Error reserve(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionCapacityV1 &capacity, uint64_t nonce, Request *&out) {
			MutexLock lock(registry->mutex);
			GDExtensionDeviceExit::ProductionAttachment *attachment = nullptr;
			RenderingDevice *device = nullptr;
			Error e = registry->production_attachment_locked(ref, true, attachment, device);
			if (e != OK) {
				return e;
			}
			const GDExtensionProductionOperationV1 operation = { ABI, nonce, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			return registry->production_reserve_metadata_locked(*attachment, operation, capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, out);
		}
	};
	static void accepted_arena_lifecycle() {
		Enrollment fixture;
		auto &registry = *fixture.registry;
		auto &record = registry.records[0];
		auto &device = registry.device_records[0];
		const uint32_t private_before = record.private_refs, device_before = device.refs;
		GDExtensionProductionCapacityV1 whole = { Enrollment::ABI, 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		auto single = whole;
		single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
		single.native_memberships = 16;
		single.retained_payload_bytes = 4;
		const auto attachment = fixture.accept(whole);
		CHECK(record.private_refs == private_before + 1);
		CHECK(device.refs == device_before + 1);
		CHECK(fixture.holder.production_children == 1);
		CHECK(record.cleanup_reservations == 2);
		CHECK(record.cleanup_by_phase[uint8_t(GDExtensionDeviceExit::CleanupPhase::BINDING)] == 2);
		Request *first = nullptr, *second = nullptr, *refused = nullptr;
		REQUIRE(fixture.reserve(attachment, single, 11, first) == OK);
		const auto first_ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, first->serial };
		const uint64_t serial_before = registry.production_serials.next;
		auto too_large = single;
		too_large.retained_payload_bytes = 5;
		CHECK(fixture.reserve(attachment, too_large, 12, refused) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(refused == nullptr);
		CHECK(registry.production_serials.next == serial_before);
		CHECK(first->attachment->children == 1);
		// With current=1 the old overall quote had 7 slots; this second real
		// Reserve needs four disjoint slots and must succeed in the quoted arena.
		REQUIRE(fixture.reserve(attachment, single, 12, second) == OK);
		CHECK(first->command_count == 4);
		CHECK(second->command_count == 4);
		CHECK((first->commands + first->command_count <= second->commands || second->commands + second->command_count <= first->commands));
		CHECK(first->attachment->children == 2);
		CHECK(first->attachment == second->attachment);
		CHECK(first->payload_offset != second->payload_offset);
		const auto second_ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, second->serial };
		const uint8_t payload[4] = { 1, 2, 3, 4 };
		const GDExtensionProductionUploadV1 upload = { Enrollment::ABI, 10, 0, 0, 0, 4 };
		CHECK(registry.production_prepare_upload(first_ref, &upload, payload, 4).native_error == OK);
		REQUIRE(registry.production_cancel(first_ref).native_error == OK);
		CHECK(registry.production_prepare_upload(first_ref, &upload, payload, 4).native_error == ERR_BUSY);
		REQUIRE(registry.production_release(first_ref).native_error == OK);
		CHECK(registry.production_release(first_ref).native_error != OK);
		CHECK(second->attachment->children == 1);
		CHECK(registry.production_close(attachment).native_error == ERR_BUSY);
		CHECK(second->cancelled);
		CHECK(record.private_refs == private_before + 1);
		CHECK(fixture.holder.production_children == 1);
		REQUIRE(registry.production_release(second_ref).native_error == OK);
		REQUIRE(registry.production_close(attachment).native_error == OK);
		CHECK(record.private_refs == private_before);
		CHECK(device.refs == device_before);
		CHECK(fixture.holder.production_children == 0);
		CHECK(record.cleanup_reservations == 0);
		CHECK(record.cleanup_by_phase[uint8_t(GDExtensionDeviceExit::CleanupPhase::BINDING)] == 0);
		for (auto *entry : registry.production_attachments) {
			CHECK(entry == nullptr);
		}
		CHECK(fixture.rd.draw_graph.qualification_command_count() == 1);
	}
	static void immutable_payload() {
		using namespace GDExtensionDeviceExit;
		static Registry registry;
		ProductionAttachment attachment;
		ProductionRequest request;
		ProductionResourceSlot resource;
		ProductionActionCell actions[2];
		ProductionUseCell use;
		use.request = &request;
		use.declaration = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
		request.uses = &use;
		request.use_count = 1;
		uint8_t storage[4] = {};
		attachment.payload = storage;
		request.attachment = &attachment;
		request.serial = 9;
		request.actions = actions;
		request.action_count = 2;
		request.resources = &resource;
		request.resource_count = 1;
		request.capacity.retained_payload_bytes = sizeof(storage);
		resource.request = &request;
		resource.request_serial = request.serial;
		resource.declaration.resource_kind = GDEXTENSION_PRODUCTION_STORAGE_V1;
		resource.declaration.physical_bytes = 8;
		resource.imported = true; // Pure metadata test: no physical/native call occurs.
		for (uint32_t i = 0; i < 2; ++i) {
			actions[i].request = &request;
			actions[i].declaration.canonical_suboperation = i + 10;
			actions[i].declaration.action_kind = GDEXTENSION_PRODUCTION_UPLOAD_V1;
			actions[i].declaration.use_count = 1;
		}
		const uint8_t original[4] = { 2, 4, 6, 8 }, changed[4] = { 2, 4, 6, 9 };
		GDExtensionProductionUploadV1 upload = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 10, 0, 0, 0, 4 };
		upload.destination_offset = 4;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_INVALID_DATA);
		CHECK(request.payload_used == 0);
		upload.destination_offset = 0;
		REQUIRE(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == OK);
		CHECK(request.payload_used == 4);
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == OK);
		CHECK(request.payload_used == 4);
		CHECK(registry.production_prepare_upload_locked(request, upload, changed, 4).native_error == ERR_INVALID_DATA);
		CHECK(storage[3] == 8);
		upload.canonical_suboperation = 11;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(actions[1].prepared);
		CHECK(registry.production_freeze_locked(request) == ERR_UNCONFIGURED);
		CHECK_FALSE(request.frozen);
		request.action_count = 1;
		REQUIRE(registry.production_freeze_locked(request) == OK);
		upload.canonical_suboperation = 10;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_BUSY);
		CHECK(storage[3] == 8);
	}
	static Error quote(const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands = 0, uint32_t workers = 0) {
		return GDExtensionDeviceExit::Registry::production_layout(capacity, out, current_commands, workers);
	}
};
TEST_CASE("[ProductionRequest] quote checks dimensions before reserving real cells") {
	GDExtensionProductionCapacityV1 capacity = {};
	capacity.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	GDExtensionProductionMetadataQuoteV1 quote = {};
	CHECK(Access::quote(capacity, quote) == ERR_INVALID_PARAMETER);
	capacity.requests = 1;
	capacity.suboperations = 2;
	capacity.resources = 2;
	capacity.resource_uses = 2;
	capacity.graph_memberships = 2;
	capacity.native_memberships = 2;
	capacity.return_cells = 2;
	REQUIRE(Access::quote(capacity, quote) == OK);
	CHECK(quote.allocation_bytes > sizeof(GDExtensionDeviceExit::ProductionRequest));
	CHECK(quote.cleanup_slots == 2);
	CHECK(quote.graph_frames == 3);
	CHECK(quote.graph_slots == 5);
	const uint64_t original_bytes = quote.allocation_bytes;
	capacity.retained_payload_bytes = 64;
	REQUIRE(Access::quote(capacity, quote) == OK);
	CHECK(quote.allocation_bytes >= original_bytes + 64);
	capacity.readback_bytes = UINT64_MAX;
	CHECK(Access::quote(capacity, quote) == ERR_PARAMETER_RANGE_ERROR);
	capacity.readback_bytes = 0;
	capacity.graph_memberships = UINT32_MAX;
	CHECK(Access::quote(capacity, quote) == ERR_PARAMETER_RANGE_ERROR);
}
TEST_CASE("[ProductionRequest] forged metadata never resolves an original attachment") {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionProductionReportV1 report = {};
	GDExtensionProductionServiceBudgetV1 budget = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 0, 0, 0, 0, 0 };
	CHECK(registry.production_observe({ UINT64_MAX, 1 }, &budget, &report).native_error != OK);
	CHECK(registry.production_cancel({ UINT64_MAX, 1 }).native_error != OK);
	CHECK(registry.production_release({ UINT64_MAX, 1 }).native_error != OK);
}
} // namespace TestProductionRequest

TEST_CASE("[ProductionRequest] payload retry is exact and the finite batch freezes before effects") {
	TestProductionRequest::Access::immutable_payload();
}

TEST_CASE("[ProductionRequest] attachment quote retains each request command prefix") {
	using TestProductionRequest::Access;
	GDExtensionProductionCapacityV1 whole = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 2, 2, 2, 2, 2, 32, 0, 2, 0, 0 };
	GDExtensionProductionCapacityV1 single = whole;
	single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
	single.native_memberships = 16;
	GDExtensionProductionMetadataQuoteV1 total = {}, first = {}, second = {};
	REQUIRE(Access::quote(whole, total, 1, 0) == OK);
	REQUIRE(Access::quote(single, first, 1, 0) == OK);
	REQUIRE(Access::quote(single, second, 1, 0) == OK);
	CHECK(total.graph_slots == 8);
	CHECK(total.graph_slots == first.graph_slots + second.graph_slots);
	CHECK(total.graph_frames == first.graph_frames + second.graph_frames);
	const uint64_t without_workers = total.allocation_bytes;
	REQUIRE(Access::quote(whole, total, 7, 3) == OK);
	REQUIRE(Access::quote(single, first, 7, 3) == OK);
	REQUIRE(Access::quote(single, second, 7, 3) == OK);
	CHECK(total.graph_slots == first.graph_slots + second.graph_slots);
	CHECK(total.allocation_bytes > without_workers);
	const auto retained = total;
	CHECK(Access::quote(whole, total, UINT32_MAX, 0) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(total.allocation_bytes == retained.allocation_bytes);
	CHECK(total.graph_slots == retained.graph_slots);
	CHECK(Access::quote(whole, total, 1, UINT32_MAX) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(total.graph_slots == retained.graph_slots);
}

TEST_CASE("[ProductionRequest] actual accepted arena reserves two requests and refunds on close") {
	TestProductionRequest::Access::accepted_arena_lifecycle();
}
