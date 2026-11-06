/**************************************************************************/
/*  test_checked_draw_bind.cpp                                            */
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

#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_checked_draw_bind)

class TestCheckedBindAccess {
	using RD = RenderingDevice;
	using RDD = RenderingDeviceDriver;
	using RDG = RenderingDeviceGraph;
	struct Driver : TransferTestDriverBase {
		Vector<uint64_t> payload;
		uint64_t limit_get(Limit p_limit) override {
			CHECK(p_limit == LIMIT_MAX_BOUND_UNIFORM_SETS);
			return 8;
		}
		uint64_t buffer_get_dynamic_offsets(Span<BufferID>) override { return 0; }
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_begin_render_pass(CommandBufferID, RenderPassID, FramebufferID, CommandBufferType, const Rect2i &, VectorView<RenderPassClearValue>) override { payload.push_back(10); }
		void command_end_render_pass(CommandBufferID) override { payload.push_back(20); }
		void command_render_set_scissor(CommandBufferID, VectorView<Rect2i>) override {}
		void command_render_bind_vertex_buffers(CommandBufferID, uint32_t count, const BufferID *buffers, const uint64_t *offsets, uint64_t) override {
			payload.push_back(1);
			payload.push_back(count);
			for (uint32_t i = 0; i < count; ++i) {
				payload.push_back(buffers[i].id);
				payload.push_back(offsets[i]);
			}
		}
		void command_render_bind_index_buffer(CommandBufferID, BufferID buffer, IndexBufferFormat format, uint64_t offset) override {
			payload.push_back(2);
			payload.push_back(buffer.id);
			payload.push_back(format);
			payload.push_back(offset);
		}
	};
	struct Fixture {
		RD rd;
		Driver driver;
		Vector<RID> sets, vertices, indices;
		RDG::ResourceTracker vertex_tracker, index_tracker;
		static constexpr RD::DrawListID LIST = int64_t(RD::ID_TYPE_DRAW_LIST) << RD::ID_BASE_SHIFT;
		Fixture() {
			rd.driver = &driver;
			rd.render_thread_id = Thread::get_caller_id();
			rd.draw_graph.initialize(&driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
			rd.draw_graph.begin();
			begin();
			vertex_tracker.buffer_driver_id = RDD::BufferID(31);
			index_tracker.buffer_driver_id = RDD::BufferID(41);
			for (uint32_t i = 0; i < 2; ++i) {
				auto *worker = memnew(RD::TransferWorker);
				worker->index = i;
				rd.transfer_worker_pool.push_back(worker);
			}
			rd.transfer_worker_pool_size = 2;
			rd.transfer_worker_operation_used_by_draw.resize(2);
			rd.transfer_worker_operation_used_by_draw[0] = rd.transfer_worker_operation_used_by_draw[1] = 0;
		}
		void begin() {
			rd._draw_list_start(Rect2i());
			// Original graph fixture setup, not a Scene/source authority grant.
			rd.draw_list.begin_handle = rd.draw_graph.add_draw_list_begin(RDD::RenderPassID(21), RDD::FramebufferID(22), Rect2i(), {}, {}, {});
		}
		void end() {
			rd.draw_graph.add_draw_list_end();
			rd._draw_list_end(nullptr);
		}
		~Fixture() {
			if (rd.draw_list.active) {
				end();
			}
			for (RID rid : sets) {
				rd.uniform_set_owner.free(rid);
			}
			for (RID rid : vertices) {
				rd.vertex_array_owner.free(rid);
			}
			for (RID rid : indices) {
				rd.index_array_owner.free(rid);
			}
			for (auto *worker : rd.transfer_worker_pool) {
				memdelete(worker);
			}
			rd.transfer_worker_pool.clear();
			rd.transfer_worker_pool_size = 0;
			rd.driver = nullptr;
		}
		RID uniform(uint32_t id) {
			RD::UniformSet set;
			set.driver_id = RDD::UniformSetID(id);
			set.format = id;
			RID rid = rd.uniform_set_owner.make_rid(set);
			sets.push_back(rid);
			return rid;
		}
		RID vertex() {
			RD::VertexArray array;
			array.description = 7;
			array.vertex_count = 4;
			array.max_instances_allowed = 3;
			array.buffers.push_back(RDD::BufferID(31));
			array.buffers.push_back(RDD::BufferID(32));
			array.offsets.push_back(4);
			array.offsets.push_back(uint64_t(1) << 35);
			array.draw_trackers.push_back(&vertex_tracker);
			array.transfer_worker_indices.push_back(0);
			array.transfer_worker_indices.push_back(1);
			array.transfer_worker_indices.push_back(0);
			array.transfer_worker_operations.push_back(5);
			array.transfer_worker_operations.push_back(9);
			array.transfer_worker_operations.push_back(7);
			RID rid = rd.vertex_array_owner.make_rid(array);
			vertices.push_back(rid);
			return rid;
		}
		RID index() {
			RD::IndexArray array;
			array.driver_id = RDD::BufferID(41);
			array.draw_tracker = &index_tracker;
			array.offset = 3;
			array.indices = 6;
			array.format = RD::INDEX_BUFFER_FORMAT_UINT32;
			array.transfer_worker_index = 1;
			array.transfer_worker_operation = 11;
			RID rid = rd.index_array_owner.make_rid(array);
			indices.push_back(rid);
			return rid;
		}
	};
	static void no_effect(const RD::BindAppendResult &result, Error error) {
		CHECK(result.error == error);
		CHECK(result.effect == RD::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(result.state_accepted);
		CHECK_FALSE(result.state_changed);
		CHECK_FALSE(result.bind_handle.is_valid());
		CHECK(result.transfer_count == 0);
		CHECK_FALSE(result.capture_complete);
	}

public:
	static void uniform_state() {
		Fixture f;
		const RID first = f.uniform(51), next = f.uniform(52);
		const auto before = f.rd.draw_graph.add_draw_list_set_scissor(Rect2i());
		const auto accepted = f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, first, 0);
		CHECK(accepted.error == OK);
		CHECK(accepted.state_accepted);
		CHECK(accepted.state_changed);
		CHECK(accepted.capture_complete);
		CHECK(accepted.effect == RD::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(accepted.bind_handle.is_valid());
		const auto after = f.rd.draw_graph.add_draw_list_set_scissor(Rect2i());
		const auto marker = f.rd.draw_graph.add_draw_list_set_scissor(Rect2i());
		CHECK(after.offset - before.offset == marker.offset - after.offset);
		CHECK(f.rd.draw_list.state.sets[0].uniform_set == first);
		CHECK_FALSE(f.rd.draw_list.state.sets[0].bound);
		const auto duplicate = f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, first, 0);
		CHECK(duplicate.state_accepted);
		CHECK_FALSE(duplicate.state_changed);
		ERR_PRINT_OFF;
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, next, 8), ERR_INVALID_PARAMETER);
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST + 1, next, 0), ERR_INVALID_PARAMETER);
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, RID(), 0), ERR_INVALID_PARAMETER);
		f.rd.frame_wait_error = ERR_CANT_ACQUIRE_RESOURCE;
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, next, 0), ERR_CANT_ACQUIRE_RESOURCE);
		f.rd.frame_wait_error = OK;
		f.rd.draw_list.active = false;
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, next, 0), ERR_INVALID_PARAMETER);
		f.rd.draw_list.active = true;
		f.rd.render_thread_id ^= 1;
		no_effect(f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, next, 0), ERR_UNAVAILABLE);
		f.rd.render_thread_id = Thread::get_caller_id();
		// The alias is rejected at the original late validation point. The new
		// pending state survives; it must never be reported as no state change.
		auto *set = f.rd.uniform_set_owner.get_or_null(next);
		set->attachable_textures.push_back({ 0, first });
		f.rd.draw_list_bound_textures.push_back(first);
		const auto alias = f.rd._draw_list_bind_uniform_set_recorded(Fixture::LIST, next, 0);
		ERR_PRINT_ON;
		CHECK(alias.error == ERR_INVALID_PARAMETER);
		CHECK(alias.state_changed);
		CHECK_FALSE(alias.state_accepted);
		CHECK_FALSE(alias.capture_complete);
		CHECK_FALSE(alias.bind_handle.is_valid());
		CHECK(alias.effect == RD::NativeAppendEffect::NoNewEffect);
		CHECK(f.rd.draw_list.state.sets[0].uniform_set == next);
		f.rd.draw_list_bound_textures.clear();
		// Public ordinary setter still uses the same CPU state body.
		f.rd.draw_list.state.sets[0].bound = true;
		f.rd.draw_list_bind_uniform_set(Fixture::LIST, first, 0);
		CHECK(f.rd.draw_list.state.sets[0].uniform_set == first);
		CHECK_FALSE(f.rd.draw_list.state.sets[0].bound);
	}
	static void geometry(bool ordinary) {
		Fixture f;
		const RID va = f.vertex(), ia = f.index();
		RD::NativeTransferDependency transfers[3];
		RDG::RecordHandle vertex_handle, index_handle;
		if (ordinary) {
			f.rd.draw_list_bind_vertex_array(Fixture::LIST, va);
			f.rd.draw_list_bind_index_array(Fixture::LIST, ia);
		} else {
			ERR_PRINT_OFF;
			no_effect(f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, va, transfers, 2), ERR_PARAMETER_RANGE_ERROR);
			CHECK(f.rd.vertex_array_owner.get_or_null(va)->transfer_worker_indices.size() == 3);
			CHECK(f.rd.transfer_worker_operation_used_by_draw[0] == 0);
			no_effect(f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, nullptr, 0), ERR_PARAMETER_RANGE_ERROR);
			CHECK(f.rd.index_array_owner.get_or_null(ia)->transfer_worker_index == 1);
			ERR_PRINT_ON;
			const auto vertex = f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, va, transfers, 3);
			CHECK(vertex.error == OK);
			CHECK(vertex.state_accepted);
			CHECK(vertex.state_changed);
			CHECK(vertex.capture_complete);
			CHECK(vertex.effect == RD::NativeAppendEffect::Recorded);
			CHECK(vertex.transfer_count == 3);
			CHECK(transfers[0].worker == 0);
			CHECK(transfers[0].operation == 5);
			CHECK(transfers[1].worker == 1);
			CHECK(transfers[1].operation == 9);
			CHECK(transfers[2].worker == 0);
			CHECK(transfers[2].operation == 7);
			vertex_handle = vertex.bind_handle;
			const auto index = f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, transfers, 3);
			CHECK(index.error == OK);
			CHECK(index.state_accepted);
			CHECK(index.state_changed);
			CHECK(index.capture_complete);
			CHECK(index.transfer_count == 1);
			CHECK(transfers[0].worker == 1);
			CHECK(transfers[0].operation == 11);
			index_handle = index.bind_handle;
			for (const auto &handle : { vertex_handle, index_handle }) {
				CHECK(handle.kind == 3);
				CHECK(f.rd.draw_graph.contains_record(handle));
			}
			const auto repeated = f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, va, nullptr, 0);
			CHECK(repeated.error == OK);
			CHECK(repeated.state_accepted);
			CHECK(repeated.capture_complete);
			CHECK_FALSE(repeated.state_changed);
			CHECK_FALSE(repeated.bind_handle.is_valid());
			CHECK(repeated.transfer_count == 0);
			CHECK(repeated.effect == RD::NativeAppendEffect::NoNewEffect);
			const auto repeated_index = f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, nullptr, 0);
			CHECK(repeated_index.error == OK);
			CHECK(repeated_index.state_accepted);
			CHECK(repeated_index.capture_complete);
			CHECK_FALSE(repeated_index.state_changed);
			CHECK_FALSE(repeated_index.bind_handle.is_valid());
			CHECK(repeated_index.transfer_count == 0);
		}
		CHECK(f.rd.vertex_array_owner.get_or_null(va)->transfer_worker_indices.is_empty());
		CHECK(f.rd.vertex_array_owner.get_or_null(va)->transfer_worker_operations.is_empty());
		CHECK(f.rd.index_array_owner.get_or_null(ia)->transfer_worker_index == -1);
		CHECK(f.rd.transfer_worker_operation_used_by_draw[0] == 7);
		CHECK(f.rd.transfer_worker_operation_used_by_draw[1] == 11);
		CHECK(f.vertex_tracker.draw_list_usage == RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ);
		CHECK(f.index_tracker.draw_list_usage == RDG::RESOURCE_USAGE_INDEX_BUFFER_READ);
		CHECK(f.rd.draw_list.validation.vertex_array_size == 4);
		CHECK(f.rd.draw_list.validation.index_array_count == 6);
		f.end();
		RDG::CommandBufferPool pool;
		RDD::CommandBufferID primary(11);
		f.rd.draw_graph.end(false, false, primary, pool);
		const uint64_t expected[] = { 10, 1, 2, 31, 4, 32, uint64_t(1) << 35, 2, 41, RDD::INDEX_BUFFER_FORMAT_UINT32, 12, 20 };
		REQUIRE(f.driver.payload.size() == int(sizeof(expected) / sizeof(expected[0])));
		for (uint32_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
			CHECK(f.driver.payload[i] == expected[i]);
		}
		if (!ordinary) {
			for (const auto &handle : { vertex_handle, index_handle }) {
				RDD::CommandBufferID actual;
				CHECK(f.rd.draw_graph.contains_record(handle));
				CHECK(f.rd.draw_graph.resolve_record_command_buffer(handle, actual));
				CHECK(actual == RDD::CommandBufferID(11));
			}
		}
	}
	static void preflight() {
		Fixture f;
		const RID va = f.vertex(), ia = f.index();
		RD::NativeTransferDependency transfers[3];
		// An actual prior original graph usage is the conflicting fact.
		f.rd.draw_graph.add_draw_list_usage(&f.vertex_tracker, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		f.rd.draw_graph.add_draw_list_usage(&f.index_tracker, RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ);
		ERR_PRINT_OFF;
		no_effect(f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, va, transfers, 3), ERR_INVALID_PARAMETER);
		no_effect(f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, transfers, 3), ERR_INVALID_PARAMETER);
		CHECK(f.rd.vertex_array_owner.get_or_null(va)->transfer_worker_indices.size() == 3);
		CHECK(f.rd.index_array_owner.get_or_null(ia)->transfer_worker_index == 1);
		CHECK(f.rd.transfer_worker_operation_used_by_draw[0] == 0);
		CHECK(f.rd.transfer_worker_operation_used_by_draw[1] == 0);
		ERR_PRINT_ON;
		f.end();
		f.begin(); // Earlier list usage cannot reject this new original list.
		CHECK(f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, va, transfers, 3).error == OK);
		auto *array = f.rd.index_array_owner.get_or_null(ia);
		array->offset = UINT32_MAX;
		ERR_PRINT_OFF;
		no_effect(f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, transfers, 3), ERR_PARAMETER_RANGE_ERROR);
		ERR_PRINT_ON;
		CHECK(array->transfer_worker_index == 1);
		CHECK_FALSE(f.rd.draw_list.state.index_array.is_valid());
		array->offset = 3;
		CHECK(f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, transfers, 3).error == OK);
		// Another RID with the same tracker/usage is accepted in the same list.
		const RID same_usage = f.vertex();
		CHECK(f.rd._draw_list_bind_vertex_array_recorded(Fixture::LIST, same_usage, transfers, 3).error == OK);
		f.end();
		f.rd.draw_graph.begin();
		f.begin();
		// Prior-frame usage must be reset by the original graph, even when the
		// new frame reuses its first list index.
		f.rd.index_array_owner.get_or_null(ia)->draw_tracker = &f.vertex_tracker;
		f.rd.index_array_owner.get_or_null(ia)->driver_id = RDD::BufferID(31);
		CHECK(f.rd._draw_list_bind_index_array_recorded(Fixture::LIST, ia, transfers, 3).error == OK);
		CHECK(f.vertex_tracker.draw_list_usage == RDG::RESOURCE_USAGE_INDEX_BUFFER_READ);
	}
};

TEST_CASE("[RenderingDevice][CheckedBind] uniform acceptance records CPU state without a native instruction") {
	TestCheckedBindAccess::uniform_state();
}
TEST_CASE("[RenderingDevice][CheckedBind] geometry binds preserve original payload transfers and mapping") {
	SUBCASE("checked") {
		TestCheckedBindAccess::geometry(false);
	}
	SUBCASE("ordinary") {
		TestCheckedBindAccess::geometry(true);
	}
}
TEST_CASE("[RenderingDevice][CheckedBind] original usage conflicts and offset overflow refuse before effects") {
	TestCheckedBindAccess::preflight();
}
