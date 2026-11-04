/**************************************************************************/
/*  test_native_disposal.cpp                                              */
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

#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_native_disposal)

class TestNativeDisposalAccess {
public:
	static void attach(RenderingDevice &p_rd, RenderingDeviceDriver &p_driver) {
		p_rd.driver = &p_driver;
		p_rd.is_main_instance = true;
		p_rd.render_thread_id = Thread::get_caller_id();
		p_rd.frames.resize(1);
	}
	static void detach(RenderingDevice &p_rd) {
		p_rd.frames.clear();
		p_rd.driver = nullptr;
	}
	static Error create(RenderingDevice &p_rd, RID &r_rid, NativeDisposalWitness &r_witness) {
		return p_rd._native_disposal_storage_create(128, {}, r_rid, r_witness);
	}
	static Error release(RenderingDevice &p_rd, const NativeDisposalWitness &p_witness) { return p_rd._native_disposal_free(p_witness); }
	static Error observe(RenderingDevice &p_rd, const NativeDisposalWitness &p_witness, NativeDisposalWitness::Report &r_report) { return p_rd._native_disposal_observe(p_witness, r_report); }
	static void dispose(RenderingDevice &p_rd) { p_rd._free_pending_resources(0); }
	static Error unconfigured_maintenance(RenderingDevice &p_rd) {
		p_rd.frames.clear();
		const Error result = p_rd._native_disposal_maintenance();
		p_rd.frames.resize(1);
		return result;
	}
	static void local(RenderingDevice &p_rd, bool p_local) { p_rd.is_main_instance = !p_local; }
	static void worker(RenderingDevice &p_rd, RID p_rid, bool p_active) { p_rd.storage_buffer_owner.get_or_null(p_rid)->transfer_worker_index = p_active ? 0 : -1; }
	static void poison(RenderingDevice &p_rd, RID p_rid) { ++p_rd.storage_buffer_owner.get_or_null(p_rid)->backing_serial; }
	static int refs(const NativeDisposalWitness &p_witness) { return p_witness.cell->get_reference_count(); }
	static NativeDisposalWitness::State state(const NativeDisposalWitness &p_witness) { return p_witness.cell->state; }
	static void repair_for_fixture_cleanup(RenderingDevice &p_rd, RID p_rid) {
		// Deliberate fixture-only undo; the production API cannot clear UNKNOWN.
		auto *buffer = p_rd.storage_buffer_owner.get_or_null(p_rid);
		--buffer->backing_serial;
		buffer->native_disposal.cell->state = NativeDisposalWitness::State::LIVE;
	}
	static RID shader(RenderingDevice &p_rd) {
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(31);
		shader.uniform_sets.resize(1);
		RenderingDevice::ShaderUniform uniform;
		uniform.type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = 0;
		uniform.length = 128;
		uniform.writable = true;
		shader.uniform_sets.write[0].push_back(uniform);
		shader.set_formats.push_back(1);
		return p_rd.shader_owner.make_rid(shader);
	}
	static void release_shader(RenderingDevice &p_rd, RID p_shader) {
		p_rd.free_rid(p_shader);
		dispose(p_rd);
		CHECK(p_rd.dependency_map.is_empty());
		CHECK(p_rd.reverse_dependency_map.is_empty());
	}
	static Error set(RenderingDevice &p_rd, RID p_buffer, RID p_shader, RID &r_set, NativeDisposalWitness &r_witness, bool p_unsupported = false) {
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = p_unsupported ? RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = 0;
		uniform.append_id(p_buffer);
		return p_rd._native_disposal_uniform_set_create(VectorView<RenderingDevice::Uniform>(&uniform, 1), p_shader, 0, r_set, r_witness);
	}
};

