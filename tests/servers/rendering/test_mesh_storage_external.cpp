/**************************************************************************/
/*  test_mesh_storage_external.cpp                                        */
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

#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "tests/test_macros.h"
#include "tests/servers/rendering/test_native_material_mesh_fixture.h"

#include <cstring>
#include <type_traits>
#ifdef THREADS_ENABLED
#include "core/os/semaphore.h"
#include <thread>
#endif

TEST_FORCE_LINK(test_mesh_storage_external)

TEST_CASE("[B4][K05] external surface descriptor requires distinct generations") {
	RendererRD::MeshStorage::ExternalSurfaceDescriptor descriptor;
	CHECK(descriptor.vertex_count == 0);
	CHECK(descriptor.index_count == 0);
	CHECK(descriptor.generation == 0);
	CHECK(descriptor.current_allocation_generation == 0);
	CHECK(descriptor.previous_allocation_generation == 0);
	CHECK(descriptor.index_array.is_null());
	descriptor.current_allocation_generation = 7;
	descriptor.previous_allocation_generation = 8;
	CHECK(descriptor.current_allocation_generation != descriptor.previous_allocation_generation);
}

#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"

class Indexed144FormatDriver : public TransferTestDriverBase {
public:
	uint8_t memory[32][512] = {};
	bool live[32] = {};
	uint32_t allocations = 0, frees = 0;
	BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		CHECK(bytes <= 512);
		CHECK(allocations + 1 < 32);
		if (bytes > 512 || allocations + 1 >= 32) {
			return {};
		}
		const uint32_t id = ++allocations;
		live[id] = true;
		for (uint32_t i = 0; i < bytes; ++i) {
			memory[id][i] = uint8_t(id * 17 + i);
		}
		return BufferID(id);
	}
	void buffer_free(BufferID id) override {
		CHECK((id.id > 0 && id.id < 32 && live[id.id]));
		if (!id.id || id.id >= 32 || !live[id.id]) {
			return;
		}
		live[id.id] = false;
		++frees;
	}
	void set_object_name(ObjectType, ID, const String &) override {}
	bool fail_format = false;
	uint32_t formats = 0;
	Vector<VertexAttribute> last_attributes;
	VertexFormatID vertex_format_create(Span<VertexAttribute> p_attributes, const VertexAttributeBindingsMap &) override {
		if (fail_format) {
			return VertexFormatID();
		}
		last_attributes.clear();
		for (const auto &attribute : p_attributes) {
			last_attributes.push_back(attribute);
		}
		return VertexFormatID(++formats);
	}
};

class FallbackTransferDriver : public Indexed144FormatDriver {
public:
	bool completed = false;
	uint32_t polls = 0;
	uint32_t recycles = 0;

	uint8_t *buffer_map(BufferID p_buffer) override { return memory[p_buffer.id]; }
	void buffer_unmap(BufferID) override {}
	bool command_buffer_begin(CommandBufferID) override { return true; }
	void command_buffer_end(CommandBufferID) override {}
	void command_copy_buffer(CommandBufferID, BufferID p_src, BufferID p_dst, VectorView<BufferCopyRegion> p_regions) override {
		REQUIRE(p_regions.size() == 1);
		REQUIRE((p_src.id > 0 && p_src.id < 32 && live[p_src.id]));
		REQUIRE((p_dst.id > 0 && p_dst.id < 32 && live[p_dst.id]));
		const BufferCopyRegion &region = p_regions[0];
		REQUIRE(region.src_offset + region.size <= sizeof(memory[0]));
		REQUIRE(region.dst_offset + region.size <= sizeof(memory[0]));
		std::memcpy(memory[p_dst.id] + region.dst_offset, memory[p_src.id] + region.src_offset, region.size);
	}
	uint32_t record_native_call_bound(uint32_t p_slots) const override { return p_slots; }
	Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID> p_commands, VectorView<SemaphoreID>, FenceID p_fence, VectorView<SwapChainID>) override {
		CHECK(p_commands.size() == 1);
		CHECK(p_fence == FenceID(91));
		uint32_t index = 0;
		if (!claim_native_fact(index)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		NativeCallFact fact;
		fact.serial = 17;
		fact.completion_serial = 73;
		fact.issue = IssueFact::KnownAccepted;
		fact.completion = CompletionCapability::Fence;
		finish_native_fact(index, fact, 73);
		return OK;
	}
	FencePollResult fence_poll(FenceID p_fence, uint64_t p_expected) override {
		CHECK(p_fence == FenceID(91));
		CHECK(p_expected == 73);
		++polls;
		FencePollResult result;
		result.completed = completed;
		result.completed_serial = completed ? 73 : 0;
		return result;
	}
	Error fence_recycle(FenceID p_fence, uint64_t p_completed) override {
		CHECK(p_fence == FenceID(91));
		CHECK(p_completed == 73);
		++recycles;
		return OK;
	}
	bool command_pool_reset(CommandPoolID) override { return true; }
	void command_pool_free(CommandPoolID) override {}
	void fence_free(FenceID) override {}
};

class TestIndexed144DeviceAccess {
public:
	static void attach(RenderingDevice &p_device, RenderingDeviceDriver *p_driver) { p_device.driver = p_driver; }
	static RID buffer(RenderingDevice &p_device, uint64_t p_id) {
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(p_id);
		buffer.size = 144 * 3;
		return p_device.vertex_buffer_owner.make_rid(buffer);
	}
	static uint64_t binding(RenderingDevice &p_device, RID p_array, uint32_t p_binding) { return p_device.vertex_array_owner.get_or_null(p_array)->buffers[p_binding].id; }
	static void size(RenderingDevice &p_device, RID p_buffer, uint32_t p_size) { p_device.vertex_buffer_owner.get_or_null(p_buffer)->size = p_size; }
	static uint32_t holds(RenderingDevice &p_device, RID p_buffer) {
		auto *buffer = p_device.vertex_buffer_owner.get_or_null(p_buffer);
		CHECK(buffer != nullptr);
		return buffer ? buffer->native_immutable_holds : 0;
	}
	static uint32_t device_holds(RenderingDevice &p_device) { return p_device.native_immutable_hold_count; }
	static void external_access(RenderingDevice &p_device, RID p_buffer, bool p_value) {
		auto *buffer = p_device.vertex_buffer_owner.get_or_null(p_buffer);
		CHECK(buffer != nullptr);
		if (buffer) {
			buffer->native_external_access = p_value;
		}
	}

	static uint32_t arrays(RenderingDevice &p_device) { return p_device.vertex_array_owner.get_owned_list().size(); }
	static bool owns(RenderingDevice &p_device, RID p_buffer) { return p_device.vertex_buffer_owner.owns(p_buffer); }
	static void begin_native(RenderingDevice &device, Indexed144FormatDriver &driver) {
		device.driver = &driver;
		device.is_main_instance = true;
		device.frames.resize(1);
	}
	static void end_native(RenderingDevice &device) {
		device._free_pending_resources(0);
		CHECK(device.vertex_buffer_owner.get_rid_count() == 0);
		CHECK(device.index_buffer_owner.get_rid_count() == 0);
		CHECK(device.vertex_array_owner.get_rid_count() == 0);
		CHECK(device.index_array_owner.get_rid_count() == 0);
		CHECK(device.native_immutable_hold_count == 0);
		CHECK(device.dependency_map.is_empty());
		CHECK(device.reverse_dependency_map.is_empty());
		device.vertex_formats.clear();
		device.vertex_format_cache.clear();
		device.frames.clear();
		device.driver = nullptr;
		device.native_finalization_returned = true;
	}
	enum class TransferState { DETACHED, PENDING, PROCESSED };

