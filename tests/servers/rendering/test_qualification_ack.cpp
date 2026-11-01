#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"
#include "tests/test_macros.h"

#include <atomic>
#include <thread>
TEST_FORCE_LINK(test_qualification_ack)
class TestQualificationAckAccess {
public:
	static void valid_native(GDExtensionDeviceExit::Registry::QualificationBridge::NativeUnit *unit, bool both) {
		unit->serial = 23;
		unit->retiring = true;
		unit->retire_serial = 31;
		unit->dependency_serial = 53;
		auto &backing = unit->backing;
		backing = {};
		backing.abi = { 1, 16, 0 };
		backing.unit = { 71, 23 };
		backing.known_mask = both ? 3 : 1;
		backing.event_seq = 3;
		backing.shader_rid = 41;
		backing.shader_serial = 43;
		if (both) {
			backing.pipeline_rid = 45;
			backing.pipeline_serial = 47;
		}
		auto &report = unit->report;
		report = {};
		report.abi = { 1, 16, 0 };
		report.unit = backing.unit;
		report.state = 1;
		report.return_status = 1;
		report.enqueued_mask = report.freed_mask = backing.known_mask;
		report.shader_rid = 41;
		report.shader_serial = 43;
		report.shader_frame_slot = 0;
		report.shader_frame_epoch = 72;
		report.shader_free_event_seq = 38;
		report.dependency_set_serial = 53;
		report.report_revision = 2;
		report.retire_operation = 59;
		auto &shader = unit->shader;
		shader.bridge = 11;
		shader.child = 23;
		shader.rid = 41;
		shader.serial = 43;
		shader.use_closed = true;
		shader.enqueued = true;
		shader.frame_slot = 0;
		shader.frame_epoch = 72;
		shader.reserved_event = shader.free_event = 38;
		auto &pipeline = unit->pipeline;
		pipeline.bridge = 11;
		pipeline.child = 23;
		pipeline.rid = both ? 45 : 0;
		pipeline.serial = both ? 47 : 0;
		pipeline.use_closed = true;
		pipeline.enqueued = both;
		pipeline.frame_slot = both ? 1 : UINT32_MAX;
		pipeline.frame_epoch = both ? 71 : 0;
		pipeline.reserved_event = pipeline.free_event = both ? 37 : 0;
		if (both) {
			report.pipeline_rid = 45;
			report.pipeline_serial = 47;
			report.pipeline_frame_slot = 1;
			report.pipeline_frame_epoch = 71;
			report.pipeline_free_event_seq = 37;
		}
	}
	static void native_original_return_validation(bool both) {
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
		record.private_refs = 2;
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
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = unit;
		for (uint32_t mode = 0; mode < 30; ++mode) {
			valid_native(unit, both);
			switch (mode) {
				case 0:
					unit->shader.enqueued = false;
					break;
				case 1:
					unit->shader.free_event = 0;
					break;
				case 2:
					++unit->report.shader_free_event_seq;
					break;
				case 3:
					++unit->report.shader_frame_epoch;
					break;
				case 4:
					++unit->report.shader_rid;
					break;
				case 5:
					++unit->shader.child;
					break;
				case 6:
					++unit->report.dependency_set_serial;
					break;
				case 7:
					unit->report.enqueued_mask = 0;
					break;
				case 8:
					unit->report.absent_mask = 1;
					break;
				case 9:
					unit->shader.free_event = unit->shader.reserved_event = unit->report.shader_free_event_seq = unit->backing.event_seq;
					break;
				case 10:
					unit->shader.use_closed = false;
					break;
				case 11:
					++unit->shader.reserved_event;
					break;
				case 12:
					++unit->report.unit.handle_serial;
					break;
				case 13:
					unit->shader.frame_slot = unit->report.shader_frame_slot = UINT32_MAX;
					break;
				case 14:
					++unit->report.shader_serial;
					break;
				case 15:
					++unit->shader.bridge;
					break;
				case 16:
					if (both) {
						unit->pipeline.free_event = 0;
					} else {
						unit->report.pipeline_rid = 45;
					}
					break;
				case 17:
					if (both) {
						++unit->report.pipeline_frame_epoch;
					} else {
						unit->report.pipeline_frame_slot = 1;
					}
					break;
				case 18:
					if (both) {
						++unit->report.pipeline_free_event_seq;
					} else {
						unit->report.pipeline_free_event_seq = 38;
					}
					break;
				case 19:
					if (both) {
						++unit->pipeline.bridge;
					} else {
						unit->backing.pipeline_rid = 45;
					}
					break;
				case 20:
					if (both) {
						++unit->report.pipeline_rid;
					} else {
						unit->report.freed_mask = 3;
					}
					break;
				case 21:
					if (both) {
						++unit->pipeline.serial;
					} else {
						unit->report.pipeline_serial = 47;
					}
					break;
				case 22:
					if (both) {
						unit->pipeline.enqueued = false;
					} else {
						unit->report.enqueued_mask = 3;
					}
					break;
				case 23:
					if (both) {
						++unit->pipeline.reserved_event;
					} else {
						unit->pipeline.enqueued = true;
					}
					break;
				case 24:
					unit->retiring = false;
					break;
				case 25:
					unit->report.abi.reserved = 1;
					break;
				case 26:
					++unit->report.shader_frame_slot;
					break;
				case 27:
					unit->report.report_revision = 0;
					break;
				case 28:
					unit->dependency_serial = unit->report.dependency_set_serial = 0;
					break;
				case 29:
					if (both) {
						unit->pipeline.free_event = unit->pipeline.reserved_event = unit->report.pipeline_free_event_seq = unit->shader.free_event;
					} else {
						unit->report.retire_operation = 0;
					}
					break;
			}
			const auto refused = registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 });
			CHECK(refused.native_error == (mode == 24 ? ERR_DOES_NOT_EXIST : ERR_BUSY));
			CHECK_FALSE(unit->acknowledged);
			CHECK(bridge->children == 1);
			CHECK(record.private_refs == 2);
		}
		if (both) {
			// Pipeline-only is internally self-consistent but cannot be produced by
			// the original shader-then-pipeline native unit create path.
			valid_native(unit, true);
			unit->backing.known_mask = 2;
			unit->backing.shader_rid = unit->backing.shader_serial = 0;
			unit->report.enqueued_mask = unit->report.freed_mask = 2;
			unit->report.shader_rid = unit->report.shader_serial = 0;
			unit->report.shader_frame_slot = 0;
			unit->report.shader_frame_epoch = unit->report.shader_free_event_seq = 0;
			unit->shader.rid = unit->shader.serial = 0;
			unit->shader.enqueued = false;
			unit->shader.frame_slot = UINT32_MAX;
			unit->shader.frame_epoch = unit->shader.reserved_event = unit->shader.free_event = 0;
			const auto refused = registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 });
			CHECK(refused.native_error == ERR_BUSY);
			CHECK_FALSE(unit->acknowledged);
			CHECK(bridge->children == 1);
			CHECK(record.private_refs == 2);
		}
		valid_native(unit, both);
		const auto accepted = registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 });
		CHECK(accepted.native_error == OK);
		CHECK(unit->acknowledged);
		CHECK(bridge->children == 0);
		CHECK(record.private_refs == 1);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
	}
	static void reject_error_return(bool native) {
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
		record.private_refs = 2;
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
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->storage[0] = storage;
		bridge->units[0] = unit;
		storage->retire_serial = 31;
		storage->retiring = true;
		valid_native(unit, true);
		storage->report.state = unit->report.state = 1;
		storage->report.return_status = unit->report.return_status = 1;
		storage->physical.free_event = 37;
		for (uint32_t mode = 0; mode < 5; ++mode) {
			storage->report.return_status = unit->report.return_status = mode == 0 ? 0 : 1;
			storage->report.reason = unit->report.reason = mode == 1 ? 5 : 0;
			storage->report.native_error = unit->report.native_error = mode == 2 ? ERR_CANT_ACQUIRE_RESOURCE : OK;
			storage->report.native_code = unit->report.native_code = mode == 3 ? -7 : 0;
			storage->maintenance_error = unit->maintenance_error = mode == 4 ? FAILED : OK;
			auto invalid = native ? registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 }) : registry->qualification_storage_retire_release({ 71, 11 }, { 71, 31 });
			CHECK(invalid.native_error == ERR_BUSY);
			CHECK((native ? unit->acknowledged : storage->acknowledged) == false);
			CHECK(bridge->children == 1);
			CHECK(record.private_refs == 2);
		}
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(storage);
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
	}
	static void interleaving(bool native) {
		using namespace GDExtensionDeviceExit;
		// Deliberately constructs the original Registry cells, not a second service.
		// This unit control covers ack arbitration only, not physical proof issuance.
		auto *registry = memnew(Registry);
		RenderingDevice device;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 3;
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
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->storage[0] = storage;
		bridge->units[0] = unit;
		storage->serial = unit->serial = 23;
		storage->retire_serial = unit->retire_serial = 31;
		storage->retiring = unit->retiring = true;
		storage->queued = unit->queued = true;
		auto pending = native ? registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 }) : registry->qualification_storage_retire_release({ 71, 11 }, { 71, 31 });
		CHECK(pending.native_error == ERR_BUSY);
		CHECK(pending.reason == 16);
		CHECK(bridge->children == 2);
		CHECK(record.private_refs == 3);
		std::atomic<bool> visible = false, ack_attempted = false;
		std::thread service([&] {
			{
				MutexLock lock(registry->mutex);
				valid_native(unit, true);
				storage->report.state = 1;
				storage->report.return_status = 1;
				unit->report.report_revision = UINT64_MAX;
				storage->physical.free_event = 37;
			}
			visible = true;
			while (!ack_attempted.load()) {
				std::this_thread::yield();
			}
			{
				MutexLock lock(registry->mutex);
				storage->queued = unit->queued = false;
				--bridge->children;
				--record.private_refs;
			}
		});
		while (!visible.load()) {
			std::this_thread::yield();
		}
		auto first = native ? registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 }) : registry->qualification_storage_retire_release({ 71, 11 }, { 71, 31 });
		CHECK(first.native_error == ERR_BUSY);
		CHECK(first.reason == 19);
		CHECK(bridge->children == 2);
		CHECK(record.private_refs == 3);
		CHECK_FALSE((native ? unit->acknowledged : storage->acknowledged));
		ack_attempted = true;
		service.join();
		auto second = native ? registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 }) : registry->qualification_storage_retire_release({ 71, 11 }, { 71, 31 });
		CHECK(second.native_error == OK);
		CHECK(bridge->children == 0);
		CHECK(record.private_refs == 1);
		CHECK((native ? unit->acknowledged : storage->acknowledged));
		auto stale = native ? registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 31 }) : registry->qualification_storage_retire_release({ 71, 11 }, { 71, 31 });
		CHECK(stale.native_error != OK);
		CHECK(stale.reason == 17);
		CHECK(record.private_refs == 1);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		registry->qualification_bridges[0] = nullptr;
		memdelete(storage);
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
	}
};
#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] Returned before queue tail keeps storage ack retryable and exact once") {
	TestQualificationAckAccess::interleaving(false);
}
TEST_CASE("[Qualification] Returned before queue tail keeps native unit ack retryable and exact once") {
	TestQualificationAckAccess::interleaving(true);
}
#endif

TEST_CASE("[Qualification] storage retire ack rejects inconsistent returned reports") {
	TestQualificationAckAccess::reject_error_return(false);
}
TEST_CASE("[Qualification] native unit retire ack rejects inconsistent returned reports") {
	TestQualificationAckAccess::reject_error_return(true);
}

TEST_CASE("[Qualification] native shader-only ack requires original physical return association") {
	TestQualificationAckAccess::native_original_return_validation(false);
}
TEST_CASE("[Qualification] native two-unit ack requires original physical return association") {
	TestQualificationAckAccess::native_original_return_validation(true);
}
