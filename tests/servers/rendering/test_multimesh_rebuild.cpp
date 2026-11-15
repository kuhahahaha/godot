#include "core/io/marshalls.h"
#include "servers/rendering/dummy/storage/mesh_storage.h"
#include "servers/rendering/dummy/storage/particles_storage.h"
#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_device_driver_metal3.h"
#endif

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

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	static MeshStorage::MultiMesh *get_multimesh(MeshStorage &p_storage, RID p_mm) {
		return p_storage.multimesh_owner.get_or_null(p_mm);
	}

	static uint32_t live_multimesh_count(MeshStorage &p_storage) {
		return p_storage.multimesh_owner.get_owned_list().size();
	}

	static bool dirty_list_empty(MeshStorage &p_storage) {
		return p_storage.multimesh_dirty_list == nullptr;
	}

	static void flush_dirty(MeshStorage &p_storage) {
		p_storage._update_dirty_multimeshes();
	}

	static RID skeleton_shader_rd(MeshStorage &p_storage) {
		return p_storage.skeleton_shader.version_shader[0];
	}

	static uint32_t skeleton_uniform_set_index() {
		return MeshStorage::SkeletonShader::UNIFORM_SET_SKELETON;
	}

	static void reset_faults(MeshStorage &p_storage) {
		p_storage.multimesh_test_faults = MeshStorage::MultiMeshTestFaultState();
	}

	static void set_motion_vectors(MeshStorage &p_storage, bool p_enabled, uint32_t p_frame) {
		p_storage.multimesh_test_faults.force_motion_vectors = p_enabled;
		p_storage.multimesh_test_faults.override_frame_number = p_enabled;
		p_storage.multimesh_test_faults.frame_number = p_frame;
	}

	static void fail_instance_buffer_create(MeshStorage &p_storage, uint32_t p_after = 1) {
		p_storage.multimesh_test_faults.fail_instance_buffer_create_after = p_after;
	}

	static void fail_command_buffer_create(MeshStorage &p_storage, uint32_t p_after = 1) {
		p_storage.multimesh_test_faults.fail_command_buffer_create_after = p_after;
	}

	static void fail_buffer_update(MeshStorage &p_storage, uint32_t p_after = 1) {
		p_storage.multimesh_test_faults.fail_buffer_update_after = p_after;
	}

	static void fail_buffer_copy(MeshStorage &p_storage, uint32_t p_after = 1) {
		p_storage.multimesh_test_faults.fail_buffer_copy_after = p_after;
	}

	static void fail_buffer_readback(MeshStorage &p_storage, uint32_t p_after = 1, int64_t p_truncate_bytes = -1) {
		p_storage.multimesh_test_faults.fail_buffer_readback_after = p_after;
		p_storage.multimesh_test_faults.readback_truncate_bytes = p_truncate_bytes;
	}

	static void fail_uniform_set_create(MeshStorage &p_storage, uint32_t p_after = 1) {
		p_storage.multimesh_test_faults.fail_uniform_set_create_after = p_after;
	}
#endif
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

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
namespace {

struct MultiMeshDependencySpy {
	uint32_t multimesh_changed = 0;
	uint32_t visible_changed = 0;
	uint32_t mesh_changed = 0;
	uint32_t deleted = 0;
	RendererRD::MeshStorage *reentrant_storage = nullptr;
	RID reentrant_multimesh;
	Transform3D reentrant_transform = Transform3D(Basis(), Vector3(77.0f, 88.0f, 99.0f));
	DependencyTracker tracker;

	static void _changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
		MultiMeshDependencySpy *self = static_cast<MultiMeshDependencySpy *>(p_tracker->userdata);
		if (p_notification == Dependency::DEPENDENCY_CHANGED_MULTIMESH) {
			self->multimesh_changed++;
			if (self->reentrant_storage != nullptr && self->reentrant_multimesh.is_valid()) {
				RID target = self->reentrant_multimesh;
				self->reentrant_multimesh = RID();
				self->reentrant_storage->multimesh_instance_set_transform(target, 0, self->reentrant_transform);
			}
		} else if (p_notification == Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES) {
			self->visible_changed++;
		} else if (p_notification == Dependency::DEPENDENCY_CHANGED_MESH) {
			self->mesh_changed++;
		}
	}

	static void _deleted(const RID &, DependencyTracker *p_tracker) {
		MultiMeshDependencySpy *self = static_cast<MultiMeshDependencySpy *>(p_tracker->userdata);
		self->deleted++;
	}

	MultiMeshDependencySpy() {
		tracker.userdata = this;
		tracker.changed_callback = _changed;
		tracker.deleted_callback = _deleted;
	}

	void track(Dependency *p_dependency) {
		tracker.update_begin();
		tracker.update_dependency(p_dependency);
		tracker.update_end();
	}
};

static RenderingServerTypes::SurfaceData make_test_surface(uint32_t p_vertex_count, uint32_t p_index_count, const AABB &p_aabb) {
	RenderingServerTypes::SurfaceData sd;
	sd.primitive = RSE::PRIMITIVE_TRIANGLES;
	sd.format = RSE::ARRAY_FORMAT_VERTEX;
	sd.vertex_count = p_vertex_count;
	sd.aabb = p_aabb;

	sd.vertex_data.resize(p_vertex_count * sizeof(float) * 3);
	{
		float *verts = reinterpret_cast<float *>(sd.vertex_data.ptrw());
		for (uint32_t i = 0; i < p_vertex_count; i++) {
			const float t = (p_vertex_count > 1) ? float(i) / float(p_vertex_count - 1) : 0.0f;
			verts[i * 3 + 0] = p_aabb.position.x + p_aabb.size.x * t;
			verts[i * 3 + 1] = p_aabb.position.y + p_aabb.size.y * ((i & 1) ? 1.0f : 0.0f);
			verts[i * 3 + 2] = p_aabb.position.z + p_aabb.size.z * ((i & 2) ? 1.0f : 0.0f);
		}
	}

	if (p_index_count > 0) {
		sd.format |= RSE::ARRAY_FORMAT_INDEX;
		sd.index_count = p_index_count;
		sd.index_data.resize(p_index_count * sizeof(uint16_t));
		uint16_t *indices = reinterpret_cast<uint16_t *>(sd.index_data.ptrw());
		for (uint32_t i = 0; i < p_index_count; i++) {
			indices[i] = uint16_t(i % p_vertex_count);
		}
	}

	return sd;
}