	static void begin_fallback_transfers(RenderingDevice &device, FallbackTransferDriver &driver) {
		begin_native(device, driver);
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker->command_pool = RenderingDeviceDriver::CommandPoolID(61);
		worker->command_buffer = RenderingDeviceDriver::CommandBufferID(62);
		worker->command_fence = RenderingDeviceDriver::FenceID(91);
		worker->staging_buffer_size_allocated = 128;
		worker->staging_buffer = driver.buffer_create(128, RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_FROM_BIT, RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_CPU, 0);
		device.transfer_worker_pool.push_back(worker);
		device.transfer_worker_pool_available_list.push_back(0);
		device.transfer_worker_operation_used_by_draw.push_back(0);
		device.transfer_worker_pool_size = 1;
	}
	static bool poll_fallback_transfers(RenderingDevice &device) {
		MutexLock rd_lock(device._thread_safe_);
		MutexLock pool_lock(device.transfer_worker_pool_mutex);
		auto *worker = device.transfer_worker_pool[0];
		MutexLock worker_lock(worker->thread_mutex);
		if (worker->recording) {
			device._end_transfer_worker(worker);
			device._submit_transfer_worker(worker);
		}
		return device._wait_for_transfer_worker(worker, false);
	}
	static TransferState fallback_transfer_state(RenderingDevice &device, RID rid) {
		MutexLock rd_lock(device._thread_safe_);
		const auto *buffer = device.vertex_buffer_owner.get_or_null(rid);
		REQUIRE(buffer != nullptr);
		if (buffer->transfer_worker_index < 0) {
			return TransferState::DETACHED;
		}
		MutexLock pool_lock(device.transfer_worker_pool_mutex);
		REQUIRE(uint32_t(buffer->transfer_worker_index) < device.transfer_worker_pool_size);
		auto *worker = device.transfer_worker_pool[buffer->transfer_worker_index];
		MutexLock worker_lock(worker->thread_mutex);
		MutexLock operations_lock(worker->operations_mutex);
		CHECK_FALSE(worker->poisoned);
		CHECK(buffer->transfer_worker_operation > 0);
		CHECK(buffer->transfer_worker_operation <= worker->operations_counter);
		return buffer->transfer_worker_operation <= worker->operations_processed ? TransferState::PROCESSED : TransferState::PENDING;
	}
	static uint32_t check_vertex_array_transfer(RenderingDevice &device, RID rid) {
		MutexLock rd_lock(device._thread_safe_);
		auto *array = device.vertex_array_owner.get_or_null(rid);
		REQUIRE(array != nullptr);
		CHECK(device._check_transfer_worker_vertex_array(array) == OK);
		return array->transfer_worker_indices.size();
	}
	static void check_vertex_buffer_transfer(RenderingDevice &device, RID rid) {
		MutexLock rd_lock(device._thread_safe_);
		auto *buffer = device.vertex_buffer_owner.get_or_null(rid);
		REQUIRE(buffer != nullptr);
		CHECK(device._check_transfer_worker_buffer(buffer) == OK);
	}
	static uint64_t fallback_worker_draw_use(RenderingDevice &device) {
		MutexLock rd_lock(device._thread_safe_);
		return device.transfer_worker_operation_used_by_draw[0];
	}
#ifdef THREADS_ENABLED
	template <typename Callable>
	static void with_fallback_worker_locked(RenderingDevice &device, Callable &&operation) {
		Semaphore held;
		Semaphore release;
		std::thread blocker([&]() {
			MutexLock worker_lock(device.transfer_worker_pool[0]->thread_mutex);
			held.post();
			release.wait();
		});
		held.wait();
		operation();
		release.post();
		blocker.join();
	}
#endif
	static void end_fallback_transfers(RenderingDevice &device) {
		device._free_transfer_workers();
		CHECK(device.transfer_worker_pool_size == 0);
		end_native(device);
	}
	static uint8_t fetch(RenderingDevice &device, Indexed144FormatDriver &driver, RID array, uint32_t binding) {
		const auto *original = device.vertex_array_owner.get_or_null(array);
		CHECK(original != nullptr);
		if (!original || binding >= uint32_t(original->buffers.size())) {
			return 0;
		}
		const uint64_t id = original->buffers[binding].id;
		CHECK((id > 0 && id < 32 && driver.live[id]));
		return id && id < 32 && driver.live[id] ? driver.memory[id][original->offsets[binding]] : 0;
	}
	static void dispose(RenderingDevice &device) { device._free_pending_resources(0); }
	static void preflight_only() {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		begin_native(device, driver);
		const RID buffer = device.vertex_buffer_create(40, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		auto *actual = device.vertex_buffer_owner.get_or_null(buffer);
		CHECK(actual != nullptr);
		if (!actual) {
			end_native(device);
			return;
		}
		auto check_no_holds = [&]() {
			CHECK(actual->native_immutable_holds == 0);
			CHECK(device.native_immutable_hold_count == 0);
		};
		RenderingDevice::VertexAttribute a;
		a.location = 0;
		a.format = RenderingDevice::DATA_FORMAT_R32G32B32_SFLOAT;
		a.stride = 12;
		RenderingDevice::NativeVertexArrayPreflight result;
		const uint32_t formats = driver.formats, before_arrays = arrays(device);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, { 4 }, result) == OK);
		CHECK(result.used_buffers.size() == 1);
		CHECK(result.attributes.size() == 1);
		if (!result.attributes.is_empty()) {
			CHECK(result.attributes[0].binding == 0);
		}
		check_no_holds();
		ERR_PRINT_OFF;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, { 5 }, result) == ERR_INVALID_PARAMETER);
		CHECK(result.used_buffers.is_empty());
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, { UINT64_MAX }, result) == ERR_INVALID_PARAMETER);
		CHECK(device._native_vertex_array_preflight(3, { a, a }, { buffer, buffer }, {}, result) == ERR_INVALID_DATA);
		RenderingDevice::VertexAttribute b = a;
		b.location = 1;
		b.binding = 0;
		CHECK(device._native_vertex_array_preflight(3, { a, b }, { buffer }, {}, result) == ERR_INVALID_DATA);
		a.binding = 0;
		CHECK(device._native_vertex_array_preflight(3, { a, b }, { buffer, RID() }, {}, result) == OK);
		CHECK(result.used_buffers.size() == 1); // Unused supplied null is not a graph root.
		b.stride = 16;
		CHECK(device._native_vertex_array_preflight(3, { a, b }, { buffer }, {}, result) == ERR_INVALID_DATA);
		a.frequency = RenderingDevice::VERTEX_FREQUENCY_INSTANCE;
		a.stride = 0;
		CHECK(device._native_vertex_array_preflight(0, { a }, { buffer }, {}, result) == ERR_INVALID_PARAMETER);
		CHECK(device._native_vertex_array_preflight(1, { a }, { buffer }, {}, result) == OK);
		CHECK(result.max_instances_allowed == UINT32_MAX);
		// Ordinary point-size preparation retains its effective zero-count support.
		RenderingDevice::VertexArray zero_count_array;
		HashSet<RID> zero_count_buffers;
		CHECK(device._vertex_array_prepare_locked(0, { a }, { buffer }, {}, zero_count_array, zero_count_buffers) == OK);
		CHECK(zero_count_array.max_instances_allowed == UINT32_MAX);
		a.stride = 12;
		CHECK(device._native_vertex_array_preflight(1, { a }, { buffer }, {}, result) == OK);
		CHECK(result.max_instances_allowed == 3);
		a.frequency = RenderingDevice::VERTEX_FREQUENCY_VERTEX;
		CHECK(device._native_vertex_array_preflight(0, { a }, { buffer }, {}, result) == ERR_INVALID_PARAMETER);
		check_no_holds(); // Assert before fault injection can overwrite a leaked increment.
		device.draw_list.active = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_BUSY);
		device.draw_list.active = false;
		device.compute_list.active = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_BUSY);
		device.compute_list.active = false;
		device.raytracing_list.active = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_BUSY);
		device.raytracing_list.active = false;
		device.production_finalization_started.store(true, std::memory_order_release);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_BUSY);
		device.production_finalization_started.store(false, std::memory_order_release);
		device.native_immutable_device_escaped = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		device.native_immutable_device_escaped = false;
		check_no_holds();
		const auto usage = actual->usage;
		actual->usage.set_flag(RenderingDeviceDriver::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		actual->usage = usage;
		check_no_holds();
		actual->usage.set_flag(RenderingDeviceDriver::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		actual->usage = usage;
		check_no_holds();
		// These direct counters are restored before disposal; no fake RD owner.
		actual->native_immutable_holds = UINT32_MAX;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(actual->native_immutable_holds == UINT32_MAX);
		actual->native_immutable_holds = 0;
		check_no_holds();
		actual->native_external_access = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		actual->native_external_access = false;
		check_no_holds();
		actual->native_immutable_retire_requested = true;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_BUSY);
		actual->native_immutable_retire_requested = false;
		check_no_holds();
		actual->transfer_worker_index = 0;
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		actual->transfer_worker_index = -1;
		check_no_holds();
		const auto driver_id = actual->driver_id;
		actual->driver_id = {};
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_UNAVAILABLE);
		actual->driver_id = driver_id;
		check_no_holds();
		CHECK(device._native_vertex_array_preflight(3, { a }, { RID() }, {}, result) == ERR_DOES_NOT_EXIST);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, { 0, 0 }, result) == ERR_INVALID_PARAMETER);
		a.frequency = static_cast<RenderingDevice::VertexFrequency>(-1);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_INVALID_DATA);
		a.frequency = RenderingDevice::VERTEX_FREQUENCY_VERTEX;
		a.format = static_cast<RenderingDevice::DataFormat>(-1);
		CHECK(device._native_vertex_array_preflight(3, { a }, { buffer }, {}, result) == ERR_INVALID_DATA);
		ERR_PRINT_ON;
		check_no_holds();
		CHECK(driver.formats == formats);
		CHECK(arrays(device) == before_arrays);
		CHECK(device.native_immutable_hold_count == 0);
		CHECK(device.dependency_map.is_empty());
		CHECK(device.reverse_dependency_map.is_empty());
		device.free_rid(buffer);
		dispose(device);
		CHECK(driver.frees == driver.allocations);
		end_native(device);
	}
	static void prospective_hold_increments() {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		begin_native(device, driver);
		const RID a = device.vertex_buffer_create(12, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		const RID b = device.vertex_buffer_create(12, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		using Key = RenderingDevice::NativeProspectiveKey;
		using Plan = RenderingDevice::NativeProspectiveHoldRoots;
		using Symbol = RenderingDevice::NativeProspectiveSymbol;
		Plan plans[3];
		plans[0].roots = { Key{ a, UINT32_MAX }, Key{ a, UINT32_MAX } };
		plans[1].roots = { Key{ a, UINT32_MAX } };
		plans[2].roots = { Key{ b, UINT32_MAX } };
		Symbol future;
		future.dependencies = { a, b };
		Symbol symbols[] = { future };
		const uint32_t formats = driver.formats, before_arrays = arrays(device);
		const uint32_t allocations = driver.allocations, frees = driver.frees;
		auto *actual_a = device.vertex_buffer_owner.get_or_null(a);
		auto *actual_b = device.vertex_buffer_owner.get_or_null(b);
		CHECK(actual_a != nullptr);
		CHECK(actual_b != nullptr);
		if (actual_a && actual_b) {
			MutexLock lock(device._thread_safe_);
			auto check_zero = [&]() {
				CHECK(actual_a->native_immutable_holds == 0);
				CHECK(actual_b->native_immutable_holds == 0);
				CHECK(device.native_immutable_hold_count == 0);
			};
			CHECK(device._native_prospective_hold_increments_locked({ plans, 3 }, {}) == OK);
			check_zero(); // Check before injecting metadata, so leaks cannot be erased.
			actual_a->native_immutable_holds = UINT32_MAX - 1;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 1 }, {}) == OK); // Duplicate roots: one increment.
			CHECK(actual_a->native_immutable_holds == UINT32_MAX - 1);
			CHECK(device._native_prospective_hold_increments_locked({ plans, 2 }, {}) == ERR_PARAMETER_RANGE_ERROR);
			CHECK(actual_a->native_immutable_holds == UINT32_MAX - 1);
			CHECK(actual_b->native_immutable_holds == 0);
			CHECK(device.native_immutable_hold_count == 0);
			actual_a->native_immutable_holds = UINT32_MAX - 2;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 2 }, {}) == OK);
			CHECK(actual_a->native_immutable_holds == UINT32_MAX - 2);
			plans[2].roots = { Key{ RID(), 0 } }; // Future VA adds a third real a pin through its backing edge.
			CHECK(device._native_prospective_hold_increments_locked({ plans, 3 }, { symbols, 1 }) == ERR_PARAMETER_RANGE_ERROR);
			CHECK(actual_a->native_immutable_holds == UINT32_MAX - 2);
			CHECK(actual_b->native_immutable_holds == 0);
			CHECK(device.native_immutable_hold_count == 0);
			actual_a->native_immutable_holds = UINT32_MAX - 3;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 3 }, { symbols, 1 }) == OK);
			CHECK(actual_a->native_immutable_holds == UINT32_MAX - 3);
			CHECK(actual_b->native_immutable_holds == 0);
			CHECK(device.native_immutable_hold_count == 0);
			actual_a->native_immutable_holds = 0;
			check_zero();
			device.native_immutable_hold_count = UINT32_MAX - 1;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 1 }, {}) == OK);
			CHECK(device.native_immutable_hold_count == UINT32_MAX - 1);
			CHECK(device._native_prospective_hold_increments_locked({ plans, 2 }, {}) == ERR_PARAMETER_RANGE_ERROR);
			CHECK(device.native_immutable_hold_count == UINT32_MAX - 1);
			CHECK(actual_a->native_immutable_holds == 0);
			CHECK(actual_b->native_immutable_holds == 0);
			device.native_immutable_hold_count = 0;
			plans[2].limit = 2; // Future VA plus two actual VBs needs three nodes.
			CHECK(device._native_prospective_hold_increments_locked({ plans, 3 }, { symbols, 1 }) == ERR_OUT_OF_MEMORY);
			check_zero();
			plans[2].limit = 3;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 3 }, { symbols, 1 }) == OK);
			check_zero();
			plans[0].limit = 1025;
			CHECK(device._native_prospective_hold_increments_locked({ plans, 1 }, {}) == ERR_INVALID_PARAMETER);
			check_zero();
		}
		CHECK(driver.formats == formats);
		CHECK(arrays(device) == before_arrays);
		CHECK(driver.allocations == allocations);
		CHECK(driver.frees == frees);
		CHECK(device.dependency_map.is_empty());
		CHECK(device.reverse_dependency_map.is_empty());
		device.free_rid(a);
		device.free_rid(b);
		dispose(device);
		CHECK(driver.frees == driver.allocations);
		end_native(device);
	}
	static void prospective_material_graph() {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		begin_native(device, driver);
		using Key = RenderingDevice::NativeProspectiveKey;
		using Symbol = RenderingDevice::NativeProspectiveSymbol;
		using Input = RenderingDevice::NativeProspectiveInput;
		// Only the shader and sampler exist. Future destinations remain symbols:
		// no native allocation, hold, scope, action or factory permission is issued.
		RenderingDevice::Shader shader;
		const RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::Sampler sampler;
		const RID sampler_rid = device.sampler_owner.make_rid(sampler);
		Symbol symbols[5];
		symbols[0].kind = Symbol::FUTURE_UNIFORM_BUFFER;
		symbols[1].kind = Symbol::FUTURE_STORAGE_BUFFER;
		symbols[2].kind = Symbol::FUTURE_TEXTURE;
		symbols[3].kind = Symbol::FUTURE_UNIFORM_SET;
		symbols[3].shader = shader_rid;
		symbols[3].inputs = {
			{ { RID(), 0 }, Input::UNIFORM_BUFFER },
			{ { RID(), 1 }, Input::STORAGE_BUFFER },
			{ { RID(), 2 }, Input::TEXTURE },
			{ { sampler_rid, UINT32_MAX }, Input::SAMPLER }
		};
		symbols[4].kind = Symbol::FUTURE_FRAMEBUFFER;
		symbols[4].inputs = { { { RID(), 2 }, Input::TEXTURE } };
		const Key roots[] = { { RID(), 3 }, { RID(), 4 }, { RID(), 3 } };
		Vector<Key> nodes;
		{
			MutexLock lock(device._thread_safe_);
			CHECK(device._native_prospective_closure_keys_locked({ roots, 3 }, { symbols, 5 }, 7, false, nodes) == OK);
			CHECK(nodes.size() == 7); // Five destinations, original shader and sampler.
			for (uint32_t ordinal = 0; ordinal < 5; ++ordinal) {
				uint32_t occurrences = 0;
				for (const auto &key : nodes) {
					occurrences += key.actual.is_null() && key.future == ordinal;
				}
				CHECK(occurrences == 1);
			}
			const auto good = nodes;
			const auto unchanged = [&]() {
				if (nodes.size() != good.size()) {
					return false;
				}
				for (int i = 0; i < nodes.size(); ++i) {
					if (!(nodes[i] == good[i])) {
						return false;
					}
				}
				return true;
			};
			auto refused = [&](Error expected) {
				CHECK(device._native_prospective_closure_keys_locked({ roots, 3 }, { symbols, 5 }, 7, false, nodes) == expected);
				CHECK(unchanged());
			};
			CHECK(device._native_prospective_closure_keys_locked({ roots, 3 }, { symbols, 5 }, 6, false, nodes) == ERR_OUT_OF_MEMORY);
			CHECK(unchanged());
			// A resource root alone cannot pull its future users in a one-way hold.
			const Key texture{ RID(), 2 };
			CHECK(device._native_prospective_closure_keys_locked({ &texture, 1 }, { symbols, 5 }, 7, false, nodes) == OK);
			CHECK(nodes.size() == 1);
			CHECK(device._native_prospective_closure_keys_locked({ &texture, 1 }, { symbols, 5 }, 7, true, nodes) == OK);
			CHECK(nodes.size() == 7);
			// This one-way closure contains only the future framebuffer and its
			// future texture. Preflight skips both symbolic counters: OK does not
			// create a hold or make the future framebuffer hold-eligible.
			const Key future_framebuffer{ RID(), 4 };
			CHECK(device._native_prospective_closure_keys_locked({ &future_framebuffer, 1 }, { symbols, 5 }, 2, false, nodes) == OK);
			CHECK(nodes.size() == 2);
			RenderingDevice::NativeProspectiveHoldRoots future_framebuffer_hold;
			future_framebuffer_hold.roots = { future_framebuffer };
			future_framebuffer_hold.limit = 2;
			CHECK(device._native_prospective_hold_increments_locked({ &future_framebuffer_hold, 1 }, { symbols, 5 }) == OK);
			CHECK(device.native_immutable_hold_count == 0);
			CHECK(driver.allocations == 0);
			nodes = good;
			RenderingDevice::Texture actual_texture;
			const RID texture_rid = device.texture_owner.make_rid(actual_texture);
			RenderingDevice::Framebuffer actual_framebuffer;
			const RID framebuffer_rid = device.framebuffer_owner.make_rid(actual_framebuffer);
			device._add_dependency(framebuffer_rid, texture_rid);
			const Key actual_root{ framebuffer_rid, UINT32_MAX };
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, {}, 2, false, nodes) == OK);
			CHECK(nodes.size() == 2);
			RenderingDevice::NativeProspectiveHoldRoots framebuffer_hold;
			framebuffer_hold.roots = { actual_root };
			framebuffer_hold.limit = 2;
			CHECK(device._native_prospective_hold_increments_locked({ &framebuffer_hold, 1 }, {}) == ERR_DOES_NOT_EXIST);
			CHECK(device.native_immutable_hold_count == 0); // Counting is not hold eligibility.
			CHECK(device._native_immutable_counter(framebuffer_rid) == nullptr); // No FB hold support was added.
			CHECK(device._free_dependencies(framebuffer_rid, lock));
			device.framebuffer_owner.free(framebuffer_rid);
			// Return the actual parent edge directory after its child was unlinked.
			CHECK(device._free_dependencies(texture_rid, lock));
			device.texture_owner.free(texture_rid);
			nodes = good;
			symbols[3].inputs.push_back(symbols[3].inputs[0]);
			refused(ERR_INVALID_DATA);
			symbols[3].inputs.resize(4);
			symbols[3].inputs.write[0].key.future = 99;
			refused(ERR_INVALID_PARAMETER);
			symbols[3].inputs.write[0].key = { shader_rid, 0 };
			refused(ERR_INVALID_PARAMETER); // Actual and future identity cannot coexist.
			symbols[3].inputs.write[0].key = { RID(), 1 };
			refused(ERR_INVALID_DATA); // Storage cannot satisfy a UBO input.
			symbols[3].inputs.write[0].key = { RID(), 0 };
			symbols[4].inputs.write[0].kind = Input::STORAGE_BUFFER;
			refused(ERR_INVALID_DATA); // Framebuffer attachments must be textures.
			symbols[4].inputs.write[0].kind = Input::TEXTURE;
			symbols[2].inputs = { { { RID(), 3 }, Input::TEXTURE } };
			refused(ERR_CYCLIC_LINK);
			symbols[2].inputs.clear();
			symbols[2].kind = Symbol::Kind(UINT32_MAX);
			refused(ERR_INVALID_PARAMETER);
			symbols[2].kind = Symbol::FUTURE_TEXTURE;
			symbols[3].inputs.write[0].kind = Input::Kind(UINT32_MAX);
			refused(ERR_INVALID_PARAMETER);
			symbols[3].inputs.write[0].kind = Input::UNIFORM_BUFFER;
			// Even unreachable malformed entries invalidate the complete owner roster.
			symbols[3].shader = sampler_rid;
			const Key shader_root{ shader_rid, UINT32_MAX };
			CHECK(device._native_prospective_closure_keys_locked({ &shader_root, 1 }, { symbols, 5 }, 7, false, nodes) == ERR_DOES_NOT_EXIST);
			CHECK(unchanged());
			symbols[3].shader = sampler_rid;
			refused(ERR_DOES_NOT_EXIST);
			symbols[3].shader = shader_rid;
			const RID stale = sampler_rid;
			device.sampler_owner.free(sampler_rid);
			refused(ERR_DOES_NOT_EXIST);
			CHECK_FALSE(device.sampler_owner.owns(stale));
		}
		CHECK(driver.allocations == 0);
		CHECK(device.native_immutable_hold_count == 0);
		CHECK(device.dependency_map.is_empty());
		CHECK(device.reverse_dependency_map.is_empty());
		device.shader_owner.free(shader_rid);
		end_native(device);
	}
	static void prospective_closure_only() {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		begin_native(device, driver);
		const RID a = device.vertex_buffer_create(12, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		const RID b = device.vertex_buffer_create(12, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		const RID c = device.vertex_buffer_create(12, {}, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT);
		RenderingDevice::VertexAttribute first;
		first.location = 0;
		first.binding = 0;
		first.stride = 4;
		first.format = RenderingDevice::DATA_FORMAT_R32_SFLOAT;
		auto second = first;
		second.location = 1;
		second.binding = 1;
		const auto format = device.vertex_format_create({ first, second });
		const RID old_array = device.vertex_array_create(3, format, { a, b });
		CHECK(old_array.is_valid());
		using Key = RenderingDevice::NativeProspectiveKey;
		using Symbol = RenderingDevice::NativeProspectiveSymbol;
		Symbol future;
		future.kind = Symbol::FUTURE_VERTEX_ARRAY;
		future.dependencies = { b, c };
		Symbol symbols[] = { future };
		Key actual_root{ a, UINT32_MAX }, future_root{ RID(), 0 };
		Vector<Key> nodes;
		const uint32_t before_formats = driver.formats, before_arrays = arrays(device);
		const uint32_t before_forward_edges = device.dependency_map.size(), before_reverse_edges = device.reverse_dependency_map.size();
		{
			MutexLock lock(device._thread_safe_);
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, { symbols, 1 }, 5, false, nodes) == OK);
			CHECK(nodes.size() == 1); // Reverse-only acquisition does not follow dependents.
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, { symbols, 1 }, 5, true, nodes) == OK);
			CHECK(nodes.size() == 5); // a, actual VA, b, future VA, c.
			bool saw_c = false, saw_future = false;
			for (const Key &key : nodes) {
				saw_c |= key.actual == c;
				saw_future |= key.future == 0 && key.actual.is_null();
			}
			CHECK(saw_c);
			CHECK(saw_future);
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, { symbols, 1 }, 4, true, nodes) == ERR_OUT_OF_MEMORY);
			CHECK(nodes.size() == 5); // Failure preserves the last complete observation.
			CHECK(device._native_prospective_closure_keys_locked({ &future_root, 1 }, { symbols, 1 }, 3, false, nodes) == OK);
			CHECK(nodes.size() == 3); // future VA, b, c; no incoming edge to old VA.

			// Model a true dependent framebuffer with no immutable counter.
			RenderingDevice::Framebuffer framebuffer{};
			const RID framebuffer_rid = device.framebuffer_owner.make_rid(framebuffer);
			device._add_dependency(framebuffer_rid, a);
			Symbol future_child;
			future_child.dependencies = { a, c };
			const Symbol directed_symbols[] = { future, future_child };
			const auto material_disposal = RenderingDevice::NativeClosureMode::MATERIAL_DISPOSAL;
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, { directed_symbols, 2 }, 4, material_disposal, nodes) == OK);
			CHECK(nodes.size() == 4);
			CHECK(nodes.has(actual_root));
			CHECK(nodes.has(Key{ old_array, UINT32_MAX }));
			CHECK(nodes.has(Key{ framebuffer_rid, UINT32_MAX }));
			CHECK(nodes.has(Key{ RID(), 1 }));
			CHECK_FALSE(nodes.has(Key{ b, UINT32_MAX }));
			CHECK_FALSE(nodes.has(Key{ c, UINT32_MAX }));
			CHECK_FALSE(nodes.has(future_root));
			CHECK(device._native_prospective_closure_keys_locked({ &actual_root, 1 }, { directed_symbols, 2 }, 3, material_disposal, nodes) == ERR_OUT_OF_MEMORY);
			CHECK(nodes.size() == 4); // Keep the last complete observation.

			const Key held_array{ old_array, UINT32_MAX };
			CHECK(device._native_prospective_closure_keys_locked({ &held_array, 1 }, { directed_symbols, 2 }, 6, material_disposal, nodes) == OK);
			CHECK(nodes.size() == 6);
			CHECK(nodes.has(Key{ a, UINT32_MAX }));
			CHECK(nodes.has(Key{ b, UINT32_MAX })); // A real held input stays retained.
			CHECK_FALSE(nodes.has(Key{ c, UINT32_MAX })); // Observers' other inputs stay outside.
			CHECK(device._free_dependencies(framebuffer_rid, lock));
			device.framebuffer_owner.free(framebuffer_rid);
		}
		CHECK(driver.formats == before_formats);
		CHECK(arrays(device) == before_arrays);
		CHECK(device.native_immutable_hold_count == 0);
		CHECK(device.dependency_map.size() == before_forward_edges);
		CHECK(device.reverse_dependency_map.size() == before_reverse_edges);
		for (RID buffer : { a, b, c }) {
			const auto *actual = device.vertex_buffer_owner.get_or_null(buffer);
			CHECK(actual != nullptr);
			if (actual) {
				CHECK(actual->native_immutable_holds == 0);
			}
		}
		device.free_rid(old_array);
		for (RID buffer : { a, b, c }) {
			device.free_rid(buffer);
		}
		dispose(device);
		CHECK(driver.frees == driver.allocations);
		end_native(device);
	}
	static void detach(RenderingDevice &p_device) {
		for (RID rid : p_device.vertex_array_owner.get_owned_list()) {
			p_device.free_rid(rid);
		}
		for (RID rid : p_device.vertex_buffer_owner.get_owned_list()) {
			MutexLock rd_lock(p_device._thread_safe_);
			p_device._free_dependencies(rid, rd_lock);
			p_device.vertex_buffer_owner.free(rid);
		}
		// These format IDs are test-double metadata, not driver allocations.
		p_device.vertex_formats.clear();
		p_device.vertex_format_cache.clear();
		CHECK(p_device.dependency_map.is_empty());
		CHECK(p_device.reverse_dependency_map.is_empty());
		p_device.driver = nullptr;
	}
};

