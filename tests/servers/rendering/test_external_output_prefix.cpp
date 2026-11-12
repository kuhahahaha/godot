/**************************************************************************/
/*  test_external_output_prefix.cpp                                       */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <type_traits>

TEST_FORCE_LINK(test_external_output_prefix)

class TestExternalOutputPrefixAccess {
public:
	using Storage = RendererRD::ExternalOutputStorage;
	using Report = Storage::Report;
	using State = NativeDisposalWitness::State;
	using Layout = Storage::Instance192Layout;
	static Error plan(uint32_t p_capacity, uint32_t p_surfaces, Layout &r_layout) { return Storage::plan_instance192(p_capacity, p_surfaces, r_layout); }
	static Storage *make(RenderingDevice &p_rd) { return new Storage(p_rd); }
	static Error destroy(Storage *&p_storage) { return Storage::destroy_if_returned(p_storage); }
	static Error allocate(Storage &p_storage, uint32_t p_capacity, uint32_t p_surfaces = 1) { return p_storage.allocate_instance192(p_capacity, p_surfaces); }
	static Error retire(Storage &p_storage) { return p_storage.request_retirement(); }
	static Error observe(Storage &p_storage, Report &r_report) { return p_storage.observe(r_report); }
	static Error maintain(Storage &p_storage) { return p_storage.maintenance(); }
	static void attach(RenderingDevice &p_rd, RenderingDeviceDriver &p_driver) {
		p_rd.driver = &p_driver;
		p_rd.is_main_instance = true;
		p_rd.render_thread_id = Thread::get_caller_id();
		p_rd.frames.resize(1);
	}
	static void detach(RenderingDevice &p_rd) {
		CHECK(p_rd.storage_buffer_owner.get_rid_count() == 0);
		p_rd.frames.clear();
		p_rd.driver = nullptr;
	}
	static void dispose(RenderingDevice &p_rd) { p_rd._free_pending_resources(0); }
	static void other_thread(RenderingDevice &p_rd, bool p_other) { p_rd.render_thread_id = Thread::get_caller_id() ^ uint64_t(p_other); }
	static void local(RenderingDevice &p_rd, bool p_local) { p_rd.is_main_instance = !p_local; }
	static void busy(RenderingDevice &p_rd, bool p_busy) { p_rd.compute_list.active = p_busy; }
	static void poison(RenderingDevice &p_rd, RID p_rid) { ++p_rd.storage_buffer_owner.get_or_null(p_rid)->backing_serial; }
	static Error poison_and_free(RenderingDevice &p_rd, NativeDisposalWitness &p_witness, NativeDisposalWitness::Report &r_report) {
		NativeDisposalWitness::Report before;
		CRASH_COND(p_rd._native_disposal_observe(p_witness, before) != OK || before.state != State::LIVE);
		auto *buffer = p_rd.storage_buffer_owner.get_or_null(RID::from_uint64(before.rid));
		CRASH_COND(!buffer || buffer->native_disposal.cell != p_witness.cell || buffer->backing_serial != before.serial || buffer->backing_serial == UINT64_MAX);
		++buffer->backing_serial; // Only the real backing changes; not the witness.
		const Error result = p_rd._native_disposal_free(p_witness);
		CRASH_COND(p_rd._native_disposal_observe(p_witness, r_report) != ERR_INVALID_DATA);
		CRASH_COND(r_report.state != State::UNKNOWN || r_report.rid != before.rid || r_report.serial != before.serial);
		return result;
	}
	static Error restore_poison(RenderingDevice &p_rd, NativeDisposalWitness &p_witness, const NativeDisposalWitness::Report &p_report) {
		NativeDisposalWitness::Report before;
		CRASH_COND(p_rd._native_disposal_observe(p_witness, before) != ERR_INVALID_DATA);
		CRASH_COND(before.state != State::UNKNOWN || before.rid != p_report.rid || before.serial != p_report.serial);
		auto *buffer = p_rd.storage_buffer_owner.get_or_null(RID::from_uint64(before.rid));
		CRASH_COND(!buffer || buffer->native_disposal.cell != p_witness.cell || buffer->backing_serial != before.serial + 1);
		// Test-only undo of precisely the preceding backing injection. Original
		// OBSERVE failed before storage.observe() or group release; production
		// has no UNKNOWN reset, and this never repairs membership or custody.
		--buffer->backing_serial;
		p_witness.cell->state = State::LIVE;
		NativeDisposalWitness::Report after;
		const Error result = p_rd._native_disposal_observe(p_witness, after);
		CRASH_COND(after.state != State::LIVE || after.rid != before.rid || after.serial != before.serial);
		return result;
	}
	static void cleanup_unknown_for_test(RenderingDevice &p_rd, Storage *&p_storage) {
		// Test-only undo of injected corruption. Production has no UNKNOWN reset.
		for (auto &resource : p_storage->resources) {
			if (resource.witness.cell.is_valid() && resource.witness.cell->state == State::UNKNOWN) {
				auto *buffer = p_rd.storage_buffer_owner.get_or_null(resource.rid);
				--buffer->backing_serial;
				resource.witness.cell->state = State::LIVE;
				CHECK(p_rd._native_disposal_free(resource.witness) == OK);
			}
		}
		dispose(p_rd);
		delete p_storage;
		p_storage = nullptr;
	}
};