static Vector<float> make_3d_buffer(int p_instances, float p_base_x, bool p_colors = false, bool p_custom = false) {
	const int stride = 12 + (p_colors ? 4 : 0) + (p_custom ? 4 : 0);
	Vector<float> data;
	data.resize(p_instances * stride);
	float *ptr = data.ptrw();
	for (int i = 0; i < data.size(); i++) {
		ptr[i] = 0.0f;
	}
	for (int i = 0; i < p_instances; i++) {
		float *dst = ptr + i * stride;
		dst[0] = 1.0f;
		dst[5] = 1.0f;
		dst[10] = 1.0f;
		dst[3] = p_base_x + float(i) * 2.0f;
		dst[7] = float(i);
		dst[11] = -float(i);
		int offset = 12;
		if (p_colors) {
			dst[offset + 0] = 0.25f;
			dst[offset + 1] = 0.5f;
			dst[offset + 2] = 0.75f;
			dst[offset + 3] = 1.0f;
			offset += 4;
		}
		if (p_custom) {
			dst[offset + 0] = 1.0f;
			dst[offset + 1] = 2.0f;
			dst[offset + 2] = 3.0f;
			dst[offset + 3] = 4.0f;
		}
	}
	return data;
}

static Vector<float> make_2d_buffer(int p_instances, float p_base_x, bool p_colors = false, bool p_custom = false) {
	const int stride = 8 + (p_colors ? 4 : 0) + (p_custom ? 4 : 0);
	Vector<float> data;
	data.resize(p_instances * stride);
	float *ptr = data.ptrw();
	for (int i = 0; i < data.size(); i++) {
		ptr[i] = 0.0f;
	}
	for (int i = 0; i < p_instances; i++) {
		float *dst = ptr + i * stride;
		dst[0] = 1.0f;
		dst[5] = 1.0f;
		dst[3] = p_base_x + float(i) * 3.0f;
		dst[7] = float(i) * 1.5f;
	}
	return data;
}

static Vector<uint32_t> read_command_words(RenderingDevice &p_rd, RID p_cmd) {
	Vector<uint32_t> words;
	if (!p_cmd.is_valid()) {
		return words;
	}
	Vector<uint8_t> bytes = p_rd.buffer_get_data(p_cmd);
	REQUIRE((bytes.size() % sizeof(uint32_t)) == 0);
	words.resize(bytes.size() / sizeof(uint32_t));
	for (int i = 0; i < words.size(); i++) {
		words.write[i] = decode_uint32(bytes.ptr() + i * sizeof(uint32_t));
	}
	return words;
}

static RID create_initialized_mesh(RendererRD::MeshStorage &p_storage) {
	RID mesh = p_storage.mesh_allocate();
	p_storage.mesh_initialize(mesh);
	return mesh;
}

static RID create_initialized_multimesh(RendererRD::MeshStorage &p_storage) {
	RID mm = p_storage.multimesh_allocate();
	p_storage.multimesh_initialize(mm);
	return mm;
}

} // namespace