namespace RendererRD {
class TestMeshStorageExternalAccess {
public:
	using Hold = MeshStorage::NativeSurfaceHold;
	static_assert(!std::is_copy_constructible_v<Hold>);
	static_assert(!std::is_copy_assignable_v<Hold>);
	static_assert(std::is_move_constructible_v<Hold>);
	static_assert(!std::is_copy_constructible_v<RD::NativeImmutableHold>);
	static MeshStorage::Mesh::Surface *surface(RenderingDevice &device, bool external, bool quad = false, bool cached = true) {
		auto *surface = memnew(MeshStorage::Mesh::Surface);
		surface->external = external;
		surface->vertex_count = quad ? 4 : 3;
		surface->primitive = RSE::PRIMITIVE_TRIANGLES;
		surface->format = RSE::ARRAY_FORMAT_VERTEX;
		surface->vertex_buffer_size = external ? 144 * 3 : 12 * surface->vertex_count;
		surface->vertex_buffer = device.vertex_buffer_create(surface->vertex_buffer_size, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		if (external) {
			surface->previous_vertex_buffer = device.vertex_buffer_create(144 * 3, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
			surface->current_allocation_generation = 7;
			surface->previous_allocation_generation = 8;
			surface->index_count = 3;
			surface->index_buffer_size = 12;
			surface->index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
			surface->index_array = device.index_array_create(surface->index_buffer, 0, 3);
			CHECK(device.validate_external_indexed_surface_buffers(surface->vertex_buffer, surface->previous_vertex_buffer, surface->index_buffer, surface->index_array, 3, 3));
			RID array;
			RD::VertexFormatID format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, array, format);
		} else if (cached) {
			// Original ordinary native Version metadata; the actual VA is created
			// through RD and its exact original dependency map owns the backing.
			RD::VertexAttribute attribute;
			attribute.location = 0;
			attribute.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			attribute.stride = 12;
			const RD::VertexFormatID format = device.vertex_format_create({ attribute });
			const RID array = device.vertex_array_create(surface->vertex_count, format, { surface->vertex_buffer });
			surface->versions = static_cast<MeshStorage::Mesh::Surface::Version *>(memalloc(sizeof(MeshStorage::Mesh::Surface::Version)));
			new (surface->versions) MeshStorage::Mesh::Surface::Version;
			surface->versions[0].input_mask = 1;
			surface->versions[0].vertex_array = array;
			surface->versions[0].vertex_format = format;
			surface->version_count = 1;
		}
		if (quad) {
			surface->index_count = 6;
			surface->index_buffer_size = 24;
			surface->index_buffer = device.index_buffer_create(6, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
			surface->index_array = device.index_array_create(surface->index_buffer, 0, 6);
		}
		return surface;
	}
	static NativeMaterialMeshFixture create_recipe_fixture(RenderingDevice &device, RID material, bool cached) {
		NativeMaterialMeshFixture fixture;
		fixture.previous = MeshStorage::singleton;
		fixture.storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		fixture.mesh = fixture.storage->mesh_allocate();
		fixture.storage->mesh_initialize(fixture.mesh);
		auto *mesh = fixture.storage->mesh_owner.get_or_null(fixture.mesh);
		mesh->surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh->surface_count = 1;
		auto *original = surface(device, false, true, cached);
		mesh->surfaces[0] = original;
		fixture.storage->mesh_surface_set_material(fixture.mesh, 0, material);
		fixture.vertex_buffer = original->vertex_buffer;
		fixture.index_buffer = original->index_buffer;
		fixture.index_array = original->index_array;
		fixture.versions = original->version_count;
		if (cached) { fixture.vertex_array = original->versions[0].vertex_array; }
		return fixture;
	}
	static void check_recipe_fixture(const NativeMaterialMeshFixture &fixture, bool held) {
		const auto *mesh = fixture.storage->mesh_owner.get_or_null(fixture.mesh);
		REQUIRE(mesh != nullptr);
		CHECK(mesh->selection_holds == (held ? 1u : 0u));
		REQUIRE(mesh->surface_count == 1);
		CHECK(mesh->surfaces[0]->immutable_holds == (held ? 1u : 0u));
		CHECK(mesh->surfaces[0]->version_count == fixture.versions);
		CHECK(mesh->surfaces[0]->vertex_count == 4);
		CHECK(mesh->surfaces[0]->index_count == 6);
	}
	static void destroy_recipe_fixture(NativeMaterialMeshFixture &fixture) {
		check_recipe_fixture(fixture, false);
		fixture.storage->mesh_free(fixture.mesh);
		memdelete(fixture.storage);
		MeshStorage::singleton = fixture.previous;
		fixture = NativeMaterialMeshFixture();
	}
	static void immutable_surface(bool external) {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		mesh.surfaces[0] = surface(device, external);
		auto *old = mesh.surfaces[0];
		// Independent LOD/attribute/blend roots do not follow the selected VA.
		old->attribute_buffer = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		old->blend_shape_buffer = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		old->lods = memnew_arr(MeshStorage::Mesh::Surface::LOD, 1);
		old->lod_count = 1;
		old->lods[0].index_count = 3;
		old->lods[0].index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
		old->lods[0].index_array = device.index_array_create(old->lods[0].index_buffer, 0, 3);
		old->aabb = AABB(Vector3(-1, -2, -3), Vector3(4, 5, 6));
		old->uv_scale = Vector4(2, 3, 4, 5);
		const RID backing = old->vertex_buffer;
		const RID previous = old->previous_vertex_buffer;
		const RID index = old->index_buffer;
		const RID index_array = old->index_array;
		RID root = old->versions[0].vertex_array;
		RD::NativeImmutableHold refused_capacity;
		CHECK(device.native_immutable_hold_acquire(VectorView<RID>(&root, 1), refused_capacity, 1) == ERR_OUT_OF_MEMORY);
		CHECK_FALSE(refused_capacity.is_valid());
		CHECK_FALSE(device.native_immutable_resource_is_held(root));
		CHECK_FALSE(device.native_immutable_resource_is_held(backing));
		Hold first, second;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, first) == OK);
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, second) == OK);
		CHECK(first.is_valid());
		const RID accepted_array = first.get_vertex_array();
		CHECK(MeshStorage::_native_hold_freeze_directory(first) == ERR_BUSY);
		CHECK(old->output_directory_holds == 0);
		const auto original_byte = TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0);
		Hold moved(static_cast<Hold &&>(first));
		CHECK_FALSE(first.is_valid());
		CHECK(first.get_vertex_array().is_null());
		CHECK(moved.get_vertex_array() == accepted_array);
		uint32_t retirement_roots = 0;
		CHECK(MeshStorage::_native_hold_retirement_root_count(moved, retirement_roots) == OK);
		CHECK(retirement_roots == 7 + old->version_count + 2 * old->lod_count);
		bool saw_attribute = false, saw_blend = false, saw_lod_buffer = false, saw_lod_array = false;
		for (uint32_t ordinal = 0; ordinal < retirement_roots; ++ordinal) {
			RID actual_root;
			CHECK(MeshStorage::_native_hold_retirement_root_at(moved, ordinal, actual_root) == OK);
			saw_attribute |= actual_root == old->attribute_buffer;
			saw_blend |= actual_root == old->blend_shape_buffer;
			saw_lod_buffer |= actual_root == old->lods[0].index_buffer;
			saw_lod_array |= actual_root == old->lods[0].index_array;
		}
		CHECK(saw_attribute);
		CHECK(saw_blend);
		CHECK(saw_lod_buffer);
		CHECK(saw_lod_array);
		RID beyond(accepted_array);
		CHECK(MeshStorage::_native_hold_retirement_root_at(moved, retirement_roots, beyond) == ERR_INVALID_PARAMETER);
		CHECK(beyond.is_null());

		// Original direct RD writers also reject before staging/native effects.
		uint8_t changed = 255;
		CHECK(device.buffer_update(backing, 0, 1, &changed) == ERR_BUSY);
		CHECK(device.buffer_clear(backing, 0, 4) == ERR_BUSY);
		CHECK(device.buffer_copy(backing, backing, 0, 0, 4) == ERR_BUSY);

		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		if (external) {
			RID other;
			RD::VertexFormatID format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 2), false, false, other, format);
			CHECK(old->version_count == 2); // Original versions may realloc; the hold keeps values.
			CHECK(moved.get_vertex_array() == accepted_array);
		}
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(old->immutable_retired);
		CHECK(old->immutable_holds == 2);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		Hold refused;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(old, 1, false, false, 0, refused) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(refused.is_valid());
		// Replacing the live mesh owns distinct native bytes; accepted old fetch
		// stays at the old allocation after old mesh/surface detachment.
		mesh.surfaces[0] = surface(device, external);
		mesh.surface_count = 1;
		Hold replacement;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(mesh.surfaces[0], 1, false, false, 0, replacement) == OK);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, replacement.get_vertex_array(), 0) != original_byte);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, moved.get_vertex_array(), 0) == original_byte);
		// Derive from the actual retained old Surface after COW. A current-mesh
		// lookup would select replacement's distinct bytes and fail this check.
		Hold derived;
		CHECK(MeshStorage::mesh_surface_hold_derive(moved, 1, false, false, 0, derived) == OK);
		CHECK(derived.is_valid());
		CHECK(derived.get_vertex_array() == accepted_array);
		CHECK(derived.get_format() == moved.get_format());
		CHECK(derived.get_aabb() == AABB(Vector3(-1, -2, -3), Vector3(4, 5, 6)));
		CHECK(derived.get_uv_scale() == Vector4(2, 3, 4, 5));
		CHECK(derived.get_vertex_array() != replacement.get_vertex_array());
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, derived.get_vertex_array(), 0) == original_byte);
		CHECK(old->immutable_holds == 3);
		CHECK(MeshStorage::_native_hold_freeze_directory(moved) == OK);
		CHECK(MeshStorage::_native_hold_freeze_directory(moved) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_freeze_directory(derived) == OK);
		CHECK(old->output_directory_holds == 2);
		// Moving a frozen hold transfers exactly one shared directory claim.
		Hold frozen(static_cast<Hold &&>(derived));
		CHECK(MeshStorage::_native_hold_thaw_directory(derived) == ERR_UNAUTHORIZED);
		CHECK(old->output_directory_holds == 2);
		Hold existing;
		CHECK(MeshStorage::mesh_surface_hold_derive(moved, 1, false, false, 0, existing) == OK);
		CHECK(existing.get_vertex_array() == accepted_array);
		CHECK(MeshStorage::mesh_surface_hold_release(existing) == OK);
		if (external) {
			const uint32_t prepared = old->version_count;
			RID refused_array = accepted_array;
			RD::VertexFormatID refused_format = moved.get_vertex_format();
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 4), false, false, refused_array, refused_format);
			CHECK(refused_array.is_null());
			CHECK(refused_format == 0);
			CHECK(old->version_count == prepared);
			MeshStorage::_mesh_surface_get_external_arrays(old, 1, false, false, refused_array, refused_format);
			CHECK(refused_array == accepted_array);
			CHECK(refused_format == moved.get_vertex_format());
		}
		CHECK(MeshStorage::mesh_surface_hold_release(frozen) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_thaw_directory(moved) == OK);
		CHECK(MeshStorage::_native_hold_thaw_directory(moved) == OK);
		CHECK(old->output_directory_holds == 0);
		if (external) {
			const uint32_t prepared = old->version_count;
			RID resumed_array;
			RD::VertexFormatID resumed_format = 0;
			MeshStorage::_mesh_surface_get_external_arrays(old, 1 | (1 << 4), false, false, resumed_array, resumed_format);
			CHECK(resumed_array.is_valid());
			CHECK(resumed_format != 0);
			CHECK(old->version_count == prepared + 1);
		}
		CHECK(MeshStorage::_native_hold_freeze_directory(second) == OK);
		CHECK(old->output_directory_holds == 1);
		CHECK(old->immutable_holds == 2);
		Hold no_source;
		CHECK(MeshStorage::mesh_surface_hold_derive(first, 1, false, false, 0, no_source) != OK);
		CHECK_FALSE(no_source.is_valid());
		CHECK(MeshStorage::mesh_surface_hold_release(replacement) == OK);
		const RID replacement_current = mesh.surfaces[0]->vertex_buffer;
		const RID replacement_previous = mesh.surfaces[0]->previous_vertex_buffer;
		const RID replacement_index = mesh.surfaces[0]->index_buffer;
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		if (external) {
			device.free_rid(replacement_current);
			device.free_rid(replacement_previous);
			device.free_rid(replacement_index);
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
		CHECK(MeshStorage::mesh_surface_hold_release(second) == OK);
		CHECK(old->output_directory_holds == 0);
		CHECK(device.native_immutable_resource_is_held(backing));
		if (external) {
			// The actual external backing owner can request close while retained.
			// RD keeps physical storage, rejects new acquisition, and frees once
			// the final original hold returns; it does not lose a void free request.
			device.free_rid(backing);
			CHECK(device.native_immutable_resource_is_retiring(backing));
			CHECK(TestIndexed144DeviceAccess::fetch(device, driver, accepted_array, 0) == original_byte);
			RD::NativeImmutableHold new_physical;
			RID retiring_root = accepted_array;
			CHECK(device.native_immutable_hold_acquire(VectorView<RID>(&retiring_root, 1), new_physical) == ERR_BUSY);
		}
		CHECK(MeshStorage::mesh_surface_hold_release(moved) == OK);
		CHECK_FALSE(moved.is_valid());
		CHECK(moved.get_vertex_array().is_null());
		CHECK_FALSE(device.native_immutable_resource_is_held(backing));
		if (external) {
			device.free_rid(previous);
			device.free_rid(index);
			// Original index-array ownership follows its borrowed index backing.
			CHECK_FALSE(device.native_immutable_resource_is_held(index_array));
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void ordinary_directory() {
		// Initialize the actual storage object without unrelated Skeleton shader
		// startup. Every getter input used here has an actual original RD buffer.
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		auto *previous_storage = MeshStorage::get_singleton();
		auto *storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID normal = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, false);
		mesh.surfaces[0] = original;
		// Enroll the original surface in an actual mesh RID without creating a VA.
		const RID selected_rid = storage->mesh_allocate();
		storage->mesh_initialize(selected_rid);
		auto *selected_mesh = storage->mesh_owner.get_or_null(selected_rid);
		selected_mesh->surfaces = mesh.surfaces;
		selected_mesh->surface_count = 1;
		MeshStorage::NativeSelectionLease selection, sibling;
		const uint32_t versions_before = original->version_count;
		// Counter saturation is a CPU boundary model, not admitted custody.
		selected_mesh->selection_holds = UINT32_MAX;
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(selection.is_active());
		CHECK(selected_mesh->selection_holds == UINT32_MAX);
		CHECK(original->version_count == versions_before);
		selected_mesh->selection_holds = 0;
		const uint64_t initial_revision = selected_mesh->revision;
		// This formerly changed has_bone_weights before rejecting and leaked
		// its temporary Surface. The original API must reject before allocation.
		RenderingServerTypes::SurfaceData failed_bones;
		failed_bones.format = uint64_t(RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION) | RSE::ARRAY_FORMAT_BONES;
		const uint32_t allocations_before_failed_add = driver.allocations;
		CHECK_FALSE(storage->mesh_needs_instance(selected_rid, true));
		ERR_PRINT_OFF;
		storage->mesh_add_surface(selected_rid, failed_bones);
		ERR_PRINT_ON;
		CHECK_FALSE(storage->mesh_needs_instance(selected_rid, true));
		CHECK_FALSE(selected_mesh->has_bone_weights);
		CHECK(selected_mesh->surface_count == 1);
		CHECK(selected_mesh->surfaces[0] == original);
		CHECK(selected_mesh->revision == initial_revision);
		CHECK(driver.allocations == allocations_before_failed_add);
		storage->mesh_set_path(selected_rid, "revision-before-selection");
		CHECK(selected_mesh->revision == initial_revision + 1);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == OK);
		CHECK(selection.get_revision() == initial_revision + 1);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == ERR_ALREADY_IN_USE);
		CHECK(storage->mesh_selection_acquire(selected_rid, sibling) == OK);
		CHECK(sibling.get_revision() == selection.get_revision());
		CHECK(original->version_count == versions_before);
		CHECK(original->immutable_holds == 0);
		{
			MeshStorage::NativeSelectionLease inactive;
			MeshStorage::NativeSurfaceHold source;
			CHECK(storage->mesh_selection_source_hold_acquire(inactive, 0, source) == ERR_UNAUTHORIZED);
			CHECK_FALSE(source.is_valid());
			CHECK(storage->mesh_selection_source_hold_acquire(selection, 1, source) == ERR_DOES_NOT_EXIST);
			CHECK_FALSE(source.is_valid());
			CHECK(storage->mesh_selection_source_hold_acquire(selection, 0, source) == OK);
			CHECK(source.is_valid());
			CHECK(original->immutable_holds == 1);
			CHECK(source.get_primitive() == original->primitive);
			MeshStorage::NativeVertexArrayPlan source_plan;
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(source, 1, false, false, 0, source_plan) == OK);
			// surface() already published the original mask1 VA/VF; source
			// planning must reuse that exact Version without another factory.
			CHECK(source_plan.existing_vertex_array == original->versions[0].vertex_array);
			CHECK(source_plan.existing_vertex_format == original->versions[0].vertex_format);
			CHECK(source_plan.vertex_count == original->vertex_count);
			CHECK(!source_plan.buffers.is_empty());
			if (!source_plan.buffers.is_empty()) { CHECK(source_plan.buffers[0] == original->vertex_buffer); }
			CHECK(original->version_count == versions_before);
			CHECK(selected_mesh->selection_holds == 2);
			CHECK(MeshStorage::mesh_surface_hold_release(source) == OK);
			CHECK(original->immutable_holds == 0);
			CHECK(original->version_count == versions_before);
		}
		const RID material_before = original->material;
		MeshStorage::ExternalSurfaceDescriptor descriptor;
		descriptor.vertex_count = descriptor.index_count = 3;
		descriptor.current_vertex_buffer = device.vertex_buffer_create(432, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		descriptor.previous_vertex_buffer = device.vertex_buffer_create(432, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		descriptor.index_buffer = device.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
		descriptor.index_array = device.index_array_create(descriptor.index_buffer, 0, 3);
		descriptor.vertex_buffer_size = descriptor.previous_vertex_buffer_size = 432;
		descriptor.index_buffer_size = 12;
		descriptor.primitive = RSE::PRIMITIVE_TRIANGLES;
		descriptor.material = RID::from_uint64(999);
		descriptor.generation = 1;
		descriptor.current_allocation_generation = 1;
		descriptor.previous_allocation_generation = 2;
		uint32_t added_index = UINT32_MAX;
		uint64_t added_generation = 0;
		CHECK(storage->mesh_add_external_surface(selected_rid, descriptor, added_index, added_generation) == ERR_BUSY);
		CHECK(added_index == UINT32_MAX);
		CHECK(added_generation == 0);
		// Prove this exact descriptor is accepted by the unheld original API.
		const RID descriptor_neighbor = storage->mesh_allocate();
		storage->mesh_initialize(descriptor_neighbor);
		CHECK(storage->mesh_add_external_surface(descriptor_neighbor, descriptor, added_index, added_generation) == OK);
		CHECK(storage->mesh_get_surface_count(descriptor_neighbor) == 1);
		storage->mesh_free(descriptor_neighbor);
		device.free_rid(descriptor.current_vertex_buffer);
		device.free_rid(descriptor.previous_vertex_buffer);
		device.free_rid(descriptor.index_buffer);
		ERR_PRINT_OFF;
		storage->mesh_free(selected_rid);
		storage->mesh_clear(selected_rid);
		storage->mesh_surface_remove(selected_rid, 0);
		storage->mesh_surface_set_material(selected_rid, 0, RID::from_uint64(999));
		storage->mesh_set_path(selected_rid, "must-be-refused");
		CHECK(storage->mesh_instance_create(selected_rid).is_null());
		ERR_PRINT_ON;
		CHECK(storage->mesh_owner.get_or_null(selected_rid) == selected_mesh);
		CHECK(selected_mesh->surface_count == 1);
		CHECK(selected_mesh->surfaces[0] == original);
		CHECK(original->material == material_before);
		CHECK(selected_mesh->revision == selection.get_revision());
		CHECK(storage->mesh_get_path(selected_rid) == "revision-before-selection");
		CHECK(storage->mesh_selection_release(selection) == OK);
		CHECK(selected_mesh->selection_holds == 1);
		CHECK(storage->mesh_selection_release(selection) == ERR_UNAUTHORIZED);
		CHECK(storage->mesh_selection_release(sibling) == OK);
		CHECK(selected_mesh->selection_holds == 0);
		CHECK(selection.get_revision() == 0);
		storage->mesh_surface_set_material(selected_rid, 0, RID::from_uint64(998));
		CHECK(selected_mesh->revision == initial_revision + 2);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == OK);
		CHECK(selection.get_revision() == initial_revision + 2);
		CHECK(storage->mesh_selection_release(selection) == OK);
		// Exercise the actual original shadow notification path, including a
		// stale null reverse entry while its target is selected.
		const RID shadow_rid = storage->mesh_allocate();
		storage->mesh_initialize(shadow_rid);
		auto *shadow_mesh = storage->mesh_owner.get_or_null(shadow_rid);
		uint32_t shadow_notifications = 0;
		DependencyTracker shadow_tracker;
		shadow_tracker.userdata = &shadow_notifications;
		shadow_tracker.changed_callback = [](Dependency::DependencyChangedNotification notification, DependencyTracker *tracker) {
			if (notification == Dependency::DEPENDENCY_CHANGED_MESH) {
				++*static_cast<uint32_t *>(tracker->userdata);
			}
		};
		shadow_tracker.update_begin();
		shadow_tracker.update_dependency(&selected_mesh->dependency);
		shadow_tracker.update_end();
		const uint64_t before_shadow = selected_mesh->revision;
		storage->mesh_set_shadow_mesh(selected_rid, shadow_rid);
		CHECK(selected_mesh->revision == before_shadow + 1);
		CHECK(shadow_notifications == 1);
		storage->mesh_clear(shadow_rid);
		CHECK(selected_mesh->revision == before_shadow + 2);
		CHECK(selected_mesh->shadow_mesh.is_null());
		CHECK(shadow_notifications == 2);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == OK);
		const uint64_t selected_shadow_revision = selection.get_revision();
		storage->mesh_clear(shadow_rid);
		CHECK(shadow_notifications == 3);
		CHECK(selected_mesh->revision == selected_shadow_revision);
		CHECK(storage->mesh_selection_release(selection) == OK);
		// Original reverse entries outlive invalidation. Return the test's
		// notification and reverse-edge custody explicitly before owner cleanup.
		shadow_tracker.clear();
		shadow_mesh->shadow_owners.erase(selected_mesh);
		storage->mesh_free(shadow_rid);
		// Saturation is a boundary fixture. A real MAX snapshot must return
		// before the subsequent owner mutation exhausts the revision.
		selected_mesh->revision = UINT64_MAX;
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == OK);
		CHECK(selection.get_revision() == UINT64_MAX);
		CHECK(storage->mesh_selection_release(selection) == OK);
		storage->mesh_set_path(selected_rid, "exhausted");
		CHECK(selected_mesh->revision == 0);
		CHECK(storage->mesh_selection_acquire(selected_rid, selection) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(selection.is_active());
		storage->mesh_set_path(selected_rid, "still-exhausted");
		CHECK(selected_mesh->revision == 0);
		// Restore the existing fixture's local original owner for directory tests.
		selected_mesh->surfaces = nullptr;
		selected_mesh->surface_count = 0;
		storage->mesh_free(selected_rid);
		Hold retained;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, retained) == OK);
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == ERR_BUSY);
		const uint64_t neighbor_mask = 1 | (1 << RSE::ARRAY_NORMAL);
		RID neighbor;
		RD::VertexFormatID format = 0;
		const uint32_t initial = original->version_count;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, false, false, neighbor, format);
		CHECK(neighbor.is_valid());
		CHECK(format != 0);
		CHECK(original->version_count == initial + 1);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
		const RID prepared = neighbor;
		const RD::VertexFormatID prepared_format = format;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, false, false, neighbor, format);
		CHECK(neighbor == prepared);
		CHECK(format == prepared_format);
		Hold derived;
		CHECK(MeshStorage::mesh_surface_hold_derive(retained, neighbor_mask, false, false, 0, derived) == OK);
		CHECK(derived.get_vertex_array() == prepared);
		const uint32_t frozen = original->version_count;
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, true, false, neighbor, format);
		CHECK(neighbor.is_null());
		CHECK(format == 0);
		CHECK(original->version_count == frozen);
		CHECK(MeshStorage::_native_hold_thaw_directory(retained) == OK);
		storage->mesh_surface_get_vertex_arrays_and_format(original, neighbor_mask, true, false, neighbor, format);
		CHECK(neighbor.is_valid());
		CHECK(format != 0);
		CHECK(original->version_count == frozen + 1);
		CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(derived) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(retained) == OK);
		device.free_rid(normal);
		memdelete(storage);
		MeshStorage::singleton = previous_storage;
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void prospective_plan(bool external) {
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		auto *previous_storage = MeshStorage::get_singleton();
		auto *storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID normal = device.vertex_buffer_create(16, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, external);
		mesh.surfaces[0] = original;
		const RID current = original->vertex_buffer, previous = original->previous_vertex_buffer, index = original->index_buffer;
		Hold retained;
		const Error acquired = MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, retained);
		CHECK(acquired == OK);
		if (acquired == OK) {
			using Plan = MeshStorage::NativeVertexArrayPlan;
			Plan plan;
			const uint32_t formats = driver.formats;
			const uint32_t arrays = TestIndexed144DeviceAccess::arrays(device);
			const uint32_t versions = original->version_count;
			const uint32_t holds = original->immutable_holds;
			const uint32_t buffer_holds = TestIndexed144DeviceAccess::holds(device, original->vertex_buffer);
			const uint32_t device_holds = TestIndexed144DeviceAccess::device_holds(device);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array == retained.get_vertex_array());
			CHECK(plan.existing_vertex_format == retained.get_vertex_format());
			CHECK(plan.vertex_count == 3);
			CHECK(plan.buffers.size() == 1);
			if (!plan.buffers.is_empty()) {
				CHECK(plan.buffers[0] == original->vertex_buffer);
			}
			CHECK(plan.attributes.size() == (external ? 10 : 1));
			if (!plan.attributes.is_empty()) {
				CHECK(plan.attributes[0].location == 0);
				CHECK(plan.attributes[0].binding == (external ? 0u : UINT32_MAX));
				CHECK(plan.attributes[0].stride == (external ? 144 : 12));
				CHECK(plan.attributes[0].format == (external ? RD::DATA_FORMAT_R32G32B32A32_SFLOAT : RD::DATA_FORMAT_R32G32B32_SFLOAT));
			}
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array.is_null());
			CHECK(plan.existing_vertex_format == 0);
			CHECK(plan.attributes.size() == (external ? 10 : 2));
			MeshStorage::NativeProspectiveGeometryPlan geometry;
			CHECK(MeshStorage::_native_hold_prospective_geometry(retained, 3, plan, 256, geometry) == (external ? ERR_UNAVAILABLE : OK));
			if (!external) {
				CHECK(geometry.surface == original);
				CHECK(geometry.version_key.input_mask == 3);
				CHECK(geometry.future_vertex_array);
				CHECK(!geometry.detached);
				CHECK(geometry.initial.limit == 256);
				CHECK(geometry.initial.roots.size() == 1); // Ordinary fixture has no IA.
				if (geometry.initial.roots.size() == 1) {
					CHECK(geometry.initial.roots[0].future == 1);
				}
				CHECK(geometry.directory.roots.is_empty());
				Plan wrong = plan;
				if (!wrong.attributes.is_empty()) {
					wrong.attributes.write[0].binding = 7;
					CHECK(MeshStorage::_native_hold_prospective_geometry(retained, 3, wrong, 256, geometry) == ERR_INVALID_DATA);
					CHECK(geometry.initial.roots.is_empty());
				}
			}

			CHECK(plan.buffers.size() == (external ? 1 : 2));
			if (!external) {
				CHECK(plan.offsets.size() == 2);
				if (plan.buffers.size() == 2 && plan.offsets.size() == 2) {
					CHECK(plan.buffers[1] == normal);
					CHECK(plan.offsets[0] == 0);
					CHECK(plan.offsets[1] == 0);
				}
				storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = RID();
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_INVALID_DATA);
				storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
				const RID wrong_default = device.storage_buffer_create(16);
				CHECK(wrong_default.is_valid());
				if (wrong_default.is_valid()) {
					storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = wrong_default;
					ERR_PRINT_OFF;
					CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_DOES_NOT_EXIST);
					ERR_PRINT_ON;
					CHECK(plan.attributes.is_empty());
					CHECK(plan.buffers.is_empty());
					storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
					device.free_rid(wrong_default);
				}
				TestIndexed144DeviceAccess::external_access(device, normal, true);
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_UNAVAILABLE);
				CHECK(plan.buffers.is_empty());
				TestIndexed144DeviceAccess::external_access(device, normal, false);
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
				CHECK(TestIndexed144DeviceAccess::holds(device, normal) == 0);
			} else {
				CHECK(plan.offsets.is_empty());
				CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1ull << 31, false, false, 0, plan) == ERR_UNAVAILABLE);
			}
			TestIndexed144DeviceAccess::size(device, original->vertex_buffer, original->vertex_buffer_size - 1);
			ERR_PRINT_OFF;
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_INVALID_PARAMETER);
			ERR_PRINT_ON;
			CHECK(plan.buffers.is_empty());
			CHECK(plan.existing_vertex_array.is_null());
			TestIndexed144DeviceAccess::size(device, original->vertex_buffer, original->vertex_buffer_size);
			CHECK(TestIndexed144DeviceAccess::holds(device, original->vertex_buffer) == buffer_holds);
			CHECK(TestIndexed144DeviceAccess::device_holds(device) == device_holds);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, true, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, true, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 1, plan) == ERR_UNAVAILABLE);
			original->immutable_holds = UINT32_MAX;
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_INVALID_PARAMETER);
			original->immutable_holds = holds;
			// A published failed Version must not be reclassified as future work.
			const RID saved_array = original->versions[0].vertex_array;
			original->versions[0].vertex_array = RID();
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_CANT_CREATE);
			CHECK(plan.existing_vertex_array.is_null());
			CHECK(plan.existing_vertex_format == 0);
			original->versions[0].vertex_array = saved_array;
			CHECK(driver.formats == formats);
			CHECK(TestIndexed144DeviceAccess::arrays(device) == arrays);
			CHECK(original->version_count == versions);
			CHECK(original->immutable_holds == holds);
			// Detached retained source, frozen existing and missing keys.
			MeshStorage::_mesh_surface_clear(&mesh, 0);
			mesh.surface_count = 0;
			CHECK(MeshStorage::_native_hold_freeze_directory(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == OK);
			CHECK(!plan.buffers.is_empty());
			if (!plan.buffers.is_empty()) {
				CHECK(plan.buffers[0] == original->vertex_buffer);
			}
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::_native_hold_thaw_directory(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			if (!external) {
				CHECK(MeshStorage::_native_hold_prospective_geometry(retained, 3, plan, 256, geometry) == OK);
				CHECK(geometry.detached);
				CHECK(geometry.directory.roots.size() == 3); // VB, existing VA, future VA.
				const uint32_t exact_limit = geometry.directory.roots.size();
				if (exact_limit > 0) {
					CHECK(MeshStorage::_native_hold_prospective_geometry(retained, 3, plan, exact_limit - 1, geometry) == ERR_OUT_OF_MEMORY);
					CHECK(geometry.directory.roots.is_empty());
					CHECK(MeshStorage::_native_hold_prospective_geometry(retained, 3, plan, exact_limit, geometry) == OK);
				}
			}
			Hold derived;
			CHECK(MeshStorage::mesh_surface_hold_derive(retained, 3, false, false, 0, derived) == OK);
			CHECK(derived.is_valid());
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 3, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array == derived.get_vertex_array());
			CHECK(plan.existing_vertex_format == derived.get_vertex_format());
			CHECK(MeshStorage::mesh_surface_hold_release(derived) == OK);
			CHECK(MeshStorage::mesh_surface_hold_release(retained) == OK);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, 1, false, false, 0, plan) == ERR_UNAUTHORIZED);
		} else {
			MeshStorage::_mesh_surface_clear(&mesh, 0);
			mesh.surface_count = 0;
		}
		if (external) {
			device.free_rid(current);
			device.free_rid(previous);
			device.free_rid(index);
		}
		device.free_rid(normal);
		memdelete(storage);
		MeshStorage::singleton = previous_storage;
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void ordinary_fallback_transfer() {
		using Access = TestIndexed144DeviceAccess;
		using State = Access::TransferState;
		RenderingDevice device;
		FallbackTransferDriver driver;
		Access::begin_fallback_transfers(device, driver);
		auto *previous_storage = MeshStorage::get_singleton();
		auto *storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		float normal_data[] = { 1.0f, 0.0f, 0.0f };
		float color_data[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const RID normal = device.vertex_buffer_create(sizeof(normal_data), Span<uint8_t>(reinterpret_cast<uint8_t *>(normal_data), sizeof(normal_data)));
		const RID color = device.vertex_buffer_create(sizeof(color_data), Span<uint8_t>(reinterpret_cast<uint8_t *>(color_data), sizeof(color_data)));
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_NORMAL] = normal;
		storage->mesh_default_rd_buffers[MeshStorage::DEFAULT_RD_BUFFER_COLOR] = color;
		auto *original = surface(device, false);
		constexpr uint64_t normal_mask = (1ull << RSE::ARRAY_VERTEX) | (1ull << RSE::ARRAY_NORMAL);
		constexpr uint64_t mask = normal_mask | (1ull << RSE::ARRAY_COLOR);
		RID array;
		RD::VertexFormatID format = 0;
		storage->mesh_surface_get_vertex_arrays_and_format(original, mask, false, false, array, format);
		CHECK(normal.is_valid());
		CHECK(color.is_valid());
		CHECK(array.is_valid());
		Hold retained;
		const Error acquired = MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, retained);
		CHECK(acquired == OK);
		if (acquired == OK && normal.is_valid() && color.is_valid() && array.is_valid()) {
			MeshStorage::NativeVertexArrayPlan plan;
			CHECK_FALSE(Access::poll_fallback_transfers(device));
			MeshStorage::mesh_surface_settle_completed_fallback_uploads(retained, mask);
			CHECK(Access::fallback_worker_draw_use(device) == 0);
			CHECK(Access::fallback_transfer_state(device, normal) == State::PENDING);
			CHECK(Access::fallback_transfer_state(device, color) == State::PENDING);
			CHECK(Access::check_vertex_array_transfer(device, array) == 2);
			Access::check_vertex_buffer_transfer(device, normal);
			CHECK(Access::fallback_transfer_state(device, normal) == State::PENDING);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, mask, false, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(plan.buffers.is_empty());

			driver.completed = true;
			CHECK(Access::poll_fallback_transfers(device));
			CHECK(driver.polls == 2);
			CHECK(driver.recycles == 1);
			CHECK(Access::check_vertex_array_transfer(device, array) == 0);
			CHECK(Access::fallback_transfer_state(device, normal) == State::PROCESSED);
			CHECK(Access::fallback_transfer_state(device, color) == State::PROCESSED);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, mask, false, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(plan.buffers.is_empty());

			const uint64_t previous_draw_use = Access::fallback_worker_draw_use(device);
#ifdef THREADS_ENABLED
			Access::with_fallback_worker_locked(device, [&]() {
				MeshStorage::mesh_surface_settle_completed_fallback_uploads(retained, mask);
			});
			CHECK(Access::fallback_transfer_state(device, normal) == State::PROCESSED);
			CHECK(Access::fallback_transfer_state(device, color) == State::PROCESSED);
			CHECK(Access::fallback_worker_draw_use(device) == previous_draw_use);
#endif
			MeshStorage::mesh_surface_settle_completed_fallback_uploads(retained, normal_mask);
			CHECK(Access::fallback_transfer_state(device, normal) == State::DETACHED);
			CHECK(Access::fallback_transfer_state(device, color) == State::PROCESSED);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, mask, false, false, 0, plan) == ERR_UNAVAILABLE);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, normal_mask, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array.is_null());
			Hold normal_derived;
			CHECK(MeshStorage::mesh_surface_hold_derive(retained, normal_mask, false, false, 0, normal_derived) == OK);
			if (normal_derived.is_valid()) {
				CHECK(MeshStorage::mesh_surface_hold_release(normal_derived) == OK);
			}
			MeshStorage::mesh_surface_settle_completed_fallback_uploads(retained, mask);
			CHECK(Access::fallback_transfer_state(device, color) == State::DETACHED);
			CHECK(Access::fallback_worker_draw_use(device) == previous_draw_use);
			CHECK(driver.polls == 2);
			CHECK(driver.recycles == 1);
			CHECK(MeshStorage::mesh_surface_hold_plan_vertex_array(retained, mask, false, false, 0, plan) == OK);
			CHECK(plan.existing_vertex_array == array);
			CHECK(plan.existing_vertex_format == format);
			CHECK((plan.buffers == Vector<RID>{ original->vertex_buffer, normal, color }));
			Hold all_derived;
			CHECK(MeshStorage::mesh_surface_hold_derive(retained, mask, false, false, 0, all_derived) == OK);
			if (all_derived.is_valid()) {
				CHECK(all_derived.get_vertex_array() == array);
				CHECK(MeshStorage::mesh_surface_hold_release(all_derived) == OK);
			}
		} else {
			driver.completed = true;
			CHECK(Access::poll_fallback_transfers(device));
		}
		if (acquired == OK) {
			CHECK(MeshStorage::mesh_surface_hold_release(retained) == OK);
		}
		MeshStorage::_mesh_surface_dispose(original);
		if (normal.is_valid()) {
			device.free_rid(normal);
		}
		if (color.is_valid()) {
			device.free_rid(color);
		}
		memdelete(storage);
		MeshStorage::singleton = previous_storage;
		Access::end_fallback_transfers(device);
		CHECK(driver.frees == driver.allocations);
	}

	static void incremental_surface(bool external) {
		// Actual local holds exercise metadata steps, not Core pin/packet authority.
		RenderingDevice device;
		Indexed144FormatDriver driver;
		TestIndexed144DeviceAccess::begin_native(device, driver);
		MeshStorage::Mesh mesh;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		mesh.surface_count = 1;
		auto *original = surface(device, external);
		mesh.surfaces[0] = original;
		const RID current = original->vertex_buffer, previous = original->previous_vertex_buffer, index = original->index_buffer;
		Hold exact;
		CHECK(MeshStorage::_mesh_surface_hold_acquire(original, 1, false, false, 0, exact) == OK);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		mesh.surface_count = 0;
		CHECK(MeshStorage::_native_hold_freeze_directory(exact) == OK);
		CHECK(original->output_directory_holds == 1);
		uint32_t count = 0;
		CHECK(MeshStorage::_native_hold_retirement_root_count(exact, count) == OK);
		LocalVector<RID> roots;
		for (uint32_t i = 0; i < count; ++i) {
			RID root;
			CHECK(MeshStorage::_native_hold_retirement_root_at(exact, i, root) == OK);
			if (root.is_valid()) {
				roots.push_back(root);
			}
		}
		RD::NativeImmutableHold complete;
		CHECK(device.native_immutable_hold_acquire(VectorView<RID>(roots.ptr(), roots.size()), complete) == OK);
		// A locally held directory is not an original accepted Core binding.
		// Unsupported selected-only transfer refuses without moving custody.
		CHECK(MeshStorage::_native_hold_transfer_source(exact) == nullptr);
		CHECK(MeshStorage::_native_hold_mark_transferred(exact) == ERR_INVALID_DATA);
		CHECK(exact.is_valid());
		CHECK(original->output_directory_holds == 1);
		CHECK(MeshStorage::_native_hold_begin_return(exact) == ERR_BUSY);
		CHECK(MeshStorage::_native_hold_finish_return(exact) == ERR_BUSY);
		CHECK(driver.frees == 0);
		CHECK(device.native_immutable_hold_release(complete) == OK);
		CHECK(MeshStorage::mesh_surface_hold_release(exact) == OK);
		if (external) {
			device.free_rid(current);
			device.free_rid(previous);
			device.free_rid(index);
		}
		TestIndexed144DeviceAccess::dispose(device);
		CHECK(driver.frees == driver.allocations);
		memfree(mesh.surfaces);
		TestIndexed144DeviceAccess::end_native(device);
	}
	static void run(RenderingDevice &p_device, Indexed144FormatDriver &p_driver, bool p_failure) {
		MeshStorage::Mesh mesh;
		mesh.surface_count = 1;
		mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
		auto *surface = memnew(MeshStorage::Mesh::Surface);
		mesh.surfaces[0] = surface;
		surface->external = true;
		surface->vertex_count = 3;
		surface->current_allocation_generation = 7;
		surface->previous_allocation_generation = 8;
		surface->vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 71);
		surface->previous_vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 72);
		const RID current = surface->vertex_buffer, previous = surface->previous_vertex_buffer;
		RID va;
		RD::VertexFormatID format = 0;
		if (p_failure) {
			p_driver.fail_format = true;
			ERR_PRINT_OFF;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
			ERR_PRINT_ON;
			CHECK(va.is_null());
			CHECK(surface->version_count == 0);
			CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
			p_driver.fail_format = false;
			TestIndexed144DeviceAccess::size(p_device, current, 144 * 2);
			ERR_PRINT_OFF;
			MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
			ERR_PRINT_ON;
			CHECK(va.is_null());
			CHECK(surface->version_count == 0);
			CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
			TestIndexed144DeviceAccess::size(p_device, current, 144 * 3);
		}
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
		CHECK(va.is_valid());
		CHECK(surface->version_count == 1);
		const RID position_only = va;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, va, format);
		CHECK(va == position_only);
		CHECK(surface->version_count == 1);
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va.is_valid());
		CHECK(va != position_only);
		CHECK(surface->version_count == 2);
		const RID tangent_view = va;
		surface->previous_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va != tangent_view);
		CHECK(surface->version_count == 3);
		const RID previous_changed = va;
		surface->current_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (1 << 2), false, false, va, format);
		CHECK(va != previous_changed);
		CHECK(surface->version_count == 4);
		MeshStorage::_mesh_surface_get_external_arrays(surface, (1 << 0) | (1 << 1) | (1 << 2) | (1 << 12) | (1 << 13) | (1 << 14), true, false, va, format);
		CHECK(va.is_valid());
		CHECK(surface->version_count == 5);
		CHECK(p_driver.last_attributes.size() == 13);
		bool previous_tangent = false;
		for (const auto &attribute : p_driver.last_attributes) {
			CHECK(attribute.stride == 144);
			if (attribute.location == 14) {
				previous_tangent = true;
				CHECK(attribute.binding == 1);
				CHECK(attribute.offset == 32);
				CHECK(attribute.format == RD::DATA_FORMAT_R32G32B32A32_SFLOAT);
			}
		}
		CHECK(previous_tangent);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 0) == 71);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 1) == 72);
		surface->vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 81);
		surface->previous_vertex_buffer = TestIndexed144DeviceAccess::buffer(p_device, 82);
		surface->current_allocation_generation++;
		surface->previous_allocation_generation++;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (7 << 12), true, false, va, format);
		CHECK(va.is_valid());
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 0) == 81);
		CHECK(TestIndexed144DeviceAccess::binding(p_device, va, 1) == 82);
		const RID triangle_array = va;
		MeshStorage::_mesh_surface_get_external_arrays(surface, 1 | (7 << 12), true, true, va, format);
		CHECK(va.is_valid());
		CHECK(va != triangle_array);
		MeshStorage::_mesh_surface_clear(&mesh, 0);
		memfree(mesh.surfaces);
		mesh.surfaces = nullptr;
		mesh.surface_count = 0;
		CHECK(TestIndexed144DeviceAccess::arrays(p_device) == 0);
		CHECK(TestIndexed144DeviceAccess::owns(p_device, current));
		CHECK(TestIndexed144DeviceAccess::owns(p_device, previous));
	}
};
NativeMaterialMeshFixture create_native_material_mesh_fixture(RenderingDevice &device, RID material, bool cached) {
 return TestMeshStorageExternalAccess::create_recipe_fixture(device, material, cached);
}
void check_native_material_mesh_fixture(const NativeMaterialMeshFixture &fixture, bool held) {
 TestMeshStorageExternalAccess::check_recipe_fixture(fixture, held);
}
void destroy_native_material_mesh_fixture(NativeMaterialMeshFixture &fixture) {
 TestMeshStorageExternalAccess::destroy_recipe_fixture(fixture);
}
} //namespace RendererRD

