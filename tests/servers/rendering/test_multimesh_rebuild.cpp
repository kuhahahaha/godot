#include "core/io/marshalls.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_multimesh_rebuild)

// CPU controlled-driver constituent of original K02 M03/M11. These tests
// execute the actual storage/RD ownership paths, not a GPU or product issuer.
namespace TestMultimesh {
class Driver : public TransferTestDriverBase {
	struct Copy {
		BufferID source;
		BufferID destination;
		BufferCopyRegion region;
	};
	Vector<Copy> copies;

public:
	Vector<uint8_t> memory[256];
	bool live[256] = {};
	bool gpu[256] = {};
	uint32_t returns[256] = {};
	uint32_t allocations = 0;
	uint32_t gpu_attempts = 0;
	uint32_t fail_gpu_attempt = 0;
	uint32_t allocation_failures = 0;
	uint32_t map_failures = 0;
	bool fail_next_map = false;
	uint64_t submitted_serial = 0;

	BufferID buffer_create(uint64_t p_bytes, BitField<BufferUsageBits>, MemoryAllocationType p_type, uint64_t) override {
		if (p_type == MEMORY_ALLOCATION_TYPE_GPU && ++gpu_attempts == fail_gpu_attempt) {
			++allocation_failures;
			return {};
		}
		CHECK(allocations + 1 < 256);
		CHECK(p_bytes <= 4096);
		if (allocations + 1 >= 256 || p_bytes > 4096) {
			return {};
		}
		const uint32_t id = ++allocations;
		memory[id].resize_initialized(p_bytes);
		live[id] = true;
		gpu[id] = p_type == MEMORY_ALLOCATION_TYPE_GPU;
		return BufferID(id);
	}
	void buffer_free(BufferID p_buffer) override {
		CHECK(p_buffer.id > 0);
		CHECK(p_buffer.id <= allocations);
		if (!p_buffer.id || p_buffer.id > allocations) {
			return;
		}
		CHECK(live[p_buffer.id]);
		++returns[p_buffer.id];
		live[p_buffer.id] = false;
	}
	uint8_t *buffer_map(BufferID p_buffer) override {
		if (fail_next_map) {
			fail_next_map = false;
			++map_failures;
			return nullptr;
		}
		CHECK(p_buffer.id > 0);
		CHECK(p_buffer.id <= allocations);
		if (!p_buffer.id || p_buffer.id > allocations) {
			return nullptr;
		}
		CHECK(live[p_buffer.id]);
		return live[p_buffer.id] ? memory[p_buffer.id].ptrw() : nullptr;
	}
	void buffer_unmap(BufferID) override {}
	void set_object_name(ObjectType, ID, const String &) override {}
	bool command_buffer_begin(CommandBufferID) override { return true; }
	void command_buffer_end(CommandBufferID) override {}
	bool command_pool_reset(CommandPoolID) override { return true; }
	void command_copy_buffer(CommandBufferID, BufferID p_source, BufferID p_destination, VectorView<BufferCopyRegion> p_regions) override {
		for (uint32_t i = 0; i < p_regions.size(); ++i) {
			copies.push_back({ p_source, p_destination, p_regions[i] });
		}
	}
	Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
		for (const Copy &copy : copies) {
			REQUIRE(copy.source.id > 0);
			REQUIRE(copy.source.id <= allocations);
			REQUIRE(copy.destination.id > 0);
			REQUIRE(copy.destination.id <= allocations);
			CHECK(live[copy.source.id]);
			CHECK(live[copy.destination.id]);
			REQUIRE(copy.region.src_offset + copy.region.size <= uint64_t(memory[copy.source.id].size()));
			REQUIRE(copy.region.dst_offset + copy.region.size <= uint64_t(memory[copy.destination.id].size()));
			memcpy(memory[copy.destination.id].ptrw() + copy.region.dst_offset, memory[copy.source.id].ptr() + copy.region.src_offset, copy.region.size);
		}
		copies.clear();
		QueueCallReport report;
		report.issued = true;
		report.submission_serial = ++submitted_serial;
		record_queue_call_report(report);
		return OK;
	}
	FencePollResult fence_wait_observed(FenceID, uint64_t p_serial) override {
		FencePollResult result;
		result.completed = p_serial == submitted_serial;
		result.completed_serial = submitted_serial;
		return result;
	}
	Error fence_recycle_wait(FenceID, uint64_t p_serial) override {
		CHECK(p_serial == submitted_serial);
		return OK;
	}
	uint32_t live_gpu_count() const {
		uint32_t count = 0;
		for (uint32_t id = 1; id <= allocations; ++id) {
			count += live[id] && gpu[id];
		}
		return count;
	}
};
} //namespace TestMultimesh

