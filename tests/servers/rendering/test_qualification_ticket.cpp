#include "tests/test_macros.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "core/os/thread.h"
#include "core/os/semaphore.h"
#include "servers/rendering/rendering_server.h"
#include <thread>
TEST_FORCE_LINK(test_qualification_ticket)

// These controls feed deliberately distinct source observations into the
// original ticket reducer. They do not issue Registry authority or certify a
// native dispatch; the actual Metal control remains separately required.
class TestQualificationTicketAccess {
public:
#ifdef THREADS_ENABLED
	static void replay_under_contention(bool p_prepared = false) {
		using namespace GDExtensionDeviceExit;
		// Seed only original Registry cells; this exercises replay, not issuance.
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device; device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0]; record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1; record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 7;
		auto &native = registry->device_records[0]; native.object = &device; native.live = true;
		native.epoch = 1; native.execution_thread = Thread::get_caller_id();
		struct Holder : HolderRetirement { Holder() : HolderRetirement(RecordRef{}) {} ~Holder() = default; } holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11; bridge->epoch = 1; bridge->holder = &holder; bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23; ticket->bridge_serial = 11;
		ticket->origin.abi = {1, sizeof(GDExtensionDeviceExitAbi), 0};
		ticket->origin.record_generation = 37; ticket->origin.request_nonce = 39;
		ticket->report.revision = 5; ticket->report.state = 4;
		ticket->prepared = p_prepared;
		ticket->preparation.abi = {1, sizeof(GDExtensionDeviceExitAbi), 0};
		ticket->preparation.record_generation = 37; ticket->preparation.request_nonce = 39;
		ticket->preparation.library_generation = 1; ticket->preparation.qualification_session_serial = 7;
		ticket->preparation.parent_serial = 31; ticket->preparation.reservation_serial = 97;
		ticket->preparation.attempt = 1; ticket->preparation.upload_count = 2;
		ticket->preparation.max_segments = 2; ticket->preparation.expected_result_bytes = 16;
		ticket->uploads.resize(2);
		for (uint32_t i = 0; i < 2; ++i) {
			auto &plan = ticket->uploads[i].plan;
			plan.abi = {1, sizeof(GDExtensionDeviceExitAbi), 0}; plan.upload_index = i;
			plan.role = i; plan.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1;
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
		GDExtensionQualificationUploadPlanV1 plans[] = {ticket->uploads[1].plan, ticket->uploads[0].plan};
		auto invoke = [&] { return p_prepared ? registry->qualification_prepare_uploads({71, 11}, &prepare, plans, 2, &out, &report) : registry->qualification_record({71, 11}, &request, &out, &report); };
		auto status = invoke();
		CHECK(status.native_error == OK);
		CHECK(out.registry_instance == 71); CHECK(out.handle_serial == 23);
		CHECK(memcmp(&report, &ticket->report, sizeof(report)) == 0);
		out = {}; request.body_groups_x++; plans[0].role++;
		CHECK(invoke().native_error == ERR_INVALID_DATA);
		CHECK(out.handle_serial == 0);
		request = ticket->origin; plans[0] = ticket->uploads[1].plan; ticket->acknowledged = true;
		CHECK(invoke().native_error == ERR_DOES_NOT_EXIST);
		ticket->acknowledged = false; request.request_nonce++; prepare.request_nonce++;
		CHECK(invoke().native_error == ERR_BUSY);
		CHECK(bridge->children == 1); CHECK(record.private_refs == 7);
		CHECK(device.draw_graph.qualification_command_count() == 0);
		release.post(); holder_thread.join();
		RenderingDevice::singleton = previous; device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(ticket); memdelete(bridge); memdelete(registry);
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
	static void snapshot_after_overlap() {
		using namespace GDExtensionDeviceExit;
		// Seed CPU snapshots only. This does not issue a native ticket or
		// establish GPU completion; the Metal control proves that separately.
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device; device.is_main_instance = true;
		registry->instance = 71;
		registry->records[0].owner_thread = Thread::get_caller_id();
		registry->records[0].snapshot.library_generation = 1;
		auto &native = registry->device_records[0];
		native.object = &device; native.live = true; native.epoch = 1;
		struct Holder : HolderRetirement { Holder() : HolderRetirement(RecordRef{}) {} ~Holder() = default; } holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11; bridge->epoch = 1; bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23; ticket->bridge_serial = 11;
		ticket->report.revision = 5; ticket->report.state = 8; ticket->report.owned_size = 16;
		ticket->physically_retired.store(true);
		ticket->result.instantiate(); ticket->result->delivered = true;
		const uint32_t expected_bytes[] = { 7, 8, 9, 10 };
		ticket->result->bytes.resize(sizeof(expected_bytes));
		memcpy(ticket->result->bytes.ptrw(), expected_bytes, sizeof(expected_bytes));
		GDExtensionQualificationFactV1 expected_facts[2] = {};
		expected_facts[0].graph_kind = 1; expected_facts[0].graph_index = 17;
		expected_facts[1].graph_kind = 2; expected_facts[1].graph_index = 29;
		for (const auto &fact : expected_facts) ticket->facts.push_back(fact);
		bridge->tickets[0] = ticket;
		const GDExtensionQualificationBridgeRefV1 bridge_ref = {71, 11};
		const GDExtensionQualificationTicketRefV1 ticket_ref = {71, 23};
		GDExtensionQualificationFactV1 fact = {};
		fact.graph_kind = 0x12345678;
		const auto original_fact = fact;
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 0, 1, &fact, &fact.graph_kind).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&fact, &original_fact, sizeof(fact)) == 0);
		uint32_t written = 99;
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 1, 1, &fact, &written).native_error == OK);
		CHECK(written == 1); CHECK(memcmp(&fact, &expected_facts[1], sizeof(fact)) == 0);
		CHECK(registry->qualification_facts(bridge_ref, ticket_ref, 5, 0, 1, &fact, &written).native_error == OK);
		CHECK(written == 1); CHECK(memcmp(&fact, &expected_facts[0], sizeof(fact)) == 0);
		uint32_t destination[] = { 0x12345678, 0x87654321, 0xdeadbeef, 0xa5a5a5a5 };
		const auto original_first = destination[0];
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &destination[0]).native_error == ERR_INVALID_PARAMETER);
		CHECK(destination[0] == original_first);
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &written).native_error == OK);
		CHECK(written == sizeof(destination)); CHECK(memcmp(destination, expected_bytes, sizeof(destination)) == 0);
		memset(destination, 0, sizeof(destination));
		CHECK(registry->qualification_copy(bridge_ref, ticket_ref, 5, destination, sizeof(destination), &written).native_error == OK);
		CHECK(written == sizeof(destination)); CHECK(memcmp(destination, expected_bytes, sizeof(destination)) == 0);
		CHECK(ticket->report.revision == 5); CHECK(!ticket->acknowledged);
		RenderingDevice::singleton = previous; device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(ticket); memdelete(bridge); memdelete(registry);
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
		ticket.result->request_serial = 101;
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

	static void continuation_capacity() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			mutable uint32_t last_slots = 0;
			bool native_call_observation_supported() const override { return true; }
			uint32_t qualification_native_call_bound(uint32_t slots) const override { last_slots = slots; return slots; }
			uint64_t limit_get(Limit) override { return 65535; }
		} driver;
		RenderingDevice device; device.driver = &driver;
		auto *registry = memnew(Registry); registry->records[0].snapshot.library_generation = 1;
		Registry::QualificationBridge bridge;
		GDExtensionQualificationRecordV1 request = {};
		request.abi = {1, sizeof(GDExtensionDeviceExitAbi), 0};
		request.record_generation = request.request_nonce = request.library_generation = request.qualification_session_serial = 1;
		request.attempt = 1; request.sample_count = request.range_end = 4; request.body_groups_x = 1;
		request.source_allocation_generation = 1;
		const uint32_t push[4] = {0, 4, 0, 0}; memcpy(request.body_push, push, 16); memcpy(request.capture_push, push, 16);
		struct Case { uint32_t bytes, workers, expected_slots; Error result; };
		const Case cases[] = {
			{4, 2, 12, ERR_INVALID_DATA},
			{4, 1022, 4092, ERR_INVALID_DATA},
			{4, 1023, 0, ERR_PARAMETER_RANGE_ERROR},
			{4, UINT32_MAX, 0, ERR_PARAMETER_RANGE_ERROR},
			{16384, 5, 3090, ERR_INVALID_DATA},
			{16384, 6, 3605, ERR_PARAMETER_RANGE_ERROR},
		};
		for (const auto &c : cases) {
			CAPTURE(c.bytes); CAPTURE(c.workers);
			device.transfer_worker_pool_max_size = c.workers; request.readback_size = c.bytes;
			driver.last_slots = 0; LocalVector<RID> resources;
			CHECK(registry->qualification_record_preflight(&bridge, &device, request, resources) == c.result);
			if (c.expected_slots) CHECK(driver.last_slots == c.expected_slots);
			CHECK(resources.is_empty()); CHECK(device.draw_graph.qualification_command_count() == 0);
		}
		// Invalid RID is deliberately later than capacity: no fake pipeline or
		// production grant is needed to observe preappend slot-budget admission.
		device.driver = nullptr; memdelete(registry);
	}

	static void unknown_native_error(uint32_t mode = 0) {
		using namespace GDExtensionDeviceExit;
		using RDD = RenderingDeviceDriver;
		struct Driver : TransferTestDriverBase {
			bool pending = false, finished = false;
			Error native_call_retain(uint64_t serial) override { CHECK(serial == 17); return OK; }
			FencePollResult native_call_poll(uint64_t serial) override {
				CHECK(serial == 17);
				FencePollResult out; out.completed_serial = serial;
				out.native_error = !pending; out.native_code = pending ? 0 : -1;
				out.completed = pending && finished; return out;
			}
		} driver;
		driver.pending = mode == 2;
		RenderingDevice device; device.driver = &driver; device.frames.resize(1);
		QualificationTicket ticket; ticket.report.revision = 1; ticket.frame_refs = 1;
		RDD::NativeCallFact source; source.serial = source.completion_serial = 17; source.queue_index = 3;
		source.issue = mode == 1 ? RDD::IssueFact::KnownAccepted : RDD::IssueFact::IssuedUnknown;
		source.error = mode == 2 ? OK : FAILED; source.native_code = mode == 2 ? 0 : -1;
		GDExtensionQualificationFactV1 fact = {}; fact.issue = uint32_t(source.issue); fact.completion = 1;
		fact.native_serial = fact.completion_serial = 17; fact.backend_queue = 3;
		ticket.facts.push_back(fact); ticket.natives.push_back({source, 0, 0, 0, 1, false, false});
		if (mode == 3) {
			auto failed_source = source; failed_source.serial = failed_source.completion_serial = 18;
			failed_source.issue = RDD::IssueFact::KnownAccepted;
			auto failed_fact = fact; failed_fact.issue = 1; failed_fact.completion = 3; failed_fact.error = FAILED;
			ticket.facts.push_back(failed_fact); ticket.natives.push_back({failed_source, 1, 0, 0, 1, true, false});
		}
		device.frames[0].qualification_tickets.push_back(&ticket);
		RenderingDevice::Buffer buffer; buffer.qualification_users = 1;
		RID rid = device.storage_buffer_owner.make_rid(buffer); ticket.resource_pins.push_back(rid);
		CHECK(device._qualification_poll_tickets(0) == (mode == 2 ? ERR_BUSY : FAILED));
		CHECK(ticket.facts[0].issue == (mode == 1 ? 1 : 2));
		CHECK(ticket.facts[0].completion == (mode == 2 ? 1 : mode == 1 ? 3 : 4));
		CHECK(ticket.facts[0].native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.possible_count == (mode == 1 ? 0 : 1));
		CHECK(ticket.report.known_count == (mode == 1 || mode == 3 ? 1 : 0));
		CHECK(ticket.report.state == (mode == 1 ? 7 : mode == 2 ? 4 : 6));
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		CHECK_FALSE(ticket.natives[0].owner_released);
		if (mode == 2) {
			driver.finished = true;
			CHECK(device._qualification_poll_tickets(0) == OK);
			CHECK(ticket.facts[0].issue == 1); CHECK(ticket.facts[0].completion == 2);
			CHECK(ticket.report.possible_count == 0); CHECK(ticket.report.known_count == 1);
			CHECK_FALSE(ticket.physically_retired.load());
		}
		// Synthetic reducer inputs own no GPU work. This is fixture cleanup,
		// never an acknowledgement or proof that an Unknown may retire.
		device.frames.clear(); device.storage_buffer_owner.free(rid); device.driver = nullptr;
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
  struct Driver:TransferTestDriverBase {int waits=0;Error fence_wait(FenceID)override{++waits;return ERR_TIMEOUT;}} driver;
  RenderingDevice device;device.driver=&driver;device.frames.resize(2);device.frames_drawn=10;device.frames[0].qualification_epoch=1;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},2,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  auto &staging=device.download_staging_buffers;staging.blocks.resize(1);staging.block_size=staging.max_size=64;staging.blocks.write[0].driver_id=RenderingDeviceDriver::BufferID(51);staging.blocks.write[0].frame_used=10;staging.blocks.write[0].fill_amount=32;
  RenderingDevice::Buffer buffer;buffer.driver_id=RenderingDeviceDriver::BufferID(71);buffer.size=64;buffer.backing_serial=11;buffer.draw_tracker=RenderingDeviceGraph::resource_tracker_create();buffer.draw_tracker->buffer_driver_id=buffer.driver_id;RID rid=device.storage_buffer_owner.make_rid(buffer);
  QualificationTicket ticket;ticket.recorded=true;ticket.report.revision=1;ticket.origin.readback_buffer=rid.get_id();ticket.origin.readback_size=64;ticket.result.instantiate();ticket.result->qualification_owned=true;ticket.result->bytes.resize(64);
  auto &registry=Registry::get_singleton();std::atomic<bool> locked=false,release=false;
  std::thread holder([&]{device._thread_safe_.lock();locked=true;while(!release.load())std::this_thread::yield();device._thread_safe_.unlock();});
  while(!locked.load())std::this_thread::yield();
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==0);CHECK(ticket.result->copies.is_empty());CHECK(device.frames[0].download_buffer_get_data_requests.is_empty());CHECK(ticket.facts.is_empty());CHECK(ticket.frame_refs==0);
  release=true;holder.join();
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==32);CHECK(ticket.result->copies.size()==1);CHECK(ticket.frame_refs==1);CHECK_FALSE(ticket.physically_retired.load());
  staging.blocks.write[0].frame_used=0;
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==64);CHECK(ticket.result->copies.size()==2);CHECK(ticket.frame_refs==1);CHECK(ticket.facts.size()==2);CHECK(driver.waits==0);
  if(ticket.result->copies.size()==2){CHECK(ticket.result->copies[0].region.src_offset==0);CHECK(ticket.result->copies[1].region.src_offset==32);CHECK(ticket.result->copies[1].region.size==32);}
  RenderingDeviceGraph::resource_tracker_free(buffer.draw_tracker);device.storage_buffer_owner.free(rid);device.frames.clear();device.download_staging_buffers.blocks.clear();device.driver=nullptr;
 }