TEST_CASE("[B4][K05] actual Indexed144 cache retains exact mask generations and borrowed buffers") {
	RenderingDevice device;
	Indexed144FormatDriver driver;
	TestIndexed144DeviceAccess::attach(device, &driver);
	RendererRD::TestMeshStorageExternalAccess::run(device, driver, false);
	TestIndexed144DeviceAccess::detach(device);
}

TEST_CASE("[B4][K05] failed original VA construction publishes no cached version") {
	RenderingDevice device;
	Indexed144FormatDriver driver;
	TestIndexed144DeviceAccess::attach(device, &driver);
	RendererRD::TestMeshStorageExternalAccess::run(device, driver, true);
	TestIndexed144DeviceAccess::detach(device);
}

TEST_CASE("[B4][K05] original renderer pipeline keys distinguish Indexed144 from ordinary Mesh") {
	using namespace RendererSceneRenderImplementation;
	SceneShaderForwardMobile::ShaderData::PipelineKey mobile{};
	SceneShaderForwardClustered::ShaderData::PipelineKey clustered{};
	const uint32_t ordinary_mobile = mobile.hash(), ordinary_clustered = clustered.hash();
	mobile.indexed144 = true;
	clustered.indexed144 = true;
	CHECK(mobile.hash() != ordinary_mobile);
	CHECK(clustered.hash() != ordinary_clustered);
}