TEST_CASE("[B4][MultiMesh][M01-M10] indirect transitions, capacity/layout rebuilds, surface mutations, sentinel visible=-1, zero visible, and motion vectors on native RD") {
	using Access = RendererRD::TestMultimeshRebuildAccess;

	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE_MESSAGE(available_device.get() != nullptr, "Native Metal device unavailable on macOS host.");

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);

	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	{
		RendererRD::MeshStorage storage;
		const RID shader_rd = Access::skeleton_shader_rd(storage);
		const uint32_t uset_idx = Access::skeleton_uniform_set_index();

		// R-M01 + M01 + M02: 5-field early-return predicate and direct <-> indirect transitions.
		{
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(-1, -1, 0), Vector3(2, 2, 1))));

			RID mm = create_initialized_multimesh(storage);
			MultiMeshDependencySpy spy;
			spy.track(storage.multimesh_get_dependency(mm));

			storage.multimesh_allocate_data(mm, 4, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
			storage.multimesh_set_mesh(mm, mesh);
			storage.multimesh_set_buffer(mm, make_3d_buffer(4, 1.0f));

			// Negative control for R-M01: a 4-field predicate falsely treats false->true as unchanged.
			const bool legacy_four_field_skip = (storage.multimesh_get_instance_count(mm) == 4 &&
					storage.multimesh_get_transform_format(mm) == RSE::MULTIMESH_TRANSFORM_3D &&
					storage.multimesh_uses_colors(mm) == false &&
					storage.multimesh_uses_custom_data(mm) == false);
			CHECK(legacy_four_field_skip);
			CHECK_FALSE(storage.multimesh_uses_indirect(mm));
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());

			const uint32_t notify_before_m01 = spy.multimesh_changed;
			storage.multimesh_allocate_data(mm, 4, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			CHECK(storage.multimesh_uses_indirect(mm));
			CHECK(spy.multimesh_changed > notify_before_m01);
			RID cmd = storage._multimesh_get_command_buffer_rd_rid(mm);
			REQUIRE(cmd.is_valid());
			Vector<uint32_t> words = read_command_words(rd, cmd);
			REQUIRE(words.size() == 5);
			CHECK(words[0] == 6);
			CHECK(words[1] == 4);
			CHECK(words[2] == 0);
			CHECK(words[3] == 0);
			CHECK(words[4] == 0);

			// M02: indirect true -> false retires command_buffer while keeping direct buffer valid.
			const uint32_t notify_before_m02 = spy.multimesh_changed;
			storage.multimesh_allocate_data(mm, 4, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
			CHECK_FALSE(storage.multimesh_uses_indirect(mm));
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_valid());
			CHECK(spy.multimesh_changed > notify_before_m02);

			storage.multimesh_free(mm);
			storage.mesh_free(mesh);
		}

		// M03: 32 allocate/free cycles return RD buffer memory to exact baseline after frame retirement.
		{
			rd.submit();
			rd.sync();
			const uint64_t baseline_buffer_bytes = rd.get_memory_usage(RD::MEMORY_BUFFERS);
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));
			storage.mesh_add_surface(mesh, make_test_surface(6, 12, AABB(Vector3(2, 0, 0), Vector3(1, 1, 1))));
			const uint64_t mesh_baseline_bytes = rd.get_memory_usage(RD::MEMORY_BUFFERS);

			for (int cycle = 0; cycle < 32; cycle++) {
				RID mm = create_initialized_multimesh(storage);
				storage.multimesh_allocate_data(mm, 128, RSE::MULTIMESH_TRANSFORM_3D, true, true, true);
				storage.multimesh_set_mesh(mm, mesh);
				storage.multimesh_set_buffer(mm, make_3d_buffer(128, float(cycle), true, true));
				RID uset = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
				REQUIRE(uset.is_valid());
				REQUIRE(storage._multimesh_get_command_buffer_rd_rid(mm).is_valid());
				CHECK(rd.get_memory_usage(RD::MEMORY_BUFFERS) > mesh_baseline_bytes);
				storage.multimesh_free(mm);
				CHECK_FALSE(rd.uniform_set_is_valid(uset));
				rd.submit();
				rd.sync();
				CHECK(rd.get_memory_usage(RD::MEMORY_BUFFERS) == mesh_baseline_bytes);
			}

			storage.mesh_free(mesh);
			rd.submit();
			rd.sync();
			CHECK(rd.get_memory_usage(RD::MEMORY_BUFFERS) == baseline_buffer_bytes);
			CHECK(Access::live_multimesh_count(storage) == 0);
			CHECK(Access::dirty_list_empty(storage));
		}

		// M04 + M10: same-mesh capacity resize and 3D(colors+custom) -> 2D(plain) layout rebuild.
		{
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));
			storage.mesh_add_surface(mesh, make_test_surface(6, 12, AABB(Vector3(3, 0, 0), Vector3(1, 1, 1))));

			RID mm = create_initialized_multimesh(storage);
			storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, true);
			storage.multimesh_set_mesh(mm, mesh);
			storage.multimesh_set_buffer(mm, make_3d_buffer(3, 0.0f, true, true));
			RID old_buf = storage._multimesh_get_buffer_rd_rid(mm);
			RID old_cmd = storage._multimesh_get_command_buffer_rd_rid(mm);
			RID old_uset = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			REQUIRE(old_buf.is_valid());
			REQUIRE(old_cmd.is_valid());
			REQUIRE(old_uset.is_valid());

			Vector<uint32_t> words3 = read_command_words(rd, old_cmd);
			REQUIRE(words3.size() == 10);
			CHECK(words3[0] == 6);
			CHECK(words3[1] == 3);
			CHECK(words3[5] == 12);
			CHECK(words3[6] == 3);

			// M04: resize 3 -> 7 on the identical mesh RID.
			storage.multimesh_allocate_data(mm, 7, RSE::MULTIMESH_TRANSFORM_3D, true, true, true);
			RID resized_buf = storage._multimesh_get_buffer_rd_rid(mm);
			RID resized_cmd = storage._multimesh_get_command_buffer_rd_rid(mm);
			CHECK(resized_buf.is_valid());
			CHECK(resized_buf != old_buf);
			CHECK(resized_cmd.is_valid());
			CHECK(resized_cmd != old_cmd);
			CHECK_FALSE(rd.uniform_set_is_valid(old_uset));
			Vector<uint32_t> words7 = read_command_words(rd, resized_cmd);
			REQUIRE(words7.size() == 10);
			CHECK(words7[0] == 6);
			CHECK(words7[1] == 7);
			CHECK(words7[5] == 12);
			CHECK(words7[6] == 7);

			// M10: 3D + color + custom (stride 20) -> 2D plain (stride 8) on the same mesh.
			RID uset_7 = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			REQUIRE(uset_7.is_valid());
			storage.multimesh_allocate_data(mm, 4, RSE::MULTIMESH_TRANSFORM_2D, false, false, true);
			storage.multimesh_set_buffer(mm, make_2d_buffer(4, 5.0f));
			CHECK_FALSE(rd.uniform_set_is_valid(uset_7));
			CHECK(Access::get_multimesh(storage, mm)->stride_cache == 8);
			CHECK(rd.buffer_get_data(storage._multimesh_get_buffer_rd_rid(mm)).size() == 4 * 8 * sizeof(float));
			RID uset_2d = storage.multimesh_get_2d_uniform_set(mm, shader_rd, uset_idx);
			CHECK(uset_2d.is_valid());
			Vector<uint32_t> words2d = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(words2d.size() == 10);
			CHECK(words2d[0] == 6);
			CHECK(words2d[1] == 4);
			CHECK(words2d[5] == 12);
			CHECK(words2d[6] == 4);

			storage.multimesh_free(mm);
			storage.mesh_free(mesh);
		}

		// M05 + M06: null/empty mesh transitions (both cached and GPU-only) and in-place surface remove/add/clear.
		{
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));
			storage.mesh_add_surface(mesh, make_test_surface(6, 12, AABB(Vector3(5, 0, 0), Vector3(2, 1, 1))));
			RID empty_mesh = create_initialized_mesh(storage);

			RID mm = create_initialized_multimesh(storage);
			storage.multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_2D, false, false, true);
			storage.multimesh_set_mesh(mm, mesh);
			Vector<float> buf2d = make_2d_buffer(2, 10.0f); // Instance x = 10, 13; y = 0, 1.5
			storage.multimesh_set_buffer(mm, buf2d);

			AABB initial_aabb = storage.multimesh_get_aabb(mm);
			CHECK(initial_aabb.position.is_equal_approx(Vector3(10.0f, 0.0f, 0.0f)));
			CHECK(initial_aabb.size.is_equal_approx(Vector3(10.0f, 2.5f, 1.0f)));
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 40);

			// M05: GPU-only (no data_cache) mesh -> RID() must reset AABB without error or stale bounds, then -> empty_mesh.
			storage.multimesh_set_mesh(mm, RID());
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			CHECK(storage.multimesh_get_aabb(mm) == AABB());

			storage.multimesh_set_mesh(mm, empty_mesh);
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			AABB empty_aabb = storage.multimesh_get_aabb(mm);
			CHECK(empty_aabb.position.is_equal_approx(Vector3(10.0f, 0.0f, 0.0f)));
			CHECK(empty_aabb.size.is_equal_approx(Vector3(3.0f, 1.5f, 0.0f)));

			// M06: reattach 2-surface mesh, then in-place mesh_surface_remove -> mesh_add_surface -> mesh_clear.
			storage.multimesh_set_mesh(mm, mesh);
			REQUIRE(Access::get_multimesh(storage, mm)->command_buffer_bytes == 40);

			storage.mesh_surface_remove(mesh, 0);
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 20);
			Vector<uint32_t> after_remove = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(after_remove.size() == 5);
			CHECK(after_remove[0] == 12);
			CHECK(after_remove[1] == 2);
			AABB after_remove_aabb = storage.multimesh_get_aabb(mm);
			CHECK(after_remove_aabb.position.is_equal_approx(Vector3(15.0f, 0.0f, 0.0f)));
			CHECK(after_remove_aabb.size.is_equal_approx(Vector3(5.0f, 2.5f, 1.0f)));

			storage.mesh_add_surface(mesh, make_test_surface(9, 0, AABB(Vector3(-2, 0, 0), Vector3(1, 1, 1))));
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 40);
			Vector<uint32_t> after_add = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(after_add.size() == 10);
			CHECK(after_add[0] == 12);
			CHECK(after_add[1] == 2);
			CHECK(after_add[5] == 9);
			CHECK(after_add[6] == 2);

			storage.mesh_clear(mesh);
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			AABB after_clear_aabb = storage.multimesh_get_aabb(mm);
			CHECK(after_clear_aabb.position.is_equal_approx(Vector3(10.0f, 0.0f, 0.0f)));
			CHECK(after_clear_aabb.size.is_equal_approx(Vector3(3.0f, 1.5f, 0.0f)));

			storage.multimesh_free(mm);
			storage.mesh_free(empty_mesh);
			storage.mesh_free(mesh);
		}

		// M07 + M08: sentinel visible_instances = -1, zero visible command buffer validity, and hidden cached dirty upload.
		{
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));

			RID mm = create_initialized_multimesh(storage);
			storage.multimesh_allocate_data(mm, 600, RSE::MULTIMESH_TRANSFORM_2D, false, false, true);
			storage.multimesh_set_mesh(mm, mesh);
			for (int i = 0; i < 600; i++) {
				storage.multimesh_instance_set_transform_2d(mm, i, Transform2D(0.0f, Vector2(float(i), 1.0f)));
			}
			Access::flush_dirty(storage);

			// M07: visible = 1 -> -1 uses resolved capacity 600 (never UINT32_MAX), and preserves hidden dirty region >512 across flushes.
			storage.multimesh_set_visible_instances(mm, 1);
			CHECK(storage.multimesh_get_instances_to_draw(mm) == 1);
			Vector<uint32_t> vis1 = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(vis1.size() == 5);
			CHECK(vis1[1] == 1);

			storage.multimesh_instance_set_transform_2d(mm, 550, Transform2D(0.0f, Vector2(987.5f, -42.0f)));
			Access::flush_dirty(storage);
			Access::flush_dirty(storage);

			storage.multimesh_set_visible_instances(mm, -1);
			CHECK(storage.multimesh_get_instances_to_draw(mm) == 600);
			Vector<uint32_t> vis_all = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(vis_all.size() == 5);
			CHECK(vis_all[1] == 600);
			CHECK(vis_all[1] != UINT32_MAX);

			Access::flush_dirty(storage);
			Vector<uint8_t> gpu_bytes = rd.buffer_get_data(storage._multimesh_get_buffer_rd_rid(mm));
			const float *gpu_floats = reinterpret_cast<const float *>(gpu_bytes.ptr());
			CHECK(Math::is_equal_approx(gpu_floats[550 * 8 + 3], 987.5f));
			CHECK(Math::is_equal_approx(gpu_floats[550 * 8 + 7], -42.0f));

			// M08: visible = 0 keeps a valid zero-instance indirect command buffer, and visible = 7 restores drawing.
			storage.multimesh_set_visible_instances(mm, 0);
			CHECK(storage.multimesh_get_instances_to_draw(mm) == 0);
			RID zero_cmd = storage._multimesh_get_command_buffer_rd_rid(mm);
			REQUIRE(zero_cmd.is_valid());
			Vector<uint32_t> vis0 = read_command_words(rd, zero_cmd);
			REQUIRE(vis0.size() == 5);
			CHECK(vis0[0] == 6);
			CHECK(vis0[1] == 0);

			storage.multimesh_set_visible_instances(mm, 7);
			CHECK(storage.multimesh_get_instances_to_draw(mm) == 7);
			Vector<uint32_t> vis7 = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(vis7.size() == 5);
			CHECK(vis7[0] == 6);
			CHECK(vis7[1] == 7);

			storage.multimesh_free(mm);
			storage.mesh_free(mesh);
		}

		// M09: motion-vector buffer doubling, current/previous half rotation, uniform-set invalidation, and same-mesh resize.
		{
			RID mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));

			RID mm = create_initialized_multimesh(storage);
			MultiMeshDependencySpy spy;
			spy.track(storage.multimesh_get_dependency(mm));

			storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			storage.multimesh_set_mesh(mm, mesh);
			Vector<float> frame0 = make_3d_buffer(3, 1.0f);
			storage.multimesh_set_buffer(mm, frame0);
			RID buf_before = storage._multimesh_get_buffer_rd_rid(mm);
			RID uset_before = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			REQUIRE(uset_before.is_valid());

			Access::set_motion_vectors(storage, true, 10);
			const uint32_t notify_before = spy.multimesh_changed;
			Vector<float> frame10 = make_3d_buffer(3, 20.0f);
			storage.multimesh_set_buffer(mm, frame10);

			RID buf_motion = storage._multimesh_get_buffer_rd_rid(mm);
			CHECK(buf_motion.is_valid());
			CHECK(buf_motion != buf_before);
			CHECK_FALSE(rd.uniform_set_is_valid(uset_before));
			CHECK(spy.multimesh_changed > notify_before);
			RID uset_motion = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			CHECK(uset_motion.is_valid());

			Vector<uint8_t> doubled_bytes = rd.buffer_get_data(buf_motion);
			REQUIRE(doubled_bytes.size() == 3 * 12 * sizeof(float) * 2);
			const float *doubled_floats = reinterpret_cast<const float *>(doubled_bytes.ptr());
			CHECK(Math::is_equal_approx(doubled_floats[3], 20.0f));
			CHECK(Math::is_equal_approx(doubled_floats[36 + 3], 20.0f));

			// Frame 11 dirty transform update rotates offsets and preserves Frame 10 in previous half.
			Access::set_motion_vectors(storage, true, 11);
			storage.multimesh_instance_set_transform(mm, 0, Transform3D(Basis(), Vector3(99.0f, 0.0f, 0.0f)));
			Access::flush_dirty(storage);

			uint32_t cur_off = 0;
			uint32_t prev_off = 0;
			storage._multimesh_get_motion_vectors_offsets(mm, cur_off, prev_off);
			CHECK(cur_off != prev_off);
			Vector<uint8_t> rotated_bytes = rd.buffer_get_data(storage._multimesh_get_buffer_rd_rid(mm));
			const float *rotated_floats = reinterpret_cast<const float *>(rotated_bytes.ptr());
			CHECK(Math::is_equal_approx(rotated_floats[cur_off * 12 + 3], 99.0f));
			CHECK(Math::is_equal_approx(rotated_floats[prev_off * 12 + 3], 20.0f));

			// Resize 3 -> 5 while motion vectors remain active.
			storage.multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			Vector<float> frame_resized = make_3d_buffer(5, 50.0f);
			storage.multimesh_set_buffer(mm, frame_resized);
			CHECK(rd.buffer_get_data(storage._multimesh_get_buffer_rd_rid(mm)).size() == 5 * 12 * sizeof(float) * 2);
			Vector<uint32_t> resized_cmd = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(resized_cmd.size() == 5);
			CHECK(resized_cmd[0] == 6);
			CHECK(resized_cmd[1] == 5);

			Access::reset_faults(storage);
			storage.multimesh_free(mm);
			storage.mesh_free(mesh);
		}
	}

	rd.finalize();
}