namespace RendererRD {
class TestMultimeshRebuildAccess {
	struct Notifications {
		uint32_t invalidations = 0;
		uint32_t visible = 0;
		uint32_t deleted = 0;
		static void changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
			auto &state = *static_cast<Notifications *>(p_tracker->userdata);
			state.invalidations += p_notification == Dependency::DEPENDENCY_CHANGED_MULTIMESH;
			state.visible += p_notification == Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES;
		}
		static void removed(const RID &, DependencyTracker *p_tracker) {
			++static_cast<Notifications *>(p_tracker->userdata)->deleted;
		}
	};

	struct Fixture {
		RenderingDevice *previous_device = RenderingDevice::get_singleton();
		MeshStorage *previous_storage = MeshStorage::get_singleton();
		TestMultimesh::Driver driver;
		RenderingDevice device;
		MeshStorage *storage = nullptr;
		RID mesh;
		RenderingDevice::TransferWorker *worker = nullptr;

		Fixture() {
			device.singleton = &device;
			device.driver = &driver;
			device.frames.resize(1);
			device.frames_drawn = 10;
			worker = memnew(RenderingDevice::TransferWorker);
			worker->staging_buffer = driver.buffer_create(4096, RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_FROM_BIT, RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_CPU, 0);
			worker->staging_buffer_size_allocated = 4096;
			worker->command_buffer = RenderingDeviceDriver::CommandBufferID(1);
			worker->command_pool = RenderingDeviceDriver::CommandPoolID(1);
			worker->command_fence = RenderingDeviceDriver::FenceID(1);
			device.transfer_worker_pool.resize(1);
			device.transfer_worker_pool[0] = worker;
			device.transfer_worker_pool_size = 1;
			device.transfer_worker_operation_used_by_draw.resize(1);
			device.transfer_worker_pool_available_list.push_back(0);
			storage = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
			mesh = storage->mesh_allocate();
			storage->mesh_initialize(mesh);
			// Real native mesh owner with fixed triangle draw metadata. Raster
			// geometry is outside these allocation/disposal/notification checks.
			auto *original = storage->mesh_owner.get_or_null(mesh);
			original->surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
			original->surfaces[0] = memnew(MeshStorage::Mesh::Surface);
			original->surfaces[0]->vertex_count = 3;
			original->surfaces[0]->primitive = RSE::PRIMITIVE_TRIANGLES;
			original->surface_count = 1;
		}
		void complete_transfers() {
			if (worker->recording) {
				device._end_transfer_worker(worker);
			}
			if (worker->submission_pending) {
				device._submit_transfer_worker(worker);
			}
			CHECK(device._wait_for_transfer_worker(worker, true));
		}
		void dispose() {
			complete_transfers();
			device._free_pending_resources(0);
		}
		RID create(uint32_t p_capacity = 3) {
			RID mm = storage->multimesh_allocate();
			storage->multimesh_initialize(mm);
			storage->multimesh_set_mesh(mm, mesh);
			storage->multimesh_allocate_data(mm, p_capacity, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			complete_transfers();
			return mm;
		}
		uint64_t driver_id(RID p_rid) {
			const auto *buffer = device.storage_buffer_owner.get_or_null(p_rid);
			CHECK(buffer != nullptr);
			return buffer ? buffer->driver_id.id : 0;
		}
		void command(RID p_multimesh, uint32_t p_visible) {
			complete_transfers();
			const RID rid = storage->multimesh_get_command_buffer_rd_rid(p_multimesh);
			REQUIRE(rid.is_valid());
			const uint64_t id = driver_id(rid);
			REQUIRE(id != 0);
			CHECK(driver.live[id]);
			const auto &bytes = driver.memory[id];
			REQUIRE(bytes.size() == 20);
			const uint32_t expected[5] = { 3, p_visible, 0, 0, 0 };
			for (uint32_t word = 0; word < 5; ++word) {
				CHECK(decode_uint32(bytes.ptr() + word * 4) == expected[word]);
			}
		}
		void fail_update_map() {
			// A valid full upload staging block forces the original update
			// allocator to request a new block, whose actual driver map fails.
			auto &staging = device.upload_staging_buffers;
			staging.block_size = 4096;
			staging.max_size = 8192;
			staging.blocks.resize(1);
			auto &block = staging.blocks.write[0];
			block.driver_id = driver.buffer_create(4096, RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_FROM_BIT, RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_CPU, 0);
			block.data_ptr = driver.buffer_map(block.driver_id);
			block.frame_used = device.frames_drawn;
			block.fill_amount = 4096;
			driver.fail_next_map = true;
		}
		~Fixture() {
			for (RID mm : storage->multimesh_owner.get_owned_list()) {
				storage->multimesh_free(mm);
			}
			storage->mesh_free(mesh);
			dispose();
			CHECK(device.storage_buffer_owner.get_rid_count() == 0);
			CHECK(storage->multimesh_owner.get_rid_count() == 0);
			CHECK(driver.live_gpu_count() == 0);
			CHECK(device.buffer_memory == 0);
			driver.buffer_free(worker->staging_buffer);
			memdelete(worker);
			device.transfer_worker_pool.clear();
			device.transfer_worker_pool_available_list.clear();
			device.transfer_worker_operation_used_by_draw.clear();
			device.transfer_worker_pool_size = 0;
			for (auto &block : device.upload_staging_buffers.blocks) {
				driver.buffer_unmap(block.driver_id);
				driver.buffer_free(block.driver_id);
			}
			device.upload_staging_buffers.blocks.clear();
			for (uint32_t id = 1; id <= driver.allocations; ++id) {
				CHECK_FALSE(driver.live[id]);
				CHECK(driver.returns[id] == 1);
			}
			memdelete(storage);
			MeshStorage::singleton = previous_storage;
			device.frames.clear();
			device.driver = nullptr;
			device.singleton = previous_device;
			device.native_finalization_returned = true;
		}
	};

public:
	static uint32_t upload_size(uint32_t p_regions, uint32_t p_region_bytes, uint32_t p_buffer_bytes) {
		return MeshStorage::_multimesh_dirty_upload_size(p_regions, p_region_bytes, p_buffer_bytes);
	}
	static void disposal_cycles() {
		Fixture f;
		uint64_t commands[32] = {};
		for (uint32_t cycle = 0; cycle < 32; ++cycle) {
			const RID mm = f.create();
			f.command(mm, 3);
			const RID command = f.storage->multimesh_get_command_buffer_rd_rid(mm);
			const uint64_t id = f.driver_id(command);
			for (uint32_t previous = 0; previous < cycle; ++previous) {
				CHECK(commands[previous] != id);
			}
			commands[cycle] = id;
			CHECK(f.driver.returns[id] == 0);
			f.storage->multimesh_free(mm);
			CHECK_FALSE(f.device.storage_buffer_owner.owns(command));
			// RID invalidation is specifically insufficient: native ownership is
			// still live until the original RD deferred-disposal path runs.
			CHECK(f.driver.live[id]);
			CHECK(f.driver.returns[id] == 0);
			CHECK(f.device.frames[0].buffers_to_dispose_of.size() == 2);
			f.dispose();
			CHECK_FALSE(f.driver.live[id]);
			CHECK(f.driver.returns[id] == 1);
			CHECK(f.driver.live_gpu_count() == 0);
			f.dispose();
			CHECK(f.driver.returns[id] == 1);
		}
		uint32_t returned_commands = 0;
		for (uint64_t id : commands) {
			returned_commands += f.driver.returns[id];
		}
		CHECK(returned_commands == 32);
		CHECK(f.device.storage_buffer_owner.get_rid_count() == 0);
	}
	static void failure_retry(uint32_t p_failure) {
		const bool update = p_failure == 2;
		Fixture f;
		const RID mm = f.create();
		f.command(mm, 3);
		const RID old_command = f.storage->multimesh_get_command_buffer_rd_rid(mm);
		const uint64_t old_native = f.driver_id(old_command);
		Notifications notifications;
		DependencyTracker tracker;
		tracker.userdata = &notifications;
		tracker.changed_callback = &Notifications::changed;
		tracker.deleted_callback = &Notifications::removed;
		tracker.update_dependency(&f.storage->multimesh_owner.get_or_null(mm)->dependency);
		if (p_failure < 2) {
			f.driver.fail_gpu_attempt = f.driver.gpu_attempts + 1 + p_failure;
			ERR_PRINT_OFF;
			f.storage->multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			ERR_PRINT_ON;
			CHECK(f.driver.allocation_failures == 1);
		} else if (update) {
			f.fail_update_map();
			ERR_PRINT_OFF;
			f.storage->multimesh_set_visible_instances(mm, 1);
			ERR_PRINT_ON;
			CHECK(f.driver.map_failures == 1);
			// The failed staging block is a real native prefix, returned by
			// _insert_staging_block before the old command's deferred free.
			CHECK_FALSE(f.driver.gpu[f.driver.allocations]);
			CHECK_FALSE(f.driver.live[f.driver.allocations]);
			CHECK(f.driver.returns[f.driver.allocations] == 1);
			CHECK(notifications.visible == 1);
		} else {
			// Instance allocation succeeds; initialization of the new command
			// fails in the original transfer worker's actual driver map.
			f.driver.fail_next_map = true;
			ERR_PRINT_OFF;
			f.storage->multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			ERR_PRINT_ON;
			CHECK(f.driver.map_failures == 1);
			CHECK(f.driver.gpu[f.driver.allocations]);
			CHECK_FALSE(f.driver.live[f.driver.allocations]);
			CHECK(f.driver.returns[f.driver.allocations] == 1);
		}
		CHECK(notifications.invalidations == 1);
		CHECK(f.storage->multimesh_get_command_buffer_rd_rid(mm).is_null());
		CHECK_FALSE(f.device.storage_buffer_owner.owns(old_command));
		CHECK(f.driver.live[old_native]);
		CHECK(f.driver.returns[old_native] == 0);
		f.dispose();
		CHECK_FALSE(f.driver.live[old_native]);
		CHECK(f.driver.returns[old_native] == 1);
		CHECK(f.driver.live_gpu_count() == (p_failure == 0 ? 0 : 1));
		// Replay the exact same public operation. It must restore a new valid
		// current command even when the allocation/visible metadata is equal.
		if (!update) {
			f.storage->multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
		} else {
			f.storage->multimesh_set_visible_instances(mm, 1);
		}
		f.command(mm, update ? 1 : 5);
		const RID restored = f.storage->multimesh_get_command_buffer_rd_rid(mm);
		CHECK(restored != old_command);
		CHECK(f.driver_id(restored) != old_native);
		CHECK(f.driver.live_gpu_count() == 2);
		CHECK(notifications.invalidations == 2);
		f.storage->multimesh_free(mm);
		CHECK(notifications.deleted == 1);
		f.dispose();
		CHECK(f.driver.live_gpu_count() == 0);
		CHECK(f.driver.returns[old_native] == 1);
	}
};
} //namespace RendererRD