TEST_CASE("[B4][NativeSurfaceHold] ordinary native surface survives mesh replacement and explicit last return") {
	RendererRD::TestMeshStorageExternalAccess::immutable_surface(false);
	RendererRD::TestMeshStorageExternalAccess::incremental_surface(false);
	RendererRD::TestMeshStorageExternalAccess::ordinary_directory();
}

TEST_CASE("[B4][NativeSurfaceHold] Indexed144 surface keeps exact borrowed native backings and deferred owner free") {
	RendererRD::TestMeshStorageExternalAccess::immutable_surface(true);
	RendererRD::TestMeshStorageExternalAccess::incremental_surface(true);
}

TEST_CASE("[B4][MaterialPreparation] Mobile color preparation preserves transparent blending") {
	using MobileShader = RendererSceneRenderImplementation::SceneShaderForwardMobile;
	// This is the production predicate used before derive/pipeline allocation
	// for OPAQUE, TRANSPARENT and PROBE. No mirrored test-only admission rule.
	SUBCASE("transparent surfaces never select opaque fog-mask variants") {
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_FOG_MASK, true));
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_FOG_MASK, true));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_PASS, true));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_COLOR_PASS, true));
	}
	SUBCASE("opaque and alpha-clipped classifier result retains fog-mask support") {
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_FOG_MASK, false));
		CHECK(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_LIGHTMAP_FOG_MASK, false));
	}
	SUBCASE("noncolor and unadmitted multiview variants remain refused") {
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_SHADOW_PASS, false));
		CHECK_FALSE(MobileShader::ShaderData::external_color_version_supported(MobileShader::SHADER_VERSION_COLOR_PASS_MULTIVIEW, true));
	}
}