namespace RendererRD {
Error external_storage_poison_and_free(RenderingDevice &p_rd, NativeDisposalWitness &p_witness, NativeDisposalWitness::Report &r_report) {
	return TestExternalOutputPrefixAccess::poison_and_free(p_rd, p_witness, r_report);
}
Error external_storage_restore_poison(RenderingDevice &p_rd, NativeDisposalWitness &p_witness, const NativeDisposalWitness::Report &p_report) {
	return TestExternalOutputPrefixAccess::restore_poison(p_rd, p_witness, p_report);
}
} // namespace RendererRD

namespace TestExternalOutputPrefix {
using Access = TestExternalOutputPrefixAccess;
using Storage = RendererRD::ExternalOutputStorage;
using State = NativeDisposalWitness::State;
static_assert(!std::is_constructible_v<Storage, RenderingDevice &>);
static_assert(!std::is_destructible_v<Storage>);
static_assert(!std::is_copy_constructible_v<Storage>);

class Driver : public TransferTestDriverBase {
public:
	uint32_t creates = 0;
	uint32_t fail_on = 0;
	Vector<uint64_t> bytes;
	Vector<uint64_t> returned;
	Vector<String> names;
	Storage *storage = nullptr;
	BufferID buffer_create(uint64_t p_bytes, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		bytes.push_back(p_bytes);
		++creates;
		return creates == fail_on ? BufferID() : BufferID(100 + creates);
	}
	void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override {
		// DEV_ENABLED names each successfully allocated storage buffer. Keep the
		// shared transfer driver strict; only this fixture admits that operation.
		CHECK(p_type == OBJECT_TYPE_BUFFER);
		CHECK(creates != fail_on);
		CHECK(p_driver_id.id == 100 + creates);
		CHECK(p_name.begins_with("RID:"));
		names.push_back(p_name);
	}
	void buffer_free(BufferID p_id) override {
		// Core must not publish RETURNED before this exact native call returns.
		if (storage) {
			Access::Report report;
			REQUIRE(Access::observe(*storage, report) == OK);
			CHECK(!report.all_driver_returned);
			CHECK(report.enqueued_count > 0);
		}
		CHECK(p_id.id >= 101);
		CHECK(p_id.id <= 100 + creates);
		CHECK(returned.find(p_id.id) == -1);
		returned.push_back(p_id.id);
	}
};

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] third allocation failure retains exact native prefix until returned") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	driver.fail_on = 3;
	ERR_PRINT_OFF;
	CHECK(Access::allocate(*storage, 2) == ERR_CANT_CREATE);
	ERR_PRINT_ON;
	CHECK(driver.creates == 3);
	CHECK(driver.bytes[0] == 384);
	CHECK(driver.bytes[1] == 384);
	CHECK(driver.bytes[2] == 32);
	Access::Report report;
	REQUIRE(Access::observe(*storage, report) == OK);
	CHECK(report.created_count == 2);
