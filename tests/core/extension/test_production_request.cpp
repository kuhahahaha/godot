/**************************************************************************/
/*  test_production_request.cpp                                           */
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

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_shader_container.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_production_request)

namespace TestProductionRequest {
struct Access {
	using Registry = GDExtensionDeviceExit::Registry;
	using Request = GDExtensionDeviceExit::ProductionRequest;
	// The original bounded container codec produces the native unit's actual
	// reflected shader/pipeline. Deterministic CPU allocation is not a GPU grant.
	class UnitContainer : public RenderingShaderContainer {
	protected:
		uint32_t _format() const override { return 0x43504143; }
		uint32_t _format_version() const override { return 1; }
		bool _set_code_from_spirv(const ReflectShader &) override { return false; }

	public:
		void initialize() {
			reflection_data.pipeline_type = RenderingDeviceCommons::PIPELINE_TYPE_COMPUTE;
			reflection_data.stage_count = 1;
			reflection_data.set_count = 1;
			reflection_data.compute_local_size[0] = reflection_data.compute_local_size[1] = reflection_data.compute_local_size[2] = 1;
			reflection_shader_stages.push_back(RenderingDeviceCommons::SHADER_STAGE_COMPUTE);
			reflection_binding_set_uniforms_count.push_back(1);
			ReflectionBindingData binding;
			binding.type = RenderingDeviceCommons::UNIFORM_TYPE_STORAGE_BUFFER;
			binding.binding = 0;
			binding.stages = 1u << RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
			binding.length = 16;
			binding.writable = 1;
			reflection_binding_set_uniforms_data.push_back(binding);
			Shader shader;
			shader.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
			shader.code_compressed_bytes.push_back(42);
			shader.code_decompressed_size = shader.code_compressed_bytes.size();
			shaders.push_back(shader);
		}
	};
	class UnitFormat : public RenderingShaderContainerFormat {
	public:
		Ref<RenderingShaderContainer> create_container() const override { return memnew(UnitContainer); }
		ShaderLanguageVersion get_shader_language_version() const override { return SHADER_LANGUAGE_VULKAN_VERSION_1_1; }
		ShaderSpirvVersion get_shader_spirv_version() const override { return SHADER_SPIRV_VERSION_1_3; }
	};
	struct QuoteDriver : TransferTestDriverBase {
		// Finite CPU source facts follow the original native-observe fixture.
		// They prove Core caller/retention behavior, never Runtime or GPU grant.
		uint64_t next_source = 1, execution_thread = 0;
		uint32_t issues = 0, retains = 0, polls = 0, releases = 0;
		bool qualification_cleanup_active = false;
		uint32_t qualification_legacy_end_segments = 0;
		struct QualificationCleanupScope {
			QuoteDriver &driver;
			explicit QualificationCleanupScope(QuoteDriver &p_driver) : driver(p_driver) {
				CHECK_FALSE(driver.qualification_cleanup_active);
				driver.qualification_cleanup_active = true;
			}
			~QualificationCleanupScope() { driver.qualification_cleanup_active = false; }
		};
		void end_segment() override {
			CHECK(Thread::get_caller_id() == execution_thread);
			// Only original unit qualification maintenance may use legacy frame
			// close. Production budgeted owner turns must keep this scope false.
			CHECK(qualification_cleanup_active);
			++qualification_legacy_end_segments;
		}
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			QueueCallReport report;
			NativeCallFact fact;
			fact.serial = next_source++;
			fact.completion_serial = fact.serial + 1000;
			fact.queue_index = 1;
			fact.completion = CompletionCapability::Fence;
			fact.issue = IssueFact::KnownAccepted;
			report.calls.push_back(fact);
			report.issued = true;
			report.native_issue_count = 1;
			report.submission_serial = fact.serial + 2000;
			record_queue_call_report(report);
			++issues;
			return OK;
		}
		Error native_call_retain(uint64_t serial) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((serial > 0 && serial < 128));
			++retains;
			return OK;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((serial > 0 && serial < 128));
			++polls;
			return { true, false, serial + 1000, 0, IssueFact::KnownAccepted };
		}
		Error native_call_release(uint64_t serial) override {
			CHECK((serial > 0 && serial < 128));
			++releases;
			return OK;
		}
		uint32_t allocations = 0, buffer_frees = 0;
		bool buffer_live = false;
		BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits> usage, MemoryAllocationType allocation, uint64_t) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((bytes == 16 && allocation == MEMORY_ALLOCATION_TYPE_GPU));
			CHECK(usage.has_flag(BUFFER_USAGE_STORAGE_BIT));
			CHECK(!buffer_live);
			++allocations;
			buffer_live = true;
			return BufferID(1);
		}
		void buffer_free(BufferID id) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((id == BufferID(1) && buffer_live));
			CHECK_FALSE(set_live);
			buffer_live = false;
			++buffer_frees;
		}
		bool command_pool_reset(CommandPoolID id) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(id == CommandPoolID(94));
			return true;
		}
		bool command_buffer_begin(CommandBufferID id) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(id == CommandBufferID(90));
			return true;
		}
		void begin_segment(uint32_t frame, uint32_t) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(frame == 0);
		}
		uint64_t api_trait_get(ApiTrait trait) override {
			if (trait == API_TRAIT_HONORS_PIPELINE_BARRIERS) {
				return 1;
			}
			if (trait == API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE || trait == API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS) {
				return 0;
			}
			if (trait == API_TRAIT_SHADER_CHANGE_INVALIDATION) {
				return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
			}
			FAIL_CHECK("Unexpected source-service CPU driver trait");
			return 0;
		}
		UnitFormat format;
		uint32_t shader_creates = 0, pipeline_creates = 0, set_creates = 0, set_frees = 0, shader_frees = 0, pipeline_frees = 0;
		bool set_live = false;
		const RenderingShaderContainerFormat &get_shader_container_format() const override { return format; }
		ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &container, const Vector<ImmutableSampler> &) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			const auto reflection = container->get_shader_reflection();
			CHECK(reflection.pipeline_type == PIPELINE_TYPE_COMPUTE);
			CHECK(uint64_t(container->get_compile_flags()) == 0);
			CHECK(reflection.uniform_sets.size() == 1);
			if (reflection.uniform_sets.size() != 1 || reflection.uniform_sets[0].size() != 1) {
				FAIL_CHECK("Unexpected original unit reflection in SET fixture");
				return {};
			}
			const auto &binding = reflection.uniform_sets[0][0];
			CHECK((binding.binding == 0 && binding.type == UNIFORM_TYPE_STORAGE_BUFFER && binding.length == 16 && binding.writable));
			++shader_creates;
			return ShaderID(80);
		}
		PipelineID compute_pipeline_create(ShaderID shader, VectorView<PipelineSpecializationConstant>) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(shader == ShaderID(80));
			++pipeline_creates;
			return PipelineID(81);
		}
		uint32_t shader_get_layout_hash(ShaderID id) override {
			CHECK(id == ShaderID(80));
			return 123;
		}
		void shader_free(ShaderID id) override {
			CHECK(id == ShaderID(80));
			++shader_frees;
		}
		void pipeline_free(PipelineID id) override {
			CHECK(id == PipelineID(81));
			++pipeline_frees;
		}
		bool shader_retire_ready_nonblocking(ShaderID) const override { return true; }
		UniformSetID uniform_set_create(VectorView<BoundUniform> uniforms, ShaderID shader, uint32_t set, int pool) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((shader == ShaderID(80) && set == 0 && pool == -1 && !set_live));
			CHECK(uniforms.size() == 1);
			if (uniforms.size() != 1) {
				return {};
			}
			CHECK((uniforms[0].binding == 0 && uniforms[0].type == UNIFORM_TYPE_STORAGE_BUFFER && uniforms[0].ids.size() == 1));
			if (uniforms[0].ids.size() != 1) {
				return {};
			}
			CHECK(uniforms[0].ids[0].id == 1);
			++set_creates;
			set_live = true;
			return UniformSetID(82);
		}
		void uniform_set_free(UniformSetID id) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK((id == UniformSetID(82) && set_live));
			++set_frees;
			set_live = false;
		}
		void set_object_name(ObjectType, ID, const String &name) override { CHECK(name.begins_with("RID:")); }
		void command_buffer_end(CommandBufferID id) override { CHECK(id == CommandBufferID(90)); }
		CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(90); }
		SemaphoreID semaphore_create() override { return SemaphoreID(92); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		Error qualification_prepare_segment(QueueCallReport &report, bool nonblocking = true) override {
			CHECK(nonblocking);
			report = {};
			return OK;
		}
		FencePollResult fence_poll(FenceID id, uint64_t serial) override {
			CHECK(id == FenceID(93));
			return { true, false, serial, 0 };
		}
		Error fence_recycle(FenceID id, uint64_t) override {
			CHECK(id == FenceID(93));
			return OK;
		}
		Error native_call_recycle(uint64_t serial) override {
			CHECK(serial > 0);
			return OK;
		}
		// This metadata-only driver prepares no hidden work. Real native
		// operations retain the strict base's fail-fast behavior.
		bool native_issue_bound(NativeIssueKind kind, uint32_t commands, uint32_t &bound) const override {
			bound = kind == NativeIssueKind::PREPARE_SEGMENT ? 0 : commands ? 1
																			: 0;
			return true;
		}
		uint32_t qualification_native_call_bound_for_profile(uint32_t main, uint32_t workers, uint32_t frames) const override {
			return frames && frames <= main && workers <= UINT32_MAX - main ? main + workers : 0;
		}
	};
	struct Holder : GDExtensionDeviceExit::HolderRetirement {
		Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
	};
	// Controlled Core enrollment only. Every native driver operation remains
	// the fail-fast TransferTestDriverBase implementation. No GPU is initialized.
	struct Enrollment {
		RenderingDevice *previous = RenderingDevice::get_singleton();
		RenderingDevice rd;
		QuoteDriver driver;
		Registry *registry = memnew(Registry);
		Holder holder;
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
			// Also restore TLS/singleton on a REQUIRE exception. These are actual
			// no-effect production returns, not a second allocator or raw arena free.
			for (auto *attachment : registry->production_attachments) {
				if (!attachment) {
					continue;
				}
				for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) {
					auto &request = attachment->requests[i];
					if (request.serial) {
						registry->production_cancel({ INSTANCE, request.serial });
						registry->production_release({ INSTANCE, request.serial });
					}
				}
				registry->production_close({ INSTANCE, attachment->serial });
			}
			if (qualification.handle_serial) {
				registry->qualification_close(qualification);
			}
			frame.leave();
			reference.reset();
			registry->records[0].holders = nullptr;
			memdelete(registry);
			rd.driver = nullptr;
			rd.frames.clear();
			rd.native_finalization_returned = true;
			RenderingDevice::singleton = previous;
		}
		GDExtensionProductionAttachmentRefV1 accept(const GDExtensionProductionCapacityV1 &capacity) {
			GDExtensionProductionMetadataQuoteV1 quote = {};
			CHECK(rd.draw_graph.qualification_command_count() == 1);
			REQUIRE(registry->production_quote(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, &capacity, &quote).native_error == OK);
			CHECK(quote.graph_slots == 8);
			GDExtensionProductionAttachmentRefV1 attachment = {};
			GDExtensionProductionInstallationReportV1 report = {};
			REQUIRE(registry->production_accept(&ABI, { INSTANCE, 5 }, { INSTANCE, 9 }, { INSTANCE, 6 }, { INSTANCE, 7 }, { INSTANCE, 3 }, &quote, &attachment, &report).native_error == OK);
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
			const GDExtensionProductionOperationV1 operation = { ABI, nonce, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			return registry->production_reserve_metadata_locked(*attachment, operation, capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, out);
		}
	};

	// Same live singleton and original FIFO used by the engine. Only the newly
	// reserved record, device and views belong to this controlled CPU fixture.
	struct SingletonEnrollment {
		Registry &registry = Registry::get_singleton();
		RenderingServerDefault *server = nullptr;
		RenderingDevice rd;
		QuoteDriver driver;
		Holder holder;
		GDExtensionDeviceExit::RecordRef reference, scope;
		GDExtensionDeviceExit::RegistrationContext frame;
		GDExtensionDeviceExitSceneReadyContextRef ready = {};
		GDExtensionProducerRegistrationRefV1 producer = {};
		GDExtensionDeviceRegistryContextRef context = {};
		GDExtensionOriginalDeviceSessionRef session = {};
		GDExtensionProductionAttachmentRefV1 attachment = {};
		GDExtensionQualificationBridgeRefV1 qualification = {};
		Request *request = nullptr;
		uint32_t device_slot = UINT32_MAX;
		uint64_t instance = 0;

		Error prepare_device(RenderingServerDefault *p_server) {
			using namespace GDExtensionDeviceExit;
			if (!p_server) {
				return ERR_INVALID_PARAMETER;
			}
			// The dummy renderer has no RD. The normal RD constructor supplied the
			// singleton here; neither the live Registry nor RS singleton is replaced.
			if (!(RenderingDevice::get_singleton() == &rd)) {
				return ERR_UNAVAILABLE;
			}
			if (p_server->create_thread) {
				return ERR_UNAVAILABLE;
			}
			if (!(p_server->server_thread == Thread::get_caller_id())) {
				return ERR_UNAVAILABLE;
			}
			server = p_server;
			server->sync();
			rd.driver = &driver;
			driver.execution_thread = Thread::get_caller_id();
			rd.is_main_instance = true;
			rd.frames.resize(1);
			if (rd.frames.size() != 1) {
				return ERR_OUT_OF_MEMORY;
			}
			rd.transfer_worker_pool_max_size = 0;
			rd.draw_graph.add_capture_timestamp(RDD::QueryPoolID(1), 0);
			if (Error error = registry.device_created(&rd); error != OK) {
				return error;
			}
			return OK;
		}
		Error prepare_owner_device(RenderingServerDefault *p_server) {
			// Split-owner fixtures construct this RD on their real owner worker.
			// Register its owner there; only make_current may later move execution.
			if (!p_server || p_server->create_thread || p_server->server_thread == Thread::get_caller_id() || RenderingDevice::get_singleton() != &rd || rd.render_thread_id != Thread::get_caller_id()) {
				return ERR_UNAVAILABLE;
			}
			server = p_server;
			rd.driver = &driver;
			driver.execution_thread = Thread::get_caller_id();
			rd.is_main_instance = true;
			rd.frames.resize(1);
			if (rd.frames.size() != 1) {
				return ERR_OUT_OF_MEMORY;
			}
			rd.transfer_worker_pool_max_size = 0;
			rd.draw_graph.add_capture_timestamp(RDD::QueryPoolID(1), 0);
			return registry.device_created(&rd);
		}
		Error initialize(RenderingServerDefault *p_server, uint32_t loader_kind = 1) {
			const Error error = prepare_device(p_server);
			return error == OK ? initialize_owner(loader_kind) : error;
		}
		Error initialize_owner(uint32_t loader_kind = 1, bool owned_storage = false, bool owned_set = false) {
			using namespace GDExtensionDeviceExit;
			if (Error error = registry.reserve_open(loader_kind, reference, scope, nullptr); error != OK) {
				return error;
			}
			{
				MutexLock lock(registry.mutex);
				if (!(registry.phase == Registry::Phase::RUNNING)) {
					return ERR_UNAVAILABLE;
				}
				if (!((registry.live_services & (1u << 5)) != 0)) {
					return ERR_UNAVAILABLE;
				}
				instance = registry.instance;
				if (!(instance != 0)) {
					return ERR_UNAVAILABLE;
				}
				auto &record = registry.records[reference.slot];
				record.attached = true;
				record.guard_enrollment = GuardEnrollment::QUALIFIED;
				record.scene_ready_state = Registry::Record::SceneReadyState::IN_FRAME;
				record.producer_attachment_identity = this;
				if (!(registry.holder_serials.take(holder.handle_serial))) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				record.producer_code_holder = holder.handle_serial;
				holder.typed_code_hold = true;
				holder.generation = record.snapshot.library_generation;
				holder.acquired_thread = record.owner_thread;
				if (!(registry.action_serials.take(holder.terminal_action))) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				record.holders = &holder;
				auto &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
				scene.initialized = true;
				auto &action = scene.binding_action;
				action.serial = holder.terminal_action;
				action.generation = holder.generation;
				action.owner_thread = record.owner_thread;
				action.level = GDEXTENSION_INITIALIZATION_SCENE;
				action.kind = TerminalActionKind::LEVEL_BINDING;
				action.services = 1u << 5;
				action.wrapper = &Enrollment::original_terminator;
				action.userdata = this;
				record.original_initialization.deinitialize = action.wrapper;
				record.original_initialization.userdata = action.userdata;
				frame.enter(reference, true);
				frame.level_phase = LevelPhase::SCENE_READY;
				if (!(registry.frame_serials.take(frame.frame_id))) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				ready.registry_instance = instance;
				if (Error error = registry.issue_borrow_locked(Registry::ViewDomain::SCENE_READY, 0, ready.handle_serial); error != OK) {
					return error;
				}
				uint32_t view = UINT32_MAX;
				if (Error error = registry.producer_view_capacity_locked(view); error != OK) {
					return error;
				}
				registry.issue_producer_view_locked(reference.slot, view, producer);
				for (uint32_t i = 0; i < Registry::RECORD_CAPACITY; ++i) {
					if (registry.device_records[i].object == &rd) {
						device_slot = i;
						break;
					}
				}
				if (!(device_slot != UINT32_MAX)) {
					return ERR_UNAVAILABLE;
				}
			}
			if (owned_set) {
				uint64_t actual_object = 0;
				const Error bound = Error(registry.bind_main_scene_ready_device_session(&Enrollment::ABI, ready, &context, &session, &actual_object).native_error);
				if (bound != OK) {
					return bound;
				}
				if (actual_object != uint64_t(rd.get_instance_id())) {
					return ERR_INVALID_DATA;
				}
			} else if (Error error = Error(registry.bind_scene_ready_device_session(&Enrollment::ABI, ready, &rd, &context, &session).native_error); error != OK) {
				return error;
			}
			GDExtensionProductionCapacityV1 whole = { Enrollment::ABI, 2, 2, owned_set ? 3u : 2u, owned_set ? 3u : 2u, 2, 32, 0, owned_set ? 3u : 2u, 8, 0 };
			GDExtensionProductionMetadataQuoteV1 quote = {};
			if (Error error = Error(registry.production_quote(&Enrollment::ABI, ready, producer, context, session, &whole, &quote).native_error); error != OK) {
				return error;
			}
			GDExtensionProductionInstallationReportV1 report = {};
			if (Error error = Error(registry.production_accept(&Enrollment::ABI, ready, producer, context, session, { instance, holder.handle_serial }, &quote, &attachment, &report).native_error); error != OK) {
				return error;
			}
			auto single = whole;
			single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
			single.native_memberships = 16;
			single.retained_payload_bytes = 4;
			if (owned_set) {
				single.resources = single.return_cells = 3;
				single.resource_uses = 3;
			}
			MutexLock lock(registry.mutex);
			ProductionAttachment *accepted = nullptr;
			RenderingDevice *device = nullptr;
			if (Error error = registry.production_attachment_locked(attachment, true, accepted, device); error != OK) {
				return error;
			}
			const GDExtensionProductionOperationV1 operation = { Enrollment::ABI, 91, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, uint32_t(owned_storage ? GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 : GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1), 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			const GDExtensionProductionResourceV1 set_resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 }
			};
			const GDExtensionProductionUseV1 set_use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 16, 0 };
			if (Error error = registry.production_reserve_metadata_locked(*accepted, operation, single, &action, 1, owned_set ? set_resources : &resource, owned_set ? 3 : 1, owned_set ? &set_use : &use, 1, nullptr, 0, request); error != OK) {
				return error;
			}
			return request ? OK : ERR_INVALID_DATA;
		}
		bool owner_cleaned = false;
		void cleanup_owner() {
			using namespace GDExtensionDeviceExit;
			// This CPU fixture never initialized native RD state. Suppress native
			// teardown on every destructor path, including partial setup/refund failure.
			// Original entity-hold checks still run in the actual RD destructor.
			rd.driver = nullptr;
			rd.native_finalization_returned = true;
			// Explicit early returns may leave only a prefix of initialization acquired.
			// Drain and return only this fixture's acquired handles; never assume slot 0.
			// No fabricated return, singleton swap, or private alternate queue.
			if (server && Thread::get_caller_id() == server->server_thread) {
				server->sync();
			}
			if (request && request->serial) {
				const GDExtensionProductionRequestRefV1 ref = { instance, request->serial };
				if (request->control.state == ProductionControlState::PENDING) {
					if (!Access::check_step(registry.production_control_schedule(ref, request->control.serial) == OK)) {
						return;
					}
					server->sync();
				}
				if (request->control.state == ProductionControlState::RETURNED) {
					ProductionControlOutput output = {};
					if (!Access::check_step(registry.production_control_consume(ref, request->control.serial, &output) == OK)) {
						return;
					}
				}
				if (request->serial) {
					if (!Access::check_step(registry.production_cancel(ref).native_error == OK)) {
						return;
					}
					if (!Access::check_step(registry.production_release(ref).native_error == OK)) {
						return;
					}
				}
			}
			if (attachment.handle_serial) {
				if (!Access::check_step(registry.production_close(attachment).native_error == OK)) {
					return;
				}
			}
			if (qualification.handle_serial) {
				if (!Access::check_step(registry.qualification_close(qualification).native_error == OK)) {
					return;
				}
			}
			if (producer.handle_serial) {
				if (!Access::check_step(registry.release_producer_registration_view(producer) == OK)) {
					return;
				}
			}
			if (context.handle_serial) {
				if (!Access::check_step(registry.release_device_reference(instance, context.handle_serial, false).native_error == OK)) {
					return;
				}
			}
			if (session.handle_serial) {
				if (!Access::check_step(registry.release_device_reference(instance, session.handle_serial, true).native_error == OK)) {
					return;
				}
			}
			frame.leave();
			if (reference.is_valid()) {
				MutexLock lock(registry.mutex);
				auto &record = registry.records[reference.slot];
				if (!Access::check_step(record.private_refs == 2)) {
					return;
				}
				if (!Access::check_step(record.external_refs == 0)) {
					return;
				}
				if (!Access::check_step(record.cleanup_reservations == 0)) {
					return;
				}
				if (!Access::check_step(holder.production_children == 0)) {
					return;
				}
				record.holders = nullptr;
				record.attached = false;
			}
			scope.reset();
			reference.reset();
			rd.driver = nullptr;
			rd.frames.clear();
			rd.native_finalization_returned = true;
			// RD's real destructor returns its real device_created enrollment.
			owner_cleaned = true;
		}
		~SingletonEnrollment() {
			if (!owner_cleaned) {
				cleanup_owner();
			}
		}
	};
	struct SingletonCounts {
		uint32_t records = 0, views = 0, devices = 0, device_refs = 0, attachments = 0;
		uint64_t private_refs = 0, external_refs = 0, instance = 0;
		uint32_t services = 0;
	};
	static SingletonCounts singleton_counts() {
		auto &registry = Registry::get_singleton();
		MutexLock lock(registry.mutex);
		SingletonCounts count;
		count.instance = registry.instance;
		count.services = registry.live_services;
		for (const auto &r : registry.records) {
			count.records += r.occupied;
			count.private_refs += r.private_refs;
			count.external_refs += r.external_refs;
		}
		for (const auto &v : registry.views) {
			count.views += v.serial != 0;
		}
		for (const auto &d : registry.device_records) {
			count.devices += d.epoch != 0;
		}
		for (const auto &r : registry.device_refs) {
			count.device_refs += r.serial != 0;
		}
		for (const auto *a : registry.production_attachments) {
			count.attachments += a != nullptr;
		}
		return count;
	}
	struct ForeignDispatch {
		RenderingServerDefault *server;
		uint64_t instance, attachment, request, serial;
		uint64_t called_thread = 0;
		bool returned = false;
		static void run(void *opaque) {
			auto &p = *static_cast<ForeignDispatch *>(opaque);
			p.called_thread = Thread::get_caller_id();
			p.server->_production_control_dispatch(p.instance, p.attachment, p.request, p.serial);
			p.returned = true;
		}
	};
	static bool check_step(bool success) {
		CHECK(success);
		return success;
	}
	static void singleton_initialization_refusal() {
		using namespace GDExtensionDeviceExit;
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server != nullptr)) {
			return;
		}
		if (!check_step(!server->create_thread && server->server_thread == Thread::get_caller_id() && RenderingDevice::get_singleton() == nullptr)) {
			return;
		}
		server->sync();
		auto &registry = Registry::get_singleton();
		const auto before = singleton_counts();
		const auto original_zero = registry.records[0];
		auto *const original_frame = RegistrationContext::current;
		{
			SingletonEnrollment fixture;
			// Original reserve_open rejects an invalid kind before issuing a record.
			// No global capacity/issuer or existing singleton record is overwritten.
			const Error error = fixture.initialize(server, 0);
			if (!check_step(error == ERR_INVALID_PARAMETER)) {
				return;
			}
			if (!check_step(!fixture.reference.is_valid() && !fixture.scope.is_valid())) {
				return;
			}
			if (!check_step(fixture.request == nullptr && fixture.attachment.handle_serial == 0 && fixture.producer.handle_serial == 0 && fixture.context.handle_serial == 0 && fixture.session.handle_serial == 0)) {
				return;
			}
		}
		const auto after = singleton_counts();
		CHECK(after.instance == before.instance);
		CHECK(after.services == before.services);
		CHECK(after.records == before.records);
		CHECK(after.views == before.views);
		CHECK(after.devices == before.devices);
		CHECK(after.device_refs == before.device_refs);
		CHECK(after.attachments == before.attachments);
		CHECK(after.private_refs == before.private_refs);
		CHECK(after.external_refs == before.external_refs);
		const auto &zero = registry.records[0];
		CHECK(zero.occupied == original_zero.occupied);
		CHECK(zero.attached == original_zero.attached);
		CHECK(zero.private_refs == original_zero.private_refs);
		CHECK(zero.external_refs == original_zero.external_refs);
		CHECK(zero.snapshot.library_generation == original_zero.snapshot.library_generation);
		CHECK(zero.owner_thread == original_zero.owner_thread);
		CHECK(zero.guard_enrollment == original_zero.guard_enrollment);
		CHECK(zero.scene_ready_state == original_zero.scene_ready_state);
		CHECK(zero.producer_attachment_identity == original_zero.producer_attachment_identity);
		CHECK(zero.producer_registration_serial == original_zero.producer_registration_serial);
		CHECK(zero.producer_code_holder == original_zero.producer_code_holder);
		CHECK(zero.holders == original_zero.holders);
		CHECK(RegistrationContext::current == original_frame);
		CHECK(RenderingServer::get_singleton() == server);
		CHECK(RenderingDevice::get_singleton() == nullptr);
	}