TEST_CASE("[B4][MultiMesh][M11] deterministic fault injection fails closed without leaks or stale commands and recovers on retry") {
	using Access = RendererRD::TestMultimeshRebuildAccess;

	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE_MESSAGE(available_device.get() != nullptr, "Native Metal device unavailable on macOS host.");

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);

	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	{
		RendererRD::MeshStorage storage;
		const RID shader_rd = Access::skeleton_shader_rd(storage);
		const uint32_t uset_idx = Access::skeleton_uniform_set_index();

		RID mesh2 = create_initialized_mesh(storage);
		storage.mesh_add_surface(mesh2, make_test_surface(4, 6, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));
		storage.mesh_add_surface(mesh2, make_test_surface(6, 12, AABB(Vector3(2, 0, 0), Vector3(1, 1, 1))));
		rd.submit();
		rd.sync();
		const uint64_t baseline_with_mesh = rd.get_memory_usage(RD::MEMORY_BUFFERS);

		// 1. Preflight invalid enum and byte-overflow preserve existing interpolator + GPU state untouched.
		{
			RID mm = create_initialized_multimesh(storage);
			storage.multimesh_set_physics_interpolated(mm, true);
			storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			storage.multimesh_set_mesh(mm, mesh2);
			storage.multimesh_set_buffer(mm, make_3d_buffer(3, 4.0f));
			RID orig_buf = storage._multimesh_get_buffer_rd_rid(mm);
			RID orig_cmd = storage._multimesh_get_command_buffer_rd_rid(mm);

			ERR_PRINT_OFF;
			storage.multimesh_allocate_data(mm, 3, RSE::MultimeshTransformFormat(99), false, false, true);
			storage.multimesh_allocate_data(mm, 53687092, RSE::MULTIMESH_TRANSFORM_3D, true, true, false);
			ERR_PRINT_ON;

			CHECK(storage.multimesh_get_instance_count(mm) == 3);
			CHECK(storage._multimesh_get_buffer_rd_rid(mm) == orig_buf);
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm) == orig_cmd);
			CHECK(storage.multimesh_get_buffer(mm).size() == 36);

			storage.multimesh_free(mm);
		}

		// 2. Instance buffer allocation failure -> fail-closed + dependency notify + identical-5-field retry recovery.
		{
			RID mm = create_initialized_multimesh(storage);
			MultiMeshDependencySpy spy;
			spy.track(storage.multimesh_get_dependency(mm));

			storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			storage.multimesh_set_mesh(mm, mesh2);
			RID uset = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			REQUIRE(uset.is_valid());

			const uint32_t before = spy.multimesh_changed;
			Access::fail_instance_buffer_create(storage, 1);
			ERR_PRINT_OFF;
			storage.multimesh_allocate_data(mm, 6, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			ERR_PRINT_ON;

			CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			CHECK_FALSE(rd.uniform_set_is_valid(uset));
			CHECK(storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx).is_null());
			CHECK(spy.multimesh_changed > before);

			// Retry with the exact same 5-field signature must not early-return on a failed buffer!
			storage.multimesh_allocate_data(mm, 6, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_valid());
			RID recovered_cmd = storage._multimesh_get_command_buffer_rd_rid(mm);
			REQUIRE(recovered_cmd.is_valid());
			Vector<uint32_t> words = read_command_words(rd, recovered_cmd);
			REQUIRE(words.size() == 10);
			CHECK(words[1] == 6);
			CHECK(words[6] == 6);

			storage.multimesh_free(mm);
		}

		// 3. Command buffer create failure & 2nd-surface visible_instances update failure + same-arg retries.
		{
			RID mm = create_initialized_multimesh(storage);
			MultiMeshDependencySpy spy;
			spy.track(storage.multimesh_get_dependency(mm));

			storage.multimesh_set_mesh(mm, mesh2);
			Access::fail_command_buffer_create(storage, 1);
			ERR_PRINT_OFF;
			storage.multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			ERR_PRINT_ON;

			RID preserved_buf = storage._multimesh_get_buffer_rd_rid(mm);
			REQUIRE(preserved_buf.is_valid());
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());

			// Same-5-field allocate retry rebuilds only the missing command_buffer without reallocating buffer.
			storage.multimesh_allocate_data(mm, 5, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			CHECK(storage._multimesh_get_buffer_rd_rid(mm) == preserved_buf);
			REQUIRE(storage._multimesh_get_command_buffer_rd_rid(mm).is_valid());

			// Fail command_buffer_create inside multimesh_set_mesh, then recover via same-mesh retry.
			RID alt_mesh = create_initialized_mesh(storage);
			storage.mesh_add_surface(alt_mesh, make_test_surface(3, 3, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))));
			Access::fail_command_buffer_create(storage, 1);
			ERR_PRINT_OFF;
			storage.multimesh_set_mesh(mm, alt_mesh);
			ERR_PRINT_ON;
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			storage.multimesh_set_mesh(mm, alt_mesh);
			REQUIRE(storage._multimesh_get_command_buffer_rd_rid(mm).is_valid());
			storage.multimesh_set_mesh(mm, mesh2);
			storage.mesh_free(alt_mesh);

			// Fail 2nd surface command buffer_update inside multimesh_set_visible_instances -> never leave half-updated command buffer!
			const uint32_t before_vis_fail = spy.multimesh_changed;
			Access::fail_buffer_update(storage, 2);
			ERR_PRINT_OFF;
			storage.multimesh_set_visible_instances(mm, 2);
			ERR_PRINT_ON;
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
			CHECK(Access::get_multimesh(storage, mm)->command_buffer_bytes == 0);
			CHECK(storage._multimesh_get_buffer_rd_rid(mm) == preserved_buf);
			CHECK(spy.multimesh_changed > before_vis_fail);

			// Same-visible retry (visible_instances == 2) must rebuild command_buffer instead of early-returning!
			storage.multimesh_set_visible_instances(mm, 2);
			Vector<uint32_t> words = read_command_words(rd, storage._multimesh_get_command_buffer_rd_rid(mm));
			REQUIRE(words.size() == 10);
			CHECK(words[0] == 6);
			CHECK(words[1] == 2);
			CHECK(words[5] == 12);
			CHECK(words[6] == 2);

			storage.multimesh_free(mm);
		}

		// 4. _multimesh_set_buffer upload failure (direct & 2nd motion-vector write) -> fail-closed + retry recovery.
		{
			for (int pass = 0; pass < 2; pass++) {
				RID mm = create_initialized_multimesh(storage);
				MultiMeshDependencySpy spy;
				spy.track(storage.multimesh_get_dependency(mm));

				storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
				storage.multimesh_set_mesh(mm, mesh2);
				if (pass == 1) {
					Access::set_motion_vectors(storage, true, 1); // Newly enabled motion vectors write current + previous halves!
					Access::fail_buffer_update(storage, 2);
				} else {
					Access::fail_buffer_update(storage, 1);
				}

				const uint32_t before = spy.multimesh_changed;
				ERR_PRINT_OFF;
				storage.multimesh_set_buffer(mm, make_3d_buffer(3, 9.0f));
				ERR_PRINT_ON;

				CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
				CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
				CHECK(spy.multimesh_changed > before);

				Access::reset_faults(storage);
				storage.multimesh_allocate_data(mm, 3, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
				storage.multimesh_set_buffer(mm, make_3d_buffer(3, 9.0f));
				CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_valid());
				CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_valid());

				storage.multimesh_free(mm);
			}
		}

		// 5. _update_dirty_multimeshes upload failure (bulk, 2nd sparse region, AND re-entrant sibling dirtying).
		{
			RID mm_fail = create_initialized_multimesh(storage);
			RID mm_sibling = create_initialized_multimesh(storage);
			storage.multimesh_allocate_data(mm_fail, 600, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			storage.multimesh_set_mesh(mm_fail, mesh2);
			storage.multimesh_set_buffer(mm_fail, make_3d_buffer(600, 0.0f));

			storage.multimesh_allocate_data(mm_sibling, 4, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
			storage.multimesh_set_buffer(mm_sibling, make_3d_buffer(4, 0.0f));

			// Use 2600 instances (6 regions of 512): first flush clears initial 6-region local-cache dirty state,
			// second single-region dirty + flush leaves previous_data_cache_dirty_region_count == 1 (region 0).
			storage.multimesh_allocate_data(mm_fail, 2600, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
			storage.multimesh_instance_set_transform(mm_fail, 0, Transform3D(Basis(), Vector3(11.0f, 0.0f, 0.0f)));
			Access::flush_dirty(storage);
			storage.multimesh_instance_set_transform(mm_fail, 0, Transform3D(Basis(), Vector3(12.0f, 0.0f, 0.0f)));
			Access::flush_dirty(storage);

			MultiMeshDependencySpy spy;
			spy.reentrant_storage = &storage;
			spy.reentrant_multimesh = mm_sibling;
			spy.track(storage.multimesh_get_dependency(mm_fail));

			// Dirty region 0 (index 1) and region 4 (index 2100) -> total_dirty_regions = 1 + 2 = 3 <= 6/2 (3), sparse path!
			storage.multimesh_instance_set_transform(mm_fail, 1, Transform3D(Basis(), Vector3(22.0f, 0.0f, 0.0f)));
			storage.multimesh_instance_set_transform(mm_fail, 2100, Transform3D(Basis(), Vector3(33.0f, 0.0f, 0.0f)));

			Access::fail_buffer_update(storage, 2);
			ERR_PRINT_OFF;
			Access::flush_dirty(storage);
			ERR_PRINT_ON;

			CHECK(storage._multimesh_get_buffer_rd_rid(mm_fail).is_null());
			CHECK(storage._multimesh_get_command_buffer_rd_rid(mm_fail).is_null());
			CHECK(spy.multimesh_changed == 1);
			spy.reentrant_multimesh = RID();

			Access::reset_faults(storage);
			Access::flush_dirty(storage);
			CHECK(Access::dirty_list_empty(storage));
			Vector<float> sibling_buf = storage.multimesh_get_buffer(mm_sibling);
			REQUIRE(sibling_buf.size() == 48);
			CHECK(Math::is_equal_approx(sibling_buf[3], 77.0f));
			CHECK(Math::is_equal_approx(sibling_buf[7], 88.0f));
			CHECK(Math::is_equal_approx(sibling_buf[11], 99.0f));

			storage.multimesh_free(mm_sibling);
			storage.multimesh_free(mm_fail);
		}

		// 6. _multimesh_enable_motion_vectors all 4 failure sites (create, copy 1, copy 2, doubled_cache update) leak zero RD buffers.
		{
			for (int mode = 0; mode < 4; mode++) {
				CAPTURE(mode);
				RID mm = create_initialized_multimesh(storage);
				MultiMeshDependencySpy spy;
				spy.track(storage.multimesh_get_dependency(mm));

				storage.multimesh_allocate_data(mm, 4, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
				storage.multimesh_set_mesh(mm, mesh2);
				if (mode < 3) {
					storage.multimesh_set_buffer(mm, make_3d_buffer(4, 3.0f)); // GPU-only buffer_set path
				} else {
					storage.multimesh_instance_set_transform(mm, 0, Transform3D(Basis(), Vector3(5.0f, 0.0f, 0.0f)));
					Access::flush_dirty(storage); // Populated CPU data_cache path
				}

				Access::set_motion_vectors(storage, true, 20);
				if (mode == 0) {
					Access::fail_instance_buffer_create(storage, 1);
				} else if (mode == 1) {
					Access::fail_buffer_copy(storage, 1);
				} else if (mode == 2) {
					Access::fail_buffer_copy(storage, 2);
				} else {
					Access::fail_buffer_update(storage, 1);
				}

				const uint32_t before = spy.multimesh_changed;
				ERR_PRINT_OFF;
				if (mode < 3) {
					storage.multimesh_set_buffer(mm, make_3d_buffer(4, 6.0f));
				} else {
					storage.multimesh_instance_set_transform(mm, 1, Transform3D(Basis(), Vector3(8.0f, 0.0f, 0.0f)));
					Access::flush_dirty(storage);
				}
				ERR_PRINT_ON;

				CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
				CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
				CHECK_FALSE(Access::get_multimesh(storage, mm)->motion_vectors_enabled);
				CHECK(spy.multimesh_changed > before);

				Access::reset_faults(storage);
				storage.multimesh_free(mm);
				rd.submit();
				rd.sync();
				CHECK(rd.get_memory_usage(RD::MEMORY_BUFFERS) == baseline_with_mesh);
			}
		}

		// 7. Readback failures (empty AND truncated bytes) in _multimesh_refresh_mesh, _multimesh_make_local, and _multimesh_get_buffer.
		{
			for (int64_t trunc : { int64_t(-1), int64_t(16) }) {
				CAPTURE(trunc);
				// 7a: _multimesh_refresh_mesh auto-AABB readback failure.
				{
					RID mm = create_initialized_multimesh(storage);
					MultiMeshDependencySpy spy;
					spy.track(storage.multimesh_get_dependency(mm));
					storage.multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
					storage.multimesh_set_buffer(mm, make_3d_buffer(2, 1.0f));
					Access::fail_buffer_readback(storage, 1, trunc);
					const uint32_t before = spy.multimesh_changed;
					ERR_PRINT_OFF;
					storage.multimesh_set_mesh(mm, mesh2);
					ERR_PRINT_ON;
					CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
					CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
					CHECK(spy.multimesh_changed > before);
					Access::reset_faults(storage);
					storage.multimesh_free(mm);
				}

				// 7b: _multimesh_make_local failure leaves data_cache empty (no OOB copy or uninitialized floats).
				{
					RID mm = create_initialized_multimesh(storage);
					MultiMeshDependencySpy spy;
					spy.track(storage.multimesh_get_dependency(mm));
					storage.multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
					storage.multimesh_set_mesh(mm, mesh2);
					storage.multimesh_set_buffer(mm, make_3d_buffer(2, 3.0f));
					Access::fail_buffer_readback(storage, 1, trunc);
					const uint32_t before = spy.multimesh_changed;
					ERR_PRINT_OFF;
					Transform3D got = storage.multimesh_instance_get_transform(mm, 1);
					ERR_PRINT_ON;
					CHECK(got == Transform3D());
					CHECK(Access::get_multimesh(storage, mm)->data_cache.is_empty());
					CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
					CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
					CHECK(spy.multimesh_changed > before);
					Access::reset_faults(storage);
					storage.multimesh_free(mm);
				}

				// 7c: _multimesh_get_buffer failure returns empty Vector<float> and invalidates GPU resources.
				{
					RID mm = create_initialized_multimesh(storage);
					MultiMeshDependencySpy spy;
					spy.track(storage.multimesh_get_dependency(mm));
					storage.multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_3D, false, false, true);
					storage.multimesh_set_mesh(mm, mesh2);
					storage.multimesh_set_buffer(mm, make_3d_buffer(2, 7.0f));
					Access::fail_buffer_readback(storage, 1, trunc);
					const uint32_t before = spy.multimesh_changed;
					ERR_PRINT_OFF;
					Vector<float> got = storage.multimesh_get_buffer(mm);
					ERR_PRINT_ON;
					CHECK(got.is_empty());
					CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_null());
					CHECK(storage._multimesh_get_command_buffer_rd_rid(mm).is_null());
					CHECK(spy.multimesh_changed > before);
					Access::reset_faults(storage);
					storage.multimesh_free(mm);
				}
			}
		}

		// 8. Uniform set creation failure returns null RID cleanly and recovers on subsequent query.
		{
			RID mm = create_initialized_multimesh(storage);
			storage.multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
			storage.multimesh_set_buffer(mm, make_3d_buffer(2, 1.0f));

			Access::fail_uniform_set_create(storage, 1);
			CHECK(storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx).is_null());
			CHECK(storage._multimesh_get_buffer_rd_rid(mm).is_valid());
			RID uset_3d = storage.multimesh_get_3d_uniform_set(mm, shader_rd, uset_idx);
			CHECK(uset_3d.is_valid());

			Access::fail_uniform_set_create(storage, 1);
			CHECK(storage.multimesh_get_2d_uniform_set(mm, shader_rd, uset_idx).is_null());
			RID uset_2d = storage.multimesh_get_2d_uniform_set(mm, shader_rd, uset_idx);
			CHECK(uset_2d.is_valid());

			Access::reset_faults(storage);
			storage.multimesh_free(mm);
		}

		storage.mesh_free(mesh2);
		rd.submit();
		rd.sync();
		CHECK(Access::live_multimesh_count(storage) == 0);
		CHECK(Access::dirty_list_empty(storage));
	}

	rd.finalize();
}
#endif // defined(METAL_ENABLED) && defined(MACOS_ENABLED)