#ifdef DEV_ENABLED
	REQUIRE(driver.names.size() == 2);
	CHECK(driver.names[0] == "RID:" + itos(report.resources[0].native.rid));
	CHECK(driver.names[1] == "RID:" + itos(report.resources[1].native.rid));
#else
	CHECK(driver.names.is_empty());
#endif
	CHECK(report.enqueued_count == 2);
	CHECK(report.all_driver_returned == false);
	CHECK(report.creation_error == ERR_CANT_CREATE);
	CHECK(report.resources[0].native.rid != report.resources[1].native.rid);
	CHECK(report.resources[0].native.serial != report.resources[1].native.serial);
	CHECK(report.resources[0].native.serial != 0);
	const uint64_t rid = report.resources[0].native.rid;
	const uint64_t serial = report.resources[0].native.serial;
	CHECK(Access::allocate(*storage, 2) == ERR_ALREADY_IN_USE);
	CHECK(Access::destroy(storage) == ERR_BUSY);
	CHECK(driver.creates == 3);
	CHECK(driver.returned.is_empty());
	Access::dispose(rd); // The real RD queue, a fake driver, no draw or frame guess.
	REQUIRE(Access::observe(*storage, report) == OK);
	CHECK(report.all_driver_returned);
	CHECK(report.returned_count == 2);
	CHECK(report.resources[0].native.rid == rid);
	CHECK(report.resources[0].native.serial == serial);
	CHECK(driver.returned.size() == 2);
	CHECK(driver.returned.has(101));
	CHECK(driver.returned.has(102));
	CHECK(Access::destroy(storage) == OK);
	CHECK(storage == nullptr);
	Access::detach(rd);
}

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] zero capacity omits records and retains real control extents") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	REQUIRE(Access::allocate(*storage, 0, 2) == OK);
	CHECK(driver.creates == 5);
	const uint64_t expected[] = { 32, 64, 32, 40, 528 }; // 240-byte proof header + two 144-byte surface rows.
	for (uint32_t i = 0; i < 5; ++i) {
		CHECK(driver.bytes[i] == expected[i]);
	}
	Access::Report report;
	REQUIRE(Access::observe(*storage, report) == OK);
	CHECK(report.resources[0].native.state == State::EMPTY);
	CHECK(report.resources[1].native.state == State::EMPTY);
	CHECK(report.resources[0].bytes == 0);
	CHECK(report.resources[1].bytes == 0);
	CHECK(report.total_bytes == 696);
	CHECK(report.live_count == 5);
	CHECK(Access::destroy(storage) == ERR_BUSY);
	CHECK(Access::retire(*storage) == OK);
	Access::dispose(rd);
	CHECK(Access::destroy(storage) == OK);
	Access::detach(rd);
}

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] all sizes preflight before native creation") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	CHECK(Access::allocate(*storage, UINT32_MAX) == ERR_INVALID_PARAMETER);
	CHECK(Access::allocate(*storage, 1, UINT32_MAX) == ERR_INVALID_PARAMETER);
	CHECK(Access::allocate(*storage, 1, RendererRD::ExternalOutputConfig::MAX_FLATTENED_SURFACES + 1) == ERR_INVALID_PARAMETER);
	CHECK(Access::allocate(*storage, 1, 0) == ERR_INVALID_PARAMETER);
	CHECK(driver.creates == 0);
	CHECK(Access::destroy(storage) == OK);
	Access::detach(rd);
}

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] pure layout rejects surface and capacity extrema without changing output") {
	using Config = RendererRD::ExternalOutputConfig;
	Access::Layout layout;
	for (uint32_t i = 0; i < 7; ++i) {
		layout.bytes[i] = 101 + i;
	}
	layout.total_bytes = 0x123456789abcdef0ULL;
	layout.resource_count = 43;
	const auto sentinel = layout;
	const uint32_t invalid[][2] = {
		{ 0, 0 },
		{ 1, Config::MAX_FLATTENED_SURFACES + 1 },
		{ 1, UINT32_MAX },
		{ (1u << 20) + 1, 1 },
		{ UINT32_MAX, Config::MAX_FLATTENED_SURFACES },
	};
	for (const auto &extent : invalid) {
		CHECK(Access::plan(extent[0], extent[1], layout) == ERR_INVALID_PARAMETER);
		for (uint32_t i = 0; i < 7; ++i) {
			CHECK(layout.bytes[i] == sentinel.bytes[i]);
		}
		CHECK(layout.total_bytes == sentinel.total_bytes);
		CHECK(layout.resource_count == sentinel.resource_count);
	}
	// This is arithmetic preflight only: no surface directory or driver call.
	for (uint32_t capacity : { 0u, 1u, 1u << 20 }) {
		REQUIRE(Access::plan(capacity, Config::MAX_FLATTENED_SURFACES, layout) == OK);
		const uint64_t surfaces = Config::MAX_FLATTENED_SURFACES;
		CHECK(layout.bytes[0] == uint64_t(capacity) * 192);
		CHECK(layout.bytes[1] == uint64_t(capacity) * 192);
		CHECK(layout.bytes[2] == 32);
		CHECK(layout.bytes[3] == 64);
		CHECK(layout.bytes[4] == 32);
		CHECK(layout.bytes[5] == surfaces * 20);
		CHECK(layout.bytes[6] == 240 + surfaces * 144);
		CHECK(layout.total_bytes == uint64_t(capacity) * 384 + 368 + surfaces * 164);
		CHECK(layout.resource_count == (capacity ? 7u : 5u));
	}
}

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] busy retirement retries without allocation or next draw") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	REQUIRE(Access::allocate(*storage, 1) == OK);
	Access::busy(rd, true);
	CHECK(Access::retire(*storage) == ERR_BUSY);
	CHECK(Access::maintain(*storage) == ERR_BUSY); // Actual Core preflight, no hidden scheduler.
	Access::Report report;
	REQUIRE(Access::observe(*storage, report) == OK);
	CHECK(report.live_count == 7);
	CHECK(report.retirement_requested);
	CHECK(Access::allocate(*storage, 1) == ERR_ALREADY_IN_USE);
	CHECK(driver.returned.is_empty());
	Access::busy(rd, false);
	CHECK(Access::retire(*storage) == OK);
	CHECK(Access::retire(*storage) == OK);
	Access::dispose(rd);
	CHECK(Access::observe(*storage, report) == OK);
	CHECK(report.all_driver_returned);
	CHECK(driver.creates == 7);
	CHECK(driver.returned.size() == 7);
	CHECK(Access::destroy(storage) == OK);
	Access::detach(rd);
}

