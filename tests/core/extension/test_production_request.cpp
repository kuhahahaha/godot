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
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_manager.h"
#include "core/extension/gdextension_production_request.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
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
		void initialize(uint32_t p_storage_bytes) {
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
			binding.length = p_storage_bytes;
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
		bool upload_mode = false, staging_live = false;
		uint8_t upload_memory[3][128] = {};
		uint32_t upload_copies = 0;
		uint8_t *buffer_map(BufferID id) override {
			CHECK((upload_mode && id == BufferID(2) && staging_live));
			return upload_mode && id == BufferID(2) && staging_live ? upload_memory[2] : nullptr;
		}
		void buffer_unmap(BufferID id) override { CHECK((upload_mode && id == BufferID(2) && staging_live)); }
		void command_copy_buffer(CommandBufferID, BufferID source, BufferID destination, VectorView<BufferCopyRegion> regions) override {
			CHECK((upload_mode && source == BufferID(2) && destination == BufferID(1) && staging_live && buffer_live));
			for (uint32_t i = 0; i < regions.size(); ++i) {
				const auto &region = regions[i];
				CHECK((region.src_offset + region.size <= 64 && region.dst_offset + region.size <= 128));
				if (region.src_offset + region.size <= 64 && region.dst_offset + region.size <= 128) {
					memcpy(upload_memory[1] + region.dst_offset, upload_memory[2] + region.src_offset, region.size);
					++upload_copies;
				}
			}
		}

		BufferID buffer_create(uint64_t bytes, BitField<BufferUsageBits> usage, MemoryAllocationType allocation, uint64_t) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			if (upload_mode && allocation == MEMORY_ALLOCATION_TYPE_CPU) {
				CHECK((bytes == 64 && usage.has_flag(BUFFER_USAGE_TRANSFER_FROM_BIT) && !staging_live && allocations == 1));
				++allocations;
				staging_live = true;
				return BufferID(2);
			}
			CHECK((bytes == (upload_mode ? 128u : 16u) && allocation == MEMORY_ALLOCATION_TYPE_GPU));
			CHECK(usage.has_flag(BUFFER_USAGE_STORAGE_BIT));
			CHECK(!buffer_live);
			++allocations;
			buffer_live = true;
			return BufferID(1);
		}
		void buffer_free(BufferID id) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			if (upload_mode && id == BufferID(2)) {
				CHECK(staging_live);
				staging_live = false;
				++buffer_frees;
				return;
			}
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
		uint64_t limit_get(Limit p_limit) override {
			// Immutable limits are queried by owner-side prepare before RT append.
			if (p_limit == LIMIT_MAX_PUSH_CONSTANT_SIZE) {
				return 128;
			}
			if (p_limit == LIMIT_MAX_BOUND_UNIFORM_SETS || p_limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X || p_limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y || p_limit == LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z) {
				return 64;
			}
			FAIL_CHECK("Unexpected source-service CPU driver limit: ", uint32_t(p_limit));
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
			CHECK((binding.binding == 0 && binding.type == UNIFORM_TYPE_STORAGE_BUFFER && binding.length == (upload_mode ? 128u : 16u) && binding.writable));
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
		uint32_t dispatches = 0;
		void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID set, ShaderID shader, uint32_t index) override {
			CHECK(set == UniformSetID(82));
			CHECK(shader == ShaderID(80));
			CHECK(index == 0);
			CHECK(set_live);
		}
		uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count) const override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			return 0;
		}
		void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID> sets, ShaderID shader, uint32_t first, uint32_t count, uint32_t dynamic) override {
			CHECK(sets.size() == 1);
			CHECK(shader == ShaderID(80));
			CHECK(first == 0);
			CHECK(count == 1);
			CHECK(dynamic == 0);
			CHECK(set_live);
		}
		void command_bind_compute_pipeline(CommandBufferID, PipelineID id) override { CHECK(id == PipelineID(81)); }
		void command_compute_dispatch(CommandBufferID, uint32_t x, uint32_t y, uint32_t z) override {
			CHECK(x == 1);
			CHECK(y == 1);
			CHECK(z == 1);
			++dispatches;
		}
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
		Error initialize_owner(uint32_t loader_kind = 1, bool owned_storage = false, bool owned_set = false, GDExtensionDeviceExit::ProductionRecordReceiver receiver = nullptr, void *userdata = nullptr, bool keep_list = false, uint32_t record_actions = 1, bool upload_prefix = false, GDExtensionDeviceExit::ProductionMetadataReceiver metadata_receiver = nullptr) {
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
			if (record_actions > 1) {
				whole.suboperations = whole.graph_memberships = record_actions + 1;
				whole.native_memberships = 2048;
			}
			if (upload_prefix) {
				whole.graph_memberships = 5;
				whole.native_memberships = 256;
				whole.retained_payload_bytes = 128;
			}
			GDExtensionProductionMetadataQuoteV1 quote = {};
			if (Error error = Error(registry.production_quote(&Enrollment::ABI, ready, producer, context, session, &whole, &quote).native_error); error != OK) {
				return error;
			}
			GDExtensionProductionInstallationReportV1 report = {};
			if (Error error = Error(registry.production_accept(&Enrollment::ABI, ready, producer, context, session, { instance, holder.handle_serial }, &quote, &attachment, &report).native_error); error != OK) {
				return error;
			}
			if (receiver) {
				if (Error error = registry.production_install_record_receiver(ready, attachment, receiver, userdata, metadata_receiver); error != OK) {
					return error;
				}
				if (Error error = Error(registry.production_open(ready, attachment).native_error); error != OK) {
					return error;
				}
			}
			auto single = whole;
			single.requests = single.suboperations = single.resources = single.resource_uses = single.graph_memberships = single.return_cells = 1;
			single.native_memberships = 16;
			single.retained_payload_bytes = 4;
			if (record_actions > 1) {
				single.suboperations = single.graph_memberships = record_actions;
				single.native_memberships = 2048;
			}
			if (upload_prefix) {
				single.graph_memberships = 4;
				single.native_memberships = 256;
				single.retained_payload_bytes = 96;
			}
			if (owned_set) {
				single.resources = single.return_cells = 3;
				single.resource_uses = 3;
			}
			const GDExtensionProductionOperationV1 operation = { Enrollment::ABI, 91, 1, 10, 10 + record_actions };
			const GDExtensionProductionActionV1 action = { 10, uint32_t(receiver && !upload_prefix ? GDEXTENSION_PRODUCTION_DISPATCH_V1 : GDEXTENSION_PRODUCTION_UPLOAD_V1), 0, 0, 1, uint32_t(keep_list ? GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1 : GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1), UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, uint32_t(owned_storage ? GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 : GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1), 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			const GDExtensionProductionResourceV1 set_resources[] = {
				{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, upload_prefix ? 128u : 16u, 0 },
				{ GDEXTENSION_PRODUCTION_SET_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 }
			};
			const GDExtensionProductionUseV1 set_use = { 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, upload_prefix ? 128u : 16u, 0 };
			if (receiver) {
				LocalVector<GDExtensionProductionActionV1> action_rows;
				action_rows.resize(record_actions);
				for (uint32_t i = 0; i < record_actions; ++i) {
					action_rows[i] = action;
					action_rows[i].canonical_suboperation = 10 + i;
					if (record_actions > 1) {
						action_rows[i].list_boundary = uint32_t(i + 1 == record_actions ? GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 : GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1);
					}
				}
				GDExtensionProductionRequestRefV1 reserved = {};
				if (Error error = Error(registry.production_reserve(attachment, &operation, &single, action_rows.ptr(), action_rows.size(), owned_set ? set_resources : &resource, owned_set ? 3 : 1, owned_set ? &set_use : &use, 1, nullptr, 0, &reserved).native_error); error != OK) {
					return error;
				}
				MutexLock lock(registry.mutex);
				RenderingDevice *device = nullptr;
				return registry.production_request_locked(reserved, false, request, device);
			}
			MutexLock lock(registry.mutex);
			ProductionAttachment *accepted = nullptr;
			RenderingDevice *device = nullptr;
			if (Error error = registry.production_attachment_locked(attachment, true, accepted, device); error != OK) {
				return error;
			}
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
	template <typename NextRT>
	static bool drain_request_cursor(SingletonEnrollment &fixture, GDExtensionProductionRequestRefV1 ref, NextRT next_rt) {
		using namespace GDExtensionDeviceExit;
		auto &request = *fixture.request;
		bool passed = true;
		auto check = [&](bool value) {
			CHECK_MESSAGE(value, "Request tail serial=", ref.handle_serial, " cursor=", request.maintenance_serial, " cell=", uint32_t(request.control.state));
			passed &= value;
		};
		auto owned_active = [&]() {
			MutexLock lock(fixture.rd._thread_safe_);
			const auto &cursor = fixture.rd.native_disposal_continuation;
			const bool original = fixture.rd.native_disposal_active && cursor.production_owner == &request;
			check(!original || (request.maintenance_serial && request.maintenance_serial == cursor.serial));
			return original && request.maintenance_serial && request.maintenance_serial == cursor.serial;
		};
		check(request.control.state == ProductionControlState::OWNER_CONSUMED);
		auto budget = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, 0, 1, 1, 1, 0 };
		GDExtensionProductionReportV1 report = {};
		uint32_t turns = 0;
		for (; turns < 64 && owned_active(); ++turns) {
			if (request.control.state == ProductionControlState::OWNER_CONSUMED) {
				budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			}
			const Error scheduled = Error(fixture.registry.production_observe(ref, &budget, &report).native_error);
			check(scheduled == OK || scheduled == ERR_BUSY);
			if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
				next_rt(); // Only the original RS FIFO executes an accepted service cell.
				if (request.control.state == ProductionControlState::RETURNED) {
					check(request.control.output.service_usage.receipts <= 1 && request.control.output.service_usage.passes <= 1);
				}
				const Error consumed = Error(fixture.registry.production_observe(ref, &budget, &report).native_error);
				check(consumed == OK || consumed == ERR_BUSY);
			}
			check(request.serial == ref.handle_serial);
		}
		INFO("Request tail turns=", turns, "/64 cursor=", request.maintenance_serial, " cause=", report.cause.native_error);
		{
			MutexLock lock(fixture.rd._thread_safe_);
			check(!fixture.rd.native_disposal_active || fixture.rd.native_disposal_continuation.production_owner != &request);
			check(request.maintenance_serial == 0);
		}
		check(request.control.state == ProductionControlState::OWNER_CONSUMED);
		return passed;
	}
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
				test.check(drain_request_cursor(fixture, ref, [&]() { test.rendezvous(); }));
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
		bool record_window = false, keep_list = false, close_before_ack = false, budget_pause = false, upload_prefix = false;
		GDExtensionProductionAppendV1 upload_append = {};
		uint32_t metadata_calls = 0;
		bool metadata_returned = false, metadata_release = false;
		uint64_t release_control = 0;
		GDExtensionDeviceExit::ProductionControlOutput release_output = {};
		uint32_t receiver_calls = 0;
		uint64_t receiver_thread = 0;
		GDExtensionDeviceExit::ProductionPhysicalContextRef expired_context;
		GDExtensionProductionSegmentV1 recorded_segment = {};
		static void metadata_receiver(void *opaque, GDExtensionProductionAttachmentRefV1 attachment) {
			auto &test = *static_cast<SplitSet *>(opaque);
			auto &fixture = *test.setup.fixture;
			auto &registry = fixture.registry;
			auto &request = *fixture.request;
			using namespace GDExtensionDeviceExit;
			++test.metadata_calls;
			test.check(__LINE__, Registry::is_production_metadata_entry() && RegistrationContext::current && RegistrationContext::current->level_phase == LevelPhase::PRODUCTION_METADATA);
			test.check(__LINE__, Thread::get_caller_id() == registry.records[fixture.reference.slot].owner_thread && Thread::get_caller_id() != fixture.rd.render_thread_id);
			test.check(__LINE__, attachment.handle_serial == fixture.attachment.handle_serial && registry.records[fixture.reference.slot].quiescing);
			const auto ref = GDExtensionProductionRequestRefV1{ fixture.instance, request.serial };
			uint64_t control = 0;
			test.check(__LINE__, registry.production_record_schedule(ref, &control) == ERR_UNAUTHORIZED);
			const auto dispatch = GDExtensionProductionDispatchV1{ Enrollment::ABI, 10, 0, 2, 1, 1, 1, 1, 0, 0 };
			test.check(__LINE__, registry.production_prepare_dispatch(ref, &dispatch, nullptr, 0).native_error == ERR_UNAUTHORIZED);
			GDExtensionProductionAppendV1 forbidden = {};
			test.check(__LINE__, registry.production_append(test.expired_context, ref, 10, request.revision, &forbidden).native_error == ERR_UNAUTHORIZED);
			if (test.metadata_calls == 1) {
				// The initial record return was consumed before this fresh ACK frame.
				test.check(__LINE__, !test.metadata_release && request.control.state == ProductionControlState::OWNER_CONSUMED);
				test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision + 1).native_error == ERR_DOES_NOT_EXIST);
				test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == OK);
				test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == OK);
			} else {
				// A later RELEASE return still owns its actual cell. Preserve the
				// original ACK and consume that return before any arena reclamation.
				test.check(__LINE__, test.metadata_release && request.acknowledged_segment == test.recorded_segment.segment);
				test.check(__LINE__, request.control.state == ProductionControlState::RETURNED && request.control.input.kind == ProductionControlKind::RELEASE && request.control.serial == test.release_control);
				test.check(__LINE__, registry.production_control_consume(ref, test.release_control, &test.release_output) == OK);
			}
			test.check(__LINE__, registry.production_close(attachment).native_error == ERR_BUSY); // Real callback/code return still owns its frame.
			test.metadata_returned = true;
		}
		static GDExtensionDeviceSessionResultV1 owner_ack(SingletonEnrollment &fixture, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionSegmentV1 &segment) {
			GDExtensionDeviceExit::CallbackScope owner_entry(fixture.reference);
			if (owner_entry.get_error() != OK) {
				return { int32_t(owner_entry.get_error()), 0, 0, 0 };
			}
			return fixture.registry.production_ack_segment(ref, segment.segment, segment.segment_revision);
		}
		static void record_receiver(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *operation) {
			auto &test = *static_cast<SplitSet *>(opaque);
			auto &fixture = *test.setup.fixture;
			auto &request = *fixture.request;
			auto &registry = fixture.registry;
			++test.receiver_calls;
			test.receiver_thread = Thread::get_caller_id();
			test.expired_context = context;
			test.check(__LINE__, test.receiver_thread == fixture.driver.execution_thread && test.receiver_thread != registry.records[fixture.reference.slot].owner_thread);
			test.check(__LINE__, operation && operation->owner_request_nonce == request.operation.owner_request_nonce);
			test.check(__LINE__, registry.records[fixture.reference.slot].active_callbacks > 0);

			if (test.upload_prefix) {
				const Error error = Error(registry.production_append(context, ref, 10, request.revision, &test.upload_append).native_error);
				if (test.receiver_calls == 1) {
					test.check(__LINE__, error == ERR_BUSY && test.upload_append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 && test.upload_append.recorded_bytes == 64 && test.upload_append.parent_bytes == 96 && test.upload_append.graph_fact_count == 1);
					test.check(__LINE__, request.next_action == 0 && !request.cancelled && !request.effects_pending);
				} else {
					test.check(__LINE__, error == OK && test.upload_append.state == GDEXTENSION_PRODUCTION_APPENDED_V1 && test.upload_append.recorded_bytes == 96 && test.upload_append.parent_bytes == 96 && test.upload_append.graph_fact_count == 1);
					test.check(__LINE__, request.next_action == 1 && request.graphs[1].captured.begin == 64 && request.graphs[1].captured.end == 96);
				}
				return;
			}
			if (test.budget_pause) {
				while (request.next_action < request.action_count) {
					GDExtensionProductionAppendV1 part = {};
					const Error error = Error(registry.production_append(context, ref, 10 + request.next_action, request.revision, &part).native_error);
					if (error == ERR_BUSY) {
						test.check(__LINE__, request.next_action == 64 && !request.cancelled && !request.effects_pending);
						return;
					}
					test.check(__LINE__, error == OK && part.state == GDEXTENSION_PRODUCTION_APPENDED_V1);
					if (error != OK) {
						return;
					}
				}
				return;
			}
			GDExtensionProductionAppendV1 appended = {};
			auto forged = context;
			++forged.frame_serial;
			test.check(__LINE__, registry.production_append(forged, ref, 10, request.revision, &appended).native_error == ERR_UNAUTHORIZED);
			test.check(__LINE__, registry.production_append(context, ref, 11, request.revision, &appended).native_error == ERR_INVALID_DATA);
			test.check(__LINE__, request.next_action == 0 && request.graph_count == 0);
			test.check(__LINE__, registry.production_append(context, ref, 10, request.revision, &appended).native_error == OK);
			test.check(__LINE__, appended.state == GDEXTENSION_PRODUCTION_APPENDED_V1 && request.next_action == 1 && request.graph_count == 1);
			if (test.keep_list) {
				test.check(__LINE__, request.compute_list_open && fixture.rd.compute_list.active);
				return; // Actual protected scope return must close and seal this prefix.
			}
			test.check(__LINE__, registry.production_close_segment(context, ref, &test.recorded_segment).native_error == OK);
			test.check(__LINE__, test.recorded_segment.segment && request.acknowledged_segment == 0);
		}

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
				FAIL_CHECK("SplitSet failed original source check at line ", line);
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
			fixture.driver.upload_mode = test.upload_prefix;
			test.initialized = fixture.initialize_owner(1, false, true, test.record_window ? &record_receiver : nullptr, &test, test.keep_list, test.budget_pause ? 65u : 1u, test.upload_prefix, test.close_before_ack ? &metadata_receiver : nullptr);
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
			const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, test.upload_prefix ? 128u : 16u, 1, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == ERR_BUSY);
			test.rendezvous(2);
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == OK);
			test.rendezvous(3); // Fill the real FIFO before accepting SET_CREATE.
			const GDExtensionProductionSetBindingV1 expected = { Enrollment::ABI, 0, 0, 0, 1, 0, test.upload_prefix ? 128u : 16u };
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
			test.check(__LINE__, request.uses[request.resources[2].first_binding].binding.resource_slot == 1 && request.uses[request.resources[2].first_binding].binding.bytes == expected.bytes);
			const auto *storage_dependencies = fixture.rd.dependency_map.getptr(request.resources[1].primary);
			const auto *shader_dependencies = fixture.rd.dependency_map.getptr(request.resources[0].secondary);
			test.check(__LINE__, storage_dependencies && storage_dependencies->has(request.resources[2].primary));
			test.check(__LINE__, shader_dependencies && shader_dependencies->has(request.resources[2].primary));
			if (test.record_window) {
				const GDExtensionProductionDispatchV1 dispatch = { Enrollment::ABI, 10, 0, 2, 1, 1, 1, 1, 0, 0 };
				if (test.upload_prefix) {
					uint8_t payload[96];
					for (uint32_t i = 0; i < 96; ++i) {
						payload[i] = uint8_t(i ^ 0x5a);
					}
					const GDExtensionProductionUploadV1 upload = { Enrollment::ABI, 10, 1, 0, 0, 96 };
					test.check(__LINE__, registry.production_prepare_upload(ref, &upload, payload, sizeof(payload)).native_error == OK);
					test.rendezvous(12); // Original RT staging pool, no upload effect yet.
				} else {
					for (uint32_t i = 0; i < request.action_count; ++i) {
						auto prepared = dispatch;
						prepared.canonical_suboperation = 10 + i;
						test.check(__LINE__, registry.production_prepare_dispatch(ref, &prepared, nullptr, 0).native_error == OK);
					}
				}
				test.rendezvous(10); // Saturate the same RS FIFO before the first record turn.
				uint64_t control = 0;
				test.check(__LINE__, registry.production_record_schedule(ref, &control) == ERR_BUSY && request.control.state == ProductionControlState::PENDING && test.receiver_calls == 0);
				test.rendezvous(9); // Stale queue entries cannot execute a merely Pending cell.
				test.check(__LINE__, request.control.state == ProductionControlState::PENDING && test.receiver_calls == 0);
				test.check(__LINE__, registry.production_record_schedule(ref, &control) == OK);
				test.rendezvous(9);
				test.check(__LINE__, test.receiver_calls == 1 && request.control.state == ProductionControlState::RETURNED);
				ProductionControlOutput returned = {};
				test.check(__LINE__, registry.production_control_consume(ref, control, &returned) == OK && returned.result.native_error == OK);
				GDExtensionProductionAppendV1 expired = {};
				test.check(__LINE__, registry.production_append(test.expired_context, ref, 10, request.revision, &expired).native_error == ERR_UNAUTHORIZED);
				test.check(__LINE__, !request.compute_list_open && !fixture.rd.compute_list.active && !request.cancelled);
				if (test.keep_list || test.budget_pause || test.upload_prefix) {
					test.recorded_segment = returned.segment;
				}
				test.check(__LINE__, test.recorded_segment.segment != 0);
				if (test.upload_prefix) {
					test.check(__LINE__, returned.record_pause == ProductionRecordPause::UPLOAD_PREFIX && returned.append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 && returned.append.recorded_bytes == 64 && returned.append.parent_bytes == 96);
					test.check(__LINE__, test.recorded_segment.first_canonical == 10 && test.recorded_segment.end_canonical == 10 && test.recorded_segment.incomplete_upload_parent == 10);
					GDExtensionProductionReportV1 progress = {};
					auto maintenance = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 4, 4, 0 };
					auto prefix_returned = [&]() {
						return request.graph_count == 1 && !request.graphs[0].pending && progress.known_obligations > 0 && progress.complete_obligations == progress.known_obligations && progress.possible_obligations == 0 &&
								(progress.cause.native_error == OK || progress.cause.native_error == ERR_BUSY) && request.control.state == ProductionControlState::OWNER_CONSUMED && fixture.rd._recording_error() == OK &&
								fixture.rd.upload_staging_buffers.blocks.size() == 1 && fixture.rd.frames_drawn > fixture.rd.upload_staging_buffers.blocks[0].frame_used;
					};
					uint32_t prefix_turns = 0;
					for (; prefix_turns < 128 && !prefix_returned(); ++prefix_turns) {
						if (request.control.state == ProductionControlState::OWNER_CONSUMED) {
							maintenance.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
						}
						const Error scheduled = Error(registry.production_observe(ref, &maintenance, &progress).native_error);
						test.check(__LINE__, scheduled == OK || scheduled == ERR_BUSY);
						if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
							test.rendezvous(6); // Continue only through the same original RS FIFO.
							if (request.control.state == ProductionControlState::RETURNED) {
								test.check(__LINE__, request.control.output.service_usage.receipts <= 4 && request.control.output.service_usage.passes <= 4);
							}
							const Error consumed = Error(registry.production_observe(ref, &maintenance, &progress).native_error);
							test.check(__LINE__, consumed == OK || consumed == ERR_BUSY);
						}
					}
					INFO("Upload prefix turns=", prefix_turns, "/128 known=", progress.known_obligations, " complete=", progress.complete_obligations, " possible=", progress.possible_obligations, " cause=", progress.cause.native_error, " cell=", uint32_t(request.control.state), " recording error=", int(fixture.rd._recording_error()), " cursor active=", fixture.rd.native_disposal_active.load(), " phase=", fixture.rd.native_disposal_continuation.phase, " frames=", fixture.rd.frames_drawn, " staging blocks=", fixture.rd.upload_staging_buffers.blocks.size());
					test.check(__LINE__, prefix_returned());
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
					test.check(__LINE__, registry.production_record_schedule(ref, &control) == OK);
					test.rendezvous(9);
					test.check(__LINE__, registry.production_control_consume(ref, control, &returned) == OK && returned.result.native_error == OK && test.receiver_calls == 2);
					test.recorded_segment = returned.segment;
					test.check(__LINE__, test.recorded_segment.first_canonical == 10 && test.recorded_segment.end_canonical == 11 && test.recorded_segment.incomplete_upload_parent == UINT64_MAX && !request.cancelled);
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
				} else if (test.budget_pause) {
					test.check(__LINE__, returned.record_pause == ProductionRecordPause::APPEND_LIMIT && returned.append.state == GDEXTENSION_PRODUCTION_APPENDED_V1);
					test.check(__LINE__, request.next_action == 64 && test.recorded_segment.end_canonical == 74 && !request.cancelled);
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
					test.check(__LINE__, registry.production_record_schedule(ref, &control) == OK);
					test.rendezvous(9);
					test.check(__LINE__, registry.production_control_consume(ref, control, &returned) == OK && returned.result.native_error == OK && test.receiver_calls == 2);
					test.recorded_segment = returned.segment;
					test.check(__LINE__, request.next_action == 65 && test.recorded_segment.first_canonical == 74 && test.recorded_segment.end_canonical == 75 && !request.cancelled && !request.compute_list_open && !fixture.rd.compute_list.active);
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
				} else if (test.close_before_ack) {
					fixture.frame.leave();
					test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED);
					test.rendezvous(11); // Original unit owner accepts retirement before the actual close seal.
					test.check(__LINE__, fixture.rd.production_finalization_started.load());
					test.check(__LINE__, registry.production_close(fixture.attachment).native_error == ERR_BUSY && request.cancelled);
					{
						MutexLock lock(registry.mutex);
						registry.records[fixture.reference.slot].quiescing = true;
					}
					test.check(__LINE__, registry.production_record_schedule(ref, &control) == ERR_BUSY);
					test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED);
					test.check(__LINE__, registry.records[fixture.reference.slot].active_callbacks == 0);
					auto *manager = GDExtensionManager::get_singleton();
					test.check(__LINE__, manager != nullptr);
					if (manager) {
						manager->frame();
					} // Real Core frame hook, no caller-created RegistrationContext.
					test.check(__LINE__, test.metadata_calls == 1 && test.metadata_returned && !Registry::is_production_metadata_entry());
					test.check(__LINE__, registry.records[fixture.reference.slot].active_callbacks == 0);
					test.check(__LINE__, test.receiver_calls == 1 && request.acknowledged_segment == test.recorded_segment.segment);
				} else {
					test.check(__LINE__, registry.production_ack_segment(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED); // SceneReady is installation, not the normal metadata ACK frame.
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK && test.receiver_calls == 1);
				}
			}

			GDExtensionProductionReportV1 report = {};
			auto budget = GDExtensionProductionServiceBudgetV1{ Enrollment::ABI, OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			auto zero = budget;
			zero.receipts = 0;
			// The strict CPU profile has no hidden preparation issue. Allow the
			// original cursor phases plus four receipts per declared native edge,
			// sixteen per graph end/issue/frame cycle and eight per owned resource.
			// This is a finite test retry bound, never extra per-turn permission.
			const uint64_t record_turn_limit = 64 + uint64_t(request.capacity.native_memberships) * 4 + uint64_t(request.capacity.graph_memberships) * 16 + uint64_t(request.resource_count) * 8;
			uint64_t record_turns = 0;
			if (test.record_window) {
				test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
				auto recorded_complete = [&]() {
					bool pending = false;
					for (uint32_t g = 0; g < request.graph_count; ++g) {
						pending |= request.graphs[g].pending;
					}
					return report.known_obligations > 0 && report.known_obligations == report.complete_obligations && report.possible_obligations == 0 && !pending && request.control.state == ProductionControlState::OWNER_CONSUMED && (report.cause.native_error == OK || report.cause.native_error == ERR_BUSY);
				};
				test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK);
				while (record_turns < record_turn_limit && !recorded_complete()) {
					if (request.control.state == ProductionControlState::OWNER_CONSUMED) {
						budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
					}
					const Error scheduled = Error(registry.production_observe(ref, &budget, &report).native_error);
					test.check(__LINE__, scheduled == OK || scheduled == ERR_BUSY);
					if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
						test.rendezvous(6); // Same original RS FIFO, never inline execution.
						if (request.control.state == ProductionControlState::RETURNED) {
							test.check(__LINE__, request.control.output.service_usage.receipts <= 1 && request.control.output.service_usage.passes <= 1);
						}
						const Error consumed = Error(registry.production_observe(ref, &budget, &report).native_error);
						test.check(__LINE__, consumed == OK || consumed == ERR_BUSY);
					}
					++record_turns;
					// A finished record may auto-retire its SET only after actual
					// native Complete. Busy alone never establishes that boundary.
					if (report.possible_obligations || report.known_obligations != report.complete_obligations) {
						test.check(__LINE__, fixture.driver.set_frees == 0 && fixture.driver.buffer_frees == 0);
					}
				}
				INFO("Record turns=", record_turns, "/", record_turn_limit, " known=", report.known_obligations, " possible=", report.possible_obligations, " complete=", report.complete_obligations, " facts=", report.fact_count, " physical=", report.physical_returns_pending, " cause=", report.cause.native_error, " cell=", uint32_t(request.control.state));
				test.check(__LINE__, recorded_complete());
				test.check(__LINE__, fixture.driver.set_creates == 1 && fixture.driver.buffer_frees == 0);
			} else if (!test.close_before_ack) {
				test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK && fixture.driver.set_creates == 1);
				test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
				test.rendezvous(6);
				const auto first_observed = registry.production_observe(ref, &budget, &report);
				INFO("First observed error=", first_observed.native_error, " cause=", report.cause.native_error, " known=", report.known_obligations, " possible=", report.possible_obligations, " complete=", report.complete_obligations, " facts=", report.fact_count, " physical=", report.physical_returns_pending, " output=", request.control.output.result.native_error, " control=", uint32_t(request.control.state));
				test.check(__LINE__, first_observed.native_error == OK);
				budget.receipts = 2;
				test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
				test.rendezvous(6);
				const auto second_observed = registry.production_observe(ref, &budget, &report);
				INFO("Second observed error=", second_observed.native_error, " cause=", report.cause.native_error, " known=", report.known_obligations, " possible=", report.possible_obligations, " complete=", report.complete_obligations, " facts=", report.fact_count, " physical=", report.physical_returns_pending, " output=", request.control.output.result.native_error, " control=", uint32_t(request.control.state));
				test.check(__LINE__, second_observed.native_error == OK && fixture.driver.set_creates == 1 && fixture.driver.set_frees == 0);
			}

			const GDExtensionStorageRetireRequestV1 retire_origin = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 1, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 retire = {};
			GDExtensionStorageRetireReportV1 retired = {};
			test.check(__LINE__, registry.production_storage_retire(ref, 1, &retire_origin, &retire, &retired).native_error == OK);
			test.check(__LINE__, registry.production_storage_release(ref, retire).native_error == ERR_BUSY && fixture.driver.buffer_frees == 0);
			test.check(__LINE__, registry.production_cancel(ref).native_error == OK);
			budget.receipts = 1;
			const uint64_t retire_turn_limit = test.record_window ? record_turn_limit : 64;
			uint64_t retire_turns = 0;
			for (; retire_turns < retire_turn_limit && !retired.state; ++retire_turns) {
				if (test.record_window && request.control.state == ProductionControlState::OWNER_CONSUMED) {
					budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				}
				test.check(__LINE__, registry.production_storage_observe(ref, retire, &budget, &retired).native_error == ERR_BUSY);
				test.rendezvous(6);
				test.check(__LINE__, request.control.output.service_usage.receipts <= 1 && request.control.output.service_usage.passes <= 1);
				const Error observed = Error(registry.production_storage_observe(ref, retire, &budget, &retired).native_error);
				test.check(__LINE__, observed == OK || observed == ERR_BUSY);
				if (test.record_window) {
					test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
				}
			}
			INFO("Retire turns=", retire_turns, "/", retire_turn_limit, " source turns=", record_turns, " state=", retired.state, " native error=", retired.native_error, " event=", retired.event_seq, " frame=", retired.frame_slot, " epoch=", retired.frame_epoch, " set frees=", fixture.driver.set_frees, " buffer frees=", fixture.driver.buffer_frees, " cell=", uint32_t(request.control.state), " storage uses=", request.resources[1].use_count);
			test.check(__LINE__, retired.state == 1 && fixture.driver.set_frees == 1 && fixture.driver.buffer_frees == 1 && !fixture.driver.set_live);
			if (test.record_window) {
				test.check(__LINE__, retired.event_seq > 0 && request.resources[1].retire_report.event_seq == retired.event_seq && request.resources[2].returned);
			}
			test.check(__LINE__, !fixture.rd.dependency_map.has(RID::from_uint64(backing.rid)));
			test.check(__LINE__, registry.production_storage_release(ref, retire).native_error == OK);
			test.check(__LINE__, drain_request_cursor(fixture, ref, [&]() { test.rendezvous(6); }));
			if (test.record_window) {
				// Complete observation and physical set/storage return do not return
				// the original native observer reference. Each RELEASE turn runs on
				// the original FIFO, then its owner consumes at most one receipt.
				test.check(__LINE__, registry.production_release(ref).native_error == ERR_BUSY);
				const uint64_t release_turn_limit = uint64_t(request.native_count) + 1;
				uint64_t release_turns = 0;
				for (; release_turns < release_turn_limit && request.serial; ++release_turns) {
					test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
					budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
					test.check(__LINE__, registry.production_control_accept(ref, ProductionControlKind::RELEASE, &test.release_control, &budget) == OK);
					test.check(__LINE__, registry.production_control_schedule(ref, test.release_control) == OK);
					test.rendezvous(6);
					if (test.close_before_ack) {
						test.metadata_release = true;
						GDExtensionManager::get_singleton()->frame();
						test.check(__LINE__, !Registry::is_production_metadata_entry());
					} else {
						test.check(__LINE__, registry.production_control_consume(ref, test.release_control, &test.release_output) == OK);
					}
					test.check(__LINE__, test.release_output.result.native_error == OK);
					test.check(__LINE__, test.release_output.service_usage.passes <= 1 && test.release_output.service_usage.receipts <= 1);
					test.check(__LINE__, test.release_output.owner_usage.passes <= 1 && test.release_output.owner_usage.receipts <= 1);
					test.check(__LINE__, test.release_output.owner_return.native_error == OK || test.release_output.owner_return.native_error == ERR_BUSY);
					if (test.release_output.owner_return.native_error == ERR_BUSY) {
						test.check(__LINE__, request.serial == ref.handle_serial && request.control.state == ProductionControlState::OWNER_CONSUMED);
					}
				}
				INFO("Release turns=", release_turns, "/", release_turn_limit, " owner return=", test.release_output.owner_return.native_error, " retains=", fixture.driver.retains, " releases=", fixture.driver.releases);
				test.check(__LINE__, test.release_output.owner_return.native_error == OK && request.serial == 0);
			} else {
				test.check(__LINE__, registry.production_release(ref).native_error == OK);
			}
			if (test.record_window) {
				test.check(__LINE__, fixture.driver.retains > 0 && fixture.driver.retains == fixture.driver.releases);
			}
			if (!test.close_before_ack) {
				test.check(__LINE__, fixture.driver.qualification_legacy_end_segments == 0);
			}
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
	static Error create_split_unit(SingletonEnrollment &fixture, GDExtensionNativeUnitBackingV1 &unit, uint32_t p_storage_bytes) {
		Ref<UnitContainer> container;
		container.instantiate();
		container->initialize(p_storage_bytes);
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
		if (!check_step(binding.binding == 0 && binding.type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER && binding.length == p_storage_bytes && binding.writable && reflection.compute_local_size[0] == 1 && reflection.compute_local_size[1] == 1 && reflection.compute_local_size[2] == 1)) {
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
	static void singleton_split_owner_set(bool record_window = false, bool keep_list = false, bool close_before_ack = false, bool budget_pause = false, bool upload_prefix = false) {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		SplitSet test;
		test.record_window = record_window;
		test.keep_list = keep_list;
		test.close_before_ack = close_before_ack;
		test.budget_pause = budget_pause;
		test.upload_prefix = upload_prefix;
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
				if (!test.record_window) {
					fixture.rd.frames[0].qualification_epoch = 1;
				}
				fixture.rd.frames[0].fence = RDD::FenceID(93);
				fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
				fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
				fixture.rd.main_queue = RDD::CommandQueueID(1);
				if (test.record_window) {
					CHECK(fixture.rd._begin_frame(false, true) == OK);
				}
				test.unit_created = create_split_unit(fixture, test.unit, test.upload_prefix ? 128u : 16u);
			} else if (test.phase == 3 || test.phase == 10) {
				uint32_t filled = 0;
				while (filled < (1u << 20) && server->_try_production_control_dispatch(0, 0, 0, 0)) {
					++filled;
				}
				CHECK((filled > 0 && filled < (1u << 20)));
			} else if (test.phase == 12) {
				fixture.rd.upload_staging_buffers.block_size = fixture.rd.upload_staging_buffers.max_size = 64;
				fixture.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
				CHECK(fixture.rd._insert_staging_block(fixture.rd.upload_staging_buffers) == OK);
			} else if (test.phase == 11) {
				QuoteDriver::QualificationCleanupScope cleanup_scope(fixture.driver);
				const GDExtensionNativeUnitRetireRequestV1 retire = { Enrollment::ABI, test.unit.unit, 92, 0 };
				GDExtensionNativeUnitRetireReportV1 report = {};
				CHECK(fixture.registry.qualification_native_unit_retire(fixture.qualification, &retire, &test.unit_retired, &report).native_error == OK);
				CHECK(test.unit_retired.handle_serial != 0);
				CHECK(fixture.rd._begin_production_finalization() == ERR_BUSY);
			} else if (test.phase == 7) {
				QuoteDriver::QualificationCleanupScope cleanup_scope(fixture.driver);
				const GDExtensionNativeUnitRetireRequestV1 retire = { Enrollment::ABI, test.unit.unit, 92, 0 };
				GDExtensionNativeUnitRetireReportV1 report = {};
				if (!test.close_before_ack) {
					CHECK(fixture.registry.qualification_native_unit_retire(fixture.qualification, &retire, &test.unit_retired, &report).native_error == OK);
				}
				fixture.rd._free_pending_resources(0);
				if (test.upload_prefix) {
					CHECK(fixture.driver.upload_copies == 2);
					for (uint32_t i = 0; i < 96; ++i) {
						CHECK(fixture.driver.upload_memory[1][i] == uint8_t(i ^ 0x5a));
					}
					for (auto &block : fixture.rd.upload_staging_buffers.blocks) {
						fixture.driver.buffer_unmap(block.driver_id);
						fixture.driver.buffer_free(block.driver_id);
					}
					fixture.rd.upload_staging_buffers.blocks.clear();
					CHECK_FALSE(fixture.driver.staging_live);
					CHECK(fixture.driver.buffer_frees == 2);
				}
				if (!test.close_before_ack) {
					CHECK(fixture.registry.qualification_native_unit_retire(fixture.qualification, &retire, &test.unit_retired, &report).native_error == OK);
				}

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
		if (test.close_before_ack) {
			CHECK(test.final_legacy_end_segments > 0);
		} else {
			CHECK(test.final_legacy_end_segments == 0);
		}
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
	struct MetadataCallProbe {
		SingletonEnrollment *fixture = nullptr;
		uint32_t calls = 0;
		Callable existing;
		Signal cached_signal;
		GDExtensionPtrBuiltInMethod cached_signal_connect = nullptr;
		Error connect_cached_signal() {
			CHECK(cached_signal_connect != nullptr);
			if (!cached_signal_connect) {
				return ERR_UNCONFIGURED;
			}
			const int64_t flags = Object::CONNECT_DEFERRED;
			const GDExtensionConstTypePtr arguments[] = { &existing, &flags };
			int64_t returned = OK;
			cached_signal_connect(&cached_signal, arguments, &returned, 2);
			return Error(returned);
		}
		GDExtensionDeviceExit::GuardedLibraryRef previous_library = {};
		uint32_t nested_calls = 0, callable_frees = 0;
		static void callable_free(void *opaque) { ++static_cast<MetadataCallProbe *>(opaque)->callable_frees; }
		static void nested_call(void *opaque, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr returned, GDExtensionCallError *error) {
			auto &probe = *static_cast<MetadataCallProbe *>(opaque);
			auto &fixture = *probe.fixture;
			using namespace GDExtensionDeviceExit;
			++probe.nested_calls;
			CHECK(Registry::is_production_metadata_entry());
			CHECK(RegistrationContext::current->registration_allowed);
			const auto private_refs = fixture.registry.records[fixture.reference.slot].private_refs;
			const auto cleanup_refs = fixture.registry.records[fixture.reference.slot].cleanup_reservations;
			GDExtensionCallableCustomInfo2 info = {};
			info.callable_userdata = &probe;
			info.call_func = &nested_call;
			info.token = fixture.registry.records[fixture.reference.slot].original_owner;
			const auto create = reinterpret_cast<GDExtensionInterfaceCallableCustomCreate2>(GDExtension::get_interface_function("callable_custom_create2"));
			Callable rejected;
			CHECK(create != nullptr);
			if (create) {
				ERR_PRINT_OFF;
				create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&rejected), &info);
				ERR_PRINT_ON;
			}
			CHECK(rejected.is_null());
			RecordRef rejected_binding;
			CHECK(fixture.registry.retain_binding_owner(info.token, rejected_binding) == ERR_UNAUTHORIZED);
			CHECK_FALSE(rejected_binding.is_valid());
			CHECK(fixture.registry.records[fixture.reference.slot].private_refs == private_refs);
			CHECK(fixture.registry.records[fixture.reference.slot].cleanup_reservations == cleanup_refs);
			*reinterpret_cast<Variant *>(returned) = Variant();
			*error = { GDEXTENSION_CALL_OK, 0, 0 };
		}
		static void unused_record(void *, GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) { FAIL_CHECK("Metadata probe unexpectedly acquired an RT record invocation"); }
		static void owner(void *opaque, GDExtensionProductionAttachmentRefV1 attachment) {
			auto &probe = *static_cast<MetadataCallProbe *>(opaque);
			auto &fixture = *probe.fixture;
			using namespace GDExtensionDeviceExit;
			++probe.calls;
			CHECK(Registry::is_production_metadata_entry());
			CHECK(Thread::get_caller_id() == fixture.rd.render_thread_id);
			CHECK(fixture.registry.records[fixture.reference.slot].active_callbacks == 1);
			if (probe.previous_library.handle_serial) {
				MutexLock lock(fixture.registry.mutex);
				Registry::View *expired = nullptr;
				CHECK(fixture.registry.resolve_borrow_locked(probe.previous_library.registry_instance, probe.previous_library.handle_serial, Registry::ViewDomain::LIBRARY, expired) == ERR_INVALID_DATA);
			}
			GuardedLibraryRef fresh_library = {};
			const GDExtensionDeviceExitAbi library_abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
			CHECK(fixture.registry.borrow_guarded_library(&library_abi, fixture.registry.records[fixture.reference.slot].original_owner, &fresh_library) == OK);
			CHECK(fresh_library.handle_serial != probe.previous_library.handle_serial);
			probe.previous_library = fresh_library;
			const auto clone_hold = reinterpret_cast<GDExtensionInterfaceDeviceExitCloneCodeHoldV1>(GDExtension::get_interface_function("device_exit_clone_code_hold_v1"));
			const auto &record_before_clone = fixture.registry.records[fixture.reference.slot];
			const auto private_refs_before_clone = record_before_clone.private_refs;
			const auto holders_before_clone = record_before_clone.binding_holders;
			const auto holder_serial_before_clone = fixture.registry.holder_serials.next;
			const auto serial_before_clone = fixture.registry.serials.next;
			GDExtensionDeviceExitLibraryCodeHoldRef rejected_hold = {};
			CHECK(clone_hold != nullptr);
			if (clone_hold) {
				CHECK(clone_hold(&library_abi, { fixture.instance, fixture.holder.handle_serial }, &rejected_hold) == ERR_UNAUTHORIZED);
			}
			CHECK(rejected_hold.handle_serial == 0);
			CHECK(record_before_clone.private_refs == private_refs_before_clone);
			CHECK(record_before_clone.binding_holders == holders_before_clone);
			CHECK(fixture.registry.holder_serials.next == holder_serial_before_clone);
			CHECK(fixture.registry.serials.next == serial_before_clone);
			if (probe.calls == 1) {
				CHECK(probe.cached_signal.is_connected(probe.existing));
				probe.cached_signal.disconnect(probe.existing); // Existing connection cleanup stays legal.
				CHECK_FALSE(probe.cached_signal.is_connected(probe.existing));
				CHECK(probe.connect_cached_signal() == ERR_UNAUTHORIZED);
				CHECK_FALSE(probe.cached_signal.is_connected(probe.existing));
				if (probe.cached_signal.is_connected(probe.existing)) {
					probe.cached_signal.disconnect(probe.existing); // Failed-case cleanup after the actual connection assertion.
				}
				Callable::CallError nested_error;
				Variant returned;
				probe.existing.callp(nullptr, 0, returned, nested_error);
				CHECK(nested_error.error == Callable::CallError::CALL_OK);
				CHECK(probe.nested_calls == 1);
				probe.existing = Callable(); // Original registered cleanup stays legal inside metadata.
				CHECK(probe.callable_frees == 1);
			}
			const auto graph_before = fixture.rd.draw_graph.qualification_command_count();
			const auto allocations = fixture.driver.allocations;
			const auto ref = GDExtensionProductionRequestRefV1{ fixture.instance, fixture.request->serial };
			uint64_t control = 0;
			CHECK(fixture.registry.production_record_schedule(ref, &control) == ERR_UNAUTHORIZED);
			const StringName compute_name("compute_list_begin");
			Callable::CallError error;
			const Variant result = fixture.rd.callp(compute_name, nullptr, 0, error);
			CHECK(error.error == Callable::CallError::CALL_ERROR_INVALID_METHOD);
			CHECK(result.get_type() == Variant::NIL);
			const auto call = reinterpret_cast<GDExtensionInterfaceObjectMethodBindCall>(GDExtension::get_interface_function("object_method_bind_call"));
			const auto ptrcall = reinterpret_cast<GDExtensionInterfaceObjectMethodBindPtrcall>(GDExtension::get_interface_function("object_method_bind_ptrcall"));
			auto *method = ClassDB::get_method("RenderingDevice", compute_name);
			CHECK(call != nullptr);
			CHECK(ptrcall != nullptr);
			CHECK(method != nullptr);
			if (call && ptrcall && method) {
				Variant returned;
				GDExtensionCallError refused = {};
				call(method, &fixture.rd, nullptr, 0, reinterpret_cast<GDExtensionUninitializedVariantPtr>(&returned), &refused);
				CHECK(refused.error == GDEXTENSION_CALL_ERROR_INVALID_METHOD);
				int64_t list = 99;
				ptrcall(method, &fixture.rd, nullptr, &list);
				CHECK(list == int64_t(RenderingDevice::INVALID_ID));
				CallbackScope nested(fixture.reference);
				CHECK(nested.get_error() == OK);
				CHECK(Registry::is_production_metadata_entry());
				const auto private_refs = fixture.registry.records[fixture.reference.slot].private_refs;
				RecordRef rejected_owner;
				CHECK(fixture.registry.check_entry_classes(fixture.reference, 1u << 6) == ERR_UNAUTHORIZED);
				CHECK(fixture.registry.check_current_entry_classes(1u << 8) == ERR_UNAUTHORIZED);
				CHECK(fixture.registry.authenticate_registration(fixture.reference) == ERR_UNAUTHORIZED);
				CHECK(fixture.registry.retain_binding_owner(nullptr, rejected_owner) == ERR_UNAUTHORIZED);
				CHECK(fixture.registry.retain_callable_owner(nullptr, rejected_owner) == ERR_UNAUTHORIZED);
				CHECK_FALSE(rejected_owner.is_valid());
				CHECK(fixture.registry.records[fixture.reference.slot].private_refs == private_refs);
				ptrcall(method, &fixture.rd, nullptr, &list);
				CHECK(list == int64_t(RenderingDevice::INVALID_ID));
			}
			ERR_PRINT_OFF;
			CHECK(ClassDB::instantiate("RenderingDevice") == nullptr);
			ERR_PRINT_ON;
			CHECK(MessageQueue::get_singleton()->push_callp(&fixture.rd, compute_name, nullptr, 0) == ERR_UNAUTHORIZED);
			const Callable deferred(&fixture.rd, compute_name);
			const Variant callable(deferred);
			Variant ignored;
			Callable::CallError deferred_error;
			const Variant **no_arguments = nullptr;
			const_cast<Variant &>(callable).callp("call_deferred", no_arguments, 0, ignored, deferred_error);
			CHECK_FALSE(fixture.rd.compute_list.active);
			CHECK(fixture.rd.draw_graph.qualification_command_count() == graph_before);
			CHECK(fixture.driver.allocations == allocations);
			CHECK(fixture.registry.production_close(attachment).native_error == ERR_BUSY);
		}
	};
	static void singleton_metadata_scope() {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		SingletonEnrollment fixture;
		if (!check_step(fixture.prepare_device(server) == OK)) {
			return;
		}
		MetadataCallProbe probe;
		probe.fixture = &fixture;
		if (!check_step(fixture.initialize_owner(1, false, true, &MetadataCallProbe::unused_record, &probe, false, 1, false, &MetadataCallProbe::owner) == OK)) {
			return;
		}
		{
			MutexLock lock(fixture.registry.mutex);
			fixture.registry.records[fixture.reference.slot].guard.entry_classes |= 1u << 8;
		}
		GDExtensionCallableCustomInfo2 info = {};
		info.callable_userdata = &probe;
		info.call_func = &MetadataCallProbe::nested_call;
		info.free_func = &MetadataCallProbe::callable_free;
		info.token = fixture.registry.records[fixture.reference.slot].original_owner;
		const auto create = reinterpret_cast<GDExtensionInterfaceCallableCustomCreate2>(GDExtension::get_interface_function("callable_custom_create2"));
		CHECK(create != nullptr);
		if (create) {
			create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&probe.existing), &info);
		}
		CHECK(probe.existing.is_custom());
		probe.cached_signal = Signal(&fixture.rd, "property_list_changed");
		const auto builtin = reinterpret_cast<GDExtensionInterfaceVariantGetPtrBuiltinMethod>(GDExtension::get_interface_function("variant_get_ptr_builtin_method"));
		CHECK(builtin != nullptr);
		if (builtin) {
			const StringName connect_name("connect");
			probe.cached_signal_connect = builtin(GDEXTENSION_VARIANT_TYPE_SIGNAL, &connect_name, Variant::get_builtin_method_hash(Variant::SIGNAL, connect_name));
		}
		CHECK(probe.connect_cached_signal() == OK); // Same real cached builtin works outside metadata.
		CHECK(probe.cached_signal.is_connected(probe.existing));
		fixture.frame.leave(); // No SceneReady or pre-admitted normal frame survives.
		auto *manager = GDExtensionManager::get_singleton();
		if (!check_step(manager != nullptr)) {
			return;
		}
		manager->frame();
		CHECK(probe.calls == 1);
		manager->frame(); // Same reserved metadata frame ID, fresh invocation and borrowed view.
		CHECK(probe.calls == 2);
		{
			MutexLock lock(fixture.registry.mutex);
			for (const auto &view : fixture.registry.views) {
				CHECK_FALSE(view.serial == probe.previous_library.handle_serial);
			}
		}
		CHECK_FALSE(GDExtensionDeviceExit::Registry::is_production_metadata_entry());
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(fixture.rd.compute_list.active);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
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
		CHECK(record.cleanup_reservations == 3);
		// Two request credits plus the accepted metadata callback credit.
		CHECK(record.cleanup_by_phase[uint8_t(GDExtensionDeviceExit::CleanupPhase::BINDING)] == 3);
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
	CHECK(quote.cleanup_slots == 3);
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

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] protected original FIFO frame issues and expires physical record context") {
	TestProductionRequest::Access::singleton_split_owner_set(true);
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] KEEP scope return closes prefix and owner ACK survives actual seal and close") {
	TestProductionRequest::Access::singleton_split_owner_set(true, true, true);
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] actual 64 append frame pause preserves original suffix without cancellation") {
	TestProductionRequest::Access::singleton_split_owner_set(true, false, false, true);
}
#endif

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] protected upload KnownPrefix returns exact same-parent suffix in later FIFO frame") {
	TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, true);
}
#endif

TEST_CASE("[SceneTree][ProductionRequest] fresh accepted metadata frame rejects ordinary ClassDB record and deferred escape") {
	TestProductionRequest::Access::singleton_metadata_scope();
}