#endif
 static void reduction(bool copy_failure) {
  using namespace GDExtensionDeviceExit;
  RenderingDevice device;TransferTestDriverBase driver;device.driver=&driver;
  QualificationTicket ticket;ticket.report.revision=1;ticket.origin.readback_size=4;
  ticket.recorded=true;ticket.ordering_closed=true;ticket.result.instantiate();
  ticket.result->qualification_owned=true;ticket.result->bytes.resize(4);ticket.result->issued_size=4;
  RenderingDevice::Buffer buffer;buffer.qualification_users=1;RID rid=device.storage_buffer_owner.make_rid(buffer);ticket.resource_pins.push_back(rid);
  GDExtensionQualificationFactV1 graph={};graph.semantic_kind=3;graph.command_slot=0;ticket.facts.push_back(graph);
  ticket.graphs.push_back({{},0,0,0,1,true,false});ticket.frame_refs=1;
  for(uint32_t i=0;i<2;++i) {
   RenderingDeviceDriver::NativeCallFact source;source.serial=17+i;source.completion_serial=73+i;source.queue_index=1;
   GDExtensionQualificationFactV1 fact={};fact.issue=1;fact.completion=i?1:2;
   ticket.natives.push_back({source,ticket.facts.size(),0,0,1,true,false});ticket.facts.push_back(fact);
  }
  auto &registry=Registry::get_singleton();
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK(ticket.facts[0].native_serial==17);CHECK(ticket.facts[0].completion_serial==73);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==1);
  ticket.facts[2].completion=2;ticket.observation_dirty=true;
  if(copy_failure)ticket.result->error=ERR_CANT_ACQUIRE_RESOURCE;
  else {ticket.result->completed_size=4;ticket.result->delivered=true;}
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(ticket.report.owned_size==0);
  ticket.graphs[0].returned=true;ticket.frame_refs=0;
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK(ticket.physically_retired.load());CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==0);
  if(copy_failure){CHECK(ticket.report.state==5);CHECK(ticket.report.owned_size==0);CHECK(ticket.report.native_error==ERR_CANT_ACQUIRE_RESOURCE);CHECK(ticket.admission_closed.load());}
  else {CHECK(ticket.report.state==8);CHECK(ticket.report.owned_size==4);}
  device.storage_buffer_owner.free(rid);device.driver=nullptr;
 }
 static void owner_stop() {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);RenderingDevice device;auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=registry->private_limit;
  registry->device_records[0].object=&device;registry->device_records[0].live=true;registry->device_records[0].epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){} ~Holder()=default;};Holder holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=1;registry->qualification_bridges[0]=bridge;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->report.revision=1;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  auto ordinary=registry->qualification_cancel({71,11},{71,23});CHECK(ordinary.native_error==ERR_BUSY);CHECK(ordinary.reason==16);CHECK_FALSE(ticket->stop_requested);
  ticket->prepared=true;ticket->report.state=8;auto complete=registry->qualification_cancel({71,11},{71,23});CHECK(complete.native_error==ERR_BUSY);CHECK(complete.reason==16);CHECK(ticket->report.state==8);CHECK_FALSE(ticket->stop_requested);ticket->report.state=0;