TEST_CASE("[RenderingDevice][ExternalOutputPrefix] unknown is sticky and cannot become releasable on retry") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	REQUIRE(Access::allocate(*storage, 0) == OK);
	Access::Report report;
	REQUIRE(Access::observe(*storage, report) == OK);
	Access::poison(rd, RID::from_uint64(report.resources[2].native.rid));
	CHECK(Access::retire(*storage) == ERR_INVALID_DATA);
	CHECK(Access::observe(*storage, report) == ERR_INVALID_DATA);
	CHECK(report.unknown_count == 1);
	CHECK(report.enqueued_count == 4);
	CHECK(!report.all_driver_returned);
	CHECK(Access::retire(*storage) == ERR_INVALID_DATA);
	CHECK(Access::destroy(storage) == ERR_INVALID_DATA);
	CHECK(Access::allocate(*storage, 0) == ERR_ALREADY_IN_USE);
	// Test cleanup only; permanently retained in production while UNKNOWN.
	driver.storage = nullptr;
	Access::cleanup_unknown_for_test(rd, storage);
	Access::detach(rd);
}
TEST_CASE("[RenderingDevice][ExternalOutputPrefix] closed empty and first failure prefixes cannot reopen") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	CHECK(Access::retire(*storage) == OK);
	CHECK(Access::allocate(*storage, 1) == ERR_ALREADY_IN_USE);
	CHECK(driver.creates == 0);
	CHECK(Access::destroy(storage) == OK);
	storage = Access::make(rd);
	driver.storage = storage;
	driver.fail_on = 1;
	ERR_PRINT_OFF;
	CHECK(Access::allocate(*storage, 1) == ERR_CANT_CREATE);
	ERR_PRINT_ON;
	Access::Report report;
	CHECK(Access::observe(*storage, report) == OK);
	CHECK(report.created_count == 0);
	CHECK(report.all_driver_returned); // Empty prefix only, no OutputReleased.
	CHECK(report.creation_error == ERR_CANT_CREATE);
	CHECK(report.retirement_requested);
	CHECK(Access::allocate(*storage, 1) == ERR_ALREADY_IN_USE);
	CHECK(driver.creates == 1);
	CHECK(driver.returned.is_empty());
	CHECK(Access::destroy(storage) == OK);
	Access::detach(rd);
}
TEST_CASE("[RenderingDevice][ExternalOutputPrefix] wrong device owner cannot close or destroy even an empty prefix") {
	RenderingDevice rd;
	Driver driver;
	Access::attach(rd, driver);
	Storage *storage = Access::make(rd);
	driver.storage = storage;
	Access::local(rd, true);
	CHECK(Access::allocate(*storage, 1) == ERR_UNAUTHORIZED);
	CHECK(Access::retire(*storage) == ERR_UNAUTHORIZED);
	Access::Report report;
	CHECK(Access::observe(*storage, report) == ERR_UNAUTHORIZED);
	CHECK(Access::destroy(storage) == ERR_UNAUTHORIZED);
	CHECK(driver.creates == 0);
	Access::local(rd, false);
	Access::other_thread(rd, true);
	CHECK(Access::retire(*storage) == ERR_UNAUTHORIZED);
	CHECK(Access::destroy(storage) == ERR_UNAUTHORIZED);
	Access::other_thread(rd, false);
	CHECK(Access::observe(*storage, report) == OK);
	CHECK(!report.retirement_requested);
	CHECK(report.created_count == 0);
	CHECK(Access::destroy(storage) == OK);
	Access::detach(rd);
}
} // namespace TestExternalOutputPrefix

