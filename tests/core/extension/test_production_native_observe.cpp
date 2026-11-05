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
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstring>

TEST_FORCE_LINK(test_production_native_observe)

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
		Physical *buffer_cells[32] = {}, *set_cell = nullptr;
		uint32_t buffer_frees = 0, set_creates = 0, set_frees = 0, set_prepares = 0, set_binds = 0;
		uint32_t expected_set_bindings = 1;
		bool set_live = false;
		String set_name;
		UniformSetID uniform_set_create(VectorView<BoundUniform> uniforms, ShaderID shader, uint32_t set, int pool) override {
			CHECK(shader == ShaderID(80));
			CHECK(set == 0);
			CHECK(pool == -1);
			CHECK_FALSE(set_live);
			CHECK(uniforms.size() == expected_set_bindings);
			if (uniforms.size() != expected_set_bindings || set_live) {
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
			set_live = true;
			return UniformSetID(82);
		}
		void uniform_set_free(UniformSetID id) override {
			CHECK(id == UniformSetID(82));
			CHECK(set_live);
			Access::before_free(set_cell);
			set_live = false;
			++set_frees;
		}
		void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID set, ShaderID shader, uint32_t index) override {
			CHECK(set == UniformSetID(82));
			CHECK(shader == ShaderID(80));
			CHECK(index == 0);
			CHECK(set_live);
			++set_prepares;
		}
		uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count) const override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			if (sets.size() == 1) {
				CHECK(sets[0] == UniformSetID(82));
			}
			return 0;
		}
		void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count, uint32_t dynamic) override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			CHECK(dynamic == 0);
			CHECK(set_live);
			if (sets.size() == 1) {
				CHECK(sets[0] == UniformSetID(82));
			}
			++set_binds;
		}
		uint8_t memory[32][256] = {};
		uint32_t delivery_maps = 0, delivery_unmaps = 0;
		uint64_t delivery_map_id = 0;
		uint32_t sizes[32] = {}, allocations = 0, copy_calls = 0, maps = 0;
		bool live[32] = {};
		BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
			CHECK(bytes > 0);
			CHECK(bytes <= 256);
			CHECK(allocations + 1 < 32);
			if (!bytes || bytes > 256 || allocations + 1 >= 32) {
				return {};
			}
			const uint32_t id = ++allocations;
			sizes[id] = uint32_t(bytes);
			live[id] = true;
			return BufferID(id);
		}
		uint8_t *buffer_map(BufferID id) override {
			CHECK((id.id > 0 && id.id < 32 && live[id.id]));
			if (!id.id || id.id >= 32 || !live[id.id]) {
				return nullptr;
			}
			++maps;
			return memory[id.id];
		}
		uint8_t *buffer_map_observed(BufferID id, int64_t &native_code) override {
			native_code = 0;
			CHECK(delivery_map_id == 0);
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
				CHECK(id.id == 82);
				CHECK(set_live);
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
				memcpy(memory[to.id] + region.dst_offset, memory[from.id] + region.src_offset, region.size);
				++copy_calls;
			}
		}
		CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(91); }
		bool command_buffer_begin(CommandBufferID id) override {
			CHECK((id == CommandBufferID(90) || id == CommandBufferID(91)));
			return true;
		}
		void command_buffer_end(CommandBufferID id) override { CHECK((id == CommandBufferID(90) || id == CommandBufferID(91))); }
		bool command_pool_reset(CommandPoolID id) override {
			CHECK(id == CommandPoolID(94));
			return true;
		}
		void begin_segment(uint32_t frame, uint32_t) override { CHECK(frame == 0); }
		void end_segment() override {
			++legacy_end_segments;
			CHECK_FALSE(budget_call_active);
		}
		SemaphoreID semaphore_create() override { return SemaphoreID(92); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		bool honors_pipeline_barriers = true;
		uint64_t api_trait_get(ApiTrait trait) override {
			switch (trait) {
				case API_TRAIT_HONORS_PIPELINE_BARRIERS:
					return honors_pipeline_barriers;
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
			CHECK(id == FenceID(93));
			CHECK(serial > 0);
			++fence_polls;
			return { complete, native_error, serial, native_error ? -27 : 0 };
		}
		Error fence_recycle(FenceID id, uint64_t serial) override {
			CHECK(id == FenceID(93));
			CHECK(serial > 0);
			++fence_recycles;
			return OK;
		}
		uint32_t dispatches = 0, pushed[4] = {};
		uint64_t limit_get(Limit limit) override {
			CHECK((limit == LIMIT_MAX_PUSH_CONSTANT_SIZE || limit == LIMIT_MAX_BOUND_UNIFORM_SETS || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y || limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z));
			return limit == LIMIT_MAX_PUSH_CONSTANT_SIZE ? 128 : 64;
		}
		void command_bind_compute_pipeline(CommandBufferID, PipelineID id) override { CHECK(id == PipelineID(81)); }
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
		Error native_call_retain(uint64_t serial) override {
			REQUIRE(serial < 128);
			if (serial >= 128) {
				return {};
			}
			++retains[serial];
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

			return { complete && (serial != hidden_serial || hidden_ready), native_error, serial + 1000, native_error ? -27 : 0, unknown ? IssueFact::NotIssued : IssueFact::KnownAccepted };
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
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
			CHECK((sources_per_issue == 1 || sources_per_issue == 2));
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
				fact.queue_index = 1;
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
			CHECK(quote.graph_slots == uint64_t(capacity.graph_memberships) * 2 + uint64_t(capacity.requests) * 2);
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
		e.original_bridge = memnew(Registry::QualificationBridge);
		e.original_bridge->serial = 101;
		e.original_bridge->epoch = r.attachment->epoch;
		e.original_bridge->library = r.attachment->library;
		e.original_bridge->device = r.attachment->device;
		e.registry->qualification_bridges[0] = e.original_bridge;
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
	static Request *owned_setup(Enrollment &e, bool dispatch, uint32_t mismatch = 0) {
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
	static void execute_transfers(Enrollment &e, Request &r, bool deliver) {
		using namespace GDExtensionDeviceExit;
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
		for (uint32_t g = 0; g < r.graph_count; ++g) {
			if (r.graphs[g].device_epoch != e.rd.frames[0].qualification_epoch) {
				continue;
			}
			uint32_t slot = UINT32_MAX;
			RDD::CommandBufferID actual;
			const bool found = e.rd.draw_graph.resolve_record_command_buffer_slot(r.graphs[g].handle, slot) && e.rd.draw_graph.resolve_record_command_buffer(r.graphs[g].handle, actual);
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
	static Request *borrowed_readback_setup(Enrollment &e) {
		using namespace GDExtensionDeviceExit;
		auto c = capacity(4);
		c.retained_payload_bytes = c.readback_bytes = 16;
		c.readbacks = 1;
		const auto attachment_ref = e.accept(c);
		if (!attachment_ref.handle_serial) {
			return nullptr;
		}
		const GDExtensionProductionOperationV1 op = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 251, 1, 10, 12 };
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
				error = e.registry->production_reserve_metadata_locked(*attachment, op, c, actions, 2, &resource, 1, uses, 2, &readback, 1, r);
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
		const auto copied = e.registry->production_prepare_readback(ref, 11, 0).native_error;
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
};
TEST_CASE("[ProductionNativeObserve] zero budgets and bounded fair original serial polling") {
	Access::bounds_and_fairness();
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
