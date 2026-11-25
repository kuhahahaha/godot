/**************************************************************************/
/*  test_production_native_observe.cpp                                    */
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

#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/io/native_drain_diagnostic.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/rendering_server_default.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstring>

TEST_FORCE_LINK(test_production_native_observe)

namespace TestProductionRequest {
void wave_g_deliver_native_invocation(
		RendererViewport &p_owner,
		const Ref<RendererViewport::Viewport::CameraCapture> &p_capture,
		RendererViewport::Viewport::CameraSourceSubscriber *p_subscriber);
} // namespace TestProductionRequest

namespace TestProductionNativeObserve {
struct Access {
	using Registry = GDExtensionDeviceExit::Registry;
	using Request = GDExtensionDeviceExit::ProductionRequest;
	using Physical = GDExtensionDeviceExit::QualificationPhysicalReturn;
	static void before_free(const Physical *cell) {
		if (cell) {
			CHECK(cell->enqueued);
			CHECK(cell->free_event == 0);
		}
	}
	struct QuoteDriver : TransferTestDriverBase {
		// Finite CPU backing emulator. Copies run only when the original graph
		// emits command_copy_buffer; no readback completion fields are injected.
		uint32_t queue_issues = 0, fence_polls = 0, fence_recycles = 0, native_recycles = 0;
		uint32_t sources_per_issue = 1, preparations = 0;
		uint32_t screen_acquires = 0;
		// Opt-in T11 native-boundary controls. The worker itself is allocated,
		// recorded, submitted and recycled only by the original RD methods.
		bool worker_mode = false, source_ready[128] = {};
		uint8_t worker_memory[16 * 1024] = {};
		uint64_t worker_buffer_id = 0, worker_serial = 0, main_serial = 0;
		uint32_t worker_resets = 0, worker_recycles = 0, main_worker_waits = 0;
		uint32_t worker_pool_frees = 0, worker_fence_frees = 0;
		bool worker_retain_required = false;
		bool main_frame_retain_required = false;
		FramebufferID swap_chain_acquire_framebuffer(CommandQueueID, SwapChainID, bool &resize) override {
			++screen_acquires;
			resize = false;
			return {}; // A controlled unavailable surface; never fabricates a drawable.
		}
		bool hidden_mode = false, hidden_pending = false, hidden_busy_seen = false, hidden_ready = false;
		uint64_t hidden_serial = 0;
		uint32_t hidden_issues = 0;
		uint32_t legacy_end_segments = 0, shader_frees = 0;
		bool ordinary_shader_free_allowed = false;
		mutable uint32_t shader_polls = 0;
		bool budget_call_active = false;
		uint32_t receipt_calls() const {
			uint32_t total = queue_issues + fence_polls + fence_recycles + native_recycles + buffer_frees + set_frees + shader_frees + shader_polls;
			for (uint32_t i = 0; i < 128; ++i) {
				total += retains[i] + polls[i] + releases[i];
			}
			return total;
		}
		bool shader_retire_ready_nonblocking(ShaderID shader) const override {
			CHECK(budget_call_active);
			CHECK(shader == ShaderID(80));
			++shader_polls;
			return true; // This finite CPU shader has no asynchronous compiler.
		}
		void shader_free(ShaderID shader) override {
			CHECK(ordinary_shader_free_allowed);
			CHECK(budget_call_active);
			CHECK(shader == ShaderID(80));
			++shader_frees;
		}
		Error native_call_recycle(uint64_t serial) override {
			CHECK((serial > 0 && serial < 128));
			++native_recycles;
			return OK;
		}
		Physical *second_set_cell = nullptr;
		bool allow_second_set = false, second_set_live = false;
		Physical *buffer_cells[32] = {}, *set_cell = nullptr;
		uint32_t buffer_frees = 0, set_creates = 0, set_frees = 0, set_prepares = 0, set_binds = 0;
		uint32_t expected_set_bindings = 1;
		bool set_live = false;
		String set_name;
		UniformSetID uniform_set_create(VectorView<BoundUniform> uniforms, ShaderID shader, uint32_t set, int pool) override {
			CHECK(shader == ShaderID(80));
			CHECK(set == 0);
			CHECK(pool == -1);
			CHECK_FALSE((set_live && (!allow_second_set || second_set_live)));
			CHECK(uniforms.size() == expected_set_bindings);
			if (uniforms.size() != expected_set_bindings || (set_live && (!allow_second_set || second_set_live))) {
				return {};
			}
			for (uint32_t i = 0; i < uniforms.size(); ++i) {
				CHECK(uniforms[i].type == UNIFORM_TYPE_STORAGE_BUFFER);
				CHECK(uniforms[i].binding == i);
				CHECK(uniforms[i].ids.size() == 1);
				if (uniforms[i].ids.size() != 1) {
					return {};
				}
				const uint64_t id = uniforms[i].ids[0].id;
				CHECK(id == uniforms[0].ids[0].id);
				CHECK((id > 0 && id < 32 && live[id]));
				if (!id || id >= 32 || !live[id]) {
					return {};
				}
			}
			++set_creates;
			if (set_live) {
				second_set_live = true;
				return UniformSetID(83);
			}
			set_live = true;
			return UniformSetID(82);
		}
		void (*set_free_observer)(void *, UniformSetID) = nullptr;
		void *set_free_observer_context = nullptr;
		void uniform_set_free(UniformSetID id) override {
			if (set_free_observer) {
				set_free_observer(set_free_observer_context, id);
			}
			if (id == UniformSetID(83)) {
				CHECK(allow_second_set);
				CHECK(second_set_live);
				Access::before_free(second_set_cell);
				second_set_live = false;
				++set_frees;
				return;
			}
			CHECK(id == UniformSetID(82));
			CHECK(set_live);
			Access::before_free(set_cell);
			set_live = false;
			++set_frees;
		}
		void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID set, ShaderID shader, uint32_t index) override {
			CHECK((set == UniformSetID(82) || (allow_second_set && set == UniformSetID(83))));
			CHECK(shader == ShaderID(80));
			CHECK(index == 0);
			CHECK((set == UniformSetID(83) ? second_set_live : set_live));
			++set_prepares;
		}
		uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count) const override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			if (sets.size() == 1) {
				CHECK((sets[0] == UniformSetID(82) || (allow_second_set && sets[0] == UniformSetID(83))));
			}
			return 0;
		}
		void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count, uint32_t dynamic) override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			CHECK(dynamic == 0);
			if (sets.size() == 1) {
				CHECK((sets[0] == UniformSetID(82) || (allow_second_set && sets[0] == UniformSetID(83))));
				CHECK((sets[0] == UniformSetID(83) ? second_set_live : set_live));
			}
			++set_binds;
		}
		uint8_t memory[32][256] = {};
		uint32_t delivery_maps = 0, delivery_unmaps = 0;
		uint64_t delivery_map_id = 0;
		bool fail_delivery_map = false;
		uint32_t sizes[32] = {}, allocations = 0, copy_calls = 0, maps = 0;
		bool live[32] = {};
		BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
			CHECK(bytes > 0);
			const bool worker_staging = worker_mode && bytes == sizeof(worker_memory) && !worker_buffer_id;
			CHECK((bytes <= 256 || worker_staging));
			CHECK(allocations + 1 < 32);
			if (!bytes || (bytes > 256 && !worker_staging) || allocations + 1 >= 32) {
				return {};
			}
			const uint32_t id = ++allocations;
			sizes[id] = uint32_t(bytes);
			live[id] = true;
			if (worker_staging) {
				worker_buffer_id = id;
			}
			return BufferID(id);
		}
		uint8_t *buffer_map(BufferID id) override {
			CHECK((id.id > 0 && id.id < 32 && live[id.id]));
			if (!id.id || id.id >= 32 || !live[id.id]) {
				return nullptr;
			}
			++maps;
			return id.id == worker_buffer_id ? worker_memory : memory[id.id];
		}
		uint8_t *buffer_map_observed(BufferID id, int64_t &native_code) override {
			native_code = 0;
			CHECK(delivery_map_id == 0);
			if (fail_delivery_map) {
				native_code = -7001;
				return nullptr;
			}
			uint8_t *data = buffer_map(id);
			if (data) {
				delivery_map_id = id.id;
				++delivery_maps;
			}
			return data;
		}
		void buffer_unmap(BufferID id) override {
			CHECK((id.id > 0 && id.id < 32 && live[id.id]));
			if (delivery_map_id == id.id) {
				delivery_map_id = 0;
				++delivery_unmaps;
			}
		}
		void buffer_free(BufferID id) override {
			CHECK((id.id > 0 && id.id < 32 && live[id.id]));
			if (id.id > 0 && id.id < 32) {
				Access::before_free(buffer_cells[id.id]);
				++buffer_frees;
				live[id.id] = false;
			}
		}
		void set_object_name(ObjectType type, ID id, const String &name) override {
			CHECK(name.begins_with("RID:"));
			if (type == OBJECT_TYPE_UNIFORM_SET) {
				if (id.id == 83) {
					CHECK(allow_second_set);
					CHECK(second_set_live);
				} else {
					CHECK(id.id == 82);
					CHECK(set_live);
				}
				set_name = name;
			} else {
				CHECK(type == OBJECT_TYPE_BUFFER);
				CHECK((id.id > 0 && id.id < 32 && live[id.id]));
			}
		}
		void command_copy_buffer(CommandBufferID, BufferID from, BufferID to, VectorView<BufferCopyRegion> regions) override {
			REQUIRE((from.id > 0 && from.id < 32 && live[from.id]));
			REQUIRE((to.id > 0 && to.id < 32 && live[to.id]));
			if (!from.id || from.id >= 32 || !to.id || to.id >= 32 || !live[from.id] || !live[to.id]) {
				return;
			}
			for (uint32_t i = 0; i < regions.size(); ++i) {
				const auto &region = regions[i];
				CHECK(region.src_offset + region.size <= sizes[from.id]);
				CHECK(region.dst_offset + region.size <= sizes[to.id]);
				if (region.src_offset + region.size > sizes[from.id] || region.dst_offset + region.size > sizes[to.id]) {
					return;
				}
				uint8_t *destination = to.id == worker_buffer_id ? worker_memory : memory[to.id];
				const uint8_t *source = from.id == worker_buffer_id ? worker_memory : memory[from.id];
				memcpy(destination + region.dst_offset, source + region.src_offset, region.size);
				++copy_calls;
			}
		}
		FenceID fence_create() override {
			CHECK(worker_mode);
			return FenceID(98);
		}
		void fence_free(FenceID id) override {
			CHECK(worker_mode);
			CHECK(id == FenceID(98));
			++worker_fence_frees;
		}
		CommandPoolID command_pool_create(CommandQueueFamilyID family, CommandBufferType type) override {
			CHECK(worker_mode);
			CHECK(family == CommandQueueFamilyID(2));
			CHECK(type == COMMAND_BUFFER_TYPE_PRIMARY);
			return CommandPoolID(97);
		}
		void command_pool_free(CommandPoolID id) override {
			CHECK(worker_mode);
			CHECK(id == CommandPoolID(97));
			++worker_pool_frees;
		}
		uint32_t expected_begin_frame = 0;
		CommandBufferID command_buffer_create(CommandPoolID pool) override { return CommandBufferID(worker_mode && pool == CommandPoolID(97) ? 96 : 91); }
		bool command_buffer_begin(CommandBufferID id) override {
			if (expected_begin_frame == 1) {
				CHECK(id == CommandBufferID(92));
			} else {
				CHECK((id == CommandBufferID(90) || id == CommandBufferID(91) || (worker_mode && id == CommandBufferID(96))));
			}
			return true;
		}
		void command_buffer_end(CommandBufferID id) override { CHECK((id == CommandBufferID(90) || id == CommandBufferID(91) || (worker_mode && id == CommandBufferID(96)))); }
		bool command_pool_reset(CommandPoolID id) override {
			const auto expected_pool = CommandPoolID(expected_begin_frame == 1 ? 96 : 94);
			CHECK((id == expected_pool || (worker_mode && id == CommandPoolID(97))));
			if (id == CommandPoolID(97)) {
				++worker_resets;
			}
			return true;
		}
		void begin_segment(uint32_t frame, uint32_t) override { CHECK(frame == expected_begin_frame); }
		void end_segment() override {
			++legacy_end_segments;
			CHECK_FALSE(budget_call_active);
		}
		SemaphoreID semaphore_create() override { return SemaphoreID(92); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		bool honors_pipeline_barriers = true;
		bool coalesce_compute_passes = false;
		uint32_t compute_pass_begins = 0, compute_pass_ends = 0, compute_memory_barriers = 0;
		void command_begin_compute_pass(CommandBufferID) override { ++compute_pass_begins; }
		void command_end_compute_pass(CommandBufferID) override { ++compute_pass_ends; }
		void command_compute_memory_barrier(CommandBufferID) override { ++compute_memory_barriers; }
		uint64_t api_trait_get(ApiTrait trait) override {
			switch (trait) {
				case API_TRAIT_HONORS_PIPELINE_BARRIERS:
					return honors_pipeline_barriers;
				case API_TRAIT_COALESCE_COMPUTE_PASSES:
					return coalesce_compute_passes;
				case API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE:
				case API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS:
					return 0;
				case API_TRAIT_SHADER_CHANGE_INVALIDATION:
					return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
				default:
					FAIL_CHECK("Unexpected driver trait in production primitive fixture");
					return 0;
			}
		}
		FencePollResult fence_poll(FenceID id, uint64_t serial) override {
			CHECK((id == FenceID(93) || (worker_mode && id == FenceID(98))));
			CHECK(serial > 0);
			++fence_polls;
			if (worker_mode) {
				REQUIRE((serial > 2000 && serial < 2128));
				CHECK(serial == (id == FenceID(98) ? worker_serial : main_serial) + 2000);
				return { source_ready[serial - 2000], native_error, serial, native_error ? -27 : 0 };
			}
			return { complete, native_error, serial, native_error ? -27 : 0 };
		}
		Error fence_recycle(FenceID id, uint64_t serial) override {
			CHECK((id == FenceID(93) || (worker_mode && id == FenceID(98))));
			CHECK(serial > 0);
			if (worker_mode && id == FenceID(98)) {
				CHECK(serial == worker_serial + 2000);
				CHECK(source_ready[worker_serial]);
				if (worker_retain_required) {
					CHECK(retains[worker_serial] > 0);
					CHECK(retain_error == OK);
				}
				++worker_recycles;
			}
			if (main_frame_retain_required && id == FenceID(93) && serial > 2000 && serial < 2128) {
				CHECK(retains[serial - 2000] > 0);
			}
			++fence_recycles;
			return OK;
		}
		uint32_t dispatches = 0, pipeline_binds = 0, pushed[4] = {};
		uint64_t limit_get(Limit limit) override {
			CHECK((limit == LIMIT_MAX_PUSH_CONSTANT_SIZE || limit == LIMIT_MAX_BOUND_UNIFORM_SETS || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z));
			return limit == LIMIT_MAX_PUSH_CONSTANT_SIZE ? 128 : 64;
		}
		void command_bind_compute_pipeline(CommandBufferID, PipelineID id) override {
			CHECK(id == PipelineID(81));
			++pipeline_binds;
		}
		void command_bind_push_constants(CommandBufferID, ShaderID id, uint32_t first, VectorView<uint32_t> data) override {
			CHECK(id == ShaderID(80));
			CHECK(first == 0);
			CHECK(data.size() == 4);
			if (data.size() != 4) {
				return;
			}
			for (uint32_t i = 0; i < 4; ++i) {
				pushed[i] = data[i];
			}
		}
		void command_compute_dispatch(CommandBufferID, uint32_t x, uint32_t y, uint32_t z) override {
			CHECK(x == 2);
			CHECK(y == 3);
			CHECK(z == 4);
			++dispatches;
			if (hidden_mode) {
				hidden_pending = true;
			}
		}
		uint32_t retains[128] = {}, polls[128] = {}, releases[128] = {};
		uint64_t next_serial = 1;
		Registry *loss_registry = nullptr;
		bool callback_blocked = false, callback_delivered = false;
		Error retain_error = OK, release_error = OK, issue_error = OK;
		uint64_t fail_release_serial = 0;
		bool complete = false, native_error = false, unknown = false;
		uint32_t qualification_native_call_bound_for_profile(uint32_t main, uint32_t workers, uint32_t frames) const override { return frames && frames <= main && workers <= UINT32_MAX - main && uint64_t(main + workers) * sources_per_issue <= UINT32_MAX ? (main + workers) * sources_per_issue : 0; }
		// Opt-in capability for the original Height observer fixture only.
		bool height_observation = false;
		bool native_call_observation_supported() const override { return height_observation; }
		bool height_retain_pause = false;
		Error native_call_retain(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++retains[serial];
			// Opt-in original range retain refusal; no source fact is rewritten.
			if (height_retain_pause) return ERR_BUSY;
			return retain_error;
		}
		Error native_call_release(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++releases[serial];
			return serial == fail_release_serial ? release_error : OK;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++polls[serial];
			if (loss_registry) {
				// Probe from another thread: try_lock on a mutex already owned
				// by this thread would itself be undefined behavior in RED.
				struct Probe {
					Registry *registry;
					bool acquired = false;
					static void run(void *data) {
						auto &probe = *static_cast<Probe *>(data);
						probe.acquired = probe.registry->mutex.try_lock();
						if (probe.acquired) {
							probe.registry->mutex.unlock();
						}
					}
				} probe{ loss_registry };
#ifdef THREADS_ENABLED
				Thread thread;
				thread.start(&Probe::run, &probe);
				CHECK_MESSAGE(thread.is_started(), "loss callback probe thread must start");
				if (!thread.is_started()) {
					callback_blocked = true;
					return {};
				}
				thread.wait_to_finish();
#else
				Probe::run(&probe); // No-thread builds use the dummy mutex.
#endif
				callback_blocked = !probe.acquired;
				if (probe.acquired) {
					loss_registry->device_lost(Enrollment::INSTANCE, 1);
					callback_delivered = true;
				}
			}

			return { (worker_mode ? source_ready[serial] : complete) && (serial != hidden_serial || hidden_ready), native_error, serial + 1000, native_error ? -27 : 0, unknown ? IssueFact::NotIssued : IssueFact::KnownAccepted };
		}
		Error qualification_prepare_segment(QueueCallReport &report, bool nonblocking = true) override {
			CHECK(nonblocking);
			++preparations;
			report = {};
			if (!hidden_mode || !hidden_pending) {
				return OK;
			}
			if (!hidden_busy_seen) {
				hidden_busy_seen = true;
				return ERR_BUSY;
			}
			CHECK(hidden_serial == 0);
			NativeCallFact fact;
			fact.serial = next_serial++;
			fact.completion_serial = fact.serial + 1000;
			fact.queue_index = 2;
			fact.completion = CompletionCapability::Fence;
			fact.issue = IssueFact::KnownAccepted;
			hidden_serial = fact.serial;
			hidden_pending = false;
			++hidden_issues;
			++queue_issues; // The backend's actual post-graph hidden issue.
			report.calls.push_back(fact);
			report.issued = true;
			report.native_issue_count = 1;
			return OK;
		}
		bool native_issue_bound(NativeIssueKind kind, uint32_t commands, uint32_t &bound) const override {
			switch (kind) {
				case NativeIssueKind::MAIN_SLOT:
				case NativeIssueKind::TRANSFER_WORKER:
					bound = commands ? sources_per_issue : 0;
					return true;
				case NativeIssueKind::PREPARE_SEGMENT:
					bound = hidden_mode ? 1 : 0;
					return true;
			}
			return false;
		}
		uint32_t record_native_call_bound(uint32_t slots) const override { return slots * sources_per_issue; }
		Error command_queue_execute_and_present(CommandQueueID queue, VectorView<SemaphoreID> waits, VectorView<CommandBufferID> commands, VectorView<SemaphoreID> signals, FenceID fence, VectorView<SwapChainID>) override {
			CHECK((sources_per_issue == 1 || sources_per_issue == 2));
			if (worker_mode) {
				REQUIRE(commands.size() == 1);
				if (queue == CommandQueueID(2)) {
					CHECK(commands[0] == CommandBufferID(96));
					CHECK(fence == FenceID(98));
					CHECK(waits.size() == 0);
					REQUIRE(signals.size() == 1);
					CHECK(signals[0] == SemaphoreID(92));
				} else {
					CHECK(queue == CommandQueueID(1));
					CHECK(commands[0] == CommandBufferID(90));
					REQUIRE(waits.size() == 1);
					CHECK(waits[0] == SemaphoreID(92));
					++main_worker_waits;
				}
			}
			QueueCallReport report;
			for (uint32_t source = 0; source < sources_per_issue; ++source) {
				uint32_t index = UINT32_MAX;
				if (bounded_native_capture_active() && !claim_native_fact(index)) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				++queue_issues; // Each controlled backend native call's actual frontier.
				NativeCallFact fact;
				fact.serial = next_serial++;
				fact.completion_serial = fact.serial + 1000;
				fact.queue_index = worker_mode ? queue.id : 1;
				if (worker_mode) {
					(queue == CommandQueueID(2) ? worker_serial : main_serial) = fact.serial;
				}
				fact.completion = CompletionCapability::Fence;
				fact.issue = unknown ? IssueFact::IssuedUnknown : IssueFact::KnownAccepted;
				fact.error = issue_error;
				fact.native_code = issue_error == OK ? 0 : -9;
				if (bounded_native_capture_active()) {
					finish_native_fact(index, fact, fact.serial + 2000);
				} else {
					report.calls.push_back(fact);
					report.submission_serial = fact.serial + 2000;
				}
			}
			if (!bounded_native_capture_active()) {
				report.issued = true;
				report.native_issue_count = sources_per_issue;
				report.error = issue_error;
				report.native_code = issue_error == OK ? 0 : -9;
				record_queue_call_report(report);
			}
			return issue_error;
		}
		NativeCallFact issue() {
			QueueCallReport report;
			command_queue_execute_and_present_observed(CommandQueueID(1), {}, CommandBufferID(2), {}, FenceID(3), {}, report);
			REQUIRE(report.calls.size() == 1);
			if (report.calls.size() != 1) {
				return {};
			}
			return report.calls[0];
		}
	};
	struct BudgetDriverGuard {
		QuoteDriver &driver;
		bool previous;
		uint32_t legacy_before;
		explicit BudgetDriverGuard(QuoteDriver &p_driver) : driver(p_driver), previous(p_driver.budget_call_active), legacy_before(p_driver.legacy_end_segments) { driver.budget_call_active = true; }
		~BudgetDriverGuard() {
			CHECK(driver.legacy_end_segments == legacy_before);
			driver.budget_call_active = previous;
		}
	};
	struct Holder : GDExtensionDeviceExit::HolderRetirement {
		Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
	};
	// Controlled original driver harness; no real GPU is initialized.
	struct Enrollment {
		RenderingDevice *previous = RenderingDevice::get_singleton();
		RenderingDevice rd;
		QuoteDriver driver;
		Registry *registry = memnew(Registry);
		Holder holder;
		Registry::QualificationBridge *original_bridge = nullptr;
		Registry::QualificationBridge::NativeUnit *original_unit = nullptr;
		Registry::QualificationBridge::Storage *original_storage = nullptr;
		RID original_shader, original_pipeline;
		GDExtensionQualificationBridgeRefV1 qualification = {};
		GDExtensionDeviceExit::RecordRef reference;
		GDExtensionDeviceExit::RegistrationContext frame;
		static constexpr GDExtensionDeviceExitAbi ABI = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		static constexpr uint64_t INSTANCE = 9101;
		static void original_terminator(void *, GDExtensionInitializationLevel) {}
		Enrollment() {
			using namespace GDExtensionDeviceExit;
			rd.driver = &driver;
			RenderingDevice::singleton = &rd;
			rd.is_main_instance = true;
			rd.render_thread_id = Thread::get_caller_id();
			rd.frames.resize(1);
			rd.transfer_worker_pool_max_size = 0;
			// One real graph metadata command is the prefix being quoted. It is never
			// issued to a driver and is unrelated to the production request's effects.
			rd.draw_graph.add_capture_timestamp(RDD::QueryPoolID(1), 0);
			registry->instance = INSTANCE;
			auto &record = registry->records[0];
			record.occupied = record.attached = true;
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = 1;
			record.producer_registration_serial = 2;
			record.producer_attachment_identity = this;
			record.producer_code_holder = 3;
			record.guard_enrollment = GuardEnrollment::QUALIFIED;
			record.scene_ready_state = Registry::Record::SceneReadyState::IN_FRAME;
			auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
			scene.initialized = true;
			scene.binding_action.serial = 4;
			scene.binding_action.generation = 1;
			scene.binding_action.owner_thread = record.owner_thread;
			scene.binding_action.level = GDEXTENSION_INITIALIZATION_SCENE;
			scene.binding_action.kind = TerminalActionKind::LEVEL_BINDING;
			scene.binding_action.services = 1u << 5;
			scene.binding_action.wrapper = &original_terminator;
			scene.binding_action.userdata = this;
			record.original_initialization.deinitialize = &original_terminator;
			record.original_initialization.userdata = this;
			registry->live_services = 1u << 5;
			record.private_refs = 1;
			holder.typed_code_hold = true;
			holder.handle_serial = 3;
			holder.generation = 1;
			holder.acquired_thread = record.owner_thread;
			holder.terminal_action = 4;
			record.holders = &holder;
			auto &device = registry->device_records[0];
			device.object = &rd;
			device.live = true;
			device.epoch = 1;
			device.session = 1;
			device.execution_thread = rd.render_thread_id;
			registry->device_refs[0] = { 6, 0, 0, false, false };
			registry->device_refs[1] = { 7, 0, 0, true, false };
			reference = RecordRef(registry, 0);
			frame.enter(reference, true);
			frame.frame_id = 8;
			frame.level_phase = LevelPhase::SCENE_READY;
			registry->views[0] = { 5, 0, Registry::ViewDomain::SCENE_READY, 8, 0 };
			registry->views[1] = { 9, 0, Registry::ViewDomain::PRODUCER_REGISTRATION, 0, 0 };
			registry->note_view_slot(1);
		}
		~Enrollment() {
			// Test teardown restores TLS/singleton even after REQUIRE. Injected
			// driver cells are discarded here, never credited as production proof.
			// Metadata itself returns through its original allocated owner.
			for (auto *attachment : registry->production_attachments) {
				if (!attachment) {
					continue;
				}
				for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
					auto &request = attachment->requests[i];
					if (request.serial) {
						// Failed-fixture abandonment only: discard an unconsumed
						// mechanical mailbox before the arena teardown below. This
						// credits no production execution, completion, or return.
						request.control = {};
						for (uint32_t slot = 0; slot < request.resource_count; ++slot) {
							request.resources[slot].creating = false;
						}
						request.native_count = 0;
						request.graph_count = 0;
						request.source_device_lost = false;
						request.effects_pending = false;
						// Fixture abandonment only: return actual backing before the
						// finite arena is destroyed, never a successful release claim.
						rd.frames[0].download_buffer_get_data_requests.clear();
						{
							MutexLock lock(registry->mutex);
							registry->production_unpin_resources_locked(request);
						}
						for (uint32_t slot = 0; slot < request.resource_count; ++slot) {
							auto &resource = request.resources[slot];
							if (resource.owned_serial && rd.uniform_set_owner.owns(resource.primary)) {
								rd.qualification_retiring = true;
								rd.free_rid(resource.primary);
								rd.qualification_retiring = false;
							}
						}
						rd._free_pending_resources(0);
						for (uint32_t slot = 0; slot < request.resource_count; ++slot) {
							auto &resource = request.resources[slot];
							if (resource.owned_serial && rd.storage_buffer_owner.owns(resource.primary)) {
								rd.qualification_retiring = true;
								rd.free_rid(resource.primary);
								rd.qualification_retiring = false;
							}
							request.returns[slot].pending = false;
						}
						rd._free_pending_resources(0);
						request.readback_count = 0;

						registry->production_cancel({ INSTANCE, request.serial });
						registry->production_release({ INSTANCE, request.serial });
					}
				}
				registry->production_close({ INSTANCE, attachment->serial });
			}
			if (original_bridge) {
				if (original_unit) {
					CHECK(original_unit->shader.production_uses == nullptr);
					CHECK(original_unit->pipeline.production_uses == nullptr);
					CHECK(original_unit->shader.dependent_children.load() == 0);
					if (rd.compute_pipeline_owner.owns(original_pipeline)) {
						rd.compute_pipeline_owner.free(original_pipeline);
					}
					if (rd.shader_owner.owns(original_shader)) {
						rd.shader_owner.free(original_shader);
					}
					original_bridge->units[0] = nullptr;
					memdelete(original_unit);
				}
				if (original_storage) {
					CHECK(original_storage->physical.production_uses == nullptr);
					const RID rid = RID::from_uint64(original_storage->backing.rid);
					if (rd.storage_buffer_owner.owns(rid)) {
						rd.qualification_retiring = true;
						rd.free_rid(rid);
						rd.qualification_retiring = false;
						rd._free_pending_resources(0);
					}
					original_bridge->storage[0] = nullptr;
					memdelete(original_storage);
				}
				registry->qualification_bridges[0] = nullptr;
				memdelete(original_bridge);
			}
			if (qualification.handle_serial) {
				registry->qualification_close(qualification);
			}
			frame.leave();
			reference.reset();
			registry->records[0].holders = nullptr;
			memdelete(registry);
			for (auto &block : rd.upload_staging_buffers.blocks) {
				driver.buffer_unmap(block.driver_id);
				driver.buffer_free(block.driver_id);
			}
			for (auto &block : rd.download_staging_buffers.blocks) {
				driver.buffer_unmap(block.driver_id);
				driver.buffer_free(block.driver_id);
			}
			rd.upload_staging_buffers.blocks.clear();
			rd.download_staging_buffers.blocks.clear();
			rd.driver = nullptr;
			rd.frames.clear();
			rd.native_finalization_returned = true;
			RenderingDevice::singleton = previous;
		}
		GDExtensionProductionAttachmentRefV1 accept(const GDExtensionProductionCapacityV1 &capacity) {
			GDExtensionProductionMetadataQuoteV1 quote = {};
			CHECK(rd.draw_graph.qualification_command_count() == 1);
			const auto production_quote_error = registry->production_quote(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, &capacity, &quote).native_error;
			REQUIRE(production_quote_error == OK);
			if (production_quote_error != OK) {
				return {};
			}
			// One existing graph command per request, plus each reserved graph;
			// every possible frame also reserves main + worker source slots.
			const uint64_t expected_frames = uint64_t(capacity.graph_memberships) + capacity.requests;
			const uint64_t expected_slots = uint64_t(capacity.requests) + capacity.graph_memberships + expected_frames * (uint64_t(rd.transfer_worker_pool_max_size) + 1);
			CHECK(quote.graph_slots == expected_slots);
			GDExtensionProductionAttachmentRefV1 attachment = {};
			GDExtensionProductionInstallationReportV1 report = {};
			const auto production_accept_error = registry->production_accept(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, { INSTANCE, 3 }, &quote, &attachment, &report).native_error;
			REQUIRE(production_accept_error == OK);
			if (production_accept_error != OK) {
				return {};
			}
			CHECK(report.state == GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1);
			CHECK(report.allocated_bytes == quote.allocation_bytes);
			CHECK(report.returned_bytes == 0);
			return attachment;
		}
		Error reserve(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionCapacityV1 &capacity, uint64_t nonce, Request *&out) {
			MutexLock lock(registry->mutex);
			GDExtensionDeviceExit::ProductionAttachment *attachment = nullptr;
			RenderingDevice *device = nullptr;
			Error e = registry->production_attachment_locked(ref, true, attachment, device);
			if (e != OK) {
				return e;
			}
			const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), nonce, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			return registry->production_reserve_metadata_locked(*attachment, operation, capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, out);
		}
	};

	static GDExtensionProductionCapacityV1 capacity(uint32_t n = 3) {
		GDExtensionProductionCapacityV1 c = {};
		c.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>();
		c.requests = 1;
		c.suboperations = c.resources = c.resource_uses = c.graph_memberships = c.return_cells = n;
		// This driver has one source per graph, no worker or hidden source.
		// Membership storage counts graph-source edges, not command capacity.
		c.native_memberships = n;
		return c;
	}
	static GDExtensionProductionServiceBudgetV1 budget(uint32_t receipts = 128, uint32_t passes = 64) { return { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, receipts, passes, 0 }; }
	static Request *setup(Enrollment &e, uint32_t n = 3) {
		auto c = capacity(n);
		auto a = e.accept(c);
		Request *r = nullptr;
		REQUIRE(e.reserve(a, c, 31, r) == OK);
		return r;
	}
	static Request *setup_v2(Enrollment &e) {
		using namespace GDExtensionDeviceExit;
		const auto c = capacity(1); const auto ref = e.accept(c); Request *r = nullptr;
		MutexLock lock(e.registry->mutex); ProductionAttachment *attachment = nullptr; RenderingDevice *rd = nullptr;
		REQUIRE(e.registry->production_attachment_locked(ref, true, attachment, rd) == OK);
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 312, 1, 10, 11 };
		const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
		REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, &resource, 1, &use, 1, {}, 0, r, 2) == OK);
		return r;
	}

	static void capture(Enrollment &e, Request &r, uint32_t count) {
		// Rows originate only from this strict driver's actual issue/report call.
		// This test-only enrollment does not stand in for B's production window.
		for (uint32_t i = 0; i < count; ++i) {
			auto &c = r.natives[i];
			c.request = &r;
			c.owner_index = i;
			c.member.source = e.driver.issue();
		}
		r.native_count = count;
	}
	static GDExtensionDeviceExit::ProductionControlOutput turn(Enrollment &e, Request &r, GDExtensionDeviceExit::ProductionControlKind kind, const GDExtensionProductionServiceBudgetV1 &b) {
		BudgetDriverGuard budget_guard(e.driver);
		uint64_t serial = 0;
		auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r.serial };
		const Error accepted = e.registry->production_control_accept(ref, kind, &serial, &b);
		REQUIRE(accepted == OK);
		if (accepted != OK) {
			return {};
		}
		// Exact fixed mailbox dispatch; original FIFO publication is covered by the
		// existing ProductionRequest singleton tests. No second queue is created.
		uint64_t retry = 0;
		REQUIRE(e.registry->production_control_accept(ref, kind, &retry, &b) == OK);
		CHECK(retry == serial);
		auto changed = b;
		++changed.receipts;
		CHECK(e.registry->production_control_accept(ref, kind, &retry, &changed) == ERR_BUSY);
		r.control.state = GDExtensionDeviceExit::ProductionControlState::QUEUED;
		{
			MutexLock lock(e.registry->mutex);
			e.registry->production_control_execute_locked(r, &e.rd);
		}
		GDExtensionDeviceExit::ProductionControlOutput out = {};
		REQUIRE(e.registry->production_control_consume(ref, serial, &out) == OK);
		return out;
	}
	static GDExtensionDeviceExit::ProductionControlOutput execute_public_control(Enrollment &e, GDExtensionProductionRequestRefV1 ref, GDExtensionDeviceExit::ProductionControlKind kind, uint32_t slot = UINT32_MAX) {
		using namespace GDExtensionDeviceExit;
		uint64_t serial = 0;
		{
			MutexLock lock(e.registry->mutex);
			Request *request = nullptr;
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_request_locked(ref, false, request, rd) == OK);
			REQUIRE(request != nullptr);
			REQUIRE(rd == &e.rd);
			auto &cell = request->control;
			REQUIRE(cell.state == ProductionControlState::PENDING);
			REQUIRE(cell.serial != 0);
			REQUIRE(cell.input.request_serial == ref.handle_serial);
			REQUIRE(cell.input.record_generation == request->operation.record_generation);
			REQUIRE(cell.input.kind == kind);
			if (kind == ProductionControlKind::STORAGE_CREATE || kind == ProductionControlKind::SET_CREATE) {
				REQUIRE(cell.input.resource_slot == slot);
				REQUIRE(slot < request->resource_count);
				REQUIRE(request->resources[slot].creating);
				REQUIRE(request->resources[slot].owned_serial != 0);
			}
			serial = cell.serial;
			// Mechanical fixed-cell fixture only: the checked public entry already
			// accepted this input. The local Registry cannot publish to the global
			// FIFO. No direct create accept, origin reconstruction, or RT grant.
			cell.state = ProductionControlState::QUEUED;
			e.registry->production_control_execute_locked(*request, rd);
			CHECK(cell.state == ProductionControlState::RETURNED);
		}
		ProductionControlOutput output = {};
		REQUIRE(e.registry->production_control_consume(ref, serial, &output) == OK);
		CHECK(output.request_serial == ref.handle_serial);
		CHECK(output.control_serial == serial);
		return output;
	}
	static GDExtensionDeviceSessionResultV1 create_storage(Enrollment &e, GDExtensionProductionRequestRefV1 ref, uint32_t slot, const GDExtensionStorageCreateRequestV1 *origin, GDExtensionStorageBackingV1 *backing) {
		const auto allocations = e.driver.allocations;
		const auto receipts = e.driver.receipt_calls();
		const auto accepted = e.registry->production_storage_create(ref, slot, origin, backing);
		REQUIRE(accepted.native_error == ERR_UNAVAILABLE);
		CHECK(e.driver.allocations == allocations);
		CHECK(e.driver.receipt_calls() == receipts);
		const auto output = execute_public_control(e, ref, GDExtensionDeviceExit::ProductionControlKind::STORAGE_CREATE, slot);
		REQUIRE(output.result.native_error == OK);
		// Exact public retry reads the result of the original frozen origin.
		const auto retry_allocations = e.driver.allocations;
		const auto result = e.registry->production_storage_create(ref, slot, origin, backing);
		CHECK(e.driver.allocations == retry_allocations);
		return result;
	}
	static GDExtensionDeviceSessionResultV1 create_set(Enrollment &e, GDExtensionProductionRequestRefV1 ref, uint32_t slot, uint32_t unit, uint32_t set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t count) {
		const auto creates = e.driver.set_creates;
		const auto receipts = e.driver.receipt_calls();
		const auto accepted = e.registry->production_set_create(ref, slot, unit, set, bindings, count);
		if (e.rd.render_thread_id == Thread::get_caller_id()) {
			REQUIRE(accepted.native_error == OK);
			CHECK(e.driver.set_creates == creates + 1);
			CHECK(e.driver.receipt_calls() == receipts);
			const auto retry_creates = e.driver.set_creates;
			const auto result = e.registry->production_set_create(ref, slot, unit, set, bindings, count);
			CHECK(e.driver.set_creates == retry_creates);
			return result;
		}
		REQUIRE(accepted.native_error == ERR_UNAVAILABLE);
		CHECK(e.driver.set_creates == creates);
		CHECK(e.driver.receipt_calls() == receipts);
		const auto output = execute_public_control(e, ref, GDExtensionDeviceExit::ProductionControlKind::SET_CREATE, slot);
		REQUIRE(output.result.native_error == OK);
		const auto retry_creates = e.driver.set_creates;
		const auto result = e.registry->production_set_create(ref, slot, unit, set, bindings, count);
		CHECK(e.driver.set_creates == retry_creates);
		return result;
	}
	static void bounds_and_fairness() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 3);
		auto b = budget(1, 1);
		auto zero = b;
		zero.receipts = 0;
		const auto revision = r->revision;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, zero);
		CHECK(out.service_usage.receipts == 0);
		CHECK(r->observe_cursor == 0);
		CHECK(r->revision == revision);
		auto expired = b;
		expired.deadline_micros = 1;
		turn(e, *r, ProductionControlKind::OBSERVE, expired);
		CHECK(r->observe_cursor == 0);
		for (uint32_t i = 1; i <= 3; ++i) {
			out = turn(e, *r, ProductionControlKind::OBSERVE, b);
			CHECK(out.service_usage.receipts == 1);
			CHECK(out.service_usage.passes == 1);
			CHECK(e.driver.retains[i] == 1);
			CHECK(e.driver.polls[i] == 0);
		}
		for (uint32_t i = 1; i <= 3; ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, b);
			CHECK(e.driver.polls[i] == 1);
			CHECK(e.driver.retains[i] == 1);
		}
		e.driver.complete = true;
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.report.complete_obligations == 3);
		CHECK(out.report.known_obligations == 3);
		CHECK(out.report.recorded_action_end == 0);
		CHECK(out.report.operation.canonical_begin == 10);
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 0);
		CHECK(e.driver.polls[1] == 2);
	}
	static void alias_and_errors() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		e.driver.issue_error = FAILED;
		capture(e, *r, 1);
		r->natives[1].request = r;
		r->natives[1].owner_index = 0;
		r->natives[1].member.source = r->natives[0].member.source;
		r->native_count = 2;
		e.driver.native_error = true;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(e.driver.retains[1] == 1);
		CHECK(e.driver.polls[1] == 1);
		CHECK(out.report.fact_count == 2);
		CHECK(out.report.known_obligations == 1);
		CHECK(r->natives[0].observation.completion_error == FAILED);
		CHECK(r->natives[0].observation.error == FAILED);
		GDExtensionProductionFactV1 facts[2] = {};
		uint32_t written = 0;
		auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		REQUIRE(e.registry->production_facts(ref, r->revision, 0, facts, 2, &written).native_error == OK);
		CHECK(written == 2);
		CHECK(facts[0].issue_error == FAILED);
		CHECK(facts[0].issue_native_code == -9);
		CHECK(facts[0].completion_native_code == -27);
		CHECK(facts[0].completion == 3);
		CHECK(facts[1].native_serial == facts[0].native_serial);
		e.driver.native_error = false;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(r->natives[0].observation.completion_error == OK);
		CHECK(r->natives[0].observation.error == FAILED);
		CHECK(e.driver.polls[1] == 2);
		const auto old = r->revision;
		r->natives[1].member.source.completion_serial++;
		CHECK(e.registry->production_facts(ref, old, 0, facts, 2, &written).native_error == ERR_INVALID_DATA);
		r->natives[1].member.source = r->natives[0].member.source;
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
	}
	static void loss_and_retention() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		e.driver.unknown = true;
		capture(e, *r, 1);
		e.driver.retain_error = ERR_BUSY;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 1);
		CHECK(e.driver.polls[1] == 0);
		CHECK(r->natives[0].member.retention_error == ERR_BUSY);
		e.driver.retain_error = OK;
		e.driver.complete = true;
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.service_usage.receipts == 2);
		CHECK(out.report.complete_obligations == 0);
		CHECK(out.report.possible_obligations == 1);
		e.registry->device_records[0].live = false;
		auto zero = budget();
		zero.controls = 0;
		auto revision = r->revision;
		turn(e, *r, ProductionControlKind::OBSERVE, zero);
		CHECK_FALSE(r->source_device_lost);
		CHECK(r->revision == revision);
		out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(r->source_device_lost);
		CHECK(out.result.native_error == ERR_UNAVAILABLE);
		CHECK(e.driver.polls[1] == 1);
		e.registry->device_records[0].live = true;
		e.driver.unknown = false;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(e.driver.polls[1] == 1);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, r->serial }).native_error == ERR_BUSY);
	}
	static void partial_release() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 3);
		e.driver.complete = true;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		const auto children = r->attachment->children;
		const auto revision = r->revision;
		auto zero = budget();
		zero.receipts = 0;
		auto out = turn(e, *r, ProductionControlKind::RELEASE, zero);
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
		CHECK(r->revision == revision);
		CHECK(r->release_cursor == 0);
		e.driver.fail_release_serial = 2;
		e.driver.release_error = ERR_BUSY;
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(out.owner_usage.receipts == 3);
		CHECK(e.driver.releases[1] == 1);
		CHECK(e.driver.releases[2] == 1);
		CHECK(e.driver.releases[3] == 1);
		CHECK(r->attachment->children == children);
		CHECK(r->natives[0].member.owner_released);
		CHECK_FALSE(r->natives[1].member.owner_released);
		CHECK(r->release_started);
		auto *attachment = r->attachment;
		e.driver.release_error = OK;
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == OK);
		CHECK(out.owner_usage.receipts == 1);
		CHECK(e.driver.releases[1] == 1);
		CHECK(e.driver.releases[2] == 2);
		CHECK(e.driver.releases[3] == 1);
		CHECK(attachment->children == children - 1);
		CHECK(r->serial == 0);
	}
	static void expired_owner_turn() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 1);
		e.driver.complete = true;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		auto b = budget();
		b.deadline_micros = OS::get_singleton()->get_ticks_usec() + 1000;
		uint64_t serial = 0;
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const Error accepted = e.registry->production_control_accept(ref, ProductionControlKind::RELEASE, &serial, &b);
		REQUIRE(accepted == OK);
		if (accepted != OK) {
			return;
		}
		r->control.state = ProductionControlState::QUEUED;
		{
			MutexLock lock(e.registry->mutex);
			e.registry->production_control_execute_locked(*r, &e.rd);
		}
		OS::get_singleton()->delay_usec(2000);
		ProductionControlOutput out = {};
		REQUIRE(e.registry->production_control_consume(ref, serial, &out) == OK);
		CHECK(out.owner_return.native_error == ERR_BUSY);
		CHECK(out.owner_usage.receipts == 0);
		CHECK(e.driver.releases[1] == 0);
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(e.registry->production_control_consume(ref, serial, &out) == ERR_DOES_NOT_EXIST);
		out = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(out.owner_return.native_error == OK);
		CHECK(e.driver.releases[1] == 1);
	}
	static void synchronous_loss_callback() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e);
		if (!r) {
			return;
		}
		capture(e, *r, 2);
		e.driver.loss_registry = e.registry;
		e.driver.native_error = true;
		const auto children = r->attachment->children;
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK_FALSE(e.driver.callback_blocked);
		CHECK(e.driver.callback_delivered);
		CHECK_FALSE(e.registry->device_records[0].live);
		CHECK(out.result.native_error == ERR_UNAVAILABLE);
		CHECK(r->source_device_lost);
		CHECK(r->natives[0].member.retained);
		CHECK(r->natives[0].observation.completion_error == FAILED);
		CHECK(r->natives[0].observation.completion_native_code == -27);
		CHECK(r->natives[0].observation.state ==
				NativeSourceObservation::State::Unknown);
		CHECK(e.driver.retains[1] == 1);
		CHECK(e.driver.polls[1] == 1);
		CHECK(e.driver.retains[2] == 0);
		CHECK(e.driver.polls[2] == 0);
		CHECK(out.report.complete_obligations == 0);
		CHECK(r->attachment->children == children);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, r->serial })
						.native_error == ERR_BUSY);
		CHECK(e.driver.releases[1] == 0);
		e.driver.loss_registry = nullptr;
		e.registry->device_records[0].live =
				true; // Fixture teardown only, never progress.
	}
	static void graph_initialize(Enrollment &e) {
		e.rd.draw_graph.initialize(&e.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		e.rd.draw_graph.begin();
		e.rd.frames[0].qualification_epoch = 1;
		e.rd.frames[0].fence = RDD::FenceID(93);
		e.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		e.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		e.rd.main_queue = RDD::CommandQueueID(1);
		e.rd.frames_drawn = 3;
	}
	static bool import_original_unit(Enrollment &e, Request &r, bool writable = true, bool alias = false, bool alias_writable = true) {
		using namespace GDExtensionDeviceExit;
		// Reuse TestProductionResourceUseAccess's original bridge/unit metadata
		// enrollment. Public production_import_unit performs all original
		// origin/backing/RID/physical checks; this is no compiler or Runtime grant.
		if (!e.original_bridge) {
			e.original_bridge = memnew(Registry::QualificationBridge);
			e.original_bridge->serial = 101;
			e.original_bridge->epoch = r.attachment->epoch;
			e.original_bridge->library = r.attachment->library;
			e.original_bridge->device = r.attachment->device;
			e.registry->qualification_bridges[0] = e.original_bridge;
		}
		e.original_unit = memnew(Registry::QualificationBridge::NativeUnit);
		auto &unit = *e.original_unit;
		unit.serial = 102;
		unit.creating = false;
		e.original_bridge->units[0] = &unit;
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(80);
		shader.qualification_return = &unit.shader;
		shader.uniform_sets.resize(1);
		shader.set_formats.push_back(123);
		RenderingDevice::ShaderUniform reflection;
		reflection.binding = 0;
		reflection.type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		reflection.length = 128;
		reflection.writable = writable;
		shader.uniform_sets.write[0].push_back(reflection);
		if (alias) {
			reflection.binding = 1;
			reflection.writable = alias_writable;
			shader.uniform_sets.write[0].push_back(reflection);
		}
		e.original_shader = e.rd.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(81);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = e.original_shader;
		pipeline.qualification_return = &unit.pipeline;
		pipeline.set_formats.push_back(123);
		pipeline.push_constant_size = 16;
		pipeline.local_group_size[0] = pipeline.local_group_size[1] = pipeline.local_group_size[2] = 1;
		e.original_pipeline = e.rd.compute_pipeline_owner.make_rid(pipeline);
		unit.shader.bridge = unit.pipeline.bridge = e.original_bridge->serial;
		unit.shader.child = unit.pipeline.child = unit.serial;
		unit.shader.rid = e.original_shader.get_id();
		unit.pipeline.rid = e.original_pipeline.get_id();
		unit.shader.serial = 131;
		unit.pipeline.serial = 132;
		unit.origin.abi = Enrollment::ABI;
		unit.origin.library_generation = r.attachment->library_generation;
		unit.origin.operation_serial = 115;
		unit.origin.container_byte_count = 16;
		unit.origin.container_digest[0] = 1;
		unit.origin.record_slot = 4;
		unit.origin.physical_unit = 5;
		unit.origin.preparation_generation = 6;
		unit.backing.abi = Enrollment::ABI;
		unit.backing.unit = { Enrollment::INSTANCE, unit.serial };
		unit.backing.shader_rid = unit.shader.rid;
		unit.backing.pipeline_rid = unit.pipeline.rid;
		unit.backing.shader_serial = unit.shader.serial;
		unit.backing.pipeline_serial = unit.pipeline.serial;
		unit.backing.known_mask = 3;
		unit.backing.state = 1;
		const auto error = e.registry->production_import_unit({ Enrollment::INSTANCE, r.serial }, 0, &unit.backing).native_error;
		CHECK(error == OK);
		return error == OK;
	}
	static Request *owned_setup(Enrollment &e, bool dispatch, uint32_t mismatch = 0, bool prepare_dispatch = true) {
		using namespace GDExtensionDeviceExit;
		auto c = capacity(8);
		c.retained_payload_bytes = 16;
		auto attachment_ref = e.accept(c);
		if (!attachment_ref.handle_serial) {
			return nullptr;
		}
		ProductionAttachment *attachment = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 171, 1, 10, 11 };
		GDExtensionProductionActionV1 action = { 10, uint32_t(dispatch ? GDEXTENSION_PRODUCTION_DISPATCH_V1 : GDEXTENSION_PRODUCTION_UPLOAD_V1), 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 }
		};
		const GDExtensionProductionResourceV1 storage_resources[] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 }
		};
		GDExtensionProductionUseV1 use = { dispatch ? 1u : 0u, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
		if (mismatch == 1) {
			action.use_count = 0;
		}
		if (mismatch == 2) {
			use.resource_slot = 0;
			use.byte_size = 0;
		}
		if (mismatch == 3) {
			use.resource_slot = 3;
		}
		if (mismatch == 4) {
			use.access = GDEXTENSION_PRODUCTION_WRITE_V1;
		}
		if (mismatch == 5) {
			use.byte_size = 64;
		}
		if (mismatch == 6) {
			use.access = GDEXTENSION_PRODUCTION_READ_V1;
		}
		const bool aliases = mismatch >= 7 && mismatch <= 9;
		const bool metadata = mismatch >= 10;
		GDExtensionProductionUseV1 uses[3] = { use, { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 128, 0 }, {} };
		if (aliases) {
			action.use_count = 2;
			uses[0].access = GDEXTENSION_PRODUCTION_READ_V1;
			if (mismatch == 8) {
				uses[0].byte_size = 64;
				uses[1].byte_offset = 64;
				uses[1].byte_size = 64;
			}
		}
		if (metadata) {
			action.use_count = mismatch == 12 ? 3 : 2;
			uses[0].access = GDEXTENSION_PRODUCTION_READ_V1;
			uses[1] = { mismatch == 11 ? 2u : 0u, GDEXTENSION_PRODUCTION_READ_V1, 0, 0, 0 };
			uses[2] = { 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 0, 0 };
		}
		{
			MutexLock lock(e.registry->mutex);
			Error error = e.registry->production_attachment_locked(attachment_ref, true, attachment, rd);
			if (error == OK) {
				error = e.registry->production_reserve_metadata_locked(*attachment, operation, c, &action, 1, dispatch ? resources : storage_resources, dispatch ? (mismatch == 3 ? 4 : 3) : 2, uses, action.use_count ? action.use_count : 1, nullptr, 0, r);
			}
			CHECK(error == OK);
			if (error != OK) {
				return nullptr;
			}
		}
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		for (uint32_t i = dispatch ? 1u : 0u; i < (dispatch && mismatch == 3 ? 4u : 2u); ++i) {
			if (i == 2) {
				continue;
			}
			const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 128, i, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			const auto error = create_storage(e, ref, i, &origin, &backing).native_error;
			CHECK(error == OK);
			if (error != OK) {
				return nullptr;
			}
			const auto allocations = e.driver.allocations;
			GDExtensionStorageBackingV1 retry = {};
			CHECK(e.registry->production_storage_create(ref, i, &origin, &retry).native_error == OK);
			CHECK(e.driver.allocations == allocations);
			CHECK(retry.rid == backing.rid);
			CHECK(retry.backing_serial == backing.backing_serial);
			const auto *buffer = e.rd.storage_buffer_owner.get_or_null(r->resources[i].primary);
			if (!buffer || buffer->driver_id.id >= 32) {
				FAIL_CHECK("owned buffer driver identity missing");
				return nullptr;
			}
			e.driver.buffer_cells[buffer->driver_id.id] = &r->resources[i].owned_physical;
		}
		if (dispatch) {
			if (!import_original_unit(e, *r, aliases ? false : mismatch != 4, aliases, mismatch != 9)) {
				return nullptr;
			}
			const GDExtensionProductionSetBindingV1 bindings[] = {
				{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 },
				{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 1, 0, 1, 0, 128 }
			};
			e.driver.expected_set_bindings = aliases ? 2 : 1;
			if (!prepare_dispatch) {
				return r;
			}
			const auto set_error = create_set(e, ref, 2, 0, 0, bindings, e.driver.expected_set_bindings).native_error;
			CHECK(set_error == OK);
			if (set_error != OK) {
				return nullptr;
			}
			e.driver.set_cell = &r->resources[2].owned_physical;
			CHECK(e.registry->production_set_create(ref, 2, 0, 0, bindings, e.driver.expected_set_bindings).native_error == OK);
			CHECK(e.driver.set_creates == 1);

#ifdef DEV_ENABLED
			CHECK(e.driver.set_name == "RID:" + itos(r->resources[2].primary.get_id()));
#else
			CHECK(e.driver.set_name.is_empty());
#endif
			const uint32_t push[4] = { 7, 11, 13, 17 };
			const GDExtensionProductionDispatchV1 input = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
			const auto prepare_error = e.registry->production_prepare_dispatch(ref, &input, push, sizeof(push)).native_error;
			if ((mismatch && mismatch <= 4) || mismatch == 9) {
				CHECK(prepare_error == ERR_INVALID_DATA);
				return r;
			}
			CHECK(prepare_error == OK);
			if (prepare_error != OK) {
				return nullptr;
			}
		}
		return r;
	}
	static GDExtensionDeviceSessionResultV1 observe_storage(Enrollment &e, GDExtensionProductionRequestRefV1 ref, GDExtensionStorageRetireRefV1 retire, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionStorageRetireReportV1 *report) {
		using namespace GDExtensionDeviceExit;
		BudgetDriverGuard budget_guard(e.driver);
		CHECK(budget->receipts <= 128);
		CHECK(budget->passes <= 64);
		const auto before = e.driver.receipt_calls();
		const auto frees_before = e.driver.buffer_frees;
		uint64_t next_serial = 0;
		{
			MutexLock lock(e.registry->mutex);
			Request *request = nullptr;
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_request_locked(ref, false, request, rd) == OK);
			next_serial = request->next_control_serial;
		}
		const auto accepted = e.registry->production_storage_observe(ref, retire, budget, report);
		CHECK(e.driver.receipt_calls() == before);
		if (accepted.native_error != ERR_UNAVAILABLE) {
			// Zero/expired budgets are metadata queries and accept no new turn.
			CHECK(accepted.native_error == OK);
			MutexLock lock(e.registry->mutex);
			Request *request = nullptr;
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_request_locked(ref, false, request, rd) == OK);
			CHECK(request->control.state == ProductionControlState::OWNER_CONSUMED);
			CHECK(request->next_control_serial == next_serial);
			return accepted;
		}
		const auto output = execute_public_control(e, ref, ProductionControlKind::OBSERVE);
		CHECK(output.service_usage.receipts <= budget->receipts);
		CHECK(output.service_usage.passes <= budget->passes);
		CHECK(output.service_usage.controls <= 1);
		if (output.report.possible_obligations || output.report.known_obligations != output.report.complete_obligations) {
			// Legal native polling may spend a receipt while incomplete. It must
			// not free backing or publish a physical return before that frontier.
			CHECK(e.driver.buffer_frees == frees_before);
			CHECK(report->state != 1);
		}
		// Consume the mechanical RT result, then query the actual physical report
		// with zero service credit. The query must never make driver calls.
		auto query_budget = *budget;
		query_budget.controls = query_budget.receipts = query_budget.passes = 0;
		const auto query_before = e.driver.receipt_calls();
		CHECK(e.registry->production_storage_observe(ref, retire, &query_budget, report).native_error == OK);
		CHECK(e.driver.receipt_calls() == query_before);
		if (output.report.possible_obligations || output.report.known_obligations != output.report.complete_obligations) {
			CHECK(report->state != 1);
		}
		return output.result;
	}
	static bool retire_storage(Enrollment &e, Request &r, uint32_t slot, GDExtensionStorageRetireRefV1 &retire) {
		const auto &resource = r.resources[slot];
		const auto &created = resource.create_origin;
		const GDExtensionStorageRetireRequestV1 origin = { Enrollment::ABI, resource.owned_backing.backing, resource.primary.get_id(), resource.backing_serial, created.allocation_slot, created.part, created.allocation_generation, 1, 0 };
		GDExtensionStorageRetireReportV1 report = {};
		const auto error = e.registry->production_storage_retire({ Enrollment::INSTANCE, r.serial }, slot, &origin, &retire, &report).native_error;
		CHECK(error == OK);
		return error == OK;
	}
	static bool finish_owned_maintenance(Enrollment &e, Request &r, const GDExtensionProductionServiceBudgetV1 &limit) {
		using namespace GDExtensionDeviceExit;
		uint32_t turns = 0;
		for (; turns < 128 && r.maintenance_serial && e.rd.native_disposal_active && r.maintenance_serial == e.rd.native_disposal_continuation.serial; ++turns) {
			auto fresh = limit;
			fresh.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			const auto calls = e.driver.receipt_calls();
			const auto output = turn(e, r, ProductionControlKind::OBSERVE, fresh);
			CHECK(output.service_usage.receipts <= limit.receipts);
			CHECK(output.service_usage.passes <= limit.passes);
			CHECK(e.driver.receipt_calls() - calls <= limit.receipts);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
		}
		const bool pending = r.maintenance_serial && e.rd.native_disposal_active && r.maintenance_serial == e.rd.native_disposal_continuation.serial;
		INFO("Original cursor tail turns=", turns, "/128 serial=", r.maintenance_serial, " active=", e.rd.native_disposal_active.load(), " phase=", e.rd.native_disposal_continuation.phase);
		CHECK_FALSE(pending);
		return !pending;
	}
	static bool return_owned_storage(Enrollment &e, Request &r, uint32_t slot) {
		GDExtensionStorageRetireRefV1 retire = {};
		if (!retire_storage(e, r, slot, retire)) {
			return false;
		}
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r.serial };
		const uint32_t receipts = e.driver.sources_per_issue;
		const auto small = budget(receipts, 1);
		GDExtensionStorageRetireReportV1 report = {};
		for (uint32_t turn_index = 0; turn_index < 128 && report.state != 1; ++turn_index) {
			const auto before = e.driver.receipt_calls();
			const auto error = observe_storage(e, ref, retire, &small, &report).native_error;
			CHECK((error == OK || error == ERR_BUSY));
			CHECK(e.driver.receipt_calls() - before <= receipts);
		}
		CHECK(report.state == 1);
		CHECK(report.return_status == 1);
		CHECK(report.event_seq != 0);
		if (report.state != 1) {
			return false;
		}
		const auto error = e.registry->production_storage_release(ref, retire).native_error;
		CHECK(error == OK);
		return error == OK && finish_owned_maintenance(e, r, small);
	}
	static void storage_retirement_turns() {
		Enrollment e;
		auto *r = owned_setup(e, false);
		e.driver.complete = true;
		if (!r) {
			return;
		}
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		GDExtensionStorageRetireRefV1 retires[2] = {};
		for (uint32_t i = 0; i < 2; ++i) {
			if (!retire_storage(e, *r, i, retires[i])) {
				return;
			}
		}
		CHECK(e.driver.buffer_frees == 0);
		CHECK(e.registry->production_storage_release(ref, retires[0]).native_error == ERR_BUSY);
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		CHECK(e.registry->production_close(attachment).native_error == ERR_BUSY);
		auto zero = budget(0, 1);
		GDExtensionStorageRetireReportV1 reports[2] = {};
		CHECK(observe_storage(e, ref, retires[0], &zero, &reports[0]).native_error == OK);
		CHECK(e.driver.buffer_frees == 0);
		auto expired = budget(1, 1);
		expired.deadline_micros = 1;
		const auto expired_before = e.driver.receipt_calls();
		CHECK(observe_storage(e, ref, retires[0], &expired, &reports[0]).native_error == OK);
		CHECK(e.driver.receipt_calls() == expired_before);
		const auto small = budget(1, 1);
		bool released[2] = {};
		uint32_t turns = 0;
		for (; turns < 128 && (!released[0] || !released[1]); ++turns) {
			const uint32_t slot = turns % 2;
			if (released[slot]) {
				continue;
			}
			const auto before = e.driver.receipt_calls();
			const auto error = observe_storage(e, ref, retires[slot], &small, &reports[slot]).native_error;
			CHECK((error == OK || error == ERR_BUSY));
			CHECK(e.driver.receipt_calls() - before <= 1);
			if (reports[slot].state == 1) {
				CHECK(reports[slot].return_status == 1);
				CHECK(reports[slot].event_seq != 0);
				CHECK(e.registry->production_storage_release(ref, retires[slot]).native_error == OK);
				released[slot] = true;
			}
		}
		CHECK(turns > 1);
		CHECK(released[0]);
		CHECK(released[1]);
		CHECK(e.driver.buffer_frees == 2);
		if (!released[0] || !released[1]) {
			return;
		}
		if (!finish_owned_maintenance(e, *r, small)) {
			return;
		}
		CHECK(e.registry->production_release(ref).native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment).native_error == OK);
		CHECK(e.rd.production_entity_holds == nullptr);
	}
	static void original_set_create_frame_gate(uint32_t revoke) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = owned_setup(e, true, 0, false);
		REQUIRE(r != nullptr);
		e.driver.complete = true;
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		const GDExtensionProductionAttachmentRefV1 attachment{ Enrollment::INSTANCE, r->attachment->serial };
		const GDExtensionProductionSetBindingV1 binding{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		const Thread::ID rt_id = e.rd.render_thread_id;
		const auto set_rt = [&](Thread::ID p_id) {
			e.rd.render_thread_id = p_id;
			e.registry->device_records[0].execution_thread = p_id;
		};
		struct RenderThreadRestore {
			RenderingDevice &rd;
			Registry *registry;
			Thread::ID id;
			~RenderThreadRestore() {
				rd.render_thread_id = id;
				registry->device_records[0].execution_thread = id;
			}
		} rt_restore{ e.rd, e.registry, rt_id };
		// Exercise the off-render-thread async SET_CREATE control handoff & resume path.
		set_rt(Thread::UNASSIGNED_ID);
		const auto accepted = e.registry->production_set_create(ref, 2, 0, 0, &binding, 1);
		set_rt(rt_id);
		REQUIRE(accepted.native_error == ERR_UNAVAILABLE); // The local fixture has no singleton FIFO.
		auto &slot = r->resources[2];
		const uint64_t owner = slot.owned_serial;
		const uint64_t retirement = slot.retire_serial;
		const uint64_t physical = slot.owned_physical.serial;
		const uint64_t first_control = r->control.serial;
		const uint32_t bound_rows = r->binding_used;
		REQUIRE(first_control != 0);
		REQUIRE(slot.creating);
		CHECK(e.driver.set_creates == 0);

		// Existing controlled pending-advance input. No completion/return fact
		// is fabricated; public SET_CREATE accepted the exact original slot.
		struct EndGate {
			RenderingDevice &rd;
			~EndGate() { rd.qualification_frame_advance_pending = false; }
		} end_gate{ e.rd };
		e.rd.qualification_frame_advance_pending = true;
		REQUIRE(e.rd._recording_error() == ERR_BUSY);
		const auto blocked = execute_public_control(e, ref, ProductionControlKind::SET_CREATE, 2);
		REQUIRE(blocked.result.native_error == ERR_BUSY);
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(r->control.serial == first_control);
		CHECK(e.driver.set_creates == 0);
		CHECK(slot.primary.is_null());
		REQUIRE(slot.creating);
		CHECK(r->returns[2].pending);

		// A changed origin cannot reuse the accepted control/resource.
		auto different = binding;
		different.bytes = 64;
		set_rt(Thread::UNASSIGNED_ID);
		CHECK(e.registry->production_set_create(ref, 2, 0, 0, &different, 1).native_error == ERR_INVALID_DATA);
		set_rt(rt_id);
		CHECK(r->control.serial == first_control);
		CHECK(e.driver.set_creates == 0);
		e.rd.qualification_frame_advance_pending = false;
		REQUIRE(e.rd._recording_error() == OK);
		if (revoke == 1) {
			REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		} else if (revoke == 2) {
			const auto closed = e.registry->begin_quiesce(e.reference);
			REQUIRE((closed == OK || closed == ERR_BUSY));
			REQUIRE(e.registry->records[0].quiescing);
		}
		set_rt(Thread::UNASSIGNED_ID);
		const auto next = e.registry->production_set_create(ref, 2, 0, 0, &binding, 1);
		set_rt(rt_id);
		CHECK(slot.owned_serial == owner);
		CHECK(slot.retire_serial == retirement);
		CHECK(slot.owned_physical.serial == physical);
		CHECK(r->binding_used == bound_rows);
		if (revoke) {
			REQUIRE(next.native_error == ERR_BUSY);
			CHECK_FALSE(slot.creating);
			CHECK_FALSE(r->returns[2].pending);
			CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
			CHECK(r->control.serial == first_control);
			CHECK(e.driver.set_creates == 0);
			CHECK(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == ERR_BUSY);
			CHECK(r->control.serial == first_control);
		} else {
			REQUIRE(next.native_error == ERR_UNAVAILABLE);
			REQUIRE(r->control.state == ProductionControlState::PENDING);
			CHECK(r->control.serial > first_control);
			CHECK(e.driver.set_creates == 0);
			const auto completed = execute_public_control(e, ref, ProductionControlKind::SET_CREATE, 2);
			REQUIRE(completed.result.native_error == OK);
			REQUIRE(e.driver.set_creates == 1);
			REQUIRE(slot.primary.is_valid());
			CHECK_FALSE(slot.creating);
			CHECK(slot.owned_serial == owner);
			CHECK(slot.retire_serial == retirement);
			CHECK(slot.owned_physical.serial == physical);
			CHECK(r->binding_used == bound_rows);
			const uint64_t created_control = r->control.serial;
			CHECK(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
			CHECK(r->control.serial == created_control);
			CHECK(e.driver.set_creates == 1);
			e.driver.set_cell = &slot.owned_physical;
		}
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		REQUIRE(return_owned_storage(e, *r, 1));
		CHECK(e.driver.set_frees == (revoke ? 0u : 1u));
		REQUIRE(finish_owned_maintenance(e, *r, budget(1, 1)));
		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		REQUIRE(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		REQUIRE(e.registry->production_close(attachment).native_error == OK);
	}

	static void owned_set_retirement_batch(uint32_t receipts) {
		using namespace GDExtensionDeviceExit;
		// Measure both enqueue and physical FREE prices in the original MQ
		// source. Its retained loop charges one FREE per returned SET, and this
		// original no-issue fixture owns no signaled fence or extra native work.
		const auto mq_source = own_set_with_ordinary_disposal(true);
		REQUIRE(mq_source.native_frees > 0);
		REQUIRE(mq_source.logical_receipts > 0);
		REQUIRE(mq_source.logical_passes > 0);
		REQUIRE(mq_source.receipts % mq_source.native_frees == 0);
		REQUIRE(mq_source.passes % mq_source.native_frees == 0);
		const auto receipt_price = mq_source.receipts / mq_source.native_frees;
		const auto pass_price = mq_source.passes / mq_source.native_frees;
		REQUIRE(receipt_price > 0);
		REQUIRE(pass_price > 0);
		CHECK(mq_source.native_calls == mq_source.native_frees);
		Enrollment e;
		e.driver.allow_second_set = true;
		const auto c = capacity(8);
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		ProductionAttachment *attachment = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation = { production_dto_abi<GDExtensionProductionOperationV1>(), 571, 1, 10, 11 };
		const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 3, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 }
		};
		const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_locked(*attachment, operation, c, &action, 1, resources, 4, &use, 1, nullptr, 0, r) == OK);
		}
		REQUIRE(r != nullptr);
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(create_storage(e, ref, 1, &origin, &backing).native_error == OK);
		const auto *buffer = e.rd.storage_buffer_owner.get_or_null(r->resources[1].primary);
		REQUIRE(buffer != nullptr);
		REQUIRE(buffer->driver_id.id < 32);
		e.driver.buffer_cells[buffer->driver_id.id] = &r->resources[1].owned_physical;
		REQUIRE(import_original_unit(e, *r));
		const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		REQUIRE(create_set(e, ref, 2, 0, 0, &binding, 1).native_error == OK);
		e.driver.set_cell = &r->resources[2].owned_physical;
		REQUIRE(create_set(e, ref, 3, 0, 0, &binding, 1).native_error == OK);
		e.driver.second_set_cell = &r->resources[3].owned_physical;
		REQUIRE(e.driver.set_creates == 2);
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		const uint64_t cursor_before = e.rd.next_native_disposal_serial;
		// The foreign-prefix MQ oracle measures FREE prices, but this batch
		// may also use the original general cursor. Witness that real source
		// at its native FREE and after each original turn; do not guess a count.
		struct CursorWitness {
			RenderingDevice &rd;
			Request &request;
			QuoteDriver &driver;
			uint64_t first_serial;
			Vector<uint64_t> serials;
			CursorWitness(RenderingDevice &p_rd, Request &p_request, QuoteDriver &p_driver, uint64_t p_first) : rd(p_rd), request(p_request), driver(p_driver), first_serial(p_first) {}
			~CursorWitness() {
				driver.set_free_observer = nullptr;
				driver.set_free_observer_context = nullptr;
			}
			void observe() {
				if (!rd.native_disposal_active) {
					return; // Selective MQ return does not allocate an RD cursor.
				}
				const auto &cursor = rd.native_disposal_continuation;
				CHECK(cursor.production_owner == &request);
				CHECK(cursor.height_selection.is_null());
				CHECK_FALSE(cursor.in_place_no_disposal);
				if (!serials.has(cursor.serial)) {
					CHECK(cursor.serial == first_serial + uint64_t(serials.size()));
					serials.push_back(cursor.serial);
				}
			}
			static void before_set_free(void *context, RDD::UniformSetID id) {
				auto &self = *static_cast<CursorWitness *>(context);
				self.observe();
				if (self.rd.native_disposal_active) {
					const auto &cursor = self.rd.native_disposal_continuation;
					const auto &physical = self.request.resources[id == RDD::UniformSetID(83) ? 3 : 2].owned_physical;
					CHECK(cursor.phase == 4);
					CHECK(cursor.target == physical.frame_slot);
					REQUIRE(cursor.target < self.rd.frames.size());
					CHECK(self.rd.frames[cursor.target].qualification_epoch == physical.frame_epoch);
					CHECK(self.rd.frames[cursor.target].disposal.active);
				}
			}
			void after_turn() {
				observe();
				CHECK(rd.next_native_disposal_serial == first_serial + uint64_t(serials.size()));
				if (rd.native_disposal_active) {
					CHECK(request.maintenance_serial == rd.native_disposal_continuation.serial);
				} else {
					CHECK(request.maintenance_serial == 0);
				}
			}
		} cursor_witness(e.rd, *r, e.driver, cursor_before);
		e.driver.set_free_observer = &CursorWitness::before_set_free;
		e.driver.set_free_observer_context = &cursor_witness;
		REQUIRE_FALSE(e.rd.frames[0].fence_signaled);
		const auto offered = budget(receipts, 16);
		const auto native_before = e.driver.receipt_calls();
		const auto batch = turn(e, *r, ProductionControlKind::OBSERVE, offered);
		cursor_witness.after_turn();
		CHECK(e.driver.receipt_calls() - native_before <= offered.receipts);
		CHECK(batch.service_usage.receipts <= receipts);
		CHECK(batch.service_usage.passes <= 16);
		CHECK(r->resources[2].owned_physical.enqueued);
		CHECK(r->resources[3].owned_physical.enqueued == (receipts >= 2));
		const uint32_t enqueued = uint32_t(r->resources[2].owned_physical.enqueued) +
				uint32_t(r->resources[3].owned_physical.enqueued);
		const uint32_t logical_receipts = enqueued * mq_source.logical_receipts;
		const uint32_t logical_passes = enqueued * mq_source.logical_passes;
		REQUIRE(logical_receipts <= offered.receipts);
		REQUIRE(logical_passes <= offered.passes);
		const uint32_t receipt_paid = (offered.receipts - logical_receipts) / receipt_price;
		const uint32_t pass_paid = (offered.passes - logical_passes) / pass_price;
		const uint32_t source_paid_returns = MIN(enqueued, MIN(receipt_paid, pass_paid));
		CHECK(e.driver.set_frees == source_paid_returns);
		CHECK(r->resources[2].returned == (source_paid_returns >= 1));
		CHECK(r->resources[3].returned == (source_paid_returns >= 2));
		CHECK(r->returns[2].pending == (source_paid_returns < 1));
		CHECK(r->returns[3].pending == (source_paid_returns < 2));
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		if (receipts >= 2) {
			CHECK(r->resources[2].owned_physical.frame_slot == r->resources[3].owned_physical.frame_slot);
			CHECK(r->resources[2].owned_physical.frame_epoch == r->resources[3].owned_physical.frame_epoch);
		}
		e.driver.complete = true;
		for (uint32_t pass = 0; pass < 64 && (!r->resources[2].returned || !r->resources[3].returned); ++pass) {
			const auto before = e.driver.receipt_calls();
			const auto observed = turn(e, *r, ProductionControlKind::OBSERVE, budget(3, 16));
			cursor_witness.after_turn();
			CHECK(observed.service_usage.receipts <= 3);
			CHECK(observed.service_usage.passes <= 16);
			CHECK(e.driver.receipt_calls() - before <= 3);
		}
		REQUIRE(r->resources[2].returned);
		REQUIRE(r->resources[3].returned);
		CHECK(e.driver.set_frees == e.driver.set_creates);
		CHECK_FALSE(r->returns[2].pending);
		CHECK_FALSE(r->returns[3].pending);
		CHECK(r->resources[2].owned_physical.free_event != 0);
		CHECK(r->resources[3].owned_physical.free_event != 0);
		CHECK(r->resources[2].owned_physical.free_event != r->resources[3].owned_physical.free_event);
		cursor_witness.after_turn();
		e.driver.set_free_observer = nullptr;
		e.driver.set_free_observer_context = nullptr;
		CHECK(e.original_unit->shader.dependent_children.load() == 0);
		CHECK(r->resources[1].owned_physical.dependent_children.load() == 0);
		GDExtensionStorageRetireRefV1 retire = {};
		REQUIRE(retire_storage(e, *r, 1, retire));
		GDExtensionStorageRetireReportV1 report = {};
		const auto limit = budget(3, 16);
		for (uint32_t pass = 0; pass < 128 && report.state != 1; ++pass) {
			const auto observed = observe_storage(e, ref, retire, &limit, &report).native_error;
			CHECK((observed == OK || observed == ERR_BUSY));
		}
		REQUIRE(report.state == 1);
		REQUIRE(e.registry->production_storage_release(ref, retire).native_error == OK);
		REQUIRE(finish_owned_maintenance(e, *r, limit));
		REQUIRE(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
	}
	struct SetReturnOracle {
		uint32_t native_frees, receipts, passes, native_calls, foreign_count, logical_receipts, logical_passes;
		bool returned, free_event, foreign_live, same_foreign, cursor_unchanged;
		bool operator==(const SetReturnOracle &other) const {
			return native_frees == other.native_frees && receipts == other.receipts &&
					passes == other.passes && native_calls == other.native_calls &&
					foreign_count == other.foreign_count && logical_receipts == other.logical_receipts &&
					logical_passes == other.logical_passes && returned == other.returned &&
					free_event == other.free_event && foreign_live == other.foreign_live &&
					same_foreign == other.same_foreign && cursor_unchanged == other.cursor_unchanged;
		}
	};
	// The expected row is returned by the existing MQ source implementation.
	// Both arms use the real public owned SET create/cancel and logical free.
	// This is a controlled CPU-driver ownership fixture, not a native certificate.
	static SetReturnOracle own_set_with_ordinary_disposal(bool source_reference) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true; // Existing finite driver's ready input; no native report is written.
		e.driver.allow_second_set = true;
		auto *r = owned_setup(e, true);
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		const GDExtensionProductionAttachmentRefV1 attachment{ Enrollment::INSTANCE, r->attachment->serial };
		const RID own_set = r->resources[2].primary;
		// Use the existing finite driver shader shape with a distinct ordinary RD
		// owner. No qualification or output-source field is supplied to this set.
		auto ordinary_shader = *e.rd.shader_owner.get_or_null(r->resources[0].secondary);
		ordinary_shader.qualification_return = nullptr;
		const RID ordinary_shader_rid = e.rd.shader_owner.make_rid(ordinary_shader);
		const auto *original_buffer = e.rd.storage_buffer_owner.get_or_null(r->resources[1].primary);
		REQUIRE(original_buffer != nullptr);
		const RID ordinary_buffer = e.rd.storage_buffer_create(original_buffer->size);
		REQUIRE(ordinary_buffer.is_valid());
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = 0;
		uniform.append_id(ordinary_buffer);
		const RID foreign = e.rd.uniform_set_create(VectorView<RenderingDevice::Uniform>(&uniform, 1), ordinary_shader_rid, 0);
		REQUIRE(foreign.is_valid());
		const auto *foreign_live = e.rd.uniform_set_owner.get_or_null(foreign);
		REQUIRE(foreign_live != nullptr);
		const auto driver_id = foreign_live->driver_id;
		const auto shader = foreign_live->shader_id;
		const auto format = foreign_live->format;
		REQUIRE(foreign_live->qualification_return == nullptr);
		REQUIRE(foreign_live->qualification_dependencies.is_empty());
		REQUIRE(foreign_live->production_output_dependencies.is_empty());
		REQUIRE(foreign_live->production_disposal_uses == nullptr);
		e.rd.free_rid(foreign);
		REQUIRE_FALSE(e.rd.uniform_set_is_valid(foreign));
		REQUIRE(e.rd.frames[0].uniform_sets_to_dispose_of.size() == 1);
		const auto *foreign_element = e.rd.frames[0].uniform_sets_to_dispose_of.front();
		ProductionDisposalUse source_member;
		if (source_reference) {
			// The SOURCE's original public/internal owner attachment checks the
			// actual live RID/driver identity; never inject a completed member.
			REQUIRE(e.rd._production_disposal_attach_owned(source_member, *r, own_set) == OK);
		}
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		const auto cursor_before = e.rd.next_native_disposal_serial;
		const auto free_before_enqueue = e.driver.set_frees;
		// One original receipt admits only the logical free, never also its
		// physical return. This is the one-short control for both source arms.
		const auto enqueued = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
		CHECK(enqueued.service_usage.receipts <= 1);
		CHECK(enqueued.service_usage.passes <= 1);
		REQUIRE(r->resources[2].owned_physical.enqueued);
		CHECK(r->resources[2].owned_physical.free_event == 0);
		CHECK(e.driver.set_frees == free_before_enqueue);
		CHECK_FALSE(r->resources[2].returned);
		if (!source_reference) {
			const auto calls = e.driver.receipt_calls();
			// Preserve the pre-existing whole-frame foreign callback/readback
			// veto even though the selective resource itself is this owner's.
			e.rd.frames[0].download_buffer_get_data_requests.push_back(RenderingDevice::BufferGetDataRequest{});
			const auto foreign_readback = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
			CHECK(foreign_readback.result.native_error == ERR_BUSY);
			CHECK(e.driver.receipt_calls() == calls);
			CHECK(e.driver.set_frees == free_before_enqueue);
			CHECK(r->resources[2].owned_physical.free_event == 0);
			e.rd.frames[0].download_buffer_get_data_requests.clear();
		}
		const auto physical_before = e.driver.set_frees;
		const auto calls_before = e.driver.receipt_calls();
		uint32_t receipts, passes;
		if (source_reference) {
			const auto allowance = budget(1, 1);
			RenderingDevice::NativeDisposalBudget native{ allowance.receipts, allowance.passes, allowance.deadline_micros };
			RenderingDevice::NativeDisposalUsage used;
			BudgetDriverGuard guard(e.driver);
			REQUIRE(e.rd._production_mq_drain_owned_disposals_step(*r, nullptr, 0, native, used) == OK);
			receipts = used.polls + used.frees + used.issues + used.retains + used.releases + used.recycles;
			passes = used.passes;
		} else {
			// Generic maintenance still refuses the genuine foreign prefix. The
			// exact own-SET cleanup runs afterwards under this same original turn.
			const auto actual = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
			receipts = actual.service_usage.receipts;
			passes = actual.service_usage.passes;
		}
		SetReturnOracle observed{
			e.driver.set_frees - physical_before, receipts, passes,
			e.driver.receipt_calls() - calls_before,
			uint32_t(e.rd.frames[0].uniform_sets_to_dispose_of.size()),
			enqueued.service_usage.receipts, enqueued.service_usage.passes,
			false, r->resources[2].owned_physical.free_event != 0,
			e.driver.second_set_live,
			e.rd.frames[0].uniform_sets_to_dispose_of.front() == foreign_element &&
					foreign_element->get().driver_id == driver_id && foreign_element->get().shader_id == shader &&
					foreign_element->get().format == format && foreign_element->get().qualification_return == nullptr &&
					foreign_element->get().production_disposal_uses == nullptr,
			e.rd.next_native_disposal_serial == cursor_before
		};
		CHECK(receipts <= 1);
		CHECK(passes <= 1);
		CHECK(observed.native_calls <= receipts);
		turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
		observed.returned = r->resources[2].returned && !r->returns[2].pending;
		CHECK(e.original_unit->shader.dependent_children.load() == 0);
		CHECK(r->resources[1].owned_physical.dependent_children.load() == 0);
		// Let the original ordinary RD owner, not production, return its set.
		const auto own_free_event = r->resources[2].owned_physical.free_event;
		e.rd._free_pending_resources(0);
		CHECK_FALSE(e.driver.second_set_live);
		CHECK(r->resources[2].owned_physical.free_event == own_free_event);
		CHECK(e.rd.frames[0].uniform_sets_to_dispose_of.is_empty());
		e.rd.free_rid(ordinary_buffer);
		e.rd._free_pending_resources(0);
		// The ordinary fixture shader borrows the finite driver's original shape,
		// so only its independent RID owner is released, not that shared driver.
		e.rd.shader_owner.free(ordinary_shader_rid);
		REQUIRE(return_owned_storage(e, *r, 1));
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		REQUIRE(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		REQUIRE(e.registry->production_close(attachment).native_error == OK);
		return observed;
	}
	static void owned_set_dispatch(uint32_t logical_use = 0, bool honors_barriers = false) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.honors_pipeline_barriers = honors_barriers;
		auto *r = owned_setup(e, true, logical_use);
		if (!r) {
			return;
		}
		if (logical_use == 8) {
			MutexLock lock(e.registry->mutex);
			auto &first = r->uses[r->resources[2].first_binding].binding;
			auto &second = r->uses[r->resources[2].first_binding + 1].binding;
			const auto original_first = first;
			const auto original_second = second;
			// Helper-only window projections: set_create currently admits full
			// physical buffers. Restore them before any actual driver recording.
			first.bytes = 64;
			second.offset = 64;
			second.bytes = 64;
			CHECK(e.registry->production_dispatch_closure_locked(*r, e.rd, r->actions[0], r->actions[0].dispatch) == OK);
			second.offset = 0;
			CHECK(e.registry->production_dispatch_closure_locked(*r, e.rd, r->actions[0], r->actions[0].dispatch) == ERR_INVALID_DATA);
			second = original_second;
			first.offset = 64;
			CHECK(e.registry->production_dispatch_closure_locked(*r, e.rd, r->actions[0], r->actions[0].dispatch) == ERR_INVALID_DATA);
			first = original_first;
		}
		CHECK(e.driver.set_creates == 1);
		CHECK(e.original_unit->shader.dependent_children.load() == 1);
		CHECK(r->resources[1].owned_physical.dependent_children.load() == 1);
		const auto append = effect(e, *r, 0);
		CHECK(append.state == CheckedRecordAppend::State::Appended);
		CHECK(append.error == OK);
		CHECK(r->next_action == 1);
		CHECK_FALSE(r->compute_list_open);
		const auto segment = close_segment(e, *r);
		CHECK(segment.end_canonical == 11);
		e.driver.complete = true;
		execute_transfers(e, *r, true);
		CHECK(e.driver.dispatches == 1);
		CHECK(e.driver.set_prepares == (honors_barriers ? 0u : 1u));
		CHECK(e.driver.set_binds == 1);
		const uint32_t push[4] = { 7, 11, 13, 17 };
		for (uint32_t i = 0; i < 4; ++i) {
			CHECK(e.driver.pushed[i] == push[i]);
		}
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		const auto small = budget(1, 1);
		GDExtensionStorageRetireRefV1 retire = {};
		if (!retire_storage(e, *r, 1, retire)) {
			return;
		}
		GDExtensionStorageRetireReportV1 early = {};
		const auto before_early = e.driver.receipt_calls();
		// A nonzero public Observe now accepts a real maintenance turn. Probe
		// dependency protection with a zero query and the actual release gate.
		const auto early_query = budget(0, 1);
		CHECK(observe_storage(e, ref, retire, &early_query, &early).native_error == OK);
		CHECK(e.registry->production_storage_release(ref, retire).native_error == ERR_BUSY);
		CHECK(e.driver.receipt_calls() == before_early);
		CHECK(e.driver.buffer_frees == 0);
		CHECK(e.driver.set_frees == 0);

		for (uint32_t i = 0; i < 128 && !r->resources[2].returned; ++i) {
			const auto before = e.driver.receipt_calls();
			const bool already_complete = r->natives[0].observation.state == NativeSourceObservation::State::Complete;
			const auto polls_before = e.driver.polls[r->natives[0].member.source.serial];
			const auto observed = turn(e, *r, ProductionControlKind::OBSERVE, small);
			if (already_complete) {
				CHECK(e.driver.polls[r->natives[0].member.source.serial] == polls_before);
			}
			CHECK(observed.service_usage.receipts <= 1);
			CHECK(observed.service_usage.passes <= 1);
			CHECK(e.driver.receipt_calls() - before <= 1);
		}
		CHECK(e.driver.set_frees == 1);
		CHECK(r->resources[2].returned);
		CHECK(e.original_unit->shader.dependent_children.load() == 0);
		CHECK(r->resources[1].owned_physical.dependent_children.load() == 0);
		if (!r->resources[2].returned) {
			return;
		}
		GDExtensionStorageRetireReportV1 report = {};
		for (uint32_t i = 0; i < 128 && report.state != 1; ++i) {
			const auto before = e.driver.receipt_calls();
			const auto error = observe_storage(e, ref, retire, &small, &report).native_error;
			CHECK((error == OK || error == ERR_BUSY));
			CHECK(e.driver.receipt_calls() - before <= 1);
		}
		CHECK(report.state == 1);
		CHECK(e.driver.buffer_frees == 1);
		if (report.state != 1) {
			return;
		}
		CHECK(e.registry->production_storage_release(ref, retire).native_error == OK);
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		if (!finish_owned_maintenance(e, *r, small)) {
			return;
		}
		const auto release = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(release.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.original_unit->shader.production_uses == nullptr);
		CHECK(e.original_unit->pipeline.production_uses == nullptr);
		CHECK(e.registry->production_close(attachment).native_error == OK);
	}
	static void consumed_transport_rejects_without_source() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto c = capacity(3);
		const auto attachment_ref = e.accept(c);
		ProductionAttachment *attachment = nullptr; RenderingDevice *rd = nullptr; Request *request = nullptr;
		const GDExtensionProductionOperationV1 operation = { production_dto_abi<GDExtensionProductionOperationV1>(), 471, 1, 10, 11 };
		const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
		const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, &resource, 1, &use, 1, {}, 0, request, 2) == OK);
		}
		REQUIRE(request != nullptr);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, request->serial };
		const uint64_t next = request->next_control_serial;
		const auto driver_before = e.driver.receipt_calls();
		GDExtensionOutputConsumedSelectorV2 selector = { production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 0 };
		CHECK(e.registry->output_consumed_prepare_v2(ref, &selector).native_error == ERR_UNAUTHORIZED);
		CHECK_FALSE(request->actions[0].consumed_proof_requested);
		GDExtensionOutputConsumedBorrowResultV2 out;
		memset(&out, 0xa5, sizeof(out));
		const auto untouched = out;
		auto service = budget();
		CHECK(e.registry->output_consumed_borrow_v2(ref, &selector, &service, &out).native_error == ERR_UNAUTHORIZED);
		CHECK(memcmp(&out, &untouched, sizeof(out)) == 0);
		selector.abi.abi_version = 1;
		CHECK(e.registry->output_consumed_borrow_v2(ref, &selector, &service, &out).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&out, &untouched, sizeof(out)) == 0);
		selector.abi = production_dto_abi<GDExtensionOutputConsumedSelectorV2>();
		service.controls = 0;
		CHECK(e.registry->output_consumed_borrow_v2(ref, &selector, &service, &out).native_error == ERR_BUSY);
		CHECK(memcmp(&out, &untouched, sizeof(out)) == 0);
		// Overlapping complete spans refuse before either a cell or an output changes.
		alignas(GDExtensionOutputConsumedBorrowResultV2) unsigned char bytes[sizeof(GDExtensionOutputConsumedBorrowResultV2)];
		memset(bytes, 0xa5, sizeof(bytes));
		memcpy(bytes, &selector, sizeof(selector));
		unsigned char saved[sizeof(bytes)]; memcpy(saved, bytes, sizeof(bytes));
		CHECK(e.registry->output_consumed_borrow_v2(ref, reinterpret_cast<GDExtensionOutputConsumedSelectorV2 *>(bytes), &service,
				reinterpret_cast<GDExtensionOutputConsumedBorrowResultV2 *>(bytes)).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
		GDExtensionOutputConsumedProofRefV2 expected{};
		expected.abi = production_dto_abi<GDExtensionOutputConsumedProofRefV2>();
		GDExtensionOutputConsumedProofV2 proof; memset(&proof, 0xa5, sizeof(proof)); const auto proof_before = proof;
		CHECK(e.registry->output_consumed_read_v2(ref, &expected, &proof).native_error == ERR_DOES_NOT_EXIST);
		CHECK(memcmp(&proof, &proof_before, sizeof(proof)) == 0);
		service = budget();
		GDExtensionProductionReportV2 report; memset(&report, 0xa5, sizeof(report)); const auto report_before = report;
		CHECK(e.registry->output_consumed_return_v2(ref, &expected, &service, &report).native_error == ERR_UNAUTHORIZED);
		CHECK(memcmp(&report, &report_before, sizeof(report)) == 0);
		CHECK(request->next_control_serial == next);
		CHECK(request->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(e.driver.receipt_calls() == driver_before);
	}
	static void consumed_transport_preserves_v1() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		Request *request = setup(e);
		REQUIRE(request != nullptr);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, request->serial };
		const uint64_t next = request->next_control_serial;
		GDExtensionOutputConsumedSelectorV2 selector = { production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 0 };
		CHECK(e.registry->output_consumed_prepare_v2(ref, &selector).native_error == ERR_INVALID_DATA);
		CHECK_FALSE(request->actions[0].consumed_proof_requested);
		GDExtensionOutputConsumedBorrowResultV2 out; memset(&out, 0xa5, sizeof(out)); const auto before = out;
		auto service = budget();
		CHECK(e.registry->output_consumed_borrow_v2(ref, &selector, &service, &out).native_error == ERR_INVALID_DATA);
		CHECK(memcmp(&out, &before, sizeof(out)) == 0);
		CHECK(request->next_control_serial == next);
		CHECK(request->control.state == ProductionControlState::OWNER_CONSUMED);
	}
	static void checked_dispatch_primitive() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.rd.draw_graph.initialize(&e.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		e.rd.draw_graph.begin();
		// The same ordinary RD shader/pipeline construction used by RecordSource.
		// No unit import, runtime scope/window or production handler grant is implied.
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(80);
		const RID shader_rid = e.rd.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(81);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = 16;
		pipeline.local_group_size[0] = pipeline.local_group_size[1] = pipeline.local_group_size[2] = 1;
		const RID pipeline_rid = e.rd.compute_pipeline_owner.make_rid(pipeline);
		const auto list = e.rd.compute_list_begin();
		CHECK(list != int64_t(RenderingDevice::INVALID_ID));
		if (list == RenderingDevice::INVALID_ID) {
			e.rd.compute_pipeline_owner.free(pipeline_rid);
			e.rd.shader_owner.free(shader_rid);
			return;
		}
		e.rd.compute_list_bind_compute_pipeline(list, pipeline_rid);
		const OriginalOperationIdentity identity = { 91, 1, 1, 1, 10, 11 };
		const uint32_t push[4] = { 7, 11, 13, 17 };
		const auto append = e.rd._record_source_dispatch(identity, list, 2, 3, 4, push, sizeof(push));
		CHECK(append.error == OK);
		CHECK(append.state == CheckedRecordAppend::State::Appended);
		CHECK(e.rd.draw_graph.contains_record(append.handle));
		CHECK(e.driver.dispatches == 0);
		e.rd.compute_list_end();
		RDG::CommandBufferPool pool;
		auto primary = RDD::CommandBufferID(90);
		e.rd.draw_graph.end(false, false, primary, pool);
		CHECK(e.driver.dispatches == 1);
		for (uint32_t i = 0; i < 4; ++i) {
			CHECK(e.driver.pushed[i] == push[i]);
		}
		e.rd.compute_pipeline_owner.free(pipeline_rid);
		e.rd.shader_owner.free(shader_rid);
	}
	static Request *transfer_setup(Enrollment &e, bool partial = false) {
		using namespace GDExtensionDeviceExit;
		auto c = capacity(8);
		c.readbacks = 1;
		c.retained_payload_bytes = 96;
		c.readback_bytes = partial ? 96 : 16;
		c.native_memberships *= e.driver.sources_per_issue;
		auto attachment_ref = e.accept(c);
		if (!attachment_ref.handle_serial) {
			return nullptr;
		}
		ProductionAttachment *attachment = nullptr;
		RenderingDevice *rd = nullptr;
		Request *request = nullptr;
		const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 71, 1, 10, 13 };
		const GDExtensionProductionActionV1 actions[] = {
			{ 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 11, GDEXTENSION_PRODUCTION_COPY_V1, 0, 1, 2, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 12, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 3, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 }
		};
		const GDExtensionProductionUseV1 uses[] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 96, 0 },
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 96, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 96, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, partial ? 0u : 4u, partial ? 96u : 16u, 0 }
		};
		const GDExtensionProductionReadbackV1 readback = { 1, 0, 11, 12, partial ? 0u : 4u, partial ? 96u : 16u };
		{
			MutexLock lock(e.registry->mutex);
			Error error = e.registry->production_attachment_locked(attachment_ref, true, attachment, rd);
			if (error == OK) {
				error = e.registry->production_reserve_metadata_locked(*attachment, operation, c, actions, 3, resources, 2, uses, 4, &readback, 1, request);
			}
			CHECK(error == OK);
			if (error != OK) {
				return nullptr;
			}
		}
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, request->serial };
		for (uint32_t i = 0; i < 2; ++i) {
			GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 128, i, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			const auto error = create_storage(e, ref, i, &origin, &backing).native_error;
			CHECK(error == OK);
			if (error != OK) {
				return nullptr;
			}
			CHECK(backing.rid != 0);
		}
		uint8_t payload[96];
		for (uint32_t i = 0; i < 96; ++i) {
			payload[i] = uint8_t(i ^ 0x5a);
		}
		const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, partial ? 96u : 32u };
		const GDExtensionProductionCopyV1 copy = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCopyV1>(), 11, 0, 1, 0, 0, partial ? 96u : 32u };
		const Error upload_error = Error(e.registry->production_prepare_upload(ref, &upload, payload, upload.bytes).native_error);
		const Error copy_error = Error(e.registry->production_prepare_copy(ref, &copy).native_error);
		const Error readback_error = Error(e.registry->production_prepare_readback(ref, 12, 0).native_error);
		CHECK(upload_error == OK);
		CHECK(copy_error == OK);
		CHECK(readback_error == OK);
		if (upload_error != OK || copy_error != OK || readback_error != OK) {
			return nullptr;
		}
		e.rd.upload_staging_buffers.block_size = partial ? 64 : 128;
		e.rd.upload_staging_buffers.max_size = partial ? 64 : 256;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		e.rd.download_staging_buffers.block_size = 128;
		e.rd.download_staging_buffers.max_size = 256;
		e.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		const Error up = e.rd._insert_staging_block(e.rd.upload_staging_buffers);
		const Error down = e.rd._insert_staging_block(e.rd.download_staging_buffers);
		CHECK(up == OK);
		CHECK(down == OK);
		return up == OK && down == OK ? request : nullptr;
	}
	static GDExtensionDeviceExit::CheckedRecordAppend effect(Enrollment &e, Request &r, uint32_t index) {
		const GDExtensionDeviceExit::OriginalOperationIdentity identity = { r.operation.owner_request_nonce, r.attachment->library_generation, 1, r.operation.record_generation, r.operation.canonical_begin, r.operation.canonical_end };
		MutexLock rd_lock(e.rd._thread_safe_);
		MutexLock registry_lock(e.registry->mutex);
		return e.registry->production_record_effect_locked(r, e.rd, index, identity);
	}
	static GDExtensionProductionSegmentV1 close_segment(Enrollment &e, Request &r) {
		GDExtensionProductionSegmentV1 segment = {};
		MutexLock lock(e.registry->mutex);
		CHECK(e.registry->production_close_segment_locked(r, segment) == OK);
		return segment;
	}
	static void execute_transfers(Enrollment &e, Request &r, bool deliver, Request *additional = nullptr) {
		using namespace GDExtensionDeviceExit;
		if (e.driver.worker_mode) {
			// Original RD worker submission and main semaphore chain. As below,
			// only the local Registry's main capture is dispatched mechanically:
			// qualification hooks otherwise target the installed singleton.
			REQUIRE(e.rd._end_frame(true) == OK);
			{
				MutexLock lock(e.registry->mutex);
				e.registry->production_seal_graphs_locked(e.rd);
			}
			REQUIRE(e.rd._execute_frame(false) == OK);
			{
				MutexLock lock(e.registry->mutex);
				for (uint32_t slot = 0; slot < e.rd.frames[0].chain_reports.size(); ++slot) {
					const auto &report = e.rd.frames[0].chain_reports[slot];
					e.registry->production_capture_issue_locked(e.rd, slot, NativeSourceIssueView{ report }, report.error);
				}
			}
			if (deliver) {
				CHECK(e.rd._stall_for_frame(0, true) == OK);
			}
			return;
		}
		RDG::CommandBufferPool pool;
		auto primary = RDD::CommandBufferID(90);
		e.rd.draw_graph.end(false, false, primary, pool);
		e.rd._seal_buffer_readback_records();
		{
			MutexLock lock(e.registry->mutex);
			e.registry->production_seal_graphs_locked(e.rd);
		}
		// Resolve each actual slot from the graph; a shared slot issues once,
		// while every graph membership still captures its original source edge.
		RDD::CommandBufferID buffers[32];
		bool used[32] = {};
		for (auto *request : { &r, additional }) {
			if (!request) {
				continue;
			}
			for (uint32_t g = 0; g < request->graph_count; ++g) {
				if (request->graphs[g].device_epoch != e.rd.frames[0].qualification_epoch) {
					continue;
				}
				uint32_t slot = UINT32_MAX;
				RDD::CommandBufferID actual;
				const bool found = e.rd.draw_graph.resolve_record_command_buffer_slot(request->graphs[g].handle, slot) && e.rd.draw_graph.resolve_record_command_buffer(request->graphs[g].handle, actual);
				CHECK(found);
				CHECK(slot < 32);
				if (!found || slot >= 32) {
					return;
				}
				if (used[slot]) {
					CHECK(buffers[slot] == actual);
				}
				buffers[slot] = actual;
				used[slot] = true;
			}
		}
		for (uint32_t slot = 0; slot < 32; ++slot) {
			if (!used[slot]) {
				continue;
			}
			RDD::QueueCallReport report;
			CHECK(e.driver.command_queue_execute_and_present_observed(RDD::CommandQueueID(1), {}, buffers[slot], {}, RDD::FenceID(93), {}, report) == OK);
			e.rd._observe_buffer_readback_issue(slot, buffers[slot], OK, report);
			{
				MutexLock lock(e.registry->mutex);
				e.registry->production_capture_issue_locked(e.rd, slot, NativeSourceIssueView{ report }, OK);
			}
			e.rd.frames[0].chain_reports.push_back(report);
		}
		e.rd.frames[0].fence_signaled = true;
		if (deliver) {
			CHECK(e.rd._stall_for_frame(0, true) == OK);
		}
	}
	static void ordinal_transfer_dispatch_roundtrip(bool cancel_before_completion, bool partial_upload = false) {
		using namespace GDExtensionDeviceExit;
		// Existing RD graph + deterministic CPU backing driver. This exercises
		// real checked native mechanics, not a GPU kernel or Runtime source grant.
		Enrollment e;
		auto c = capacity(8);
		c.readbacks = 1;
		c.retained_payload_bytes = partial_upload ? 112 : 48;
		c.readback_bytes = 16;
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		ProductionAttachment *attachment = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation = { production_dto_abi<GDExtensionProductionOperationV1>(), 271, 3, 10, 11 };
		const GDExtensionProductionActionV2 actions[] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 1, 10, 1, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 2, 10, 2, 0, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 2, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 }
		};
		const GDExtensionProductionUseV1 uses[] = {
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, partial_upload ? 96u : 32u, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 4, 16, 0 }
		};
		const GDExtensionProductionReadbackV2 readback = { 1, 0, 1, 2, 4, 16 };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, actions }, 3, resources, 3, uses, 3, { nullptr, &readback, operation.canonical_begin }, 1, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		CHECK(r->contract_version == 2);
		CHECK(r->action_count == 3);
		CHECK(r->operation.canonical_end - r->operation.canonical_begin == 1);
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(create_storage(e, ref, 1, &origin, &backing).native_error == OK);
		const auto *buffer = e.rd.storage_buffer_owner.get_or_null(r->resources[1].primary);
		REQUIRE(buffer != nullptr);
		REQUIRE(buffer->driver_id.id < 32);
		e.driver.buffer_cells[buffer->driver_id.id] = &r->resources[1].owned_physical;
		REQUIRE(import_original_unit(e, *r));
		const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		REQUIRE(create_set(e, ref, 2, 0, 0, &binding, 1).native_error == OK);
		e.driver.set_cell = &r->resources[2].owned_physical;
		uint8_t payload[96];
		for (uint32_t n = 0; n < 96; ++n) {
			payload[n] = uint8_t(n ^ 0x5a);
		}
		const GDExtensionProductionUploadV2 upload = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 0, 0, 1, 0, 0, partial_upload ? 96u : 32u };
		const uint32_t push[] = { 7, 11, 13, 17 };
		GDExtensionProductionDispatchV2 dispatch = { production_dto_abi<GDExtensionProductionDispatchV2>(), 1, 10, 1, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
		REQUIRE(e.registry->production_prepare_upload_v2(ref, &upload, payload, upload.bytes).native_error == OK);
		++dispatch.source_recipe_substep;
		CHECK(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == ERR_INVALID_DATA);
		CHECK_FALSE(r->actions[1].prepared);
		--dispatch.source_recipe_substep;
		REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
		REQUIRE(e.registry->production_prepare_readback_v2(ref, 2, 0).native_error == OK);
		e.rd.upload_staging_buffers.block_size = partial_upload ? 64 : 128;
		e.rd.upload_staging_buffers.max_size = partial_upload ? 64 : 256;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		e.rd.download_staging_buffers.block_size = 128;
		e.rd.download_staging_buffers.max_size = 256;
		e.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		REQUIRE(e.rd._insert_staging_block(e.rd.upload_staging_buffers) == OK);
		REQUIRE(e.rd._insert_staging_block(e.rd.download_staging_buffers) == OK);
		if (partial_upload) {
			// The existing bounded staging path records a real upload prefix;
			// neither the next physical action nor the logical requirement advances.
			const auto prefix = effect(e, *r, 0);
			REQUIRE(prefix.state == CheckedRecordAppend::State::KnownPrefix);
			CHECK(prefix.error == ERR_BUSY);
			CHECK(r->actions[0].recorded_bytes == 64);
			CHECK(r->next_action == 0);
			REQUIRE(r->graph_count == 1);
			CHECK_FALSE(r->effects_pending);
			GDExtensionProductionReportV2 prefix_report = {};
			const auto query = budget(0, 0);
			REQUIRE(e.registry->production_observe_v2(ref, &query, &prefix_report).native_error == OK);
			CHECK(prefix_report.recorded_action_end == 0);
			CHECK(prefix_report.action_count == 3);
			CHECK(prefix_report.operation.canonical_begin == 10);
			CHECK(prefix_report.operation.canonical_end == 11);
			CHECK(prefix_report.possible_obligations > 0);
			CHECK(prefix_report.ready_readbacks == 0);
			GDExtensionProductionSegmentV2 prefix_segment = {};
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_close_segment_locked(*r, prefix_segment) == OK);
			}
			CHECK(prefix_segment.first_action_ordinal == 0);
			CHECK(prefix_segment.end_action_ordinal == 0);
			CHECK(prefix_segment.incomplete_upload_action_ordinal == 0);
			CHECK(effect(e, *r, 0).error == ERR_BUSY);
			CHECK(r->graph_count == 1);
			CHECK(r->actions[0].recorded_bytes == 64);
			CHECK(e.registry->production_cancel(ref).native_error == OK);
			CHECK(effect(e, *r, 1).error != OK);
			CHECK(r->graph_count == 1);
			execute_transfers(e, *r, false);
			REQUIRE(r->native_count == 1);
			const auto incomplete_prefix = turn(e, *r, ProductionControlKind::OBSERVE, budget());
			CHECK(incomplete_prefix.report.recorded_action_end == 0);
			CHECK(incomplete_prefix.report.action_count == 3);
			CHECK(incomplete_prefix.report.complete_obligations == 0);
			CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
			CHECK(e.driver.set_frees == 0);
			CHECK(e.driver.buffer_frees == 0);
			e.driver.complete = true;
			REQUIRE(e.rd._stall_for_frame(0, true) == OK);
			const auto completed_prefix = turn(e, *r, ProductionControlKind::OBSERVE, budget());
			CHECK(completed_prefix.report.known_obligations > 0);
			CHECK(completed_prefix.report.complete_obligations == completed_prefix.report.known_obligations);
			CHECK(completed_prefix.report.recorded_action_end == 0);
			CHECK(completed_prefix.report.action_count == 3);
			CHECK(completed_prefix.report.cancelled == 1);
			CHECK(completed_prefix.report.ready_readbacks == 0);
			GDExtensionProductionFactV2 prefix_facts[2] = {};
			uint32_t prefix_fact_count = 0;
			REQUIRE(e.registry->production_facts_v2(ref, r->revision, 0, prefix_facts, 2, &prefix_fact_count).native_error == OK);
			REQUIRE(prefix_fact_count == 2);
			CHECK(prefix_facts[0].action_ordinal == 0);
			CHECK(prefix_facts[1].action_ordinal == 0);
			CHECK(prefix_facts[0].canonical_suboperation == 10);
			CHECK(prefix_facts[1].canonical_suboperation == 10);
			CHECK(prefix_facts[1].phase == 3);
			CHECK(prefix_facts[1].membership_parent == 0);
			CHECK(e.driver.dispatches == 0);
			CHECK(r->readbacks[0].source.issued == 0);
			uint8_t absent[16] = {};
			uint64_t absent_bytes = 91;
			CHECK(e.registry->production_copy(ref, r->revision, 0, absent, sizeof(absent), &absent_bytes).native_error == ERR_BUSY);
			CHECK(absent_bytes == 91);
			{
				MutexLock lock(e.registry->mutex);
				CHECK(e.registry->production_ack_segment_locked(*r, prefix_segment.segment, prefix_segment.segment_revision) == OK);
			}
			for (uint32_t n = 0; n < 128 && !r->resources[2].returned; ++n) {
				const auto observed = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
				CHECK(observed.service_usage.passes <= 1);
				CHECK(observed.service_usage.receipts <= 1);
			}
			REQUIRE(r->resources[2].returned);
			REQUIRE(return_owned_storage(e, *r, 1));
			CHECK(e.driver.set_frees == 1);
			CHECK(e.driver.buffer_frees == 1);
			const auto released_prefix = turn(e, *r, ProductionControlKind::RELEASE, budget());
			CHECK(released_prefix.owner_return.native_error == OK);
			CHECK(r->serial == 0);
			CHECK(e.registry->production_close(attachment_ref).native_error == OK);
			CHECK(e.rd.production_entity_holds == nullptr);
			return;
		}

		const auto out_of_order = effect(e, *r, 1);
		CHECK(out_of_order.state == CheckedRecordAppend::State::Rejected);
		CHECK(out_of_order.error == ERR_UNAUTHORIZED); // The original effect guard rejects a foreign next-action index.
		CHECK(r->graph_count == 0);
		CHECK(r->next_action == 0);
		for (uint32_t ordinal = 0; ordinal < 3; ++ordinal) {
			const auto append = effect(e, *r, ordinal);
			REQUIRE(append.state == CheckedRecordAppend::State::Appended);
			REQUIRE(append.error == OK);
			CHECK(r->next_action == ordinal + 1);
			if (ordinal == 0) {
				// Same logical canonical must not allow another physical row to
				// adopt the first row's exact already-captured graph handle.
				MutexLock rd_lock(e.rd._thread_safe_);
				MutexLock lock(e.registry->mutex);
				CHECK(e.registry->production_capture_graph_locked(*r, e.rd, 1, append) == ERR_INVALID_DATA);
				CHECK(r->graph_count == 1);
			}
		}
		REQUIRE(r->graph_count == 3);
		GDExtensionProductionSegmentV2 segment = {};
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_close_segment_locked(*r, segment) == OK);
		}
		CHECK(segment.first_action_ordinal == 0);
		CHECK(segment.end_action_ordinal == 3);
		CHECK(segment.incomplete_upload_action_ordinal == UINT64_MAX);
		uint8_t output[16] = {};
		uint64_t written = 77;
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		CHECK(written == 77);
		execute_transfers(e, *r, false);
		REQUIRE(r->native_count == 3);
		CHECK(e.driver.dispatches == 1);
		CHECK(e.driver.set_binds == 1);
		for (uint32_t n = 0; n < 4; ++n) {
			CHECK(e.driver.pushed[n] == push[n]);
		}
		if (cancel_before_completion) {
			CHECK(e.registry->production_cancel(ref).native_error == OK);
		}
		const auto incomplete = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(incomplete.report.recorded_action_end == 3);
		CHECK(incomplete.report.action_count == 3);
		CHECK(incomplete.report.operation.canonical_begin == 10);
		CHECK(incomplete.report.operation.canonical_end == 11);
		CHECK(incomplete.report.complete_obligations == 0);
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		CHECK(e.driver.buffer_frees == 0);
		CHECK(e.driver.set_frees == 0);
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		e.driver.complete = true; // Original deterministic native-source seam, not a GPU claim.
		REQUIRE(e.rd._stall_for_frame(0, true) == OK);
		const auto complete = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(complete.report.known_obligations > 0);
		CHECK(complete.report.complete_obligations == complete.report.known_obligations);
		GDExtensionProductionFactV2 facts[16] = {};
		uint32_t fact_count = 0;
		REQUIRE(e.registry->production_facts_v2(ref, r->revision, 0, facts, 16, &fact_count).native_error == OK);
		REQUIRE(fact_count == r->graph_count + r->native_count);
		REQUIRE(fact_count == 6);
		bool seen[3] = {};
		bool seen_native[3] = {};
		for (uint32_t n = 0; n < fact_count; ++n) {
			REQUIRE(facts[n].action_ordinal < 3);
			CHECK(facts[n].canonical_suboperation == 10);
			CHECK(facts[n].source_recipe_substep == facts[n].action_ordinal);
			seen[facts[n].action_ordinal] = true;
			if (facts[n].phase == 3) {
				REQUIRE(facts[n].membership_parent < r->graph_count);
				CHECK(facts[n].action_ordinal == r->graphs[facts[n].membership_parent].fact.action_ordinal);
				seen_native[facts[n].action_ordinal] = true;
			}
		}
		CHECK(seen[0]);
		CHECK(seen[1]);
		CHECK(seen[2]);
		CHECK(seen_native[0]);
		CHECK(seen_native[1]);
		CHECK(seen_native[2]);
		REQUIRE(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == OK);
		CHECK(written == 16);
		// The existing CPU driver validates dispatch binding/order but does not
		// emulate shader instructions: verify only the uploaded/read-back bytes.
		for (uint32_t n = 0; n < 16; ++n) {
			CHECK(output[n] == payload[n + 4]);
		}
		const uint64_t original_backing = r->readbacks[0].source.backing_serial;
		++r->readbacks[0].source.backing_serial;
		written = 99;
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		CHECK(written == 99);
		r->readbacks[0].source.backing_serial = original_backing;
		{
			MutexLock lock(e.registry->mutex);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision + 1) == ERR_DOES_NOT_EXIST);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision) == OK);
		}
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		for (uint32_t n = 0; n < 128 && !r->resources[2].returned; ++n) {
			const auto observed = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
			CHECK(observed.service_usage.passes <= 1);
			CHECK(observed.service_usage.receipts <= 1);
		}
		REQUIRE(r->resources[2].returned);
		CHECK(e.driver.set_frees == 1);
		REQUIRE(return_owned_storage(e, *r, 1));
		CHECK(e.driver.buffer_frees == 1);
		const auto release = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(release.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		CHECK(e.rd.production_entity_holds == nullptr);
	}

	static void stall_for_frame_retains_production_sources() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = transfer_setup(e);
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		for (uint32_t i = 0; i < 3; ++i) {
			REQUIRE(effect(e, *r, i).error == OK);
		}
		// Issue frame 0 WITHOUT delivering/stalling or running any OBSERVE turn first.
		execute_transfers(e, *r, false);
		REQUIRE(r->native_count > 0);
		const uint64_t issued_serial = r->natives[0].member.source.serial;
		REQUIRE((issued_serial > 0 && issued_serial < 128));
		CHECK_FALSE(r->natives[0].member.retained);
		CHECK(e.driver.retains[issued_serial] == 0);

		// BUG-69: If retain returns ERR_BUSY, ordinary _stall_for_frame MUST return ERR_BUSY
		// without recycling the frame fence (which would drop unretained completion records).
		e.driver.complete = true;
		e.driver.main_frame_retain_required = true;
		e.driver.retain_error = ERR_BUSY;
		CHECK(e.rd._stall_for_frame(0, true) == ERR_BUSY);
		CHECK(e.driver.fence_recycles == 0);
		CHECK_FALSE(r->natives[0].member.retained);

		// Once retain succeeds, ordinary _stall_for_frame MUST retain all unretained production
		// sources BEFORE recycling the frame fence, so subsequent OBSERVE completes cleanly.
		e.driver.retain_error = OK;
		REQUIRE(e.rd._stall_for_frame(0, true) == OK);
		CHECK(e.driver.fence_recycles == 1);
		CHECK(r->natives[0].member.retained);
		CHECK(e.driver.retains[issued_serial] > 0);
		e.driver.main_frame_retain_required = false;

		const auto observed = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(observed.owner_return.native_error == OK);
		CHECK(observed.report.known_obligations > 0);
		CHECK(observed.report.complete_obligations == observed.report.known_obligations);

		CHECK(e.registry->production_cancel(ref).native_error == OK);
		auto retire_slot = [&](uint32_t slot) {
			GDExtensionStorageRetireRefV1 retire = {};
			REQUIRE(retire_storage(e, *r, slot, retire));
			const auto b = budget(16, 4);
			GDExtensionStorageRetireReportV1 report = {};
			for (uint32_t turn_index = 0; turn_index < 32 && report.state != 1; ++turn_index) {
				const auto err = observe_storage(e, ref, retire, &b, &report).native_error;
				CHECK((err == OK || err == ERR_BUSY));
			}
			CHECK(report.state == 1);
			CHECK(e.registry->production_storage_release(ref, retire).native_error == OK);
			CHECK(finish_owned_maintenance(e, *r, b));
		};
		retire_slot(0);
		retire_slot(1);
		const auto release = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(release.owner_return.native_error == OK);
		CHECK(r->serial == 0);
	}

	static void transfer_roundtrip(uint32_t source_count = 1) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.sources_per_issue = source_count;
		auto *r = transfer_setup(e);
		if (!r) {
			return;
		}
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		for (uint32_t i = 0; i < 3; ++i) {
			const auto append = effect(e, *r, i);
			CHECK(append.error == OK);
			CHECK(append.state == CheckedRecordAppend::State::Appended);
			CHECK(r->next_action == i + 1);
		}
		CHECK(r->graph_count == 3);
		CHECK(r->readbacks[0].source.issued == 16);
		CHECK(r->readbacks[0].source.completed == 0);
		const auto segment = close_segment(e, *r);
		CHECK(segment.first_canonical == 10);
		CHECK(segment.end_canonical == 13);
		CHECK(e.registry->production_close({ Enrollment::INSTANCE, r->attachment->serial }).native_error == ERR_BUSY);
		uint8_t output[16] = {};
		uint64_t written = 77;
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		CHECK(written == 77);
		e.driver.complete = true;
		execute_transfers(e, *r, true);
		CHECK(e.driver.copy_calls == 3);
		CHECK(r->native_count == r->graph_count * source_count);
		CHECK(e.driver.queue_issues == source_count);
		CHECK(r->readbacks[0].source.completed == 16);
		CHECK(r->readbacks[0].source.delivered);
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		e.driver.native_error = true;
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		e.driver.native_error = false;
		const auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(out.report.complete_obligations > 0);
		CHECK(out.report.complete_obligations == out.report.known_obligations);
		CHECK(e.registry->production_copy(ref, r->revision - 1, 0, output, sizeof(output), &written).native_error == ERR_DOES_NOT_EXIST);
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, 15, &written).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == OK);
		CHECK(written == 16);
		for (uint32_t i = 0; i < 16; ++i) {
			CHECK(output[i] == uint8_t((i + 4) ^ 0x5a));
		}
		const auto original_offset = r->readbacks[0].source.offset;
		++r->readbacks[0].source.offset; // Corrupt only after real delivery: exact range must fail closed.
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		r->readbacks[0].source.offset = original_offset;
		const auto backing = r->readbacks[0].source.backing_serial;
		++r->readbacks[0].source.backing_serial;
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		r->readbacks[0].source.backing_serial = backing;
		++r->readbacks[0].source.request_serial;
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == ERR_BUSY);
		r->readbacks[0].source.request_serial = r->serial;

		{
			MutexLock lock(e.registry->mutex);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision + 1) == ERR_DOES_NOT_EXIST);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision) == OK);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision) == OK);
		}
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		CHECK(e.registry->production_copy(ref, r->revision, 0, output, sizeof(output), &written).native_error == OK);
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		if (!return_owned_storage(e, *r, 0) || !return_owned_storage(e, *r, 1)) {
			return;
		}
		CHECK(e.driver.buffer_frees == 2);
		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment).native_error == OK);
		CHECK(e.rd.production_entity_holds == nullptr);
	}
	static void upload_prefix_frontier() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = transfer_setup(e, true);
		if (!r) {
			return;
		}
		const auto first = effect(e, *r, 0);
		CHECK(first.state == CheckedRecordAppend::State::KnownPrefix);
		CHECK(first.error == ERR_BUSY);
		CHECK(r->next_action == 0);
		CHECK(r->actions[0].recorded_bytes == 64);
		CHECK(r->graph_count == 1);
		CHECK_FALSE(r->effects_pending);
		const auto segment = close_segment(e, *r);
		CHECK(segment.first_canonical == 10);
		CHECK(segment.end_canonical == 10);
		CHECK(segment.incomplete_upload_parent == 10);
		const auto graph_count = r->graph_count;
		CHECK(effect(e, *r, 0).error == ERR_BUSY);
		CHECK(r->graph_count == graph_count);
		e.driver.complete = true;
		execute_transfers(e, *r, false);
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		{
			MutexLock lock(e.registry->mutex);
			CHECK(e.registry->production_ack_segment_locked(*r, segment.segment, segment.segment_revision) == OK);
		}
		CHECK(e.rd._stall_for_frame(0, true) == OK);
		++e.rd.frames_drawn;
		++e.rd.frames[0].qualification_epoch;
		e.rd.draw_graph.begin();
		const auto suffix = effect(e, *r, 0);
		CHECK(suffix.state == CheckedRecordAppend::State::Appended);
		CHECK(r->actions[0].recorded_bytes == 96);
		CHECK(r->next_action == 1);
		CHECK(r->graph_count == 2);
		CHECK(r->graphs[1].captured.begin == 64);
		CHECK(r->graphs[1].captured.end == 96);
		for (uint32_t i = 1; i < 3; ++i) {
			const auto append = effect(e, *r, i);
			CHECK(append.error == OK);
			CHECK(append.state == CheckedRecordAppend::State::Appended);
		}
		const auto final_segment = close_segment(e, *r);
		CHECK(final_segment.end_canonical == 13);
		execute_transfers(e, *r, true);
		CHECK(e.driver.copy_calls == 4);
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		uint8_t bytes[96] = {};
		uint64_t written = 0;
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		CHECK(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == OK);
		CHECK(written == 96);
		for (uint32_t i = 0; i < 96; ++i) {
			CHECK(bytes[i] == uint8_t(i ^ 0x5a));
		}
		{
			MutexLock lock(e.registry->mutex);
			CHECK(e.registry->production_ack_segment_locked(*r, final_segment.segment, final_segment.segment_revision) == OK);
		}
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		CHECK(effect(e, *r, 1).error != OK);
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		if (!return_owned_storage(e, *r, 0) || !return_owned_storage(e, *r, 1)) {
			return;
		}
		const auto returned = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(returned.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment).native_error == OK);
	}
	static void membership_capacity_before_effect() {
		using namespace GDExtensionDeviceExit;
		for (uint32_t sources = 1; sources <= 2; ++sources) {
			Enrollment e;
			e.driver.sources_per_issue = sources;
			auto large = capacity(8);
			large.native_memberships *= sources;
			const auto attachment_ref = e.accept(large);
			if (!attachment_ref.handle_serial) {
				return;
			}
			auto small = large;
			small.suboperations = sources == 1 ? 1 : 2;
			small.resources = small.resource_uses = small.return_cells = 1;
			small.graph_memberships = sources == 1 ? 8 : 2;
			small.native_memberships = small.suboperations;
			GDExtensionProductionMetadataQuoteV1 scalar_layout = {};
			CHECK(e.registry->production_layout(small, scalar_layout) == OK);
			const GDExtensionProductionOperationV1 op = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 201, 1, 10, 10 + small.suboperations };
			GDExtensionProductionActionV1 actions[2] = {
				{ 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
				{ 11, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX }
			};
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 128, 0 };
			Request *request = nullptr;
			Error reserved = FAILED;
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *attachment = nullptr;
				RenderingDevice *rd = nullptr;
				const Error found = e.registry->production_attachment_locked(attachment_ref, true, attachment, rd);
				CHECK(found == OK);
				if (found != OK) {
					return;
				}
				reserved = e.registry->production_reserve_metadata_locked(*attachment, op, small, actions, small.suboperations, &resource, 1, &use, 1, nullptr, 0, request);
				CHECK(reserved == ERR_PARAMETER_RANGE_ERROR);
				CHECK(request == nullptr);
				CHECK(attachment->children == 0);
				if (request) {
					CHECK_FALSE(request->effects_pending);
				}
			}
			CHECK(e.driver.receipt_calls() == 0);
			CHECK(e.driver.allocations == 0);
			CHECK(e.driver.set_creates == 0);
			CHECK(e.driver.preparations == 0);
			CHECK(e.rd.draw_graph.qualification_command_count() == 1);
			if (request) { // RED cleanup only, not acceptance of the under-reserved request.
				const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, request->serial };
				CHECK(e.registry->production_cancel(ref).native_error == OK);
				CHECK(e.registry->production_release(ref).native_error == OK);
			}
			CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		}
	}
	static void reflected_dispatch_use_rejection() {
		using namespace GDExtensionDeviceExit;
		for (uint32_t mismatch : { 1u, 2u, 3u, 4u, 9u }) {
			Enrollment e;
			auto *r = owned_setup(e, true, mismatch);
			if (!r) {
				return;
			}
			const auto before = e.driver.receipt_calls();
			const auto commands = e.rd.draw_graph.qualification_command_count();
			const auto append = effect(e, *r, 0);
			CHECK(append.state == CheckedRecordAppend::State::Rejected);
			CHECK((append.error == ERR_INVALID_DATA || append.error == ERR_UNCONFIGURED));
			CHECK_FALSE(append.handle.is_valid());
			CHECK_FALSE(r->compute_list_open);
			CHECK_FALSE(e.rd.compute_list.active);
			CHECK_FALSE(r->effects_pending);
			CHECK(r->next_action == 0);
			CHECK(r->graph_count == 0);
			CHECK(e.rd.draw_graph.qualification_command_count() == commands);
			CHECK(e.driver.receipt_calls() == before);
			CHECK(e.driver.preparations == 0);
			CHECK(e.driver.dispatches == 0);
			CHECK(e.driver.set_prepares == 0);
			CHECK(e.driver.set_binds == 0);
			const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
			CHECK(e.registry->production_cancel(ref).native_error == OK);
			// The original frame began before this no-effect request. Its actual
			// maintenance issue still needs the CPU driver's real completion fact;
			// cancellation cannot turn that outstanding frame into a free receipt.
			for (uint32_t turn_index = 0; turn_index < 128 && (!e.driver.queue_issues || !e.driver.polls[1]); ++turn_index) {
				const auto output = turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
				CHECK(output.service_usage.receipts <= 1);
				CHECK(output.service_usage.passes <= 1);
			}
			CHECK(e.driver.queue_issues > 0);
			CHECK(e.driver.polls[1] > 0);
			CHECK_FALSE(r->resources[2].returned);
			CHECK(e.driver.set_frees == 0);
			e.driver.complete = true; // Original deterministic native completion seam.
			for (uint32_t turn_index = 0; turn_index < 128 && !r->resources[2].returned; ++turn_index) {
				turn(e, *r, ProductionControlKind::OBSERVE, budget(1, 1));
			}
			CHECK(r->resources[2].returned);
			if (!r->resources[2].returned || !return_owned_storage(e, *r, 1)) {
				return;
			}
			if (mismatch == 3 && !return_owned_storage(e, *r, 3)) {
				return;
			}
			const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
			CHECK(e.registry->production_release(ref).native_error == OK);
			CHECK(e.registry->production_close(attachment).native_error == OK);
		}
	}
	static void post_graph_hidden_source() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		graph_initialize(e);
		e.driver.hidden_mode = true;
		e.driver.complete = true;
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(80);
		const RID shader_rid = e.rd.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(81);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = 16;
		pipeline.local_group_size[0] = pipeline.local_group_size[1] = pipeline.local_group_size[2] = 1;
		const RID pipeline_rid = e.rd.compute_pipeline_owner.make_rid(pipeline);
		// Original graph execution, not the test, makes hidden work appear.
		e.rd.frames[0].primary_command_buffer_begun = e.driver.command_buffer_begin(e.rd.frames[0].command_buffer);
		const auto list = e.rd.compute_list_begin();
		CHECK(list != int64_t(RenderingDevice::INVALID_ID));
		if (list == RenderingDevice::INVALID_ID) {
			e.rd.compute_pipeline_owner.free(pipeline_rid);
			e.rd.shader_owner.free(shader_rid);
			return;
		}
		e.rd.compute_list_bind_compute_pipeline(list, pipeline_rid);
		const OriginalOperationIdentity identity = { 231, 1, 1, 1, 10, 11 };
		const uint32_t push[] = { 7, 11, 13, 17 };
		const auto append = e.rd._record_source_dispatch(identity, list, 2, 3, 4, push, sizeof(push));
		CHECK(append.state == CheckedRecordAppend::State::Appended);
		e.rd.compute_list_end();
		const RenderingDevice::NativeDisposalBudget budget = { 2, 1, OS::get_singleton()->get_ticks_usec() + 10000000 };
		auto step = [&]() {
			RenderingDevice::NativeDisposalUsage usage;
			const auto before = e.driver.receipt_calls();
			BudgetDriverGuard guard(e.driver);
			const Error error = e.rd._native_disposal_maintenance_step(budget, usage);
			const uint32_t receipts = usage.polls + usage.frees + usage.issues + usage.retains + usage.releases + usage.recycles;
			CHECK(receipts <= budget.receipts);
			CHECK(usage.passes <= budget.passes);
			CHECK(e.driver.receipt_calls() - before == receipts);
			CHECK((error == OK || error == ERR_BUSY));
			return error;
		};
		for (uint32_t i = 0; i < 128 && (!e.driver.hidden_serial || !e.driver.polls[e.driver.hidden_serial]); ++i) {
			CHECK(step() == ERR_BUSY);
		}
		CHECK(e.driver.dispatches == 1);
		CHECK(e.driver.hidden_busy_seen);
		CHECK(e.driver.hidden_issues == 1);
		CHECK(e.driver.preparations >= 3);
		CHECK(e.rd.native_disposal_active);
		if (e.driver.hidden_serial) {
			CHECK(e.driver.polls[e.driver.hidden_serial] > 0);
		}
		e.driver.hidden_ready = true; // Controlled original driver completion only.
		Error finished = ERR_BUSY;
		for (uint32_t i = 0; i < 128 && finished == ERR_BUSY; ++i) {
			finished = step();
		}
		CHECK(finished == OK);
		CHECK_FALSE(e.rd.native_disposal_active);
		CHECK(e.driver.dispatches == 1);
		CHECK(e.driver.hidden_issues == 1);
		CHECK(e.driver.legacy_end_segments == 0);
		CHECK(e.rd.frames[0].qualification_hidden_reports.is_empty());
		e.rd.compute_pipeline_owner.free(pipeline_rid);
		e.rd.shader_owner.free(shader_rid);
	}
	static Request *borrowed_readback_setup(Enrollment &e, bool incoming_pool = false) {
		using namespace GDExtensionDeviceExit;
		auto c = capacity(4);
		if (e.driver.worker_mode) {
			c.native_memberships *= 2; // Original quote covers main + worker per graph.
		}
		c.retained_payload_bytes = c.readback_bytes = 16;
		c.readbacks = 1;
		auto pool = c;
		if (incoming_pool) {
			pool.requests = 2;
			pool.suboperations *= 2;
			pool.resources *= 2;
			pool.resource_uses *= 3;
			pool.graph_memberships *= 2;
			pool.native_memberships *= 2;
			pool.return_cells *= 2;
			pool.readbacks *= 3;
			pool.retained_payload_bytes *= 2;
			pool.readback_bytes *= 3;
		}
		const auto attachment_ref = e.accept(pool);
		if (!attachment_ref.handle_serial) {
			return nullptr;
		}
		const GDExtensionProductionOperationV1 op = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 251, 1, 10, incoming_pool ? 11u : 12u };
		const GDExtensionProductionActionV1 actions[] = {
			{ 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 11, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 };
		const GDExtensionProductionUseV1 uses[] = { { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 16, 0 }, { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 } };
		const GDExtensionProductionReadbackV1 readback = { 0, 0, 10, 11, 0, 16 };
		Request *r = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *attachment = nullptr;
			RenderingDevice *rd = nullptr;
			Error error = e.registry->production_attachment_locked(attachment_ref, true, attachment, rd);
			if (error == OK) {
				error = e.registry->production_reserve_metadata_locked(*attachment, op, c, actions, incoming_pool ? 1 : 2, &resource, 1, uses, incoming_pool ? 1 : 2, incoming_pool ? nullptr : &readback, incoming_pool ? 0 : 1, r);
			}
			CHECK(error == OK);
			if (error != OK) {
				return nullptr;
			}
		}
		graph_initialize(e);
		e.original_bridge = memnew(Registry::QualificationBridge);
		e.original_bridge->serial = 301;
		e.original_bridge->epoch = r->attachment->epoch;
		e.registry->qualification_bridges[0] = e.original_bridge;
		e.original_storage = memnew(Registry::QualificationBridge::Storage);
		auto &storage = *e.original_storage;
		storage.serial = 302;
		storage.creating = false;
		storage.origin = { Enrollment::ABI, 128, 0, 0, 1, 1, 0 };
		storage.physical.bridge = e.original_bridge->serial;
		storage.physical.child = storage.serial;
		e.original_bridge->storage[0] = &storage;
		const RID rid = e.rd._qualification_storage_create(128, &storage.physical);
		CHECK(rid.is_valid());
		if (rid.is_null()) {
			return nullptr;
		}
		const auto *buffer = e.rd.storage_buffer_owner.get_or_null(rid);
		storage.backing = { Enrollment::ABI, { Enrollment::INSTANCE, storage.serial }, rid.get_id(), buffer->backing_serial, buffer->size, uint64_t(buffer->usage), OK, 1 };
		e.driver.buffer_cells[buffer->driver_id.id] = &storage.physical;
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const auto imported = e.registry->production_import_storage(ref, 0, &storage.backing).native_error;
		CHECK(imported == OK);
		if (imported != OK) {
			return nullptr;
		}
		uint8_t payload[16];
		for (uint32_t i = 0; i < 16; ++i) {
			payload[i] = uint8_t(i ^ 0x5a);
		}
		const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 16 };
		const auto prepared = e.registry->production_prepare_upload(ref, &upload, payload, sizeof(payload)).native_error;
		const auto copied = incoming_pool ? OK : e.registry->production_prepare_readback(ref, 11, 0).native_error;
		CHECK(prepared == OK);
		CHECK(copied == OK);
		if (prepared != OK || copied != OK) {
			return nullptr;
		}
		e.rd.upload_staging_buffers.block_size = e.rd.download_staging_buffers.block_size = 128;
		e.rd.upload_staging_buffers.max_size = e.rd.download_staging_buffers.max_size = 128;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		e.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		const auto up = e.rd._insert_staging_block(e.rd.upload_staging_buffers);
		const auto down = e.rd._insert_staging_block(e.rd.download_staging_buffers);
		CHECK(up == OK);
		CHECK(down == OK);
		return up == OK && down == OK ? r : nullptr;
	}
	static void incoming_readback_roundtrip(uint32_t mode = 0) {
		using namespace GDExtensionDeviceExit;
		// Original real RD storage/import and graph/copy driver; no shader or
		// CPU result fabrication. The prior request owns the actual upload.
		Enrollment e;
		auto *producer = borrowed_readback_setup(e, true);
		REQUIRE(producer != nullptr);
		auto &attachment = *producer->attachment;
		const GDExtensionProductionAttachmentRefV1 attachment_ref{ Enrollment::INSTANCE, attachment.serial };
		const bool output_prefix = mode == 7;
		const bool prefix = mode == 1 || mode == 2 || output_prefix;
		const bool rejected_prefix = mode == 1 || output_prefix;
		const bool postwrite = mode == 3;
		const bool mixed = mode == 6;
		const uint32_t count = mixed ? 3 : prefix || postwrite ? 2
															   : 1;
		const uint64_t copy_ordinal = prefix ? 1 : 0;
		auto c = capacity(4);
		c.readbacks = mixed ? 2 : 1;
		c.readback_bytes = mixed ? 24 : 16;
		c.retained_payload_bytes = 16;
		GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 252, 1, 10, 11 };
		GDExtensionProductionActionV2 actions[3] = {};
		GDExtensionProductionUseV1 uses[3] = {};
		for (uint32_t i = 0; i < count; ++i) {
			const bool read = i == copy_ordinal;
			actions[i] = { i, 10, i, 0, read ? uint32_t(GDEXTENSION_PRODUCTION_READBACK_V1) : uint32_t(GDEXTENSION_PRODUCTION_UPLOAD_V1), 0, i, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, read ? 0u : UINT32_MAX };
			uses[i] = { read ? 0u : 1u, read ? uint32_t(GDEXTENSION_PRODUCTION_READ_V1) : uint32_t(GDEXTENSION_PRODUCTION_WRITE_V1), read ? 0u : mode == 2 ? 16u
																																						   : 12u,
				read ? 16u : 8u, 0 };
		}
		if (mixed) {
			actions[2].action_kind = GDEXTENSION_PRODUCTION_READBACK_V1;
			actions[2].readback_slot = 1;
			uses[2] = { 1, GDEXTENSION_PRODUCTION_READ_V1, 12, 8, 0 };
		}
		if (output_prefix) {
			actions[0].action_kind = GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1;
			actions[0].physical_unit = UINT32_MAX;
			actions[0].list_boundary = GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1;
			uses[0] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 };
		}
		GDExtensionProductionResourceV1 resources[2] = { producer->resources[0].declaration, producer->resources[0].declaration };
		const uint32_t resource_count = count > 1 ? 2 : 1;
		GDExtensionProductionReadbackSourceV1 row{ 0, GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1, copy_ordinal, 0, 16, {}, { 1, e.original_storage->backing.backing_serial } };
		GDExtensionProductionReadbackSourceV1 rows[] = { row, { 1, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, 2, 12, 8, { 1 }, {} } };
		Request *r = nullptr;
		// All invalid typed rows must fail before claiming the second arena
		// slice. Zero ordinal is deliberately not interpreted as a writer.
		for (uint32_t invalid = 0; !mixed && invalid < 9; ++invalid) {
			auto bad = row;
			auto bad_uses = uses[copy_ordinal];
			switch (invalid) {
				case 0:
					bad.source_kind = 0;
					break;
				case 1:
					bad.source_kind = 99;
					break;
				case 2:
					bad.writer.producer_action_ordinal = 1;
					break;
				case 3:
					bad.incoming.original_generation = 0;
					break;
				case 4:
					bad.incoming.original_generation = 2;
					break;
				case 5:
					bad.incoming.backing_serial = 0;
					break;
				case 6:
					bad.byte_offset = 127;
					break;
				case 7:
					uses[copy_ordinal].access = GDEXTENSION_PRODUCTION_WRITE_V1;
					break;
				case 8:
					bad.source_kind = GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1;
					break;
			}
			MutexLock lock(e.registry->mutex);
			CHECK(e.registry->production_reserve_metadata_rows_locked(attachment, op, c, { nullptr, actions }, count, resources, resource_count, uses, count, { nullptr, nullptr, 0, &bad }, 1, r, 2) == ERR_INVALID_DATA);
			CHECK(r == nullptr);
			CHECK(attachment.children == 1);
			uses[copy_ordinal] = bad_uses;
		}
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionReadbackV2 old_row{ 0, 0, 0, copy_ordinal, 0, 16 };
			CHECK(e.registry->production_reserve_metadata_rows_locked(attachment, op, c, { nullptr, actions }, count, resources, resource_count, uses, count, { nullptr, &old_row, 0 }, 1, r, 2) == ERR_INVALID_DATA);
			if (mixed) {
				rows[1].writer.producer_action_ordinal = 0; // Incoming copy is not a writer.
				CHECK(e.registry->production_reserve_metadata_rows_locked(attachment, op, c, { nullptr, actions }, count, resources, resource_count, uses, count, { nullptr, nullptr, 0, rows }, 2, r, 2) == ERR_INVALID_DATA);
				CHECK(r == nullptr);
				CHECK(attachment.children == 1);
				rows[1].writer.producer_action_ordinal = 1;
			}
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(attachment, op, c, { nullptr, actions }, count, resources, resource_count, uses, count, { nullptr, nullptr, 0, rows }, mixed ? 2 : 1, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		CHECK(r->contract_version == 2);
		CHECK(r->readbacks[0].declaration.producer_action_ordinal == 0);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		for (uint32_t i = 0; i < resource_count; ++i) {
			REQUIRE(e.registry->production_import_storage(ref, i, &e.original_storage->backing).native_error == OK);
		}
		CHECK(r->resources[0].use_count == 1);
		CHECK(e.original_storage->physical.production_uses == &r->resources[resource_count - 1].uses[0]);
		CHECK(producer->resources[0].use_count == 1);
		CHECK(e.registry->production_prepare_readback(ref, 10 + copy_ordinal, 0).native_error == ERR_INVALID_DATA);
		REQUIRE(e.registry->production_prepare_readback_v2(ref, copy_ordinal, 0).native_error == OK);
		if (mixed) {
			REQUIRE(e.registry->production_prepare_readback_v2(ref, 2, 1).native_error == OK);
		}
		if (output_prefix) {
			GDExtensionProductionOutputPrepareV1 prepared{};
			prepared.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
			prepared.canonical_suboperation = 10;
			prepared.source_use_count = prepared.source_generation = 1;
			prepared.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
			prepared.input.particles = prepared.input.common.generation = 1;
			prepared.input.common.current_camera = { 1, 1, 1 };
			for (uint32_t axis = 0; axis < 3; ++axis) {
				prepared.input.common.bounds.size[axis] = 1;
			}
			prepared.roles[0] = { 1, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			prepared.roles[1] = { 2, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			prepared.roles[2] = { 7, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			REQUIRE(e.registry->production_prepare_output(ref, &prepared).native_error == OK);
			CHECK(r->actions[0].output_source_prepared);
		} else if (count > 1) {
			const uint64_t ordinal = prefix ? 0 : 1;
			const uint8_t replacement[8] = { 201, 202, 203, 204, 205, 206, 207, 208 };
			const GDExtensionProductionUploadV2 upload{ production_dto_abi<GDExtensionProductionUploadV2>(), ordinal, 10, uint32_t(ordinal), 0, 1, 0, uses[ordinal].byte_offset, 8 };
			REQUIRE(e.registry->production_prepare_upload_v2(ref, &upload, replacement, sizeof(replacement)).native_error == OK);
		}
		if (rejected_prefix) {
			const auto before = e.driver.copy_calls;
			const auto allocations = e.driver.allocations;
			const auto rejected = effect(e, *r, 0);
			CHECK(rejected.error == (output_prefix ? ERR_UNAVAILABLE : ERR_INVALID_DATA));
			CHECK(rejected.state == CheckedRecordAppend::State::Rejected);
			CHECK_FALSE(r->frozen);
			CHECK(r->next_action == 0);
			CHECK(r->graph_count == 0);
			CHECK(e.driver.copy_calls == before);
			CHECK(e.driver.allocations == allocations);
			CHECK(r->actions[0].output_state == ProductionActionCell::OutputState::UNBOUND);
			CHECK(r->actions[0].output_root == 0);
		} else {
			// First freeze on the schedule-style Registry-only path, then
			// invalidate the real backing before append to prove dynamic checks.
			{
				MutexLock lock(e.registry->mutex);
				++r->readbacks[0].declaration.incoming.backing_serial;
				CHECK(e.registry->production_freeze_locked(*r) == ERR_INVALID_DATA);
				CHECK_FALSE(r->frozen);
				--r->readbacks[0].declaration.incoming.backing_serial;
				REQUIRE(e.registry->production_freeze_locked(*r) == OK);
			}
			auto *buffer = e.rd.storage_buffer_owner.get_or_null(r->resources[0].primary);
			REQUIRE(buffer != nullptr);
			++buffer->backing_serial;
			CHECK(effect(e, *r, 0).error == ERR_INVALID_DATA);
			CHECK(r->next_action == 0);
			CHECK(r->graph_count == 0);
			--buffer->backing_serial;
		}
		REQUIRE(effect(e, *producer, 0).state == CheckedRecordAppend::State::Appended);
		if (!rejected_prefix) {
			for (uint32_t i = 0; i < count; ++i) {
				REQUIRE(effect(e, *r, i).state == CheckedRecordAppend::State::Appended);
			}
			CHECK(r->readbacks[0].source.issued == 16);
			CHECK(r->readbacks[0].source.completed == 0);
			CHECK_FALSE(r->readbacks[0].source.delivered);
		}
		uint8_t bytes[16] = {};
		uint64_t written = 99;
		CHECK(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == ERR_BUSY);
		CHECK(written == 99);
		e.driver.complete = true;
		e.driver.fail_delivery_map = mode == 4;
		execute_transfers(e, *r, false, producer);
		if (mode == 5) {
			for (uint32_t i = 0; i < 256 && !e.driver.delivery_maps; ++i) {
				turn(e, *r, ProductionControlKind::OBSERVE, budget(8, 1));
			}
			CHECK(e.driver.delivery_maps == 1);
			CHECK(e.driver.delivery_unmaps == 0);
			CHECK(r->readbacks[0].source.returned < r->readbacks[0].source.issued);
			CHECK(e.registry->production_cancel(ref).native_error == OK);
			CHECK(e.registry->production_release(ref).native_error == ERR_BUSY);
			CHECK(r->resources[0].use_count == 1);
			CHECK(e.original_storage->physical.production_uses == &r->resources[resource_count - 1].uses[0]);
		}
		turn(e, *producer, ProductionControlKind::OBSERVE, budget());
		for (uint32_t i = 0; i < 256 && (r->readbacks[0].source.returned != r->readbacks[0].source.issued || (mixed && r->readbacks[1].source.returned != r->readbacks[1].source.issued)); ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, budget());
		}
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		if (!rejected_prefix && mode != 4) {
			REQUIRE(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == OK);
			CHECK(written == 16);
			for (uint32_t i = 0; i < 16; ++i) {
				CHECK(bytes[i] == uint8_t(i ^ 0x5a));
			}
			CHECK(e.driver.delivery_maps == (mixed ? 2u : 1u));
			CHECK(e.driver.delivery_unmaps == (mixed ? 2u : 1u));
			if (mixed) {
				REQUIRE(e.registry->production_copy(ref, r->revision, 1, bytes, sizeof(bytes), &written).native_error == OK);
				CHECK(written == 8);
				for (uint32_t i = 0; i < 8; ++i) {
					CHECK(bytes[i] == uint8_t(201 + i));
				}
			}
		} else if (mode == 4) {
			CHECK(r->readbacks[0].source.error == ERR_CANT_ACQUIRE_RESOURCE);
			CHECK(r->readbacks[0].source.returned == 16);
			CHECK(r->readbacks[0].source.completed == 0);
			CHECK_FALSE(r->readbacks[0].source.delivered);
			CHECK(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error != OK);
		}
		// Return one borrower first. The other original edge still pins the
		// source; neither read completion nor cancellation returns its owner.
		CHECK(e.registry->production_cancel({ Enrollment::INSTANCE, producer->serial }).native_error == OK);
		REQUIRE(finish_owned_maintenance(e, *producer, budget()));
		REQUIRE(turn(e, *producer, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(producer->serial == 0);
		CHECK(r->resources[0].use_count == 1);
		CHECK(e.original_storage->physical.production_uses == &r->resources[resource_count - 1].uses[0]);
		CHECK(e.original_storage->physical.production_uses != nullptr);
		CHECK(e.driver.buffer_frees == 0);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		REQUIRE(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.original_storage->physical.production_uses == nullptr);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		auto &physical = e.original_storage->physical;
		const RID rid = RID::from_uint64(e.original_storage->backing.rid);
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(rid);
		e.rd.qualification_retiring = false;
		CHECK_FALSE(e.rd.storage_buffer_owner.owns(rid));
		CHECK(physical.enqueued);
		CHECK(physical.free_event == 0);
		e.rd._free_pending_resources(0);
		CHECK(e.driver.buffer_frees == 1);
		CHECK(physical.free_event != 0);
	}

	static void incoming_worker_roundtrip(uint32_t mode) {
		using namespace GDExtensionDeviceExit;
		// 0: pending recording; 1: submitted before append plus retain Busy;
		// 2: completed/recycled before append; 3: worker -> prior UPLOAD -> read.
		Enrollment e;
		e.driver.worker_mode = true;
		e.rd.transfer_worker_pool_max_size = 1;
		e.rd.transfer_worker_pool.resize(1);
		e.rd.transfer_worker_operation_used_by_draw.resize(1);
		e.rd.transfer_queue = RDD::CommandQueueID(2);
		e.rd.transfer_queue_family = RDD::CommandQueueFamilyID(2);
		e.rd.frames[0].transfer_worker_semaphores.push_back(e.driver.semaphore_create());
		auto *producer = borrowed_readback_setup(e, true);
		REQUIRE(producer != nullptr);
		e.rd.frames[0].primary_command_buffer_begun = e.driver.command_buffer_begin(e.rd.frames[0].command_buffer);
		const auto attachment_ref = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, producer->attachment->serial };
		const RID rid = RID::from_uint64(e.original_storage->backing.rid);
		auto *buffer = e.rd.storage_buffer_owner.get_or_null(rid);
		REQUIRE(buffer != nullptr);
		uint8_t initial[16];
		for (uint32_t i = 0; i < 16; ++i) {
			initial[i] = uint8_t(i ^ 0xa7);
		}
		REQUIRE(e.rd._buffer_initialize(buffer, Span<uint8_t>(initial, 16), 1) == OK);
		REQUIRE(e.rd.transfer_worker_pool_size == 1);
		auto *worker = e.rd.transfer_worker_pool[0];
		REQUIRE(worker != nullptr);
		const uint64_t operation = buffer->transfer_worker_operation;
		CHECK(buffer->transfer_worker_index == 0);
		CHECK(operation == 1);
		CHECK(worker->operations_counter == operation);
		CHECK(worker->operations_processed == 0);
		CHECK(worker->operations_submitted == 0);
		CHECK(worker->recording);
		CHECK_FALSE(worker->submitted);
		CHECK(worker->staging_buffer_size_in_use == 16);
		CHECK(e.driver.worker_serial == 0);
		auto poll_worker = [&]() {
			MutexLock lock(worker->thread_mutex);
			return e.rd._wait_for_transfer_worker(worker, false);
		};
		if (mode == 1 || mode == 2) {
			MutexLock lock(worker->thread_mutex);
			e.rd._end_transfer_worker(worker);
			e.rd._submit_transfer_worker(worker, e.rd.frames[0].transfer_worker_semaphores[0]);
			REQUIRE(worker->submitted);
			CHECK(worker->operations_submitted == operation);
			CHECK(worker->last_submission.calls.size() == 1);
			CHECK(worker->submission_serial == e.driver.worker_serial + 2000);
		}
		if (mode == 1) {
			CHECK_FALSE(poll_worker());
			CHECK(worker->operations_processed == 0);
			CHECK(worker->submitted);
			CHECK(e.driver.worker_recycles == 0);
			CHECK(e.driver.worker_resets == 0);
		}
		if (mode == 2) {
			e.driver.source_ready[e.driver.worker_serial] = true;
			REQUIRE(poll_worker());
			CHECK(worker->operations_processed == operation);
			CHECK_FALSE(worker->submitted);
			CHECK(worker->submission_serial == 0);
			CHECK(worker->staging_buffer_size_in_use == 0);
			CHECK(e.driver.worker_recycles == 1);
			CHECK(e.driver.worker_resets == 1);
			CHECK(e.driver.retains[e.driver.worker_serial] == 0);
		}

		auto c = capacity(4);
		c.native_memberships *= 2;
		c.readbacks = 1;
		c.retained_payload_bytes = c.readback_bytes = 16;
		const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 254, 1, 10, 11 };
		const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 };
		const GDExtensionProductionReadbackSourceV1 row{ 0, GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1, 0, 0, 16, {}, { 1, e.original_storage->backing.backing_serial } };
		Request *r = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*producer->attachment, op, c, { nullptr, &action }, 1, &producer->resources[0].declaration, 1, &use, 1, { nullptr, nullptr, 0, &row }, 1, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(e.registry->production_import_storage(ref, 0, &e.original_storage->backing).native_error == OK);
		REQUIRE(e.registry->production_prepare_readback_v2(ref, 0, 0).native_error == OK);
		if (mode == 3) {
			REQUIRE(effect(e, *producer, 0).state == CheckedRecordAppend::State::Appended);
			CHECK(producer->resources[0].transfer_pending);
			CHECK(producer->resources[0].transfer_operation == operation);
			CHECK(buffer->transfer_worker_index == 0);
		}
		REQUIRE(effect(e, *r, 0).state == CheckedRecordAppend::State::Appended);
		CHECK(buffer->transfer_worker_index == (mode == 2 ? -1 : 0));
		CHECK(r->readbacks[0].source.issued == 16);
		CHECK(r->readbacks[0].source.completed == 0);
		CHECK_FALSE(r->readbacks[0].source.delivered);
		CHECK(r->resources[0].transfer_pending == (mode != 2));
		if (mode != 2) {
			CHECK(r->resources[0].transfer_worker == worker->index);
			CHECK(r->resources[0].transfer_operation == operation);
			CHECK(r->resources[0].transfer_action_ordinal == 0);
		}
		e.driver.worker_retain_required = mode != 2;
		if (mode == 1) {
			// The graph appeared after the original submit. Join its exact earlier
			// report now; even a complete fence cannot bypass failed retention.
			e.driver.retain_error = ERR_BUSY;
			CHECK(e.rd._submit_transfer_workers(e.rd.frames[0].command_buffer, true) == ERR_BUSY);
			REQUIRE(r->native_count == 1);
			CHECK_FALSE(r->natives[0].member.retained);
			CHECK(worker->production_observer_pending);
			e.driver.source_ready[e.driver.worker_serial] = true;
			CHECK_FALSE(poll_worker());
			CHECK(worker->submitted);
			CHECK(worker->operations_processed == 0);
			CHECK(worker->staging_buffer_size_in_use == 16);
			CHECK(e.driver.worker_recycles == 0);
			CHECK(e.driver.worker_resets == 0);
			e.driver.retain_error = OK;
			REQUIRE(poll_worker());
			CHECK(r->natives[0].member.retained);
			CHECK_FALSE(worker->production_observer_pending);
			CHECK(worker->operations_processed == operation);
		}
		execute_transfers(e, *r, false, mode == 3 ? producer : nullptr);
		REQUIRE(e.driver.worker_serial != 0);
		REQUIRE(e.driver.main_serial != 0);
		CHECK(e.driver.worker_serial != e.driver.main_serial);
		CHECK(e.driver.queue_issues == 2);
		CHECK(e.driver.main_worker_waits == 1);
		CHECK(e.rd.frames[0].semaphores_to_wait_on.is_empty());
		auto worker_members = [&](Request &request) {
			uint32_t count = 0;
			for (uint32_t n = 0; n < request.native_count; ++n) {
				const auto &native = request.natives[n];
				if (native.member.source.serial == e.driver.worker_serial) {
					++count;
					CHECK(native.member.source.queue_index == 2);
					CHECK(native.member.source.completion_serial == e.driver.worker_serial + 1000);
					CHECK(native.member.retained);
					CHECK(native.fact.membership_parent == 0);
				}
			}
			return count;
		};
		CHECK(worker_members(*r) == (mode == 2 ? 0u : 1u));
		auto retained_worker_owners = [&](Request &request) {
			uint32_t owners = 0;
			for (uint32_t n = 0; n < request.native_count; ++n) {
				const auto &cell = request.natives[n];
				if (cell.member.source.serial == e.driver.worker_serial && cell.owner_index == n && cell.member.retained && !cell.member.owner_released) {
					++owners;
				}
			}
			return owners;
		};
		const uint32_t producer_worker_owners = retained_worker_owners(*producer);
		const uint32_t reader_worker_owners = retained_worker_owners(*r);
		CHECK(producer_worker_owners == (mode == 3 ? 1u : 0u));
		CHECK(reader_worker_owners == (mode == 2 ? 0u : 1u));
		// Both mode3 requests independently retain the exact same worker issue.
		CHECK(e.driver.retains[e.driver.worker_serial] >= producer_worker_owners + reader_worker_owners);
		CHECK(e.driver.releases[e.driver.worker_serial] == 0);
		CHECK(worker_members(*producer) == (mode == 3 ? 1u : 0u));
		CHECK_FALSE(r->resources[0].transfer_pending);
		CHECK_FALSE(producer->resources[0].transfer_pending);
		if (mode == 3) {
			uint32_t path_length = 0;
			CHECK(e.rd.draw_graph.has_dependency_path(producer->graphs[0].handle, r->graphs[0].handle, path_length));
			CHECK(path_length > 0);
			CHECK(producer->graphs[0].fact.command_slot == r->graphs[0].fact.command_slot);
		}
		uint8_t bytes[16] = {};
		uint64_t written = 77;
		auto blocked_copy = [&]() {
			CHECK(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == ERR_BUSY);
			CHECK(written == 77);
			CHECK_FALSE(r->readbacks[0].source.delivered);
			CHECK(r->readbacks[0].source.completed == 0);
			CHECK(e.original_storage->physical.production_uses != nullptr);
			CHECK(e.original_storage->physical.free_event == 0);
			CHECK(e.driver.buffer_frees == 0);
		};
		auto observe = [&](Request &request) {
			const auto limit = budget(8, 4);
			const auto before = e.driver.receipt_calls();
			const auto output = turn(e, request, ProductionControlKind::OBSERVE, limit);
			CHECK(output.service_usage.receipts <= limit.receipts);
			CHECK(output.service_usage.passes <= limit.passes);
			CHECK(e.driver.receipt_calls() - before <= limit.receipts);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
		};
		observe(*r);
		blocked_copy();
		if (mode == 0 || mode == 3) {
			CHECK_FALSE(poll_worker());
			CHECK(worker->operations_processed == 0);
			CHECK(worker->submitted);
			CHECK(e.driver.worker_recycles == 0);
			CHECK(e.driver.worker_resets == 0);
			e.rd._free_transfer_workers();
			CHECK(e.rd.transfer_worker_pool_size == 1);
			CHECK(e.driver.worker_pool_frees == 0);
			CHECK(e.driver.worker_fence_frees == 0);
			e.driver.source_ready[e.driver.worker_serial] = true;
			REQUIRE(poll_worker());
			CHECK(worker->operations_processed == operation);
			CHECK_FALSE(worker->submitted);
			CHECK(worker->submission_serial == 0);
			CHECK(worker->staging_buffer_size_in_use == 0);
		}
		CHECK(e.driver.worker_recycles == 1);
		CHECK(e.driver.worker_resets == 1);
		CHECK(worker->operations_processed == operation);
		// Processing alone does not mutate the resource marker. Its next
		// authenticated dependency check may now clear it without a new issue.
		CHECK(buffer->transfer_worker_index == (mode == 2 ? -1 : 0));
		REQUIRE(e.rd._check_transfer_worker_buffer(buffer) == OK);
		CHECK(buffer->transfer_worker_index == -1);
		CHECK(e.driver.worker_recycles == 1);
		observe(*r); // Worker completion alone cannot publish an unfinished copy.
		blocked_copy();
		e.driver.source_ready[e.driver.main_serial] = true;
		for (uint32_t i = 0; i < 128 && !r->readbacks[0].source.delivered; ++i) {
			observe(*r);
		}
		REQUIRE(r->readbacks[0].source.delivered);
		REQUIRE(finish_owned_maintenance(e, *r, budget(8, 4)));
		REQUIRE(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == OK);
		CHECK(written == 16);
		CHECK(r->readbacks[0].source.completed == 16);
		CHECK(r->readbacks[0].source.returned == 16);
		for (uint32_t i = 0; i < 16; ++i) {
			CHECK(bytes[i] == uint8_t(i ^ (mode == 3 ? 0x5a : 0xa7)));
		}
		if (mode == 3) {
			observe(*producer);
		}
		uint32_t returned_worker_owners = 0;
		for (auto *request : { producer, r }) {
			const uint32_t original_worker_owners = request == producer ? producer_worker_owners : reader_worker_owners;
			const GDExtensionProductionRequestRefV1 request_ref{ Enrollment::INSTANCE, request->serial };
			REQUIRE(e.registry->production_cancel(request_ref).native_error == OK);
			REQUIRE(finish_owned_maintenance(e, *request, budget(8, 4)));
			REQUIRE(turn(e, *request, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
			CHECK(request->serial == 0);
			returned_worker_owners += original_worker_owners;
			CHECK(e.driver.releases[e.driver.worker_serial] == returned_worker_owners);
		}
		CHECK(e.original_storage->physical.production_uses == nullptr);
		REQUIRE(e.registry->production_close(attachment_ref).native_error == OK);
		CHECK(e.driver.releases[e.driver.worker_serial] == (mode == 2 ? 0u : mode == 3 ? 2u : 1u));
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(rid);
		e.rd.qualification_retiring = false;
		CHECK(physical.enqueued);
		CHECK(physical.free_event == 0);
		e.rd._free_pending_resources(0);
		CHECK(physical.free_event != 0);
		CHECK(e.driver.buffer_frees == 1);
		e.rd._free_transfer_workers();
		CHECK(e.rd.transfer_worker_pool_size == 0);
		CHECK(e.driver.worker_pool_frees == 1);
		CHECK(e.driver.worker_fence_frees == 1);
		CHECK(e.driver.buffer_frees == 2);
	}

	static void incoming_reflected_read(uint32_t mode) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *producer = borrowed_readback_setup(e, true);
		REQUIRE(producer != nullptr);
		auto &attachment = *producer->attachment;
		const GDExtensionProductionAttachmentRefV1 attachment_ref{ Enrollment::INSTANCE, attachment.serial };
		const bool alias = mode == 2;
		const bool readonly = mode == 1;
		auto c = capacity(4);
		c.resource_uses = 6;
		c.retained_payload_bytes = c.readback_bytes = 16;
		c.readbacks = 1;
		const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 253, 1, 10, 11 };
		const uint32_t dispatch_uses = alias ? 2 : 1;
		const GDExtensionProductionActionV2 actions[] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, dispatch_uses, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 1, 10, 1, 0, GDEXTENSION_PRODUCTION_READBACK_V1, 0, dispatch_uses, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			producer->resources[0].declaration,
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			producer->resources[0].declaration
		};
		// The only declared dispatch accesses are READ and disjoint from the
		// incoming range. Writable reflection still protects the full SSBO.
		GDExtensionProductionUseV1 uses[] = {
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 80, 4, 0 },
			{ 3, GDEXTENSION_PRODUCTION_READ_V1, 80, 4, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 }
		};
		if (!alias) {
			uses[1] = uses[2];
		}
		const GDExtensionProductionReadbackSourceV1 row{ 1, GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1, 1, 0, 16, {}, { 1, e.original_storage->backing.backing_serial } };
		Request *r = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(attachment, op, c, { nullptr, actions }, 2, resources, alias ? 4 : 3, uses, dispatch_uses + 1, { nullptr, nullptr, 0, &row }, 1, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(e.registry->production_import_storage(ref, 1, &e.original_storage->backing).native_error == OK);
		if (alias) {
			REQUIRE(e.registry->production_import_storage(ref, 3, &e.original_storage->backing).native_error == OK);
		}
		REQUIRE(import_original_unit(e, *r, !readonly && !alias, alias, true));
		const GDExtensionProductionSetBindingV1 bindings[] = {
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 },
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 1, 0, 3, 0, 128 }
		};
		e.driver.expected_set_bindings = alias ? 2 : 1;
		REQUIRE(create_set(e, ref, 2, 0, 0, bindings, e.driver.expected_set_bindings).native_error == OK);
		const bool persistent_set = r->resources[2].cached_persistent_set && !attachment.persistent_sets.is_empty() && attachment.persistent_sets[0] != nullptr;
		e.driver.set_cell = persistent_set ? &attachment.persistent_sets[0]->physical : &r->resources[2].owned_physical;
		const uint32_t push[] = { 7, 11, 13, 17 };
		const GDExtensionProductionDispatchV2 dispatch{ production_dto_abi<GDExtensionProductionDispatchV2>(), 0, 10, 0, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
		REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
		REQUIRE(e.registry->production_prepare_readback_v2(ref, 1, 0).native_error == OK);
		REQUIRE(effect(e, *producer, 0).state == CheckedRecordAppend::State::Appended);
		const auto before_prepares = e.driver.set_prepares;
		const auto first = effect(e, *r, 0);
		if (readonly) {
			REQUIRE(first.state == CheckedRecordAppend::State::Appended);
			REQUIRE(first.error == OK);
		} else {
			CHECK(first.state == CheckedRecordAppend::State::Rejected);
			CHECK(first.error == ERR_INVALID_DATA);
			CHECK_FALSE(r->frozen);
			CHECK(r->graph_count == 0);
			CHECK(r->next_action == 0);
			CHECK(e.driver.set_prepares == before_prepares);
			CHECK_FALSE(r->append_effect_entered);
		}
		if (readonly) {
			REQUIRE(effect(e, *r, 1).state == CheckedRecordAppend::State::Appended);
		}
		e.driver.complete = true;
		execute_transfers(e, *r, true, producer);
		turn(e, *producer, ProductionControlKind::OBSERVE, budget());
		turn(e, *r, ProductionControlKind::OBSERVE, budget());
		if (readonly) {
			uint8_t bytes[16] = {};
			uint64_t written = 0;
			REQUIRE(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == OK);
			CHECK(written == 16);
			for (uint32_t i = 0; i < 16; ++i) {
				CHECK(bytes[i] == uint8_t(i ^ 0x5a));
			}
		}
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		if (!persistent_set) {
			for (uint32_t i = 0; i < 128 && !r->resources[2].returned; ++i) {
				turn(e, *r, ProductionControlKind::OBSERVE, budget());
			}
			REQUIRE(r->resources[2].returned);
			CHECK(e.driver.set_frees == 1);
		}
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		REQUIRE(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.original_storage->physical.production_uses == &producer->resources[0].uses[0]);
		CHECK(e.registry->production_cancel({ Enrollment::INSTANCE, producer->serial }).native_error == OK);
		REQUIRE(finish_owned_maintenance(e, *producer, budget()));
		REQUIRE(turn(e, *producer, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(producer->serial == 0);
		CHECK(e.original_storage->physical.production_uses == nullptr);
		if (persistent_set) {
			CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
			e.rd._free_pending_resources(0);
			CHECK(e.driver.set_frees == 1);
			CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		} else {
			CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		}
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(e.original_storage->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
		CHECK(e.driver.buffer_frees == 1);
		CHECK(physical.free_event != 0);
	}

	static void mapped_readback_keeps_request_alive() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = borrowed_readback_setup(e);
		if (!r) {
			return;
		}
		for (uint32_t i = 0; i < 2; ++i) {
			CHECK(effect(e, *r, i).state == CheckedRecordAppend::State::Appended);
		}
		e.driver.complete = true;
		execute_transfers(e, *r, false);
		const auto small = budget(8, 1);
		for (uint32_t i = 0; i < 256 && !e.driver.delivery_maps; ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, small);
		}
		CHECK(e.driver.delivery_maps == 1);
		CHECK(e.driver.delivery_unmaps == 0);
		CHECK(e.driver.delivery_map_id != 0);
		CHECK_FALSE(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.returned < r->readbacks[0].source.issued);
		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const auto serial = r->serial;
		const auto epoch = e.rd.frames[0].qualification_epoch;
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		GDExtensionProductionReportV1 report = {};
		auto query = small;
		query.receipts = 0;
		const auto next_control = r->next_control_serial;
		const auto native_calls = e.driver.receipt_calls();
		CHECK(e.registry->production_observe(ref, &query, &report).native_error == OK);
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(r->next_control_serial == next_control);
		CHECK(e.driver.receipt_calls() == native_calls);
		CHECK(report.physical_returns_pending == 0);
		CHECK(report.known_obligations == report.complete_obligations);
		CHECK(report.possible_obligations > 0);
		const auto premature_release = e.registry->production_release(ref).native_error;
		CHECK(premature_release == ERR_BUSY);
		if (premature_release != ERR_BUSY) {
			return;
		}
		CHECK(r->serial == serial);
		CHECK(e.rd.frames[0].qualification_epoch == epoch);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests[0].capture == &r->readbacks[0].capture);
		auto expired = small;
		expired.deadline_micros = 1;
		turn(e, *r, ProductionControlKind::OBSERVE, expired);
		CHECK(e.driver.delivery_unmaps == 0);
		CHECK(r->serial == serial);
		CHECK(e.rd.frames[0].qualification_epoch == epoch);
		for (uint32_t i = 0; i < 256 && (!r->readbacks[0].source.delivered || !e.rd.frames[0].download_buffer_get_data_requests.is_empty()); ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, small);
		}
		CHECK(e.driver.delivery_maps == 1);
		CHECK(e.driver.delivery_unmaps == 1);
		CHECK(e.driver.delivery_map_id == 0);
		CHECK(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.returned == r->readbacks[0].source.issued);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.is_empty());
		uint8_t bytes[16] = {};
		uint64_t written = 0;
		CHECK(e.registry->production_copy(ref, r->revision, 0, bytes, sizeof(bytes), &written).native_error == OK);
		CHECK(written == 16);
		for (uint32_t i = 0; i < 16; ++i) {
			CHECK(bytes[i] == uint8_t(i ^ 0x5a));
		}
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		if (!finish_owned_maintenance(e, *r, budget(e.driver.sources_per_issue, 1))) {
			return;
		}
		const auto returned = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(returned.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment).native_error == OK);
		// The request borrowed this original storage; its release drops the
		// production use but cannot retire the separate source owner's backing.
		auto &physical = e.original_storage->physical;
		CHECK(physical.production_uses == nullptr);
		CHECK(physical.dependent_children.load() == 0);
		CHECK_FALSE(physical.use_closed.load());
		CHECK_FALSE(physical.enqueued);
		CHECK(physical.free_event == 0);
		const RID original_rid = RID::from_uint64(e.original_storage->backing.rid);
		const auto frees_before = e.driver.buffer_frees;
		physical.use_closed.store(true);
		const bool previous_retiring = e.rd.qualification_retiring;
		CHECK_FALSE(previous_retiring);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(original_rid);
		e.rd.qualification_retiring = previous_retiring;
		CHECK_FALSE(e.rd.storage_buffer_owner.owns(original_rid));
		CHECK(physical.enqueued);
		CHECK(physical.free_event == 0);
		e.rd._free_pending_resources(0);
		CHECK(e.driver.buffer_frees == frees_before + 1);
		CHECK(physical.free_event != 0);
	}
	static inline uint32_t ordinary_frame_callbacks = 0;
	static void ordinary_frame_drawn() { ++ordinary_frame_callbacks; }
	static void ordinary_frame_owner_deferral(bool draw) {
		using namespace GDExtensionDeviceExit;
		auto *server = draw ? Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton()) : nullptr;
		auto *compositor = draw ? RendererCompositor::get_singleton() : nullptr;
		if (draw) {
			REQUIRE(server != nullptr);
			REQUIRE(compositor != nullptr);
			REQUIRE(Thread::is_main_thread());
			REQUIRE(server->is_on_render_thread());
			REQUIRE(RenderingDevice::get_singleton() == nullptr);
			server->sync();
		}
		Enrollment e;
		auto *r = owned_setup(e, false);
		REQUIRE(r != nullptr);
		e.driver.complete = true;
		const GDExtensionProductionAttachmentRefV1 attachment{ Enrollment::INSTANCE, r->attachment->serial };
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		GDExtensionStorageRetireRefV1 retires[2]{};
		GDExtensionStorageRetireReportV1 reports[2]{};
		for (uint32_t slot = 0; slot < 2; ++slot) {
			REQUIRE(retire_storage(e, *r, slot, retires[slot]));
		}
		const auto small = budget(1, 1);
		for (uint32_t attempt = 0; attempt < 32 && !e.rd.native_disposal_active; ++attempt) {
			const auto error = observe_storage(e, ref, retires[0], &small, &reports[0]).native_error;
			REQUIRE((error == OK || error == ERR_BUSY));
		}
		// Original accepted service and real storage retirement create this cursor.
		// No owner/active/frozen/completion fields are installed by the test.
		REQUIRE(e.rd.native_disposal_active);
		REQUIRE(e.rd.native_disposal_continuation.production_owner == r);
		REQUIRE(r->maintenance_serial == e.rd.native_disposal_continuation.serial);
		CHECK(e.rd.frame_wait_error == OK);
		CHECK(e.rd.frame_issue_error == OK);
		CHECK(e.rd._recording_error() == ERR_BUSY);
		const auto before_cursor = e.rd.native_disposal_continuation;
		const auto before_receipts = e.driver.receipt_calls();
		const auto before_preparations = e.driver.preparations;
		const auto before_graph = e.rd.draw_graph.qualification_command_count();
		const auto before_frames = e.rd.frames_drawn;
		const uint64_t before_compositor = compositor ? compositor->get_frame_number() : 0;
		if (draw) {
			ordinary_frame_callbacks = 0;
			server->request_frame_drawn_callback(callable_mp_static(&Access::ordinary_frame_drawn));
			server->draw(false, 0.01); // Original ordinary scheduler, independent of owner service.
			CHECK(ordinary_frame_callbacks == 0);
			CHECK(compositor->get_frame_number() == before_compositor);
			CHECK(server->has_changed()); // A deferred draw retains the demand to retry.
		} else {
			// Surface metadata only. The controlled driver returns no drawable;
			// even attempting acquire while the owner holds the frame is forbidden.
			constexpr DisplayServerEnums::WindowID screen = 77;
			e.rd.screen_swap_chains[screen] = RDD::SwapChainID(120);
			e.rd.screen_framebuffers[screen] = RDD::FramebufferID(121);
			CHECK(e.rd.frames[e.rd.frame].swap_chains_to_present.is_empty());
			CHECK(e.rd.screen_prepare_for_drawing(screen) == ERR_BUSY);
			CHECK(e.driver.screen_acquires == 0);
			CHECK(e.rd.screen_framebuffers.has(screen));
			if (e.rd.screen_framebuffers.has(screen)) {
				CHECK(e.rd.screen_framebuffers[screen] == RDD::FramebufferID(121));
			}
			CHECK(e.rd.frames[e.rd.frame].swap_chains_to_present.is_empty());
			e.rd.screen_framebuffers.erase(screen);
			e.rd.screen_swap_chains.erase(screen);
		}
		const auto &after_cursor = e.rd.native_disposal_continuation;
		CHECK(e.rd.native_disposal_active);
		CHECK(after_cursor.production_owner == before_cursor.production_owner);
		CHECK(after_cursor.serial == before_cursor.serial);
		CHECK(after_cursor.phase == before_cursor.phase);
		CHECK(after_cursor.target == before_cursor.target);
		CHECK(after_cursor.end_phase == before_cursor.end_phase);
		CHECK(after_cursor.begin_phase == before_cursor.begin_phase);
		CHECK(after_cursor.slot == before_cursor.slot);
		CHECK(e.driver.receipt_calls() == before_receipts);
		CHECK(e.driver.preparations == before_preparations);
		CHECK(e.rd.draw_graph.qualification_command_count() == before_graph);
		CHECK(e.rd.frames_drawn == before_frames);
		// The same original owner, with the same one-receipt/one-pass bound,
		// services the real return. Ordinary draw never adopts or drains it.
		for (uint32_t slot = 0; slot < 2; ++slot) {
			for (uint32_t attempt = 0; attempt < 128 && !reports[slot].state; ++attempt) {
				const auto error = observe_storage(e, ref, retires[slot], &small, &reports[slot]).native_error;
				REQUIRE((error == OK || error == ERR_BUSY));
			}
			REQUIRE(reports[slot].state == 1);
			CHECK(reports[slot].event_seq != 0);
			REQUIRE(e.registry->production_storage_release(ref, retires[slot]).native_error == OK);
		}
		REQUIRE(finish_owned_maintenance(e, *r, small));
		CHECK(e.rd._recording_error() == OK);
		CHECK(e.driver.buffer_frees == 2);
		if (draw) {
			server->draw(false, 0.01);
			CHECK(ordinary_frame_callbacks == 1);
			CHECK(compositor->get_frame_number() == before_compositor + 1);
			CHECK_FALSE(server->has_changed());
		}
		REQUIRE(e.registry->production_release(ref).native_error == OK);
		CHECK(r->serial == 0);
		REQUIRE(e.registry->production_close(attachment).native_error == OK);
		CHECK(e.rd.production_entity_holds == nullptr);
	}
	static void height_retain_before_original_frame_advance() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.height_observation = true;
		auto *request = owned_setup(e, false);
		REQUIRE(request != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, request->serial };
		const uint8_t payload[16] = {};
		const GDExtensionProductionUploadV1 upload{ production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, sizeof(payload) };
		REQUIRE(e.registry->production_prepare_upload(ref, &upload, payload, sizeof(payload)).native_error == OK);
		// owned_setup creates the original owned storage only. This upload
		// also needs its finite actual RD staging block, as transfers_setup does.
		e.rd.upload_staging_buffers.block_size = 128;
		e.rd.upload_staging_buffers.max_size = 128;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		REQUIRE(e.rd._insert_staging_block(e.rd.upload_staging_buffers) == OK);
		Ref<NativeHeightRange> range;
		REQUIRE(e.rd._native_height_range_prepare(range) == OK);
		const auto appended = effect(e, *request, 0);
		REQUIRE(appended.state == CheckedRecordAppend::State::Appended);
		REQUIRE(request->graph_count > 0);
		close_segment(e, *request);
		// Mechanical observer fixture takes only the actual original accepted
		// graph handles. Native sources come exclusively from the issue hook.
		range->recording_started = range->recording_closed = true;
		range->has_recorded_effect = true;
		for (uint32_t i = 0; i < request->graph_count; ++i) {
			auto &member = range->graphs[range->graph_count++];
			member.handle = request->graphs[i].handle;
			member.frame = request->graphs[i].device_frame;
			member.frame_epoch = request->graphs[i].device_epoch;
		}
		e.rd.frames[e.rd.frame].native_height_ranges.push_back(range);
		const auto epoch = e.rd.frames[e.rd.frame].qualification_epoch;
		e.driver.height_retain_pause = true;
		e.driver.complete = true;
		// Stop on the actual first issued source. A one-pass issue turn cannot
		// also recycle the Frame, making the old-source RED cleanup independent
		// of the failed phase-2 retain barrier.
		for (uint32_t i = 0; i < 128 && !range->source_count; ++i) {
			const auto output = turn(e, *request, ProductionControlKind::OBSERVE, budget(1, 1));
			CHECK(output.service_usage.receipts <= 1);
			CHECK(output.service_usage.passes <= 1);
		}
		REQUIRE(range->source_count > 0);
		CHECK_FALSE(range->sources[0].retained);
		CHECK(e.rd.native_disposal_active);
		CHECK(e.rd.native_disposal_continuation.production_owner == request);
		CHECK(e.rd.native_disposal_continuation.phase == 2);
		CHECK(e.rd.native_disposal_continuation.slot == e.rd.native_disposal_continuation.slot_count);
		CHECK(e.rd.frames[e.rd.frame].qualification_epoch == epoch);
		const auto issued = e.driver.queue_issues;
		REQUIRE(issued > 0);
		if (e.rd.native_disposal_continuation.phase == 2) {
			// Fresh accepted budget reaches the actual original retain callback.
			// Its Busy result must preserve the same original Frame/slot/source.
			const auto serial = range->sources[0].original.serial;
			const auto retains = e.driver.retains[serial];
			const auto paused = turn(e, *request, ProductionControlKind::OBSERVE, budget(1, 1));
			CHECK(paused.service_usage.receipts <= 1);
			CHECK(paused.service_usage.passes <= 1);
			CHECK(e.driver.retains[serial] == retains + 1);
			CHECK_FALSE(range->sources[0].retained);
			CHECK(e.rd.native_disposal_continuation.phase == 2);
			CHECK(e.rd.native_disposal_continuation.slot == e.rd.native_disposal_continuation.slot_count);
			CHECK(e.rd.frames[e.rd.frame].qualification_epoch == epoch);
			CHECK(e.driver.queue_issues == issued);
		}
		e.driver.height_retain_pause = false;
		// This existing suite owns a local Registry. Dispatch the original seal
		// and capture hooks using the actual emitted reports for fixture cleanup,
		// as execute_transfers does; never write native serials or completion.
		{
			BudgetDriverGuard guard(e.driver);
			MutexLock lock(e.registry->mutex);
			e.registry->production_seal_graphs_locked(e.rd);
			for (uint32_t slot = 0; slot < e.rd.frames[e.rd.frame].chain_reports.size(); ++slot) {
				const auto &report = e.rd.frames[e.rd.frame].chain_reports[slot];
				e.registry->production_capture_issue_locked(e.rd, slot, NativeSourceIssueView{ report }, report.error);
			}
		}
		// An old-source RED has already failed the exact phase assertion above.
		// Repair only that fixture's pending retain through the original helper
		// before allowing its already-advanced cursor to recycle the Frame.
		// This repair is deliberately outside service-usage evidence.
		if (e.rd.native_disposal_continuation.phase != 2) {
			BudgetDriverGuard guard(e.driver);
			REQUIRE(e.rd._native_height_retain_frame_sources(e.rd.frame) == OK);
		}
		for (uint32_t i = 0; i < 128 && !range->sources[0].retained; ++i) {
			const auto resumed = turn(e, *request, ProductionControlKind::OBSERVE, budget(1, 1));
			CHECK(resumed.service_usage.receipts <= 1);
			CHECK(resumed.service_usage.passes <= 1);
		}
		CHECK(range->sources[0].retained);
		CHECK(e.driver.queue_issues == issued); // Original attempted slot cannot replay.
		const RenderingDevice::NativeDisposalBudget cleanup{ 128, 128, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage used;
		NativeHeightRange::Snapshot snapshot;
		{
			BudgetDriverGuard guard(e.driver);
			REQUIRE(e.rd._native_height_range_observe(range, cleanup, used, snapshot) == OK);
			REQUIRE(e.rd._native_height_range_return(range, cleanup, used) == OK);
		}
		CHECK(snapshot.native_complete);
		CHECK(used.polls + used.releases <= 128);
		REQUIRE(finish_owned_maintenance(e, *request, budget(1, 1)));
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		REQUIRE(return_owned_storage(e, *request, 0));
		REQUIRE(return_owned_storage(e, *request, 1));
		const GDExtensionProductionAttachmentRefV1 attachment{ Enrollment::INSTANCE, request->attachment->serial };
		const auto released = turn(e, *request, ProductionControlKind::RELEASE, budget());
		CHECK(released.owner_return.native_error == OK);
		CHECK(e.registry->production_close(attachment).native_error == OK);
	}

	static void fresh_buffer_target(bool pending_advance, bool buffer_on_target, bool foreign_readback) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true;
		const auto c = capacity(3);
		const auto attachment = e.accept(c);
		REQUIRE(attachment.handle_serial);
		Request *request = nullptr;
		REQUIRE(e.reserve(attachment, c, 31, request) == OK);
		REQUIRE(request);
		e.rd.frames.resize(2);
		graph_initialize(e);
		e.rd.frames[1].qualification_epoch = 2;
		e.rd.frames[1].fence = RDD::FenceID(95);
		e.rd.frames[1].command_buffer = RDD::CommandBufferID(92);
		e.rd.frames[1].command_pool = RDD::CommandPoolID(96);
		e.rd.next_qualification_frame_epoch = 3;
		const uint32_t target = pending_advance ? 0 : 1;
		e.driver.expected_begin_frame = target;
		const uint32_t queued = buffer_on_target ? target : 1 - target;
		RenderingDevice::Buffer buffer;
		buffer.driver_id = e.driver.buffer_create(16, {}, RDD::MEMORY_ALLOCATION_TYPE_GPU, 0);
		buffer.size = 16;
		REQUIRE(buffer.driver_id.id);
		const RID rid = e.rd.storage_buffer_owner.make_rid(buffer);
		e.rd.frame = queued;
		e.rd.free_rid(rid);
		e.rd.frame = 0;
		REQUIRE(e.rd.frames[queued].buffers_to_dispose_of.size() == 1);
		e.rd.qualification_frame_advance_pending = pending_advance;
		if (foreign_readback) {
			// This unrelated readback has no Registry capture and remains global.
			e.rd.frames[1 - target].download_buffer_get_data_requests.push_back(RenderingDevice::BufferGetDataRequest{});
		}
		const auto allowance = budget(1, 1);
		ProductionServiceUsage used = {};
		const auto frees_before = e.driver.buffer_frees;
		const auto cursor_before = e.rd.next_native_disposal_serial;
		{
			MutexLock registry_lock(e.registry->mutex);
			MutexLock device_lock(e.rd._thread_safe_);
			BudgetDriverGuard guard(e.driver);
			CHECK(e.registry->production_maintenance_locked(*request, e.rd, allowance, used) == ERR_BUSY);
		}
		if (foreign_readback) {
			e.rd.frames[1 - target].download_buffer_get_data_requests.clear();
		}
		if (buffer_on_target || foreign_readback) {
			CHECK_FALSE(e.rd.native_disposal_active);
			CHECK(e.rd.next_native_disposal_serial == cursor_before);
			CHECK(request->maintenance_serial == 0);
			CHECK((used.controls == 0 && used.receipts == 0 && used.passes == 0));
			e.rd.qualification_frame_advance_pending = false; // End this fixture's unstarted advance.
		} else {
			CHECK(e.rd.native_disposal_active);
			CHECK(e.rd.native_disposal_continuation.target == target);
			CHECK(request->maintenance_serial == cursor_before);
			CHECK((used.passes <= 1 && used.receipts <= 1));
			CHECK(finish_owned_maintenance(e, *request, allowance));
		}
		CHECK(e.driver.buffer_frees == frees_before);
		CHECK(e.rd.frames[queued].buffers_to_dispose_of.size() == 1);
		// Dispose the test-owned buffer through the ordinary owner after the check.
		e.rd._free_pending_resources(queued);
		CHECK(e.driver.buffer_frees == frees_before + 1);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, request->serial }).native_error == OK);
		CHECK(e.registry->production_close(attachment).native_error == OK);
	}
	static void maintenance_cursor_owner() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true; // Original CPU driver polls report actual issued calls ready.
		auto c = capacity(6); // Two independent three-row reservation slices.
		c.requests = 2;
		auto attachment = e.accept(c);
		REQUIRE(attachment.handle_serial != 0);
		if (!attachment.handle_serial) {
			return;
		}
		const auto single = capacity(3); // Reserve always requests exactly one cell.
		Request *first = nullptr, *second = nullptr;
		const Error first_reserved = e.reserve(attachment, single, 31, first);
		CHECK(first_reserved == OK);
		if (first_reserved != OK || !first) {
			return;
		}
		const Error second_reserved = e.reserve(attachment, single, 32, second);
		CHECK(second_reserved == OK);
		if (second_reserved != OK || !second) {
			return;
		}
		REQUIRE(first != nullptr);
		REQUIRE(second != nullptr);
		graph_initialize(e);
		auto b = budget(1, 1);
		auto maintenance = [&](Request &request, ProductionServiceUsage &usage) {
			MutexLock registry_lock(e.registry->mutex);
			if (!e.rd._thread_safe_.try_lock()) {
				return ERR_BUSY; // Match original Registry service admission; never wait for RD here.
			}
			struct DeviceUnlock {
				Mutex &mutex;
				~DeviceUnlock() { mutex.unlock(); }
			} device_unlock{ e.rd._thread_safe_ };
			BudgetDriverGuard guard(e.driver);
			return e.registry->production_maintenance_locked(request, e.rd, b, usage);
		};
#ifdef THREADS_ENABLED
		struct Contention {
			RenderingDevice *device;
			Registry *registry;
			Semaphore ready{};
			std::atomic<bool> attempt_finished{ false }, holding{ false };
			bool timeout = false, acquired_registry = false;
			static void run(void *data) {
				auto &probe = *static_cast<Contention *>(data);
				MutexLock device_lock(probe.device->_thread_safe_);
				probe.holding.store(true, std::memory_order_release);
				probe.ready.post();
				// A broken blocking admission must fail boundedly, not hang the
				// runner. The fixed path returns while this actual RD lock is held.
				const uint64_t deadline = OS::get_singleton()->get_ticks_usec() + 5000000;
				while (!probe.attempt_finished.load(std::memory_order_acquire) && OS::get_singleton()->get_ticks_usec() < deadline) {
					OS::get_singleton()->delay_usec(100);
				}
				probe.timeout = !probe.attempt_finished.load(std::memory_order_acquire);
				if (!probe.timeout) {
					// Mirror the original RD -> Registry source-retention order only
					// after the nonblocking admission has returned its Registry lock.
					MutexLock registry_lock(probe.registry->mutex);
					probe.acquired_registry = true;
				}
				probe.holding.store(false, std::memory_order_release);
			}
		} contention{ &e.rd, e.registry };
		Thread contender;
		const auto before_calls = e.driver.receipt_calls();
		const auto before_preparations = e.driver.preparations;
		const uint64_t before_cursor_serial = e.rd.next_native_disposal_serial;
		contender.start(&Contention::run, &contention);
		contention.ready.wait();
		ProductionServiceUsage denied = {};
		const Error blocked = maintenance(*first, denied);
		const bool returned_while_held = contention.holding.load(std::memory_order_acquire);
		contention.attempt_finished.store(true, std::memory_order_release);
		contender.wait_to_finish(); // No Registry or RD lock is held by this caller.
		CHECK(blocked == ERR_BUSY);
		CHECK(returned_while_held);
		CHECK_FALSE(contention.timeout);
		CHECK(contention.acquired_registry);
		CHECK((denied.controls == 0 && denied.receipts == 0 && denied.passes == 0));
		CHECK(e.driver.receipt_calls() == before_calls);
		CHECK(e.driver.preparations == before_preparations);
		CHECK_FALSE(e.rd.native_disposal_active);
		CHECK(e.rd.next_native_disposal_serial == before_cursor_serial);
		CHECK(first->maintenance_serial == 0);
#endif
		RenderingDevice::NativeDisposalBudget native_budget{ 1, 1, b.deadline_micros };
		auto native_step = [&]() {
			RenderingDevice::NativeDisposalUsage usage;
			BudgetDriverGuard guard(e.driver);
			const Error error = e.rd._native_disposal_maintenance_step(native_budget, usage);
			CHECK(usage.passes <= 1);
			CHECK(usage.polls + usage.frees + usage.issues + usage.retains + usage.releases + usage.recycles <= 1);
			return error;
		};
		ProductionServiceUsage used = {};
		CHECK(maintenance(*first, used) == ERR_BUSY);
		REQUIRE(e.rd.native_disposal_active);
		const uint64_t first_serial = e.rd.native_disposal_continuation.serial;
		CHECK(first_serial != 0);
		CHECK(first->maintenance_serial == first_serial);
		CHECK((used.passes <= 1 && used.receipts <= 1));
		const auto children = first->attachment->children;
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, first->serial }).native_error == ERR_BUSY);
		CHECK(first->attachment->children == children);
		used = {};
		const auto calls = e.driver.receipt_calls();
		CHECK(maintenance(*second, used) == ERR_BUSY);
		CHECK(second->maintenance_serial == 0);
		CHECK((used.passes == 0 && used.receipts == 0));
		CHECK(native_step() == ERR_BUSY); // Ordinary/qualification owner zero cannot borrow it.
		CHECK(e.driver.receipt_calls() == calls);
		CHECK(e.rd.native_disposal_continuation.serial == first_serial);
		RenderingDevice::Shader ordinary_shader;
		ordinary_shader.driver_id = RDD::ShaderID(80);
		const RID ordinary_rid = e.rd.shader_owner.make_rid(ordinary_shader);
		e.rd.free_rid(ordinary_rid); // Legal RT logical free between original owner turns.
		CHECK_FALSE(e.rd.shader_owner.owns(ordinary_rid));
		CHECK(e.rd.frames[0].shaders_to_dispose_of.size() == 1);
		const auto ordinary_frees = e.driver.shader_frees;
		// No pending graph, retiring resource or readback triggers these turns.
		// Only the original accepted cursor permits bounded continuation.
		CHECK((first->graph_count == 0 && first->readback_count == 0));
		for (uint32_t i = 0; i < 64 && e.rd.native_disposal_active; ++i) {
			auto output = turn(e, *first, ProductionControlKind::OBSERVE, b);
			CHECK((output.service_usage.passes <= 1 && output.service_usage.receipts <= 1));
		}
		REQUIRE_FALSE(e.rd.native_disposal_active);
		CHECK(first->maintenance_serial == 0);
		CHECK(e.driver.shader_frees == ordinary_frees);
		CHECK(e.rd.frames[0].shaders_to_dispose_of.size() == 1);
		CHECK(e.rd.has_pending_resources_for_processing());
		// A different original lane starts another real cursor on this same RD.
		e.driver.ordinary_shader_free_allowed = true;
		CHECK(native_step() == ERR_BUSY);
		REQUIRE(e.rd.native_disposal_active);
		CHECK(e.rd.native_disposal_continuation.serial > first_serial);
		used = {};
		const auto foreign_calls = e.driver.receipt_calls();
		CHECK(maintenance(*first, used) == ERR_BUSY);
		CHECK(first->maintenance_serial == 0);
		CHECK(e.driver.receipt_calls() == foreign_calls);
		for (uint32_t i = 0; i < 64 && e.rd.native_disposal_active; ++i) {
			const Error error = native_step();
			CHECK((error == OK || error == ERR_BUSY));
		}
		REQUIRE_FALSE(e.rd.native_disposal_active);
		CHECK(e.driver.shader_frees == ordinary_frees + 1);
		CHECK(e.rd.frames[0].shaders_to_dispose_of.is_empty());
		// Counter boundary only: the actual starter allocates the final serial;
		// no numeric fixture write supplies a request ownership token.
		e.rd.next_native_disposal_serial = UINT64_MAX;
		used = {};
		CHECK(maintenance(*first, used) == ERR_BUSY);
		CHECK(first->maintenance_serial == UINT64_MAX);
		CHECK(e.rd.next_native_disposal_serial == 0);
		for (uint32_t i = 0; i < 64 && e.rd.native_disposal_active; ++i) {
			auto output = turn(e, *first, ProductionControlKind::OBSERVE, b);
			CHECK((output.service_usage.passes <= 1 && output.service_usage.receipts <= 1));
		}
		REQUIRE_FALSE(e.rd.native_disposal_active);
		used = {};
		const auto exhausted_calls = e.driver.receipt_calls();
		CHECK(maintenance(*first, used) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(e.rd.native_disposal_active);
		CHECK(first->maintenance_serial == 0);
		CHECK(e.driver.receipt_calls() == exhausted_calls);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, first->serial }).native_error == OK);
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, second->serial }).native_error == OK);
	}
	static void native_record_wait_rejection(bool release_trigger) {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto c = capacity(4); c.retained_payload_bytes = 96;
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial);
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 310, 1, 10, 11 };
		const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 96, 0 };
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *attachment = nullptr; RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, &resource, 1, &use, 1, {}, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		const GDExtensionStorageCreateRequestV1 origin{ Enrollment::ABI, 128, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backing{};
		REQUIRE(create_storage(e, ref, 0, &origin, &backing).native_error == OK);
		uint8_t payload[96]; memset(payload, 0x6b, sizeof(payload));
		const GDExtensionProductionUploadV2 upload{ production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 0, 0, 0, 0, 0, sizeof(payload) };
		REQUIRE(e.registry->production_prepare_upload_v2(ref, &upload, payload, sizeof(payload)).native_error == OK);
		e.rd.upload_staging_buffers.block_size = e.rd.upload_staging_buffers.max_size = 64;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		REQUIRE(e.rd._insert_staging_block(e.rd.upload_staging_buffers) == OK);
		const auto prefix = effect(e, *r, 0);
		REQUIRE(prefix.state == CheckedRecordAppend::State::KnownPrefix);
		REQUIRE(r->frozen);
		GDExtensionProductionSegmentV2 segment{};
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_close_segment_locked(*r, segment) == OK);
		}
		REQUIRE(segment.segment != 0);
		REQUIRE(r->graph_count == 1);
		const auto before_calls = e.driver.receipt_calls();
		const auto before_commands = e.rd.draw_graph.qualification_command_count();
		const auto before_revision = r->revision;
		// Fixed-cell failure harness only: opt-in metadata cannot grant a native
		// scope. The prefix above is actual original upload/graph work. No camera,
		// native output receipt, successful result, or physical completion is faked.
		r->native_prepared_executor = true;
		uint64_t serial = 0;
		REQUIRE(e.registry->production_control_accept(ref, ProductionControlKind::RECORD, &serial) == OK);
		CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY); // PENDING is not window waiting.
		CHECK(r->control.state == ProductionControlState::PENDING);
		r->control.state = ProductionControlState::QUEUED;
		r->control.record_history.admitted = true; // Local Registry's existing mechanical FIFO fixture.
		CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
		CHECK(r->control.state == ProductionControlState::QUEUED); // CQMT has not transferred it yet.
		r->control.scene_waiting = true;
		r->capture_inflight = 1;
		CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
		CHECK(r->control.state == ProductionControlState::QUEUED);
		r->capture_inflight = 0;
		if (release_trigger) {
			const auto b = budget(); GDExtensionProductionServiceBudgetV1 remaining{}; GDExtensionProductionReportV2 report{};
			CHECK(e.registry->production_release_v2(ref, &b, &remaining, &report).native_error == ERR_BUSY);
		} else {
			CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
		}
		REQUIRE(r->control.state == ProductionControlState::RETURNED);
		CHECK_FALSE(r->control.scene_waiting);
		CHECK(r->control.serial == serial);
		CHECK_FALSE(r->control.record_history.control_effect_entered);
		CHECK_FALSE(r->control.record_history.control_append_entered);
		CHECK(r->append_effect_entered); // Earlier actual prefix remains historical work.
		CHECK_FALSE(r->cancelled); // Original RECORD custody still precedes cancel.
		CHECK(r->revision == before_revision);
		CHECK(r->actions[0].recorded_bytes == 64);
		CHECK(r->next_action == 0);
		CHECK(r->graph_count == 1);
		CHECK_FALSE(r->effects_pending);
		CHECK(r->acknowledged_segment != segment.segment);
		{
			MutexLock lock(e.registry->mutex);
			CHECK_FALSE(e.registry->production_native_record_reject_wait_locked(*r)); // Late close/dispatch cannot execute twice.
		}
		CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
		GDExtensionProductionRecordReportV2 returned{};
		REQUIRE(e.registry->production_record_consume_v2(ref, serial, &returned).native_error == OK);
		CHECK(returned.result.native_error == ERR_UNAVAILABLE);
		CHECK(returned.segment.segment == segment.segment);
		CHECK(returned.segment.segment_revision == segment.segment_revision);
		CHECK(returned.segment.incomplete_upload_action_ordinal == 0);
		CHECK(returned.append.abi.abi_version == 0); // No action ran in this rejected turn.
		CHECK(r->native_prepared_unread == 0);
		CHECK(e.registry->production_record_consume_v2(ref, serial, &returned).native_error == ERR_DOES_NOT_EXIST);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		CHECK(e.registry->production_release(ref).native_error == ERR_BUSY); // Actual prior graph is still owed.
		CHECK(e.driver.receipt_calls() == before_calls);
		CHECK(e.rd.draw_graph.qualification_command_count() == before_commands);
		CHECK(r->graph_count == 1);
		CHECK(r->acknowledged_segment != segment.segment); // Refusal did not ACK the prefix.
	}
	static void native_prepared_executor_refusals() {
		using namespace GDExtensionDeviceExit;
		// CPU fixture negative evidence only. No renderer scope, callback flag,
		// native camera receipt, or successful prepared action result is fabricated.
		Enrollment e;
		auto c = capacity(4);
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 309, 1, 10, 11 };
		const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 16, 0 };
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *attachment = nullptr; RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, &resource, 1, &use, 1, {}, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		CHECK(e.registry->production_prepare_native_record_v2(ref).native_error == ERR_UNCONFIGURED);
		CHECK_FALSE(r->native_prepared_executor);
		GDExtensionProductionPreparedResultRefV2 expected{};
		expected.abi = production_dto_abi<GDExtensionProductionPreparedResultRefV2>();
		expected.registry_instance = Enrollment::INSTANCE; expected.request_serial = r->serial;
		expected.control_serial = 1; expected.canonical_begin = 10; expected.canonical_end = 11;
		GDExtensionProductionPreparedActionResultV2 out{}, untouched{};
		memset(&out, 0x5a, sizeof(out)); untouched = out;
		expected.reserved = 1;
		CHECK(e.registry->production_prepared_result_read_v2(ref, &expected, &out).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&out, &untouched, sizeof(out)) == 0);
		expected.reserved = 0;
		CHECK(e.registry->production_prepared_result_read_v2(ref, &expected, &out).native_error == ERR_INVALID_DATA);
		CHECK(memcmp(&out, &untouched, sizeof(out)) == 0);
		const auto calls = e.driver.receipt_calls();
		const auto commands = e.rd.draw_graph.qualification_command_count();
		{
			MutexLock lock(e.registry->mutex);
			// Even opt-in metadata cannot manufacture the private stack borrow.
			r->native_prepared_executor = true;
			CHECK(e.registry->production_record_receive_locked(*r, e.rd) == ERR_UNAVAILABLE);
			r->native_prepared_executor = false;
		}
		CHECK(r->next_action == 0);
		CHECK(r->native_prepared_unread == 0);
		CHECK_FALSE(r->effects_pending);
		CHECK(e.driver.receipt_calls() == calls);
		CHECK(e.rd.draw_graph.qualification_command_count() == commands);
	}
	static void staged_predecessor_rejects_unproven_intent() {
		using namespace GDExtensionDeviceExit;
		Enrollment e; auto *request = setup_v2(e); REQUIRE(request != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, request->serial };
		GDExtensionProductionOutputPlanV2 intent{};
		intent.abi = production_dto_abi<GDExtensionProductionOutputPlanV2>();
		intent.plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
		intent.plan.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		intent.plan.acquire.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		intent.predecessor.abi = production_dto_abi<GDExtensionProductionOutputPredecessorV2>();
		intent.predecessor.proof.abi = production_dto_abi<GDExtensionOutputConsumedProofRefV2>();
		intent.plan.acquire.input.common.has_previous_consumed_tuple = 1;
		intent.plan.acquire.input.common.previous_consumed_tuple_token.bytes[0] = 1;
		const auto native_budget = budget(); const auto turn_budget = budget();
		// Caller bytes cannot invent the actual prior proof/current camera. This
		// test asserts refusal only, without a fabricated positive renderer view.
		CHECK(e.registry->production_stage_output_plan_with_predecessor_v2(ref, &intent, &native_budget, &turn_budget).native_error == ERR_INVALID_DATA);
		CHECK(request->native_staged_anchor == UINT64_MAX);
		CHECK_FALSE(request->actions[0].staged_predecessor_intent);
		CHECK_FALSE(request->frozen);
		CHECK(request->native_stage_controls_used == 0);
		CHECK(request->native_stage_passes_used == 0);
		{ MutexLock lock(e.registry->mutex);
		  CHECK(e.registry->production_output_history_validate_locked(*request, request->actions[0], e.rd, intent.plan.acquire.input) == ERR_INVALID_DATA);
		}
	}
	static void staged_camera_refusals_and_expiry(bool exhausted = false) {
		using namespace GDExtensionDeviceExit;
		// CPU state-machine evidence only. It cannot attest a successful native
		// camera invocation; no positive camera/source/result is fabricated.
		Enrollment e;
		auto c = capacity(4);
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 310, 1, 10, 11 };
		const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *attachment = nullptr; RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
		}
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		GDExtensionProductionOutputPrepareV1 incomplete{};
		incomplete.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		incomplete.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		incomplete.source_use_count = 1; incomplete.source_generation = 1;
		incomplete.input.particles = 1; incomplete.input.common.generation = 1;
		for (unsigned i = 0; i < 3; ++i) { incomplete.input.common.bounds.size[i] = 1; }
		CHECK(e.registry->production_prepare_output(ref, &incomplete).native_error == ERR_INVALID_PARAMETER);
		GDExtensionProductionOutputPlanV1 plan{};
		plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
		plan.source_space = 1; plan.finalize_action_ordinal = 1; plan.seal_action_ordinal = 2;
		plan.guard_dispatch_limit = plan.guard_graph_limit = plan.guard_native_limit = plan.guard_owned_resource_limit = 1;
		const auto staging_budget = budget();
		// This structurally valid plan reaches the existing request check.
		// The public stage already owns Registry; the internal plan helper must
		// borrow it rather than deadlock before detecting this wrong action roster.
		CHECK(e.registry->production_stage_output_plan_v2(ref, &plan, &staging_budget).native_error == ERR_INVALID_DATA);
		{
			MutexLock lock(e.registry->mutex);
			r->actions[0].prepared = true; r->actions[0].output_camera_pending = true;
			CHECK(e.registry->production_freeze_rd_locked(*r, nullptr) == ERR_UNCONFIGURED);
			CHECK_FALSE(r->frozen);
			// Mechanical original-queue fixture: expiry must settle only this
			// original mailbox without manufacturing a frame, source, or ACK.
			r->native_prepared_executor = true; r->native_static_sealed = true;
			r->native_stage_budget = budget();
			if (exhausted) {
				r->native_stage_controls_used = r->native_stage_budget.controls;
			} else {
				r->native_stage_budget.deadline_micros = 1;
			}
			Request *guarded = nullptr; RenderingDevice *rd = nullptr;
			CHECK(e.registry->production_request_locked(ref, true, guarded, rd) == ERR_BUSY);
			CHECK_FALSE(e.registry->production_native_camera_prefix_locked(*r, e.rd, 0));
			CHECK_FALSE(e.registry->production_native_batch_frame_locked(*r));
			r->control = {}; r->control.serial = 11;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true; r->control.record_history.admitted = true;
		}
		const auto calls = e.driver.receipt_calls();
		const auto commands = e.rd.draw_graph.qualification_command_count();
		GDExtensionProductionRecordReportV3 returned{};
		CHECK(e.registry->production_record_consume_v3(ref, 12, &returned).native_error == ERR_DOES_NOT_EXIST);
		CHECK(r->control.state == ProductionControlState::QUEUED);
		REQUIRE(e.registry->production_record_consume_v3(ref, 11, &returned).native_error == OK);
		CHECK(returned.record.result.native_error == ERR_UNAVAILABLE);
		CHECK(returned.abi.abi_version == 3);
		CHECK(returned.controls_used == 0); CHECK(returned.passes_used == 0); CHECK(returned.receipts_used == 0);
		CHECK(returned.offered.controls == r->native_stage_budget.controls);
		CHECK(returned.remaining.controls == (exhausted ? 0 : returned.offered.controls));
		CHECK(returned.remaining.passes == returned.offered.passes);
		CHECK(returned.remaining.receipts == returned.offered.receipts);
		CHECK(returned.remaining.deadline_micros == returned.offered.deadline_micros);
		CHECK(e.registry->production_record_consume_v3(ref, 11, &returned).native_error == ERR_BUSY);
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK_FALSE(r->frozen); CHECK_FALSE(r->cancelled);
		CHECK(r->next_action == 0); CHECK(r->native_prepared_unread == 0);
		CHECK(r->acknowledged_segment == 0);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		CHECK(e.driver.receipt_calls() == calls);
		CHECK(e.rd.draw_graph.qualification_command_count() == commands);
	}
	class DelayedDeliveryTestBuffers : public RenderSceneBuffers {
	public:
		void configure(const RenderSceneBuffersConfiguration *p_config) override {}
		void set_fsr_sharpness(float p_fsr_sharpness) override {}
		void set_texture_mipmap_bias(float p_texture_mipmap_bias) override {}
		void set_anisotropic_filtering_level(RSE::ViewportAnisotropicFiltering p_level) override {}
		void set_use_debanding(bool p_use_debanding) override {}
	};
	static void staged_camera_delayed_delivery_refusal() {
		using namespace GDExtensionDeviceExit;
		REQUIRE(RSG::viewport != nullptr);
		REQUIRE(RSG::scene != nullptr);
		// Real immutable capture from the existing viewport source primitive.
		// This test exercises delayed source dispatch, not native GPU authority.
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		const RID camera_rid = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera_rid);
		RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
		RSG::viewport->viewport_attach_camera(viewport, camera_rid);
		auto *vp = RSG::viewport->viewport_owner.get_or_null(viewport);
		REQUIRE(vp != nullptr);
		if (vp->render_buffers.is_null()) {
			vp->render_buffers = Ref<RenderSceneBuffers>(memnew(DelayedDeliveryTestBuffers));
		}
		REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, { 901, 1, 1 }));
		RendererSceneRender::CameraData camera;
		camera.set_camera(Transform3D(), Projection(), false, false, Vector2(), 0, 1);
		camera.source_viewport = viewport;
		const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera, camera_rid, vp->render_buffers);
		Ref<RendererViewport::Viewport::CameraCapture> capture;
		REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(viewport, token, capture));
		{
			Enrollment e; auto c = capacity(4); const auto attachment = e.accept(c);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 311, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{ MutexLock lock(e.registry->mutex); ProductionAttachment *a = nullptr; RenderingDevice *rd = nullptr;
			  REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
			  REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			auto &action = r->actions[0]; auto &subscriber = action.camera_ready_subscriber;
			action.prepared = action.output_camera_pending = true; action.camera_ready_viewport = viewport;
			subscriber.registry = e.registry; subscriber.attachment = attachment;
			subscriber.request = { Enrollment::INSTANCE, r->serial }; subscriber.action_ordinal = 0;
			subscriber.delivery = capture;
			GDExtensionProductionCameraSampleV1 sample{}; REQUIRE(production_camera_sample(*capture.ptr(), sample) == OK);
			// Both legal ordering windows are checked: stage intent before final
			// static sealing, then intent plus the sealed native roster.
			CHECK(e.registry->camera_ready_dispatch_observer(subscriber, sample) == ERR_BUSY);
			r->native_static_sealed = true;
			CHECK(e.registry->camera_ready_dispatch_observer(subscriber, sample) == ERR_BUSY);
			CHECK(action.preinput_camera_source.is_null()); CHECK_FALSE(subscriber.delivery_pinned);
			CHECK(r->capture_inflight == 0); CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
			subscriber.delivery.unref(); subscriber.registry = nullptr;
		}
		CHECK(RSG::viewport->free(viewport));
		CHECK(RSG::scene->free(camera_rid));
	}
	static void staged_camera_unprepared_cull_preserves_stage_budget() {
		using namespace GDExtensionDeviceExit;
		REQUIRE(RSG::viewport != nullptr);
		REQUIRE(RSG::scene != nullptr);
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		const RID camera_rid = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera_rid);
		RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
		RSG::viewport->viewport_attach_camera(viewport, camera_rid);
		auto *vp = RSG::viewport->viewport_owner.get_or_null(viewport);
		REQUIRE(vp != nullptr);
		if (vp->render_buffers.is_null()) {
			vp->render_buffers = Ref<RenderSceneBuffers>(memnew(DelayedDeliveryTestBuffers));
		}
		REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, { 902, 1, 1 }));
		RendererSceneRender::CameraData camera;
		camera.set_camera(Transform3D(), Projection(), false, false, Vector2(), 0, 1);
		camera.source_viewport = viewport;
		const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera, camera_rid, vp->render_buffers);
		Ref<RendererViewport::Viewport::CameraCapture> capture;
		REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(viewport, token, capture));
		{
			Enrollment e;
			auto c = capacity(4);
			const auto attachment = e.accept(c);
			REQUIRE(attachment.handle_serial != 0);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 312, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *a = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			auto &action = r->actions[0];
			auto &subscriber = action.camera_ready_subscriber;
			r->native_prepared_executor = true;
			r->native_static_sealed = true;
			r->native_staged_anchor = 0;
			r->native_stage_budget = budget();
			r->control = {};
			r->control.serial = 11;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			action.prepared = true;
			action.output_camera_pending = true;
			action.output_plan_prepared = true;
			action.preinput_host_prepared = true;
			action.camera_ready_viewport = viewport;
			action.scene_cull_prepared = false;
			subscriber.registry = e.registry;
			subscriber.attachment = attachment;
			subscriber.request = { Enrollment::INSTANCE, r->serial };
			subscriber.action_ordinal = 0;
			subscriber.subscribed = true;
			subscriber.last_error = OK;
			REQUIRE(RSG::viewport->viewport_subscribe_camera_source(viewport, &subscriber));
			TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, &subscriber);
			CHECK(subscriber.get_last_error() == ERR_BUSY);
			CHECK(r->native_stage_controls_used == 0);
			CHECK(r->native_stage_passes_used == 0);
			CHECK(r->native_stage_receipts_used == 0);
			CHECK(r->control.state == ProductionControlState::QUEUED);
			CHECK(r->control.scene_waiting);
			CHECK(RSG::viewport->viewport_unsubscribe_camera_source(viewport, &subscriber));
			subscriber.subscribed = false;
			subscriber.registry = nullptr;
		}
		CHECK(RSG::viewport->free(viewport));
		CHECK(RSG::scene->free(camera_rid));
	}

	static void record_v3_actual_usage_projection() {
		using namespace GDExtensionDeviceExit;
		Enrollment e; auto *r = setup_v2(e); REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		uint64_t serial = 0; const auto offered = budget(5, 7);
		REQUIRE(e.registry->production_control_accept(ref, ProductionControlKind::RECORD, &serial, &offered) == OK);
		{ MutexLock lock(e.registry->mutex);
		  r->native_stage_control = serial;
		  r->control.state = ProductionControlState::QUEUED;
		  // Actual original executor produces the failed receive and its usage;
		  // the fixture does not write RETURNED output or fabricate a camera.
		  e.registry->production_control_execute_locked(*r, nullptr);
		}
		GDExtensionProductionRecordReportV3 report{};
		REQUIRE(e.registry->production_record_consume_v3(ref, serial, &report).native_error == OK);
		CHECK(report.record.result.native_error == ERR_UNAVAILABLE);
		CHECK(report.controls_used == 1); CHECK(report.passes_used == 1); CHECK(report.receipts_used == 1);
		CHECK(report.remaining.controls == offered.controls - 1);
		CHECK(report.remaining.passes == offered.passes - 1);
		CHECK(report.remaining.receipts == offered.receipts - 1);
		CHECK(report.remaining.deadline_micros == offered.deadline_micros);
		CHECK(report.record.control_serial == serial); CHECK(report.record.request_serial == r->serial);
	}

	static void record_v3_two_turns_original_stage_slice() {
		using namespace GDExtensionDeviceExit;
		Enrollment e; auto *r = setup_v2(e); REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		auto original = budget(9, 7); original.controls = 3;
		r->native_static_sealed = true; r->native_stage_budget = original;
		for (uint32_t turn = 0; turn < 2; ++turn) {
			// Deliberately different exact per-control quotes. Neither replaces
			// the original immutable stage slice used by offered/remaining.
			auto control_quote = budget(turn ? 3 : 5, turn ? 2 : 4);
			uint64_t serial = 0;
			REQUIRE(e.registry->production_control_accept(ref, ProductionControlKind::RECORD, &serial, &control_quote) == OK);
			{ Registry::Lock lock(*e.registry);
			  r->control.state = ProductionControlState::QUEUED;
			  REQUIRE(e.registry->production_native_stage_charge_locked(*r, true) == OK);
			  // Production charge + executor, not a hand-authored RETURNED report.
			  e.registry->production_control_execute_locked(*r, nullptr);
			}
			GDExtensionProductionRecordReportV3 report{};
			REQUIRE(e.registry->production_record_consume_v3(ref, serial, &report).native_error == OK);
			CHECK(report.record.control_serial == serial); CHECK(report.controls_used == 1);
			CHECK(report.passes_used == 1); CHECK(report.receipts_used == 1);
			CHECK(memcmp(&report.offered, &original, sizeof(original)) == 0);
			CHECK(report.remaining.controls == original.controls - turn - 1);
			CHECK(report.remaining.passes == original.passes - turn - 1);
			CHECK(report.remaining.receipts == original.receipts - turn - 1);
		}
		CHECK(r->native_stage_controls_used == 2);
		CHECK(r->native_stage_passes_used == 2); CHECK(r->native_stage_receipts_used == 2);
	}

	static void hard_ceiling() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e, 100);
		if (!r) {
			return;
		}
		capture(e, *r, 100);
		auto out = turn(e, *r, ProductionControlKind::OBSERVE, budget(UINT32_MAX, UINT32_MAX));
		CHECK(out.service_usage.passes == 64);
		CHECK(out.service_usage.receipts == 128);
		CHECK(e.driver.retains[64] == 1);
		CHECK(e.driver.retains[65] == 0);
		CHECK(r->observe_cursor == 64);
		auto b = budget(1, 1);
		out = turn(e, *r, ProductionControlKind::OBSERVE, b);
		CHECK(e.driver.retains[65] == 1);
		CHECK(out.service_usage.receipts == 1);
	}

	static void bug120_stall_for_frame_preserves_busy_readback() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = borrowed_readback_setup(e);
		REQUIRE(r != nullptr);
		for (uint32_t i = 0; i < 2; ++i) {
			REQUIRE(effect(e, *r, i).state == CheckedRecordAppend::State::Appended);
		}
		e.driver.complete = true;
		execute_transfers(e, *r, false);
		REQUIRE(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);

		// Simulate transient ERR_BUSY from production_readback_delivery(*capture, true).
		r->capture_inflight = 1;
		e.rd.production_recording_request = r;
		CHECK(e.rd._stall_for_frame(0, true) == ERR_BUSY);
		CHECK_FALSE(e.rd.frame_wait_draining.load());
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);
		CHECK_FALSE(e.rd.frames[0].download_buffer_staging_buffers.is_empty());
		CHECK_FALSE(e.rd.frames[0].download_buffer_copy_regions.is_empty());
		CHECK_FALSE(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.error == OK);
		CHECK(r->readbacks[0].source.returned == 0);

		// Once transient contention clears, retry delivers the preserved request cleanly!
		e.rd.production_recording_request = nullptr;
		r->capture_inflight = 0;
		CHECK(e.rd._stall_for_frame(0, true) == OK);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.is_empty());
		CHECK(e.rd.frames[0].download_buffer_staging_buffers.is_empty());
		CHECK(e.rd.frames[0].download_buffer_copy_regions.is_empty());
		CHECK(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.error == OK);
		CHECK(r->readbacks[0].source.completed == 16);
		CHECK(r->readbacks[0].source.returned == 16);

		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		for (uint32_t i = 0; i < 64; ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, budget());
			GDExtensionProductionReportV1 rep = {};
			auto q = budget(0, 0);
			if (e.registry->production_observe(ref, &q, &rep).native_error == OK &&
					rep.possible_obligations == 0 && rep.physical_returns_pending == 0 &&
					rep.known_obligations == rep.complete_obligations && !e.rd.native_disposal_active) {
				break;
			}
		}
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		CHECK(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment).native_error == OK);
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(e.original_storage->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
	}

	static void bug120_split_readback_order_and_sibling_pump_and_closure() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true;
		auto *r = borrowed_readback_setup(e, true);
		REQUIRE(r != nullptr);
		const auto attachment_ref = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };

		// Bind r->readbacks[0].capture to its arena storage and owning request.
		uint8_t rb_storage[16] = {};
		r->readback_count = 1;
		auto &rb_slot = r->readbacks[0];
		rb_slot.capture.registry = e.registry;
		rb_slot.capture.request_serial = r->serial;
		rb_slot.capture.state = &rb_slot.source;
		rb_slot.capture.bytes = rb_storage;
		rb_slot.capture.byte_capacity = 16;

		// Add a second concurrent sibling request on the same attachment/RD.
		Request *sibling = nullptr;
		REQUIRE(e.reserve(attachment_ref, capacity(3), 77, sibling) == OK);
		REQUIRE(sibling != nullptr);

		// 1. Sibling pump starvation check:
		//    r finished all actions + acked segment; sibling is mid-plan with CLOSED lists.
		r->next_action = r->action_count;
		r->acknowledged_segment = r->segment.segment;
		sibling->action_count = 2;
		sibling->next_action = 1;
		sibling->compute_list_open = false;
		sibling->draw_list_open = false;
		sibling->capture_inflight = 0;
		sibling->continuation_inflight = false;
		CHECK(e.registry->production_readback_ready_to_pump(rb_slot.capture));
		// While sibling actually holds an open compute list, pump must still wait.
		sibling->compute_list_open = true;
		CHECK_FALSE(e.registry->production_readback_ready_to_pump(rb_slot.capture));
		sibling->compute_list_open = false;
		sibling->action_count = 0;
		sibling->next_action = 0;

		// 2. Sibling output-closure buffer check in exact_closure:
		//    A retiring buffer owned by sibling via ProductionDisposalUse (attached through
		//    the real _production_disposal_attach_owned -> free_rid path) must NOT deadlock
		//    r's production_maintenance_locked with foreign_buffer.
		RenderingDevice::Buffer sib_buf;
		sib_buf.driver_id = e.driver.buffer_create(16, {}, RDD::MEMORY_ALLOCATION_TYPE_GPU, 0);
		sib_buf.size = 16;
		sib_buf.backing_serial = 909;
		REQUIRE(sib_buf.driver_id.id != 0);
		const RID sib_rid = e.rd.storage_buffer_owner.make_rid(sib_buf);
		ProductionDisposalUse sib_use;
		REQUIRE(e.rd._production_disposal_attach_owned(sib_use, *sibling, sib_rid) == OK);
		e.rd.free_rid(sib_rid);
		REQUIRE(e.rd.frames[0].buffers_to_dispose_of.size() == 1);
		e.rd.qualification_frame_advance_pending = true;
		{
			ProductionServiceUsage used = {};
			MutexLock reg_lock(e.registry->mutex);
			MutexLock rd_lock(e.rd._thread_safe_);
			BudgetDriverGuard guard(e.driver);
			const Error m_err = e.registry->production_maintenance_locked(*r, e.rd, budget(4, 4), used);
			CHECK((m_err == OK || m_err == ERR_BUSY));
			CHECK((e.rd.native_disposal_active || e.rd.frames[0].buffers_to_dispose_of.is_empty()));
		}
		REQUIRE(finish_owned_maintenance(e, *r, budget(16, 16)));
		CHECK(e.rd.frames[0].buffers_to_dispose_of.is_empty());

		// 3. Split multi-frame readback order in mq_in_place AND rb_frame fast-path:
		//    Put segment 0 (offset=0, size=8, final=false) on frame 1 and segment 1 (offset=8, size=8, final=true) on frame 0.
		e.rd.frames.resize(2);
		e.rd.frames[1].qualification_epoch = 2;
		e.rd.frames[1].fence = RDD::FenceID(93);
		e.rd.frames[1].command_buffer = RDD::CommandBufferID(92);
		e.rd.frames[1].command_pool = RDD::CommandPoolID(96);
		e.rd.next_qualification_frame_epoch = 3;
		e.driver.expected_begin_frame = 0;

		const RDD::BufferID stage0 = e.driver.buffer_create(8, {}, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
		const RDD::BufferID stage1 = e.driver.buffer_create(8, {}, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
		REQUIRE(stage0.id != 0);
		REQUIRE(stage1.id != 0);
		if (uint8_t *p0 = e.driver.buffer_map(stage0)) {
			for (uint32_t i = 0; i < 8; ++i) {
				p0[i] = uint8_t(0x10 + i);
			}
			e.driver.buffer_unmap(stage0);
		}
		if (uint8_t *p1 = e.driver.buffer_map(stage1)) {
			for (uint32_t i = 0; i < 8; ++i) {
				p1[i] = uint8_t(0x20 + i);
			}
			e.driver.buffer_unmap(stage1);
		}

		rb_slot.source.issued = 16;
		rb_slot.source.returned = 0;
		rb_slot.source.completed = 0;
		rb_slot.source.delivered = false;
		rb_slot.source.error = OK;

		RenderingDevice::BufferGetDataRequest seg0;
		seg0.capture = &rb_slot.capture;
		seg0.result_offset = 0;
		seg0.size = 8;
		seg0.frame_local_index = 0;
		seg0.frame_local_count = 1;
		seg0.final_segment = false;
		e.rd.frames[1].download_buffer_staging_buffers.push_back(stage0);
		e.rd.frames[1].download_buffer_copy_regions.push_back({ 0, 0, 8 });
		e.rd.frames[1].download_buffer_get_data_requests.push_back(seg0);
		e.rd.frames[1].fence_signaled = true;
		RDD::QueueCallReport rep1;
		rep1.submission_serial = 11;
		e.rd.frames[1].chain_reports.push_back(rep1);

		RenderingDevice::BufferGetDataRequest seg1;
		seg1.capture = &rb_slot.capture;
		seg1.result_offset = 8;
		seg1.size = 8;
		seg1.frame_local_index = 0;
		seg1.frame_local_count = 1;
		seg1.final_segment = true;
		e.rd.frames[0].download_buffer_staging_buffers.push_back(stage1);
		e.rd.frames[0].download_buffer_copy_regions.push_back({ 0, 0, 8 });
		e.rd.frames[0].download_buffer_get_data_requests.push_back(seg1);
		e.rd.frames[0].fence_signaled = true;
		RDD::QueueCallReport rep0;
		rep0.submission_serial = 12;
		e.rd.frames[0].chain_reports.push_back(rep0);

		// 3a. Pure mq_in_place on frame 0 MUST refuse out-of-order segment 1 (result_offset=8) with ERR_BUSY!
		const uint32_t saved_sib_res = sibling->resource_count;
		const uint32_t saved_sib_act = sibling->action_count;
		const auto saved_sib_kind = sibling->actions[0].declaration.action_kind;
		sibling->resource_count = 0;
		sibling->action_count = 1;
		sibling->actions[0].declaration.action_kind = GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1;
		sibling->graph_count = 1;
		sibling->graphs[0].pending = true;
		e.rd.frame = 0;
		{
			ProductionServiceUsage used = {};
			MutexLock reg_lock(e.registry->mutex);
			MutexLock rd_lock(e.rd._thread_safe_);
			BudgetDriverGuard guard(e.driver);
			CHECK(e.registry->production_maintenance_locked(*sibling, e.rd, budget(64, 16), used) == ERR_BUSY);
		}
		CHECK_FALSE(e.rd.native_disposal_active);
		CHECK(rb_slot.source.error == OK);
		CHECK(rb_slot.source.returned == 0);
		sibling->graph_count = 0;
		sibling->resource_count = saved_sib_res;
		sibling->action_count = saved_sib_act;
		sibling->actions[0].declaration.action_kind = saved_sib_kind;

		// 3b. rb_frame fast-path MUST stall frame 1 FIRST (offset=0) and frame 0 SECOND (offset=8) with 0 ERR_INVALID_DATA!
		r->graph_count = 1;
		r->graphs[0].pending = false;
		for (uint32_t step = 0; step < 32 && !rb_slot.source.delivered; ++step) {
			ProductionServiceUsage used = {};
			MutexLock reg_lock(e.registry->mutex);
			MutexLock rd_lock(e.rd._thread_safe_);
			BudgetDriverGuard guard(e.driver);
			const Error m_err = e.registry->production_maintenance_locked(*r, e.rd, budget(64, 16), used);
			CHECK((m_err == OK || m_err == ERR_BUSY));
		}
		CHECK(rb_slot.source.error == OK);
		CHECK(rb_slot.source.delivered);
		CHECK(rb_slot.source.returned == 16);
		CHECK(rb_slot.source.completed == 16);
		for (uint32_t i = 0; i < 8; ++i) {
			CHECK(rb_storage[i] == uint8_t(0x10 + i));
			CHECK(rb_storage[8 + i] == uint8_t(0x20 + i));
		}

		e.driver.buffer_free(stage0);
		e.driver.buffer_free(stage1);
		r->graph_count = 0;
		e.rd.frames[0].chain_reports.clear();
		e.rd.frames[1].chain_reports.clear();
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, sibling->serial }).native_error == OK);
		CHECK(e.registry->production_cancel({ Enrollment::INSTANCE, r->serial }).native_error == OK);
		CHECK(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(e.original_storage->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
	}

	static void bug124_disposal_stall_pops_delivered_and_unmaps_interrupted_map() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true;
		auto *r0 = borrowed_readback_setup(e, true);
		REQUIRE(r0 != nullptr);
		const auto attachment_ref = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r0->attachment->serial };
		Request *r1 = nullptr;
		REQUIRE(e.reserve(attachment_ref, capacity(3), 88, r1) == OK);
		REQUIRE(r1 != nullptr);

		uint8_t dst0[8] = {}, dst1[8] = {};
		r0->readbacks = r0->attachment->readbacks;
		r0->readback_count = 1;
		auto &rb0 = r0->readbacks[0];
		rb0.capture.registry = e.registry;
		rb0.capture.request_serial = r0->serial;
		rb0.capture.state = &rb0.source;
		rb0.capture.bytes = dst0;
		rb0.capture.byte_capacity = 8;
		rb0.source = {};
		rb0.source.issued = 8;

		r1->readbacks = r1->attachment->readbacks + 1;
		r1->readback_count = 1;
		auto &rb1 = r1->readbacks[0];
		rb1.capture.registry = e.registry;
		rb1.capture.request_serial = r1->serial;
		rb1.capture.state = &rb1.source;
		rb1.capture.bytes = dst1;
		rb1.capture.byte_capacity = 8;
		rb1.source = {};
		rb1.source.issued = 8;

		const RDD::BufferID stage0 = e.driver.buffer_create(8, {}, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
		const RDD::BufferID stage1 = e.driver.buffer_create(8, {}, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
		REQUIRE(stage0.id != 0);
		REQUIRE(stage1.id != 0);
		if (uint8_t *p0 = e.driver.buffer_map(stage0)) {
			for (uint32_t i = 0; i < 8; ++i) {
				p0[i] = uint8_t(0x30 + i);
			}
			e.driver.buffer_unmap(stage0);
		}
		if (uint8_t *p1 = e.driver.buffer_map(stage1)) {
			for (uint32_t i = 0; i < 8; ++i) {
				p1[i] = uint8_t(0x40 + i);
			}
			e.driver.buffer_unmap(stage1);
		}

		// Queue BOTH requests on frame 0 with fence_signaled = true.
		RenderingDevice::BufferGetDataRequest req0;
		req0.capture = &rb0.capture;
		req0.result_offset = 0;
		req0.size = 8;
		req0.frame_local_index = 0;
		req0.frame_local_count = 1;
		req0.final_segment = true;

		RenderingDevice::BufferGetDataRequest req1;
		req1.capture = &rb1.capture;
		req1.result_offset = 0;
		req1.size = 8;
		req1.frame_local_index = 1;
		req1.frame_local_count = 1;
		req1.final_segment = true;

		e.rd.frames[0].download_buffer_staging_buffers.push_back(stage0);
		e.rd.frames[0].download_buffer_staging_buffers.push_back(stage1);
		e.rd.frames[0].download_buffer_copy_regions.push_back({ 0, 0, 8 });
		e.rd.frames[0].download_buffer_copy_regions.push_back({ 0, 0, 8 });
		e.rd.frames[0].download_buffer_get_data_requests.push_back(req0);
		e.rd.frames[0].download_buffer_get_data_requests.push_back(req1);
		e.rd.frames[0].fence_signaled = true;
		e.rd.frames[0].primary_command_buffer_begun = true;
		RDD::QueueCallReport rep0;
		rep0.submission_serial = 21;
		e.rd.frames[0].chain_reports.push_back(rep0);

		// 1. Budget of 5 passes: fence_poll (1), fence_recycle (2), req0 map (3), req0 unmap (4),
		//    req1 map (5) -> returns ERR_BUSY right before req1 unmap (pass 6), leaving cursor.mapped != nullptr!
		RenderingDevice::NativeDisposalBudget slice;
		slice.receipts = 5;
		slice.passes = 5;
		slice.deadline_usec = OS::get_singleton()->get_ticks_usec() + 60000000ull;
		RenderingDevice::NativeDisposalUsage actual;
		e.rd.native_disposal_budget = &slice;
		e.rd.native_disposal_usage = &actual;
		CHECK(e.rd._native_disposal_stall_step(0) == ERR_BUSY);
		e.rd.native_disposal_budget = nullptr;
		e.rd.native_disposal_usage = nullptr;

		// Request 0 MUST already be delivered and popped; ONLY Request 1 remains at [0] and is mapped.
		CHECK(rb0.source.delivered);
		CHECK(rb0.source.error == OK);
		CHECK(rb0.source.returned == 8);
		CHECK(rb0.source.completed == 8);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests[0].capture == &rb1.capture);
		CHECK(e.rd.frames[0].download_buffer_staging_buffers.size() == 2);
		CHECK(e.rd.frames[0].maintenance_wait.mapped != nullptr);
		CHECK(e.driver.delivery_map_id == stage1.id);
		CHECK_FALSE(rb1.source.delivered);
		CHECK(rb1.source.error == OK);

		// 2. Now _begin_frame() runs with !native_disposal_active:
		//    Must NOT clobber maintenance_wait before _stall_for_frame(), must cleanly unmap
		//    cursor.mapped, must NOT re-deliver Request 0 (0 ERR_INVALID_DATA!), and must deliver Request 1!
		e.rd.frame = 0;
		e.driver.expected_begin_frame = 0;
		CHECK(e.rd._begin_frame(false, true) == OK);
		CHECK(e.driver.delivery_map_id == 0);
		CHECK(rb0.source.error == OK);
		CHECK(rb0.source.returned == 8);
		CHECK(rb1.source.delivered);
		CHECK(rb1.source.error == OK);
		CHECK(rb1.source.returned == 8);
		CHECK(rb1.source.completed == 8);
		for (uint32_t i = 0; i < 8; ++i) {
			CHECK(dst0[i] == uint8_t(0x30 + i));
			CHECK(dst1[i] == uint8_t(0x40 + i));
		}
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.is_empty());
		CHECK(e.rd.frames[0].download_buffer_staging_buffers.is_empty());
		CHECK(e.rd.frames[0].download_buffer_copy_regions.is_empty());
		CHECK(e.rd.frames[0].maintenance_wait.mapped == nullptr);

		// 3. Also verify _production_mq_drain_owned_disposals_step resumes a Phase-4-interrupted stall
		//    even when fence_signaled is already false!
		rb1.source = {};
		rb1.source.issued = 8;
		memset(dst1, 0, sizeof(dst1));
		e.rd.frames[0].download_buffer_staging_buffers.push_back(stage1);
		e.rd.frames[0].download_buffer_copy_regions.push_back({ 0, 0, 8 });
		req1.frame_local_index = 0;
		e.rd.frames[0].download_buffer_get_data_requests.push_back(req1);
		e.rd.frames[0].fence_signaled = false;
		e.rd.frames[0].primary_command_buffer_begun = false;
		e.rd.frames[0].maintenance_wait = {};
		e.rd.frames[0].maintenance_wait.phase = 4;
		e.rd.frames[0].maintenance_wait.fence_returned = true;

		RenderingDevice::Buffer owned_buf;
		owned_buf.driver_id = e.driver.buffer_create(16, {}, RDD::MEMORY_ALLOCATION_TYPE_GPU, 0);
		owned_buf.size = 16;
		owned_buf.backing_serial = 910;
		REQUIRE(owned_buf.driver_id.id != 0);
		const RID owned_rid = e.rd.storage_buffer_owner.make_rid(owned_buf);
		ProductionDisposalUse owned_use;
		REQUIRE(e.rd._production_disposal_attach_owned(owned_use, *r1, owned_rid) == OK);
		e.rd.free_rid(owned_rid);

		RenderingDevice::NativeDisposalBudget mq_budget;
		mq_budget.receipts = 32;
		mq_budget.passes = 16;
		mq_budget.deadline_usec = OS::get_singleton()->get_ticks_usec() + 60000000ull;
		RenderingDevice::NativeDisposalUsage mq_usage;
		CHECK(e.rd._production_mq_drain_owned_disposals_step(*r1, nullptr, 0, mq_budget, mq_usage) == OK);
		CHECK(rb1.source.delivered);
		CHECK(rb1.source.error == OK);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.is_empty());
		CHECK(e.rd.frames[0].buffers_to_dispose_of.is_empty());

		e.driver.buffer_free(stage0);
		e.driver.buffer_free(stage1);
		e.rd.frames[0].chain_reports.clear();
		CHECK(e.registry->production_release({ Enrollment::INSTANCE, r1->serial }).native_error == OK);
		CHECK(e.registry->production_cancel({ Enrollment::INSTANCE, r0->serial }).native_error == OK);
		CHECK(turn(e, *r0, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(e.original_storage->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
	}

	static void bug125_stall_for_frame_marks_chain_reports_returned_before_deferred_busy() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true;
		e.driver.height_observation = true;
		auto *r = borrowed_readback_setup(e);
		REQUIRE(r != nullptr);
		for (uint32_t i = 0; i < 2; ++i) {
			REQUIRE(effect(e, *r, i).state == CheckedRecordAppend::State::Appended);
		}
		execute_transfers(e, *r, false);
		REQUIRE(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);
		REQUIRE_FALSE(e.rd.frames[0].chain_reports.is_empty());
		REQUIRE_FALSE(e.rd.frames[0].chain_reports[0].calls.is_empty());

		// Trigger transient ERR_BUSY during capture delivery inside _stall_for_frame.
		r->capture_inflight = 1;
		e.rd.production_recording_request = r;
		CHECK(e.rd._stall_for_frame(0, true) == ERR_BUSY);
		CHECK_FALSE(e.rd.frames[0].fence_signaled);
		// BUG-125: chain_reports calls must ALREADY be marked source_owner_returned = true!
		for (const auto &rep : e.rd.frames[0].chain_reports) {
			for (const auto &call : rep.calls) {
				if (!call.presentation_only) {
					CHECK(call.source_owner_returned);
				}
			}
		}
		const uint64_t tracked_serial = e.rd.frames[0].chain_reports[0].calls[0].serial;
		const uint32_t polls_after_recycle = tracked_serial < 128 ? e.driver.polls[tracked_serial] : 0;

		// Clear transient contention and retry _stall_for_frame: must NOT re-poll the recycled call!
		e.rd.production_recording_request = nullptr;
		r->capture_inflight = 0;
		CHECK(e.rd._stall_for_frame(0, true) == OK);
		if (tracked_serial < 128) {
			CHECK(e.driver.polls[tracked_serial] == polls_after_recycle);
		}
		CHECK(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.error == OK);

		const auto ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, r->serial };
		const auto attachment = GDExtensionProductionAttachmentRefV1{ Enrollment::INSTANCE, r->attachment->serial };
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		for (uint32_t i = 0; i < 64; ++i) {
			turn(e, *r, ProductionControlKind::OBSERVE, budget());
			GDExtensionProductionReportV1 rep = {};
			auto q = budget(0, 0);
			if (e.registry->production_observe(ref, &q, &rep).native_error == OK &&
					rep.possible_obligations == 0 && rep.physical_returns_pending == 0 &&
					rep.known_obligations == rep.complete_obligations && !e.rd.native_disposal_active) {
				break;
			}
		}
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		CHECK(turn(e, *r, ProductionControlKind::RELEASE, budget()).owner_return.native_error == OK);
		CHECK(e.registry->production_close(attachment).native_error == OK);
		auto &physical = e.original_storage->physical;
		physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(e.original_storage->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
	}

	static void bug137_staged_cull_permanent_failure_and_bounded_busy() {
		using namespace GDExtensionDeviceExit;
		REQUIRE(RSG::viewport != nullptr);
		REQUIRE(RSG::scene != nullptr);
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		const RID camera_rid = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera_rid);
		RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
		RSG::viewport->viewport_attach_camera(viewport, camera_rid);
		auto *vp = RSG::viewport->viewport_owner.get_or_null(viewport);
		REQUIRE(vp != nullptr);
		if (vp->render_buffers.is_null()) {
			vp->render_buffers = Ref<RenderSceneBuffers>(memnew(DelayedDeliveryTestBuffers));
		}
		REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, { 937, 1, 1 }));
		RendererSceneRender::CameraData camera;
		camera.set_camera(Transform3D(), Projection(), false, false, Vector2(), 0, 1);
		camera.source_viewport = viewport;
		const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera, camera_rid, vp->render_buffers);
		Ref<RendererViewport::Viewport::CameraCapture> capture;
		REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(viewport, token, capture));

		// Part 1: Repeated ERR_BUSY from pre_cull is bounded by stage pass/receipt budget
		// and terminates into RETURNED instead of spinning uncharged until deadline_micros.
		{
			Enrollment e;
			auto c = capacity(4);
			const auto attachment = e.accept(c);
			REQUIRE(attachment.handle_serial != 0);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 337, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *a = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
				a->opened = true;
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			auto &action = r->actions[0];
			auto &subscriber = action.camera_ready_subscriber;
			r->native_prepared_executor = true;
			r->native_static_sealed = true;
			r->native_staged_anchor = 0;
			r->native_stage_budget = budget(3, 3);
			r->control = {};
			r->control.serial = 11;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			r->control.record_history.admitted = true;
			action.prepared = true;
			action.output_source_prepared = true;
			action.output_camera_pending = true;
			action.output_plan_prepared = true;
			action.preinput_host_prepared = true;
			action.camera_ready_viewport = viewport;
			action.scene_cull_prepared = false;
			action.scene_cull_attempted = true;
			action.scene_cull_attempted_camera = { token.view_id, token.view_epoch, token.capture_ordinal };
			action.scene_cull_error = ERR_BUSY;
			subscriber.registry = e.registry;
			subscriber.attachment = attachment;
			subscriber.request = ref;
			subscriber.action_ordinal = 0;
			subscriber.subscribed = true;
			subscriber.last_error = OK;
			REQUIRE(RSG::viewport->viewport_subscribe_camera_source(viewport, &subscriber));

			// First 2 attempts charge passes/receipts while remaining QUEUED.
			TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, &subscriber);
			CHECK(subscriber.get_last_error() == ERR_BUSY);
			CHECK(r->native_stage_passes_used == 1);
			CHECK(r->native_stage_receipts_used == 1);
			CHECK(r->control.state == ProductionControlState::QUEUED);
			CHECK(r->control.scene_waiting);

			TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, &subscriber);
			CHECK(r->native_stage_passes_used == 2);
			CHECK(r->native_stage_receipts_used == 2);
			CHECK(r->control.state == ProductionControlState::QUEUED);

			// 3rd attempt exhausts passes/receipts and immediately rejects wait into RETURNED.
			TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, &subscriber);
			CHECK(r->native_stage_passes_used == 3);
			CHECK(r->native_stage_receipts_used == 3);
			CHECK(r->control.state == ProductionControlState::RETURNED);
			CHECK_FALSE(r->control.scene_waiting);

			GDExtensionProductionRecordReportV3 report{};
			REQUIRE(e.registry->production_record_consume_v3(ref, 11, &report).native_error == OK);
			CHECK(report.record.result.native_error == ERR_UNAVAILABLE);
			CHECK(report.remaining.passes == 0);
			CHECK(report.remaining.receipts == 0);

			CHECK(RSG::viewport->viewport_unsubscribe_camera_source(viewport, &subscriber));
			subscriber.subscribed = false;
			subscriber.registry = nullptr;
		}

		// Part 2: Permanent scene_cull_error (e.g. ERR_INVALID_DATA) on this capture
		// surfaces immediately through RETURNED without waiting for deadline_micros.
		{
			Enrollment e;
			auto c = capacity(4);
			const auto attachment = e.accept(c);
			REQUIRE(attachment.handle_serial != 0);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 338, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *a = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
				a->opened = true;
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			auto &action = r->actions[0];
			auto &subscriber = action.camera_ready_subscriber;
			r->native_prepared_executor = true;
			r->native_static_sealed = true;
			r->native_staged_anchor = 0;
			r->native_stage_budget = budget(16, 16);
			r->control = {};
			r->control.serial = 11;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			r->control.record_history.admitted = true;
			action.prepared = true;
			action.output_source_prepared = true;
			action.output_camera_pending = true;
			action.output_plan_prepared = true;
			action.preinput_host_prepared = true;
			action.camera_ready_viewport = viewport;
			action.scene_cull_prepared = false;
			action.scene_cull_attempted = true;
			action.scene_cull_attempted_camera = { token.view_id, token.view_epoch, token.capture_ordinal };
			action.scene_cull_error = ERR_INVALID_DATA;
			subscriber.registry = e.registry;
			subscriber.attachment = attachment;
			subscriber.request = ref;
			subscriber.action_ordinal = 0;
			subscriber.subscribed = true;
			subscriber.last_error = OK;
			REQUIRE(RSG::viewport->viewport_subscribe_camera_source(viewport, &subscriber));

			TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, &subscriber);
			CHECK(subscriber.get_last_error() == ERR_INVALID_DATA);
			CHECK(r->control.state == ProductionControlState::RETURNED);
			CHECK_FALSE(r->control.scene_waiting);

			GDExtensionProductionRecordReportV3 report{};
			REQUIRE(e.registry->production_record_consume_v3(ref, 11, &report).native_error == OK);
			CHECK(report.record.result.native_error == ERR_INVALID_DATA);
			CHECK(report.controls_used == 1);
			CHECK(report.passes_used == 1);
			CHECK(report.receipts_used == 1);
			CHECK(r->next_action == 0);

			CHECK(RSG::viewport->viewport_unsubscribe_camera_source(viewport, &subscriber));
			subscriber.subscribed = false;
			subscriber.registry = nullptr;
		}
		CHECK(RSG::viewport->free(viewport));
		CHECK(RSG::scene->free(camera_rid));
	}

	static void bug137_production_cancel_wakes_scene_waiting_staged_and_non_native() {
		using namespace GDExtensionDeviceExit;
		// Case 1: Staged (native_static_sealed) QUEUED && scene_waiting RECORD.
		{
			Enrollment e;
			auto c = capacity(4);
			const auto attachment = e.accept(c);
			REQUIRE(attachment.handle_serial != 0);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 339, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *a = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
				a->opened = true;
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			r->native_prepared_executor = true;
			r->native_static_sealed = true;
			r->native_stage_budget = budget(16, 16);
			r->control = {};
			r->control.serial = 21;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			r->control.record_history.admitted = true;

			// production_cancel wakes QUEUED && scene_waiting into RETURNED (returns ERR_BUSY until consumed).
			CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
			REQUIRE(r->control.state == ProductionControlState::RETURNED);
			CHECK_FALSE(r->control.scene_waiting);

			GDExtensionProductionRecordReportV3 report{};
			REQUIRE(e.registry->production_record_consume_v3(ref, 21, &report).native_error == OK);
			CHECK(report.record.result.native_error == ERR_UNAVAILABLE);
			CHECK(e.registry->production_cancel(ref).native_error == OK);
		}

		// Case 2: Non-native-executor scene_registration QUEUED && scene_waiting RECORD.
		{
			Enrollment e;
			auto c = capacity(4);
			const auto attachment = e.accept(c);
			REQUIRE(attachment.handle_serial != 0);
			Request *r = nullptr;
			const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 340, 1, 10, 11 };
			const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *a = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
				a->scene_registration = 77;
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			r->control = {};
			r->control.serial = 22;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			r->control.record_history.admitted = true;

			CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
			REQUIRE(r->control.state == ProductionControlState::RETURNED);
			CHECK_FALSE(r->control.scene_waiting);

			ProductionControlOutput out{};
			REQUIRE(e.registry->production_control_consume(ref, 22, &out) == OK);
			CHECK(out.result.native_error == ERR_UNAVAILABLE);
			CHECK(e.registry->production_cancel(ref).native_error == OK);
		}
	}

	static void bug136_mq_record_checks_recording_error_before_append() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto c = capacity(4);
		const auto attachment = e.accept(c);
		REQUIRE(attachment.handle_serial != 0);
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 341, 1, 10, 11 };
		const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		struct ReceiverState {
			Registry *registry = nullptr;
			GDExtensionDeviceSessionResultV1 res = {};
			bool called = false;
		} state{ e.registry };
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *a = nullptr;
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment, true, a, rd) == OK);
			a->opened = true;
			a->record_receiver = [](void *p_userdata, GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *) {
				auto *s = static_cast<ReceiverState *>(p_userdata);
				s->called = true;
				s->res = s->registry->production_record_material_qualification(p_context, p_req, 0);
			};
			a->record_receiver_userdata = &state;
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		uint64_t serial = 0;
		REQUIRE(e.registry->production_control_accept(ref, ProductionControlKind::RECORD, &serial) == OK);
		e.rd.qualification_frame_advance_pending = true;
		{
			Registry::Lock lock(*e.registry);
			r->control.state = ProductionControlState::QUEUED;
			e.registry->production_control_execute_locked(*r, &e.rd);
		}
		e.rd.qualification_frame_advance_pending = false;
		ProductionControlOutput out{};
		REQUIRE(e.registry->production_control_consume(ref, serial, &out) == OK);

		CHECK(state.called);
		CHECK(state.res.native_error == ERR_BUSY);
		CHECK_FALSE(r->frozen);
		CHECK_FALSE(r->append_effect_entered);
		CHECK_FALSE(r->effects_pending);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
	}

	static void bug138c_trim_idle_staging_buffers_and_transfer_worker() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.rd.frames.resize(2);
		e.rd.upload_staging_buffers.block_size = 64u;
		e.rd.upload_staging_buffers.max_size = 1024u;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		e.rd.upload_staging_buffers.current = 0;
		e.rd.upload_staging_buffers.used = false;

		e.rd.download_staging_buffers.block_size = 64u;
		e.rd.download_staging_buffers.max_size = 1024u;
		e.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		e.rd.download_staging_buffers.current = 0;
		e.rd.download_staging_buffers.used = false;

		for (int i = 0; i < 8; ++i) {
			REQUIRE(e.rd._insert_staging_block(e.rd.upload_staging_buffers) == OK);
			e.rd.upload_staging_buffers.blocks.write[0].frame_used = 1;
			REQUIRE(e.rd._insert_staging_block(e.rd.download_staging_buffers) == OK);
			e.rd.download_staging_buffers.blocks.write[0].frame_used = 1;
		}
		e.rd.upload_staging_buffers.current = 3;
		e.rd.download_staging_buffers.current = 4;
		REQUIRE(e.rd.upload_staging_buffers.blocks.size() == 8);
		REQUIRE(e.rd.download_staging_buffers.blocks.size() == 8);

		RenderingDevice::TransferWorker *w_idle = memnew(RenderingDevice::TransferWorker);
		w_idle->index = 0;
		w_idle->staging_buffer = e.driver.buffer_create(64u, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 1);
		w_idle->staging_buffer_size_allocated = 8u * 1024u * 1024u;
		w_idle->max_transfer_size = 8u * 1024u * 1024u;
		REQUIRE(w_idle->staging_buffer.id != 0);

		RenderingDevice::TransferWorker *w_busy = memnew(RenderingDevice::TransferWorker);
		w_busy->index = 1;
		w_busy->submitted = true;
		w_busy->staging_buffer = e.driver.buffer_create(64u, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 1);
		w_busy->staging_buffer_size_allocated = 8u * 1024u * 1024u;
		w_busy->max_transfer_size = 8u * 1024u * 1024u;
		REQUIRE(w_busy->staging_buffer.id != 0);

		e.rd.transfer_worker_pool.resize(2);
		e.rd.transfer_worker_pool[0] = w_idle;
		e.rd.transfer_worker_pool[1] = w_busy;
		e.rd.transfer_worker_pool_size = 2;

		const uint32_t frees_before = e.driver.buffer_frees;
		// 1. Before 8 idle frames past ring size (frames_drawn = 5 < 1 + 2 + 8): nothing trimmed.
		e.rd.frames_drawn = 5;
		e.rd._trim_idle_staging_buffers();
		CHECK(e.rd.upload_staging_buffers.blocks.size() == 8);
		CHECK(e.rd.download_staging_buffers.blocks.size() == 8);
		CHECK(w_idle->staging_buffer.id != 0);
		CHECK(w_idle->idle_frames == 1);
		CHECK(e.driver.buffer_frees == frees_before);

		// 2. With a pending download readback on frame 0 and frames_drawn = 20:
		//    upload staging trims to floor (2 blocks), download staging MUST NOT trim.
		e.rd.frames[0].download_buffer_staging_buffers.push_back(e.rd.download_staging_buffers.blocks[0].driver_id);
		e.rd.frames_drawn = 20;
		e.rd._trim_idle_staging_buffers();
		CHECK(e.rd.upload_staging_buffers.blocks.size() == 2);
		CHECK(e.rd.upload_staging_buffers.current >= 0);
		CHECK(e.rd.upload_staging_buffers.current < e.rd.upload_staging_buffers.blocks.size());
		CHECK(e.rd.download_staging_buffers.blocks.size() == 8);
		CHECK(e.driver.buffer_frees == frees_before + 6);

		// 3. Once download readbacks drain, download staging trims cleanly to floor (2 blocks).
		e.rd.frames[0].download_buffer_staging_buffers.clear();
		e.rd._trim_idle_staging_buffers();
		CHECK(e.rd.download_staging_buffers.blocks.size() == 2);
		CHECK(e.rd.download_staging_buffers.current >= 0);
		CHECK(e.rd.download_staging_buffers.current < e.rd.download_staging_buffers.blocks.size());
		CHECK(e.driver.buffer_frees == frees_before + 12);

		// 4. Advance idle frames to trigger idle TransferWorker staging trim (> 8 frames),
		//    while submitted TransferWorker keeps its buffer intact.
		for (int step = 0; step < 10; ++step) {
			e.rd._trim_idle_staging_buffers();
		}
		CHECK(w_idle->staging_buffer.id == 0);
		CHECK(w_idle->staging_buffer_size_allocated == 0);
		CHECK(w_idle->max_transfer_size == 0);
		CHECK(w_busy->staging_buffer.id != 0);
		CHECK(w_busy->staging_buffer_size_allocated == 8u * 1024u * 1024u);
		CHECK(e.driver.buffer_frees == frees_before + 13);

		e.driver.buffer_free(w_busy->staging_buffer);
		w_busy->staging_buffer = RDD::BufferID();
		e.rd.transfer_worker_pool.clear();
		e.rd.transfer_worker_pool_size = 0;
		memdelete(w_idle);
		memdelete(w_busy);
	}

	static void bug138c_arena_pod_reclaim_on_idle_and_reuse() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(16);
		pool_cap.requests = 4;
		pool_cap.readbacks = 8;
		pool_cap.retained_payload_bytes = 256u * 1024u;
		pool_cap.readback_bytes = 128u * 1024u;
		const auto attachment_ref = e.accept(pool_cap);
		REQUIRE(attachment_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
			att->scene_prefix_bytes = 64;
			memset(att->payload, 0x5A, 64);
		}
		REQUIRE(att != nullptr);

		auto req_cap = capacity(4);
		req_cap.requests = 1;
		req_cap.readbacks = 1;
		req_cap.retained_payload_bytes = 96u * 1024u;
		req_cap.readback_bytes = 64u * 1024u;
		const GDExtensionProductionActionV2 actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};
		Request *r1 = nullptr;
		Request *r2 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op1{ production_dto_abi<GDExtensionProductionOperationV1>(), 801, 1, 10, 12 };
			const GDExtensionProductionOperationV1 op2{ production_dto_abi<GDExtensionProductionOperationV1>(), 802, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op1, req_cap, { nullptr, actions }, 2, nullptr, 0, nullptr, 0, {}, 0, r1, 2) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op2, req_cap, { nullptr, actions }, 2, nullptr, 0, nullptr, 0, {}, 0, r2, 2) == OK);
		}
		REQUIRE(r1 != nullptr);
		REQUIRE(r2 != nullptr);
		REQUIRE(att->children == 2);
		memset(att->payload + r1->payload_offset, 0xA1, req_cap.retained_payload_bytes);
		memset(att->readback_bytes + r1->readback_offset, 0xB2, req_cap.readback_bytes);

		// Releasing r1 while r2 is still live (children == 1) must NOT reclaim yet.
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		REQUIRE(e.registry->production_cancel(ref1).native_error == OK);
		REQUIRE(e.registry->production_release(ref1).native_error == OK);
		CHECK(att->children == 1);
		CHECK(att->arena_reclaimed_bytes_total == 0);

		// Releasing r2 drops children to 0. Under BUG-170, transient wave
		// boundaries must NOT immediately call MADV_FREE_REUSABLE while active.
		REQUIRE(e.registry->production_cancel(ref2).native_error == OK);
		REQUIRE(e.registry->production_release(ref2).native_error == OK);
		CHECK(att->children == 0);
		CHECK(att->arena_reclaimed_bytes_total == 0);
		CHECK(att->payload_reclaim_hwm > att->scene_prefix_bytes);
		CHECK(att->readback_reclaim_hwm > 0);
		CHECK(att->natives_reclaim_hwm > 0);
		CHECK(att->mq_reclaim_hwm > 0);

		// Sustained idle cooldown (ARENA_IDLE_RECLAIM_FRAMES) triggers POD/MQ reclaim.
		for (uint32_t f = 0; f + 1 < ProductionAttachment::ARENA_IDLE_RECLAIM_FRAMES; ++f) {
			e.registry->production_observe_completed_frames_rt(e.rd);
		}
		CHECK(att->arena_reclaimed_bytes_total == 0);
		e.registry->production_observe_completed_frames_rt(e.rd);
		CHECK(att->payload_reclaim_hwm == 0);
		CHECK(att->readback_reclaim_hwm == 0);
		CHECK(att->natives_reclaim_hwm == 0);
		CHECK(att->mq_reclaim_hwm == 0);
#if defined(__APPLE__)
		CHECK(att->arena_reclaimed_bytes_total > 0);
#endif
		// Persistent scene_prefix_bytes at payload[0..64) must be untouched.
		for (size_t i = 0; i < 64; ++i) {
			CHECK(att->payload[i] == 0x5A);
		}

		// Subsequent request allocation, read/write, and release must succeed cleanly.
		Request *r3 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op3{ production_dto_abi<GDExtensionProductionOperationV1>(), 803, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op3, req_cap, { nullptr, actions }, 2, nullptr, 0, nullptr, 0, {}, 0, r3, 2) == OK);
		}
		REQUIRE(r3 != nullptr);
		memset(att->payload + r3->payload_offset, 0xC3, 4096);
		CHECK(att->payload[r3->payload_offset + 4095] == 0xC3);
		memset(att->readback_bytes + r3->readback_offset, 0xD4, 4096);
		CHECK(att->readback_bytes[r3->readback_offset + 4095] == 0xD4);
		CHECK(r3->natives[0].owner_index == 0);
		const GDExtensionProductionRequestRefV1 ref3{ Enrollment::INSTANCE, r3->serial };
		REQUIRE(e.registry->production_cancel(ref3).native_error == OK);
		REQUIRE(e.registry->production_release(ref3).native_error == OK);
		CHECK(att->children == 0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
	}

	static void pillar1_persistent_set_cache_hit_pingpong_evict_and_close() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.allow_second_set = true;
		e.driver.complete = true;
		auto c = capacity(8);
		c.retained_payload_bytes = 64;
		auto pool = c;
		pool.requests = 2;
		pool.suboperations *= 2;
		pool.resources *= 2;
		pool.resource_uses *= 2;
		pool.graph_memberships *= 2;
		pool.native_memberships *= 2;
		pool.return_cells *= 2;
		pool.retained_payload_bytes *= 2;
		const auto attachment_ref = e.accept(pool);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		// Create QualificationBridge with 1 NativeUnit + 2 retained Storage buffers (A & B).
		e.original_bridge = memnew(Registry::QualificationBridge);
		e.original_bridge->serial = 401;
		e.original_bridge->epoch = att->epoch;
		e.original_bridge->library = att->library;
		e.original_bridge->device = att->device;
		e.registry->qualification_bridges[0] = e.original_bridge;

		auto make_retained_storage = [&](uint32_t slot_idx, uint64_t serial, uint32_t alloc_slot) -> Registry::QualificationBridge::Storage * {
			auto *st = memnew(Registry::QualificationBridge::Storage);
			st->serial = serial;
			st->creating = false;
			st->origin = { Enrollment::ABI, 128, alloc_slot, 0, 1, 1, 0 };
			st->physical.bridge = e.original_bridge->serial;
			st->physical.child = st->serial;
			e.original_bridge->storage[slot_idx] = st;
			const RID rid = e.rd._qualification_storage_create(128, &st->physical);
			REQUIRE(rid.is_valid());
			const auto *buf = e.rd.storage_buffer_owner.get_or_null(rid);
			REQUIRE(buf != nullptr);
			st->physical.rid = rid.get_id();
			st->backing = { Enrollment::ABI, { Enrollment::INSTANCE, st->serial }, rid.get_id(), buf->backing_serial, buf->size, uint64_t(buf->usage), OK, 1 };
			return st;
		};
		auto *storage_a = make_retained_storage(0, 402, 10);
		auto *storage_b = make_retained_storage(1, 403, 11);
		e.original_storage = storage_a;

		auto reserve_request = [&](uint64_t op_serial, uint32_t alloc_slot, Registry::QualificationBridge::Storage *st, bool init_unit) -> Request * {
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), op_serial, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, alloc_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			};
			const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_reserve_metadata_locked(*att, op, c, &action, 1, resources, 3, &use, 1, nullptr, 0, r) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			if (init_unit) {
				REQUIRE(import_original_unit(e, *r));
			} else {
				REQUIRE(e.registry->production_import_unit(ref, 0, &e.original_unit->backing).native_error == OK);
			}
			REQUIRE(e.registry->production_import_storage(ref, 1, &st->backing).native_error == OK);
			return r;
		};

		const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		const auto release_req = [&](Request *r) {
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			REQUIRE(e.registry->production_cancel(ref).native_error == OK);
			const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
			REQUIRE(released.owner_return.native_error == OK);
			CHECK(r->serial == 0);
		};

		// 1. Frame 1 (parity A) — cold MISS populates persistent set cache.
		Request *r1 = reserve_request(601, 10, storage_a, true);
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		REQUIRE(e.registry->production_set_create(ref1, 2, 0, 0, &binding, 1).native_error == OK);
		CHECK(r1->control.serial != 0); // Cold miss used RT SET_CREATE control.
		CHECK(e.driver.set_creates == 1);
		CHECK(r1->resources[2].cached_persistent_set);
		CHECK_FALSE(r1->returns[2].pending);
		const RID cached_rid_a = r1->resources[2].primary;
		const uint64_t cached_serial_a = r1->resources[2].backing_serial;
		REQUIRE(cached_rid_a.is_valid());
		CHECK(att->persistent_set_lookup.size() == 1);
		CHECK(att->persistent_sets.size() == 1);
		CHECK(storage_a->physical.dependent_children.load() == 1);
		CHECK(e.original_unit->shader.dependent_children.load() == 1);
		release_req(r1);
		CHECK(e.driver.set_frees == 0);

		// 2. Frame 2 (parity A) — warm Main-thread HIT (ZERO control, ZERO driver set_create!).
		Request *r2 = reserve_request(602, 10, storage_a, false);
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		REQUIRE(e.registry->production_set_create(ref2, 2, 0, 0, &binding, 1).native_error == OK);
		CHECK(r2->control.serial == 0); // Zero RT SET_CREATE control accepted!
		CHECK(e.driver.set_creates == 1);
		CHECK(r2->resources[2].primary == cached_rid_a);
		CHECK(r2->resources[2].backing_serial == cached_serial_a);
		CHECK(r2->resources[2].cached_persistent_set);
		CHECK_FALSE(r2->returns[2].pending);
		const uint32_t push2[4] = { 1, 2, 3, 4 };
		const GDExtensionProductionDispatchV1 disp2 = { production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 2, 1, 1, 1, 1, sizeof(push2), 0 };
		REQUIRE(e.registry->production_prepare_dispatch(ref2, &disp2, push2, sizeof(push2)).native_error == OK);
		const auto disp_check = [&]() {
			MutexLock lock(e.registry->mutex);
			return e.registry->production_dispatch_closure_locked(*r2, e.rd, r2->actions[0], r2->actions[0].dispatch);
		};
		CHECK(disp_check() == OK);
		release_req(r2);
		CHECK(e.driver.set_frees == 0);

		// 3. Frame 3 (parity B) — cold MISS populates 2nd cache entry; Frames 4 & 5 alternate A/B with 0 creates.
		Request *r3 = reserve_request(603, 11, storage_b, false);
		const GDExtensionProductionRequestRefV1 ref3{ Enrollment::INSTANCE, r3->serial };
		REQUIRE(e.registry->production_set_create(ref3, 2, 0, 0, &binding, 1).native_error == OK);
		CHECK(r3->control.serial != 0); // Cold miss for parity B used RT SET_CREATE control.
		CHECK(e.driver.set_creates == 2);
		CHECK(att->persistent_set_lookup.size() == 2);
		const RID cached_rid_b = r3->resources[2].primary;
		REQUIRE(cached_rid_b.is_valid());
		CHECK(cached_rid_b != cached_rid_a);
		release_req(r3);

		for (uint32_t iter = 0; iter < 4; ++iter) {
			const bool use_a = (iter % 2) == 0;
			Request *rk = reserve_request(610 + iter, use_a ? 10 : 11, use_a ? storage_a : storage_b, false);
			const GDExtensionProductionRequestRefV1 refk{ Enrollment::INSTANCE, rk->serial };
			REQUIRE(e.registry->production_set_create(refk, 2, 0, 0, &binding, 1).native_error == OK);
			CHECK(rk->control.serial == 0);
			CHECK(rk->resources[2].primary == (use_a ? cached_rid_a : cached_rid_b));
			release_req(rk);
		}
		CHECK(e.driver.set_creates == 2);
		CHECK(e.driver.set_frees == 0);

		// 4. Two-state eviction while in-flight request pins parity A, then automatic RT retire on qualification_retire_step.
		Request *r_pin = reserve_request(620, 10, storage_a, false);
		const GDExtensionProductionRequestRefV1 ref_pin{ Enrollment::INSTANCE, r_pin->serial };
		REQUIRE(e.registry->production_set_create(ref_pin, 2, 0, 0, &binding, 1).native_error == OK);
		CHECK(r_pin->control.serial == 0);
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_evict_for_dependency_rt(&e.rd, &storage_a->physical);
		}
		// Evicted from lookup, but NOT freed or use_closed while r_pin holds production_uses!
		CHECK(att->persistent_set_lookup.size() == 1);
		CHECK(att->persistent_sets.size() == 2);
		CHECK(e.driver.set_frees == 0);
		PersistentSetEntry *entry_a = nullptr;
		for (uint32_t pi = 0; pi < att->persistent_sets.size(); ++pi) {
			if (att->persistent_sets[pi] && att->persistent_sets[pi]->rid == cached_rid_a) {
				entry_a = att->persistent_sets[pi];
				break;
			}
		}
		REQUIRE(entry_a != nullptr);
		CHECK(entry_a->evicted);
		CHECK_FALSE(entry_a->physical.use_closed.load());
		release_req(r_pin);
		// Now unpinned: qualification_retire_step on retiring storage_a evicts & frees cached_rid_a automatically!
		storage_a->retiring = true;
		e.registry->qualification_retire_step(e.original_bridge, &e.rd, storage_a->serial, 1);
		e.rd._free_pending_resources(0);
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
		}
		CHECK(e.driver.set_frees == 1);
		CHECK(storage_a->physical.dependent_children.load() == 0);
		CHECK(att->persistent_sets.size() == 1);
		storage_a->retiring = false;

		// 5. production_close drains & frees remaining persistent set B cleanly!
		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.driver.set_frees == 2);
		CHECK(storage_b->physical.dependent_children.load() == 0);
		CHECK(e.original_unit->shader.dependent_children.load() == 0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		CHECK(e.rd.production_entity_holds == nullptr);

		// Clean up storage_a and storage_b cleanly (zero RID leaks).
		for (auto *st : { storage_a, storage_b }) {
			if (!st->physical.enqueued && !st->physical.free_event) {
				st->physical.use_closed.store(true);
				e.rd.qualification_retiring = true;
				e.rd.free_rid(RID::from_uint64(st->backing.rid));
				e.rd.qualification_retiring = false;
				e.rd._free_pending_resources(0);
			}
			CHECK(st->physical.free_event != 0);
			memdelete(st);
		}
		e.original_storage = nullptr;
		e.original_bridge->storage[0] = nullptr;
		e.original_bridge->storage[1] = nullptr;
	}

	static void pillar1_observe_fast_path_and_passive_rt_harvest() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = false;
		auto c = capacity(8);
		c.retained_payload_bytes = 64;
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 701, 1, 10, 11 };
		const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
		};
		const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, &action }, 1, resources, 3, &use, 1, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(import_original_unit(e, *r));

		e.original_storage = memnew(Registry::QualificationBridge::Storage);
		auto &st = *e.original_storage;
		st.serial = 702;
		st.creating = false;
		st.origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
		st.physical.bridge = e.original_bridge->serial;
		st.physical.child = st.serial;
		e.original_bridge->storage[0] = &st;
		const RID srid = e.rd._qualification_storage_create(128, &st.physical);
		REQUIRE(srid.is_valid());
		const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
		REQUIRE(sbuf != nullptr);
		st.physical.rid = srid.get_id();
		st.backing = { Enrollment::ABI, { Enrollment::INSTANCE, st.serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
		REQUIRE(e.registry->production_import_storage(ref, 1, &st.backing).native_error == OK);

		const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
		REQUIRE(r->resources[2].cached_persistent_set);

		const uint32_t push[4] = { 7, 11, 13, 17 };
		const GDExtensionProductionDispatchV2 dispatch = { production_dto_abi<GDExtensionProductionDispatchV2>(), 0, 10, 0, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
		REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
		REQUIRE(effect(e, *r, 0).state == CheckedRecordAppend::State::Appended);
		GDExtensionProductionSegmentV2 seg = {};
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_close_segment_locked(*r, seg) == OK);
		}
		REQUIRE(seg.segment != 0);
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_ack_segment_locked(*r, seg.segment, seg.segment_revision) == OK);
		}
		execute_transfers(e, *r, false);
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_retain_sources_locked(e.rd, 0, e.rd.frames[0].qualification_epoch) == OK);
		}
		REQUIRE(r->native_count >= 1);
		REQUIRE(r->natives[0].member.retained);
		CHECK(r->natives[0].observation.state != NativeSourceObservation::State::Complete);

		// 1. While GPU fence is still pending, nonzero OBSERVE must NOT fast-path.
		const auto nonzero = budget(4, 4);
		GDExtensionProductionReportV2 rep = {};
		const uint64_t ctrl_before = r->control.serial;
		const auto pending_obs = e.registry->production_observe_v2(ref, &nonzero, &rep);
		CHECK(pending_obs.native_error == ERR_UNAVAILABLE);
		CHECK(r->control.serial > ctrl_before);
		CHECK(rep.complete_obligations == 0);
		const auto pending_exec = execute_public_control(e, ref, ProductionControlKind::OBSERVE, 0);
		CHECK((pending_exec.result.native_error == OK || pending_exec.result.native_error == ERR_BUSY));
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(r->natives[0].observation.state != NativeSourceObservation::State::Complete);

		// 2. GPU completes -> _begin_frame passively harvests completed frame obligations on RT!
		e.driver.complete = true;
		e.driver.expected_begin_frame = 0;
		REQUIRE(e.rd._begin_frame(false, true) == OK);
		CHECK(r->natives[0].observation.state == NativeSourceObservation::State::Complete);

		// 3. Main-thread OBSERVE fast-path returns synchronous OK with ZERO new RT control cell!
		const uint64_t ctrl_after_harvest = r->control.serial;
		GDExtensionProductionReportV2 settled = {};
		const auto fast_obs = e.registry->production_observe_v2(ref, &nonzero, &settled);
		CHECK(fast_obs.native_error == OK);
		CHECK(r->control.serial == ctrl_after_harvest); // Zero new OBSERVE control accepted!
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(settled.cause.native_error == OK);
		CHECK(settled.recorded_action_end == 1);
		CHECK(settled.possible_obligations == 0);
		CHECK(settled.known_obligations == settled.complete_obligations);
		CHECK(settled.known_obligations >= 1);
		CHECK(settled.physical_returns_pending == 0);

		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		REQUIRE(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		st.physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(st.backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
		CHECK(st.physical.free_event != 0);
	}

	static void pillar1_passive_harvest_while_control_returned_and_other_frame_readback() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = false;
		auto c = capacity(8);
		c.retained_payload_bytes = 64;
		c.readback_bytes = 16;
		c.readbacks = 1;
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);
		e.rd.frames.resize(2);
		e.rd.frames[1].qualification_epoch = 2;
		e.rd.frames[1].fence = RDD::FenceID(93);
		e.rd.frames[1].command_buffer = RDD::CommandBufferID(92);
		e.rd.frames[1].command_pool = RDD::CommandPoolID(96);
		e.rd.next_qualification_frame_epoch = 3;
		e.rd.download_staging_buffers.block_size = 128;
		e.rd.download_staging_buffers.max_size = 256;
		e.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		REQUIRE(e.rd._insert_staging_block(e.rd.download_staging_buffers) == OK);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 711, 1, 10, 12 };
		const GDExtensionProductionActionV2 actions[] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
		};
		const GDExtensionProductionResourceV1 resources[] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
		};
		const GDExtensionProductionUseV1 uses[] = {
			{ 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 },
		};
		const GDExtensionProductionReadbackV2 readback = { 1, 0, 0, 1, 0, 16 };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, actions }, 2, resources, 3, uses, 2, { nullptr, &readback, 0, nullptr }, 1, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(import_original_unit(e, *r));

		e.original_storage = memnew(Registry::QualificationBridge::Storage);
		auto &st = *e.original_storage;
		st.serial = 712;
		st.creating = false;
		st.origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
		st.physical.bridge = e.original_bridge->serial;
		st.physical.child = st.serial;
		e.original_bridge->storage[0] = &st;
		const RID srid = e.rd._qualification_storage_create(128, &st.physical);
		REQUIRE(srid.is_valid());
		const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
		REQUIRE(sbuf != nullptr);
		st.physical.rid = srid.get_id();
		st.backing = { Enrollment::ABI, { Enrollment::INSTANCE, st.serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
		REQUIRE(e.registry->production_import_storage(ref, 1, &st.backing).native_error == OK);

		const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
		REQUIRE(r->resources[2].cached_persistent_set);

		const uint32_t push[4] = { 7, 11, 13, 17 };
		const GDExtensionProductionDispatchV2 dispatch = { production_dto_abi<GDExtensionProductionDispatchV2>(), 0, 10, 0, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
		REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
		REQUIRE(e.registry->production_prepare_readback_v2(ref, 1, 0).native_error == OK);
		REQUIRE(effect(e, *r, 0).state == CheckedRecordAppend::State::Appended);
		REQUIRE(effect(e, *r, 1).state == CheckedRecordAppend::State::Appended);
		GDExtensionProductionSegmentV2 seg = {};
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_close_segment_locked(*r, seg) == OK);
			REQUIRE(e.registry->production_ack_segment_locked(*r, seg.segment, seg.segment_revision) == OK);
		}
		execute_transfers(e, *r, false);
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_retain_sources_locked(e.rd, 0, e.rd.frames[0].qualification_epoch) == OK);
		}
		REQUIRE(e.rd.frames[0].download_buffer_get_data_requests.size() == 1);
		REQUIRE(e.rd.frames[0].fence_signaled);
		REQUIRE(r->native_count >= 1);
		REQUIRE(r->natives[0].member.retained);
		CHECK_FALSE(r->readbacks[0].source.delivered);

		// Put control into RETURNED (unconsumed OBSERVE #1 while GPU was still pending — exact live Metal v22 scenario!).
		const auto nonzero = budget(4, 4);
		GDExtensionProductionReportV2 rep = {};
		CHECK(e.registry->production_observe_v2(ref, &nonzero, &rep).native_error == ERR_UNAVAILABLE);
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(r->control.state == ProductionControlState::PENDING);
			r->control.state = ProductionControlState::QUEUED;
			e.registry->production_control_execute_locked(*r, &e.rd);
			REQUIRE(r->control.state == ProductionControlState::RETURNED);
		}
		CHECK(r->natives[0].observation.state != NativeSourceObservation::State::Complete);
		CHECK_FALSE(r->readbacks[0].source.delivered);

		// GPU completes while RD advances to frame 1 (frame 0 still holds fence + readback!).
		e.driver.complete = true;
		e.rd.frame = 1;
		e.driver.expected_begin_frame = 1;
		REQUIRE(e.rd._begin_frame(false, true) == OK);

		// Passive RT harvest MUST have completed BOTH natives[] (despite control == RETURNED!) AND frame 0's readback!
		CHECK(r->natives[0].observation.state == NativeSourceObservation::State::Complete);
		CHECK(r->readbacks[0].source.delivered);
		CHECK(r->readbacks[0].source.returned == r->readbacks[0].source.issued);
		CHECK_FALSE(e.rd.frames[0].fence_signaled);
		CHECK(e.rd.frames[0].download_buffer_get_data_requests.is_empty());

		// Consuming OBSERVE #1 projects freshly harvested 100% settled report immediately!
		GDExtensionProductionReportV2 after_consume = {};
		const auto consume_obs = e.registry->production_observe_v2(ref, &nonzero, &after_consume);
		CHECK((consume_obs.native_error == OK || consume_obs.native_error == ERR_BUSY));
		CHECK(r->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(after_consume.possible_obligations == 0);
		CHECK(after_consume.known_obligations == after_consume.complete_obligations);
		CHECK(after_consume.ready_readbacks == 1);

		// Follow-up OBSERVE hits Main-thread fast-path with ZERO new RT control cell!
		const uint64_t ctrl_before_fast = r->control.serial;
		GDExtensionProductionReportV2 fast_rep = {};
		CHECK(e.registry->production_observe_v2(ref, &nonzero, &fast_rep).native_error == OK);
		CHECK(r->control.serial == ctrl_before_fast);
		CHECK(fast_rep.ready_readbacks == 1);

		// Main-thread RELEASE fast-path releases cleanly in 1 synchronous call!
		GDExtensionProductionServiceBudgetV1 rem = {};
		GDExtensionProductionReportV2 rel_rep = {};
		CHECK(e.registry->production_release_v2(ref, &nonzero, &rem, &rel_rep).native_error == OK);
		CHECK(rel_rep.service_control_serial == 0);
		CHECK(rel_rep.service_pending == 0);
		CHECK(r->serial == 0);
		CHECK(att->children == 0);

		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(1);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		st.physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(st.backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(1);
		e.rd._free_pending_resources(0);
		CHECK(st.physical.free_event != 0);
	}

	static void pillar1_release_main_fast_path_and_rt_guard() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.complete = true;
		auto c = capacity(8);
		c.retained_payload_bytes = 64;
		auto pool = c;
		pool.requests = 2;
		pool.suboperations *= 2;
		pool.resources *= 2;
		pool.resource_uses *= 2;
		pool.graph_memberships *= 2;
		pool.native_memberships *= 2;
		pool.return_cells *= 2;
		pool.retained_payload_bytes *= 2;
		const auto attachment_ref = e.accept(pool);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto reserve_and_complete = [&](uint64_t op_serial, bool init_unit_and_storage) -> Request * {
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), op_serial, 1, 10, 11 };
			const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			};
			const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, &action }, 1, resources, 3, &use, 1, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			if (init_unit_and_storage) {
				REQUIRE(import_original_unit(e, *r));
				e.original_storage = memnew(Registry::QualificationBridge::Storage);
				auto &st = *e.original_storage;
				st.serial = 722;
				st.creating = false;
				st.origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
				st.physical.bridge = e.original_bridge->serial;
				st.physical.child = st.serial;
				e.original_bridge->storage[0] = &st;
				const RID srid = e.rd._qualification_storage_create(128, &st.physical);
				REQUIRE(srid.is_valid());
				const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
				REQUIRE(sbuf != nullptr);
				st.physical.rid = srid.get_id();
				st.backing = { Enrollment::ABI, { Enrollment::INSTANCE, st.serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
			} else {
				REQUIRE(e.registry->production_import_unit(ref, 0, &e.original_unit->backing).native_error == OK);
			}
			REQUIRE(e.registry->production_import_storage(ref, 1, &e.original_storage->backing).native_error == OK);
			const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
			REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
			REQUIRE(r->resources[2].cached_persistent_set);
			const uint32_t push[4] = { 7, 11, 13, 17 };
			const GDExtensionProductionDispatchV2 dispatch = { production_dto_abi<GDExtensionProductionDispatchV2>(), 0, 10, 0, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
			REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
			REQUIRE(effect(e, *r, 0).state == CheckedRecordAppend::State::Appended);
			GDExtensionProductionSegmentV2 seg = {};
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_close_segment_locked(*r, seg) == OK);
				REQUIRE(e.registry->production_ack_segment_locked(*r, seg.segment, seg.segment_revision) == OK);
			}
			const uint32_t fidx = e.rd.frame;
			execute_transfers(e, *r, false);
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_retain_sources_locked(e.rd, fidx, e.rd.frames[fidx].qualification_epoch) == OK);
			}
			e.driver.expected_begin_frame = fidx;
			REQUIRE(e.rd._begin_frame(false, true) == OK);
			REQUIRE(r->natives[0].observation.state == NativeSourceObservation::State::Complete);
			return r;
		};

		// 1. Settled PersistentSetEntry request completes RELEASE synchronously on Main (ZERO RT turn!).
		Request *r1 = reserve_and_complete(731, true);
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		const auto offered = budget(8, 8);
		GDExtensionProductionServiceBudgetV1 rem = {};
		GDExtensionProductionReportV2 rel_rep = {};
		const uint64_t nat_serial = r1->natives[0].member.source.serial;
		REQUIRE(nat_serial < 128);
		const uint32_t releases_before = e.driver.releases[nat_serial];
		CHECK(e.registry->production_release_v2(ref1, &offered, &rem, &rel_rep).native_error == OK);
		CHECK(rel_rep.service_control_serial == 0);
		CHECK(rel_rep.service_pending == 0);
		CHECK(rel_rep.cause.native_error == OK);
		CHECK(r1->serial == 0);
		CHECK(att->children == 0);
		CHECK(e.driver.releases[nat_serial] == releases_before + 1);
		CHECK(rem.controls == offered.controls - 1);
		CHECK(rem.receipts == offered.receipts - 1);

		// 2. Request with RT-only custody (unreturned preinput host transform) MUST NOT bypass RT RELEASE!
		Request *r2 = reserve_and_complete(732, false);
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		r2->actions[0].preinput_host_prepared = true;
		r2->actions[0].preinput_host_returned = false;
		GDExtensionProductionServiceBudgetV1 rem2 = {};
		GDExtensionProductionReportV2 rel_rep2 = {};
		CHECK(e.registry->production_release_v2(ref2, &offered, &rem2, &rel_rep2).native_error == ERR_BUSY);
		CHECK(rel_rep2.service_control_serial != 0);
		CHECK(rel_rep2.service_pending == 1);
		CHECK(r2->serial == ref2.handle_serial);
		CHECK(r2->control.state == ProductionControlState::PENDING);
		CHECK(r2->control.input.kind == ProductionControlKind::RELEASE);
		// Execute the RT RELEASE turn cleanly and consume.
		{
			MutexLock lock(e.registry->mutex);
			r2->control.state = ProductionControlState::QUEUED;
			e.registry->production_control_execute_locked(*r2, &e.rd);
		}
		CHECK(r2->actions[0].preinput_host_returned);
		CHECK(e.registry->production_release_v2(ref2, &offered, &rem2, &rel_rep2).native_error == OK);
		CHECK(r2->serial == 0);
		CHECK(att->children == 0);

		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		auto &st = *e.original_storage;
		st.physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(st.backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
		CHECK(st.physical.free_event != 0);
	}

	static void bug145_persistent_sets_by_dep_ok_eviction_and_clean_close() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.allow_second_set = true;
		auto c = capacity(4);
		c.resources = 4;
		c.resource_uses = 8;
		c.graph_memberships = 4;
		c.native_memberships = 8;
		c.return_cells = 4;
		auto pool = c;
		pool.requests = 4;
		pool.suboperations *= 4;
		pool.resources *= 4;
		pool.resource_uses *= 4;
		pool.graph_memberships *= 4;
		pool.native_memberships *= 4;
		pool.return_cells *= 4;
		pool.retained_payload_bytes *= 4;
		const auto attachment_ref = e.accept(pool);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto make_storage = [&](uint64_t serial) -> Registry::QualificationBridge::Storage * {
			auto *st = memnew(Registry::QualificationBridge::Storage);
			st->serial = serial;
			st->creating = false;
			st->origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
			st->physical.bridge = e.original_bridge->serial;
			st->physical.child = st->serial;
			const RID srid = e.rd._qualification_storage_create(128, &st->physical);
			REQUIRE(srid.is_valid());
			const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
			REQUIRE(sbuf != nullptr);
			st->physical.rid = srid.get_id();
			st->backing = { Enrollment::ABI, { Enrollment::INSTANCE, st->serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
			return st;
		};
		Registry::QualificationBridge::Storage *storage_a = nullptr;
		Registry::QualificationBridge::Storage *storage_b = nullptr;

		auto populate_set = [&](uint64_t op_serial, Registry::QualificationBridge::Storage *&st, bool init_unit, uint64_t st_serial) {
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), op_serial, 1, 10, 11 };
			const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			};
			const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, &action }, 1, resources, 3, &use, 1, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			if (init_unit) {
				REQUIRE(import_original_unit(e, *r));
			} else {
				REQUIRE(e.registry->production_import_unit(ref, 0, &e.original_unit->backing).native_error == OK);
			}
			if (!st) {
				st = make_storage(st_serial);
				if (init_unit) {
					e.original_bridge->storage[0] = st;
					e.original_storage = st;
				} else {
					e.original_bridge->storage[1] = st;
				}
			}
			REQUIRE(e.registry->production_import_storage(ref, 1, &st->backing).native_error == OK);
			const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
			REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
			REQUIRE(r->resources[2].cached_persistent_set);
			REQUIRE(e.registry->production_cancel(ref).native_error == OK);
			REQUIRE(e.registry->production_release(ref).native_error == OK);
		};

		populate_set(8410, storage_a, true, 841);
		populate_set(8411, storage_b, false, 842);

		// Verify O(k) reverse dependency index mirrors both storages + shared shader.
		{
			MutexLock lock(e.registry->mutex);
			auto *bucket_a = att->persistent_sets_by_dep.getptr(&storage_a->physical);
			auto *bucket_b = att->persistent_sets_by_dep.getptr(&storage_b->physical);
			auto *bucket_sh = att->persistent_sets_by_dep.getptr(&e.original_unit->shader);
			REQUIRE(bucket_a != nullptr);
			REQUIRE(bucket_b != nullptr);
			REQUIRE(bucket_sh != nullptr);
			CHECK(bucket_a->size() == 1);
			CHECK(bucket_b->size() == 1);
			CHECK(bucket_sh->size() == 2);
		}

		// Evict ONLY storage_a via O(k) bucket lookup: storage_b entry MUST stay live and indexed.
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_evict_for_dependency_rt(&e.rd, &storage_a->physical);
		}
		e.rd._free_pending_resources(0);
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
		}
		CHECK(e.driver.set_frees == 1);
		CHECK(att->persistent_sets.size() == 1);
		CHECK(att->persistent_set_lookup.size() == 1);
		{
			MutexLock lock(e.registry->mutex);
			CHECK(att->persistent_sets_by_dep.getptr(&storage_a->physical) == nullptr);
			auto *bucket_b = att->persistent_sets_by_dep.getptr(&storage_b->physical);
			auto *bucket_sh = att->persistent_sets_by_dep.getptr(&e.original_unit->shader);
			REQUIRE(bucket_b != nullptr);
			REQUIRE(bucket_sh != nullptr);
			CHECK(bucket_b->size() == 1);
			CHECK(bucket_sh->size() == 1);
		}

		// Clean attachment close drains remaining set and empties persistent_sets_by_dep.
		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		CHECK(e.driver.set_frees == 2);

		for (auto *st : { storage_a, storage_b }) {
			st->physical.use_closed.store(true);
			e.rd.qualification_retiring = true;
			e.rd.free_rid(RID::from_uint64(st->backing.rid));
			e.rd.qualification_retiring = false;
		}
		e.rd._free_pending_resources(0);
		CHECK(storage_a->physical.free_event != 0);
		CHECK(storage_b->physical.free_event != 0);
		e.original_bridge->storage[1] = nullptr;
		memdelete(storage_b);
	}

	static void bug145_output_custody_return_in_observe_and_multipass_release_fallback() {
		using namespace GDExtensionDeviceExit;
		REQUIRE(RSG::viewport != nullptr);
		REQUIRE(RSG::scene != nullptr);
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		const RID camera_rid = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera_rid);
		RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
		RSG::viewport->viewport_attach_camera(viewport, camera_rid);
		auto *vp = RSG::viewport->viewport_owner.get_or_null(viewport);
		REQUIRE(vp != nullptr);
		if (vp->render_buffers.is_null()) {
			vp->render_buffers = Ref<RenderSceneBuffers>(memnew(DelayedDeliveryTestBuffers));
		}
		REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, { 1451, 1, 1 }));
		RendererSceneRender::CameraData camera;
		camera.set_camera(Transform3D(), Projection(), false, false, Vector2(), 0, 1);
		camera.source_viewport = viewport;
		const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera, camera_rid, vp->render_buffers);
		Ref<RendererViewport::Viewport::CameraCapture> capture;
		REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(viewport, token, capture));

		Enrollment e;
		e.driver.complete = true;
		auto c = capacity(8);
		c.retained_payload_bytes = 64;
		auto pool = c;
		pool.requests = 4;
		pool.suboperations *= 4;
		pool.resources *= 4;
		pool.resource_uses *= 4;
		pool.graph_memberships *= 4;
		pool.native_memberships *= 4;
		pool.return_cells *= 4;
		pool.retained_payload_bytes *= 4;
		const auto attachment_ref = e.accept(pool);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto reserve_and_complete = [&](uint64_t op_serial, bool init_unit_and_storage) -> Request * {
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), op_serial, 1, 10, 11 };
			const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			};
			const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, &action }, 1, resources, 3, &use, 1, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			if (init_unit_and_storage) {
				REQUIRE(import_original_unit(e, *r));
				e.original_storage = memnew(Registry::QualificationBridge::Storage);
				auto &st = *e.original_storage;
				st.serial = 845;
				st.creating = false;
				st.origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
				st.physical.bridge = e.original_bridge->serial;
				st.physical.child = st.serial;
				e.original_bridge->storage[0] = &st;
				const RID srid = e.rd._qualification_storage_create(128, &st.physical);
				REQUIRE(srid.is_valid());
				const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
				REQUIRE(sbuf != nullptr);
				st.physical.rid = srid.get_id();
				st.backing = { Enrollment::ABI, { Enrollment::INSTANCE, st.serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
			} else {
				REQUIRE(e.registry->production_import_unit(ref, 0, &e.original_unit->backing).native_error == OK);
			}
			REQUIRE(e.registry->production_import_storage(ref, 1, &e.original_storage->backing).native_error == OK);
			const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
			REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
			const uint32_t push[4] = { 1, 2, 3, 4 };
			const GDExtensionProductionDispatchV2 dispatch = { production_dto_abi<GDExtensionProductionDispatchV2>(), 0, 10, 0, 0, 0, 2, 1, 2, 3, 4, sizeof(push), 0 };
			REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &dispatch, push, sizeof(push)).native_error == OK);
			REQUIRE(effect(e, *r, 0).state == CheckedRecordAppend::State::Appended);
			GDExtensionProductionSegmentV2 seg = {};
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_close_segment_locked(*r, seg) == OK);
				REQUIRE(e.registry->production_ack_segment_locked(*r, seg.segment, seg.segment_revision) == OK);
			}
			const uint32_t fidx = e.rd.frame;
			execute_transfers(e, *r, false);
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_retain_sources_locked(e.rd, fidx, e.rd.frames[fidx].qualification_epoch) == OK);
			}
			e.driver.expected_begin_frame = fidx;
			REQUIRE(e.rd._begin_frame(false, true) == OK);
			REQUIRE(r->natives[0].observation.state == NativeSourceObservation::State::Complete);
			return r;
		};

		// Part A: Action in OutputState::RETURNED returns host, subscriber, and camera/cull captures
		// inside existing RT OBSERVE turn -> Main production_release_v2 hits 0-RT-turn fast-path!
		Request *r1 = reserve_and_complete(8451, true);
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		{
			auto &act = r1->actions[0];
			act.output_state = ProductionActionCell::OutputState::RETURNED;
			act.preinput_host_prepared = true;
			act.preinput_host_returned = false;
			act.camera_ready_viewport = viewport;
			act.camera_ready_subscriber.registry = e.registry;
			act.camera_ready_subscriber.attachment = attachment_ref;
			act.camera_ready_subscriber.request = ref1;
			act.camera_ready_subscriber.action_ordinal = 0;
			act.camera_ready_subscriber.subscribed = true;
			REQUIRE(RSG::viewport->viewport_subscribe_camera_source(viewport, &act.camera_ready_subscriber));
			act.preinput_camera_source = capture;
			act.scene_cull_capture = capture;
		}
		const auto offered = budget(8, 8);
		{
			Registry::Lock lock(*e.registry);
			GDExtensionProductionReportV2 pre_snap{};
			REQUIRE(e.registry->production_report_contents_locked(*r1, pre_snap) == OK);
			CHECK_FALSE(e.registry->production_release_main_fast_path_eligible_locked(*r1, offered, pre_snap));
			// Run ONE RT OBSERVE turn.
			r1->control = {};
			r1->control.serial = 41;
			r1->control.state = ProductionControlState::QUEUED;
			r1->control.input.kind = ProductionControlKind::OBSERVE;
			r1->control.input.request_serial = r1->serial;
			r1->control.input.budget = offered;
			e.registry->production_control_execute_locked(*r1, &e.rd);
		}
		CHECK(r1->control.state == ProductionControlState::RETURNED);
		CHECK(r1->control.output.result.native_error == OK);
		CHECK(r1->actions[0].preinput_host_returned);
		CHECK(r1->actions[0].camera_ready_subscriber.released);
		CHECK(r1->actions[0].camera_ready_subscriber.closed);
		CHECK_FALSE(r1->actions[0].camera_ready_subscriber.subscribed);
		CHECK(r1->actions[0].preinput_camera_source.is_null());
		CHECK(r1->actions[0].scene_cull_capture.is_null());
		CHECK(r1->actions[0].preinput_camera_returned);
		CHECK(r1->control.output.service_usage.passes == 2);
		CHECK(r1->control.output.service_usage.receipts == 2);
		ProductionControlOutput obs_out{};
		REQUIRE(e.registry->production_control_consume(ref1, 41, &obs_out) == OK);
		GDExtensionProductionServiceBudgetV1 rem1 = {};
		GDExtensionProductionReportV2 rel_rep1 = {};
		CHECK(e.registry->production_release_v2(ref1, &offered, &rem1, &rel_rep1).native_error == OK);
		CHECK(rel_rep1.service_control_serial == 0);
		CHECK(rel_rep1.service_pending == 0);
		CHECK(r1->serial == 0);

		// Part B: Non-RETURNED action keeps custody in OBSERVE, and multi-pass production_release_locked
		// drains BOTH subscriber unsubscribe AND capture unref in ONE RT turn when budget.passes >= 2.
		Request *r2 = reserve_and_complete(8452, false);
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		{
			auto &act = r2->actions[0];
			act.output_state = ProductionActionCell::OutputState::BOUND;
			act.preinput_host_prepared = true;
			act.preinput_host_returned = false;
			act.camera_ready_viewport = viewport;
			act.camera_ready_subscriber.registry = e.registry;
			act.camera_ready_subscriber.attachment = attachment_ref;
			act.camera_ready_subscriber.request = ref2;
			act.camera_ready_subscriber.action_ordinal = 0;
			act.camera_ready_subscriber.subscribed = true;
			REQUIRE(RSG::viewport->viewport_subscribe_camera_source(viewport, &act.camera_ready_subscriber));
			act.preinput_camera_source = capture;
			act.scene_cull_capture = capture;
		}
		{
			Registry::Lock lock(*e.registry);
			r2->control = {};
			r2->control.serial = 42;
			r2->control.state = ProductionControlState::QUEUED;
			r2->control.input.kind = ProductionControlKind::OBSERVE;
			r2->control.input.request_serial = r2->serial;
			r2->control.input.budget = offered;
			e.registry->production_control_execute_locked(*r2, &e.rd);
		}
		// Still ACQUIRED -> OBSERVE MUST NOT drop custody!
		CHECK_FALSE(r2->actions[0].preinput_host_returned);
		CHECK_FALSE(r2->actions[0].camera_ready_subscriber.released);
		CHECK(r2->actions[0].preinput_camera_source.is_valid());
		CHECK(r2->actions[0].scene_cull_capture.is_valid());
		REQUIRE(e.registry->production_control_consume(ref2, 42, &obs_out) == OK);

		// Multi-pass RT RELEASE drains host + subscriber + captures in ONE turn with budget(8, 8).
		ProductionServiceUsage rel_usage{};
		{
			Registry::Lock lock(*e.registry);
			const auto rel_res = e.registry->production_release_locked(*r2, &e.rd, false, &offered, &rel_usage);
			CHECK(rel_res.native_error == OK);
		}
		CHECK(r2->actions[0].preinput_host_returned);
		CHECK(r2->actions[0].camera_ready_subscriber.released);
		CHECK(r2->actions[0].preinput_camera_source.is_null());
		CHECK(r2->actions[0].scene_cull_capture.is_null());
		CHECK(r2->actions[0].preinput_camera_returned);
		CHECK(rel_usage.passes >= 2);
		CHECK(rel_usage.receipts >= 2);
		GDExtensionProductionServiceBudgetV1 rem2 = {};
		GDExtensionProductionReportV2 rel_rep2 = {};
		CHECK(e.registry->production_release_v2(ref2, &offered, &rem2, &rel_rep2).native_error == OK);
		CHECK(r2->serial == 0);

		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		auto &st = *e.original_storage;
		st.physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(st.backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
		capture.unref();
		RSG::viewport->free(viewport);
		RSG::scene->free(camera_rid);
	}

	static void bug145_staged_prepare_native_record_v2_and_deferred_rt_subscribe_on_dispatch() {
		using namespace GDExtensionDeviceExit;
		REQUIRE(RSG::viewport != nullptr);
		REQUIRE(RSG::scene != nullptr);
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		REQUIRE(server != nullptr);
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		const RID camera_rid = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera_rid);
		RSG::viewport->viewport_set_size(viewport, 64, 48, 1);
		RSG::viewport->viewport_attach_camera(viewport, camera_rid);

		Enrollment e;
		auto c = capacity(4);
		const auto attachment_ref = e.accept(c);
		REQUIRE(attachment_ref.handle_serial != 0);
		Request *r = nullptr;
		ProductionAttachment *att = nullptr;
		const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 8453, 1, 10, 11 };
		const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
			att->opened = true;
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, operation, c, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		REQUIRE(att != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		auto &anchor = r->actions[0];
		auto &sub = anchor.camera_ready_subscriber;
		anchor.prepared = true;
		anchor.output_source_prepared = true;
		anchor.output_camera_pending = true;
		anchor.host_view_prepared = true;
		anchor.host_view_received = false;
		anchor.camera_ready_viewport = viewport;
		anchor.host_view_camera = camera_rid;
		sub.registry = e.registry;
		sub.attachment = attachment_ref;
		sub.request = ref;
		sub.action_ordinal = 0;
		sub.subscribed = false;
		sub.closed = false;
		sub.released = false;
		r->native_staged_anchor = 0;
		r->native_stage_budget = budget(4, 4);

		// 1. Closed subscriber or unprepared host view still rejected with ERR_UNCONFIGURED.
		sub.closed = true;
		CHECK(e.registry->production_prepare_native_record_v2(ref).native_error == ERR_UNCONFIGURED);
		sub.closed = false;
		anchor.host_view_prepared = false;
		CHECK(e.registry->production_prepare_native_record_v2(ref).native_error == ERR_UNCONFIGURED);
		anchor.host_view_prepared = true;

		// 2. Main-bound host_view_prepared (without prior RT OBSERVE subscribe!) is accepted.
		REQUIRE(e.registry->production_prepare_native_record_v2(ref).native_error == OK);
		CHECK(r->native_prepared_executor);
		CHECK(r->native_static_sealed);
		CHECK_FALSE(anchor.host_view_received);
		CHECK_FALSE(sub.subscribed);

		// 3. Staged RECORD dispatch on RT performs deferred inline subscribe and parks scene_waiting = true.
		r->control = {};
		r->control.serial = 55;
		r->control.state = ProductionControlState::QUEUED;
		r->control.input.kind = ProductionControlKind::RECORD;
		r->control.input.request_serial = r->serial;
		r->control.input.record_generation = r->operation.record_generation;
		r->control.input.budget = r->native_stage_budget;
		r->control.record_history.admitted = true;
		e.registry->production_control_dispatch(Enrollment::INSTANCE, attachment_ref.handle_serial, r->serial, 55);
		CHECK(anchor.host_view_received);
		CHECK(sub.subscribed);
		CHECK(r->control.state == ProductionControlState::QUEUED);
		CHECK(r->control.scene_waiting);
		CHECK(r->capture_inflight == 0);

		CHECK(RSG::viewport->viewport_unsubscribe_camera_source(viewport, &sub));
		sub.subscribed = false;
		sub.released = true;
		sub.registry = nullptr;
		CHECK(e.registry->production_cancel(ref).native_error == ERR_BUSY);
		GDExtensionProductionRecordReportV3 cancel_report{};
		CHECK(e.registry->production_record_consume_v3(ref, 55, &cancel_report).native_error == OK);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
		CHECK(e.registry->production_release(ref).native_error == OK);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);
		RSG::viewport->free(viewport);
		RSG::scene->free(camera_rid);
	}

	static void bug166_distinct_native_owners_skip_alias_rows() {
		using namespace GDExtensionDeviceExit;
		// Part A: Production capture path with 220 native cells where distinct
		// submission owners live at indices 0 and 180 (179 alias rows between,
		// 39 alias rows after). OBSERVE, passive RT harvest, and RELEASE must
		// each complete in 1 single pass without charging alias rows.
		{
			Enrollment e;
			e.driver.complete = false;
			constexpr uint32_t TOTAL_CELLS = 220;
			constexpr uint32_t SECOND_OWNER = 180;
			const auto c = capacity(TOTAL_CELLS);
			const auto att_ref = e.accept(c);
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				ProductionAttachment *attachment = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_attachment_locked(att_ref, true, attachment, rd) == OK);
				const GDExtensionProductionOperationV1 operation{ production_dto_abi<GDExtensionProductionOperationV1>(), 8660, 1, 10, 11 };
				const GDExtensionProductionActionV2 action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
				const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
				const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*attachment, operation, c, { nullptr, &action }, 1, &resource, 1, &use, 1, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };

			{
				MutexLock lock(e.registry->mutex);
				const uint64_t frame_epoch = e.rd.frames[0].qualification_epoch;
				for (uint32_t g = 0; g < TOTAL_CELLS; ++g) {
					auto &graph = r->graphs[g];
					new (&graph) ProductionGraphCell();
					graph.request = r;
					graph.device_frame = 0;
					graph.device_epoch = frame_epoch;
					graph.pending = true;
					graph.fact.phase = 2;
					graph.fact.command_slot = g < SECOND_OWNER ? 0u : 1u;
					graph.fact.action_ordinal = g;
				}
				r->graph_count = TOTAL_CELLS;

				RDD::QueueCallReport report0;
				report0.calls.push_back(e.driver.issue());
				e.registry->production_capture_issue_locked(e.rd, 0, NativeSourceIssueView{ report0 }, OK);

				RDD::QueueCallReport report1;
				report1.calls.push_back(e.driver.issue());
				e.registry->production_capture_issue_locked(e.rd, 1, NativeSourceIssueView{ report1 }, OK);
			}

			REQUIRE(r->native_count == TOTAL_CELLS);
			REQUIRE(r->native_owner_count == 2);
			CHECK(r->native_owner_indices[0] == 0);
			CHECK(r->native_owner_indices[1] == SECOND_OWNER);
			CHECK_FALSE(r->native_owners_overflowed);
			CHECK(r->natives[0].owner_index == 0);
			CHECK(r->natives[SECOND_OWNER - 1].owner_index == 0);
			CHECK(r->natives[SECOND_OWNER].owner_index == SECOND_OWNER);
			CHECK(r->natives[TOTAL_CELLS - 1].owner_index == SECOND_OWNER);

			// 1) production_poll_native_locked() retains and polls BOTH distinct
			//    owners (0 and 180) in ONE single pass, skipping all 218 alias rows.
			const auto obs = turn(e, *r, ProductionControlKind::OBSERVE, budget());
			CHECK(obs.service_usage.passes == 2);
			CHECK(obs.service_usage.receipts == 4);
			CHECK(e.driver.retains[1] == 1);
			CHECK(e.driver.retains[2] == 1);
			CHECK(e.driver.polls[1] == 1);
			CHECK(e.driver.polls[2] == 1);
			CHECK(r->natives[0].member.retained);
			CHECK(r->natives[SECOND_OWNER].member.retained);
			CHECK_FALSE(r->natives[1].member.retained);
			CHECK_FALSE(r->natives[SECOND_OWNER + 1].member.retained);

			// 2) production_observe_completed_frames_rt() harvests BOTH distinct
			//    owners (including index 180 >= 64) in ONE single RT pass.
			e.driver.complete = true;
			e.registry->production_observe_completed_frames_rt(e.rd);
			CHECK(r->natives[0].observation.state == NativeSourceObservation::State::Complete);
			CHECK(r->natives[SECOND_OWNER].observation.state == NativeSourceObservation::State::Complete);
			CHECK(e.driver.polls[1] == 2);
			CHECK(e.driver.polls[2] == 2);

			// 3) production_release_v2() qualifies for zero-RT-hop Main fast-path
			//    despite native_count == 220 > 64 and releases BOTH distinct owners
			//    in ONE single pass without charging alias rows.
			{
				MutexLock lock(e.registry->mutex);
				r->next_action = r->action_count;
				r->segment.segment = 1;
				r->acknowledged_segment = 1;
				e.rd.is_main_instance = true;
				GDExtensionProductionReportV2 snap = {};
				REQUIRE(e.registry->production_report_contents_locked(*r, snap) == OK);
				CHECK(snap.known_obligations == 2);
				CHECK(snap.complete_obligations == 2);
				CHECK(e.registry->production_release_main_fast_path_eligible_locked(*r, budget(), snap));
			}
			const auto offered = budget();
			GDExtensionProductionServiceBudgetV1 remaining = {};
			GDExtensionProductionReportV2 rel_report = {};
			REQUIRE(e.registry->production_release_v2(ref, &offered, &remaining, &rel_report).native_error == OK);
			CHECK(r->serial == 0);
			CHECK(rel_report.service_control_serial == 0);
			CHECK(rel_report.service_pending == 0);
			CHECK(offered.passes - remaining.passes == 2);
			CHECK(offered.receipts - remaining.receipts == 2);
			CHECK(e.driver.releases[1] == 1);
			CHECK(e.driver.releases[2] == 1);
		}

		// Part B: Unindexed fallback path (e.g. >64 owners overflow / direct cell
		// setup) with 220 rows and distinct owners at 0 and 180 still completes
		// production_poll_native_locked() and production_observe_completed_frames_rt()
		// in 1 single pass.
		{
			Enrollment e;
			e.driver.complete = false;
			constexpr uint32_t TOTAL_CELLS = 220;
			constexpr uint32_t SECOND_OWNER = 180;
			auto *r = setup(e, TOTAL_CELLS);
			REQUIRE(r != nullptr);
			const auto src0 = e.driver.issue();
			const auto src1 = e.driver.issue();
			for (uint32_t i = 0; i < TOTAL_CELLS; ++i) {
				auto &cell = r->natives[i];
				cell.request = r;
				cell.owner_index = i < SECOND_OWNER ? 0u : SECOND_OWNER;
				cell.member.source = i < SECOND_OWNER ? src0 : src1;
			}
			r->native_count = TOTAL_CELLS;
			REQUIRE(r->native_owner_count == 0);

			const auto obs = turn(e, *r, ProductionControlKind::OBSERVE, budget());
			CHECK(obs.service_usage.passes == 2);
			CHECK(obs.service_usage.receipts == 4);
			CHECK(e.driver.retains[1] == 1);
			CHECK(e.driver.retains[2] == 1);
			CHECK(e.driver.polls[1] == 1);
			CHECK(e.driver.polls[2] == 1);

			e.driver.complete = true;
			e.registry->production_observe_completed_frames_rt(e.rd);
			CHECK(r->natives[0].observation.state == NativeSourceObservation::State::Complete);
			CHECK(r->natives[SECOND_OWNER].observation.state == NativeSourceObservation::State::Complete);
			CHECK(e.driver.polls[1] == 2);
			CHECK(e.driver.polls[2] == 2);
		}
	}

	static void bug170_prefix_reset_and_no_per_wave_madvise_thrash() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(64);
		pool_cap.requests = 2;
		pool_cap.suboperations = 64;
		pool_cap.resources = 64;
		pool_cap.resource_uses = 512;
		pool_cap.graph_memberships = 128;
		pool_cap.native_memberships = 128;
		pool_cap.return_cells = 64;
		pool_cap.readbacks = 16;
		pool_cap.retained_payload_bytes = 128u * 1024u;
		pool_cap.readback_bytes = 64u * 1024u;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto req_cap = pool_cap;
		req_cap.requests = 1;
		const GDExtensionProductionActionV2 actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};
		const GDExtensionProductionResourceV1 resources[2] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
		};
		const GDExtensionProductionUseV1 uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
		};

		// Run 6 consecutive single-request waves where children drops to 0 every wave.
		// Verify: (a) ZERO per-wave MADV_FREE_REUSABLE thrash, (b) prefix-only reset
		// clears all touched cells (actions, resources, uses including bindings/spares,
		// graphs, returns) and releases tail .request pointers so subsequent reserves
		// reuse slot 0 cleanly with zero stale state.
		for (uint32_t wave = 0; wave < 6; ++wave) {
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 9170 + wave, 1, 10, 12 };
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, req_cap, { nullptr, actions }, 2, resources, 2, uses, 2, {}, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			CHECK(r->action_count == 2);
			CHECK(r->resource_count == 2);
			CHECK(r->use_count == 2);
			CHECK(r->uses_touched_end() == 2);
			// Verify zero stale fields from prior wave across touched prefix.
			CHECK_FALSE(r->actions[0].prepared);
			CHECK_FALSE(r->actions[1].prepared);
			CHECK_FALSE(r->resources[0].imported);
			CHECK(r->uses[2].binding.binding == 0);
			CHECK(r->uses[2].output_role == ProductionUseCell::OutputRole::UNUSED);
			CHECK(r->uses[3].output_role == ProductionUseCell::OutputRole::UNUSED);
			CHECK_FALSE(r->graphs[0].pending);
			CHECK_FALSE(r->returns[0].pending);
			CHECK(r->returns[0].serial == 0);
			// Verify headroom tail cells are claimed for this request.
			CHECK(r->uses[req_cap.resource_uses - 1].request == r);
			CHECK(r->actions[req_cap.suboperations - 1].request == r);

			// Mutate touched prefix cells + binding slot [2] + spare slot [3] + graph [0] + return [0].
			{
				MutexLock lock(e.registry->mutex);
				r->actions[0].prepared = true;
				r->actions[1].prepared = true;
				r->resources[0].imported = true;
				r->uses[2].binding.binding = 42 + wave;
				r->binding_used = 1;
				r->note_use_touched_end(r->use_count + r->binding_used);
				r->uses[3].output_role = ProductionUseCell::OutputRole::DISPOSAL_RESERVED;
				r->note_disposal_use_index(3);
				CHECK(r->uses_touched_end() == 4);
				r->graphs[0].pending = false;
				r->graphs[0].fact.action_ordinal = 77;
				r->graph_count = 1;
				r->returns[0].serial = 555 + wave;
				r->returns[0].pending = false;
			}

			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			REQUIRE(e.registry->production_cancel(ref).native_error == OK);
			REQUIRE(e.registry->production_release(ref).native_error == OK);
			CHECK(att->children == 0);
			// Active per-wave releases must NOT invoke MADV_FREE_REUSABLE.
			CHECK(att->arena_reclaimed_bytes_total == 0);
			// Tail cells must have .request cleared so next reserve finds slot 0.
			CHECK(att->uses[req_cap.resource_uses - 1].request == nullptr);
			CHECK(att->actions[req_cap.suboperations - 1].request == nullptr);
			// Simulate 1 active frame between waves (well below ARENA_IDLE_RECLAIM_FRAMES).
			e.registry->production_observe_completed_frames_rt(e.rd);
			CHECK(att->arena_reclaimed_bytes_total == 0);
		}
		// Closing while idle reclaims immediately before freeing the arena.
		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}
	static void bug183b_reclaim_all_hwm_cell_subarenas_and_reacquire() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(64);
		pool_cap.requests = 2;
		pool_cap.suboperations = 128;
		pool_cap.resources = 128;
		pool_cap.resource_uses = 512;
		pool_cap.readbacks = 128;
		pool_cap.graph_memberships = 128;
		pool_cap.return_cells = 1024;
		// Keep the 4 previously-reclaimed pools small so we can prove reclaimed bytes
		// exceed their combined byte span by >256 KiB from the 8 HWM cell sub-arenas.
		pool_cap.native_memberships = 128;
		pool_cap.retained_payload_bytes = 64;
		pool_cap.readback_bytes = 64;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto req_cap = pool_cap;
		req_cap.requests = 1;
		const GDExtensionProductionActionV2 actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};
		const GDExtensionProductionResourceV1 resources[2] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
		};
		const GDExtensionProductionUseV1 uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
		};

		Request *r1 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op1{ production_dto_abi<GDExtensionProductionOperationV1>(), 18301, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op1, req_cap, { nullptr, actions }, 2, resources, 2, uses, 2, {}, 0, r1, 2) == OK);
		}
		REQUIRE(r1 != nullptr);
		CHECK(att->children == 1);
		CHECK(att->actions_hwm == req_cap.suboperations);
		CHECK(att->resources_hwm == req_cap.resources);
		CHECK(att->uses_hwm == req_cap.resource_uses);
		CHECK(att->readbacks_hwm == req_cap.readbacks);
		CHECK(att->graphs_hwm == req_cap.graph_memberships);
		CHECK(att->returns_hwm == req_cap.return_cells);
		CHECK(att->commands_hwm == r1->command_count);
		CHECK(att->frames_hwm == r1->frame_count);

		// Defensive guard: reclaim_pod_pages() must be a strict no-op while children > 0.
		{
			MutexLock lock(e.registry->mutex);
			att->reclaim_pod_pages();
		}
		CHECK(att->actions_hwm == req_cap.suboperations);
		CHECK(att->uses_hwm == req_cap.resource_uses);
		CHECK(att->arena_reclaimed_bytes_total == 0);

		// Mutate touched cells across pools before releasing r1.
		{
			MutexLock lock(e.registry->mutex);
			r1->actions[0].prepared = true;
			r1->resources[0].imported = true;
			r1->uses[2].binding.binding = 77;
			r1->binding_used = 1;
			r1->note_use_touched_end(r1->use_count + r1->binding_used);
			r1->graphs[0].fact.action_ordinal = 99;
			r1->graph_count = 1;
			r1->returns[0].serial = 18301;
		}
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		REQUIRE(e.registry->production_cancel(ref1).native_error == OK);
		REQUIRE(e.registry->production_release(ref1).native_error == OK);
		CHECK(att->children == 0);
		CHECK(att->has_reclaimable_arena_pages());
		CHECK(att->arena_reclaimed_bytes_total == 0);

		// Idle frames below cooldown must not reclaim yet.
		for (uint32_t f = 0; f + 1 < ProductionAttachment::ARENA_IDLE_RECLAIM_FRAMES; ++f) {
			e.registry->production_observe_completed_frames_rt(e.rd);
		}
		CHECK(att->arena_reclaimed_bytes_total == 0);
		CHECK(att->uses_hwm == req_cap.resource_uses);

		// Cooldown boundary destroys all 8 HWM cell pools, advises their pages, and resets HWMs to 0.
		e.registry->production_observe_completed_frames_rt(e.rd);
		CHECK(att->actions_hwm == 0);
		CHECK(att->resources_hwm == 0);
		CHECK(att->uses_hwm == 0);
		CHECK(att->readbacks_hwm == 0);
		CHECK(att->graphs_hwm == 0);
		CHECK(att->returns_hwm == 0);
		CHECK(att->commands_hwm == 0);
		CHECK(att->frames_hwm == 0);
		CHECK_FALSE(att->has_reclaimable_arena_pages());
#if defined(__APPLE__) || defined(__linux__)
		const uint64_t old_pod_upper_bound = pool_cap.retained_payload_bytes + pool_cap.readback_bytes + uint64_t(pool_cap.native_memberships) * sizeof(ProductionNativeCell);
		CHECK(att->arena_reclaimed_bytes_total > old_pod_upper_bound + 256u * 1024u);
#endif
		const uint64_t reclaimed_after_idle = att->arena_reclaimed_bytes_total;
		// Subsequent idle tick must be a no-op.
		e.registry->production_observe_completed_frames_rt(e.rd);
		CHECK(att->arena_reclaimed_bytes_total == reclaimed_after_idle);

		// Re-reserve a fresh request on the same attachment after full HWM reclaim.
		Request *r2 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op2{ production_dto_abi<GDExtensionProductionOperationV1>(), 18302, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op2, req_cap, { nullptr, actions }, 2, resources, 2, uses, 2, {}, 0, r2, 2) == OK);
		}
		REQUIRE(r2 != nullptr);
		CHECK(att->children == 1);
		CHECK(att->actions_hwm == req_cap.suboperations);
		CHECK(att->resources_hwm == req_cap.resources);
		CHECK(att->uses_hwm == req_cap.resource_uses);
		CHECK(att->readbacks_hwm == req_cap.readbacks);
		CHECK(att->graphs_hwm == req_cap.graph_memberships);
		CHECK(att->returns_hwm == req_cap.return_cells);
		CHECK(att->commands_hwm == r2->command_count);
		CHECK(att->frames_hwm == r2->frame_count);
		CHECK_FALSE(r2->actions[0].prepared);
		CHECK_FALSE(r2->resources[0].imported);
		CHECK(r2->uses[2].binding.binding == 0);
		CHECK(r2->graphs[0].fact.action_ordinal == 0);
		CHECK(r2->returns[0].serial == 0);

		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		REQUIRE(e.registry->production_cancel(ref2).native_error == OK);
		REQUIRE(e.registry->production_release(ref2).native_error == OK);
		CHECK(att->children == 0);
		CHECK(att->has_reclaimable_arena_pages());
		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}
	static void bug183b2_mmap_backed_arena_reclaim_and_unmap() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(64);
		pool_cap.requests = 2;
		pool_cap.suboperations = 64;
		pool_cap.resources = 64;
		pool_cap.resource_uses = 256;
		pool_cap.readbacks = 32;
		pool_cap.graph_memberships = 64;
		pool_cap.native_memberships = 64;
		pool_cap.return_cells = 256;
		pool_cap.retained_payload_bytes = 64u * 1024u;
		pool_cap.readback_bytes = 32u * 1024u;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);
		REQUIRE(att->arena != nullptr);
#if (defined(__APPLE__) || defined(__linux__))
		CHECK(att->arena_mmap_bytes >= att->quote.allocation_bytes);
		CHECK((att->arena_mmap_bytes & 4095u) == 0u);
		CHECK((uintptr_t(att->arena) & 4095u) == 0u);
#endif
		auto req_cap = pool_cap;
		req_cap.requests = 1;
		const GDExtensionProductionActionV2 actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};
		const GDExtensionProductionResourceV1 resources[2] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
		};
		const GDExtensionProductionUseV1 uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
		};
		Request *r1 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op1{ production_dto_abi<GDExtensionProductionOperationV1>(), 18321, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op1, req_cap, { nullptr, actions }, 2, resources, 2, uses, 2, {}, 0, r1, 2) == OK);
		}
		REQUIRE(r1 != nullptr);
		memset(att->payload + r1->payload_offset, 0x7E, 32u * 1024u);
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		REQUIRE(e.registry->production_cancel(ref1).native_error == OK);
		REQUIRE(e.registry->production_release(ref1).native_error == OK);
		for (uint32_t f = 0; f < ProductionAttachment::ARENA_IDLE_RECLAIM_FRAMES; ++f) {
			e.registry->production_observe_completed_frames_rt(e.rd);
		}
#if defined(__APPLE__) || defined(__linux__)
		CHECK(att->arena_reclaimed_bytes_total > 0);
#endif
		CHECK(att->actions_hwm == 0);
		CHECK(att->uses_hwm == 0);

		Request *r2 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op2{ production_dto_abi<GDExtensionProductionOperationV1>(), 18322, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op2, req_cap, { nullptr, actions }, 2, resources, 2, uses, 2, {}, 0, r2, 2) == OK);
		}
		REQUIRE(r2 != nullptr);
		memset(att->payload + r2->payload_offset, 0x3D, 4096);
		CHECK(att->payload[r2->payload_offset + 4095] == 0x3D);
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		REQUIRE(e.registry->production_cancel(ref2).native_error == OK);
		REQUIRE(e.registry->production_release(ref2).native_error == OK);
		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}
	static void bug184a_per_frame_validation_cache_and_rid_index() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		constexpr uint32_t kStorageCount = 20;
		constexpr uint32_t kGuardSlot = kStorageCount;
		constexpr uint32_t kTotalResources = kStorageCount + 1;
		auto c = capacity(32);
		c.resources = 32;
		c.resource_uses = 64;
		c.return_cells = 32;
		c.retained_payload_bytes = 256;
		c.readback_bytes = 64;
		const auto att_ref = e.accept(c);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *attachment = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r = nullptr;
		const GDExtensionProductionOperationV1 operation = { production_dto_abi<GDExtensionProductionOperationV1>(), 18401, 1, 10, 14 };
		const GDExtensionProductionActionV1 actions[] = {
			{ 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 11, GDEXTENSION_PRODUCTION_COPY_V1, 0, 1, 2, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 12, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 3, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 13, GDEXTENSION_PRODUCTION_COPY_V1, 0, 4, 2, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};
		GDExtensionProductionResourceV1 resources[kTotalResources] = {};
		for (uint32_t i = 0; i < kStorageCount; ++i) {
			resources[i] = { GDEXTENSION_PRODUCTION_STORAGE_V1, i, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
		}
		resources[kGuardSlot] = { GDEXTENSION_PRODUCTION_STORAGE_V1, kGuardSlot, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 32, 0 };
		const GDExtensionProductionUseV1 uses[] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
			{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
		};
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, attachment, rd) == OK);
			REQUIRE(e.registry->production_reserve_metadata_locked(*attachment, operation, c, actions, 4, resources, kTotalResources, uses, 6, nullptr, 0, r) == OK);
		}
		REQUIRE(r != nullptr);
		graph_initialize(e);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, r->serial };
		for (uint32_t i = 0; i < kTotalResources; ++i) {
			GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, i == kGuardSlot ? 32u : 128u, i, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			REQUIRE(create_storage(e, ref, i, &origin, &backing).native_error == OK);
			REQUIRE(backing.rid != 0);
		}
		uint8_t payload[32] = {};
		for (uint32_t i = 0; i < 32; ++i) {
			payload[i] = uint8_t(0xA0 + i);
		}
		const GDExtensionProductionUploadV1 up0 = { production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 32 };
		const GDExtensionProductionCopyV1 cp1 = { production_dto_abi<GDExtensionProductionCopyV1>(), 11, 0, 1, 0, 0, 32 };
		const GDExtensionProductionUploadV1 up2 = { production_dto_abi<GDExtensionProductionUploadV1>(), 12, 2, 0, 0, 32 };
		const GDExtensionProductionCopyV1 cp3 = { production_dto_abi<GDExtensionProductionCopyV1>(), 13, 2, 3, 0, 0, 32 };
		REQUIRE(e.registry->production_prepare_upload(ref, &up0, payload, 32).native_error == OK);
		REQUIRE(e.registry->production_prepare_copy(ref, &cp1).native_error == OK);
		REQUIRE(e.registry->production_prepare_upload(ref, &up2, payload, 32).native_error == OK);
		REQUIRE(e.registry->production_prepare_copy(ref, &cp3).native_error == OK);
		e.rd.upload_staging_buffers.block_size = 256;
		e.rd.upload_staging_buffers.max_size = 512;
		e.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		REQUIRE(e.rd._insert_staging_block(e.rd.upload_staging_buffers) == OK);
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_freeze_rd_locked(*r, &e.rd) == OK);
			// Mark slot kGuardSlot as an unmatched output_guard_readonly whose consumer is action index 2.
			r->resources[kGuardSlot].output_guard_readonly = true;
			r->resources[kGuardSlot].output_guard_history = false;
			r->resources[kGuardSlot].output_guard_consumer = 2;
		}

		void *saved_frame = nullptr;
		e.registry->test_push_production_physical_frame(7001, saved_frame);

		// Action 0: cache miss -> full scan validates all 20 slots, caches frame & builds O(1) rid_to_slot.
		const auto obs0 = effect(e, *r, 0);
		REQUIRE(obs0.error == OK);
		REQUIRE(obs0.state == CheckedRecordAppend::State::Appended);
		CHECK(e.rd.production_resource_validation_cache.valid);
		CHECK(e.rd.production_resource_validation_cache.rid_index_valid);
		CHECK_FALSE(e.rd.production_resource_validation_cache.rid_index_active);
		CHECK(e.rd.production_resource_validation_cache.frame_serial == 7001);
		REQUIRE(e.rd.production_resource_validation_cache.index_dependent_slots.size() == 1);
		CHECK(e.rd.production_resource_validation_cache.index_dependent_slots[0] == kGuardSlot);

		// Action 1 (COPY 0 -> 1): cache HIT + O(1) rid_to_slot lookup in buffer_copy.
		const auto obs1 = effect(e, *r, 1);
		REQUIRE(obs1.error == OK);
		REQUIRE(obs1.state == CheckedRecordAppend::State::Appended);
		CHECK(e.rd.production_resource_validation_cache.valid);
		CHECK(e.rd.production_resource_validation_cache.frame_serial == 7001);

		// Action 2: cache HIT MUST still reject unbound output_guard_readonly slot when index == output_guard_consumer!
		const auto obs2_denied = effect(e, *r, 2);
		CHECK(obs2_denied.error == ERR_UNAUTHORIZED);
		CHECK(obs2_denied.state == CheckedRecordAppend::State::Rejected);
		CHECK_FALSE(e.rd.production_resource_validation_cache.valid);
		CHECK(r->next_action == 2);

		// Re-target guard consumer past action 3 so action 2 succeeds and rebuilds cache.
		{
			MutexLock lock(e.registry->mutex);
			r->resources[kGuardSlot].output_guard_consumer = 99;
		}
		const auto obs2_ok = effect(e, *r, 2);
		REQUIRE(obs2_ok.error == OK);
		REQUIRE(obs2_ok.state == CheckedRecordAppend::State::Appended);
		CHECK(e.rd.production_resource_validation_cache.valid);
		CHECK(e.rd.production_resource_validation_cache.rid_index_valid);

		// Close an unrelated background slot (slot 15) + bump epoch: action 3 (COPY 2 -> 3) MUST reject!
		{
			MutexLock lock(e.registry->mutex);
			r->resources[15].owned_physical.use_closed.store(true);
			e.registry->bump_resource_permission_epoch();
		}
		const auto obs3_bg_closed = effect(e, *r, 3);
		CHECK(obs3_bg_closed.error == ERR_UNAUTHORIZED);
		CHECK(r->next_action == 3);

		// Restore slot 15, verify O(1) rid_to_slot lookup handles duplicate RIDs (first closed, second valid).
		{
			MutexLock lock(e.registry->mutex);
			r->resources[15].owned_physical.use_closed.store(false);
			e.registry->bump_resource_permission_epoch();
		}
		// Close action 3's own destination slot (slot 3) WITHOUT bumping epoch -> action 3 STILL rejects!
		{
			MutexLock lock(e.registry->mutex);
			// Re-warm cache validity for frame 7001 to prove cache-hit checks action-owned slots live.
			e.rd.production_resource_validation_cache.valid = true;
			e.rd.production_resource_validation_cache.permission_epoch = e.registry->resource_permission_epoch.load(std::memory_order_acquire);
			r->resources[3].owned_physical.use_closed.store(true);
		}
		const auto obs3_dst_closed = effect(e, *r, 3);
		CHECK(obs3_dst_closed.error == ERR_UNAUTHORIZED);
		CHECK(r->next_action == 3);

		// Restore slot 3 -> action 3 succeeds cleanly!
		{
			MutexLock lock(e.registry->mutex);
			r->resources[3].owned_physical.use_closed.store(false);
		}
		const auto obs3_ok = effect(e, *r, 3);
		REQUIRE(obs3_ok.error == OK);
		REQUIRE(obs3_ok.state == CheckedRecordAppend::State::Appended);
		CHECK(e.rd.production_resource_validation_cache.valid);
		CHECK(e.rd.production_resource_validation_cache.rid_index_valid);

		// Verify O(1) rid_to_slot lookup directly with rid_index_active = true on valid + unknown RIDs.
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.rd.production_recording_request = r;
			++r->capture_inflight;
			e.rd.production_resource_validation_cache.rid_index_active = true;
			CHECK(e.rd._production_resource_permitted(r->resources[0].primary));
			CHECK(e.rd._production_resource_permitted(r->resources[19].primary));
			CHECK_FALSE(e.rd._production_resource_permitted(RID::from_uint64(0xDEADBEEFull)));
			r->resources[19].owned_physical.use_closed.store(true);
			CHECK_FALSE(e.rd._production_resource_permitted(r->resources[19].primary));
			r->resources[19].owned_physical.use_closed.store(false);
			e.rd.production_resource_validation_cache.rid_index_active = false;
			--r->capture_inflight;
			e.rd.production_recording_request = nullptr;
		}

		e.registry->test_pop_production_physical_frame(saved_frame);
		{
			MutexLock lock(e.registry->mutex);
			r->resources[kGuardSlot].output_guard_readonly = false;
		}
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		e.driver.complete = true;
		execute_transfers(e, *r, true);
		const auto observed_done = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(observed_done.report.known_obligations > 0);
		CHECK(observed_done.report.complete_obligations == observed_done.report.known_obligations);
		const auto full_budget = budget();
		for (uint32_t i = 0; i < kTotalResources; ++i) {
			GDExtensionStorageRetireRefV1 retire = {};
			REQUIRE(retire_storage(e, *r, i, retire));
			GDExtensionStorageRetireReportV1 report = {};
			for (uint32_t t = 0; t < 16 && report.state != 1; ++t) {
				const auto err = observe_storage(e, ref, retire, &full_budget, &report).native_error;
				REQUIRE((err == OK || err == ERR_BUSY));
			}
			REQUIRE(report.state == 1);
			REQUIRE(e.registry->production_storage_release(ref, retire).native_error == OK);
		}
		REQUIRE(finish_owned_maintenance(e, *r, full_budget));
		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}
	static void bug185b_o1_request_lookup_free_cursor_and_dedup_validation() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(64);
		pool_cap.requests = 4;
		pool_cap.suboperations = 64;
		pool_cap.resources = 64;
		pool_cap.resource_uses = 512;
		pool_cap.readbacks = 16;
		pool_cap.graph_memberships = 64;
		pool_cap.native_memberships = 128;
		pool_cap.return_cells = 64;
		pool_cap.retained_payload_bytes = 16u * 1024u;
		pool_cap.readback_bytes = 4u * 1024u;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
			CHECK(production_serial_index_has_attachment(*e.registry, att->serial));
		}
		REQUIRE(att != nullptr);

		auto req_cap = capacity(8);
		req_cap.requests = 1;
		req_cap.suboperations = 8;
		req_cap.resources = 12;
		req_cap.resource_uses = 64;
		req_cap.readbacks = 2;
		req_cap.graph_memberships = 8;
		req_cap.native_memberships = 16;
		req_cap.return_cells = 12;
		req_cap.retained_payload_bytes = 2048;
		req_cap.readback_bytes = 256;

		// Build 1 action with 32 uses referencing 8 distinct storage slots (each duplicated 4x)
		// to verify count_max_action_storage_uses() deduplicates accurately without O(U^2) loops.
		GDExtensionProductionResourceV1 resources[10] = {};
		for (uint32_t i = 0; i < 8; ++i) {
			resources[i] = { GDEXTENSION_PRODUCTION_STORAGE_V1, i, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 };
		}
		resources[8] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 8, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 1, 32, 0 };
		resources[9] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 };

		GDExtensionProductionUseV1 dup_uses[32] = {};
		for (uint32_t u = 0; u < 32; ++u) {
			dup_uses[u] = { u % 8u, GDEXTENSION_PRODUCTION_READ_V1, 0, 64, 0 };
		}
		const GDExtensionProductionActionV2 single_action[1] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 32, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
		};

		Request *r0 = nullptr;
		Request *r1 = nullptr;
		Request *r2 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op0{ production_dto_abi<GDExtensionProductionOperationV1>(), 18501, 1, 10, 11 };
			const GDExtensionProductionOperationV1 op1{ production_dto_abi<GDExtensionProductionOperationV1>(), 18502, 1, 10, 11 };
			const GDExtensionProductionOperationV1 op2{ production_dto_abi<GDExtensionProductionOperationV1>(), 18503, 1, 10, 11 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op0, req_cap, { nullptr, single_action }, 1, resources, 8, dup_uses, 32, {}, 0, r0, 2) == OK);
			CHECK(att->uses_free_cursor == req_cap.resource_uses);
			CHECK(att->actions_free_cursor == req_cap.suboperations);
			CHECK(r0->native_workers_bound == MIN(8u, e.rd.transfer_worker_pool_max_size));

			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op1, req_cap, { nullptr, single_action }, 1, resources, 8, dup_uses, 32, {}, 0, r1, 2) == OK);
			CHECK(att->uses_free_cursor == 2u * req_cap.resource_uses);
			CHECK(r1->uses == att->uses + req_cap.resource_uses);

			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op2, req_cap, { nullptr, single_action }, 1, resources, 8, dup_uses, 32, {}, 0, r2, 2) == OK);
			CHECK(att->uses_free_cursor == 3u * req_cap.resource_uses);
			CHECK(r2->uses == att->uses + 2u * req_cap.resource_uses);

			// O(1) lookup via production_serial_index resolves every live request.
			CHECK(production_serial_index_request_count(*e.registry) == 3u);
			for (Request *req : { r0, r1, r2 }) {
				Request *found = nullptr;
				RenderingDevice *rd = nullptr;
				REQUIRE(e.registry->production_request_locked({ Enrollment::INSTANCE, req->serial }, true, found, rd) == OK);
				CHECK(found == req);
			}
		}

		// Out-of-order release of r1 rewinds free_cursor to r1's hole; r3 MUST reuse r1's exact slice.
		const uint64_t r1_old_serial = r1->serial;
		const ProductionUseCell *r1_uses_slot = r1->uses;
		const GDExtensionProductionRequestRefV1 ref0{ Enrollment::INSTANCE, r0->serial };
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		const GDExtensionProductionRequestRefV1 ref2{ Enrollment::INSTANCE, r2->serial };
		REQUIRE(e.registry->production_cancel(ref1).native_error == OK);
		REQUIRE(e.registry->production_release(ref1).native_error == OK);
		CHECK(att->uses_free_cursor == req_cap.resource_uses);
		{
			MutexLock lock(e.registry->mutex);
			Request *missing = nullptr;
			RenderingDevice *rd = nullptr;
			CHECK(e.registry->production_request_locked({ Enrollment::INSTANCE, r1_old_serial }, false, missing, rd) == ERR_DOES_NOT_EXIST);
			CHECK_FALSE(production_serial_index_has_request(*e.registry, r1_old_serial));
		}

		Request *r3 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op3{ production_dto_abi<GDExtensionProductionOperationV1>(), 18504, 1, 10, 11 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op3, req_cap, { nullptr, single_action }, 1, resources, 8, dup_uses, 32, {}, 0, r3, 2) == OK);
			CHECK(r3->uses == r1_uses_slot);
			CHECK(att->uses_free_cursor == 2u * req_cap.resource_uses);
		}
		const GDExtensionProductionRequestRefV1 ref3{ Enrollment::INSTANCE, r3->serial };
		REQUIRE(e.registry->production_cancel(ref0).native_error == OK);
		REQUIRE(e.registry->production_release(ref0).native_error == OK);
		REQUIRE(e.registry->production_cancel(ref2).native_error == OK);
		REQUIRE(e.registry->production_release(ref2).native_error == OK);
		REQUIRE(e.registry->production_cancel(ref3).native_error == OK);
		REQUIRE(e.registry->production_release(ref3).native_error == OK);
		CHECK(att->children == 0);
		CHECK(att->uses_free_cursor == 0);
		CHECK(att->actions_free_cursor == 0);

		// Verify NATIVE_OUTPUT_GUARD_V1 O(U) readback validation parity:
		// (a) valid guard readback accepted, (b) leaked use in another action rejected,
		// (c) duplicate guard readback on same slot rejected.
		const GDExtensionProductionActionV2 guard_actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
		};
		const GDExtensionProductionUseV1 guard_uses_ok[2] = {
			{ 9, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
			{ 8, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
		};
		const GDExtensionProductionUseV1 guard_uses_leaked[2] = {
			{ 8, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 8, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
		};
		const GDExtensionProductionReadbackSourceV1 guard_rb_ok[1] = {
			{ 8, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 1, 0, 32, { 0 }, {} },
		};
		Request *good = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			Request *bad = nullptr;
			const GDExtensionProductionOperationV1 op_bad{ production_dto_abi<GDExtensionProductionOperationV1>(), 18505, 1, 10, 12 };
			CHECK(e.registry->production_reserve_metadata_rows_locked(*att, op_bad, req_cap, { nullptr, guard_actions }, 2, resources, 10, guard_uses_leaked, 2, { nullptr, nullptr, 0, guard_rb_ok }, 1, bad, 2) == ERR_INVALID_DATA);
			CHECK(bad == nullptr);

			const GDExtensionProductionOperationV1 op_good{ production_dto_abi<GDExtensionProductionOperationV1>(), 18506, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op_good, req_cap, { nullptr, guard_actions }, 2, resources, 10, guard_uses_ok, 2, { nullptr, nullptr, 0, guard_rb_ok }, 1, good, 2) == OK);
			REQUIRE(good != nullptr);
		}
		const GDExtensionProductionRequestRefV1 good_ref{ Enrollment::INSTANCE, good->serial };
		REQUIRE(e.registry->production_cancel(good_ref).native_error == OK);
		REQUIRE(e.registry->production_release(good_ref).native_error == OK);

		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
		CHECK(production_serial_index_attachment_count(*e.registry) == 0u);
		CHECK(production_serial_index_request_count(*e.registry) == 0u);
	}
	static void bug186a_record_receive_gates_transient_disposal_busy() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto c = capacity(4);
		c.requests = 2;
		const auto att_ref = e.accept(c);
		REQUIRE(att_ref.handle_serial != 0);

		Request *r = nullptr;
		Request *r_ctrl = nullptr;
		auto c_req = capacity(4);
		c_req.requests = 1;
		GDExtensionProductionCapacityV1 c_ctrl = {};
		c_ctrl.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		c_ctrl.requests = 1;
		const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 18601, 1, 10, 11 };
		const GDExtensionProductionOperationV1 op_ctrl{ production_dto_abi<GDExtensionProductionOperationV1>(), 18602, 1, 10, 11 };
		const GDExtensionProductionActionV2 row{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		struct ReceiverState {
			uint32_t calls = 0;
		} state;
		{
			MutexLock lock(e.registry->mutex);
			ProductionAttachment *a = nullptr;
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, a, rd) == OK);
			a->opened = true;
			a->record_receiver = [](void *p_userdata, GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {
				++static_cast<ReceiverState *>(p_userdata)->calls;
			};
			a->record_receiver_userdata = &state;
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, op, c_req, { nullptr, &row }, 1, nullptr, 0, nullptr, 0, {}, 0, r, 2) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*a, op_ctrl, c_ctrl, {}, 0, nullptr, 0, nullptr, 0, {}, 0, r_ctrl, 2) == OK);
		}
		REQUIRE(r != nullptr);
		REQUIRE(r->action_count == 1);
		REQUIRE(r_ctrl != nullptr);
		REQUIRE(r_ctrl->action_count == 0);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		const GDExtensionProductionRequestRefV1 ctrl_ref{ Enrollment::INSTANCE, r_ctrl->serial };

		// 1. Transient disposal_active=1 with action_count > 0 && request.frozen=true (actual RD recording)
		//    returns ERR_BUSY BEFORE invoking record_receiver (eliminating ERROR: Recording is transiently busy spam).
		e.rd.native_disposal_active.store(true);
		{
			Registry::Lock lock(*e.registry);
			r->frozen = true;
			r->control.serial = 1;
			r->control.state = ProductionControlState::EXECUTING;
			r->control.input.output_cancel = false;
			CHECK(e.registry->production_record_receive_locked(*r, e.rd) == ERR_BUSY);
			r->control.state = ProductionControlState::OWNER_CONSUMED;
		}
		CHECK(state.calls == 0);

		// 2. Frozen control-plane RECORD (action_count == 0 && request.frozen == true, e.g., Create/Withdraw/Poll/Seal
		//    frozen by production_record_schedule) MUST execute cleanly during transient disposal_active=1 (BUG-187A).
		{
			Registry::Lock lock(*e.registry);
			REQUIRE(e.registry->production_freeze_locked(*r_ctrl) == OK);
			REQUIRE(r_ctrl->frozen);
			r_ctrl->control.serial = 10;
			r_ctrl->control.state = ProductionControlState::EXECUTING;
			r_ctrl->control.input.output_cancel = false;
			CHECK(e.registry->production_record_receive_locked(*r_ctrl, e.rd) == OK);
			r_ctrl->control.state = ProductionControlState::OWNER_CONSUMED;
		}
		CHECK(state.calls == 1);

		// 3. Unfrozen request (request.frozen=false) MUST also execute cleanly during transient disposal_active=1.
		{
			Registry::Lock lock(*e.registry);
			r->frozen = false;
			r->control.serial = 2;
			r->control.state = ProductionControlState::EXECUTING;
			r->control.input.output_cancel = false;
			CHECK(e.registry->production_record_receive_locked(*r, e.rd) == OK);
			r->control.state = ProductionControlState::OWNER_CONSUMED;
		}
		CHECK(state.calls == 2);

		// 4. Cleanup (output_cancel=true) is STILL permitted even when frozen with actions during disposal_active=1.
		{
			Registry::Lock lock(*e.registry);
			r->frozen = true;
			r->control.serial = 3;
			r->control.state = ProductionControlState::EXECUTING;
			r->control.input.output_cancel = true;
			CHECK(e.registry->production_record_receive_locked(*r, e.rd) == OK);
			r->control.input.output_cancel = false;
			r->control.state = ProductionControlState::OWNER_CONSUMED;
		}
		CHECK(state.calls == 3);

		// 5. Clearing native_disposal_active restores normal frozen action-recording RECORD dispatch.
		e.rd.native_disposal_active.store(false);
		{
			Registry::Lock lock(*e.registry);
			r->frozen = true;
			r->control.serial = 4;
			r->control.state = ProductionControlState::EXECUTING;
			CHECK(e.registry->production_record_receive_locked(*r, e.rd) == OK);
			r->control.state = ProductionControlState::OWNER_CONSUMED;
			r->frozen = false;
		}
		CHECK(state.calls == 4);

		REQUIRE(e.registry->production_cancel(ctrl_ref).native_error == OK);
		REQUIRE(e.registry->production_release(ctrl_ref).native_error == OK);
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		REQUIRE(e.registry->production_release(ref).native_error == OK);
		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}
	static void bug186c_bidirectional_readback_and_natives_placement_and_reclaim() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(128);
		pool_cap.requests = 4;
		pool_cap.suboperations = 32;
		pool_cap.resources = 32;
		pool_cap.resource_uses = 64;
		pool_cap.readbacks = 8;
		pool_cap.graph_memberships = 32;
		pool_cap.native_memberships = 128;
		pool_cap.return_cells = 32;
		pool_cap.retained_payload_bytes = 64u * 1024u;
		pool_cap.readback_bytes = 4u * 1024u * 1024u;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);

		ProductionAttachment *att = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		// 1. Reserve transient sim request r_sim1 with 3 MiB readback + 32 natives (bottom-up at 0).
		auto sim1_cap = capacity(32);
		sim1_cap.requests = 1;
		sim1_cap.suboperations = 2;
		sim1_cap.resources = 2;
		sim1_cap.resource_uses = 4;
		sim1_cap.readbacks = 1;
		sim1_cap.graph_memberships = 4;
		sim1_cap.native_memberships = 32;
		sim1_cap.return_cells = 4;
		sim1_cap.retained_payload_bytes = 1024;
		sim1_cap.readback_bytes = 3u * 1024u * 1024u;

		const GDExtensionProductionResourceV1 sim_res[1] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 4u * 1024u * 1024u, 0 },
		};
		const GDExtensionProductionActionV2 sim_actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
		};
		const GDExtensionProductionUseV1 sim1_uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 3u * 1024u * 1024u, 0 },
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 3u * 1024u * 1024u, 0 },
		};
		const GDExtensionProductionReadbackV2 sim1_rb[1] = {
			{ 0, 0, 0, 1, 0, 3u * 1024u * 1024u },
		};
		Request *r_sim1 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 18610, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, sim1_cap, { nullptr, sim_actions }, 2, sim_res, 1, sim1_uses, 2, { nullptr, sim1_rb, 0, nullptr }, 1, r_sim1, 2) == OK);
		}
		REQUIRE(r_sim1 != nullptr);
		CHECK(r_sim1->readback_offset == 0u);
		CHECK(r_sim1->natives_offset == 0u);

		// 2. Reserve long-lived NATIVE_OUTPUT_V1 request r_out with 32B guard readback + 16 natives.
		//    Must be placed TOP-DOWN at the ceiling so it NEVER bisects the bottom-up sim region!
		auto out_cap = capacity(16);
		out_cap.requests = 1;
		out_cap.suboperations = 2;
		out_cap.resources = 2;
		out_cap.resource_uses = 4;
		out_cap.readbacks = 1;
		out_cap.graph_memberships = 4;
		out_cap.native_memberships = 16;
		out_cap.return_cells = 4;
		out_cap.retained_payload_bytes = 1024;
		out_cap.readback_bytes = 32u;

		const GDExtensionProductionResourceV1 out_res[2] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 64, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 1, 32, 0 },
		};
		const GDExtensionProductionActionV2 out_actions[2] = {
			{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
		};
		const GDExtensionProductionUseV1 out_uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
		};
		const GDExtensionProductionReadbackSourceV1 out_rb[1] = {
			{ 1, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 1, 0, 32, { 0 }, {} },
		};
		Request *r_out = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 18611, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, out_cap, { nullptr, out_actions }, 2, out_res, 2, out_uses, 2, { nullptr, nullptr, 0, out_rb }, 1, r_out, 2) == OK);
		}
		REQUIRE(r_out != nullptr);
		CHECK(r_out->readback_offset == pool_cap.readback_bytes - 32u);
		CHECK(r_out->natives_offset == pool_cap.native_memberships - 16u);
		CHECK(att->readback_reclaim_top_lwm == pool_cap.readback_bytes - 32u);
		CHECK(att->natives_reclaim_top_lwm == pool_cap.native_memberships - 16u);

		// 3. Release r_sim1, then reserve a 3.9 MiB transient sim request r_sim2 while r_out stays pinned.
		//    Previously r_out sat at 3 MiB, bisecting 4 MiB into [0..3M] + [3M+32..4M] and causing ERR_BUSY!
		const GDExtensionProductionRequestRefV1 sim1_ref{ Enrollment::INSTANCE, r_sim1->serial };
		REQUIRE(e.registry->production_cancel(sim1_ref).native_error == OK);
		REQUIRE(e.registry->production_release(sim1_ref).native_error == OK);

		constexpr uint64_t kLargeSimBytes = 3900u * 1024u;
		auto sim2_cap = sim1_cap;
		sim2_cap.readback_bytes = kLargeSimBytes;
		const GDExtensionProductionUseV1 sim2_uses[2] = {
			{ 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kLargeSimBytes, 0 },
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, kLargeSimBytes, 0 },
		};
		const GDExtensionProductionReadbackV2 sim2_rb[1] = {
			{ 0, 0, 0, 1, 0, kLargeSimBytes },
		};
		Request *r_sim2 = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			const GDExtensionProductionOperationV1 op{ production_dto_abi<GDExtensionProductionOperationV1>(), 18612, 1, 10, 12 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, sim2_cap, { nullptr, sim_actions }, 2, sim_res, 1, sim2_uses, 2, { nullptr, sim2_rb, 0, nullptr }, 1, r_sim2, 2) == OK);
		}
		REQUIRE(r_sim2 != nullptr);
		CHECK(r_sim2->readback_offset == 0u);
		CHECK(r_sim2->natives_offset == 0u);

		// 4. Release r_sim2 and r_out, then trigger idle reclaim -> BOTH low HWM and top LWM reset cleanly!
		const GDExtensionProductionRequestRefV1 sim2_ref{ Enrollment::INSTANCE, r_sim2->serial };
		const GDExtensionProductionRequestRefV1 out_ref{ Enrollment::INSTANCE, r_out->serial };
		REQUIRE(e.registry->production_cancel(sim2_ref).native_error == OK);
		REQUIRE(e.registry->production_release(sim2_ref).native_error == OK);
		REQUIRE(e.registry->production_cancel(out_ref).native_error == OK);
		REQUIRE(e.registry->production_release(out_ref).native_error == OK);
		CHECK(att->children == 0);
		CHECK(att->has_reclaimable_arena_pages());
		CHECK(att->readback_reclaim_hwm == kLargeSimBytes);
		CHECK(att->readback_reclaim_top_lwm == pool_cap.readback_bytes - 32u);
		CHECK(att->natives_reclaim_hwm == 32u);
		CHECK(att->natives_reclaim_top_lwm == pool_cap.native_memberships - 16u);

		for (uint32_t f = 0; f < ProductionAttachment::ARENA_IDLE_RECLAIM_FRAMES; ++f) {
			e.registry->production_observe_completed_frames_rt(e.rd);
		}
		CHECK(att->readback_reclaim_hwm == 0u);
		CHECK(att->readback_reclaim_top_lwm == UINT64_MAX);
		CHECK(att->natives_reclaim_hwm == 0u);
		CHECK(att->natives_reclaim_top_lwm == UINT64_MAX);
		CHECK_FALSE(att->has_reclaimable_arena_pages());

		REQUIRE(e.registry->production_close(att_ref).native_error == OK);
	}

	static void bug195d_persistent_set_lru_and_steady_state_reap() {
		using namespace GDExtensionDeviceExit;
		static_assert(ProductionAttachment::PERSISTENT_SET_MAX_LIVE == 4096u, "BUG-195D-1 cap");
		Enrollment e;
		e.driver.allow_second_set = true;
		auto c = capacity(4);
		c.resources = 4;
		c.resource_uses = 8;
		c.graph_memberships = 4;
		c.native_memberships = 8;
		c.return_cells = 4;
		auto pool = c;
		pool.requests = 4;
		pool.suboperations *= 4;
		pool.resources *= 4;
		pool.resource_uses *= 4;
		pool.graph_memberships *= 4;
		pool.native_memberships *= 4;
		pool.return_cells *= 4;
		pool.retained_payload_bytes *= 4;
		const auto attachment_ref = e.accept(pool);
		REQUIRE(attachment_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(attachment_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		auto make_storage = [&](uint64_t serial) -> Registry::QualificationBridge::Storage * {
			auto *st = memnew(Registry::QualificationBridge::Storage);
			st->serial = serial;
			st->creating = false;
			st->origin = { Enrollment::ABI, 128, 1, 0, 1, 1, 0 };
			st->physical.bridge = e.original_bridge->serial;
			st->physical.child = st->serial;
			const RID srid = e.rd._qualification_storage_create(128, &st->physical);
			REQUIRE(srid.is_valid());
			const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
			REQUIRE(sbuf != nullptr);
			st->physical.rid = srid.get_id();
			st->backing = { Enrollment::ABI, { Enrollment::INSTANCE, st->serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
			return st;
		};
		Registry::QualificationBridge::Storage *storage_a = nullptr;
		Registry::QualificationBridge::Storage *storage_b = nullptr;

		auto use_set = [&](uint64_t op_serial, Registry::QualificationBridge::Storage *&st, bool init_unit, uint64_t st_serial) -> RID {
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), op_serial, 1, 10, 11 };
			const GDExtensionProductionActionV2 action = { 0, 10, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			};
			const GDExtensionProductionUseV1 use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 128, 0 };
			Request *r = nullptr;
			{
				MutexLock lock(e.registry->mutex);
				REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, &action }, 1, resources, 3, &use, 1, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
			}
			REQUIRE(r != nullptr);
			const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
			if (init_unit) {
				REQUIRE(import_original_unit(e, *r));
			} else {
				REQUIRE(e.registry->production_import_unit(ref, 0, &e.original_unit->backing).native_error == OK);
			}
			if (!st) {
				st = make_storage(st_serial);
				if (init_unit) {
					e.original_bridge->storage[0] = st;
					e.original_storage = st;
				} else {
					e.original_bridge->storage[1] = st;
				}
			}
			REQUIRE(e.registry->production_import_storage(ref, 1, &st->backing).native_error == OK);
			const GDExtensionProductionSetBindingV1 binding = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
			REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &binding, 1).native_error == OK);
			REQUIRE(r->resources[2].cached_persistent_set);
			const RID rid = r->resources[2].primary;
			REQUIRE(e.registry->production_cancel(ref).native_error == OK);
			REQUIRE(e.registry->production_release(ref).native_error == OK);
			return rid;
		};

		const RID rid_a = use_set(9501, storage_a, true, 951);
		const RID rid_b = use_set(9502, storage_b, false, 952);
		REQUIRE(rid_a != rid_b);

		// Verify O(1) intrusive LRU order: B is MRU head, A is LRU tail.
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(att->persistent_sets.size() == 2);
			REQUIRE(att->persistent_sets_pending_reap.is_empty());
			REQUIRE(att->persistent_set_lru_head != nullptr);
			REQUIRE(att->persistent_set_lru_tail != nullptr);
			CHECK(att->persistent_set_lru_head->rid == rid_b);
			CHECK(att->persistent_set_lru_tail->rid == rid_a);
		}

		// Main-thread cache hit on A must promote A to MRU head and demote B to LRU tail in O(1).
		const RID rid_a_hit = use_set(9503, storage_a, false, 951);
		CHECK(rid_a_hit == rid_a);
		{
			MutexLock lock(e.registry->mutex);
			CHECK(att->persistent_set_lru_head->rid == rid_a);
			CHECK(att->persistent_set_lru_tail->rid == rid_b);
		}

		// Steady-state RT reap with 0 evicted sets MUST scan 0 entries (zero per-entry mutex locks!).
		const uint64_t scanned_before = att->persistent_set_reap_entries_scanned;
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
		}
		CHECK(att->persistent_set_reap_entries_scanned == scanned_before);

		// Evicting ONLY storage_b places ONLY B on persistent_sets_pending_reap; reap touches ONLY B.
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_evict_for_dependency_rt(&e.rd, &storage_b->physical);
		}
		CHECK(att->persistent_set_lru_head != nullptr);
		CHECK(att->persistent_set_lru_head->rid == rid_a);
		CHECK(att->persistent_set_lru_tail == att->persistent_set_lru_head);
		e.rd._free_pending_resources(0);
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
		}
		CHECK(att->persistent_sets.size() == 1);
		CHECK(att->persistent_sets_pending_reap.is_empty());
		const uint64_t scanned_after_evict = att->persistent_set_reap_entries_scanned;
		CHECK(scanned_after_evict > scanned_before);
		// Subsequent steady-state reap with A still live again scans 0 entries!
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			e.registry->production_persistent_sets_reap_and_retire_rt(&e.rd, true);
		}
		CHECK(att->persistent_set_reap_entries_scanned == scanned_after_evict);

		// Clean attachment close drains A and clears all LRU & pending_reap invariants.
		CHECK(e.registry->production_close(attachment_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(attachment_ref).native_error == OK);

		for (auto *st : { storage_a, storage_b }) {
			st->physical.use_closed.store(true);
			e.rd.qualification_retiring = true;
			e.rd.free_rid(RID::from_uint64(st->backing.rid));
			e.rd.qualification_retiring = false;
			e.rd._free_pending_resources(0);
		}
	}

	static void bug195a_dispatch_war_raw_waw_clean_split_and_alias_canonicalization() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.allow_second_set = true;
		constexpr uint32_t kActions = 11;
		// Slots: 0 = unit, 1 = S0, 2 = S1, 3 = S0_alias (same physical RID as slot 1),
		//        4 = set(S0), 5 = set(S1), 6 = set(S0_alias).
		constexpr uint32_t kResources = 7;
		auto c = capacity(16);
		c.suboperations = 16;
		c.resources = 8;
		c.resource_uses = 32;
		c.graph_memberships = 32;
		c.native_memberships = 64;
		c.return_cells = 8;
		c.retained_payload_bytes = 512;
		const auto att_ref = e.accept(c);
		REQUIRE(att_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 19501, 1, 10, 10 + kActions };
		GDExtensionProductionActionV2 actions[kActions] = {};
		for (uint32_t i = 0; i < kActions; ++i) {
			actions[i] = {
				i,
				10 + i,
				0,
				0,
				GDEXTENSION_PRODUCTION_DISPATCH_V1,
				0,
				i, // first_use (1 use per dispatch)
				1, // use_count
				uint32_t(i + 1 == kActions ? GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 : GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1),
				UINT32_MAX,
			};
		}
		const GDExtensionProductionResourceV1 resources[kResources] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 128, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 4, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 5, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 6, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
		};
		const GDExtensionProductionUseV1 uses[kActions] = {
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },  // 0: S0 W[0,32)  -> open list (E0)
			{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },   // 1: S1 R[0,32)  -> disjoint resource (no split)
			{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },   // 2: S1 R[0,32)  -> RAR same span (no split)
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 64, 32, 0 }, // 3: S0 W[64,96) -> non-overlapping span on S0 (no split)
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },  // 4: S1 W[0,32)  -> WAR on S1! (split -> E0+1)
			{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },   // 5: S1 R[0,32)  -> RAW on S1! (split -> E0+2)
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },  // 6: S0 W[0,32)  -> disjoint in E0+2 (no split)
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 16, 32, 0 }, // 7: S0 W[16,48) -> WAW on S0! (split -> E0+3)
			{ 3, GDEXTENSION_PRODUCTION_READ_V1, 16, 32, 0 },  // 8: S0_alias R[16,48) -> aliased RAW! (split -> E0+4)
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 16, 32, 0 }, // 9: S0 W[16,48) -> aliased WAR! (split -> E0+5)
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 16, 32, 0 }, // 10: S0_alias W[16,48) -> aliased WAW! (split -> E0+6)
		};

		Request *r = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, actions }, kActions, resources, kResources, uses, kActions, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(import_original_unit(e, *r, true, false));

		auto make_storage = [&](uint64_t serial, uint32_t identity) -> Registry::QualificationBridge::Storage * {
			auto *st = memnew(Registry::QualificationBridge::Storage);
			st->serial = serial;
			st->creating = false;
			st->origin = { Enrollment::ABI, 128, identity, 0, 1, 1, 0 };
			st->physical.bridge = e.original_bridge->serial;
			st->physical.child = st->serial;
			const RID srid = e.rd._qualification_storage_create(128, &st->physical);
			REQUIRE(srid.is_valid());
			const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
			REQUIRE(sbuf != nullptr);
			st->physical.rid = srid.get_id();
			st->backing = { Enrollment::ABI, { Enrollment::INSTANCE, st->serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
			return st;
		};

		Registry::QualificationBridge::Storage *s0 = make_storage(1951, 1);
		Registry::QualificationBridge::Storage *s1 = make_storage(1952, 2);
		e.original_bridge->storage[0] = s0;
		e.original_bridge->storage[1] = s1;
		e.original_storage = s0;

		REQUIRE(e.registry->production_import_storage(ref, 1, &s0->backing).native_error == OK);
		REQUIRE(e.registry->production_import_storage(ref, 2, &s1->backing).native_error == OK);
		REQUIRE(e.registry->production_import_storage(ref, 3, &s0->backing).native_error == OK);
		const GDExtensionProductionSetBindingV1 bind_s0 = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, 128 };
		const GDExtensionProductionSetBindingV1 bind_s1 = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 2, 0, 128 };
		const GDExtensionProductionSetBindingV1 bind_alias = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 3, 0, 128 };
		REQUIRE(e.registry->production_set_create(ref, 4, 0, 0, &bind_s0, 1).native_error == OK);
		REQUIRE(e.registry->production_set_create(ref, 5, 0, 0, &bind_s1, 1).native_error == OK);
		REQUIRE(e.registry->production_set_create(ref, 6, 0, 0, &bind_alias, 1).native_error == OK);

		const uint32_t push[4] = { 1, 2, 3, 4 };
		for (uint32_t i = 0; i < kActions; ++i) {
			const uint32_t set_slot = (uses[i].resource_slot == 1u) ? 4u : (uses[i].resource_slot == 2u ? 5u : 6u);
			const GDExtensionProductionDispatchV2 disp = {
				production_dto_abi<GDExtensionProductionDispatchV2>(),
				i,
				10 + i,
				0,
				0,
				0,
				set_slot,
				1,
				2,
				3,
				4,
				sizeof(push),
				0,
			};
			REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &disp, push, sizeof(push)).native_error == OK);
		}
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_freeze_rd_locked(*r, &e.rd) == OK);
		}

		void *saved_frame = nullptr;
		e.registry->test_push_production_physical_frame(19501, saved_frame);
		const uint32_t initial_epoch = r->dispatch_barrier_epoch;
		const uint32_t expected_epoch_delta[kActions] = {
			1u, // 0: fresh open
			1u, // 1: disjoint resource
			1u, // 2: RAR
			1u, // 3: non-overlapping span on S0
			2u, // 4: WAR on S1
			3u, // 5: RAW on S1
			3u, // 6: disjoint in new epoch
			4u, // 7: WAW on S0
			5u, // 8: aliased RAW (slot 3 -> canon slot 1)
			6u, // 9: aliased WAR (slot 1 vs canon slot 1 read by slot 3)
			7u, // 10: aliased WAW (slot 3 vs canon slot 1 written by slot 1)
		};

		for (uint32_t i = 0; i < kActions; ++i) {
			const auto obs = effect(e, *r, i);
			REQUIRE(obs.error == OK);
			REQUIRE(obs.state == CheckedRecordAppend::State::Appended);
			CHECK(r->dispatch_barrier_epoch == initial_epoch + expected_epoch_delta[i]);
			if (i == 0) {
				// Canonical alias slot verification: slot 3 (alias of S0) maps to slot 1!
				CHECK(r->resources[1].hazard_canonical_slot == 1u);
				CHECK(r->resources[2].hazard_canonical_slot == 2u);
				CHECK(r->resources[3].hazard_canonical_slot == 1u);
			}
			CHECK(r->compute_list_open == (i + 1u < kActions));
		}
		e.registry->test_pop_production_physical_frame(saved_frame);

		// Flush RDG via execute_transfers to verify BUG-195A + BUG-199B:
		// - Exactly 11 dispatches executed,
		// - Exactly 1 compute pass / 1 pipeline bind (ZERO encoder splits or dead rebinds!),
		// - Exactly 6 intra-pass compute memory barriers (6 genuine hazards, ZERO false barriers),
		// - Exactly 11 uniform-set binds.
		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		e.driver.complete = true;
		execute_transfers(e, *r, true);
		CHECK(e.driver.dispatches == kActions);
		CHECK(e.driver.pipeline_binds == 1u);
		CHECK(e.driver.compute_pass_begins == 1u);
		CHECK(e.driver.compute_pass_ends == 1u);
		CHECK(e.driver.compute_memory_barriers == 6u);
		CHECK(e.driver.set_binds == kActions);

		const auto observed_done = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(observed_done.report.known_obligations > 0);
		CHECK(observed_done.report.complete_obligations == observed_done.report.known_obligations);
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(att_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(att_ref).native_error == OK);
		for (auto *st : { s0, s1 }) {
			st->physical.use_closed.store(true);
			e.rd.qualification_retiring = true;
			e.rd.free_rid(RID::from_uint64(st->backing.rid));
			e.rd.qualification_retiring = false;
			e.rd._free_pending_resources(0);
		}
	}

	static void bug196a_rdg_o1_record_lookup_and_inverted_frame_admission() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto pool_cap = capacity(16);
		pool_cap.requests = 4;
		pool_cap.graph_memberships = 16;
		const auto att_ref = e.accept(pool_cap);
		REQUIRE(att_ref.handle_serial != 0);
		graph_initialize(e);

		// Part 1: Verify O(1) compute_list_command_indices / draw_list_command_indices
		// parity across interleaved closed compute & draw lists plus active open lists.
		e.rd.draw_graph.begin(true);
		constexpr uint32_t kTotalClosed = 8;
		RenderingDeviceGraph::RecordHandle closed_instr[kTotalClosed] = {};
		RenderingDeviceGraph::RecordHandle closed_end[kTotalClosed] = {};
		RenderingDeviceGraph::RecordHandle draw_begin_handles[kTotalClosed] = {};
		uint32_t expected_cmd_idx[kTotalClosed] = {};

		for (uint32_t i = 0; i < kTotalClosed; ++i) {
			if (i % 3 == 2) {
				draw_begin_handles[i] = e.rd.draw_graph.add_draw_list_begin(
						RDD::RenderPassID(1),
						RDD::FramebufferID(1),
						Rect2i(0, 0, 16, 16),
						VectorView<RenderingDeviceGraph::AttachmentOperation>(),
						VectorView<RDD::RenderPassClearValue>(),
						RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
				closed_instr[i] = e.rd.draw_graph.add_draw_list_draw(3, 1);
				// Open draw list: contains_record == true, _resolve_record_command_index == false.
				CHECK(e.rd.draw_graph.contains_record(draw_begin_handles[i]));
				CHECK(e.rd.draw_graph.contains_record(closed_instr[i]));
				uint32_t open_idx = UINT32_MAX;
				CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(draw_begin_handles[i], open_idx));
				CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(closed_instr[i], open_idx));
				closed_end[i] = e.rd.draw_graph.add_draw_list_end();
			} else {
				e.rd.draw_graph.add_compute_list_begin();
				closed_instr[i] = e.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
				CHECK(e.rd.draw_graph.contains_record(closed_instr[i]));
				uint32_t open_idx = UINT32_MAX;
				CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(closed_instr[i], open_idx));
				closed_end[i] = e.rd.draw_graph.add_compute_list_end();
			}
			expected_cmd_idx[i] = i;
		}

		// Also leave one compute list actively open while querying all closed lists.
		e.rd.draw_graph.add_compute_list_begin();
		const auto active_open_dispatch = e.rd.draw_graph.add_compute_list_dispatch(2, 1, 1);
		CHECK(e.rd.draw_graph.contains_record(active_open_dispatch));
		uint32_t resolved = UINT32_MAX;
		CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(active_open_dispatch, resolved));

		for (uint32_t i = 0; i < kTotalClosed; ++i) {
			CHECK(e.rd.draw_graph.contains_record(closed_instr[i]));
			CHECK(e.rd.draw_graph.contains_record(closed_end[i]));
			resolved = UINT32_MAX;
			CHECK(e.rd.draw_graph._resolve_record_command_index(closed_instr[i], resolved));
			CHECK(resolved == expected_cmd_idx[i]);
			resolved = UINT32_MAX;
			CHECK(e.rd.draw_graph._resolve_record_command_index(closed_end[i], resolved));
			CHECK(resolved == expected_cmd_idx[i]);
			if (draw_begin_handles[i].is_valid()) {
				CHECK(e.rd.draw_graph.contains_record(draw_begin_handles[i]));
				resolved = UINT32_MAX;
				CHECK(e.rd.draw_graph._resolve_record_command_index(draw_begin_handles[i], resolved));
				CHECK(resolved == expected_cmd_idx[i]);
			}
		}

		// Invalid / out-of-range handles return false safely without OOB.
		RenderingDeviceGraph::RecordHandle bogus_compute = closed_instr[0];
		bogus_compute.index = 9999;
		CHECK_FALSE(e.rd.draw_graph.contains_record(bogus_compute));
		CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(bogus_compute, resolved));
		RenderingDeviceGraph::RecordHandle bogus_draw = draw_begin_handles[2];
		bogus_draw.index = 9999;
		CHECK_FALSE(e.rd.draw_graph.contains_record(bogus_draw));
		CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(bogus_draw, resolved));

		// Close the open compute list and verify begin() clears O(1) index tables.
		const auto final_closed = e.rd.draw_graph.add_compute_list_end();
		CHECK(e.rd.draw_graph._resolve_record_command_index(active_open_dispatch, resolved));
		CHECK(resolved == kTotalClosed);
		CHECK(e.rd.draw_graph._resolve_record_command_index(final_closed, resolved));
		CHECK(resolved == kTotalClosed);

		e.rd.draw_graph.begin(true);
		CHECK_FALSE(e.rd.draw_graph.contains_record(closed_instr[0]));
		CHECK_FALSE(e.rd.draw_graph._resolve_record_command_index(closed_instr[0], resolved));
		CHECK_FALSE(e.rd.draw_graph.contains_record(draw_begin_handles[2]));

		// Part 2: Verify inverted linear production_maintenance_frame_admission_locked
		// accepts exact multi-request coverage and rejects any uncovered command with ERR_BUSY.
		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		Request *r0 = nullptr;
		Request *r1 = nullptr;
		auto req_cap = capacity(4);
		req_cap.requests = 1;
		req_cap.graph_memberships = 4;
		const GDExtensionProductionActionV2 dummy_action{ 0, 10, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
			const GDExtensionProductionOperationV1 op0{ production_dto_abi<GDExtensionProductionOperationV1>(), 19601, 1, 10, 11 };
			const GDExtensionProductionOperationV1 op1{ production_dto_abi<GDExtensionProductionOperationV1>(), 19602, 1, 10, 11 };
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op0, req_cap, { nullptr, &dummy_action }, 1, nullptr, 0, nullptr, 0, {}, 0, r0, 2) == OK);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op1, req_cap, { nullptr, &dummy_action }, 1, nullptr, 0, nullptr, 0, {}, 0, r1, 2) == OK);
		}
		if (!r0 || !r1) {
			return;
		}

		RenderingDeviceGraph::RecordHandle h[4] = {};
		for (uint32_t i = 0; i < 3; ++i) {
			e.rd.draw_graph.add_compute_list_begin();
			h[i] = e.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
			(void)e.rd.draw_graph.add_compute_list_end();
		}
		REQUIRE(e.rd.draw_graph.qualification_command_count() == 3u);

		// Partial coverage: commands 0 and 2 covered, command 1 uncovered -> ERR_BUSY.
		r0->graphs[0].handle = h[0];
		r0->graph_count = 1;
		r1->graphs[0].handle = h[2];
		r1->graph_count = 1;
		{
			MutexLock lock(e.registry->mutex);
			NativeDrainDiagnostic::Span trace("bug196a.partial", r0->serial, 0, 0, e.rd.frame, 0, 3);
			CHECK(e.registry->production_maintenance_frame_admission_locked(*r0, e.rd, trace) == ERR_BUSY);
		}

		// Complete coverage across r0 (cmd 0) and r1 (cmds 2, 1) -> OK!
		r1->graphs[1].handle = h[1];
		r1->graph_count = 2;
		{
			MutexLock lock(e.registry->mutex);
			NativeDrainDiagnostic::Span trace("bug196a.full", r0->serial, 0, 0, e.rd.frame, 0, 3);
			CHECK(e.registry->production_maintenance_frame_admission_locked(*r0, e.rd, trace) == OK);
		}

		// Add an uncaptured 4th command (cmd 3) -> ERR_BUSY again.
		e.rd.draw_graph.add_compute_list_begin();
		h[3] = e.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
		(void)e.rd.draw_graph.add_compute_list_end();
		{
			MutexLock lock(e.registry->mutex);
			NativeDrainDiagnostic::Span trace("bug196a.uncaptured", r0->serial, 0, 0, e.rd.frame, 0, 4);
			CHECK(e.registry->production_maintenance_frame_admission_locked(*r0, e.rd, trace) == ERR_BUSY);
		}

		r0->graph_count = 0;
		r1->graph_count = 0;
		e.rd.draw_graph.begin(true);
		const GDExtensionProductionRequestRefV1 ref0{ Enrollment::INSTANCE, r0->serial };
		const GDExtensionProductionRequestRefV1 ref1{ Enrollment::INSTANCE, r1->serial };
		REQUIRE(e.registry->production_cancel(ref0).native_error == OK);
		REQUIRE(e.registry->production_release(ref0).native_error == OK);
		REQUIRE(e.registry->production_cancel(ref1).native_error == OK);
		REQUIRE(e.registry->production_release(ref1).native_error == OK);
		CHECK(e.registry->production_close(att_ref).native_error == OK);
	}

	static void bug196a_disjoint_multispan_hazard_tracker_and_overflow_fallback() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		e.driver.expected_set_bindings = 1;

		constexpr uint32_t kActions = 18;
		constexpr uint32_t kResources = 3; // 0 = unit, 1 = S0 (128B), 2 = set(S0)
		constexpr uint32_t kStorageBytes = 128;
		auto c = capacity(32);
		c.suboperations = 32;
		c.resources = 4;
		c.resource_uses = 32;
		c.graph_memberships = 64;
		c.native_memberships = 64;
		c.return_cells = 8;
		c.retained_payload_bytes = 1024;
		const auto att_ref = e.accept(c);
		REQUIRE(att_ref.handle_serial != 0);
		graph_initialize(e);

		ProductionAttachment *att = nullptr;
		RenderingDevice *rd = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_attachment_locked(att_ref, true, att, rd) == OK);
		}
		REQUIRE(att != nullptr);

		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 19610, 1, 10, 10 + kActions };
		GDExtensionProductionActionV2 actions[kActions] = {};
		for (uint32_t i = 0; i < kActions; ++i) {
			actions[i] = {
				i,
				10 + i,
				0,
				0,
				GDEXTENSION_PRODUCTION_DISPATCH_V1,
				0,
				i,
				1,
				uint32_t(i + 1 == kActions ? GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 : GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1),
				UINT32_MAX,
			};
		}
		const GDExtensionProductionResourceV1 resources[kResources] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, kStorageBytes, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
		};
		const GDExtensionProductionUseV1 uses[kActions] = {
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 8, 0 },   // 0: R[0,8)      -> open list (E0+1), read_spans=1
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 64, 8, 0 },  // 1: R[64,72)    -> RAR (no split), read_spans=2
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 16, 8, 0 }, // 2: W[16,24)    -> disjoint gap between reads! ZERO split!
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 24, 8, 0 }, // 3: W[24,32)    -> adjacent write coalesces to [16,32), ZERO split!
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 4, 8, 0 },  // 4: W[4,12)     -> overlaps R[0,8) -> WAR split (E0+2)
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 8, 0 },   // 5: R[0,8)      -> overlaps W[4,12) -> RAW split (E0+3)
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 64, 8, 0 },  // 6: R[64,72)    -> RAR (no split), read_spans=2
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 68, 8, 0 }, // 7: W[68,76)    -> overlaps R[64,72) -> WAR split (E0+4)
			// 8..15: 8 disjoint READ spans in epoch E0+4 (all disjoint from W[68,76)):
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 4, 0 },   // 8:  span 1
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 8, 4, 0 },   // 9:  span 2
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 16, 4, 0 },  // 10: span 3
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 24, 4, 0 },  // 11: span 4
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 32, 4, 0 },  // 12: span 5
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 40, 4, 0 },  // 13: span 6
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 48, 4, 0 },  // 14: span 7
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 56, 4, 0 },  // 15: span 8 (MAX_HAZARD_SPANS)
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 112, 4, 0 }, // 16: 9th disjoint READ -> overflow collapses to hull [0,116), no split!
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 88, 4, 0 }, // 17: W[88,92) inside collapsed hull [0,116) -> conservative WAR split (E0+5)
		};

		Request *r = nullptr;
		{
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_reserve_metadata_rows_locked(*att, op, c, { nullptr, actions }, kActions, resources, kResources, uses, kActions, { nullptr, nullptr, 0, nullptr }, 0, r, 2) == OK);
		}
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };
		REQUIRE(import_original_unit(e, *r, true, false));

		auto *s0 = memnew(Registry::QualificationBridge::Storage);
		s0->serial = 1961;
		s0->creating = false;
		s0->origin = { Enrollment::ABI, kStorageBytes, 1, 0, 1, 1, 0 };
		s0->physical.bridge = e.original_bridge->serial;
		s0->physical.child = s0->serial;
		const RID srid = e.rd._qualification_storage_create(kStorageBytes, &s0->physical);
		REQUIRE(srid.is_valid());
		const auto *sbuf = e.rd.storage_buffer_owner.get_or_null(srid);
		REQUIRE(sbuf != nullptr);
		s0->physical.rid = srid.get_id();
		s0->backing = { Enrollment::ABI, { Enrollment::INSTANCE, s0->serial }, srid.get_id(), sbuf->backing_serial, sbuf->size, uint64_t(sbuf->usage), OK, 1 };
		e.original_bridge->storage[0] = s0;
		e.original_storage = s0;

		REQUIRE(e.registry->production_import_storage(ref, 1, &s0->backing).native_error == OK);
		const GDExtensionProductionSetBindingV1 bind_s0 = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, kStorageBytes };
		REQUIRE(e.registry->production_set_create(ref, 2, 0, 0, &bind_s0, 1).native_error == OK);

		const uint32_t push[4] = { 1, 2, 3, 4 };
		for (uint32_t i = 0; i < kActions; ++i) {
			const GDExtensionProductionDispatchV2 disp = {
				production_dto_abi<GDExtensionProductionDispatchV2>(),
				i,
				10 + i,
				0,
				0,
				0,
				2,
				1,
				2,
				3,
				4,
				sizeof(push),
				0,
			};
			REQUIRE(e.registry->production_prepare_dispatch_v2(ref, &disp, push, sizeof(push)).native_error == OK);
		}
		{
			MutexLock rd_lock(e.rd._thread_safe_);
			MutexLock lock(e.registry->mutex);
			REQUIRE(e.registry->production_freeze_rd_locked(*r, &e.rd) == OK);
		}

		void *saved_frame = nullptr;
		e.registry->test_push_production_physical_frame(19610, saved_frame);
		const uint32_t initial_epoch = r->dispatch_barrier_epoch;
		const uint32_t expected_epoch_delta[kActions] = {
			1u, 1u, 1u, 1u,
			2u, 3u, 3u, 4u,
			4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
			4u, 5u,
		};

		for (uint32_t i = 0; i < kActions; ++i) {
			const auto obs = effect(e, *r, i);
			REQUIRE(obs.error == OK);
			REQUIRE(obs.state == CheckedRecordAppend::State::Appended);
			CHECK(r->dispatch_barrier_epoch == initial_epoch + expected_epoch_delta[i]);
			const auto &slot = r->resources[1];
			if (i == 1) {
				CHECK(slot.last_dispatch_read_span_count == 2u);
				CHECK(slot.last_dispatch_read_spans[0].begin == 0u);
				CHECK(slot.last_dispatch_read_spans[0].end == 8u);
				CHECK(slot.last_dispatch_read_spans[1].begin == 64u);
				CHECK(slot.last_dispatch_read_spans[1].end == 72u);
			} else if (i == 3) {
				// Adjacent writes [16,24) + [24,32) coalesce into [16,32).
				CHECK(slot.last_dispatch_write_span_count == 1u);
				CHECK(slot.last_dispatch_write_spans[0].begin == 16u);
				CHECK(slot.last_dispatch_write_spans[0].end == 32u);
			} else if (i == 15) {
				CHECK(slot.last_dispatch_read_span_count == ProductionResourceSlot::MAX_HAZARD_SPANS);
			} else if (i == 16) {
				// 9th disjoint span overflows to conservative bounding hull [0, 116).
				CHECK(slot.last_dispatch_read_span_count == 1u);
				CHECK(slot.last_dispatch_read_spans[0].begin == 0u);
				CHECK(slot.last_dispatch_read_spans[0].end == 116u);
			}
			CHECK(r->compute_list_open == (i + 1u < kActions));
		}
		e.registry->test_pop_production_physical_frame(saved_frame);

		REQUIRE(e.registry->production_cancel(ref).native_error == OK);
		e.driver.complete = true;
		execute_transfers(e, *r, true);
		CHECK(e.driver.dispatches == kActions);
		CHECK(e.driver.pipeline_binds == 1u);
		CHECK(e.driver.compute_pass_begins == 1u);
		CHECK(e.driver.compute_pass_ends == 1u);
		CHECK(e.driver.compute_memory_barriers == 4u);
		CHECK(e.driver.set_binds == kActions);

		const auto observed_done = turn(e, *r, ProductionControlKind::OBSERVE, budget());
		CHECK(observed_done.report.known_obligations > 0);
		CHECK(observed_done.report.complete_obligations == observed_done.report.known_obligations);
		REQUIRE(finish_owned_maintenance(e, *r, budget()));
		const auto released = turn(e, *r, ProductionControlKind::RELEASE, budget());
		CHECK(released.owner_return.native_error == OK);
		CHECK(r->serial == 0);
		CHECK(e.registry->production_close(att_ref).native_error == ERR_BUSY);
		e.rd._free_pending_resources(0);
		CHECK(e.registry->production_close(att_ref).native_error == OK);
		s0->physical.use_closed.store(true);
		e.rd.qualification_retiring = true;
		e.rd.free_rid(RID::from_uint64(s0->backing.rid));
		e.rd.qualification_retiring = false;
		e.rd._free_pending_resources(0);
	}

	static void bug199a_expired_latched_stage_budget_slice_refresh_and_intra_frame_throttling() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup_v2(e);
		REQUIRE(r != nullptr);
		const GDExtensionProductionRequestRefV1 ref{ Enrollment::INSTANCE, r->serial };

		// 1. Latch a genuine future stage budget (short 2ms window).
		auto staged_budget = budget(8, 8);
		staged_budget.controls = 4;
		staged_budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 2000ull;
		{
			Registry::Lock lock(*e.registry);
			r->native_prepared_executor = true;
			r->native_static_sealed = true;
			e.registry->production_native_stage_latch_budget_locked(*r, staged_budget);
			CHECK(e.registry->production_native_stage_budget_is_latched_locked(*r));
			CHECK(r->native_stage_budget_duration_us > 0);
			r->control = {};
			r->control.serial = 21;
			r->control.input.request_serial = r->serial;
			r->control.input.record_generation = r->operation.record_generation;
			r->control.input.kind = ProductionControlKind::RECORD;
			r->control.input.budget = r->native_stage_budget;
			r->control.state = ProductionControlState::QUEUED;
			r->control.scene_waiting = true;
			r->control.record_history.admitted = true;
		}

		// Wait past the original cached wall-clock deadline_micros.
		while (OS::get_singleton()->get_ticks_usec() < staged_budget.deadline_micros) {
			OS::get_singleton()->delay_usec(200);
		}

		// 2. Owner polling consume_v3 must NOT prematurely kill a live latched request.
		GDExtensionProductionRecordReportV3 report{};
		CHECK(e.registry->production_record_consume_v3(ref, 21, &report).native_error == ERR_BUSY);
		CHECK(r->control.state == ProductionControlState::QUEUED);

		// 3. On capture #1, refreshing the per-frame slice grants forward progress
		//    instead of livelocking forever, while keeping native_stage_budget immutable.
		{
			Registry::Lock lock(*e.registry);
			e.registry->production_native_stage_refresh_slice_locked(*r, 1, 1, 100);
			CHECK(r->native_stage_deferral_count == 1u);
			CHECK(r->native_stage_budget.deadline_micros == staged_budget.deadline_micros);
			CHECK(e.registry->production_native_stage_effective_deadline_locked(*r) > OS::get_singleton()->get_ticks_usec());
			CHECK(e.registry->production_native_stage_charge_locked(*r, true) == OK);
			CHECK(r->native_stage_controls_used == 1u);
		}

		// 4. Intra-frame throttling: if the slice expires within the SAME capture #1,
		//    refresh does NOT extend within the same capture -> ERR_BUSY!
		{
			Registry::Lock lock(*e.registry);
			r->native_stage_slice_deadline_micros = 1;
			e.registry->production_native_stage_refresh_slice_locked(*r, 1, 1, 100);
			CHECK(r->native_stage_deferral_count == 1u);
			CHECK(e.registry->production_native_stage_charge_locked(*r, true) == ERR_BUSY);

			// 5. On the NEXT capture #2, slice refreshes again -> forward progress!
			e.registry->production_native_stage_refresh_slice_locked(*r, 1, 1, 101);
			CHECK(r->native_stage_deferral_count == 2u);
			CHECK(e.registry->production_native_stage_charge_locked(*r, true) == OK);
			CHECK(r->native_stage_controls_used == 2u);

			// 6. Explicit unlatched test expiry (mutating deadline_micros directly) still fails closed.
			r->native_stage_budget.deadline_micros = 1;
			CHECK_FALSE(e.registry->production_native_stage_budget_is_latched_locked(*r));
			CHECK(e.registry->production_native_stage_charge_locked(*r, true) == ERR_BUSY);
		}
		CHECK(e.registry->production_record_consume_v3(ref, 21, &report).native_error == OK);
		CHECK(report.record.result.native_error == ERR_UNAVAILABLE);
		CHECK(e.registry->production_cancel(ref).native_error == OK);
	}

	static void bug199b_rdg_compute_pass_coalescing_and_barriers() {
		struct CoalesceDriver final : TransferTestDriverBase {
			uint32_t pass_begins = 0, pass_ends = 0, mem_barriers = 0, group_begins = 0, group_ends = 0, pipe_barriers = 0;
			uint32_t dispatches = 0, copies = 0;
			bool pass_open = false;
			uint64_t api_trait_get(ApiTrait p_trait) override {
				switch (p_trait) {
					case API_TRAIT_HONORS_PIPELINE_BARRIERS:
					case API_TRAIT_COALESCE_COMPUTE_PASSES:
						return 1;
					case API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE:
					case API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS:
						return 0;
					default:
						return TransferTestDriverBase::api_trait_get(p_trait);
				}
			}
			void command_group_begin(CommandBufferID) override {
				CHECK_FALSE(pass_open);
				++group_begins;
			}
			void command_group_end(CommandBufferID) override {
				CHECK_FALSE(pass_open);
				++group_ends;
			}
			void command_begin_compute_pass(CommandBufferID) override {
				CHECK_FALSE(pass_open);
				pass_open = true;
				++pass_begins;
			}
			void command_end_compute_pass(CommandBufferID) override {
				CHECK(pass_open);
				pass_open = false;
				++pass_ends;
			}
			void command_compute_memory_barrier(CommandBufferID) override {
				CHECK(pass_open);
				++mem_barriers;
			}
			void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {
				CHECK_FALSE(pass_open);
				++pipe_barriers;
			}
			void command_compute_dispatch(CommandBufferID, uint32_t, uint32_t, uint32_t) override {
				CHECK(pass_open);
				++dispatches;
			}
			void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override {
				CHECK_FALSE(pass_open);
				++copies;
			}
			void reset_counts() {
				pass_begins = pass_ends = mem_barriers = group_begins = group_ends = pipe_barriers = dispatches = copies = 0;
				pass_open = false;
			}
		} drv;

		RenderingDeviceGraph graph;
		graph.initialize(&drv, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		RenderingDeviceGraph::ResourceTracker b0, b1, b2, b3;
		b0.buffer_driver_id = RDD::BufferID(1);
		b1.buffer_driver_id = RDD::BufferID(2);
		b2.buffer_driver_id = RDD::BufferID(3);
		b3.buffer_driver_id = RDD::BufferID(4);
		RenderingDeviceGraph::CommandBufferPool pool;
		RDD::CommandBufferID cb(10);

		// Part A: 4 independent compute lists (L0) -> 2 dependent compute lists (L1) -> 1 dependent compute list (L2).
		// All 7 compute lists across 3 levels coalesce into 1 compute pass + 2 intra-pass memory barriers!
		graph.begin(true);
		for (RenderingDeviceGraph::ResourceTracker *t : { &b0, &b1, &b2, &b3 }) {
			graph.add_compute_list_begin();
			graph.add_compute_list_usage(t, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
			graph.add_compute_list_dispatch(1, 1, 1);
			graph.add_compute_list_end();
		}
		// Level 1: depends on b0 and b1 (2 independent lists).
		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b0, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		graph.add_compute_list_usage(&b2, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();
		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b1, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		graph.add_compute_list_usage(&b3, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();
		// Level 2: depends on b2 and b3 + includes an explicit intra-list memory barrier.
		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b2, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		graph.add_compute_list_usage(&b3, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_memory_barrier();
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();

		graph.end(true, false, cb, pool);
		CHECK_FALSE(drv.pass_open);
		CHECK(drv.dispatches == 8u);
		CHECK(drv.pass_begins == 1u);
		CHECK(drv.pass_ends == 1u);
		CHECK(drv.group_begins == 1u);
		CHECK(drv.group_ends == 1u);
		CHECK(drv.mem_barriers == 3u); // 2 cross-level + 1 intra-list

		// Part B: Compute (L0) -> BufferCopy (L1) -> Compute (L2) -> Compute (L3).
		// Must close compute pass before BufferCopy, then coalesce L2 + L3 into a 2nd compute pass!
		drv.reset_counts();
		graph.begin(true);
		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b0, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();

		RDD::BufferCopyRegion reg{};
		reg.size = 64;
		graph.add_buffer_copy(b0.buffer_driver_id, &b0, b1.buffer_driver_id, &b1, reg);

		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b1, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();

		graph.add_compute_list_begin();
		graph.add_compute_list_usage(&b1, RenderingDeviceGraph::RESOURCE_USAGE_STORAGE_BUFFER_READ);
		graph.add_compute_list_dispatch(1, 1, 1);
		graph.add_compute_list_end();

		graph.end(true, false, cb, pool);
		CHECK_FALSE(drv.pass_open);
		CHECK(drv.copies == 1u);
		CHECK(drv.dispatches == 3u);
		CHECK(drv.pass_begins == 2u);
		CHECK(drv.pass_ends == 2u);
		CHECK(drv.mem_barriers == 1u);
		CHECK(drv.group_ends == 3u); // L0 compute, L1 copy, L2+L3 coalesced compute
	}

	static void bug199b_inline_production_maintenance_drain_avoids_draw_drop() {
		using namespace GDExtensionDeviceExit;
		Enrollment e;
		auto *r = setup(e, 1);
		REQUIRE(r != nullptr);
		graph_initialize(e);
		e.rd.is_main_instance = true;
		e.rd.frames[0].primary_command_buffer_begun = true;

		// Pause production-owned maintenance on a tight budget while GPU fence is still unsignaled.
		e.driver.complete = false;
		RenderingDevice::NativeDisposalBudget tight_budget;
		tight_budget.passes = 1;
		tight_budget.receipts = 1;
		tight_budget.deadline_usec = OS::get_singleton()->get_ticks_usec() + 1000000;
		RenderingDevice::NativeDisposalUsage usage;
		CHECK(e.rd._native_disposal_maintenance_step(tight_budget, usage, r, nullptr, Ref<NativeHeightRange>(), false) == ERR_BUSY);
		REQUIRE(e.rd.native_disposal_active.load(std::memory_order_acquire));
		REQUIRE(e.rd.has_pending_production_frame_maintenance());
		r->maintenance_serial = e.rd.native_disposal_continuation.serial;

		// While GPU fence is still unsignaled, inline drain keeps cursor intact (no stall or theft).
		e.registry->try_complete_pending_production_frame_maintenance(e.rd);
		CHECK(e.rd.has_pending_production_frame_maintenance());
		CHECK(r->maintenance_serial == e.rd.native_disposal_continuation.serial);

		// Once GPU fence signals, inline drain completes maintenance before _draw() so the frame is NOT dropped!
		e.driver.complete = true;
		e.registry->try_complete_pending_production_frame_maintenance(e.rd);
		CHECK_FALSE(e.rd.has_pending_production_frame_maintenance());
		CHECK_FALSE(e.rd.native_disposal_active.load(std::memory_order_acquire));
		CHECK(r->maintenance_serial == 0);
		CHECK(e.rd.ensure_ordinary_draw_ready() == OK);

		// Foreign / unregistered owner pointer is never stolen.
		const auto *fake_owner = reinterpret_cast<const ProductionRequest *>(uintptr_t(0x1));
		e.rd.frames[0].primary_command_buffer_begun = true;
		usage = {};
		CHECK(e.rd._native_disposal_maintenance_step(tight_budget, usage, fake_owner, nullptr, Ref<NativeHeightRange>(), false) == ERR_BUSY);
		REQUIRE(e.rd.has_pending_production_frame_maintenance());
		e.registry->try_complete_pending_production_frame_maintenance(e.rd);
		CHECK(e.rd.has_pending_production_frame_maintenance());

		RenderingDevice::NativeDisposalBudget finish_budget;
		finish_budget.passes = 64;
		finish_budget.receipts = 128;
		finish_budget.deadline_usec = OS::get_singleton()->get_ticks_usec() + 5000000;
		usage = {};
		CHECK(e.rd._native_disposal_maintenance_step(finish_budget, usage, fake_owner, nullptr, Ref<NativeHeightRange>(), false) == OK);
		CHECK_FALSE(e.rd.has_pending_production_frame_maintenance());
	}
};
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] cancel rejects transferred native wait before consume without erasing actual prefix") {
	Access::native_record_wait_rejection(false);
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] release rejects transferred native wait but cannot overtake original consume") {
	Access::native_record_wait_rejection(true);
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] optin and result refs cannot manufacture an invocation") {
	Access::native_prepared_executor_refusals();
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] staged predecessor rejects unproven token and leaves original intent unbound") {
	Access::staged_predecessor_rejects_unproven_intent();
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] staged camera cannot freeze early and expires without a draw") {
	Access::staged_camera_refusals_and_expiry();
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] exhausted original stage budget returns without a draw or refill") {
	Access::staged_camera_refusals_and_expiry(true);
}
TEST_CASE("[SceneTree][ProductionNativeObserve][PreparedNativeRecordV2] delayed actual camera delivery cannot overwrite explicit staged intent") {
	Access::staged_camera_delayed_delivery_refusal();
}
TEST_CASE("[SceneTree][ProductionNativeObserve][PreparedNativeRecordV2] unprepared staged cull defers invocation without charging stage budget") {
	Access::staged_camera_unprepared_cull_preserves_stage_budget();
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] V3 returns actual original control usage and residual budget") {
	Access::record_v3_actual_usage_projection();
}
TEST_CASE("[ProductionNativeObserve][PreparedNativeRecordV2] two control quotes cannot double-debit or replace original staged slice") {
	Access::record_v3_two_turns_original_stage_slice();
}
TEST_CASE("[ProductionNativeObserve] zero budgets and bounded fair original serial polling") {
	Access::bounds_and_fairness();
}
TEST_CASE("[ProductionNativeObserve][OwnedFrameDraw] screen preparation cannot acquire or alter an owner suspended frame") {
	Access::ordinary_frame_owner_deferral(false);
}
TEST_CASE("[SceneTree][ProductionNativeObserve][OwnedFrameDraw] ordinary drawing defers until the original bounded owner returns") {
	Access::ordinary_frame_owner_deferral(true);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] original uploaded backing is copied without a request writer") {
	Access::incoming_readback_roundtrip();
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback][T11Worker] pending original worker is captured before incoming completion") {
	Access::incoming_worker_roundtrip(0);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback][T11Worker] submitted original worker joins incoming and retain Busy prevents recycle") {
	Access::incoming_worker_roundtrip(1);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback][T11Worker] completed and recycled original worker leaves no stale incoming membership") {
	Access::incoming_worker_roundtrip(2);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback][T11Worker] prior upload retains worker dependency and orders later incoming bytes") {
	Access::incoming_worker_roundtrip(3);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] physical alias partial prefix write refuses before effects") {
	Access::incoming_readback_roundtrip(1);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] adjacent prefix write preserves incoming bytes") {
	Access::incoming_readback_roundtrip(2);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] same backing write after copy preserves earlier bytes") {
	Access::incoming_readback_roundtrip(3);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] original map failure preserves error and returns each physical edge") {
	Access::incoming_readback_roundtrip(4);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] cancellation while actually mapped retains source until return") {
	Access::incoming_readback_roundtrip(5);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] one request mixes incoming and actual writer copies without weakening either source") {
	Access::incoming_readback_roundtrip(6);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] prepared native output prefix refuses before acquiring factory custody") {
	Access::incoming_readback_roundtrip(7);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] writable reflection protects the whole binding despite a disjoint READ use") {
	Access::incoming_reflected_read(0);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] readonly reflected prefix permits incoming copy") {
	Access::incoming_reflected_read(1);
}
TEST_CASE("[ProductionNativeObserve][IncomingReadback] writable second binding aliases the incoming physical object") {
	Access::incoming_reflected_read(2);
}
TEST_CASE("[ProductionNativeObserve] membership aliases preserve independent original issue and completion errors") {
	Access::alias_and_errors();
}
TEST_CASE("[ProductionNativeObserve] retention unknown and device loss retain obligations") {
	Access::loss_and_retention();
}
TEST_CASE("[ProductionNativeObserve] partial owner returns retain edges and retry only unreleased serials") {
	Access::partial_release();
}
TEST_CASE("[ProductionNativeObserve] expired immutable owner turn permits a new release budget") {
	Access::expired_owner_turn();
}
TEST_CASE("[ProductionNativeObserve] synchronous original device loss callback runs outside registry lock") {
	Access::synchronous_loss_callback();
}
TEST_CASE("[ProductionNativeObserve] private maintenance ceiling bounds oversized caller budgets") {
	Access::hard_ceiling();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] original checked dispatch records and emits exact push and groups") {
	Access::checked_dispatch_primitive();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] original upload copy readback delivers exact bytes only after native completion") {
	Access::transfer_roundtrip();
	Access::transfer_roundtrip(2);
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] staged upload prefix closes and acknowledges before exact suffix retry") {
	Access::upload_prefix_frontier();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] owned storage returns through actual driver free with one receipt per turn then closes") {
	Access::storage_retirement_turns();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] accepted set creation resumes its original slot after transient recording Busy") {
	SUBCASE("same original slot creates once after its returned control is consumed") {
		Access::original_set_create_frame_gate(0);
	}
	SUBCASE("cancelled original request never creates after the gate clears") {
		Access::original_set_create_frame_gate(1);
	}
	SUBCASE("sealed original NewWork never creates after the gate clears") {
		Access::original_set_create_frame_gate(2);
	}
}
TEST_CASE("[ProductionNativeObserve][OwnedSetBatch] dependency-clear original SETs share native return without exceeding their quote") {
	SUBCASE("one receipt queues one original SET") { Access::owned_set_retirement_batch(1); }
	SUBCASE("two receipts queue both originals before maintenance") { Access::owned_set_retirement_batch(2); }
	SUBCASE("ordinary three-receipt quote queues both originals") { Access::owned_set_retirement_batch(3); }
	SUBCASE("sixteen receipts can pay both original physical returns") { Access::owned_set_retirement_batch(16); }
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] original unit import and request owned set execute dispatch then return dependencies") {
	SUBCASE("descriptor preparation without pipeline barriers") {
		Access::owned_set_dispatch();
	}
	SUBCASE("descriptor binding with pipeline barriers") {
		Access::owned_set_dispatch(0, true);
	}
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] undersized request source memberships reject before any driver effect") {
	Access::membership_capacity_before_effect();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] reflected dispatch bindings require matching storage and compatible action access before recording") {
	Access::reflected_dispatch_use_rejection();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] post graph hidden source prepares without waiting and resumes without graph replay") {
	Access::post_graph_hidden_source();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] mapped readback cancellation retains arena until actual unmap and delivery") {
	Access::mapped_readback_keeps_request_alive();
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] logical narrow span and read use preserve legal writable physical binding") {
	Access::owned_set_dispatch(5);
	Access::owned_set_dispatch(6);
}

TEST_CASE("[ProductionNativeObserve][ProductionActions] readonly and writable aliases retain separate compatible logical accesses") {
	Access::owned_set_dispatch(7);
}
TEST_CASE("[ProductionNativeObserve][ProductionActions] distinct alias logical windows require both binding and use coverage") {
	Access::owned_set_dispatch(8);
}

TEST_CASE("[ProductionNativeObserve][ProductionActions] unit and set metadata associations preserve legal storage dispatch") {
	Access::owned_set_dispatch(10);
	Access::owned_set_dispatch(11);
	Access::owned_set_dispatch(12);
}

TEST_CASE("[ProductionNativeObserve][ProductionActions][V2] same canonical upload dispatch readback retain distinct physical rows") {
	SUBCASE("original native completion and physical return") {
		Access::ordinal_transfer_dispatch_roundtrip(false);
	}
	SUBCASE("cancellation retains incomplete original native obligations") {
		Access::ordinal_transfer_dispatch_roundtrip(true);
	}
	SUBCASE("known physical upload prefix does not complete its logical requirement") {
		Access::ordinal_transfer_dispatch_roundtrip(false, true);
	}
}

} //namespace TestProductionNativeObserve

TEST_CASE("[ProductionNativeObserve] accepted maintenance cursor resumes only its original request and never reuses serials") {
	TestProductionNativeObserve::Access::maintenance_cursor_owner();
}

TEST_CASE("[ProductionNativeObserve] fresh maintenance scopes only its actual buffer disposal target") {
	using Access = TestProductionNativeObserve::Access;
	SUBCASE("fresh ignores a buffer on the current non-target frame") {
		Access::fresh_buffer_target(false, false, false);
	}
	SUBCASE("fresh refuses the same buffer on the actual next target") {
		Access::fresh_buffer_target(false, true, false);
	}
	SUBCASE("partial ordinary advance still refuses the current target buffer") {
		Access::fresh_buffer_target(true, true, false);
	}
	SUBCASE("partial ordinary advance ignores the other buffer") {
		Access::fresh_buffer_target(true, false, false);
	}
	SUBCASE("a non-target foreign readback still prevents the entire turn") {
		Access::fresh_buffer_target(false, false, true);
	}
}

TEST_CASE("[ProductionNativeObserve][NativeHeightRange] original captured issue retains before frame advance without replay") {
	TestProductionNativeObserve::Access::height_retain_before_original_frame_advance();
}

TEST_CASE("[ProductionNativeObserve][ConsumedTransportV2] non-output source and malformed ABI cannot admit or mutate a proof") {
	TestProductionNativeObserve::Access::consumed_transport_rejects_without_source();
}
TEST_CASE("[ProductionNativeObserve][ConsumedTransportV2] V1 request rejects opt-in without capacity or control mutation") {
	TestProductionNativeObserve::Access::consumed_transport_preserves_v1();
}

TEST_CASE("[ProductionNativeObserve][OwnSetSelective] original SET return preserves a same-frame ordinary disposal") {
	using Access = TestProductionNativeObserve::Access;
	const auto expected = Access::own_set_with_ordinary_disposal(true);
	const auto actual = Access::own_set_with_ordinary_disposal(false);
	CHECK(actual == expected);
}

TEST_CASE("[ProductionNativeObserve][BUG-69] ordinary _stall_for_frame retains unretained production frame sources before fence recycle") {
	TestProductionNativeObserve::Access::stall_for_frame_retains_production_sources();
}

TEST_CASE("[ProductionNativeObserve][BUG-120] _stall_for_frame preserves readback requests on transient ERR_BUSY delivery") {
	TestProductionNativeObserve::Access::bug120_stall_for_frame_preserves_busy_readback();
}

TEST_CASE("[ProductionNativeObserve][BUG-120] split multi-frame readbacks deliver in order and sibling requests do not starve pump or maintenance") {
	TestProductionNativeObserve::Access::bug120_split_readback_order_and_sibling_pump_and_closure();
}

TEST_CASE("[ProductionNativeObserve][BUG-124] _native_disposal_stall_step pops delivered requests and _begin_frame cleanly unmaps and finishes remaining readbacks") {
	TestProductionNativeObserve::Access::bug124_disposal_stall_pops_delivered_and_unmaps_interrupted_map();
}

TEST_CASE("[ProductionNativeObserve][BUG-125] _stall_for_frame marks chain_reports source_owner_returned immediately on fence recycle before deferred readback ERR_BUSY") {
	TestProductionNativeObserve::Access::bug125_stall_for_frame_marks_chain_reports_returned_before_deferred_busy();
}

TEST_CASE("[SceneTree][ProductionNativeObserve][BUG-137] permanent scene_cull failure surfaces through RETURNED immediately while repeated ERR_BUSY is stage-budget bounded") {
	TestProductionNativeObserve::Access::bug137_staged_cull_permanent_failure_and_bounded_busy();
}

TEST_CASE("[ProductionNativeObserve][BUG-137] production_cancel immediately wakes QUEUED scene_waiting staged and non-native RECORD controls into RETURNED") {
	TestProductionNativeObserve::Access::bug137_production_cancel_wakes_scene_waiting_staged_and_non_native();
}

TEST_CASE("[ProductionNativeObserve][BUG-136] production_record_material_qualification returns _recording_error before freezing or entering append") {
	TestProductionNativeObserve::Access::bug136_mq_record_checks_recording_error_before_append();
}

TEST_CASE("[ProductionNativeObserve][RenderingDevice][BUG-138C] idle staging blocks and TransferWorker staging buffer trim after idle threshold while preserving active readbacks") {
	TestProductionNativeObserve::Access::bug138c_trim_idle_staging_buffers_and_transfer_worker();
}

TEST_CASE("[ProductionNativeObserve][BUG-138C] ProductionAttachment arena reclaims POD pages on idle while preserving scene_prefix_bytes and clean reuse") {
	TestProductionNativeObserve::Access::bug138c_arena_pod_reclaim_on_idle_and_reuse();
}

TEST_CASE("[ProductionNativeObserve][Pillar1][PersistentSetCache] persistent UniformSet cache hits on Main across frames/ping-pong, evicts safely on dependency retire, and drains on close") {
	TestProductionNativeObserve::Access::pillar1_persistent_set_cache_hit_pingpong_evict_and_close();
}

TEST_CASE("[ProductionNativeObserve][Pillar1][ObserveFastPath] passive RT _begin_frame harvest completes obligations and Main-thread OBSERVE fast-path returns synchronous OK without RT turn") {
	TestProductionNativeObserve::Access::pillar1_observe_fast_path_and_passive_rt_harvest();
}

TEST_CASE("[ProductionNativeObserve][Pillar1][PassiveHarvestReturned] passive RT _begin_frame harvests obligations while control is RETURNED and delivers other-frame production readbacks non-blockingly") {
	TestProductionNativeObserve::Access::pillar1_passive_harvest_while_control_returned_and_other_frame_readback();
}

TEST_CASE("[ProductionNativeObserve][Pillar1][ReleaseFastPath] settled PersistentSetEntry request completes RELEASE synchronously on Main while RT-only custody still takes RT RELEASE") {
	TestProductionNativeObserve::Access::pillar1_release_main_fast_path_and_rt_guard();
}

TEST_CASE("[ProductionNativeObserve][Pillar1][BUG-145] persistent_sets_by_dep O(k) eviction and clean close") {
	TestProductionNativeObserve::Access::bug145_persistent_sets_by_dep_ok_eviction_and_clean_close();
}

TEST_CASE("[SceneTree][ProductionNativeObserve][Pillar1][BUG-145] OutputState::RETURNED custody return in OBSERVE enables 0-RT-turn Main RELEASE fast-path and multi-pass fallback") {
	TestProductionNativeObserve::Access::bug145_output_custody_return_in_observe_and_multipass_release_fallback();
}

TEST_CASE("[SceneTree][ProductionNativeObserve][Pillar1][BUG-145] staged production_prepare_native_record_v2 deferred inline RT subscribe on RECORD dispatch") {
	TestProductionNativeObserve::Access::bug145_staged_prepare_native_record_v2_and_deferred_rt_subscribe_on_dispatch();
}

TEST_CASE("[ProductionNativeObserve][BUG-166] distinct native owners beyond index 64 complete OBSERVE, passive RT harvest, and RELEASE in 1 pass") {
	TestProductionNativeObserve::Access::bug166_distinct_native_owners_skip_alias_rows();
}

TEST_CASE("[ProductionNativeObserve][BUG-170] prefix-only reset leaves zero stale state across waves and avoids per-wave MADV_FREE_REUSABLE thrash") {
	TestProductionNativeObserve::Access::bug170_prefix_reset_and_no_per_wave_madvise_thrash();
}

TEST_CASE("[ProductionNativeObserve][BUG-183B] idle reclaim destroys and advises all 8 HWM cell sub-arenas and re-acquires cleanly") {
	TestProductionNativeObserve::Access::bug183b_reclaim_all_hwm_cell_subarenas_and_reacquire();
}

TEST_CASE("[ProductionNativeObserve][BUG-183B-2] ProductionAttachment arena uses page-aligned anonymous mmap and unmaps cleanly") {
	TestProductionNativeObserve::Access::bug183b2_mmap_backed_arena_reclaim_and_unmap();
}

TEST_CASE("[ProductionNativeObserve][BUG-184A] per-frame resource validation cache and O(1) RID slot index preserve output_guard_readonly and closure rejections") {
	TestProductionNativeObserve::Access::bug184a_per_frame_validation_cache_and_rid_index();
}

TEST_CASE("[ProductionNativeObserve][BUG-185B] O(1) request lookup, per-pool free_cursor reuse, and O(U) storage-dedup and guard-readback validation") {
	TestProductionNativeObserve::Access::bug185b_o1_request_lookup_free_cursor_and_dedup_validation();
}

TEST_CASE("[ProductionNativeObserve][BUG-186A] production_record_receive_locked gates transient disposal_active ERR_BUSY before record_receiver while permitting cleanup") {
	TestProductionNativeObserve::Access::bug186a_record_receive_gates_transient_disposal_busy();
}

TEST_CASE("[ProductionNativeObserve][BUG-186C] bi-directional top-down NATIVE_OUTPUT placement eliminates readback/natives bisection and reclaims both ends on idle") {
	TestProductionNativeObserve::Access::bug186c_bidirectional_readback_and_natives_placement_and_reclaim();
}

TEST_CASE("[ProductionNativeObserve][BUG-195D-1] PersistentSetCache O(1) LRU touch/eviction, 4096 cap, and O(evicted) zero-scan steady-state RT reap") {
	TestProductionNativeObserve::Access::bug195d_persistent_set_lru_and_steady_state_reap();
}

TEST_CASE("[ProductionAction][BUG-195A] compute-list hazard tracker splits cleanly on WAR/RAW/WAW and canonicalized aliases with zero dead rebinds") {
	TestProductionNativeObserve::Access::bug195a_dispatch_war_raw_waw_clean_split_and_alias_canonicalization();
}

TEST_CASE("[ProductionNativeObserve][BUG-196A] O(1) RDG record command resolution and inverted linear frame admission preserve exact parity") {
	TestProductionNativeObserve::Access::bug196a_rdg_o1_record_lookup_and_inverted_frame_admission();
}

TEST_CASE("[ProductionAction][BUG-196A] disjoint multi-span hazard tracker eliminates false bounding-hull splits and falls back safely past 8 spans") {
	TestProductionNativeObserve::Access::bug196a_disjoint_multispan_hazard_tracker_and_overflow_fallback();
}

TEST_CASE("[ProductionNativeObserve][BUG-199A] expired latched native_stage_budget refreshes per-capture slice and honors intra-frame throttling") {
	TestProductionNativeObserve::Access::bug199a_expired_latched_stage_budget_slice_refresh_and_intra_frame_throttling();
}

TEST_CASE("[RenderingDeviceGraph][BUG-199B] compute pass coalescing merges intra-level and cross-level compute lists with intra-pass memory barriers") {
	TestProductionNativeObserve::Access::bug199b_rdg_compute_pass_coalescing_and_barriers();
}

TEST_CASE("[ProductionNativeObserve][BUG-199B] try_complete_pending_production_frame_maintenance drains signaled disposal inline without dropping draw") {
	TestProductionNativeObserve::Access::bug199b_inline_production_maintenance_drain_avoids_draw_drop();
}