namespace TestExternalOutputPrefix {
TEST_CASE("[RenderingDevice][ExternalOutputPrefix] flattened surface table allocates beyond eight mesh RIDs") {
	using Config = RendererRD::ExternalOutputConfig;
	for (uint32_t surfaces : { 9u, Config::MAX_FLATTENED_SURFACES }) {
		for (uint32_t capacity : { 0u, 1u }) {
			RenderingDevice rd;
			Driver driver;
			Access::attach(rd, driver);
			Storage *storage = Access::make(rd);
			driver.storage = storage;
			REQUIRE(Access::allocate(*storage, capacity, surfaces) == OK);
			Access::Report report;
			REQUIRE(Access::observe(*storage, report) == OK);
			CHECK(report.resources[5].bytes == surfaces * 20);
			CHECK(report.resources[6].bytes == 240 + surfaces * 144);
			CHECK(driver.creates == (capacity == 0 ? 5u : 7u));
			CHECK(Access::destroy(storage) == ERR_BUSY);
			CHECK(Access::retire(*storage) == OK);
			CHECK(driver.returned.is_empty());
			Access::dispose(rd);
			CHECK(driver.returned.size() == driver.creates);
			CHECK(Access::destroy(storage) == OK);
			Access::detach(rd);
		}
	}
}

} // namespace TestExternalOutputPrefix
