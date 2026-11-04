/**************************************************************************/
/*  test_production_resource_use.cpp                                      */
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

#pragma once

#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_production_resource_use)

// Real C++ Core metadata and RD RID owners with a fake driver. This controlled
// enrollment is not a public producer, native-open grant or GPU success.
class TestProductionResourceUseAccess {
	using Registry = GDExtensionDeviceExit::Registry;
	using Request = GDExtensionDeviceExit::ProductionRequest;
	using Slot = GDExtensionDeviceExit::ProductionResourceSlot;
	using UseCell = GDExtensionDeviceExit::ProductionUseCell;
	using Attachment = GDExtensionDeviceExit::ProductionAttachment;
	using Physical = GDExtensionDeviceExit::QualificationPhysicalReturn;
	static void identity(Physical &p, uint64_t child, uint64_t rid, uint64_t serial) {
		p.bridge = 11;
		p.child = child;
		p.rid = rid;
		p.serial = serial;
	}
	struct Holder : GDExtensionDeviceExit::HolderRetirement {
		Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) { generation = 1; }
	};

public:
	static void import_set(bool unsorted_original = false) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		RenderingDevice rd;
		TransferTestDriverBase driver;
		rd.driver = &driver;
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &rd;
		rd.is_main_instance = true;
		rd.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.occupied = record.attached = true;
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.producer_registration_serial = 2;
		record.producer_attachment_identity = &rd;
		record.producer_code_holder = 3;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		auto &device = registry->device_records[0];
		device.object = &rd;
		device.live = true;
		device.epoch = 1;
		Holder holder;
		holder.handle_serial = 3;
		Attachment attachment;
		attachment.serial = 10;
		attachment.epoch = 1;
		attachment.producer_serial = 2;
		attachment.library_generation = 1;
		attachment.holder = &holder;
		attachment.quote.capacity.requests = 2;
		Request requests[2];
		Slot slots[2][3];
		UseCell uses[2][4];
		attachment.requests = requests;
		registry->production_attachments[0] = &attachment;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		registry->qualification_bridges[0] = bridge;
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		unit->serial = 12;
		unit->creating = false;
		bridge->units[0] = unit;
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(21);
		shader.qualification_return = &unit->shader;
		shader.uniform_sets.resize(1);
		RenderingDevice::ShaderUniform reflection;
		reflection.binding = 0;
		reflection.type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		shader.uniform_sets.write[0].push_back(reflection);
		if (unsorted_original) {
			reflection.binding = 1;
			shader.uniform_sets.write[0].push_back(reflection);
		}
		RID shader_rid = rd.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(22);
		pipeline.shader = shader_rid;
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.qualification_return = &unit->pipeline;
		pipeline.set_formats.push_back(123);
		RID pipeline_rid = rd.compute_pipeline_owner.make_rid(pipeline);
		identity(unit->shader, 12, shader_rid.get_id(), 31);
		identity(unit->pipeline, 12, pipeline_rid.get_id(), 32);
		unit->origin.abi = { 1, 16, 0 };
		unit->origin.library_generation = 1;
		unit->origin.operation_serial = 15;
		unit->origin.container_byte_count = 16;
		unit->origin.container_digest[0] = 1;
		unit->origin.record_slot = 4;
		unit->origin.physical_unit = 5;
		unit->origin.preparation_generation = 6;
		unit->backing.abi = { 1, 16, 0 };
		unit->backing.unit = { 71, 12 };
		unit->backing.shader_rid = shader_rid.get_id();
		unit->backing.pipeline_rid = pipeline_rid.get_id();
		unit->backing.shader_serial = 31;
		unit->backing.pipeline_serial = 32;
		unit->backing.known_mask = 3;
		unit->backing.state = 1;
		auto *original_storage = memnew(Registry::QualificationBridge::Storage);
		original_storage->serial = 13;
		original_storage->creating = false;
		original_storage->origin = { { 1, 16, 0 }, 64, 7, 8, 9, 1, 0 };
		bridge->storage[0] = original_storage;
		auto &storage = original_storage->physical;
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RDD::BufferID(23);
		buffer.size = 64;
		buffer.backing_serial = 33;
		buffer.qualification_return = &storage;
		RID buffer_rid = rd.storage_buffer_owner.make_rid(buffer);
		identity(storage, 13, buffer_rid.get_id(), 33);
		original_storage->backing = { { 1, 16, 0 }, { 71, 13 }, buffer_rid.get_id(), 33, 64, 0, OK, 1 };
		auto *child = memnew(Registry::QualificationBridge::UniformSet);
		child->serial = 14;
		child->kind = 3;
		child->object_serial = 34;
		child->creating = false;
		child->source.session_serial = 50;
		child->source.generation = 51;
		child->source.digest[0] = 1;
		child->origin.abi = { 1, 16, 0 };
		child->origin.source = child->source;
		child->origin.library_generation = 1;
		child->origin.unit = { 71, 12 };
		child->origin.shader_rid = shader_rid.get_id();
		child->origin.shader_serial = 31;
		const uint32_t binding_count = unsorted_original ? 2 : 1;
		child->origin.binding_count = binding_count;
		if (unsorted_original) {
			child->bindings.push_back({ 1, 1, { 71, 13 }, buffer_rid.get_id(), 33, {}, 0, 0 });
		}
		child->bindings.push_back({ 0, 1, { 71, 13 }, buffer_rid.get_id(), 33, {}, 0, 0 });
		RenderingDevice::UniformSet native_set;
		native_set.driver_id = RDD::UniformSetID(24);
		native_set.shader_id = shader_rid;
		native_set.shader_set = 0;
		native_set.format = 123;
		native_set.qualification_return = &child->physical;
		native_set.qualification_dependencies.push_back(&unit->shader);
		native_set.qualification_dependencies.push_back(&storage);
		RID set_rid = rd.uniform_set_owner.make_rid(native_set);
		child->rid = set_rid.get_id();
		identity(child->physical, 14, set_rid.get_id(), 34);
		child->backing = { { 1, 16, 0 }, { 71, 14 }, child->source, set_rid.get_id(), 34, 0, 3, 1 };
		bridge->sets[0] = child;
		for (uint32_t i = 0; i < 2; ++i) {
			auto &request = requests[i];
			request.attachment = &attachment;
			request.serial = 60 + i;
			request.resources = slots[i];
			request.resource_count = 3;
			request.uses = uses[i];
			request.use_count = 1;
			request.capacity.resource_uses = 4;
			for (auto &use : uses[i]) {
				use.request = &request;
			}
			uses[i][0].declaration.resource_slot = 1;
			uses[i][0].declaration.byte_size = 64;
			for (auto &slot : slots[i]) {
				slot.request = &request;
				slot.request_serial = request.serial;
			}
			auto &u = slots[i][0];
			u.declaration.resource_kind = GDEXTENSION_PRODUCTION_UNIT_V1;
			u.declaration.ownership = GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1;
			u.declaration.original_slot = 4;
			u.declaration.original_part = 5;
			u.declaration.original_generation = 6;
			auto &b = slots[i][1];
			b.declaration.resource_kind = GDEXTENSION_PRODUCTION_STORAGE_V1;
			b.declaration.ownership = GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1;
			b.declaration.physical_bytes = 64;
			b.declaration.original_slot = 7;
			b.declaration.original_part = 8;
			b.declaration.original_generation = 9;
			slots[i][2].declaration.resource_kind = GDEXTENSION_PRODUCTION_SET_V1;
			slots[i][2].declaration.ownership = GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1;
			slots[i][2].declaration.original_slot = UINT32_MAX;
			slots[i][2].declaration.original_part = child->origin.shader_set;
			slots[i][2].declaration.original_generation = child->source.generation;
		}
		const GDExtensionProductionSetBindingV1 binding{ { 1, 16, 0 }, 0, 0, 0, 1, 0, 64 };
		auto call = [&](uint32_t index, const GDExtensionQualificationResourceBackingV1 &backing, const GDExtensionProductionSetBindingV1 &expected) {
			GDExtensionProductionSetBindingV1 bindings[2] = { expected, expected };
			bindings[1].binding = 1;
			return registry->production_import_set({ 71, requests[index].serial }, 2, &backing, 0, 0, bindings, binding_count).native_error;
		};
		struct LockState {
			RenderingDevice *device = nullptr;
			Semaphore locked{};
			Semaphore release{};
		} lock_state;
		lock_state.device = &rd;
		Thread locker;
		auto lock_rd = [](void *data) {
			auto *state = static_cast<LockState *>(data);
			state->device->_thread_safe_.lock();
			state->locked.post();
			state->release.wait();
			state->device->_thread_safe_.unlock();
		};
		locker.start(lock_rd, &lock_state);
		lock_state.locked.wait();
		const auto set_before_unit_import = call(0, child->backing, binding);
		const auto busy_storage = registry->production_import_storage({ 71, requests[0].serial }, 1, &original_storage->backing).native_error;
		const auto busy_unit = registry->production_import_unit({ 71, requests[0].serial }, 0, &unit->backing).native_error;
		lock_state.release.post();
		locker.wait_to_finish();
		CHECK(set_before_unit_import == ERR_INVALID_DATA);
		CHECK(busy_storage == ERR_BUSY);
		CHECK(busy_unit == ERR_BUSY);
		CHECK(storage.production_uses == nullptr);
		CHECK(unit->shader.production_uses == nullptr);
		CHECK(unit->pipeline.production_uses == nullptr);
		for (uint32_t i = 0; i < 2; ++i) {
			const GDExtensionProductionRequestRefV1 ref{ 71, requests[i].serial };
			auto bad_storage = original_storage->backing;
			bad_storage.backing.handle_serial = 999;
			CHECK(registry->production_import_storage(ref, 1, &bad_storage).native_error == ERR_DOES_NOT_EXIST);
			bad_storage = original_storage->backing;
			++bad_storage.backing_serial;
			CHECK(registry->production_import_storage(ref, 1, &bad_storage).native_error == ERR_INVALID_DATA);
			++original_storage->origin.allocation_generation;
			CHECK(registry->production_import_storage(ref, 1, &original_storage->backing).native_error == ERR_INVALID_DATA);
			--original_storage->origin.allocation_generation;
			auto bad_unit = unit->backing;
			bad_unit.unit.handle_serial = 999;
			CHECK(registry->production_import_unit(ref, 0, &bad_unit).native_error == ERR_DOES_NOT_EXIST);
			bad_unit = unit->backing;
			++bad_unit.pipeline_serial;
			CHECK(registry->production_import_unit(ref, 0, &bad_unit).native_error == ERR_INVALID_DATA);
			++unit->origin.preparation_generation;
			CHECK(registry->production_import_unit(ref, 0, &unit->backing).native_error == ERR_INVALID_DATA);
			--unit->origin.preparation_generation;
			CHECK(slots[i][0].uses[0].physical == nullptr);
			CHECK(slots[i][0].uses[1].physical == nullptr);
			CHECK(slots[i][1].uses[0].physical == nullptr);
			REQUIRE(registry->production_import_storage(ref, 1, &original_storage->backing).native_error == OK);
			REQUIRE(registry->production_import_unit(ref, 0, &unit->backing).native_error == OK);
			auto *storage_head = storage.production_uses;
			auto *shader_head = unit->shader.production_uses;
			auto *pipeline_head = unit->pipeline.production_uses;
			CHECK(registry->production_import_storage(ref, 1, &original_storage->backing).native_error == OK);
			CHECK(registry->production_import_unit(ref, 0, &unit->backing).native_error == OK);
			CHECK(storage.production_uses == storage_head);
			CHECK(unit->shader.production_uses == shader_head);
			CHECK(unit->pipeline.production_uses == pipeline_head);
		}
		CHECK(slots[1][1].uses[0].next == &slots[0][1].uses[0]);
		CHECK(slots[1][0].uses[0].next == &slots[0][0].uses[0]);
		CHECK(slots[1][0].uses[1].next == &slots[0][0].uses[1]);
		locker.start(lock_rd, &lock_state);
		lock_state.locked.wait();
		const auto set_busy = call(0, child->backing, binding);
		lock_state.release.post();
		locker.wait_to_finish();
		CHECK(set_busy == ERR_BUSY);
		auto &set_decl = slots[0][2].declaration;
		set_decl.original_slot = 0;
		CHECK(call(0, child->backing, binding) == ERR_INVALID_DATA);
		set_decl.original_slot = UINT32_MAX;
		++set_decl.original_part;
		CHECK(call(0, child->backing, binding) == ERR_INVALID_DATA);
		--set_decl.original_part;
		++set_decl.original_generation;
		CHECK(call(0, child->backing, binding) == ERR_INVALID_DATA);
		--set_decl.original_generation;
		CHECK(child->physical.production_uses == nullptr);
		CHECK_FALSE(slots[0][2].imported);
		requests[0].capacity.resource_uses = binding_count;
		CHECK(call(0, child->backing, binding) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(child->physical.production_uses == nullptr);
		CHECK_FALSE(slots[0][2].imported);
		requests[0].capacity.resource_uses = 4;
		auto changed = binding;
		changed.bytes = 60;
		CHECK(call(0, child->backing, changed) == ERR_INVALID_DATA);
		CHECK(child->physical.production_uses == nullptr);
		Physical extra;
		auto *actual_set = rd.uniform_set_owner.get_or_null(set_rid);
		actual_set->qualification_dependencies.push_back(&extra);
		CHECK(call(0, child->backing, binding) != OK);
		CHECK(child->physical.production_uses == nullptr);
		actual_set->qualification_dependencies.remove_at(2);
		auto &original_use = slots[0][1].uses[0];
		const auto original_request_serial = original_use.request_serial;
		original_use.request_serial = requests[1].serial;
		CHECK(call(0, child->backing, binding) == ERR_INVALID_DATA);
		CHECK(child->physical.production_uses == nullptr);
		original_use.request_serial = original_request_serial;
		if (unsorted_original) {
			child->bindings[0].binding = 2;
			CHECK(call(0, child->backing, binding) == ERR_INVALID_DATA);
			CHECK(child->physical.production_uses == nullptr);
			child->bindings[0].binding = 1;
		}
		REQUIRE(call(0, child->backing, binding) == OK);
		CHECK(slots[0][2].primary == set_rid);
		CHECK(slots[0][2].use_count == 1);
		CHECK(requests[0].binding_used == binding_count);
		CHECK(uses[0][0].declaration.byte_size == 64);
		CHECK(uses[0][1].binding.bytes == 64);
		CHECK(call(0, child->backing, binding) == OK);
		CHECK(requests[0].binding_used == binding_count);
		REQUIRE(call(1, child->backing, binding) == OK);
		CHECK(child->physical.production_uses == &slots[1][2].uses[0]);
		CHECK(slots[1][2].uses[0].next == &slots[0][2].uses[0]);
		CHECK_FALSE(child->physical.use_closed.load());
		auto wrong_ref = child->backing;
		wrong_ref.resource.handle_serial = 999;
		CHECK(call(0, wrong_ref, binding) == ERR_DOES_NOT_EXIST);
		wrong_ref = child->backing;
		++wrong_ref.object_serial;
		CHECK(call(0, wrong_ref, binding) == ERR_INVALID_DATA);
		CHECK(slots[0][2].primary == set_rid);
		CHECK(rd.uniform_set_owner.owns(set_rid));
		CHECK(rd.storage_buffer_owner.owns(buffer_rid));
		CHECK(rd.compute_pipeline_owner.owns(pipeline_rid));
		CHECK(rd.shader_owner.owns(shader_rid));
		// A new request cannot borrow a child that has closed future uses.
		const auto saved_binding_used = requests[1].binding_used;
		requests[1].binding_used = requests[1].capacity.resource_uses;
		CHECK(registry->production_unpin_resources_locked(requests[1]) == ERR_INVALID_DATA);
		CHECK(child->physical.production_uses == &slots[1][2].uses[0]);
		CHECK(slots[1][2].imported);
		requests[1].binding_used = saved_binding_used;
		REQUIRE(registry->production_unpin_resources_locked(requests[1]) == OK);
		CHECK(requests[1].binding_used == 0);
		CHECK(slots[1][2].first_binding == UINT32_MAX);
		CHECK(slots[1][2].binding_count == 0);
		CHECK(slots[1][2].imported_unit_slot == UINT32_MAX);
		CHECK(slots[1][2].shader_set == UINT32_MAX);
		CHECK(uses[1][1].binding.bytes == 0);
		CHECK(uses[1][0].declaration.byte_size == 64);
		CHECK(uses[1][1].request == &requests[1]);
		CHECK(registry->production_unpin_resources_locked(requests[1]) == OK);
		CHECK(requests[0].binding_used == binding_count);
		CHECK(slots[0][2].imported);
		CHECK(uses[0][1].binding.bytes == 64);
		CHECK(child->physical.production_uses == &slots[0][2].uses[0]);
		CHECK(storage.production_uses == &slots[0][1].uses[0]);
		CHECK(unit->shader.production_uses == &slots[0][0].uses[0]);
		CHECK(unit->pipeline.production_uses == &slots[0][0].uses[1]);
		REQUIRE(registry->production_import_unit({ 71, requests[1].serial }, 0, &unit->backing).native_error == OK);
		REQUIRE(registry->production_import_storage({ 71, requests[1].serial }, 1, &original_storage->backing).native_error == OK);
		child->physical.use_closed.store(true);
		CHECK(call(1, child->backing, binding) == ERR_UNAVAILABLE);
		CHECK_FALSE(slots[1][2].imported);
		CHECK(child->physical.production_uses == &slots[0][2].uses[0]);
		child->physical.use_closed.store(false);
		REQUIRE(registry->production_unpin_resources_locked(requests[1]) == OK);
		requests[0].effects_pending = true;
		CHECK(registry->production_unpin_resources_locked(requests[0]) == ERR_BUSY);
		CHECK(child->physical.production_uses == &slots[0][2].uses[0]);
		requests[0].effects_pending = false;
		REQUIRE(registry->production_unpin_resources_locked(requests[0]) == OK);
		CHECK(registry->production_unpin_resources_locked(requests[0]) == OK);
		CHECK(requests[0].binding_used == 0);
		CHECK(uses[0][0].declaration.byte_size == 64);
		CHECK(uses[0][1].binding.bytes == 0);
		CHECK(child->physical.production_uses == nullptr);
		CHECK(storage.production_uses == nullptr);
		CHECK(unit->shader.production_uses == nullptr);
		CHECK(unit->pipeline.production_uses == nullptr);
		// RID owner removal is test cleanup only: no real driver allocation,
		// free_rid call or native free is involved in the imported-set handler.
		rd.uniform_set_owner.free(set_rid);
		rd.compute_pipeline_owner.free(pipeline_rid);
		rd.shader_owner.free(shader_rid);
		rd.storage_buffer_owner.free(buffer_rid);
		registry->production_attachments[0] = nullptr;
		registry->qualification_bridges[0] = nullptr;
		bridge->sets[0] = nullptr;
		bridge->units[0] = nullptr;
		bridge->storage[0] = nullptr;
		memdelete(original_storage);
		memdelete(child);
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
		rd.driver = nullptr;
		RenderingDevice::singleton = previous;
	}
};

TEST_CASE("[ProductionResourceUse] real set import retains original child across requests without freeing") {
	TestProductionResourceUseAccess::import_set();
}

TEST_CASE("[ProductionResourceUse] retained set bindings resolve by key regardless of original order") {
	TestProductionResourceUseAccess::import_set(true);
}