TEST_CASE("[B4][NativeVertexArrayPlan] retained ordinary source plans without factories") {
	RendererRD::TestMeshStorageExternalAccess::prospective_plan(false);
}

TEST_CASE("[B4][NativeVertexArrayPlan] ordinary fallback buffers retain completed transfer associations") {
	RendererRD::TestMeshStorageExternalAccess::ordinary_fallback_transfer();
}

TEST_CASE("[B4][NativeVertexArrayPlan] retained Indexed144 source plans without factories") {
	RendererRD::TestMeshStorageExternalAccess::prospective_plan(true);
}

TEST_CASE("[B4][NativeVertexArrayPreflight] validates exact fetches without factories or holds") {
	TestIndexed144DeviceAccess::preflight_only();
}

TEST_CASE("[B4][ProspectiveHoldIncrements] original owner counters reject combined planned pins without effects") {
	TestIndexed144DeviceAccess::prospective_hold_increments();
}

TEST_CASE("[B4][ProspectiveClosureKeys] follows actual and virtual adoption edges without staging") {
	TestIndexed144DeviceAccess::prospective_closure_only();
}

#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"

namespace RendererSceneRenderImplementation {
// Executes actual owner key/state builders without registering a shader cache
// or fabricating a ready ShaderRD variant. No ready-plan success is claimed.
class TestBackendPipelineValuePlanAccess {
public:
	static void clustered() {
		using Scene = SceneShaderForwardClustered;
		using Shader = Scene::ShaderData;
		Shader owner(Shader::NativeValuePlanTestTag{});
		owner.cull_mode = RSE::CULL_MODE_BACK;
		RendererRD::ExternalPacketPipelineRequest request;
		request.backend = RendererRD::ExternalPacketPipelineRequest::Backend::CLUSTERED;
		request.version = Scene::PIPELINE_VERSION_COLOR_PASS;
		request.specialization[0] = 17;
		request.specialization[1] = 29;
		request.specialization[2] = 31;
		request.wireframe = true;
		Shader::PipelineKey key = {};
		const Error keyed = owner.get_external_pipeline_key(request, RSE::PRIMITIVE_TRIANGLES, true, false, false, key);
		CHECK(keyed == OK);
		if (keyed != OK) {
			return;
		}
		CHECK(key.vertex_format_id == 0); // No prospective VF ID is invented.
		CHECK(key.instance192);
		CHECK_FALSE(key.indexed144);
		CHECK_FALSE(key.ubershader);
		CHECK(key.cull_mode == RD::POLYGON_CULL_BACK);
		CHECK(key.shader_specialization.packed_2 == 31);
		auto state = owner._build_native_pipeline_value_state(key, RD::TEXTURE_SAMPLES_4, false, false);
		CHECK(state.primitive == RD::RENDER_PRIMITIVE_TRIANGLES);
		CHECK(state.raster.cull_mode == RD::POLYGON_CULL_DISABLED);
		CHECK(state.raster.wireframe);
		CHECK(state.multisample.sample_count == RD::TEXTURE_SAMPLES_4);
		CHECK(state.depth_stencil.depth_compare_operator == RD::COMPARE_OP_GREATER_OR_EQUAL);
		CHECK(state.specialization.size() == 5);
		if (state.specialization.size() == 5) {
			CHECK(state.specialization[0].int_value == 17);
			CHECK(state.specialization[1].int_value == 29);
			CHECK(state.specialization[2].constant_id == 2);
			CHECK_FALSE(state.specialization[2].bool_value);
			CHECK(state.specialization[3].constant_id == 4);
			CHECK(state.specialization[3].int_value == uint32_t(RD::POLYGON_CULL_BACK));
			CHECK(state.specialization[4].constant_id == 5);
			CHECK(state.specialization[4].bool_value);
		}
		state = owner._build_native_pipeline_value_state(key, RD::TEXTURE_SAMPLES_1, false, true);
		CHECK(state.depth_stencil.depth_compare_operator == RD::COMPARE_OP_GREATER_OR_EQUAL);
		CHECK(state.depth_stencil.enable_depth_write);
		auto ordinary_key = key;
		ordinary_key.instance192 = false;
		const auto ordinary_state = owner._build_native_pipeline_value_state(ordinary_key, RD::TEXTURE_SAMPLES_1, false, true);
		CHECK(ordinary_state.depth_stencil.depth_compare_operator == RD::COMPARE_OP_EQUAL);
		CHECK_FALSE(ordinary_state.depth_stencil.enable_depth_write);
		const auto prior = key.hash();
		request.subpass = 1; // Clustered has no prospective subpass alternative.
		CHECK(owner.get_external_pipeline_key(request, RSE::PRIMITIVE_TRIANGLES, true, false, false, key) == ERR_INVALID_PARAMETER);
		CHECK(key.hash() == prior);
	}

