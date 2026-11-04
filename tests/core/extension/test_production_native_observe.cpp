/**************************************************************************/
/*  test_production_native_observe.cpp                                    */
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
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_production_native_observe)

namespace TestProductionNativeObserve {
struct Access {
	using Registry = GDExtensionDeviceExit::Registry;
	using Request = GDExtensionDeviceExit::ProductionRequest;
	struct QuoteDriver : TransferTestDriverBase {
		uint32_t retains[128] = {}, polls[128] = {}, releases[128] = {};
		uint64_t next_serial = 1;
		Registry *loss_registry = nullptr;
		bool callback_blocked = false, callback_delivered = false;
		Error retain_error = OK, release_error = OK, issue_error = OK;
		uint64_t fail_release_serial = 0;
		bool complete = false, native_error = false, unknown = false;
		uint32_t qualification_native_call_bound_for_profile(uint32_t main, uint32_t workers, uint32_t frames) const override { return frames && frames <= main && workers <= UINT32_MAX - main ? main + workers : 0; }
		Error native_call_retain(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++retains[serial];
			return retain_error;
		}
		Error native_call_release(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++releases[serial];
			return serial == fail_release_serial ? release_error : OK;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++polls[serial];
			if (loss_registry) {
				// Probe from another thread: try_lock on a mutex already owned
				// by this thread would itself be undefined behavior in RED.
				struct Probe {
					Registry *registry;
					bool acquired = false;
					static void run(void *data) {
						auto &probe = *static_cast<Probe *>(data);
						probe.acquired = probe.registry->mutex.try_lock();
						if (probe.acquired) {
							probe.registry->mutex.unlock();
						}
					}
				} probe{ loss_registry };
#ifdef THREADS_ENABLED
				Thread thread;
				thread.start(&Probe::run, &probe);
				CHECK_MESSAGE(thread.is_started(), "loss callback probe thread must start");
				if (!thread.is_started()) {
					callback_blocked = true;
					return {};
				}
				thread.wait_to_finish();
#else
				Probe::run(&probe); // No-thread builds use the dummy mutex.
#endif
				callback_blocked = !probe.acquired;
				if (probe.acquired) {
					loss_registry->device_lost(Enrollment::INSTANCE, 1);
					callback_delivered = true;
				}
			}

			return { complete, native_error, serial + 1000, native_error ? -27 : 0, unknown ? IssueFact::NotIssued : IssueFact::KnownAccepted };
		}
		uint32_t record_native_call_bound(uint32_t slots) const override { return slots; }
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
			uint32_t index = UINT32_MAX;
			if (!claim_native_fact(index)) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			NativeCallFact fact;
			fact.serial = next_serial++;
			fact.completion_serial = fact.serial + 1000;
			fact.queue_index = 1;
			fact.completion = CompletionCapability::Fence;
			fact.issue = unknown ? IssueFact::IssuedUnknown : IssueFact::KnownAccepted;
			fact.error = issue_error;
			fact.native_code = issue_error == OK ? 0 : -9;
			finish_native_fact(index, fact, fact.serial + 2000);
			return issue_error;
		}
		NativeCallFact issue() {
			QueueCallReport report;
			command_queue_execute_and_present_observed(CommandQueueID(1), {}, CommandBufferID(2), {}, FenceID(3), {}, report);
			REQUIRE(report.calls.size() == 1);
			if (report.calls.size() != 1) {
				return {};
			}
			return report.calls[0];
		}
	};
	struct Holder : GDExtensionDeviceExit::HolderRetirement {
		Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
	};
	// Controlled original driver harness; no real GPU is initialized.
	struct Enrollment {
		RenderingDevice *previous = RenderingDevice::get_singleton();
		RenderingDevice rd;
		QuoteDriver driver;
		Registry *registry = memnew(Registry);
		Holder holder;
		GDExtensionQualificationBridgeRefV1 qualification = {};
		GDExtensionDeviceExit::RecordRef reference;
		GDExtensionDeviceExit::RegistrationContext frame;
		static constexpr GDExtensionDeviceExitAbi ABI = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		static constexpr uint64_t INSTANCE = 9101;
		static void original_terminator(void *, GDExtensionInitializationLevel) {}
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
			auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
			scene.initialized = true;
			scene.binding_action.serial = 4;
			scene.binding_action.generation = 1;
			scene.binding_action.owner_thread = record.owner_thread;
			scene.binding_action.level = GDEXTENSION_INITIALIZATION_SCENE;
			scene.binding_action.kind = TerminalActionKind::LEVEL_BINDING;
			scene.binding_action.services = 1u << 5;
			scene.binding_action.wrapper = &original_terminator;
			scene.binding_action.userdata = this;
			record.original_initialization.deinitialize = &original_terminator;
			record.original_initialization.userdata = this;
			registry->live_services = 1u << 5;
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
			// Test teardown restores TLS/singleton even after REQUIRE. Injected
			// driver cells are discarded here, never credited as production proof.
			// Metadata itself returns through its original allocated owner.
			for (auto *attachment : registry->production_attachments) {
				if (!attachment) {
					continue;
				}
				for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
					auto &request = attachment->requests[i];
					if (request.serial) {
						request.native_count = 0;
						request.graph_count = 0;
						request.source_device_lost = false;
						request.effects_pending = false;
						registry->production_cancel({ INSTANCE, request.serial });
						registry->production_release({ INSTANCE, request.serial });
					}
				}
				registry->production_close({ INSTANCE, attachment->serial });
			}
			if (qualification.handle_serial) {
				registry->qualification_close(qualification);
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
			const auto production_quote_error = registry->production_quote(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, &capacity, &quote).native_error;
			REQUIRE(production_quote_error == OK);
			if (production_quote_error != OK) {
				return {};
			}
			CHECK(quote.graph_slots == uint64_t(capacity.suboperations) * 2 + 2);
			GDExtensionProductionAttachmentRefV1 attachment = {};
			GDExtensionProductionInstallationReportV1 report = {};
			const auto production_accept_error = registry->production_accept(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, { INSTANCE, 3 }, &quote, &attachment, &report).native_error;
			REQUIRE(production_accept_error == OK);
			if (production_accept_error != OK) {
				return {};
			}
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

	static GDExtensionProductionCapacityV1 capacity(uint32_t n = 3) {
		GDExtensionProductionCapacityV1 c = {};
		c.abi = Enrollment::ABI;
		c.requests = 1;
		c.suboperations = c.resources = c.resource_uses = c.graph_memberships = c.return_cells = n;
		// Original quote includes the existing one-command prefix plus all
		// graph memberships and n+1 frames; the driver quotes main+workers.
		c.native_memberships = 2 * n + 2;
		return c;
	}
	static GDExtensionProductionServiceBudgetV1 budget(uint32_t receipts = 128, uint32_t passes = 64) { return { Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, receipts, passes, 0 }; }
	static Request *setup(Enrollment &e, uint32_t n = 3) {
		auto c = capacity(n);
		auto a = e.accept(c);
		Request *r = nullptr;
		REQUIRE(e.reserve(a, c, 31, r) == OK);
		return r;
	}
	static void capture(Enrollment &e, Request &r, uint32_t count) {
		// Rows originate only from this strict driver's actual issue/report call.
		// This test-only enrollment does not stand in for B's production window.
		for (uint32_t i = 0; i < count; ++i) {
			auto &c = r.natives[i];
			c.request = &r;
			c.owner_index = i;
			c.member.source = e.driver.issue();
		}
		r.native_count = count;
	}
	static GDExtensionDeviceExit::ProductionControlOutput turn(Enrollment &e, Request &r, GDExtensionDeviceExit::ProductionControlKind kind, const GDExtensionProductionServiceBudgetV1 &b) {
		uint64_t serial = 0;
		auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r.serial };
		const Error accepted = e.registry->production_control_accept(ref, kind, &serial, &b);
		REQUIRE(accepted == OK);
		if (accepted != OK) {
			return {};
		}
		// Exact fixed mailbox dispatch; original FIFO publication is covered by the
		// existing ProductionRequest singleton tests. No second queue is created.
		uint64_t retry = 0;
		REQUIRE(e.registry->production_control_accept(ref, kind, &retry, &b) == OK);
		CHECK(retry == serial);
		auto changed = b;
		++changed.receipts;
		CHECK(e.registry->production_control_accept(ref, kind, &retry, &changed) == ERR_BUSY);
		r.control.state = GDExtensionDeviceExit::ProductionControlState::QUEUED;
		{
			MutexLock lock(e.registry->mutex);
			e.registry->production_control_execute_locked(r, &e.rd);
		}
		GDExtensionDeviceExit::ProductionControlOutput out = {};
		REQUIRE(e.registry->production_control_consume(ref, serial, &out) == OK);
		return out;
	}
	static void bounds_and_fairness() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 3);
		auto b = budget(1, 1);
		auto zero = b;
		zero.receipts = 0;
		const auto revision = r->revision;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, zero);
		CHECK(out.service_usage.receipts == 0);
		CHECK(r->observe_cursor == 0);
		CHECK(r->revision == revision);
		auto expired = b;
		expired.deadline_micros = 1;
		turn(e, *r, ProductionControlKind::OBSERVE, expired);
		CHECK(r->observe_cursor == 0);
		for (uint32_t i = 1; i <= 3; ++i) {
			out = turn(e, *r, ProductionControlKind::OBSERVE, b);
			CHECK(out.service_usage.receipts == 1);
			CHECK(out.service_usage.passes == 1);
			CHECK(e.driver.retains[i] == 1);
			CHECK(e.driver.polls[i] == 0);
		}
		for (uint32_t i = 1; i <= 3; ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, b);
			CHECK(e.driver.polls[i] == 1);
			CHECK(e.driver.retains[i] == 1);
		}
		e.driver.complete = true;
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.report.complete_obligations == 3);
		CHECK(out.report.known_obligations == 3);
		CHECK(out.report.recorded_end == 10);
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 0);
		CHECK(e.driver.polls[1] == 2);
	}
	static void alias_and_errors() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		e.driver.issue_error = FAILED;
		capture(e, *r, 1);
		r->natives[1].request = r;
		r->natives[1].owner_index = 0;
		r->natives[1].member.source = r->natives[0].member.source;
		r->native_count = 2;
		e.driver.native_error = true;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(e.driver.retains[1] == 1);
		CHECK(e.driver.polls[1] == 1);
		CHECK(out.report.fact_count == 2);
		CHECK(out.report.known_obligations == 1);
		CHECK(r->natives[0].observation.completion_error == FAILED);
		CHECK(r->natives[0].observation.error == FAILED);
		GDExtensionProductionFactV1 facts[2] = {};
		uint32_t written = 0;
		auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		REQUIRE(e.registry->production_facts(ref, r->revision, 0, facts, 2, &written).native_error == OK);
		CHECK(written == 2);
		CHECK(facts[0].issue_error == FAILED);
		CHECK(facts[0].issue_native_code == -9);
		CHECK(facts[0].completion_native_code == -27);
		CHECK(facts[0].completion == 3);
		CHECK(facts[1].native_serial == facts[0].native_serial);
		e.driver.native_error = false;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(r->natives[0].observation.completion_error == OK);
		CHECK(r->natives[0].observation.error == FAILED);
		CHECK(e.driver.polls[1] == 2);
		const auto old = r->revision;
		r->natives[1].member.source.completion_serial++;
		CHECK(e.registry->production_facts(ref, old, 0, facts, 2, &written).native_error == ERR_INVALID_DATA);
		r->natives[1].member.source = r->natives[0].member.source;
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
	}
	static void loss_and_retention() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		e.driver.unknown = true;
		capture(e, *r, 1);
		e.driver.retain_error = ERR_BUSY;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 1);
		CHECK(e.driver.polls[1] == 0);
		CHECK(r->natives[0].member.retention_error == ERR_BUSY);
		e.driver.retain_error = OK;
		e.driver.complete = true;
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 2);
		CHECK(out.report.complete_obligations == 0);
		CHECK(out.report.possible_obligations == 1);
		e.registry->device_records[0].live = false;
		auto zero = budget();
		zero.controls = 0;
		auto revision = r->revision;
		turn(e, *r, ProductionControlKind::OBSERVE, zero);
		CHECK_FALSE(r->source_device_lost);
		CHECK(r->revision == revision);
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(r->source_device_lost);
		CHECK(out.result.native_error == ERR_UNAVAILABLE);
		CHECK(e.driver.polls[1] == 1);
		e.registry->device_records[0].live = true;
		e.driver.unknown = false;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(e.driver.polls[1] == 1);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, r->serial }).native_error == ERR_BUSY);
	}
	static void partial_release() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 3);
		e.driver.complete = true;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		const auto children = r->attachment->children;
		const auto revision = r->revision;
		auto zero = budget();
		zero.receipts = 0;
		auto out = turn(e, *r, ProductionControlKind::RELEASE, zero);
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
		CHECK(r->revision == revision);
		CHECK(r->release_cursor == 0);
		e.driver.fail_release_serial = 2;
		e.driver.release_error = ERR_BUSY;
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(out.owner_usage.receipts == 3);
		CHECK(e.driver.releases[1] == 1);
		CHECK(e.driver.releases[2] == 1);
		CHECK(e.driver.releases[3] == 1);
		CHECK(r->attachment->children == children);
		CHECK(r->natives[0].member.owner_released);
		CHECK_FALSE(r->natives[1].member.owner_released);
		CHECK(r->release_started);
		auto *attachment = r->attachment;
		e.driver.release_error = OK;
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == OK);
		CHECK(out.owner_usage.receipts == 1);
		CHECK(e.driver.releases[1] == 1);
		CHECK(e.driver.releases[2] == 2);
		CHECK(e.driver.releases[3] == 1);
		CHECK(attachment->children == children - 1);
		CHECK(r->serial == 0);
	}
	static void expired_owner_turn() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 1);
		e.driver.complete = true;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		auto b = budget();
		b.deadline_micros = OS::get_singleton()->get_ticks_usec() + 1000;
		uint64_t serial = 0;
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const Error accepted = e.registry->production_control_accept(ref, ProductionControlKind::RELEASE, &serial, &b);
		REQUIRE(accepted == OK);
		if (accepted != OK) {
			return;
		}
		r->control.state = ProductionControlState::QUEUED;
		{
			MutexLock lock(e.registry->mutex);
			e.registry->production_control_execute_locked(*r, &e.rd);
		}
		OS::get_singleton()->delay_usec(2000);
		ProductionControlOutput out = {};
		REQUIRE(e.registry->production_control_consume(ref, serial, &out) == OK);
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(out.owner_usage.receipts == 0);
		CHECK(e.driver.releases[1] == 0);
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(e.registry->production_control_consume(ref, serial, &out) == ERR_DOES_NOT_EXIST);
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == OK);
		CHECK(e.driver.releases[1] == 1);
	}
	static void synchronous_loss_callback() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 2);
		e.driver.loss_registry = e.registry;
		e.driver.native_error = true;
		const auto children = r->attachment->children;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK_FALSE(e.driver.callback_blocked);
		CHECK(e.driver.callback_delivered);
		CHECK_FALSE(e.registry->device_records[0].live);
		CHECK(out.result.native_error == ERR_UNAVAILABLE);
		CHECK(r->source_device_lost);
		CHECK(r->natives[0].member.retained);
		CHECK(r->natives[0].observation.completion_error == FAILED);
		CHECK(r->natives[0].observation.completion_native_code == -27);
		CHECK(r->natives[0].observation.state ==
				NativeSourceObservation::State::Unknown);
		CHECK(e.driver.retains[1] == 1);
		CHECK(e.driver.polls[1] == 1);
		CHECK(e.driver.retains[2] == 0);
		CHECK(e.driver.polls[2] == 0);
		CHECK(out.report.complete_obligations == 0);
		CHECK(r->attachment->children == children);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, r->serial })
						.native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
		e.driver.loss_registry = nullptr;
		e.registry->device_records[0].live =
				true; // Fixture teardown only, never progress.
	}
	static void hard_ceiling() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e, 100);
		if (!r) {
			return;
		}
		capture(e, *r, 100);
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget(UINT32_MAX, UINT32_MAX));
		CHECK(out.service_usage.passes == 64);
		CHECK(out.service_usage.receipts == 128);
		CHECK(e.driver.retains[64] == 1);
		CHECK(e.driver.retains[65] == 0);
		CHECK(r->observe_cursor == 64);
		auto b = budget(1, 1);
		out = turn(e, *r, ProductionControlKind::OBSERVE, b);
		CHECK(e.driver.retains[65] == 1);
		CHECK(out.service_usage.receipts == 1);
	}
};
TEST_CASE("[ProductionNativeObserve] zero budgets and bounded fair original serial polling") {
	Access::bounds_and_fairness();
}
TEST_CASE("[ProductionNativeObserve] membership aliases preserve independent original issue and completion errors") {
	Access::alias_and_errors();
}
TEST_CASE("[ProductionNativeObserve] retention unknown and device loss retain obligations") {
	Access::loss_and_retention();
}
TEST_CASE("[ProductionNativeObserve] partial owner returns retain edges and retry only unreleased serials") {
	Access::partial_release();
}
TEST_CASE("[ProductionNativeObserve] expired immutable owner turn permits a new release budget") {
	Access::expired_owner_turn();
}
TEST_CASE("[ProductionNativeObserve] synchronous original device loss callback runs outside registry lock") {
	Access::synchronous_loss_callback();
}
TEST_CASE("[ProductionNativeObserve] private maintenance ceiling bounds oversized caller budgets") {
	Access::hard_ceiling();
}
} //namespace TestProductionNativeObserve