#ifdef THREADS_ENABLED
	// RD ownership starts on the real owner worker. The original RS remains
	// on main; its one make_current call moves only native execution to RT.
	struct SplitEnrollment {
		RenderingServerDefault *server = nullptr;
		SingletonEnrollment *fixture = nullptr;
		Semaphore owner_prepared, rt_migrated, main_released;
		Error prepared = ERR_UNCONFIGURED;
		uint64_t owner_thread = 0;
		bool execution_moved = false, owner_cleaned = false;
	};
	template <class T>
	static void split_owner_start(void *data) {
		auto &test = *static_cast<T *>(data);
		auto &setup = test.setup;
		{
			// Construction, device_created and final destruction all belong to
			// this worker. Main may borrow the pointer only between rendezvous.
			SingletonEnrollment fixture;
			setup.fixture = &fixture;
			setup.owner_thread = Thread::get_caller_id();
			setup.prepared = fixture.prepare_owner_device(setup.server);
			setup.owner_prepared.post();
			setup.rt_migrated.wait();
			if (setup.prepared == OK && setup.execution_moved) {
				T::run(data);
			}
			// Keep even failed enrollment storage alive until main has stopped
			// borrowing it. The original owner performs final destruction.
			setup.main_released.wait();
			if (!fixture.owner_cleaned) {
				fixture.cleanup_owner();
			}
			setup.owner_cleaned = fixture.owner_cleaned;
		}
		setup.fixture = nullptr;
	}
	static bool migrate_split_owner(SplitEnrollment &setup) {
		setup.owner_prepared.wait();
		bool moved = false;
		if (setup.prepared == OK) {
			auto &fixture = *setup.fixture;
			fixture.rd.make_current();
			fixture.driver.execution_thread = Thread::get_caller_id();
			MutexLock lock(fixture.registry.mutex);
			for (const auto &device : fixture.registry.device_records) {
				if (device.object == &fixture.rd) {
					moved = device.live && device.thread == setup.owner_thread && device.execution_thread == Thread::get_caller_id() && device.execution_migrated && fixture.rd.render_thread_id == Thread::get_caller_id();
					break;
				}
			}
		}
		setup.execution_moved = moved;
		setup.rt_migrated.post();
		return moved;
	}
	struct SplitOwner {
		SplitEnrollment setup;
		Semaphore owner_ready, rt_returned;
		Error initialized = ERR_UNCONFIGURED;
		uint64_t owner_thread = 0, first_serial = 0, second_serial = 0;
		bool passed = true;
		void check(bool condition) {
			passed &= condition;
		}
		void rendezvous() {
			owner_ready.post();
			rt_returned.wait();
		}
		static void run(void *data) {
			auto &test = *static_cast<SplitOwner *>(data);
			auto &fixture = *test.setup.fixture;
			auto &registry = fixture.registry;
			using namespace GDExtensionDeviceExit;
			test.owner_thread = Thread::get_caller_id();
			test.initialized = fixture.initialize_owner();
			if (test.initialized != OK) {
				test.rendezvous();
				fixture.cleanup_owner();
				return;
			}
			auto &request = *fixture.request;
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
			GDExtensionProductionReportV1 report = {};
			auto budget = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			auto zero = budget;
			zero.receipts = 0;
			const uint64_t initial_serial = request.next_control_serial;
			test.check(registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(request.next_control_serial == initial_serial);
			test.check(request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.first_serial = request.control.serial;
			test.check(request.control.state == ProductionControlState::PENDING);
			auto changed_pending = budget;
			changed_pending.receipts = 2;
			test.check(registry.production_observe(ref, &changed_pending, &report).native_error == ERR_BUSY);
			test.check(request.control.serial == test.first_serial);
			test.check(request.control.input.budget.receipts == 1);
			test.check(fixture.driver.retains == 0 && fixture.driver.polls == 0);
			test.check(registry.production_cancel(ref).native_error == ERR_BUSY);
			test.check(!request.cancelled);
			test.check(registry.production_close(fixture.attachment).native_error == ERR_BUSY);
			test.rendezvous(); // The actual server thread drains the saturated FIFO.
			test.check(request.control.state == ProductionControlState::PENDING);
			auto expired_pending = budget;
			expired_pending.deadline_micros = 1;
			test.check(registry.production_observe(ref, &expired_pending, &report).native_error == OK);
			test.check(request.control.state == ProductionControlState::PENDING && request.control.serial == test.first_serial);
			test.check(registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(request.control.state == ProductionControlState::PENDING && fixture.driver.retains == 0 && fixture.driver.polls == 0);
			test.check(registry.production_observe(ref, &changed_pending, &report).native_error == ERR_BUSY);
			test.check(request.control.serial == test.first_serial);
			test.check(request.control.input.budget.receipts == 1);
			test.check(request.control.state == ProductionControlState::QUEUED);
			test.rendezvous();
			test.check(request.control.state == ProductionControlState::RETURNED);
			test.check(fixture.driver.retains == 1 && fixture.driver.polls == 0);
			test.check(registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(request.control.state == ProductionControlState::RETURNED);
			// An expired next reduction still consumes the retained immutable return,
			// but cannot enqueue another native service turn.
			auto expired = budget;
			expired.deadline_micros = 1;
			test.check(registry.production_observe(ref, &expired, &report).native_error == OK);
			test.check(request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(request.next_control_serial == test.first_serial + 1);
			// Publish the new reduction only after the original return was consumed.
			auto reduced = budget;
			reduced.receipts = 2;
			test.check(registry.production_observe(ref, &reduced, &report).native_error == ERR_BUSY);
			test.second_serial = request.control.serial;
			test.check(test.second_serial > test.first_serial);
			test.check(request.control.input.budget.receipts == 2);
			test.rendezvous();
			test.check(registry.production_observe(ref, &reduced, &report).native_error == OK);
			test.check(request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(fixture.driver.polls == 1 && report.known_obligations == 1 && report.complete_obligations == 1);
			// Maintenance remains callable after the original owner's seal.
			{
				MutexLock lock(registry.mutex);
				registry.records[fixture.reference.slot].quiescing = true;
			}
			test.check(registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.rendezvous();
			test.check(registry.production_observe(ref, &budget, &report).native_error == OK);
			test.check(fixture.driver.retains == 1 && fixture.driver.polls == 1);
			uint64_t release = 0;
			test.check(registry.production_control_accept(ref, ProductionControlKind::RELEASE, &release, &budget) == OK);
			test.check(registry.production_control_schedule(ref, release) == OK);
			test.rendezvous();
			ProductionControlOutput returned = {};
			test.check(registry.production_control_consume(ref, release, &returned) == OK);
			test.check(returned.owner_return.native_error == OK && fixture.driver.releases == 1);
			fixture.cleanup_owner();
			test.owner_ready.post();
		}
	};
	struct SplitStorage {
		SplitEnrollment setup;
		Semaphore owner_ready, rt_returned;
		Error initialized = ERR_UNCONFIGURED;
		bool done = false, passed = true, create_busy = false;
		uint32_t turns = 0;
		void check(bool value) { passed &= value; }
		void rendezvous() {
			owner_ready.post();
			rt_returned.wait();
		}
		static void run(void *data) {
			auto &test = *static_cast<SplitStorage *>(data);
			auto &fixture = *test.setup.fixture;
			auto &registry = fixture.registry;
			using namespace GDExtensionDeviceExit;
			test.initialized = fixture.initialize_owner(1, true);
			if (test.initialized == OK) {
				auto &request = *fixture.request;
				const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
				const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
				GDExtensionStorageBackingV1 backing = {};
				if (test.create_busy) {
					fixture.rd._thread_safe_.lock();
				}
				test.check(registry.production_storage_create(ref, 0, &origin, &backing).native_error == ERR_BUSY);
				test.check(request.control.state == ProductionControlState::QUEUED);
				test.check(fixture.driver.allocations == 0);
				test.rendezvous();
				if (test.create_busy) {
					fixture.rd._thread_safe_.unlock();
					test.check(fixture.driver.allocations == 0);
					test.check(registry.production_storage_create(ref, 0, &origin, &backing).native_error == ERR_BUSY);
					test.check(request.control.state == ProductionControlState::OWNER_CONSUMED && request.resources[0].creating);
					{
						MutexLock lock(registry.mutex);
						registry.records[fixture.reference.slot].quiescing = true;
					}
					test.check(registry.production_storage_create(ref, 0, &origin, &backing).native_error == ERR_BUSY);
					test.check(!request.resources[0].creating && !request.returns[0].pending);
					test.check(request.control.state == ProductionControlState::OWNER_CONSUMED && fixture.driver.allocations == 0);
					fixture.cleanup_owner();
					test.done = true;
					test.owner_ready.post();
					return;
				}
				test.check(fixture.driver.allocations == 1);
				test.check(registry.production_storage_create(ref, 0, &origin, &backing).native_error == OK);
				test.check(backing.rid && backing.backing_serial && backing.byte_size == 16);
				auto changed = origin;
				++changed.create_attempt;
				test.check(registry.production_storage_create(ref, 0, &changed, &backing).native_error == ERR_INVALID_DATA);
				const GDExtensionStorageRetireRequestV1 retire_origin = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 0, 0, 1, 1, 0 };
				GDExtensionStorageRetireRefV1 retire = {};
				GDExtensionStorageRetireReportV1 report = {};
				test.check(registry.production_storage_retire(ref, 0, &retire_origin, &retire, &report).native_error == OK);
				auto budget = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				auto zero = budget;
				zero.receipts = 0;
				const uint64_t next = request.next_control_serial;
				test.check(registry.production_storage_observe(ref, retire, &zero, &report).native_error == OK);
				test.check(!report.state && !report.event_seq && request.next_control_serial == next && fixture.driver.buffer_frees == 0);
				// A sealed owner may service existing retirement; each turn has one
				// receipt/pass, so enqueue and actual native free cannot collapse.
				{
					MutexLock lock(registry.mutex);
					registry.records[fixture.reference.slot].quiescing = true;
				}
				for (; test.turns < 32 && !report.state; ++test.turns) {
					test.check(registry.production_storage_observe(ref, retire, &budget, &report).native_error == ERR_BUSY);
					test.rendezvous();
					const auto output = request.control.output;
					test.check(output.service_usage.passes <= 1 && output.service_usage.receipts <= 1);
					const Error observed = Error(registry.production_storage_observe(ref, retire, &budget, &report).native_error);
					test.check(observed == OK || observed == ERR_BUSY);
				}
				test.check(test.turns > 1 && report.state == 1 && report.return_status == 1 && report.event_seq);
				test.check(fixture.driver.buffer_frees == 1 && !fixture.driver.buffer_live);
				test.check(registry.production_storage_release(ref, retire).native_error == OK);
			}
			fixture.cleanup_owner();
			test.done = true;
			test.owner_ready.post();
		}
	};
	static void singleton_split_owner_storage(bool create_busy = false) {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		SplitStorage test;
		test.setup.server = server;
		test.create_busy = create_busy;
		Thread owner;
		owner.start(&split_owner_start<SplitStorage>, &test);
		if (!check_step(migrate_split_owner(test.setup))) {
			test.setup.main_released.post();
			owner.wait_to_finish();
			return;
		}
		auto &fixture = *test.setup.fixture;
		uint32_t phase = 0;
		while (true) {
			test.owner_ready.wait();
			if (test.done) {
				break;
			}
			if (phase++ == 0) {
				// Replace the quoted metadata prefix with the actual empty CPU
				// frame before the queued create. No source is fabricated complete.
				fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
				fixture.rd.draw_graph.begin();
				fixture.rd.frames[0].qualification_epoch = 1;
				fixture.rd.frames[0].fence = RDD::FenceID(93);
				fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
				fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
				fixture.rd.main_queue = RDD::CommandQueueID(1);
				fixture.rd.frames_drawn = 3;
			}
			server->sync();
			test.rt_returned.post();
		}
		test.setup.main_released.post();
		owner.wait_to_finish();
		CHECK(test.initialized == OK);
		CHECK(test.passed);
		CHECK(test.setup.owner_cleaned);
	}
	struct SplitSet {
		SplitEnrollment setup;
		Semaphore owner_ready, rt_returned;
		GDExtensionNativeUnitBackingV1 unit = {};
		GDExtensionNativeUnitRetireRefV1 unit_retired = {};
		GDExtensionNativeUnitRetireReportV1 final_unit_report = {};
		uint32_t unit_retire_turns = 0, final_shader_frees = 0, final_pipeline_frees = 0, final_legacy_end_segments = 0;
		Error unit_observed = ERR_UNCONFIGURED;
		Error initialized = ERR_UNCONFIGURED, qualification_opened = ERR_UNCONFIGURED, unit_created = ERR_UNCONFIGURED;
		uint32_t phase = 0, first_failure_line = 0, last_failure_line = 0, failure_count = 0;
		bool done = false, passed = true;
		bool check(uint32_t line, bool value) {
			if (!value) {
				if (!first_failure_line) {
					first_failure_line = line;
				}
				last_failure_line = line;
				++failure_count;
			}
			passed &= value;
			return value;
		}
		void rendezvous(uint32_t next) {
			phase = next;
			owner_ready.post();
			rt_returned.wait();
		}
		void stop_before_storage() {
			using namespace GDExtensionDeviceExit;
			auto &fixture = *setup.fixture;
			auto &registry = fixture.registry;
			if (fixture.request && fixture.request->serial) {
				const GDExtensionProductionRequestRefV1 ref = { fixture.instance, fixture.request->serial };
				check(__LINE__, registry.production_cancel(ref).native_error == OK);
				check(__LINE__, registry.production_release(ref).native_error == OK);
			}
			// A factory may have a real partial native prefix. Return that prefix
			// through the original RT retire/observe/release path before close.
			if (unit.unit.handle_serial) {
				rendezvous(7);
				GDExtensionNativeUnitRetireReportV1 report = {};
				const GDExtensionQualificationBudgetV1 budget = { Enrollment::ABI, 1, 5000, 0 };
				for (uint32_t turn = 0; turn < 32 && report.state != 1; ++turn) {
					check(__LINE__, registry.qualification_native_unit_retire_observe(fixture.qualification, unit_retired, &budget, &report).native_error == OK);
					rendezvous(8);
				}
				check(__LINE__, report.state == 1);
				check(__LINE__, registry.qualification_native_unit_retire_release(fixture.qualification, unit_retired).native_error == OK);
			}
			fixture.cleanup_owner();
			done = true;
			owner_ready.post();
		}
		static void run(void *data) {
			auto &test = *static_cast<SplitSet *>(data);
			auto &fixture = *test.setup.fixture;
			auto &registry = fixture.registry;
			using namespace GDExtensionDeviceExit;
			test.initialized = fixture.initialize_owner(1, false, true);
			if (test.initialized != OK) {
				fixture.cleanup_owner();
				test.done = true;
				test.owner_ready.post();
				return;
			}
			test.qualification_opened = Error(registry.qualification_open(&Enrollment::ABI, fixture.ready, fixture.context, fixture.session, { fixture.instance, fixture.holder.handle_serial }, &fixture.qualification).native_error);
			if (!test.check(__LINE__, test.qualification_opened == OK && fixture.qualification.handle_serial)) {
				test.stop_before_storage();
				return;
			}
			test.rendezvous(1); // Original qualification factory executes on actual RT.
			if (!test.check(__LINE__, test.unit_created == OK && test.unit.unit.handle_serial && test.unit.known_mask == 3 && test.unit.state == 1 && test.unit.native_error == OK)) {
				test.stop_before_storage();
				return;
			}
			auto &request = *fixture.request;
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
			if (!test.check(__LINE__, registry.production_import_unit(ref, 0, &test.unit).native_error == OK) || !test.check(__LINE__, request.resources[0].use_count == 2)) {
				test.stop_before_storage();
				return;
			}
			const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 1, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == ERR_BUSY);
			test.rendezvous(2);
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == OK);
			test.rendezvous(3); // Fill the real FIFO before accepting SET_CREATE.
			const GDExtensionProductionSetBindingV1 expected = { Enrollment::ABI, 0, 0, 0, 1, 0, 16 };
			uint64_t set_serial = 0;
			{
				GDExtensionProductionSetBindingV1 transient = expected;
				test.check(__LINE__, registry.production_set_create(ref, 2, 0, 0, &transient, 1).native_error == ERR_BUSY);
				set_serial = request.control.serial;
				test.check(__LINE__, request.control.state == ProductionControlState::PENDING && request.control.input.kind == ProductionControlKind::SET_CREATE && request.control.input.resource_slot == 2);
				transient.resource_slot = UINT32_MAX;
				transient.bytes = 0;
				test.check(__LINE__, registry.production_set_create(ref, 2, 0, 0, &transient, 1).native_error == ERR_INVALID_DATA);
				test.check(__LINE__, registry.production_set_create(ref, 2, 0, 0, &expected, 1).native_error == ERR_BUSY);
			}
			// The original input's lifetime ended before the first native effect.
			test.check(__LINE__, fixture.driver.set_creates == 0 && request.control.serial == set_serial && request.binding_used == 1);
			test.rendezvous(4); // Drain stale FIFO entries, leaving accepted cell pending.
			test.check(__LINE__, request.control.state == ProductionControlState::PENDING);
			test.check(__LINE__, registry.production_set_create(ref, 2, 0, 0, &expected, 1).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.serial == set_serial && request.control.state == ProductionControlState::QUEUED);
			test.rendezvous(5);
			test.check(__LINE__, registry.production_set_create(ref, 2, 0, 0, &expected, 1).native_error == OK);
			test.check(__LINE__, fixture.driver.set_creates == 1 && request.resources[2].imported && request.binding_used == 1);
			test.check(__LINE__, request.uses[request.resources[2].first_binding].binding.resource_slot == 1 && request.uses[request.resources[2].first_binding].binding.bytes == 16);
			const auto *storage_dependencies = fixture.rd.dependency_map.getptr(request.resources[1].primary);
			const auto *shader_dependencies = fixture.rd.dependency_map.getptr(request.resources[0].secondary);
			test.check(__LINE__, storage_dependencies && storage_dependencies->has(request.resources[2].primary));
			test.check(__LINE__, shader_dependencies && shader_dependencies->has(request.resources[2].primary));
			GDExtensionProductionReportV1 report = {};
			auto budget = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			auto zero = budget;
			zero.receipts = 0;
			test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK && fixture.driver.set_creates == 1);
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.rendezvous(6);
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == OK);
			budget.receipts = 2;
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.rendezvous(6);
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == OK && fixture.driver.set_creates == 1 && fixture.driver.set_frees == 0);
			const GDExtensionStorageRetireRequestV1 retire_origin = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 1, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 retire = {};
			GDExtensionStorageRetireReportV1 retired = {};
			test.check(__LINE__, registry.production_storage_retire(ref, 1, &retire_origin, &retire, &retired).native_error == OK);
			test.check(__LINE__, registry.production_storage_release(ref, retire).native_error == ERR_BUSY && fixture.driver.buffer_frees == 0);
			test.check(__LINE__, registry.production_cancel(ref).native_error == OK);
			budget.receipts = 1;
			for (uint32_t turn = 0; turn < 64 && !retired.state; ++turn) {
				test.check(__LINE__, registry.production_storage_observe(ref, retire, &budget, &retired).native_error == ERR_BUSY);
				test.rendezvous(6);
				test.check(__LINE__, request.control.output.service_usage.receipts <= 1 && request.control.output.service_usage.passes <= 1);
				const Error observed = Error(registry.production_storage_observe(ref, retire, &budget, &retired).native_error);
				test.check(__LINE__, observed == OK || observed == ERR_BUSY);
			}
			test.check(__LINE__, retired.state == 1 && fixture.driver.set_frees == 1 && fixture.driver.buffer_frees == 1 && !fixture.driver.set_live);
			test.check(__LINE__, !fixture.rd.dependency_map.has(RID::from_uint64(backing.rid)));
			test.check(__LINE__, registry.production_storage_release(ref, retire).native_error == OK);
			test.check(__LINE__, registry.production_release(ref).native_error == OK);
			test.check(__LINE__, fixture.driver.qualification_legacy_end_segments == 0);
			test.rendezvous(7); // Actual unit dependency returns before owner closes.
			GDExtensionNativeUnitRetireReportV1 unit_report = {};
			const GDExtensionQualificationBudgetV1 unit_budget = { Enrollment::ABI, 1, 5000, 0 };
			// A one-pass service may enqueue a dependency or free only the
			// preceding cutoff. Read the original report on later owner turns;
			// each actual maintenance turn still executes through the RS FIFO.
			for (; test.unit_retire_turns < 32 && unit_report.state != 1; ++test.unit_retire_turns) {
				test.unit_observed = Error(registry.qualification_native_unit_retire_observe(fixture.qualification, test.unit_retired, &unit_budget, &unit_report).native_error);
				test.check(__LINE__, test.unit_observed == OK);
				test.rendezvous(8);
			}
			test.final_unit_report = unit_report;
			test.final_shader_frees = fixture.driver.shader_frees;
			test.final_pipeline_frees = fixture.driver.pipeline_frees;
			test.final_legacy_end_segments = fixture.driver.qualification_legacy_end_segments;
			test.check(__LINE__, test.unit_observed == OK && unit_report.state == 1);
			test.check(__LINE__, registry.qualification_native_unit_retire_release(fixture.qualification, test.unit_retired).native_error == OK);
			test.check(__LINE__, fixture.driver.shader_frees == 1 && fixture.driver.pipeline_frees == 1);
			fixture.cleanup_owner();
			test.done = true;
			test.owner_ready.post();
		}
	};
	static Error create_split_unit(SingletonEnrollment &fixture, GDExtensionNativeUnitBackingV1 &unit) {
		Ref<UnitContainer> container;
		container.instantiate();
		container->initialize();
		const Vector<uint8_t> bytes = container->to_bytes();
		if (!check_step(!bytes.is_empty())) {
			return ERR_INVALID_DATA;
		}
		const Ref<RenderingShaderContainer> parsed = fixture.driver.get_shader_container_format().create_container();
		if (!check_step(parsed.is_valid() && parsed->from_bytes(bytes))) {
			return ERR_INVALID_DATA;
		}
		if (!check_step(parsed->to_bytes() == bytes && uint64_t(parsed->get_compile_flags()) == 0)) {
			return ERR_INVALID_DATA;
		}
		if (!check_step(parsed->shaders.size() == 1)) {
			return ERR_INVALID_DATA;
		}
		const auto &code = parsed->shaders[0];
		if (!check_step(code.shader_stage == RenderingDevice::SHADER_STAGE_COMPUTE && !code.code_compression_flags && code.code_decompressed_size == 1 && code.code_compressed_bytes.size() == 1 && code.code_compressed_bytes[0] == 42)) {
			return ERR_INVALID_DATA;
		}
		const auto reflection = parsed->get_shader_reflection();
		if (!check_step(reflection.pipeline_type == RenderingDevice::PIPELINE_TYPE_COMPUTE && reflection.uniform_sets.size() == 1 && reflection.uniform_sets[0].size() == 1)) {
			return ERR_INVALID_DATA;
		}
		const auto &binding = reflection.uniform_sets[0][0];
		if (!check_step(binding.binding == 0 && binding.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER && binding.length == 16 && binding.writable && reflection.compute_local_size[0] == 1 && reflection.compute_local_size[1] == 1 && reflection.compute_local_size[2] == 1)) {
			return ERR_INVALID_DATA;
		}
		GDExtensionNativeUnitCreateRequestV1 origin = {};
		origin.abi = Enrollment::ABI;
		origin.library_generation = fixture.holder.generation;
		origin.preparation_generation = 6;
		origin.operation_serial = 91;
		origin.record_slot = 4;
		origin.physical_unit = 5;
		origin.expected_flags = uint64_t(parsed->get_compile_flags());
		origin.container_byte_count = bytes.size();
		const Error digest = CryptoCore::sha256(bytes.ptr(), bytes.size(), origin.container_digest);
		if (!check_step(digest == OK)) {
			return digest;
		}
		const uint32_t shaders = fixture.driver.shader_creates, pipelines = fixture.driver.pipeline_creates;
		const Error created = Error(fixture.registry.qualification_native_unit_create(fixture.qualification, &origin, bytes.ptr(), bytes.size(), &unit).native_error);
		if (!check_step(created == OK)) {
			return created;
		}
		if (!check_step(unit.unit.handle_serial && unit.known_mask == 3 && unit.state == 1 && unit.native_error == OK && unit.effective_flags == origin.expected_flags && fixture.driver.shader_creates == shaders + 1 && fixture.driver.pipeline_creates == pipelines + 1)) {
			return ERR_INVALID_DATA;
		}
		return OK;
	}
	static void singleton_split_owner_set() {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		SplitSet test;
		test.setup.server = server;
		Thread owner;
		owner.start(&split_owner_start<SplitSet>, &test);
		if (!check_step(migrate_split_owner(test.setup))) {
			test.setup.main_released.post();
			owner.wait_to_finish();
			return;
		}
		auto &fixture = *test.setup.fixture;
		while (true) {
			test.owner_ready.wait();
			if (test.done) {
				break;
			}
			if (test.phase == 1) {
				fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
				fixture.rd.draw_graph.begin();
				fixture.rd.frames[0].qualification_epoch = 1;
				fixture.rd.frames[0].fence = RDD::FenceID(93);
				fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
				fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
				fixture.rd.main_queue = RDD::CommandQueueID(1);
				test.unit_created = create_split_unit(fixture, test.unit);
			} else if (test.phase == 3) {
				uint32_t filled = 0;
				while (filled < (1u << 20) && server->_try_production_control_dispatch(0, 0, 0, 0)) {
					++filled;
				}
				CHECK((filled > 0 && filled < (1u << 20)));
			} else if (test.phase == 7) {
				QuoteDriver::QualificationCleanupScope cleanup_scope(fixture.driver);
				const GDExtensionNativeUnitRetireRequestV1 retire = { Enrollment::ABI, test.unit.unit, 92, 0 };
				GDExtensionNativeUnitRetireReportV1 report = {};
				CHECK(fixture.registry.qualification_native_unit_retire(fixture.qualification, &retire, &test.unit_retired, &report).native_error == OK);
				fixture.rd._free_pending_resources(0);
				CHECK(fixture.registry.qualification_native_unit_retire(fixture.qualification, &retire, &test.unit_retired, &report).native_error == OK);

			} else if (test.phase == 8) {
				QuoteDriver::QualificationCleanupScope cleanup_scope(fixture.driver);
				server->sync();
			} else {
				server->sync();
			}
			test.rt_returned.post();
		}
		test.setup.main_released.post();
		owner.wait_to_finish();
		CHECK(test.initialized == OK);
		CHECK(test.qualification_opened == OK);
		CHECK(test.unit_created == OK);
		CHECK_MESSAGE(test.passed, "SET first failure line=", test.first_failure_line, ", last failure line=", test.last_failure_line, ", failed checks=", test.failure_count, ", final phase=", test.phase, ", unit turns=", test.unit_retire_turns, ", observe error=", int(test.unit_observed), ", unit state=", test.final_unit_report.state, ", reason=", test.final_unit_report.reason, ", enqueued mask=", test.final_unit_report.enqueued_mask, ", freed mask=", test.final_unit_report.freed_mask, ", shader frees=", test.final_shader_frees, ", pipeline frees=", test.final_pipeline_frees);
		CHECK(test.final_legacy_end_segments > 0);
		CHECK(test.setup.owner_cleaned);
	}
	static void singleton_split_owner_observe() {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		uint32_t filled = 0;
		while (filled < (1u << 20) && server->_try_production_control_dispatch(0, 0, 0, 0)) {
			++filled;
		}
		if (!check_step(filled > 0 && filled < (1u << 20))) {
			server->sync();
			return;
		}
		SplitOwner test;
		test.setup.server = server;
		Thread owner;
		owner.start(&split_owner_start<SplitOwner>, &test);
		if (!check_step(migrate_split_owner(test.setup))) {
			test.setup.main_released.post();
			owner.wait_to_finish();
			server->sync();
			return;
		}
		auto &fixture = *test.setup.fixture;
		test.owner_ready.wait();
		if (test.initialized == OK) {
			CHECK(test.owner_thread != server->server_thread);
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, fixture.request->serial };
			GDExtensionProductionReportV1 report = {};
			GDExtensionProductionServiceBudgetV1 zero = { Enrollment::ABI, 0, 0, 0, 0, 0 };
			CHECK(fixture.registry.production_observe(ref, &zero, &report).native_error == ERR_UNAUTHORIZED);
			for (uint32_t phase = 0; phase < 5; ++phase) {
				server->sync();
				if (phase == 0) {
					// Mechanical private capture of a real CPU driver's issue report.
					RDD::QueueCallReport issued;
					CHECK(fixture.driver.command_queue_execute_and_present_observed(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), {}, issued) == OK);
					CHECK(issued.calls.size() == 1);
					if (issued.calls.size() == 1) {
						MutexLock lock(fixture.registry.mutex);
						auto &cell = fixture.request->natives[0];
						cell.request = fixture.request;
						cell.owner_index = 0;
						cell.member.source = issued.calls[0];
						fixture.request->native_count = 1;
					}
				}
				test.rt_returned.post();
				test.owner_ready.wait();
			}
		} else {
			test.rt_returned.post();
		}
		test.setup.main_released.post();
		owner.wait_to_finish();
		CHECK(test.initialized == OK);
		CHECK(test.passed);
		CHECK(test.setup.owner_cleaned);
	}