TEST_CASE("[B4][MultiMesh] rounded dirty uploads cannot wrap into a zero byte success") {
	using Access = RendererRD::TestMultimeshRebuildAccess;
	// A legal 134217727-instance 2D allocation is 4294967264 bytes.
	// Its 262144 rounded regions span exactly 2^32 bytes before clamping.
	CHECK(Access::upload_size(262144, 16384, 4294967264u) == 4294967264u);
	CHECK(Access::upload_size(2, 24576, 30000) == 30000);
	CHECK(Access::upload_size(1, 24576, 49152) == 24576);
	CHECK(Access::upload_size(0, 24576, 49152) == 0);
}

TEST_CASE("[B4][MultiMesh] allocation preflight bounds the complete buffer before mutation") {
	struct Case {
		int instances;
		RSE::MultimeshTransformFormat format;
		bool colors;
		bool custom;
		bool motion;
		Error error;
		uint32_t bytes;
	};
	const Case cases[] = {
		{ 0, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 0 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 96 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, true, true, false, OK, 192 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 240 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 480 },
		{ -1, RSE::MULTIMESH_TRANSFORM_3D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 0, RSE::MultimeshTransformFormat(99), false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 134217727, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 4294967264u },
		{ 134217728, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 53687091, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 4294967280u },
		{ 53687092, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, ERR_INVALID_PARAMETER, 17 },
		{ 26843545, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 4294967200u },
		{ 26843546, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, ERR_INVALID_PARAMETER, 17 },
	};
	for (const Case &test : cases) {
		uint32_t bytes = 17;
		CHECK(RendererMeshStorage::_multimesh_buffer_size(test.instances, test.format, test.colors, test.custom, test.motion, bytes) == test.error);
		CHECK(bytes == test.bytes);
	}
}

TEST_CASE("[B4][MultiMesh][M03] 32 original command owners return exactly once through native deferred free") {
	RendererRD::TestMultimeshRebuildAccess::disposal_cycles();
}

TEST_CASE("[B4][MultiMesh][M11] failed native instance allocation removes stale command and retries equal shape") {
	RendererRD::TestMultimeshRebuildAccess::failure_retry(0);
}

TEST_CASE("[B4][MultiMesh][M11] failed native command allocation removes stale command and retries equal shape") {
	RendererRD::TestMultimeshRebuildAccess::failure_retry(1);
}

TEST_CASE("[B4][MultiMesh][M11] failed native update staging removes stale command and retries equal visible count") {
	RendererRD::TestMultimeshRebuildAccess::failure_retry(2);
}

TEST_CASE("[B4][MultiMesh][M11] failed native command initialization returns its prefix and retries equal shape") {
	RendererRD::TestMultimeshRebuildAccess::failure_retry(3);
}
