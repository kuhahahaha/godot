#include "core/extension/gdextension_qualification_physical.h"
#include "servers/rendering/rendering_device_graph.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <type_traits>

TEST_FORCE_LINK(test_external_output_contract)

class ExternalOutputGraphDriver : public TransferTestDriverBase {
public:
	bool begin_ok = true;
	uint32_t copies = 0;
	uint32_t draws = 0;
	uint32_t indexed_draws = 0;
	uint32_t binds = 0;
	uint32_t clears = 0;
	uint32_t allocations = 0;
	uint32_t persistent_maps = 0;
	uint64_t clear_offset = 0;
	uint64_t clear_size = 0;
	uint8_t mapped_bytes[32] = {};
	BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		allocations++;
		return BufferID(); // Stop valid readback at the allocation boundary; no fake GPU result.
	}
	uint8_t *buffer_persistent_map_advance(BufferID, uint64_t) override {
		persistent_maps++;
		return mapped_bytes;
	}
	void command_clear_buffer(CommandBufferID, BufferID, uint64_t p_offset, uint64_t p_size) override {
		clears++;
		clear_offset = p_offset;
		clear_size = p_size;
	}
	uint32_t compute_binds = 0;
	uint32_t indirect_dispatches = 0;
	uint64_t indirect_offset = 0;
	void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
	void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override { copies++; }
	void command_compute_dispatch(CommandBufferID, uint32_t, uint32_t, uint32_t) override {}
	void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID>, ShaderID, uint32_t, uint32_t, uint32_t) override { compute_binds++; }
	void command_compute_dispatch_indirect(CommandBufferID, BufferID, uint64_t p_offset) override {
		indirect_dispatches++;
		indirect_offset = p_offset;
	}
	CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(12); }
	bool command_buffer_begin(CommandBufferID) override { return begin_ok; }
	void command_buffer_end(CommandBufferID) override {}
	SemaphoreID semaphore_create() override { return SemaphoreID(13); }
	void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
	void command_begin_render_pass(CommandBufferID, RenderPassID, FramebufferID, CommandBufferType, const Rect2i &, VectorView<RenderPassClearValue>) override {}
	void command_end_render_pass(CommandBufferID) override {}
	void command_bind_render_uniform_sets(CommandBufferID, VectorView<UniformSetID>, ShaderID, uint32_t, uint32_t, uint32_t) override { binds++; }
	void command_render_draw_indirect(CommandBufferID, BufferID, uint64_t, uint32_t, uint32_t) override { draws++; }
	void command_render_draw_indexed_indirect(CommandBufferID, BufferID, uint64_t, uint32_t, uint32_t) override { indexed_draws++; }
	uint64_t api_trait_get(ApiTrait p_trait) override { return p_trait == API_TRAIT_HONORS_PIPELINE_BARRIERS ? 1 : 0; }
	uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID>, ShaderID, uint32_t, uint32_t) const override { return 0; }
};