#endif
	static void singleton_control_fifo() {
		using namespace GDExtensionDeviceExit;
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server != nullptr)) {
			return;
		}
		if (!check_step(!(server->create_thread))) {
			return;
		}
		if (!check_step(server->server_thread == Thread::get_caller_id())) {
			return;
		}
		if (!check_step(RenderingDevice::get_singleton() == nullptr)) {
			return;
		}
		auto &registry = Registry::get_singleton();
		server->sync();
		auto *const original_frame = RegistrationContext::current;
		const auto before = singleton_counts();
		{
			SingletonEnrollment fixture;
			const Error initialized = fixture.initialize(server);
			if (!check_step(initialized == OK)) {
				return;
			}
			if (!check_step(fixture.request != nullptr)) {
				return;
			}
			auto *request = fixture.request;
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request->serial };
			ProductionControlOutput output = {};
			uint64_t serial = 0;
			GDExtensionProductionReportV1 public_report = {};
			GDExtensionProductionServiceBudgetV1 public_budget = { Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			if (!check_step(registry.production_observe(ref, &public_budget, &public_report).native_error == ERR_BUSY && request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
			// Even the actual RT owner cannot overtake its original FIFO.
			server->sync();
			if (!check_step(registry.production_observe(ref, &public_budget, &public_report).native_error == OK && request->control.state == ProductionControlState::OWNER_CONSUMED)) {
				return;
			}
			if (!check_step(registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial) == OK)) {
				return;
			}
			// A real queued stale serial cannot turn a merely pending cell into return.
			if (!check_step(server->_try_production_control_dispatch(fixture.instance, fixture.attachment.handle_serial, ref.handle_serial, serial + 1))) {
				return;
			}
			server->sync();
			if (!check_step(request->control.state == ProductionControlState::PENDING)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == ERR_BUSY)) {
				return;
			}
			if (!check_step(registry.production_control_schedule(ref, serial) == OK)) {
				return;
			}
			if (!check_step(request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == ERR_BUSY)) {
				return;
			}
			// Negative calls cannot consume the genuine queued admission. The
			// successful path below still runs only through the original FIFO.
			server->_production_control_dispatch(fixture.instance, fixture.attachment.handle_serial, ref.handle_serial, serial + 1);
			if (!check_step(request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
			server->_production_control_dispatch(0, fixture.attachment.handle_serial, ref.handle_serial, serial);
			if (!check_step(request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
#ifdef THREADS_ENABLED
			ForeignDispatch foreign{ server, fixture.instance, fixture.attachment.handle_serial, ref.handle_serial, serial };
			Thread thread;
			thread.start(&ForeignDispatch::run, &foreign);
			thread.wait_to_finish();
			if (!check_step(foreign.returned)) {
				return;
			}
			if (!check_step(foreign.called_thread != 0)) {
				return;
			}
			if (!check_step(foreign.called_thread != server->server_thread)) {
				return;
			}
			if (!check_step(request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == ERR_BUSY)) {
				return;
			}
#endif
			server->sync(); // Real NoThread FIFO, not a GPU completion assertion.
			if (!check_step(request->control.state == ProductionControlState::RETURNED)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == OK)) {
				return;
			}
			if (!check_step(output.request_serial == ref.handle_serial)) {
				return;
			}
			if (!check_step(output.control_serial == serial)) {
				return;
			}
			if (!check_step(output.result.native_error == OK)) {
				return;
			}

			// No-effect scope does not invent native completion: a controlled pending
			// marker makes the real RT release refuse and retain the original request.
			if (!check_step(registry.production_control_accept(ref, ProductionControlKind::RELEASE, &serial) == OK)) {
				return;
			}
			request->effects_pending = true;
			const Error scheduled_unknown = registry.production_control_schedule(ref, serial);
			if (scheduled_unknown == OK) {
				server->sync();
			}
			const bool obligation_retained = request->effects_pending;
			request->effects_pending = false; // Restore test input before any early exit.
			if (!check_step(scheduled_unknown == OK && obligation_retained)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == OK)) {
				return;
			}
			if (!check_step(output.result.native_error == ERR_BUSY && request->serial == ref.handle_serial)) {
				return;
			}
			// Each tuple mismatch goes through the actual queued Core receiver. Restore
			// only this test-owned field before the original owner consumes its error.
			for (uint32_t mismatch = 0; mismatch < 3; ++mismatch) {
				if (!check_step(registry.production_control_accept(ref, ProductionControlKind::CANCEL, &serial) == OK)) {
					return;
				}
				if (!check_step(registry.production_control_schedule(ref, serial) == OK)) {
					return;
				}
				uint64_t *field = mismatch == 0 ? &registry.device_records[fixture.device_slot].epoch : mismatch == 1 ? &registry.records[fixture.reference.slot].producer_registration_serial
																													  : &fixture.holder.handle_serial;
				const uint64_t saved = *field;
				*field = 0;
				server->sync();
				*field = saved;
				if (!check_step(request->control.state == ProductionControlState::RETURNED)) {
					return;
				}
				if (!check_step(registry.production_control_consume(ref, serial, &output) == OK)) {
					return;
				}
				if (!check_step(output.result.native_error == ERR_INVALID_DATA)) {
					return;
				}
				if (!check_step(!(request->cancelled))) {
					return;
				}
			}
			if (!check_step(registry.production_control_accept(ref, ProductionControlKind::CANCEL, &serial) == OK)) {
				return;
			}
			uint32_t queued = 0;
			// Identical fixed scalar Core commands fill the same bounded FIFO. Invalid
			// registry identity makes them inert when the original RS dispatches them.
			for (; queued < 65536; ++queued) {
				if (!server->_try_production_control_dispatch(0, 0, 0, 0)) {
					break;
				}
			}
			if (!check_step(queued > 0)) {
				return;
			}
			if (!check_step(queued < 65536)) {
				return;
			}
			if (!check_step(registry.production_control_schedule(ref, serial) == ERR_BUSY)) {
				return;
			}
			if (!check_step(request->control.state == ProductionControlState::PENDING)) {
				return;
			}
			if (!check_step(request->control.serial == serial)) {
				return;
			}
			if (!check_step(!(request->cancelled))) {
				return;
			}
			if (!check_step(registry.production_close(fixture.attachment).native_error == ERR_BUSY)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == ERR_BUSY)) {
				return;
			}
			server->sync();
			if (!check_step(request->control.state == ProductionControlState::PENDING)) {
				return;
			}
			if (!check_step(registry.production_control_schedule(ref, serial) == OK)) {
				return;
			}
			if (!check_step(request->control.state == ProductionControlState::QUEUED)) {
				return;
			}
			if (!check_step(!(request->cancelled))) {
				return;
			}
			server->sync();
			if (!check_step(request->cancelled)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == OK)) {
				return;
			}
			if (!check_step(output.result.native_error == OK)) {
				return;
			}
			if (!check_step(registry.production_control_accept(ref, ProductionControlKind::RELEASE, &serial) == OK)) {
				return;
			}
			if (!check_step(registry.production_control_schedule(ref, serial) == OK)) {
				return;
			}
			server->sync();
			if (!check_step(request->serial == ref.handle_serial)) {
				return;
			}
			if (!check_step(request->attachment && request->attachment->children == 1)) {
				return;
			}
			if (!check_step(registry.production_release(ref).native_error == ERR_BUSY)) {
				return;
			}
			if (!check_step(registry.production_control_consume(ref, serial, &output) == OK)) {
				return;
			}
			if (!check_step(output.result.native_error == OK)) {
				return;
			}
			if (!check_step(request->serial == 0)) {
				return;
			}
		}
		const auto after = singleton_counts();
		if (!check_step(after.instance == before.instance)) {
			return;
		}
		if (!check_step(after.services == before.services)) {
			return;
		}
		if (!check_step(after.records == before.records)) {
			return;
		}
		if (!check_step(after.views == before.views)) {
			return;
		}
		if (!check_step(after.devices == before.devices)) {
			return;
		}
		if (!check_step(after.device_refs == before.device_refs)) {
			return;
		}
		if (!check_step(after.attachments == before.attachments)) {
			return;
		}
		if (!check_step(after.private_refs == before.private_refs)) {
			return;
		}
		if (!check_step(after.external_refs == before.external_refs)) {
			return;
		}
		if (!check_step(RegistrationContext::current == original_frame)) {
			return;
		}
		if (!check_step(RenderingServer::get_singleton() == server)) {
			return;
		}
		if (!check_step(RenderingDevice::get_singleton() == nullptr)) {
			return;
		}
	}
	static void accepted_arena_lifecycle() {
		Enrollment fixture;
		auto &registry = *fixture.registry;
		auto &record = registry.records[0];
		auto &device = registry.device_records[0];
		const uint32_t private_before = record.private_refs, device_before = device.refs;
		GDExtensionProductionCapacityV1 whole = { Enrollment::ABI, 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		auto single = whole;
		single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
		single.native_memberships = 16;
		single.retained_payload_bytes = 4;
		const auto attachment = fixture.accept(whole);
		CHECK(record.private_refs == private_before + 1);
		CHECK(device.refs == device_before + 1);
		CHECK(fixture.holder.production_children == 1);
		CHECK(record.cleanup_reservations == 2);
		CHECK(record.cleanup_by_phase[uint8_t(GDExtensionDeviceExit::CleanupPhase::BINDING)] == 2);
		Request *first = nullptr, *second = nullptr, *refused = nullptr;
		REQUIRE(fixture.reserve(attachment, single, 11, first) == OK);
		const auto first_ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, first->serial };
		const uint64_t serial_before = registry.production_serials.next;
		auto too_large = single;
		too_large.retained_payload_bytes = 5;
		CHECK(fixture.reserve(attachment, too_large, 12, refused) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(refused == nullptr);
		CHECK(registry.production_serials.next == serial_before);
		CHECK(first->attachment->children == 1);
		// With current=1 the old overall quote had 7 slots; this second real
		// Reserve needs four disjoint slots and must succeed in the quoted arena.
		REQUIRE(fixture.reserve(attachment, single, 12, second) == OK);
		CHECK(first->command_count == 4);
		CHECK(second->command_count == 4);
		CHECK((first->commands + first->command_count <= second->commands || second->commands + second->command_count <= first->commands));
		CHECK(first->attachment->children == 2);
		CHECK(first->attachment == second->attachment);
		CHECK(first->payload_offset != second->payload_offset);
		const auto second_ref = GDExtensionProductionRequestRefV1{ Enrollment::INSTANCE, second->serial };
		const uint8_t payload[4] = { 1, 2, 3, 4 };
		const GDExtensionProductionUploadV1 upload = { Enrollment::ABI, 10, 0, 0, 0, 4 };
		CHECK(registry.production_prepare_upload(first_ref, &upload, payload, 4).native_error == OK);
		REQUIRE(registry.production_cancel(first_ref).native_error == OK);
		CHECK(registry.production_prepare_upload(first_ref, &upload, payload, 4).native_error == ERR_BUSY);
		REQUIRE(registry.production_release(first_ref).native_error == OK);
		CHECK(registry.production_release(first_ref).native_error != OK);
		CHECK(second->attachment->children == 1);
		CHECK(registry.production_close(attachment).native_error == ERR_BUSY);
		CHECK(second->cancelled);
		CHECK(record.private_refs == private_before + 1);
		CHECK(fixture.holder.production_children == 1);
		REQUIRE(registry.production_release(second_ref).native_error == OK);
		REQUIRE(registry.production_close(attachment).native_error == OK);
		CHECK(record.private_refs == private_before);
		CHECK(device.refs == device_before);
		CHECK(fixture.holder.production_children == 0);
		CHECK(record.cleanup_reservations == 0);
		CHECK(record.cleanup_by_phase[uint8_t(GDExtensionDeviceExit::CleanupPhase::BINDING)] == 0);
		for (auto *entry : registry.production_attachments) {
			CHECK(entry == nullptr);
		}
		CHECK(fixture.rd.draw_graph.qualification_command_count() == 1);
	}

	static void actual_entity_hold() {
		Enrollment fixture;
		auto &registry = *fixture.registry;
		auto &record = registry.records[0];
		auto &device = registry.device_records[0];
		auto &action = record.levels[GDEXTENSION_INITIALIZATION_SCENE].binding_action;
		const GDExtensionProductionCapacityV1 capacity = { Enrollment::ABI, 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		GDExtensionProductionMetadataQuoteV1 quote = {};
		REQUIRE(registry.production_quote(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 9 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, &capacity, &quote).native_error == OK);
		GDExtensionProductionAttachmentRefV1 refused = {};
		GDExtensionProductionInstallationReportV1 report = {};
		action.services = 0;
		CHECK(registry.production_accept(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 9 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, { Enrollment::INSTANCE, 3 }, &quote, &refused, &report).native_error == ERR_UNAUTHORIZED);
		CHECK(refused.handle_serial == 0);
		CHECK(fixture.rd.production_entity_holds == nullptr);
		action.services = 1u << 5;
		action.generation = 2;
		CHECK(registry.production_accept(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 9 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, { Enrollment::INSTANCE, 3 }, &quote, &refused, &report).native_error == ERR_UNAUTHORIZED);
		action.generation = 1;
		registry.live_services = 0;
		CHECK(registry.production_accept(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 9 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, { Enrollment::INSTANCE, 3 }, &quote, &refused, &report).native_error == ERR_UNAVAILABLE);
		registry.live_services = 1u << 5;
		const auto attachment = fixture.accept(capacity);
		auto *edge = fixture.rd.production_entity_holds;
		REQUIRE(edge != nullptr);
		CHECK(edge->held_device == &fixture.rd);
		CHECK(edge->epoch == device.epoch);
		CHECK(edge->terminal_action == action.serial);
		++device.epoch;
		CHECK(registry.production_close(attachment).native_error == ERR_INVALID_DATA);
		CHECK(fixture.rd.production_entity_holds == edge);
		--device.epoch;
		CHECK(registry.release_code_hold(&Enrollment::ABI, { Enrollment::INSTANCE, 3 }) == ERR_BUSY);
		CHECK(registry.service_stopping(1u << 5) == ERR_BUSY);
		record.guard_enrollment = GDExtensionDeviceExit::GuardEnrollment::REVOKED;
		CHECK(registry.service_stopping(1u << 5) == ERR_BUSY);
		record.guard_enrollment = GDExtensionDeviceExit::GuardEnrollment::QUALIFIED;
		ERR_PRINT_OFF;
		fixture.rd.finalize();
		ERR_PRINT_ON;
		CHECK(device.live);
		CHECK(fixture.rd.production_finalization_started.load());
		CHECK_FALSE(fixture.rd.native_finalization_returned);
		CHECK(fixture.rd.driver == &fixture.driver);
		CHECK(fixture.rd.frames.size() == 1);
		CHECK(fixture.rd.production_entity_holds == edge);
		CHECK(device.object == &fixture.rd);
		CHECK(registry.production_close(attachment).native_error == OK);
		CHECK(fixture.rd.production_entity_holds == nullptr);
		CHECK(fixture.rd.production_entity_registry.load() == nullptr);
		CHECK_FALSE(fixture.rd.native_finalization_returned);
		// No native allocations exist in this fake-driver fixture. Remove only
		// its explicit CPU frame stub and then exercise the real empty finalize.
		fixture.rd.frames.clear();
		fixture.rd.driver = nullptr;
		fixture.rd.finalize();
		CHECK(fixture.rd.native_finalization_returned);
	}
	static void entity_hold_after_loss() {
		Enrollment fixture;
		auto &registry = *fixture.registry;
		// Match the actual B Host's original SCENE USER terminal route.
		auto &scene = registry.records[0].levels[GDEXTENSION_INITIALIZATION_SCENE];
		scene.user_action = scene.binding_action;
		scene.user_action.kind = GDExtensionDeviceExit::TerminalActionKind::LEVEL_USER;
		scene.binding_action = {};
		const GDExtensionProductionCapacityV1 whole = { Enrollment::ABI, 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		const auto attachment = fixture.accept(whole);
		auto single = whole;
		single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
		single.native_memberships = 16;
		single.retained_payload_bytes = 4;
		Request *request = nullptr;
		REQUIRE(fixture.reserve(attachment, single, 21, request) == OK);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, request->serial };
		registry.device_lost(&fixture.rd);
		CHECK_FALSE(registry.device_records[0].live);
		ERR_PRINT_OFF;
		fixture.rd.finalize();
		ERR_PRINT_ON;
		CHECK_FALSE(registry.device_records[0].live);
		CHECK(fixture.rd.production_entity_holds != nullptr);
		// Controlled unknown obligation: loss cannot promote it to completion or
		// turn a metadata Close into actual physical disposal.
		request->effects_pending = true;
		CHECK(registry.production_cancel(ref).native_error == ERR_BUSY);
		CHECK(registry.production_release(ref).native_error == ERR_BUSY);
		CHECK(registry.production_close(attachment).native_error == ERR_BUSY);
		CHECK(fixture.rd.production_entity_holds != nullptr);
		CHECK_FALSE(fixture.rd.native_finalization_returned);
		request->effects_pending = false; // Remove only the injected CPU-test state.
		REQUIRE(registry.production_release(ref).native_error == OK);
		REQUIRE(registry.production_close(attachment).native_error == OK);
		CHECK(fixture.rd.production_entity_holds == nullptr);
		CHECK_FALSE(fixture.rd.native_finalization_returned);
	}

	static void finalization_preserves_qualification_cleanup() {
		Enrollment fixture;
		auto &registry = *fixture.registry;
		registry.device_refs[0].qualification_main = true;
		registry.device_refs[1].qualification_main = true;
		REQUIRE(registry.qualification_open(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, { Enrollment::INSTANCE, 3 }, &fixture.qualification).native_error == OK);
		CHECK(fixture.holder.qualification_children == 1);
		const GDExtensionProductionCapacityV1 whole = { Enrollment::ABI, 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		const auto attachment = fixture.accept(whole);
		auto single = whole;
		single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
		single.native_memberships = 16;
		single.retained_payload_bytes = 4;
		Request *request = nullptr;
		REQUIRE(fixture.reserve(attachment, single, 31, request) == OK);
		const GDExtensionProductionRequestRefV1 ref = { Enrollment::INSTANCE, request->serial };
		ERR_PRINT_OFF;
		fixture.rd.finalize();
		ERR_PRINT_ON;
		CHECK(registry.device_records[0].live);
		CHECK_FALSE(fixture.rd.native_finalization_returned);
		CHECK(fixture.rd.driver == &fixture.driver);
		const uint8_t payload[4] = { 1, 2, 3, 4 };
		const GDExtensionProductionUploadV1 upload = { Enrollment::ABI, 10, 0, 0, 0, 4 };
		CHECK(registry.production_prepare_upload(ref, &upload, payload, 4).native_error == ERR_BUSY);
		GDExtensionQualificationBackingV1 backing = {};
		CHECK(registry.qualification_backing(fixture.qualification, 1, &backing).native_error == ERR_BUSY);
		GDExtensionQualificationBridgeRefV1 extra = {};
		CHECK(registry.qualification_open(&Enrollment::ABI, { Enrollment::INSTANCE, 5 }, { Enrollment::INSTANCE, 6 }, { Enrollment::INSTANCE, 7 }, { Enrollment::INSTANCE, 3 }, &extra).native_error != OK);
		CHECK(extra.handle_serial == 0);
		REQUIRE(registry.production_cancel(ref).native_error == OK);
		REQUIRE(registry.production_release(ref).native_error == OK);
		REQUIRE(registry.production_close(attachment).native_error == OK);
		CHECK(fixture.holder.production_children == 0);
		// The old owner must still reach its original cleanup while native teardown
		// has not started. This fails if Busy incorrectly writes DeviceClosed.
		REQUIRE(registry.qualification_close(fixture.qualification).native_error == OK);
		fixture.qualification = {};
		CHECK(fixture.holder.qualification_children == 0);
		CHECK(registry.records[0].private_refs == 1);
		CHECK(registry.device_records[0].refs == 0);
		CHECK(registry.device_records[0].live);
		CHECK_FALSE(fixture.rd.native_finalization_returned);
	}
	static void immutable_payload() {
		using namespace GDExtensionDeviceExit;
		static Registry registry;
		ProductionAttachment attachment;
		ProductionRequest request;
		ProductionResourceSlot resource;
		ProductionActionCell actions[2];
		ProductionUseCell use;
		use.request = &request;
		use.declaration = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
		request.uses = &use;
		request.use_count = 1;
		uint8_t storage[4] = {};
		attachment.payload = storage;
		request.attachment = &attachment;
		request.serial = 9;
		request.actions = actions;
		request.action_count = 2;
		request.resources = &resource;
		request.resource_count = 1;
		request.capacity.retained_payload_bytes = sizeof(storage);
		resource.request = &request;
		resource.request_serial = request.serial;
		resource.declaration.resource_kind = GDEXTENSION_PRODUCTION_STORAGE_V1;
		resource.declaration.physical_bytes = 8;
		resource.imported = true; // Pure metadata test: no physical/native call occurs.
		for (uint32_t i = 0; i < 2; ++i) {
			actions[i].request = &request;
			actions[i].declaration.canonical_suboperation = i + 10;
			actions[i].declaration.action_kind = GDEXTENSION_PRODUCTION_UPLOAD_V1;
			actions[i].declaration.use_count = 1;
		}
		const uint8_t original[4] = { 2, 4, 6, 8 }, changed[4] = { 2, 4, 6, 9 };
		GDExtensionProductionUploadV1 upload = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 10, 0, 0, 0, 4 };
		upload.destination_offset = 4;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_INVALID_DATA);
		CHECK(request.payload_used == 0);
		upload.destination_offset = 0;
		REQUIRE(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == OK);
		CHECK(request.payload_used == 4);
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == OK);
		CHECK(request.payload_used == 4);
		CHECK(registry.production_prepare_upload_locked(request, upload, changed, 4).native_error == ERR_INVALID_DATA);
		CHECK(storage[3] == 8);
		upload.canonical_suboperation = 11;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(actions[1].prepared);
		CHECK(registry.production_freeze_locked(request) == ERR_UNCONFIGURED);
		CHECK_FALSE(request.frozen);
		request.action_count = 1;
		REQUIRE(registry.production_freeze_locked(request) == OK);
		upload.canonical_suboperation = 10;
		CHECK(registry.production_prepare_upload_locked(request, upload, original, 4).native_error == ERR_BUSY);
		CHECK(storage[3] == 8);
	}
	static Error quote(const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands = 0, uint32_t workers = 0) {
		return GDExtensionDeviceExit::Registry::production_layout(capacity, out, current_commands, workers);
	}
};
TEST_CASE("[ProductionRequest] quote checks dimensions before reserving real cells") {
	GDExtensionProductionCapacityV1 capacity = {};
	capacity.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	GDExtensionProductionMetadataQuoteV1 quote = {};
	CHECK(Access::quote(capacity, quote) == ERR_INVALID_PARAMETER);
	capacity.requests = 1;
	capacity.suboperations = 2;
	capacity.resources = 2;
	capacity.resource_uses = 2;
	capacity.graph_memberships = 2;
	capacity.native_memberships = 2;
	capacity.return_cells = 2;
	REQUIRE(Access::quote(capacity, quote) == OK);
	CHECK(quote.allocation_bytes > sizeof(GDExtensionDeviceExit::ProductionRequest));
	CHECK(quote.cleanup_slots == 2);
	CHECK(quote.graph_frames == 3);
	CHECK(quote.graph_slots == 5);
	const uint64_t original_bytes = quote.allocation_bytes;
	capacity.retained_payload_bytes = 64;
	REQUIRE(Access::quote(capacity, quote) == OK);
	CHECK(quote.allocation_bytes >= original_bytes + 64);
	capacity.readback_bytes = UINT64_MAX;
	CHECK(Access::quote(capacity, quote) == ERR_PARAMETER_RANGE_ERROR);
	capacity.readback_bytes = 0;
	capacity.graph_memberships = UINT32_MAX;
	CHECK(Access::quote(capacity, quote) == ERR_PARAMETER_RANGE_ERROR);
}
TEST_CASE("[ProductionRequest] forged metadata never resolves an original attachment") {
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionProductionReportV1 report = {};
	GDExtensionProductionServiceBudgetV1 budget = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 0, 0, 0, 0, 0 };
	CHECK(registry.production_observe({ UINT64_MAX, 1 }, &budget, &report).native_error != OK);
	CHECK(registry.production_cancel({ UINT64_MAX, 1 }).native_error != OK);
	CHECK(registry.production_release({ UINT64_MAX, 1 }).native_error != OK);
}
} // namespace TestProductionRequest