TEST_CASE("[B4][R03][MultiMesh] ordinary Mesh, MultiMesh, Dummy storage, and scene shader pipeline keys preserve non-external behavior") {
	// 1. Forward+ and Mobile pipeline keys keep ordinary indexed144 == 0 distinct from external surfaces.
	{
		using ClusteredPipelineKey = RendererSceneRenderImplementation::SceneShaderForwardClustered::ShaderData::PipelineKey;
		using MobilePipelineKey = RendererSceneRenderImplementation::SceneShaderForwardMobile::ShaderData::PipelineKey;

		ClusteredPipelineKey clustered_ordinary = {};
		ClusteredPipelineKey clustered_external = {};
		CHECK(clustered_ordinary.indexed144 == 0);
		clustered_external.indexed144 = 1;
		CHECK(clustered_ordinary.hash() != clustered_external.hash());

		MobilePipelineKey mobile_ordinary = {};
		MobilePipelineKey mobile_external = {};
		CHECK(mobile_ordinary.indexed144 == 0);
		mobile_external.indexed144 = 1;
		CHECK(mobile_ordinary.hash() != mobile_external.hash());
	}

	// 2. RendererDummy MeshStorage + ParticlesStorage ordinary contracts (preserve live test_setup singleton).
	{
		RendererDummy::MeshStorage *dummy_mesh_storage = RendererDummy::MeshStorage::get_singleton();
		REQUIRE(dummy_mesh_storage != nullptr);
		RendererDummy::ParticlesStorage dummy_particles_storage;

		RID mesh = dummy_mesh_storage->mesh_allocate();
		dummy_mesh_storage->mesh_initialize(mesh);
		RenderingServerTypes::SurfaceData sd;
		sd.primitive = RSE::PRIMITIVE_TRIANGLES;
		sd.format = RSE::ARRAY_FORMAT_VERTEX;
		sd.vertex_count = 3;
		sd.vertex_data.resize(3 * sizeof(float) * 3);
		sd.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 1));
		dummy_mesh_storage->mesh_add_surface(mesh, sd);
		CHECK(dummy_mesh_storage->mesh_get_surface_count(mesh) == 1);
		CHECK(dummy_mesh_storage->mesh_get_surface(mesh, 0).aabb == sd.aabb);

		RID mm = dummy_mesh_storage->multimesh_allocate();
		dummy_mesh_storage->multimesh_initialize(mm);
		dummy_mesh_storage->multimesh_allocate_data(mm, 2, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		dummy_mesh_storage->multimesh_set_mesh(mm, mesh);
		CHECK(dummy_mesh_storage->owns_multimesh(mm));
		Vector<float> buf;
		buf.resize(24);
		buf.fill(1.5f);
		dummy_mesh_storage->multimesh_set_buffer(mm, buf);
		CHECK(dummy_mesh_storage->multimesh_get_buffer(mm).size() == 24);

		// Invalid allocation rejected before mutating existing Dummy MultiMesh buffer.
		ERR_PRINT_OFF;
		dummy_mesh_storage->multimesh_allocate_data(mm, -5, RSE::MULTIMESH_TRANSFORM_3D, false, false, false);
		ERR_PRINT_ON;
		CHECK(dummy_mesh_storage->multimesh_get_buffer(mm).size() == 24);

		dummy_mesh_storage->multimesh_free(mm);
		dummy_mesh_storage->mesh_free(mesh);

		RID p = dummy_particles_storage.particles_allocate();
		dummy_particles_storage.particles_initialize(p);
		CHECK_FALSE(dummy_particles_storage.particles_get_emitting(p));
		dummy_particles_storage.particles_free(p);
	}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// 3. Native RendererRD::MeshStorage ordinary Mesh surface vertex/index array creation stays non-external.
	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE_MESSAGE(available_device.get() != nullptr, "Native Metal device unavailable on macOS host.");

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);

	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	{
		RendererRD::MeshStorage storage;
		rd.submit();
		rd.sync();
		const uint64_t baseline_bytes = rd.get_memory_usage(RD::MEMORY_BUFFERS);

		RID mesh = create_initialized_mesh(storage);
		storage.mesh_add_surface(mesh, make_test_surface(4, 6, AABB(Vector3(-2, -1, 0), Vector3(4, 2, 1))));
		REQUIRE(storage.mesh_get_surface_count(mesh) == 1);

		void *surf = storage.mesh_get_surface(mesh, uint32_t(0));
		REQUIRE(surf != nullptr);
		CHECK_FALSE(storage.mesh_surface_is_external(surf));
		CHECK(storage.mesh_surface_get_vertices_drawn_count(surf) == 6);

		RID vertex_array_rd;
		const RD::VertexFormatID invalid_format = RD::INVALID_ID;
		RD::VertexFormatID vertex_format = invalid_format;
		storage.mesh_surface_get_vertex_arrays_and_format(surf, 1, false, false, vertex_array_rd, vertex_format);
		CHECK(vertex_array_rd.is_valid());
		CHECK(vertex_format != invalid_format);
		CHECK(storage.mesh_surface_get_index_array(surf, 0).is_valid());

		storage.mesh_free(mesh);
		rd.submit();
		rd.sync();
		CHECK(rd.get_memory_usage(RD::MEMORY_BUFFERS) == baseline_bytes);
	}

	rd.finalize();
#endif
}