class TestExternalOutputAccess {
public:
	using Effect = RenderingDevice::NativeAppendEffect;
	static void attach(RenderingDevice &p_device, RenderingDeviceDriver &p_driver) {
		p_device.driver = &p_driver;
		p_device.draw_graph.initialize(&p_driver,
				[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
				1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		p_device.draw_graph.begin();
	}
	static RID buffer(RenderingDevice &p_device, uint64_t p_id, uint32_t p_size = 32) {
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(p_id);
		buffer.size = p_size;
		buffer.backing_serial = p_id;
		buffer.usage = RenderingDeviceDriver::BUFFER_USAGE_INDIRECT_BIT;
		buffer.draw_tracker = RenderingDeviceGraph::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		return p_device.storage_buffer_owner.make_rid(buffer);
	}
	static void persistent_buffer(RenderingDevice &p_device, RID p_buffer, GDExtensionDeviceExit::QualificationPhysicalReturn *p_returning) {
		auto *buffer = p_device.storage_buffer_owner.get_or_null(p_buffer);
		buffer->usage.set_flag(RenderingDeviceDriver::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);
		buffer->qualification_return = p_returning;
	}
	static uint32_t commands(RenderingDevice &p_device) { return p_device.draw_graph.qualification_command_count(); }
	static auto copy(RenderingDevice &p_device, RID p_source, RID p_destination, uint32_t p_source_offset, uint32_t p_destination_offset, uint32_t p_size) {
		return p_device._buffer_copy_recorded(p_source, p_destination, p_source_offset, p_destination_offset, p_size);
	}
	static auto draw(RenderingDevice &p_device, RID p_buffer, uint32_t p_offset, bool p_indices = false) {
		return p_device._draw_list_draw_indirect_recorded(0, p_indices, p_buffer, p_offset, 1, 20);
	}
	static bool contains(RenderingDevice &p_device, const RenderingDeviceGraph::RecordHandle &p_handle) { return p_device.draw_graph.contains_record(p_handle); }
	static void suspended(RenderingDevice &p_device, Error p_error) { p_device.frame_wait_error = p_error; }
	static void draw_begin(RenderingDevice &p_device) {
		p_device.draw_graph.add_draw_list_begin(RenderingDeviceDriver::RenderPassID(1), RenderingDeviceDriver::FramebufferID(2), Rect2i(), {}, {}, {}, 0, false);
		p_device.draw_list.active = true;
#ifdef DEBUG_ENABLED
		p_device.draw_list.validation.pipeline_active = true;
		p_device.draw_list.validation.pipeline_vertex_format = RenderingDevice::INVALID_ID;
#endif
	}
	static void draw_end(RenderingDevice &p_device) {
		p_device.draw_graph.add_draw_list_end();
		p_device.draw_list.active = false;
	}
	static void finish_graph(RenderingDevice &p_device) {
		RenderingDeviceGraph::CommandBufferPool pool;
		RenderingDeviceDriver::CommandBufferID primary(11);
		p_device.draw_graph.end(true, false, primary, pool);
	}
	static bool slot(RenderingDevice &p_device, const RenderingDeviceGraph::RecordHandle &p_handle, uint32_t &r_slot) {
		return p_device.draw_graph.resolve_record_command_buffer_slot(p_handle, r_slot);
	}
	static void uniform_set(RenderingDevice &p_device) {
		RenderingDevice::UniformSet set;
		set.format = 1;
		set.driver_id = RenderingDeviceDriver::UniformSetID(81);
		RID rid = p_device.uniform_set_owner.make_rid(set);
		p_device.draw_list.state.set_count = 1;
		p_device.draw_list.state.sets[0].uniform_set = rid;
		p_device.draw_list.state.sets[0].uniform_set_driver_id = set.driver_id;
		p_device.draw_list.state.sets[0].pipeline_expected_format = 1;
		p_device.draw_list.state.sets[0].uniform_set_format = 1;
		p_device.draw_list.state.sets[0].bound = false;
	}
	static void texture_preparation(RenderingDevice &p_device, bool p_shared) {
		auto *set = p_device.uniform_set_owner.get_or_null(p_device.draw_list.state.sets[0].uniform_set);
		RenderingDevice::Texture texture;
		texture.driver_id = RenderingDeviceDriver::TextureID(91);
		texture.pending_clear = !p_shared;
		if (p_shared) {
			texture.shared_fallback = memnew(RenderingDevice::Texture::SharedFallback);
			texture.shared_fallback->revision = 1;
			texture.owner = p_device.texture_owner.make_rid(texture);
			texture.shared_fallback = memnew(RenderingDevice::Texture::SharedFallback);
			texture.shared_fallback->revision = 0;
			RID shared = p_device.texture_owner.make_rid(texture);
			set->shared_textures_to_update.push_back({ 0, shared });
		} else {
			set->pending_clear_textures.push_back(p_device.texture_owner.make_rid(texture));
		}
	}
	static void compute_begin(RenderingDevice &p_device) {
		p_device.draw_graph.add_compute_list_begin();
		p_device.compute_list.active = true;
#ifdef DEBUG_ENABLED
		p_device.compute_list.validation.pipeline_active = true;
#endif
		RenderingDevice::UniformSet set;
		set.format = 1;
		set.driver_id = RenderingDeviceDriver::UniformSetID(82);
		RID rid = p_device.uniform_set_owner.make_rid(set);
		p_device.compute_list.state.set_count = 1;
		auto &state = p_device.compute_list.state.sets[0];
		state.uniform_set = rid;
		state.uniform_set_driver_id = set.driver_id;
		state.pipeline_expected_format = 1;
		state.uniform_set_format = 1;
	}
	static void compute_indirect(RenderingDevice &p_device, RID p_buffer, uint32_t p_offset) {
		p_device.compute_list_dispatch_indirect(RenderingDevice::ID_TYPE_COMPUTE_LIST, p_buffer, p_offset);
	}
	static bool compute_bound(RenderingDevice &p_device) { return p_device.compute_list.state.sets[0].bound; }
	static uint32_t dispatches(RenderingDevice &p_device) { return p_device.compute_list.state.dispatch_count; }
	static void compute_end(RenderingDevice &p_device) {
		p_device.draw_graph.add_compute_list_end();
		p_device.compute_list.active = false;
	}
	static void transfer(RenderingDevice &p_device, RID p_buffer) {
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker->index = 0;
		p_device.transfer_worker_pool.push_back(worker);
		p_device.transfer_worker_pool_size = 1;
		p_device.transfer_worker_operation_used_by_draw.resize(1);
		auto *buffer = p_device.storage_buffer_owner.get_or_null(p_buffer);
		buffer->transfer_worker_index = 0;
		buffer->transfer_worker_operation = 9;
	}
	static uint64_t transfer_used(RenderingDevice &p_device) { return p_device.transfer_worker_operation_used_by_draw[0]; }
	static void detach(RenderingDevice &p_device) {
		// These test buffers have no real device allocations or issued GPU work.
		for (RID rid : p_device.storage_buffer_owner.get_owned_list()) {
			RenderingDeviceGraph::resource_tracker_free(p_device.storage_buffer_owner.get_or_null(rid)->draw_tracker);
			p_device.storage_buffer_owner.free(rid);
		}
		for (auto *worker : p_device.transfer_worker_pool) {
			memdelete(worker);
		}
		p_device.transfer_worker_pool.clear();
		p_device.transfer_worker_pool_size = 0;
		for (RID rid : p_device.uniform_set_owner.get_owned_list()) {
			p_device.uniform_set_owner.free(rid);
		}
		for (RID rid : p_device.texture_owner.get_owned_list()) {
			auto *fallback = p_device.texture_owner.get_or_null(rid)->shared_fallback;
			if (fallback) {
				memdelete(fallback);
			}
			p_device.texture_owner.free(rid);
		}
		p_device.driver = nullptr;
	}
};

TEST_CASE("[B4] native_packet_join") {
	SUBCASE("J14 transfer dependencies and refused draw prefixes remain distinguishable") {
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID source = TestExternalOutputAccess::buffer(device, 71);
		RID destination = TestExternalOutputAccess::buffer(device, 72);
		TestExternalOutputAccess::transfer(device, source);
		const auto copied = TestExternalOutputAccess::copy(device, source, destination, 0, 0, 8);
		CHECK(copied.error == OK);
		CHECK(copied.effect == TestExternalOutputAccess::Effect::Recorded);
		CHECK(copied.transfer_count == 1);
		CHECK(copied.transfers[0].worker == 0);
		CHECK(copied.transfers[0].operation == 9);
		CHECK(TestExternalOutputAccess::transfer_used(device) == 9);
		TestExternalOutputAccess::draw_begin(device);
		TestExternalOutputAccess::uniform_set(device);
		ERR_PRINT_OFF;
		const auto prefix = TestExternalOutputAccess::draw(device, source, 0, true);
		ERR_PRINT_ON;
		CHECK(prefix.error == ERR_INVALID_PARAMETER);
		CHECK(prefix.effect == TestExternalOutputAccess::Effect::NativePrefixOnly);
		CHECK_FALSE(prefix.draw_handle.is_valid());
		CHECK(prefix.capture_complete);
		CHECK(prefix.prefix_count == 1);
		CHECK(TestExternalOutputAccess::contains(device, prefix.prefix_handles[0]));
		TestExternalOutputAccess::draw_end(device);
		CHECK(TestExternalOutputAccess::contains(device, prefix.prefix_handles[0]));
		uint32_t slot = 99;
		CHECK_FALSE(TestExternalOutputAccess::slot(device, prefix.prefix_handles[0], slot));
		TestExternalOutputAccess::finish_graph(device);
		CHECK(TestExternalOutputAccess::slot(device, prefix.prefix_handles[0], slot));
		CHECK(slot == 0);
		CHECK(driver.binds == 1);
		CHECK(driver.draws == 0);
		CHECK(driver.indexed_draws == 0);
		TestExternalOutputAccess::detach(device);
	}
	SUBCASE("J14 output capture refuses pending texture preparation before descriptor effects") {
		for (bool shared : { false, true }) {
			RenderingDevice device;
			ExternalOutputGraphDriver driver;
			TestExternalOutputAccess::attach(device, driver);
			RID command = TestExternalOutputAccess::buffer(device, 71);
			TestExternalOutputAccess::draw_begin(device);
			TestExternalOutputAccess::uniform_set(device);
			TestExternalOutputAccess::texture_preparation(device, shared);
			const uint32_t before = TestExternalOutputAccess::commands(device);
			ERR_PRINT_OFF;
			const auto rejected = TestExternalOutputAccess::draw(device, command, 0);
			ERR_PRINT_ON;
			CHECK(rejected.error == ERR_UNAVAILABLE);
			CHECK(rejected.effect == TestExternalOutputAccess::Effect::NoNewEffect);
			CHECK_FALSE(rejected.capture_complete);
			CHECK(rejected.prefix_count == 0);
			CHECK_FALSE(rejected.draw_handle.is_valid());
			CHECK(TestExternalOutputAccess::commands(device) == before);
			TestExternalOutputAccess::draw_end(device);
			TestExternalOutputAccess::detach(device);
		}
	}
	SUBCASE("J14 checked RD companions retain actual handles and exact preflight errors") {
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID source = TestExternalOutputAccess::buffer(device, 71);
		RID destination = TestExternalOutputAccess::buffer(device, 72);
		ERR_PRINT_OFF;
		const auto rejected = TestExternalOutputAccess::copy(device, source, destination, 28, 0, 8);
		ERR_PRINT_ON;
		CHECK(rejected.error == ERR_INVALID_PARAMETER);
		CHECK(rejected.effect == TestExternalOutputAccess::Effect::NoNewEffect);
		CHECK_FALSE(rejected.copy_handle.is_valid());
		CHECK(rejected.transfer_count == 0);
		const auto copied = TestExternalOutputAccess::copy(device, source, destination, 24, 24, 8);
		CHECK(copied.error == OK);
		CHECK(copied.effect == TestExternalOutputAccess::Effect::Recorded);
		CHECK(TestExternalOutputAccess::contains(device, copied.copy_handle));
		TestExternalOutputAccess::suspended(device, ERR_TIMEOUT);
		ERR_PRINT_OFF;
		const auto suspended = TestExternalOutputAccess::copy(device, source, destination, 0, 0, 8);
		const auto draw_suspended = TestExternalOutputAccess::draw(device, source, 0);
		ERR_PRINT_ON;
		CHECK(suspended.error == ERR_TIMEOUT);
		CHECK(suspended.effect == TestExternalOutputAccess::Effect::NoNewEffect);
		CHECK_FALSE(suspended.copy_handle.is_valid());
		CHECK(draw_suspended.error == ERR_TIMEOUT);
		CHECK(draw_suspended.effect == TestExternalOutputAccess::Effect::NoNewEffect);
		CHECK_FALSE(draw_suspended.draw_handle.is_valid());
		TestExternalOutputAccess::suspended(device, OK);
		TestExternalOutputAccess::draw_begin(device);
		ERR_PRINT_OFF;
		const auto draw_bad_range = TestExternalOutputAccess::draw(device, source, UINT32_MAX - 3);
		ERR_PRINT_ON;
		CHECK(draw_bad_range.error == ERR_INVALID_PARAMETER);
		CHECK(draw_bad_range.effect == TestExternalOutputAccess::Effect::NoNewEffect);
		CHECK_FALSE(draw_bad_range.draw_handle.is_valid());
		const auto drawn = TestExternalOutputAccess::draw(device, source, 0);
		CHECK(drawn.error == OK);
		CHECK(drawn.effect == TestExternalOutputAccess::Effect::Recorded);
		CHECK(TestExternalOutputAccess::contains(device, drawn.draw_handle));
		TestExternalOutputAccess::draw_end(device);
		CHECK(TestExternalOutputAccess::contains(device, drawn.draw_handle));
		TestExternalOutputAccess::detach(device);
	}
	SUBCASE("J14 public copy rejects overflowing source and destination spans before append") {
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID source = TestExternalOutputAccess::buffer(device, 71);
		RID destination = TestExternalOutputAccess::buffer(device, 72);
		const uint32_t before = TestExternalOutputAccess::commands(device);
		ERR_PRINT_OFF;
		CHECK(device.buffer_copy(source, destination, UINT32_MAX - 3, 0, 8) == ERR_INVALID_PARAMETER);
		CHECK(device.buffer_copy(source, destination, 0, UINT32_MAX - 3, 8) == ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
		CHECK(TestExternalOutputAccess::commands(device) == before);
		CHECK(device.buffer_copy(source, destination, 24, 24, 8) == OK);
		TestExternalOutputAccess::detach(device);
	}
	SUBCASE("J13 record offsets must identify actual instruction boundaries") {
		RenderingDeviceGraph graph;
		TransferTestDriverBase driver;
		graph.initialize(&driver,
				[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
				1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		graph.begin();
		graph.add_compute_list_begin();
		// The x-group word equals the dispatch opcode. Reading an arbitrary
		// offset as an instruction would mistake this payload for a dispatch.
		const auto dispatch = graph.add_compute_list_dispatch(3, 1, 1);
		auto payload = dispatch;
		payload.offset += 4;
		CHECK(graph.contains_record(dispatch));
		CHECK_FALSE(graph.contains_record(payload));
		graph.add_compute_list_end();
		CHECK(graph.contains_record(dispatch));
		CHECK_FALSE(graph.contains_record(payload));
		uint32_t slot = 99;
		CHECK_FALSE(graph.resolve_record_command_buffer_slot(payload, slot));
		CHECK(slot == 99);
		CHECK(graph.record_mapping_state(payload) == RenderingDeviceGraph::RecordMappingState::Invalid);
	}
	SUBCASE("J14 copy and indirect draws map only actual executed list instructions") {
		// This lambda also exercises the pre-feature void APIs without making
		// a missing return type a compiler error instead of a behavioral RED.
		auto check = [](auto &graph, ExternalOutputGraphDriver &driver, bool begin_ok, bool render_pass_valid) {
			using RDD = RenderingDeviceDriver;
			RenderingDeviceGraph::ResourceTracker destination;
			destination.buffer_driver_id = RDD::BufferID(72);
			if constexpr (std::is_void_v<decltype(graph.add_buffer_copy(RDD::BufferID(71), nullptr, RDD::BufferID(72), &destination, RDD::BufferCopyRegion{ 0, 0, 32 }))> ||
					std::is_void_v<decltype(graph.add_draw_list_draw_indirect(RDD::BufferID(73), 0, 1, 20))> ||
					std::is_void_v<decltype(graph.add_draw_list_draw_indexed_indirect(RDD::BufferID(73), 20, 1, 20))>) {
				FAIL("Copy and indirect draw appends must return their actual graph record handles");
			} else {
				const auto copy = graph.add_buffer_copy(RDD::BufferID(71), nullptr, RDD::BufferID(72), &destination, { 0, 0, 32 });
				CHECK(graph.contains_record(copy));
				CHECK(copy.kind == 1);
				graph.add_synchronization();
				graph.add_draw_list_begin(render_pass_valid ? RDD::RenderPassID(1) : RDD::RenderPassID(), RDD::FramebufferID(2), Rect2i(), {}, {}, {}, 0, true);
				const auto draw = graph.add_draw_list_draw_indirect(RDD::BufferID(73), 0, 1, 20);
				const auto indexed = graph.add_draw_list_draw_indexed_indirect(RDD::BufferID(73), 20, 1, 20);
				CHECK(draw.kind == 3);
				CHECK(draw.index == indexed.index);
				CHECK(draw.offset < indexed.offset);
				CHECK(graph.contains_record(draw));
				CHECK(graph.contains_record(indexed));
				auto payload = draw;
				payload.offset += 4;
				CHECK_FALSE(graph.contains_record(payload));
				auto wrong_kind = draw;
				wrong_kind.kind = 2;
				CHECK_FALSE(graph.contains_record(wrong_kind));
				uint32_t slot = 99;
				CHECK_FALSE(graph.resolve_record_command_buffer_slot(draw, slot));
				graph.add_draw_list_end();
				CHECK(graph.contains_record(draw));
				CHECK(graph.contains_record(indexed));
				CHECK_FALSE(graph.contains_record(payload));
				CHECK(graph.has_synchronization_edge(copy, draw));
				CHECK(graph.record_mapping_state(draw) == RenderingDeviceGraph::RecordMappingState::Pending);
				RenderingDeviceGraph::CommandBufferPool pool;
				RDD::CommandBufferID primary(11);
				graph.end(true, false, primary, pool);
				CHECK(driver.copies == 1);
				CHECK(graph.resolve_record_command_buffer_slot(copy, slot));
				CHECK(slot == 0);
				if (begin_ok && render_pass_valid) {
					CHECK(graph.resolve_record_command_buffer_slot(draw, slot));
					CHECK(slot == 1);
					CHECK(graph.resolve_record_command_buffer_slot(indexed, slot));
					CHECK(slot == 1);
					CHECK(driver.draws == 1);
					CHECK(driver.indexed_draws == 1);
					CHECK(graph.record_mapping_state(draw) == RenderingDeviceGraph::RecordMappingState::Recorded);
				} else {
					CHECK_FALSE(graph.resolve_record_command_buffer_slot(draw, slot));
					CHECK_FALSE(graph.resolve_record_command_buffer_slot(indexed, slot));
					CHECK(driver.draws == 0);
					CHECK(driver.indexed_draws == 0);
					CHECK(graph.record_mapping_state(draw) == (begin_ok ? RenderingDeviceGraph::RecordMappingState::Unknown : RenderingDeviceGraph::RecordMappingState::Failed));
				}
				graph.begin();
				CHECK_FALSE(graph.contains_record(copy));
				CHECK_FALSE(graph.contains_record(draw));
				CHECK_FALSE(graph.contains_record(indexed));
			}
		};
		for (int variant = 0; variant < 3; variant++) {
			RenderingDeviceGraph graph;
			ExternalOutputGraphDriver driver;
			driver.begin_ok = variant != 1;
			graph.initialize(&driver,
					[](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); },
					1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
			graph.begin();
			check(graph, driver, driver.begin_ok, variant != 2);
		}
	}
}

TEST_CASE("[RenderingDevice] compute indirect validates aligned complete command span before effects") {
	for (uint32_t offset : { 0u, 20u, 2u, 24u, UINT32_MAX - 3 }) {
		CAPTURE(offset);
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID command = TestExternalOutputAccess::buffer(device, 71, 32);
		TestExternalOutputAccess::compute_begin(device);
		const uint32_t before = TestExternalOutputAccess::commands(device);
		const bool valid = offset == 0 || offset == 20;
		ERR_PRINT_OFF;
		TestExternalOutputAccess::compute_indirect(device, command, offset);
		ERR_PRINT_ON;
		CHECK(TestExternalOutputAccess::compute_bound(device) == valid);
		CHECK(TestExternalOutputAccess::dispatches(device) == (valid ? 1 : 0));
		// Compute instructions remain inside their list until compute_end.
		CHECK(TestExternalOutputAccess::commands(device) == before);
		TestExternalOutputAccess::compute_end(device);
		TestExternalOutputAccess::finish_graph(device);
		CHECK(driver.compute_binds == (valid ? 1 : 0));
		CHECK(driver.indirect_dispatches == (valid ? 1 : 0));
		if (valid) {
			CHECK(driver.indirect_offset == offset);
		}
		TestExternalOutputAccess::detach(device);
	}
}

TEST_CASE("[RenderingDevice] buffer clear validates aligned spans and skips empty commands before effects") {
	struct ClearRange {
		uint32_t offset;
		uint32_t size;
		bool valid;
	};
	for (const ClearRange range : { ClearRange{ 0, 32, true }, { 24, 8, true }, { 1, 4, false }, { 4, 4, true }, { 28, 8, false }, { UINT32_MAX - 3, 8, false }, { 0, 0, true }, { 32, 0, true } }) {
		CAPTURE(range.offset);
		CAPTURE(range.size);
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID buffer = TestExternalOutputAccess::buffer(device, 71, 32);
		TestExternalOutputAccess::transfer(device, buffer);
		const bool issued = range.valid && range.size != 0;
		const uint32_t before = TestExternalOutputAccess::commands(device);
		ERR_PRINT_OFF;
		CHECK(device.buffer_clear(buffer, range.offset, range.size) == (range.valid ? OK : ERR_INVALID_PARAMETER));
		ERR_PRINT_ON;
		CHECK(TestExternalOutputAccess::commands(device) == before + (issued ? 1 : 0));
		CHECK(TestExternalOutputAccess::transfer_used(device) == (issued ? 9 : 0));
		if (range.valid && range.size == 0) {
			GDExtensionDeviceExit::QualificationPhysicalReturn returning;
			TestExternalOutputAccess::persistent_buffer(device, buffer, &returning);
			ERR_PRINT_OFF;
			CHECK(device.buffer_clear(buffer, range.offset, range.size) == ERR_UNAUTHORIZED);
			ERR_PRINT_ON;
			CHECK(TestExternalOutputAccess::commands(device) == before);
			CHECK(TestExternalOutputAccess::transfer_used(device) == 0);
		}
		TestExternalOutputAccess::finish_graph(device);
		CHECK(driver.clears == (issued ? 1 : 0));
		if (issued) {
			CHECK(driver.clear_offset == range.offset);
			CHECK(driver.clear_size == range.size);
		}
		TestExternalOutputAccess::detach(device);
	}
}

TEST_CASE("[RenderingDevice] buffer update validates the full span before return guard and mapping") {
	for (uint32_t offset : { 0u, 24u, 28u, UINT32_MAX - 3 }) {
		CAPTURE(offset);
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID buffer = TestExternalOutputAccess::buffer(device, 71, 32);
		const bool valid = offset == 0 || offset == 24;
		const uint32_t size = offset == 0 ? 32 : 8;
		uint8_t bytes[32];
		for (uint32_t i = 0; i < 32; ++i) {
			bytes[i] = i + 1;
		}
		// A real return guard prevents the pre-fix malformed call from mapping
		// invalid memory. Its distinct error also proves which preflight ran.
		GDExtensionDeviceExit::QualificationPhysicalReturn returning;
		TestExternalOutputAccess::persistent_buffer(device, buffer, valid ? nullptr : &returning);
		const uint32_t before = TestExternalOutputAccess::commands(device);
		ERR_PRINT_OFF;
		CHECK(device.buffer_update(buffer, offset, size, bytes) == (valid ? OK : ERR_INVALID_PARAMETER));
		ERR_PRINT_ON;
		CHECK(driver.persistent_maps == (valid ? 1 : 0));
		CHECK(TestExternalOutputAccess::commands(device) == before);
		if (valid) {
			for (uint32_t i = 0; i < 32; ++i) {
				CHECK(driver.mapped_bytes[i] == (i >= offset && i < offset + size ? bytes[i - offset] : 0));
			}
			TestExternalOutputAccess::persistent_buffer(device, buffer, &returning);
			ERR_PRINT_OFF;
			CHECK(device.buffer_update(buffer, offset, size, bytes) == ERR_UNAUTHORIZED);
			ERR_PRINT_ON;
			CHECK(driver.persistent_maps == 1);
		}
		TestExternalOutputAccess::detach(device);
	}
}

TEST_CASE("[RenderingDevice] buffer readback validates explicit and default spans before allocation") {
	for (uint32_t offset : { 0u, 24u, 28u, UINT32_MAX - 3, 4u }) {
		CAPTURE(offset);
		RenderingDevice device;
		ExternalOutputGraphDriver driver;
		TestExternalOutputAccess::attach(device, driver);
		RID buffer = TestExternalOutputAccess::buffer(device, 71, 32);
		const bool valid = offset == 0 || offset == 24;
		const uint32_t size = offset == 0 || offset == 4 ? 0 : 8;
		const uint32_t before = TestExternalOutputAccess::commands(device);
		ERR_PRINT_OFF;
		CHECK(device.buffer_get_data(buffer, offset, size).is_empty());
		ERR_PRINT_ON;
		// Legal ranges reach the original allocation call. The receiver refuses
		// allocation, so this test makes no GPU-copy or successful-readback claim.
		CHECK(driver.allocations == (valid ? 1 : 0));
		CHECK(TestExternalOutputAccess::commands(device) == before);
		TestExternalOutputAccess::detach(device);
	}
}