TEST_CASE("[ProductionRequest] payload retry is exact and the finite batch freezes before effects") {
	TestProductionRequest::Access::immutable_payload();
}

TEST_CASE("[ProductionRequest] attachment quote retains each request command prefix") {
	using TestProductionRequest::Access;
	GDExtensionProductionCapacityV1 whole = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 2, 2, 2, 2, 2, 32, 0, 2, 0, 0 };
	GDExtensionProductionCapacityV1 single = whole;
	single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
	single.native_memberships = 16;
	GDExtensionProductionMetadataQuoteV1 total = {}, first = {}, second = {};
	REQUIRE(Access::quote(whole, total, 1, 0) == OK);
	REQUIRE(Access::quote(single, first, 1, 0) == OK);
	REQUIRE(Access::quote(single, second, 1, 0) == OK);
	CHECK(total.graph_slots == 8);
	CHECK(total.graph_slots == first.graph_slots + second.graph_slots);
	CHECK(total.graph_frames == first.graph_frames + second.graph_frames);
	const uint64_t without_workers = total.allocation_bytes;
	REQUIRE(Access::quote(whole, total, 7, 3) == OK);
	REQUIRE(Access::quote(single, first, 7, 3) == OK);
	REQUIRE(Access::quote(single, second, 7, 3) == OK);
	CHECK(total.graph_slots == first.graph_slots + second.graph_slots);
	CHECK(total.allocation_bytes > without_workers);
	const auto retained = total;
	CHECK(Access::quote(whole, total, UINT32_MAX, 0) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(total.allocation_bytes == retained.allocation_bytes);
	CHECK(total.graph_slots == retained.graph_slots);
	CHECK(Access::quote(whole, total, 1, UINT32_MAX) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(total.graph_slots == retained.graph_slots);
}

TEST_CASE("[ProductionRequest] actual accepted arena reserves two requests and refunds on close") {
	TestProductionRequest::Access::accepted_arena_lifecycle();
}

TEST_CASE("[ProductionRequest] original entity hold blocks actual finalize until exact return") {
	TestProductionRequest::Access::actual_entity_hold();
}
TEST_CASE("[ProductionRequest] device loss invalidates work but preserves unknown entity obligation") {
	TestProductionRequest::Access::entity_hold_after_loss();
}

TEST_CASE("[ProductionRequest] refused finalize preserves original qualification close on same device") {
	TestProductionRequest::Access::finalization_preserves_qualification_cleanup();
}

TEST_CASE("[SceneTree][ProductionRequest] singleton RS FIFO returns retained control to original owner") {
	TestProductionRequest::Access::singleton_control_fifo();
}

TEST_CASE("[SceneTree][ProductionRequest] refused singleton fixture never claims default record zero") {
	TestProductionRequest::Access::singleton_initialization_refusal();
}

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] public Observe hands owner maintenance to original RS FIFO") {
	TestProductionRequest::Access::singleton_split_owner_observe();
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] owned storage creates on RT and retires across bounded owner turns") {
	TestProductionRequest::Access::singleton_split_owner_storage();
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] create Busy return then owner seal rejects retry without losing request") {
	TestProductionRequest::Access::singleton_split_owner_storage(true);
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] owner set bindings survive FIFO Busy and input lifetime through actual RT create") {
	TestProductionRequest::Access::singleton_split_owner_set();
}
#endif