	static void mobile() {
		using Scene = SceneShaderForwardMobile;
		using Shader = Scene::ShaderData;
		Shader owner(Shader::NativeValuePlanTestTag{});
		owner.depth_draw = Shader::DEPTH_DRAW_OPAQUE;
		owner.depth_test = Shader::DEPTH_TEST_ENABLED;
		owner.cull_mode = RSE::CULL_MODE_BACK;
		owner.uses_alpha = true;
		owner.wireframe = true;
		RendererRD::ExternalPacketPipelineRequest request;
		request.backend = RendererRD::ExternalPacketPipelineRequest::Backend::MOBILE;
		request.pass = RendererRD::ExternalPacketPass::TRANSPARENT;
		request.version = Scene::SHADER_VERSION_COLOR_PASS;
		request.subpass = 3;
		request.specialization[0] = 13;
		request.specialization[1] = 19;
		request.specialization[2] = 0x3f800000; // Exact request bits for float 1.0.
		Shader::PipelineKey key = {};
		const Error keyed = owner.get_external_pipeline_key(request, RSE::PRIMITIVE_TRIANGLES, true, false, true, key);
		CHECK(keyed == OK);
		if (keyed != OK) {
			return;
		}
		CHECK(key.vertex_format_id == 0);
		CHECK(key.render_pass == 3);
		CHECK(key.shader_specialization.packed_2 == 1.0f);
		auto state = owner._build_native_pipeline_value_state(key, RD::TEXTURE_SAMPLES_4, false, false);
		CHECK(state.raster.cull_mode == RD::POLYGON_CULL_DISABLED);
		CHECK(state.raster.wireframe); // Original owner wireframe survives a false request.
		CHECK_FALSE(state.depth_stencil.enable_depth_write);
		CHECK(state.blend.attachments.size() == 1);
		CHECK(state.specialization.size() == 6);
		if (state.specialization.size() == 6) {
			CHECK(state.specialization[0].int_value == 13);
			CHECK(state.specialization[1].int_value == 19);
			CHECK(state.specialization[2].constant_id == 2);
			CHECK(state.specialization[2].type == RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT);
			CHECK(state.specialization[2].float_value == 1.0f);
			CHECK(state.specialization[3].constant_id == 3);
			CHECK_FALSE(state.specialization[3].bool_value);
			CHECK(state.specialization[4].int_value == uint32_t(RD::POLYGON_CULL_BACK));
			CHECK(state.specialization[5].bool_value);
		}
		const auto prior = key.hash();
		request.version = Scene::SHADER_VERSION_COLOR_FOG_MASK;
		CHECK(owner.get_external_pipeline_key(request, RSE::PRIMITIVE_TRIANGLES, true, false, true, key) == ERR_INVALID_PARAMETER);
		CHECK(key.hash() == prior); // Failed key construction preserves the prior observation.
	}
};
}