#ifdef THREADS_ENABLED
  GDExtensionDeviceSessionResultV1 foreign={};std::thread other([&]{foreign=registry->qualification_cancel({71,11},{71,23});});other.join();CHECK(foreign.native_error==ERR_UNAUTHORIZED);CHECK(foreign.reason==15);CHECK_FALSE(ticket->stop_requested);
#endif
  auto stale=registry->qualification_cancel({71,11},{});CHECK(stale.native_error==ERR_DOES_NOT_EXIST);CHECK(stale.reason==17);
  auto accepted=registry->qualification_cancel({71,11},{71,23});CHECK(accepted.native_error==OK);CHECK(ticket->stop_requested);CHECK(ticket->admission_closed.load());CHECK_FALSE(ticket->physically_retired.load());CHECK_FALSE(ticket->queued);CHECK(bridge->children==1);CHECK(record.private_refs==registry->private_limit);
  CHECK(registry->qualification_cancel({71,11},{71,23}).native_error==OK);
  registry->qualification_ticket_publish(ticket,&device);CHECK(ticket->report.state==7);CHECK(ticket->report.phase==6);CHECK(ticket->report.owned_size==0);CHECK(ticket->report.required_count==0);CHECK(ticket->physically_retired.load());
  const uint64_t revision=ticket->report.revision;CHECK(registry->qualification_cancel({71,11},{71,23}).native_error==OK);CHECK(ticket->report.revision==revision);
  CHECK(registry->qualification_release({71,11},{71,23}).native_error==OK);CHECK(bridge->children==0);CHECK(record.private_refs==registry->private_limit-1);CHECK(registry->qualification_cancel({71,11},{71,23}).reason==17);
  RenderingDevice::singleton=previous;device.is_main_instance=false;registry->qualification_bridges[0]=nullptr;memdelete(ticket);memdelete(bridge);memdelete(registry);
 }
 static void unknown_keeps_pin() {
  using namespace GDExtensionDeviceExit;
  RenderingDevice device;TransferTestDriverBase driver;device.driver=&driver;QualificationTicket ticket;
  ticket.report.revision=1;ticket.append_failed=true;ticket.mapping_failed=true;
  RenderingDevice::Buffer buffer;buffer.qualification_users=1;RID rid=device.storage_buffer_owner.make_rid(buffer);ticket.resource_pins.push_back(rid);
  Registry::get_singleton().qualification_ticket_publish(&ticket,&device);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(ticket.report.state==6);CHECK(ticket.report.owned_size==0);CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==1);
  device.storage_buffer_owner.free(rid);device.driver=nullptr;
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
		device.draw_graph.initialize(&driver,
				[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
				1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
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
TEST_CASE("[Qualification] overlapping snapshot outputs are rejected without writing destination") { TestQualificationTicketAccess::snapshot_output_overlap(); }
TEST_CASE("[Qualification] same ticket snapshots remain readable after overlap refusal") { TestQualificationTicketAccess::snapshot_after_overlap(); }
TEST_CASE("[Qualification] original async map failure retains raw code and physical obligations") {
	TestQualificationTicketAccess::readback_map_error(0);
	TestQualificationTicketAccess::readback_map_error(1);
}
TEST_CASE("[Qualification] async map success and legacy failure preserve original outcome") {
	TestQualificationTicketAccess::readback_map_error(2);
	TestQualificationTicketAccess::readback_map_error(0, false);
}
TEST_CASE("[Qualification] frame poll failure retains original native code") { TestQualificationTicketAccess::frame_poll_error_retains_native_code(); }
TEST_CASE("[Qualification] ticket poll failure retains original native code") { TestQualificationTicketAccess::ticket_poll_error_retains_native_code(); }
TEST_CASE("[Qualification] ticket keeps distinct native and completion identities until every source and frame returns") {TestQualificationTicketAccess::reduction(false);}

TEST_CASE("[Qualification] sealing accepts an actual transitive graph path") { TestQualificationTicketAccess::transitive_graph_order(); }

TEST_CASE("[Qualification] zero-effect prepared ticket reacquires original resource pins before retry") { TestQualificationTicketAccess::no_effect_reacquires_resource_pin(); }
TEST_CASE("[Qualification] attempted upload keeps original writer pin across zero-effect busy publish") { TestQualificationTicketAccess::no_effect_reacquires_resource_pin(true); }
TEST_CASE("[Qualification] settled native work releases resource pins after a logical readback copy failure") {TestQualificationTicketAccess::reduction(true);}
TEST_CASE("[Qualification] unresolved source mapping keeps failed ticket resource pins") {TestQualificationTicketAccess::unknown_keeps_pin();}

TEST_CASE("[Qualification] owner stop is opt in exact ticket monotone and preserves immutable Complete") {TestQualificationTicketAccess::owner_stop();}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] exact record replay returns original ticket despite RD contention") { TestQualificationTicketAccess::replay_under_contention(); }
TEST_CASE("[Qualification] exact prepared replay returns canonical original plans despite RD contention") { TestQualificationTicketAccess::replay_under_contention(true); }
TEST_CASE("[Qualification] ticket continuation refuses contended RD before graph and resumes the same suffix") {TestQualificationTicketAccess::contended_continuation();}
#endif

TEST_CASE("[Qualification] unknown submit error preserves original issue possible set and resource obligations") { TestQualificationTicketAccess::unknown_native_error(); }

TEST_CASE("[Qualification] known accepted native error stays Failed and retains obligations") { TestQualificationTicketAccess::unknown_native_error(1); }
TEST_CASE("[Qualification] callback pending source stays pending and can become known successful") { TestQualificationTicketAccess::unknown_native_error(2); }
TEST_CASE("[Qualification] mixed Failed and Unknown sources preserve unresolved Unknown") { TestQualificationTicketAccess::unknown_native_error(3); }

TEST_CASE("[Qualification] continuation preflight reserves fresh hidden worker sources per graph frame") { TestQualificationTicketAccess::continuation_capacity(); }