namespace TestNativeDisposal {
class Driver : public TransferTestDriverBase {
public:
	NativeDisposalWitness *buffer_witness = nullptr;
	NativeDisposalWitness *set_witness = nullptr;
	int creates = 0, frees = 0, set_creates = 0, set_frees = 0, shader_frees = 0;
	bool fail_create = false;
	uint32_t buffer_names = 0, set_names = 0;
	String last_buffer_name, last_set_name;
	BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		++creates;
		return fail_create ? BufferID() : BufferID(19);
	}
	void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override {
		// Only the real DEV buffer/set naming calls belong to this fixture.
		CHECK(p_name.begins_with("RID:"));
		if (p_type == OBJECT_TYPE_BUFFER) {
			CHECK(creates > 0);
			CHECK_FALSE(fail_create);
			CHECK(p_driver_id.id == 19);
			++buffer_names;
			last_buffer_name = p_name;
		} else {
			REQUIRE(p_type == OBJECT_TYPE_UNIFORM_SET);
			CHECK(set_creates > 0);
			CHECK(p_driver_id.id == 23);
			++set_names;
			last_set_name = p_name;
		}
	}
	void buffer_free(BufferID p_id) override {
		CHECK(p_id.id == 19);
		if (buffer_witness) {
			CHECK(TestNativeDisposalAccess::state(*buffer_witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*buffer_witness) >= 2);
		}
		++frees;
	}
	UniformSetID uniform_set_create(VectorView<BoundUniform>, ShaderID, uint32_t, int p_pool) override {
		CHECK(p_pool == -1);
		++set_creates;
		return UniformSetID(23);
	}
	void uniform_set_free(UniformSetID) override {
		CHECK(TestNativeDisposalAccess::state(*set_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(TestNativeDisposalAccess::refs(*set_witness) >= 2);
		++set_frees;
	}
	void shader_free(ShaderID p_id) override {
		CHECK(p_id.id == 31);
		++shader_frees;
	}
};

TEST_CASE("[RenderingDevice][NativeDisposal] real buffer queue retains metadata until driver free returns") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
#ifdef DEV_ENABLED
	CHECK(driver.buffer_names == 1);
	CHECK(driver.set_names == 0);
	CHECK(driver.last_buffer_name == "RID:" + itos(rid.get_id()));
#else
	CHECK(driver.buffer_names == 0);
	CHECK(driver.set_names == 0);
#endif
	driver.buffer_witness = &witness;
	NativeDisposalWitness::Report report;
	CHECK(TestNativeDisposalAccess::observe(rd, witness, report) == OK);
	CHECK(report.state == NativeDisposalWitness::State::LIVE);
	CHECK(report.rid == rid.get_id());
	CHECK(report.serial != 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.frees == 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.frees == 1);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(TestNativeDisposalAccess::refs(witness) == 1);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] set cascade reports each actual free independently") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness buffer, set;
	RID rid, set_rid;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, buffer) == OK);
	const RID shader = TestNativeDisposalAccess::shader(rd);
	CHECK(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set, true) == ERR_UNAVAILABLE);
	CHECK(driver.set_creates == 0);
	TestNativeDisposalAccess::worker(rd, rid, true);
	CHECK(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set) == ERR_UNAVAILABLE);
	CHECK(driver.set_creates == 0);
	TestNativeDisposalAccess::worker(rd, rid, false);
	REQUIRE(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set) == OK);
#ifdef DEV_ENABLED
	CHECK(driver.buffer_names == 1);
	CHECK(driver.set_names == 1);
	CHECK(driver.last_buffer_name == "RID:" + itos(rid.get_id()));
	CHECK(driver.last_set_name == "RID:" + itos(set_rid.get_id()));
#else
	CHECK(driver.buffer_names == 0);
	CHECK(driver.set_names == 0);
#endif
	driver.buffer_witness = &buffer;
	driver.set_witness = &set;
	CHECK(TestNativeDisposalAccess::release(rd, buffer) == OK);
	CHECK(TestNativeDisposalAccess::state(set) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(TestNativeDisposalAccess::state(buffer) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.frees == 0);
	CHECK(driver.set_frees == 0);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.set_frees == 1);
	CHECK(driver.frees == 1);
	CHECK(TestNativeDisposalAccess::state(set) == NativeDisposalWitness::State::RETURNED);
	CHECK(TestNativeDisposalAccess::state(buffer) == NativeDisposalWitness::State::RETURNED);
	CHECK(driver.shader_frees == 0);
	TestNativeDisposalAccess::release_shader(rd, shader);
	CHECK(driver.shader_frees == 1);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] preflight and serial mismatch cannot manufacture returned") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	CHECK(TestNativeDisposalAccess::unconfigured_maintenance(rd) == ERR_UNCONFIGURED);
	TestNativeDisposalAccess::local(rd, true);
	CHECK(TestNativeDisposalAccess::create(rd, rid, witness) == ERR_UNAUTHORIZED);
	CHECK(driver.creates == 0);
	TestNativeDisposalAccess::local(rd, false);
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
	TestNativeDisposalAccess::poison(rd, rid);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == ERR_INVALID_DATA);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::UNKNOWN);
	CHECK(driver.frees == 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == ERR_INVALID_DATA);
	TestNativeDisposalAccess::repair_for_fixture_cleanup(rd, rid);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	TestNativeDisposalAccess::detach(rd);
}
TEST_CASE("[RenderingDevice][NativeDisposal] failed native creation leaves no witness or hidden pending resource") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	driver.fail_create = true;
	ERR_PRINT_OFF;
	const Error failed = TestNativeDisposalAccess::create(rd, rid, witness);
	ERR_PRINT_ON;
	CHECK(failed == ERR_CANT_CREATE);
	CHECK(rid.is_null());
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 0);
	NativeDisposalWitness::Report report;
	CHECK(TestNativeDisposalAccess::observe(rd, witness, report) == ERR_UNAUTHORIZED);
	driver.fail_create = false;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.frees == 1);
	TestNativeDisposalAccess::detach(rd);
}

} // namespace TestNativeDisposal