TEST_CASE("[B4][BackendPipelineValuePlan] original Clustered key and factory states share value construction") {
	RendererSceneRenderImplementation::TestBackendPipelineValuePlanAccess::clustered();
}

TEST_CASE("[B4][BackendPipelineValuePlan] original Mobile key preserves subpass and float specialization bits") {
	RendererSceneRenderImplementation::TestBackendPipelineValuePlanAccess::mobile();
}


namespace {
template <typename Shader>
void check_native_pipeline_factory_refusal() {
	Shader owner(typename Shader::NativeValuePlanTestTag{});
	typename Shader::PipelineKey key = {};
	typename Shader::NativePipelineValuePlan observed;
	observed.key = key;
	RendererRD::NativePipelineFactorySink missing;
	Error error = OK;
	CHECK(owner.create_native_pipeline_checked(key, observed, missing, error).is_null());
	CHECK(error == ERR_UNCONFIGURED);

	uint32_t factory_calls = 0;
	const RendererRD::NativePipelineFactorySink forbidden{ &factory_calls,
		+[](void *context, const RendererRD::NativePipelineFactoryInput &, Error &result) -> RID {
			++*static_cast<uint32_t *>(context);
			result = ERR_BUG;
			return RID();
		} };
	key.ubershader = !observed.key.ubershader;
	error = OK;
	CHECK(owner.create_native_pipeline_checked(key, observed, forbidden, error).is_null());
	CHECK(error == ERR_INVALID_DATA);
	CHECK(factory_calls == 0);

	// Equal observations still cannot admit an invalid Instance192 cull.
	// In particular an early null RID must not preserve the caller's OK.
	key = observed.key;
	key.instance192 = true;
	key.cull_mode = RD::POLYGON_CULL_MAX;
	observed.key = key;
	error = OK;
	ERR_PRINT_OFF;
	const RID refused = owner.create_native_pipeline_checked(key, observed, forbidden, error);
	ERR_PRINT_ON;
	CHECK(refused.is_null());
	CHECK(error == ERR_INVALID_PARAMETER);
	CHECK(factory_calls == 0);
}
} // namespace

TEST_CASE("[B4][NativePipelineFactory] Clustered refuses before factory with explicit error") {
	check_native_pipeline_factory_refusal<RendererSceneRenderImplementation::SceneShaderForwardClustered::ShaderData>();
}

TEST_CASE("[B4][NativePipelineFactory] Mobile refuses before factory with explicit error") {
	check_native_pipeline_factory_refusal<RendererSceneRenderImplementation::SceneShaderForwardMobile::ShaderData>();
}

TEST_CASE("[B4][ProspectiveMaterialGraph] original finite symbols validate typed future dependencies without factories") {
	TestIndexed144DeviceAccess::prospective_material_graph();
}
