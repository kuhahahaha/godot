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
#include "core/extension/gdextension_external_output_typed.h"
#include "core/extension/gdextension_manager.h"
#include "core/extension/gdextension_production_request.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_shader_container.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstring>

extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *);
extern GDExtensionInterfaceFunctionPtr gdextension_get_guarded_proc_address(const char *);

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
		void initialize_raster() {
			reflection_data.pipeline_type = RenderingDeviceCommons::PIPELINE_TYPE_RASTERIZATION;
			reflection_data.stage_count = 2;
			for (auto stage : { RenderingDeviceCommons::SHADER_STAGE_VERTEX, RenderingDeviceCommons::SHADER_STAGE_FRAGMENT }) {
				reflection_shader_stages.push_back(stage);
				Shader shader;
				shader.shader_stage = stage;
				shader.code_compressed_bytes.push_back(42);
				shader.code_decompressed_size = 1;
				shaders.push_back(shader);
			}
		}
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
		uint32_t fence_polls = 0, fence_recycles = 0, native_recycles = 0;
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
			if (directory_pending_polls) {
				--directory_pending_polls;
				return { false, false, 0, 0, IssueFact::KnownAccepted };
			}
			return { true, false, serial + 1000, 0, IssueFact::KnownAccepted };
		}
		uint32_t release_attempts = 0, release_busy_remaining = 0, release_error_remaining = 0;
		Error native_call_release(uint64_t serial) override {
			CHECK((serial > 0 && serial < 128));
			++release_attempts;
			if (release_busy_remaining) {
				--release_busy_remaining;
				return ERR_BUSY;
			}
			if (release_error_remaining) {
				--release_error_remaining;
				return FAILED;
			}
			++releases;
			return OK;
		}
		bool output_storage_factory = false;
		uint32_t output_storage_attempts = 0, output_storage_count = 0, output_storage_frees = 0, output_storage_fail_at = 0;
		uint64_t output_storage_bytes[7] = {};
		bool output_storage_live[7] = {};
		bool output_storage_indirect[7] = {};
		uint32_t allocations = 0, buffer_frees = 0, directory_pending_polls = 0;
		bool buffer_live = false;
		bool upload_mode = false, staging_live = false;
		bool auxiliary_factory = false, auxiliary_buffer_live = false, auxiliary_set_live = false;
		bool mesh_directory_factory = false, mesh_directory_buffer_live = false;
		bool mesh_directory_indexed144 = false;
		uint32_t mesh_directory_buffer_frees = 0, mesh_directory_buffer_count = 0;
		bool mesh_directory_buffers_live[4] = {};
		VertexFormatID vertex_format_create(Span<VertexAttribute>, const VertexAttributeBindingsMap &) override { return VertexFormatID(40); }
		uint32_t auxiliary_buffer_frees = 0, auxiliary_set_frees = 0, auxiliary_shader_frees = 0, auxiliary_pipeline_frees = 0;
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
			if (output_storage_factory) {
				CHECK(allocation == MEMORY_ALLOCATION_TYPE_GPU);
				CHECK(usage.has_flag(BUFFER_USAGE_STORAGE_BIT));
				++output_storage_attempts;
				if (output_storage_fail_at == output_storage_attempts) {
					return {};
				}
				REQUIRE(output_storage_count < 7);
				const uint32_t index = output_storage_count++;
				output_storage_bytes[index] = bytes;
				output_storage_indirect[index] = usage.has_flag(BUFFER_USAGE_INDIRECT_BIT);
				output_storage_live[index] = true;
				return BufferID(300 + index);
			}
			if (mesh_directory_factory) {
				CHECK((mesh_directory_indexed144 ? ((mesh_directory_buffer_count < 2 && bytes == 432 && usage.has_flag(BUFFER_USAGE_VERTEX_BIT)) || (mesh_directory_buffer_count == 2 && bytes == 12 && usage.has_flag(BUFFER_USAGE_INDEX_BIT))) : ((bytes == 36 && usage.has_flag(BUFFER_USAGE_VERTEX_BIT)) || (bytes == 12 && usage.has_flag(BUFFER_USAGE_INDEX_BIT)))));
				CHECK((allocation == MEMORY_ALLOCATION_TYPE_GPU && mesh_directory_buffer_count < 4));
				const uint32_t index = mesh_directory_buffer_count++;
				mesh_directory_buffers_live[index] = true;
				mesh_directory_buffer_live = true;
				return BufferID(40 + index);
			}
			if (auxiliary_factory) {
				CHECK((bytes == 16 && allocation == MEMORY_ALLOCATION_TYPE_GPU && usage.has_flag(BUFFER_USAGE_STORAGE_BIT) && !auxiliary_buffer_live));
				auxiliary_buffer_live = true;
				return BufferID(3);
			}
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
			if (id.id >= 300 && id.id < 307) {
				const uint32_t index = uint32_t(id.id - 300);
				CHECK(output_storage_live[index]);
				output_storage_live[index] = false;
				++output_storage_frees;
				return;
			}
			if (id.id >= 40 && id.id <= 43) {
				const uint32_t index = uint32_t(id.id - 40);
				CHECK(mesh_directory_buffers_live[index]);
				mesh_directory_buffers_live[index] = false;
				mesh_directory_buffer_live = false;
				for (uint32_t i = 0; i < 4; ++i) {
					mesh_directory_buffer_live |= mesh_directory_buffers_live[i];
				}
				++mesh_directory_buffer_frees;
				return;
			}
			if (id == BufferID(3)) {
				CHECK((auxiliary_buffer_live && !auxiliary_set_live));
				auxiliary_buffer_live = false;
				++auxiliary_buffer_frees;
				return;
			}
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
		bool raster_fail = false, raster_quiet = false, raster_live = false;
		RenderingDevice *raster_device = nullptr;
		uint32_t raster_budgeted_frees = 0;
		uint32_t raster_attempts = 0, raster_frees = 0, raster_shader_frees = 0;
		DriverWorkarounds get_driver_workarounds() const override {
			DriverWorkarounds value;
			value.dont_print_on_render_pipeline_creation_failure = raster_quiet;
			return value;
		}
		RenderPassID render_pass_create(VectorView<Attachment>, VectorView<Subpass>, VectorView<SubpassDependency>, uint32_t, AttachmentReference) override { return RenderPassID(280); }
		void render_pass_free(RenderPassID id) override { CHECK(id == RenderPassID(280)); }
		PipelineID render_pipeline_create(ShaderID shader, VertexFormatID, RenderPrimitive, PipelineRasterizationState, PipelineMultisampleState, PipelineDepthStencilState, PipelineColorBlendState, VectorView<int32_t>, BitField<PipelineDynamicStateFlags>, RenderPassID pass, uint32_t, VectorView<PipelineSpecializationConstant>) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(shader == ShaderID(280));
			CHECK(pass == RenderPassID(280));
			++raster_attempts;
			if (raster_fail) {
				return {};
			}
			CHECK_FALSE(raster_live);
			raster_live = true;
			return PipelineID(281);
		}
		const RenderingShaderContainerFormat &get_shader_container_format() const override { return format; }
		ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &container, const Vector<ImmutableSampler> &) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			const auto reflection = container->get_shader_reflection();
			if (reflection.pipeline_type == PIPELINE_TYPE_RASTERIZATION) {
				CHECK(reflection.uniform_sets.is_empty());
				return ShaderID(280);
			}
			CHECK(reflection.pipeline_type == PIPELINE_TYPE_COMPUTE);
			CHECK(uint64_t(container->get_compile_flags()) == 0);
			CHECK(reflection.uniform_sets.size() == 1);
			if (reflection.uniform_sets.size() != 1 || reflection.uniform_sets[0].size() != 1) {
				FAIL_CHECK("Unexpected original unit reflection in SET fixture");
				return {};
			}
			const auto &binding = reflection.uniform_sets[0][0];
			CHECK((binding.binding == 0 && binding.type == UNIFORM_TYPE_STORAGE_BUFFER && binding.length == (upload_mode ? 128u : 16u) && binding.writable));
			if (auxiliary_factory) {
				return ShaderID(180);
			}
			++shader_creates;
			return ShaderID(80);
		}
		PipelineID compute_pipeline_create(ShaderID shader, VectorView<PipelineSpecializationConstant>) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			if (shader == ShaderID(180)) {
				return PipelineID(181);
			}
			CHECK(shader == ShaderID(80));
			++pipeline_creates;
			return PipelineID(81);
		}
		uint32_t shader_get_layout_hash(ShaderID id) override {
			CHECK((id == ShaderID(80) || id == ShaderID(180) || id == ShaderID(280)));
			return 123;
		}
		void shader_free(ShaderID id) override {
			if (id == ShaderID(280)) {
				++raster_shader_frees;
				return;
			}
			if (id == ShaderID(180)) {
				++auxiliary_shader_frees;
				return;
			}
			CHECK(id == ShaderID(80));
			++shader_frees;
		}
		void pipeline_free(PipelineID id) override {
			if (id == PipelineID(281)) {
				REQUIRE(raster_device != nullptr);
				REQUIRE(raster_device->native_disposal_budget != nullptr);
				REQUIRE(raster_device->native_disposal_usage != nullptr);
				CHECK(raster_device->native_disposal_budget->passes == 1);
				CHECK(raster_device->native_disposal_usage->passes <= 1);
				++raster_budgeted_frees;
				CHECK(raster_live);
				raster_live = false;
				++raster_frees;
				return;
			}
			if (id == PipelineID(181)) {
				++auxiliary_pipeline_frees;
				return;
			}
			CHECK(id == PipelineID(81));
			++pipeline_frees;
		}
		void (*disposal_probe)(void *) = nullptr;
		void *disposal_probe_data = nullptr;
		bool shader_retire_ready_nonblocking(ShaderID) const override {
			if (disposal_probe) {
				disposal_probe(disposal_probe_data);
			}
			return true;
		}
		UniformSetID uniform_set_create(VectorView<BoundUniform> uniforms, ShaderID shader, uint32_t set, int pool) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			if (shader == ShaderID(180)) {
				CHECK((set == 0 && pool == -1 && !auxiliary_set_live && uniforms.size() == 1 && uniforms[0].ids.size() == 1 && uniforms[0].ids[0].id == 3));
				auxiliary_set_live = true;
				return UniformSetID(182);
			}
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
			if (id == UniformSetID(182)) {
				CHECK(auxiliary_set_live);
				auxiliary_set_live = false;
				++auxiliary_set_frees;
				return;
			}
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
			++fence_polls;
			CHECK(id == FenceID(93));
			return { true, false, serial, 0 };
		}
		Error fence_recycle(FenceID id, uint64_t) override {
			++fence_recycles;
			CHECK(id == FenceID(93));
			return OK;
		}
		Error native_call_recycle(uint64_t serial) override {
			++native_recycles;
			CHECK(serial > 0);
			return OK;
		}
		uint32_t observed_disposal_receipts() const {
			return issues + polls + retains + release_attempts + fence_polls + fence_recycles + native_recycles + mesh_directory_buffer_frees;
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
			const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), nonce, 1, 10, 11 };
			const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
			const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 16, 0 };
			const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
			return registry->production_reserve_metadata_locked(*attachment, operation, capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, out);
		}
	};

	// Same live singleton and original FIFO used by the engine. Only the newly
	// reserved record, device and views belong to this controlled CPU fixture.
	struct PublicProductionPort {
		GDExtensionInterfaceDeviceSessionProductionQuoteV1 quote = nullptr;
		GDExtensionInterfaceDeviceSessionProductionAcceptV1 accept = nullptr;
		GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1 install_record_receiver = nullptr;
		GDExtensionInterfaceDeviceSessionProductionReserveV1 reserve = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1 record_schedule = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRecordConsumeV1 record_consume = nullptr;
		GDExtensionInterfaceDeviceSessionProductionAppendV1 append = nullptr;
		GDExtensionInterfaceDeviceSessionProductionCloseSegmentV1 close_segment = nullptr;
		GDExtensionInterfaceDeviceSessionProductionAckSegmentV1 ack_segment = nullptr;
		GDExtensionInterfaceDeviceSessionProductionOpenV1 open = nullptr;
		GDExtensionInterfaceDeviceSessionProductionSetCreateV1 set_create = nullptr;
		GDExtensionInterfaceDeviceSessionProductionStorageCreateV1 storage_create = nullptr;
		GDExtensionInterfaceDeviceSessionProductionStorageRetireV1 storage_retire = nullptr;
		GDExtensionInterfaceDeviceSessionProductionStorageObserveV1 storage_observe = nullptr;
		GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1 storage_release = nullptr;
		GDExtensionInterfaceDeviceSessionProductionImportUnitV1 import_unit = nullptr;
		GDExtensionInterfaceDeviceSessionProductionImportStorageV1 import_storage = nullptr;
		GDExtensionInterfaceDeviceSessionProductionImportSetV1 import_set = nullptr;
		GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV1 prepare_dispatch = nullptr;
		GDExtensionInterfaceDeviceSessionProductionPrepareCopyV1 prepare_copy = nullptr;
		GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1 prepare_upload = nullptr;
		GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV1 prepare_readback = nullptr;
		GDExtensionInterfaceDeviceSessionProductionObserveV1 observe = nullptr;
		GDExtensionInterfaceDeviceSessionProductionFactsV1 facts = nullptr;
		GDExtensionInterfaceDeviceSessionProductionCopyV1 copy = nullptr;
		GDExtensionInterfaceDeviceSessionProductionCancelV1 cancel = nullptr;
		GDExtensionInterfaceDeviceSessionProductionReleaseV1 release = nullptr;
		GDExtensionInterfaceDeviceSessionProductionCloseV1 close = nullptr;
		GDExtensionInterfaceDeviceSessionProductionInstallationObserveV1 installation_observe = nullptr;
		bool load() {
			bool available = true;
			quote = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionQuoteV1>(GDExtension::get_interface_function("device_session_production_quote_v1"));
			CHECK_MESSAGE(quote != nullptr, "Missing installed production procedure: device_session_production_quote_v1");
			available &= quote != nullptr;
			accept = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionAcceptV1>(GDExtension::get_interface_function("device_session_production_accept_v1"));
			CHECK_MESSAGE(accept != nullptr, "Missing installed production procedure: device_session_production_accept_v1");
			available &= accept != nullptr;
			install_record_receiver = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1>(GDExtension::get_interface_function("device_session_production_install_record_receiver_v1"));
			CHECK_MESSAGE(install_record_receiver != nullptr, "Missing installed production procedure: device_session_production_install_record_receiver_v1");
			available &= install_record_receiver != nullptr;
			reserve = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionReserveV1>(GDExtension::get_interface_function("device_session_production_reserve_v1"));
			CHECK_MESSAGE(reserve != nullptr, "Missing installed production procedure: device_session_production_reserve_v1");
			available &= reserve != nullptr;
			record_schedule = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>(GDExtension::get_interface_function("device_session_production_record_schedule_v1"));
			CHECK_MESSAGE(record_schedule != nullptr, "Missing installed production procedure: device_session_production_record_schedule_v1");
			available &= record_schedule != nullptr;
			record_consume = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV1>(GDExtension::get_interface_function("device_session_production_record_consume_v1"));
			CHECK_MESSAGE(record_consume != nullptr, "Missing installed production procedure: device_session_production_record_consume_v1");
			available &= record_consume != nullptr;
			append = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionAppendV1>(GDExtension::get_interface_function("device_session_production_append_v1"));
			CHECK_MESSAGE(append != nullptr, "Missing installed production procedure: device_session_production_append_v1");
			available &= append != nullptr;
			close_segment = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionCloseSegmentV1>(GDExtension::get_interface_function("device_session_production_close_segment_v1"));
			CHECK_MESSAGE(close_segment != nullptr, "Missing installed production procedure: device_session_production_close_segment_v1");
			available &= close_segment != nullptr;
			ack_segment = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>(GDExtension::get_interface_function("device_session_production_ack_segment_v1"));
			CHECK_MESSAGE(ack_segment != nullptr, "Missing installed production procedure: device_session_production_ack_segment_v1");
			available &= ack_segment != nullptr;
			open = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionOpenV1>(GDExtension::get_interface_function("device_session_production_open_v1"));
			CHECK_MESSAGE(open != nullptr, "Missing installed production procedure: device_session_production_open_v1");
			available &= open != nullptr;
			set_create = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionSetCreateV1>(GDExtension::get_interface_function("device_session_production_set_create_v1"));
			CHECK_MESSAGE(set_create != nullptr, "Missing installed production procedure: device_session_production_set_create_v1");
			available &= set_create != nullptr;
			storage_create = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionStorageCreateV1>(GDExtension::get_interface_function("device_session_production_storage_create_v1"));
			CHECK_MESSAGE(storage_create != nullptr, "Missing installed production procedure: device_session_production_storage_create_v1");
			available &= storage_create != nullptr;
			storage_retire = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionStorageRetireV1>(GDExtension::get_interface_function("device_session_production_storage_retire_v1"));
			CHECK_MESSAGE(storage_retire != nullptr, "Missing installed production procedure: device_session_production_storage_retire_v1");
			available &= storage_retire != nullptr;
			storage_observe = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionStorageObserveV1>(GDExtension::get_interface_function("device_session_production_storage_observe_v1"));
			CHECK_MESSAGE(storage_observe != nullptr, "Missing installed production procedure: device_session_production_storage_observe_v1");
			available &= storage_observe != nullptr;
			storage_release = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1>(GDExtension::get_interface_function("device_session_production_storage_release_v1"));
			CHECK_MESSAGE(storage_release != nullptr, "Missing installed production procedure: device_session_production_storage_release_v1");
			available &= storage_release != nullptr;
			import_unit = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionImportUnitV1>(GDExtension::get_interface_function("device_session_production_import_unit_v1"));
			CHECK_MESSAGE(import_unit != nullptr, "Missing installed production procedure: device_session_production_import_unit_v1");
			available &= import_unit != nullptr;
			import_storage = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>(GDExtension::get_interface_function("device_session_production_import_storage_v1"));
			CHECK_MESSAGE(import_storage != nullptr, "Missing installed production procedure: device_session_production_import_storage_v1");
			available &= import_storage != nullptr;
			import_set = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionImportSetV1>(GDExtension::get_interface_function("device_session_production_import_set_v1"));
			CHECK_MESSAGE(import_set != nullptr, "Missing installed production procedure: device_session_production_import_set_v1");
			available &= import_set != nullptr;
			prepare_dispatch = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV1>(GDExtension::get_interface_function("device_session_production_prepare_dispatch_v1"));
			CHECK_MESSAGE(prepare_dispatch != nullptr, "Missing installed production procedure: device_session_production_prepare_dispatch_v1");
			available &= prepare_dispatch != nullptr;
			prepare_copy = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionPrepareCopyV1>(GDExtension::get_interface_function("device_session_production_prepare_copy_v1"));
			CHECK_MESSAGE(prepare_copy != nullptr, "Missing installed production procedure: device_session_production_prepare_copy_v1");
			available &= prepare_copy != nullptr;
			prepare_upload = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1>(GDExtension::get_interface_function("device_session_production_prepare_upload_v1"));
			CHECK_MESSAGE(prepare_upload != nullptr, "Missing installed production procedure: device_session_production_prepare_upload_v1");
			available &= prepare_upload != nullptr;
			prepare_readback = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV1>(GDExtension::get_interface_function("device_session_production_prepare_readback_v1"));
			CHECK_MESSAGE(prepare_readback != nullptr, "Missing installed production procedure: device_session_production_prepare_readback_v1");
			available &= prepare_readback != nullptr;
			observe = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionObserveV1>(GDExtension::get_interface_function("device_session_production_observe_v1"));
			CHECK_MESSAGE(observe != nullptr, "Missing installed production procedure: device_session_production_observe_v1");
			available &= observe != nullptr;
			facts = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionFactsV1>(GDExtension::get_interface_function("device_session_production_facts_v1"));
			CHECK_MESSAGE(facts != nullptr, "Missing installed production procedure: device_session_production_facts_v1");
			available &= facts != nullptr;
			copy = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionCopyV1>(GDExtension::get_interface_function("device_session_production_copy_v1"));
			CHECK_MESSAGE(copy != nullptr, "Missing installed production procedure: device_session_production_copy_v1");
			available &= copy != nullptr;
			cancel = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionCancelV1>(GDExtension::get_interface_function("device_session_production_cancel_v1"));
			CHECK_MESSAGE(cancel != nullptr, "Missing installed production procedure: device_session_production_cancel_v1");
			available &= cancel != nullptr;
			release = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionReleaseV1>(GDExtension::get_interface_function("device_session_production_release_v1"));
			CHECK_MESSAGE(release != nullptr, "Missing installed production procedure: device_session_production_release_v1");
			available &= release != nullptr;
			close = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionCloseV1>(GDExtension::get_interface_function("device_session_production_close_v1"));
			CHECK_MESSAGE(close != nullptr, "Missing installed production procedure: device_session_production_close_v1");
			available &= close != nullptr;
			installation_observe = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionInstallationObserveV1>(GDExtension::get_interface_function("device_session_production_installation_observe_v1"));
			CHECK_MESSAGE(installation_observe != nullptr, "Missing installed production procedure: device_session_production_installation_observe_v1");
			available &= installation_observe != nullptr;
			return available;
		}
	};
	// Only fixture inputs: all identities and request storage still come from
	// the existing original enrollment, quote/accept/open and installed port.
	struct OrdinalPlan {
		GDExtensionProductionCapacityV1 capacity = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 3, 3, 3, 3, 2048, 1, 3, 8, 4 };
		GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 91, 1, 10, 13 };
		GDExtensionProductionActionV2 actions[3] = {
			{ 0, 11, 7, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 1, 11, 2, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 1, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 2, 11, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, 0, 2, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		GDExtensionProductionResourceV1 resources[3] = {
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 4, 5, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 6, 0, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 }
		};
		GDExtensionProductionUseV1 uses[3] = {
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_WRITE_V1, 0, 16, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 4, 0 }
		};
		GDExtensionProductionReadbackV2 readback = { 1, 0, 1, 2, 0, 4 };
		uint32_t action_count = 3, resource_count = 3, use_count = 3, readback_count = 1;
		uint32_t attachment_requests = 1;
	};
	struct SingletonEnrollment {
		Registry &registry = Registry::get_singleton();
		RenderingServerDefault *server = nullptr;
		bool public_port = false;
		PublicProductionPort port;
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
		Error initialize_owner(uint32_t loader_kind = 1, bool owned_storage = false, bool owned_set = false, GDExtensionDeviceExit::ProductionRecordReceiver receiver = nullptr, void *userdata = nullptr, bool keep_list = false, uint32_t record_actions = 1, bool upload_prefix = false, GDExtensionDeviceExit::ProductionMetadataReceiver metadata_receiver = nullptr, bool output_closure = false, bool output_borrow = false, bool mesh_directory = false, bool directory_external = false, const OrdinalPlan *ordinal_plan = nullptr, RenderingDevice *shared_device = nullptr) {
			using namespace GDExtensionDeviceExit;
			RenderingDevice &enrollment_rd = shared_device ? *shared_device : rd;
			if (public_port && !port.load()) {
				return ERR_UNAVAILABLE;
			}
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
					if (registry.device_records[i].object == &enrollment_rd) {
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
				if (actual_object != uint64_t(enrollment_rd.get_instance_id())) {
					return ERR_INVALID_DATA;
				}
			} else if (Error error = Error(registry.bind_scene_ready_device_session(&Enrollment::ABI, ready, &enrollment_rd, &context, &session).native_error); error != OK) {
				return error;
			}
			GDExtensionProductionCapacityV1 whole = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, owned_set ? 3u : 2u, owned_set ? 3u : 2u, 2, 32, 0, owned_set ? 3u : 2u, 8, 0 };
			if (record_actions > 1) {
				whole.suboperations = whole.graph_memberships = record_actions + 1;
				whole.native_memberships = 2048;
			}
			if (upload_prefix) {
				whole.graph_memberships = 5;
				whole.native_memberships = 256;
				whole.retained_payload_bytes = 128;
			}
			if (output_closure) {
				whole.resource_uses += 16 + (mesh_directory ? (directory_external ? 48 : 16) : 0);
			}
			if (output_borrow) {
				whole.resource_uses += 2;
			}
			if (ordinal_plan) {
				whole = ordinal_plan->capacity;
				whole.requests = ordinal_plan->attachment_requests;
			}
			GDExtensionProductionMetadataQuoteV1 quote = {};
			if (public_port) {
				CHECK(port.quote(&Enrollment::ABI, ready, { instance, UINT64_MAX }, context, session, &whole, &quote).native_error != OK);
				CHECK(quote.allocation_bytes == 0);
			}
			if (Error error = Error((public_port ? port.quote(&Enrollment::ABI, ready, producer, context, session, &whole, &quote) : registry.production_quote(&Enrollment::ABI, ready, producer, context, session, &whole, &quote)).native_error); error != OK) {
				return error;
			}
			GDExtensionProductionInstallationReportV1 report = {};
			if (public_port) {
				GDExtensionProductionAttachmentRefV1 rejected = {};
				CHECK(port.accept(&Enrollment::ABI, ready, producer, context, session, { instance, UINT64_MAX }, &quote, &rejected, &report).native_error != OK);
				CHECK((rejected.handle_serial == 0 && holder.production_children == 0));
			}
			if (Error error = Error((public_port ? port.accept(&Enrollment::ABI, ready, producer, context, session, { instance, holder.handle_serial }, &quote, &attachment, &report) : registry.production_accept(&Enrollment::ABI, ready, producer, context, session, { instance, holder.handle_serial }, &quote, &attachment, &report)).native_error); error != OK) {
				return error;
			}
			if (receiver) {
				if (Error error = public_port ? Error(port.install_record_receiver(ready, attachment, receiver, userdata, metadata_receiver)) : registry.production_install_record_receiver(ready, attachment, receiver, userdata, metadata_receiver); error != OK) {
					return error;
				}
				if (Error error = Error((public_port ? port.open(ready, attachment) : registry.production_open(ready, attachment)).native_error); error != OK) {
					return error;
				}
			}
			if (ordinal_plan) {
				const auto reserve_v2 = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionReserveV2>(GDExtension::get_interface_function("device_session_production_reserve_v2"));
				if (!reserve_v2) {
					return ERR_UNAVAILABLE;
				}
				GDExtensionProductionRequestRefV1 reserved = {};
				const auto &p = *ordinal_plan;
				const Error error = Error(reserve_v2(attachment, &p.operation, &p.capacity, p.action_count ? p.actions : nullptr, p.action_count, p.resource_count ? p.resources : nullptr, p.resource_count, p.use_count ? p.uses : nullptr, p.use_count, p.readback_count ? &p.readback : nullptr, p.readback_count, &reserved).native_error);
				if (error != OK) {
					return error;
				}
				MutexLock lock(registry.mutex);
				RenderingDevice *device = nullptr;
				return registry.production_request_locked(reserved, false, request, device);
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
			if (output_closure) {
				single.resource_uses += 16 + (mesh_directory ? (directory_external ? 48 : 16) : 0);
			}
			if (output_borrow) {
				single.resource_uses += 2;
			}
			const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 91, 1, 10, 10 + record_actions };
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
				if (Error error = Error((public_port ? port.reserve(attachment, &operation, &single, action_rows.ptr(), action_rows.size(), owned_set ? set_resources : &resource, owned_set ? 3 : 1, owned_set ? &set_use : &use, 1, nullptr, 0, &reserved) : registry.production_reserve(attachment, &operation, &single, action_rows.ptr(), action_rows.size(), owned_set ? set_resources : &resource, owned_set ? 3 : 1, owned_set ? &set_use : &use, 1, nullptr, 0, &reserved)).native_error); error != OK) {
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
				if (!Access::check_step((public_port ? port.close(attachment) : registry.production_close(attachment)).native_error == OK)) {
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
		auto budget = GDExtensionProductionServiceBudgetV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 1, 1, 1, 0 };
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
			auto budget = GDExtensionProductionServiceBudgetV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
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
				auto budget = GDExtensionProductionServiceBudgetV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
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
		bool v1_compat_probe = false;
		bool record_window = false, keep_list = false, close_before_ack = false, budget_pause = false, upload_prefix = false;
		bool public_port = false, output_closure = false, closure_retired = false, groups_settled = false, closure_rollback = false, output_borrow = false;
		uint64_t borrowed_group = 0, owned_group = 0, qualified_group = 0;
		bool directory_external = false;
		RID directory_external_roots[4], directory_owned_roots[3];
		bool mesh_directory = false, mesh_directory_returned = false, directory_capacity_failure = false;
		bool directory_return_started = false;
		bool directory_service_paused = false;
		bool retirement_auth_checked = false;
		RendererRD::MeshStorage::Mesh directory_mesh;
		RendererRD::MeshStorage::NativeSurfaceHold directory_hold, directory_neighbor;
		uint64_t directory_selected_group = 0, neighbor_selected_group = 0;
		bool directory_neighbor_returned = false, directory_neighbor_metadata = false, directory_marked = false;
		uint32_t directory_component_turn = 0, directory_premature_finish_checks = 0, directory_pending_checks = 0;
		static inline thread_local SplitSet *directory_component = nullptr;
		uint32_t nested_disposal_probes = 0, foreign_callback_calls = 0;
		RID auxiliary_shader, auxiliary_pipeline, auxiliary_buffer, auxiliary_set;
		NativeDisposalWitness auxiliary_buffer_witness, auxiliary_set_witness;
		GDExtensionDeviceExit::ProductionOutputPin output_pin, borrowed_pin;
		GDExtensionProductionAppendV1 upload_append = {};
		uint32_t metadata_calls = 0;
		bool metadata_returned = false, metadata_release = false;
		uint64_t release_control = 0;
		GDExtensionDeviceExit::ProductionControlOutput release_output = {};
		GDExtensionProductionServiceBudgetV1 release_budget = {}, release_remaining = {};
		GDExtensionProductionReportV1 release_report = {};
		GDExtensionDeviceSessionResultV1 public_release_result = {};
		uint32_t receiver_calls = 0;
		uint64_t receiver_thread = 0;
		GDExtensionDeviceExit::ProductionPhysicalContextRef expired_context;
		GDExtensionProductionSegmentV1 recorded_segment = {};
		Error schedule(GDExtensionProductionRequestRefV1 ref, uint64_t *serial) {
			auto &fixture = *setup.fixture;
			return public_port ? Error(fixture.port.record_schedule(ref, serial)) : fixture.registry.production_record_schedule(ref, serial);
		}
		GDExtensionDeviceSessionResultV1 consume(GDExtensionProductionRequestRefV1 ref, uint64_t serial, GDExtensionProductionRecordReportV1 *out) {
			auto &fixture = *setup.fixture;
			return public_port ? fixture.port.record_consume(ref, serial, out) : fixture.registry.production_record_consume(ref, serial, out);
		}
		GDExtensionDeviceSessionResultV1 append(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, uint64_t canonical, uint64_t revision, GDExtensionProductionAppendV1 *out) {
			auto &fixture = *setup.fixture;
			return public_port ? fixture.port.append(context, ref, canonical, revision, out) : fixture.registry.production_append(context, ref, canonical, revision, out);
		}
		GDExtensionDeviceSessionResultV1 ack(GDExtensionProductionRequestRefV1 ref, uint64_t segment, uint64_t revision) {
			auto &fixture = *setup.fixture;
			return public_port ? fixture.port.ack_segment(ref, segment, revision) : fixture.registry.production_ack_segment(ref, segment, revision);
		}
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
			test.check(__LINE__, test.schedule(ref, &control) == ERR_UNAUTHORIZED);
			const auto dispatch = GDExtensionProductionDispatchV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 2, 1, 1, 1, 1, 0, 0 };
			test.check(__LINE__, registry.production_prepare_dispatch(ref, &dispatch, nullptr, 0).native_error == ERR_UNAUTHORIZED);
			GDExtensionProductionAppendV1 forbidden = {};
			test.check(__LINE__, test.append(test.expired_context, ref, 10, request.revision, &forbidden).native_error == ERR_UNAUTHORIZED);
			if (test.metadata_calls == 1) {
				// The initial record return was consumed before this fresh ACK frame.
				test.check(__LINE__, !test.metadata_release && request.control.state == ProductionControlState::OWNER_CONSUMED);
				test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision + 1).native_error == ERR_DOES_NOT_EXIST);
				test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == OK);
				test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == OK);
			} else {
				// A later RELEASE return still owns its actual cell. Preserve the
				// original ACK and consume that return before any arena reclamation.
				test.check(__LINE__, test.metadata_release && request.acknowledged_segment == test.recorded_segment.segment);
				test.check(__LINE__, request.control.state == ProductionControlState::RETURNED && request.control.input.kind == ProductionControlKind::RELEASE && request.control.serial == test.release_control);
				if (test.public_port) {
					test.public_release_result = fixture.port.release(ref, &test.release_budget, &test.release_remaining, &test.release_report);
				} else {
					test.check(__LINE__, registry.production_control_consume(ref, test.release_control, &test.release_output) == OK);
				}
			}
			test.check(__LINE__, registry.production_close(attachment).native_error == ERR_BUSY); // Real callback/code return still owns its frame.
			test.metadata_returned = true;
		}
		static GDExtensionDeviceSessionResultV1 owner_ack(SingletonEnrollment &fixture, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionSegmentV1 &segment) {
			GDExtensionDeviceExit::CallbackScope owner_entry(fixture.reference);
			if (owner_entry.get_error() != OK) {
				return { int32_t(owner_entry.get_error()), 0, 0, 0 };
			}
			return fixture.public_port ? fixture.port.ack_segment(ref, segment.segment, segment.segment_revision) : fixture.registry.production_ack_segment(ref, segment.segment, segment.segment_revision);
		}
		static void record_receiver(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *operation) {
			auto &test = *static_cast<SplitSet *>(opaque);
			auto &fixture = *test.setup.fixture;
			auto &request = *fixture.request;
			auto &registry = fixture.registry;
			++test.receiver_calls;
			test.receiver_thread = Thread::get_caller_id();
			test.expired_context = context;
			if (test.v1_compat_probe) {
				const auto append_v1 = Access::ordinal_port<GDExtensionInterfaceDeviceSessionProductionAppendV1>("device_session_production_append_v1");
				if (!test.check(__LINE__, append_v1 != nullptr)) {
					return;
				}
				const uint64_t before_revision = request.revision;
				const uint32_t before_next = request.next_action;
				GDExtensionProductionAppendV1 rejected;
				std::memset(&rejected, 0x5a, sizeof(rejected));
				const auto result = append_v1(context, ref, operation->canonical_begin, request.revision ^ 1, &rejected);
				test.check(__LINE__, Access::v1_result(result, ERR_INVALID_DATA, 0));
				test.check(__LINE__, rejected.abi.abi_version == 1 && rejected.abi.struct_size == sizeof(rejected));
				test.check(__LINE__, rejected.canonical_suboperation == operation->canonical_begin && rejected.revision == before_revision);
				test.check(__LINE__, Access::v1_result(rejected.cause, ERR_INVALID_DATA, 0));
				test.check(__LINE__, request.next_action == before_next && request.revision == before_revision);
				std::memset(&rejected, 0x5a, sizeof(rejected));
				test.check(__LINE__, Access::v1_result(append_v1(context, ref, operation->canonical_begin - 1, request.revision, &rejected), ERR_INVALID_DATA, 0));
				test.check(__LINE__, rejected.canonical_suboperation == operation->canonical_begin - 1 && rejected.revision == before_revision);
				test.check(__LINE__, rejected.abi.abi_version == 1 && Access::v1_result(rejected.cause, ERR_INVALID_DATA, 0));
				test.check(__LINE__, request.next_action == before_next && request.revision == before_revision);
			}

			test.check(__LINE__, test.receiver_thread == fixture.driver.execution_thread && test.receiver_thread != registry.records[fixture.reference.slot].owner_thread);
			test.check(__LINE__, operation && operation->owner_request_nonce == request.operation.owner_request_nonce);
			test.check(__LINE__, registry.records[fixture.reference.slot].active_callbacks > 0);
			if (test.public_port) {
				using GDExtensionDeviceExit::ProductionOutputPin;
				test.check(__LINE__, request.frozen && request.next_action == 0 && request.graph_count == 0 && request.native_count == 0);
				ProductionOutputPin refused;
				auto wrong = context;
				++wrong.frame_serial;
				test.check(__LINE__, registry.production_output_pin(wrong, ref, fixture.rd, refused) == ERR_UNAUTHORIZED && refused.is_empty());
				test.check(__LINE__, registry.production_output_pin(context, { ref.registry_instance, UINT64_MAX }, fixture.rd, refused) == ERR_UNAUTHORIZED && refused.is_empty());
				const uint32_t binding_used = request.binding_used;
				const uint64_t revision = request.revision;
				const uint64_t before_pin_serial = request.next_output_pin_serial;
				{
					// A nested admitted callback is not the original RECORD entry.
					// The outer production TLS frame remains installed, so this
					// distinguishes exact protected-entry validation from TLS-only.
					GDExtensionDeviceExit::CallbackScope nested(fixture.reference);
					test.check(__LINE__, nested.get_error() == OK);
					test.check(__LINE__, registry.production_output_pin_current(fixture.rd, refused) == ERR_UNAUTHORIZED && refused.is_empty());
					test.check(__LINE__, request.next_output_pin_serial == before_pin_serial && request.binding_used == binding_used && request.revision == revision && request.next_action == 0 && request.graph_count == 0 && request.native_count == 0);
				}
				// Unwinding the nested entry restores the original RECORD frame.
				test.check(__LINE__, registry.production_output_pin_current(fixture.rd, test.output_pin) == OK && !test.output_pin.is_empty());
				test.check(__LINE__, registry.production_output_pin_matches(test.output_pin, ref, fixture.rd));
				test.check(__LINE__, !registry.production_output_pin_matches(test.output_pin, { ref.registry_instance, UINT64_MAX }, fixture.rd));
				test.check(__LINE__, registry.production_output_pin_current(fixture.rd, test.output_pin) == ERR_ALREADY_IN_USE);
				test.check(__LINE__, registry.production_output_pin_matches(test.output_pin, ref, fixture.rd));
				if (!test.output_borrow) {
					test.check(__LINE__, registry.production_output_pin_current(fixture.rd, refused) == ERR_OUT_OF_MEMORY && refused.is_empty());
				}
				ProductionOutputPin moved(std::move(test.output_pin));
				test.check(__LINE__, test.output_pin.is_empty() && !moved.is_empty());
				test.output_pin = std::move(moved);
				const uint64_t old_pin_serial = request.next_output_pin_serial - 1;
				test.check(__LINE__, registry.production_output_unpin(test.output_pin) == OK && test.output_pin.is_empty());
				test.check(__LINE__, registry.production_output_pin(context, ref, fixture.rd, test.output_pin) == OK && request.next_output_pin_serial == old_pin_serial + 2);
				test.check(__LINE__, moved.is_empty() && !test.output_pin.is_empty() && request.binding_used == binding_used && request.revision == revision && request.graph_count == 0);
			}

			if (test.output_closure) {
				using namespace GDExtensionDeviceExit;
				auto &rd = fixture.rd;
				test.check(__LINE__, registry.production_output_pin(context, ref, rd, test.output_pin) == OK);
				test.check(__LINE__, rd._production_output_owned_reserve(context, test.output_pin, 2, test.owned_group) == OK);
				const uint64_t rolled_back_serial = test.owned_group;
				const auto next_group_serial = request.next_disposal_group_serial;
				auto wrong_rollback_context = context;
				++wrong_rollback_context.frame_serial;
				test.check(__LINE__, rd._production_output_owned_rollback(wrong_rollback_context, test.output_pin, test.owned_group) == ERR_UNAUTHORIZED);
				test.check(__LINE__, test.owned_group == rolled_back_serial && request.next_disposal_group_serial == next_group_serial);
				// Cleanup keeps exact RECORD authority after new append admission closes.
				const bool was_closing = request.attachment->closing;
				request.attachment->closing = true;
				uint64_t refused_group = 0;
				test.check(__LINE__, rd._production_output_owned_reserve(context, test.output_pin, 2, refused_group) == ERR_BUSY && !refused_group);
				test.check(__LINE__, rd._production_output_owned_rollback(context, test.output_pin, test.owned_group) == OK && !test.owned_group);
				request.attachment->closing = was_closing;
				test.check(__LINE__, request.next_disposal_group_serial == next_group_serial);
				for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
					test.check(__LINE__, request.uses[i].disposal_group_serial != rolled_back_serial);
				}
				test.check(__LINE__, rd._production_output_owned_reserve(context, test.output_pin, 2, test.owned_group) == OK && test.owned_group == next_group_serial);
				const uint32_t used = request.binding_used;
				const uint64_t revision = request.revision;
				uint64_t oversized = 0;
				test.check(__LINE__, rd._production_output_owned_reserve(context, test.output_pin, request.capacity.resource_uses, oversized) == ERR_OUT_OF_MEMORY && !oversized);
				fixture.driver.auxiliary_factory = true;
				Ref<UnitContainer> native_container;
				native_container.instantiate();
				native_container->initialize(16);
				test.auxiliary_shader = rd.shader_create_from_bytecode(native_container->to_bytes());
				test.auxiliary_pipeline = rd.compute_pipeline_create(test.auxiliary_shader);
				test.check(__LINE__, test.auxiliary_shader.is_valid() && test.auxiliary_pipeline.is_valid());
				test.check(__LINE__, rd._production_output_owned_storage_create(context, test.output_pin, test.owned_group, 0, 16, {}, test.auxiliary_buffer, test.auxiliary_buffer_witness) == OK);
				const auto retained_group = test.owned_group;
				test.check(__LINE__, rd._production_output_owned_rollback(context, test.output_pin, test.owned_group) == ERR_BUSY);
				test.check(__LINE__, test.owned_group == retained_group && fixture.driver.auxiliary_buffer_live);
				RenderingDevice::Uniform binding;
				binding.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
				binding.binding = 0;
				binding.append_id(test.auxiliary_buffer);
				Vector<RenderingDevice::Uniform> bindings;
				bindings.push_back(binding);
				if (test.closure_rollback) {
					bindings.write[0].uniform_type = RenderingDevice::UNIFORM_TYPE_SAMPLER;
					test.check(__LINE__, rd._production_output_owned_uniform_set_create(context, test.output_pin, test.owned_group, 1, bindings, test.auxiliary_shader, 0, test.auxiliary_set, test.auxiliary_set_witness) == ERR_UNAVAILABLE);
					test.check(__LINE__, test.auxiliary_set.is_null() && fixture.driver.auxiliary_buffer_live && !fixture.driver.auxiliary_set_live);
					test.check(__LINE__, rd._production_output_owned_cancel_unbound(test.output_pin, test.owned_group) == OK);
				} else {
					test.check(__LINE__, rd._production_output_owned_uniform_set_create(context, test.output_pin, test.owned_group, 1, bindings, test.auxiliary_shader, 0, test.auxiliary_set, test.auxiliary_set_witness) == OK);
				}
				fixture.driver.auxiliary_factory = false;
				fixture.driver.disposal_probe_data = &test;
				fixture.driver.disposal_probe = +[](void *data) {
					auto &actual = *static_cast<SplitSet *>(data);
					auto &probe_device = actual.setup.fixture->rd;
					if (!probe_device.native_disposal_budget) {
						return; // Original qualification's later unmetered owner lane.
					}
					const auto *original_budget = probe_device.native_disposal_budget;
					auto *original_usage = probe_device.native_disposal_usage;
					const auto *original_owner = probe_device.production_disposal_drop_owner;
					RenderingDevice::NativeDisposalUsage rejected;
					const RenderingDevice::NativeDisposalBudget offered{ 1, 1, OS::get_singleton()->get_ticks_usec() + 10000000 };
					actual.check(__LINE__, probe_device._production_output_drop_step(*actual.setup.fixture->request, offered, rejected) == ERR_UNAUTHORIZED);
					actual.check(__LINE__, probe_device.native_disposal_budget == original_budget && probe_device.native_disposal_usage == original_usage && probe_device.production_disposal_drop_owner == original_owner && rejected.passes == 0 && rejected.polls == 0 && rejected.frees == 0);
					++actual.nested_disposal_probes;
				};

				test.check(__LINE__, rd._production_output_owned_bind(context, test.output_pin, test.owned_group, 0, test.auxiliary_buffer_witness) == ERR_ALREADY_IN_USE);
				test.check(__LINE__, rd._production_output_owned_bind(context, test.output_pin, test.owned_group, 1, test.auxiliary_buffer_witness) == ERR_UNAUTHORIZED);
				const RID root = test.auxiliary_pipeline;
				RenderingDevice::NativeImmutableHold physical;
				test.check(__LINE__, rd.native_immutable_hold_acquire(VectorView<RID>(&root, 1), physical, 4) == OK);
				test.check(__LINE__, rd._production_output_hold_capture(context, test.output_pin, physical, test.borrowed_group) == OK && !physical.is_active());
				test.check(__LINE__, rd._production_output_hold_contains(test.output_pin, test.borrowed_group, test.auxiliary_pipeline));
				const RID qualified_root = request.resources[0].primary;
				RenderingDevice::NativeImmutableHold qualified_hold;
				test.check(__LINE__, rd.native_immutable_hold_acquire(VectorView<RID>(&qualified_root, 1), qualified_hold, 8) == OK);
				test.check(__LINE__, rd._production_output_hold_capture(context, test.output_pin, qualified_hold, test.qualified_group) == OK && !qualified_hold.is_active());
				test.check(__LINE__, rd._production_output_hold_contains(test.output_pin, test.qualified_group, qualified_root));
				test.check(__LINE__, !rd._production_output_hold_contains(test.output_pin, test.owned_group, test.auxiliary_buffer));
				test.check(__LINE__, rd._production_output_owned_seal(context, test.output_pin, test.owned_group) == ERR_BUSY);
				test.check(__LINE__, registry.production_output_unpin(test.output_pin) == ERR_BUSY && request.binding_used == used && request.revision == revision);
				RendererRD::external_custody_issued_pin_test(registry, ref, test.output_pin, RendererRD::ExternalCustodyTestMode::SLOT_PENDING);
				test.check(__LINE__, registry.production_output_pin_matches(test.output_pin, ref, fixture.rd));
			}

			if (test.mesh_directory) {
				using MeshStorage = RendererRD::MeshStorage;
				auto &rd = fixture.rd;
				if (!test.directory_capacity_failure) {
					fixture.driver.directory_pending_polls = 4;
				}
				fixture.driver.mesh_directory_factory = true;
				auto *surface = memnew(MeshStorage::Mesh::Surface);
				surface->format = RSE::ARRAY_FORMAT_VERTEX;
				surface->vertex_count = 3;
				surface->primitive = RSE::PRIMITIVE_TRIANGLES;
				surface->vertex_buffer_size = 36;
				surface->vertex_buffer = rd.vertex_buffer_create(36, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
				if (test.directory_external) {
					surface->external = true;
					surface->previous_vertex_buffer = rd.vertex_buffer_create(36, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
					surface->index_buffer = rd.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32);
					surface->index_count = 3;
					surface->index_array = rd.index_array_create(surface->index_buffer, 0, 3);
					surface->lod_count = 1;
					surface->lods = memnew_arr(MeshStorage::Mesh::Surface::LOD, 1);
					surface->lods[0].index_buffer = rd.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32);
					surface->lods[0].index_count = 3;
					surface->lods[0].index_array = rd.index_array_create(surface->lods[0].index_buffer, 0, 3);
					test.directory_external_roots[0] = surface->vertex_buffer;
					test.directory_external_roots[1] = surface->previous_vertex_buffer;
					test.directory_external_roots[2] = surface->index_buffer;
					test.directory_external_roots[3] = surface->index_array;
					test.directory_owned_roots[1] = surface->lods[0].index_buffer;
					test.directory_owned_roots[2] = surface->lods[0].index_array;
				}
				fixture.driver.mesh_directory_factory = false;
				RD::VertexAttribute position;
				position.location = 0;
				position.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
				position.stride = 12;
				Vector<RD::VertexAttribute> attributes;
				attributes.push_back(position);
				const RD::VertexFormatID format = rd.vertex_format_create(attributes);
				Vector<RID> buffers;
				buffers.push_back(surface->vertex_buffer);
				// The capacity negative uses a genuine complete directory larger
				// than the accepted arena, independent of other closure spare cells.
				surface->version_count = test.directory_capacity_failure ? request.capacity.resource_uses : 1;
				const uint32_t directory_limit = test.directory_capacity_failure ? surface->version_count + 1 : 8;
				surface->versions = static_cast<MeshStorage::Mesh::Surface::Version *>(memalloc(sizeof(MeshStorage::Mesh::Surface::Version) * surface->version_count));
				for (uint32_t i = 0; i < surface->version_count; ++i) {
					new (&surface->versions[i]) MeshStorage::Mesh::Surface::Version;
					surface->versions[i].input_mask = 1;
					surface->versions[i].vertex_format = format;
					surface->versions[i].vertex_array = rd.vertex_array_create(3, format, buffers);
					test.check(__LINE__, surface->versions[i].vertex_array.is_valid());
				}
				test.directory_owned_roots[0] = surface->versions[0].vertex_array;
				test.directory_mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(sizeof(MeshStorage::Mesh::Surface *)));
				test.directory_mesh.surfaces[0] = surface;
				test.directory_mesh.surface_count = 1;
				MeshStorage::NativeSurfaceHold original;
				test.check(__LINE__, MeshStorage::_mesh_surface_hold_acquire(surface, 1, false, false, 0, original) == OK);
				test.check(__LINE__, original.is_valid() && original.is_external() == test.directory_external);
				{
					GDExtensionDeviceExit::ProductionOutputPin surface_pin;
					test.check(__LINE__, registry.production_output_pin_borrow(test.output_pin, rd, surface_pin) == OK);
					RendererRD::external_custody_issued_pin_test(registry, ref, surface_pin, RendererRD::ExternalCustodyTestMode::SURFACE_RETURN, &original);
					test.check(__LINE__, surface_pin.is_empty());
				}
				test.check(__LINE__, MeshStorage::_mesh_surface_hold_acquire(surface, 1, false, false, 0, test.directory_neighbor) == OK);
				test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, original, directory_limit) == ERR_BUSY);
				MeshStorage::_mesh_surface_clear(&test.directory_mesh, 0);
				test.directory_mesh.surface_count = 0;
				test.check(__LINE__, MeshStorage::_native_hold_freeze_directory(original) == OK);
				if (test.directory_external) {
					// Existing genuine Version remains observable after freeze; no
					// new factory or source authority is introduced by this getter.
					RID selected;
					RD::VertexFormatID selected_format = 0;
					MeshStorage::_mesh_surface_get_external_arrays(surface, 1, false, false, selected, selected_format);
					test.check(__LINE__, selected == surface->versions[0].vertex_array && selected_format == format);
					const uint32_t versions_before = surface->version_count;
					const uint32_t holds_before = rd.native_immutable_hold_count;
					const uint32_t shares_before = *rd._native_immutable_counter(surface->vertex_buffer);
					const uint64_t groups_before = request.next_disposal_group_serial;
					const uint64_t revision_before = request.revision;
					// Missing mask and previous-input Version must remain absent.
					MeshStorage::_mesh_surface_get_external_arrays(surface, 2, false, false, selected, selected_format);
					test.check(__LINE__, selected.is_null() && selected_format == 0);
					MeshStorage::_mesh_surface_get_external_arrays(surface, 1, true, false, selected, selected_format);
					test.check(__LINE__, selected.is_null() && selected_format == 0);
					test.check(__LINE__, surface->version_count == versions_before && rd.native_immutable_hold_count == holds_before);
					test.check(__LINE__, *rd._native_immutable_counter(surface->vertex_buffer) == shares_before && request.next_disposal_group_serial == groups_before && request.revision == revision_before);
					test.check(__LINE__, original.is_external());
				}
				const uint32_t original_holds = rd.native_immutable_hold_count;
				const uint64_t original_group_serial = request.next_disposal_group_serial;
				const uint32_t original_shares = *rd._native_immutable_counter(surface->vertex_buffer);
				test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, original, 1) == ERR_OUT_OF_MEMORY);
				auto foreign_context = context;
				++foreign_context.frame_serial;
				test.check(__LINE__, MeshStorage::_native_hold_capture_directory(foreign_context, test.output_pin, original, directory_limit) == ERR_UNAUTHORIZED);
				test.check(__LINE__, rd.native_immutable_hold_count == original_holds && request.next_disposal_group_serial == original_group_serial && *rd._native_immutable_counter(surface->vertex_buffer) == original_shares);
				if (test.directory_capacity_failure) {
					// All Version arrays and their shared backing fit the capture
					// limit, but their real closure cannot fit the accepted arena.
					// Genuine new physical shares must roll back.
					test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, original, directory_limit) == ERR_OUT_OF_MEMORY);
					test.check(__LINE__, rd.native_immutable_hold_count == original_holds && request.next_disposal_group_serial == original_group_serial && *rd._native_immutable_counter(surface->vertex_buffer) == original_shares);
				} else {
					test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, original, 8) == OK);
					test.check(__LINE__, rd.native_immutable_hold_count == original_holds + 1 && *rd._native_immutable_counter(surface->vertex_buffer) == original_shares + 1);
					test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, original, 8) == ERR_BUSY);
					test.check(__LINE__, MeshStorage::_native_hold_thaw_directory(original) == ERR_BUSY);
					test.check(__LINE__, rd._production_output_hold_capture(context, test.output_pin, *MeshStorage::_native_hold_transfer_source(original), test.directory_selected_group) == OK);
					test.check(__LINE__, MeshStorage::_native_hold_mark_transferred(original) == OK);
					test.check(__LINE__, !original.is_valid() && !original.is_external());
				}
				if (!test.directory_capacity_failure) {
					test.check(__LINE__, MeshStorage::_native_hold_freeze_directory(test.directory_neighbor) == OK);
					test.check(__LINE__, MeshStorage::_native_hold_capture_directory(context, test.output_pin, test.directory_neighbor, 8) == OK);
					test.check(__LINE__, rd._production_output_hold_capture(context, test.output_pin, *MeshStorage::_native_hold_transfer_source(test.directory_neighbor), test.neighbor_selected_group) == OK);
					test.check(__LINE__, MeshStorage::_native_hold_mark_transferred(test.directory_neighbor) == OK);
				}
				test.directory_hold = static_cast<MeshStorage::NativeSurfaceHold &&>(original);
				test.check(__LINE__, !original.is_active() && !original.is_external());
				test.check(__LINE__, test.directory_hold.is_external() == (test.directory_capacity_failure && test.directory_external));
				test.check(__LINE__, MeshStorage::_native_hold_begin_return(test.directory_hold) == ERR_BUSY);
				if (!test.directory_capacity_failure) {
					directory_component = &test;
				}
			}

			if (test.closure_rollback) {
				return; // Actual buffer allocation prefix; no record/native effect.
			}

			if (test.upload_prefix) {
				const Error error = Error(test.append(context, ref, 10, request.revision, &test.upload_append).native_error);
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
					const Error error = Error(test.append(context, ref, 10 + request.next_action, request.revision, &part).native_error);
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
			test.check(__LINE__, test.append(forged, ref, 10, request.revision, &appended).native_error == ERR_UNAUTHORIZED);
			test.check(__LINE__, test.append(context, ref, 11, request.revision, &appended).native_error == ERR_INVALID_DATA);
			test.check(__LINE__, request.next_action == 0 && request.graph_count == 0);
			test.check(__LINE__, test.append(context, ref, 10, request.revision, &appended).native_error == OK);
			test.check(__LINE__, appended.state == GDEXTENSION_PRODUCTION_APPENDED_V1 && request.next_action == 1 && request.graph_count == 1);
			if (test.public_port) {
				GDExtensionDeviceExit::ProductionOutputPin too_late;
				const uint64_t before_pin_serial = request.next_output_pin_serial, before_revision = request.revision;
				test.check(__LINE__, registry.production_output_pin_current(fixture.rd, too_late) == ERR_BUSY && too_late.is_empty());
				test.check(__LINE__, request.next_output_pin_serial == before_pin_serial && request.revision == before_revision && request.next_action == 1 && request.graph_count == 1);
				if (test.output_borrow) {
					// Real original pin, actual appended work, same accepted spare cells.
					const uint64_t before = request.revision;
					test.check(__LINE__, registry.production_output_pin_borrow(test.output_pin, fixture.rd, test.borrowed_pin) == OK && !test.borrowed_pin.is_empty());
					test.check(__LINE__, registry.production_output_pin_matches(test.borrowed_pin, ref, fixture.rd) && request.revision == before);
				}
			}
			if (test.keep_list) {
				test.check(__LINE__, request.compute_list_open && fixture.rd.compute_list.active);
				return; // Actual protected scope return must close and seal this prefix.
			}
			test.check(__LINE__, (test.public_port ? fixture.port.close_segment(context, ref, &test.recorded_segment) : registry.production_close_segment(context, ref, &test.recorded_segment)).native_error == OK);
			test.check(__LINE__, test.recorded_segment.segment && request.acknowledged_segment == 0);
			if (test.output_closure) {
				auto &rd = fixture.rd;
				test.check(__LINE__, request.native_count == 0 && request.graph_count == 1);
				test.check(__LINE__, rd._production_output_owned_seal(context, test.output_pin, test.owned_group) == OK);
				test.check(__LINE__, rd._production_output_hold_contains(test.output_pin, test.owned_group, test.auxiliary_buffer));
				const uint32_t contents = 0;
				const uint32_t graph_commands = rd.draw_graph.qualification_command_count();
				test.check(__LINE__, rd._buffer_update(rd.storage_buffer_owner.get_or_null(test.auxiliary_buffer), test.auxiliary_buffer, 0, sizeof(contents), &contents) == ERR_BUSY && rd.draw_graph.qualification_command_count() == graph_commands);
				if (test.closure_retired) {
					rd.free_rid(test.auxiliary_shader);
				}
				test.check(__LINE__, fixture.driver.auxiliary_shader_frees == 0 && fixture.driver.auxiliary_pipeline_frees == 0 && fixture.driver.auxiliary_set_frees == 0 && fixture.driver.auxiliary_buffer_frees == 0);
			}
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
		static Error directory_service(GDExtensionDeviceExit::ProductionRequest &request, RenderingDevice &rd) {
			using MeshStorage = RendererRD::MeshStorage;
			auto *test = directory_component;
			if (!test || &request != test->setup.fixture->request || &rd != &test->setup.fixture->rd || request.control.input.kind != GDExtensionDeviceExit::ProductionControlKind::OBSERVE) {
				return ERR_INVALID_DATA;
			}
			if (test->directory_service_paused) {
				return ERR_BUSY; // Preserve the original callback rejection slice.
			}
			auto &budget = request.control.input.budget;
			auto &usage = request.control.output.service_usage;
			if (!test->retirement_auth_checked) {
				// This is the original EXECUTING OBSERVE callback and actual pin,
				// not a forged positive control/source/owner association.
				using namespace GDExtensionDeviceExit;
				const auto initial_usage = usage;
				const auto revision = request.revision;
				const auto graphs = request.graph_count, natives = request.native_count;
				const auto copied_budget = budget;
				ProductionServiceUsage copied_usage = {};
				ProductionDisposalObservation observation;
				test->check(__LINE__, rd._production_output_retire_service(test->output_pin, 0, copied_budget, usage, observation) == ERR_UNAUTHORIZED);
				test->check(__LINE__, rd._production_output_retire_service(test->output_pin, 0, budget, copied_usage, observation) == ERR_UNAUTHORIZED);
				{
					RenderingDevice foreign;
					test->check(__LINE__, foreign._production_output_retire_service(test->output_pin, 0, budget, usage, observation) == ERR_UNAUTHORIZED);
				}
				test->check(__LINE__, usage.controls == initial_usage.controls && usage.passes == initial_usage.passes && usage.receipts == initial_usage.receipts);
				test->check(__LINE__, copied_usage.controls == 0 && copied_usage.passes == 0 && copied_usage.receipts == 0);
				test->check(__LINE__, request.revision == revision && request.graph_count == graphs && request.native_count == natives);
				test->check(__LINE__, test->setup.fixture->registry.production_output_pin_matches(test->output_pin, { test->setup.fixture->instance, request.serial }, rd));
				test->retirement_auth_checked = true;
			}

			const uint32_t before = usage.passes;
			// Rendezvous phase is a message kind (every control uses 6),
			// so retain the actual owner-return boundary independently.
			if (!test->directory_return_started) {
				GDExtensionProductionReportV1 actual = {};
				{
					MutexLock lock(test->setup.fixture->registry.mutex);
					test->check(__LINE__, test->setup.fixture->registry.production_report_contents_locked(request, actual) == OK);
				}
				if (actual.possible_obligations || actual.known_obligations != actual.complete_obligations) {
					const uint32_t receipts = usage.receipts;
					test->check(__LINE__, MeshStorage::_native_hold_return_directory(test->output_pin, test->directory_hold, budget, usage) == ERR_BUSY);
					test->check(__LINE__, usage.passes == before && usage.receipts == receipts);
					++test->directory_pending_checks;
				}
				return ERR_BUSY; // Original maintenance must still get its turn.
			}
			if (test->directory_component_turn++ == 0) {
				// A malformed original captured graph cannot be projected as a
				// zero-obligation completion. Restore before any positive service.
				auto *const owner = request.graphs[0].request;
				request.graphs[0].request = nullptr;
				const uint32_t passes = usage.passes, receipts = usage.receipts;
				test->check(__LINE__, MeshStorage::_native_hold_return_directory(test->output_pin, test->directory_hold, budget, usage) == ERR_INVALID_DATA);
				test->check(__LINE__, usage.passes == passes && usage.receipts == receipts);
				request.graphs[0].request = owner;
				const auto copied_budget = budget;
				GDExtensionDeviceExit::ProductionServiceUsage copied_usage = {};
				test->check(__LINE__, MeshStorage::_native_hold_return_directory(test->output_pin, test->directory_hold, copied_budget, copied_usage) == ERR_UNAUTHORIZED && copied_usage.passes == 0);
			}
			if (test->directory_component_turn == 1 || test->directory_component_turn > 2) {
				if (!test->mesh_directory_returned) {
					const Error returned = MeshStorage::_native_hold_return_directory(test->output_pin, test->directory_hold, budget, usage);
					test->check(__LINE__, returned == OK || returned == ERR_BUSY);
					test->mesh_directory_returned = returned == OK;
					if (test->directory_component_turn == 1) {
						test->check(__LINE__, returned == ERR_BUSY && usage.passes == before + 1);
						test->check(__LINE__, MeshStorage::_native_hold_finish_return(test->directory_hold) == ERR_BUSY);
						++test->directory_premature_finish_checks;
					}
				}
			}
			if (usage.passes != before) {
				return OK;
			}
			if (!test->directory_marked) {
				const Error marked = MeshStorage::_native_hold_mark_next_retirement_root(test->output_pin, test->directory_neighbor, budget, usage, test->directory_marked);
				test->check(__LINE__, marked == OK || marked == ERR_BUSY);
				return marked;
			}
			if (!test->directory_neighbor_returned) {
				const Error returned = MeshStorage::_native_hold_return_directory(test->output_pin, test->directory_neighbor, budget, usage);
				test->check(__LINE__, returned == OK || returned == ERR_BUSY);
				test->directory_neighbor_returned = returned == OK;
				return OK;
			}
			if (!test->directory_neighbor_metadata) {
				// The component executor spends a separate original work pass on
				// pure Surface metadata finalization, after all native group facts.
				if (usage.passes >= MIN(budget.passes, 64u) || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
					return ERR_BUSY;
				}
				++usage.passes;
				usage.controls = 1;
				test->check(__LINE__, MeshStorage::_native_hold_begin_return(test->directory_neighbor) == OK);
				test->check(__LINE__, MeshStorage::_native_hold_finish_return(test->directory_neighbor) == OK);
				test->directory_neighbor_metadata = true;
			}
			return OK;
		}
		void finish_closure_rollback(const GDExtensionStorageBackingV1 &backing, GDExtensionProductionRequestRefV1 ref) {
			using namespace GDExtensionDeviceExit;
			auto &fixture = *setup.fixture;
			auto &registry = fixture.registry;
			auto &request = *fixture.request;
			check(__LINE__, request.graph_count == 0 && request.native_count == 0 && request.next_action == 0 && !request.cancelled);
			check(__LINE__, registry.production_cancel(ref).native_error == OK);
			const GDExtensionStorageRetireRequestV1 origin = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 1, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 retire = {};
			GDExtensionStorageRetireReportV1 retired = {};
			check(__LINE__, registry.production_storage_retire(ref, 1, &origin, &retire, &retired).native_error == OK);
			rendezvous(14);
			const uint64_t limit = 64 + uint64_t(request.capacity.resource_uses) * 16 + uint64_t(request.capacity.native_memberships) * 4;
			for (uint64_t turn = 0; turn < limit && (!groups_settled || !retired.state); ++turn) {
				const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				const Error scheduled = Error(registry.production_storage_observe(ref, retire, &budget, &retired).native_error);
				check(__LINE__, scheduled == OK || scheduled == ERR_BUSY);
				if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
					rendezvous(6);
					if (request.control.state == ProductionControlState::RETURNED) {
						check(__LINE__, request.control.output.service_usage.passes <= 1 && request.control.output.service_usage.receipts <= 1);
					}
					const Error consumed = Error(registry.production_storage_observe(ref, retire, &budget, &retired).native_error);
					check(__LINE__, consumed == OK || consumed == ERR_BUSY);
				}
				rendezvous(15);
			}
			check(__LINE__, groups_settled && retired.state == 1 && retired.event_seq && fixture.driver.buffer_frees == 1 && fixture.driver.set_frees == 1);
			check(__LINE__, registry.production_storage_release(ref, retire).native_error == OK);
			check(__LINE__, drain_request_cursor(fixture, ref, [&]() { rendezvous(6); }));
			rendezvous(13);
			check(__LINE__, output_pin.is_empty());
			fixture.driver.disposal_probe = nullptr;
			fixture.driver.disposal_probe_data = nullptr;
			check(__LINE__, registry.production_release(ref).native_error == OK && !request.serial);
			stop_before_storage();
		}

		static void run(void *data) {
			auto &test = *static_cast<SplitSet *>(data);
			auto &fixture = *test.setup.fixture;
			auto &registry = fixture.registry;
			using namespace GDExtensionDeviceExit;
			fixture.driver.upload_mode = test.upload_prefix;
			fixture.public_port = test.public_port;
			test.initialized = fixture.initialize_owner(1, false, true, test.record_window ? &record_receiver : nullptr, &test, test.keep_list, test.budget_pause ? 65u : 1u, test.upload_prefix, (test.close_before_ack || test.public_port) ? &metadata_receiver : nullptr, test.output_closure, test.output_borrow, test.mesh_directory && !test.directory_capacity_failure, test.directory_external);
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
			if (test.public_port) {
				// The real unit import has pinned this declared plan, but none of its
				// actions has recorded a graph/native fact. Release cannot discard it.
				test.check(__LINE__, !request.cancelled && request.next_action == 0 && request.action_count > 0 && request.graph_count == 0 && request.native_count == 0);
				const auto before_state = request.control.state;
				const uint64_t before_control = request.control.serial, before_next_control = request.next_control_serial;
				const uint32_t before_unit_uses = request.resources[0].use_count;
				const uint32_t before_retains = fixture.driver.retains, before_releases = fixture.driver.releases;
				const GDExtensionProductionServiceBudgetV1 offered = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				GDExtensionProductionServiceBudgetV1 remaining = {};
				GDExtensionProductionReportV1 report = {};
				const Error rejected = Error(fixture.port.release(ref, &offered, &remaining, &report).native_error);
				const bool intact = request.serial == ref.handle_serial && !request.cancelled && request.next_action == 0 && request.control.state == before_state && request.control.serial == before_control && request.next_control_serial == before_next_control && request.resources[0].use_count == before_unit_uses;
				test.check(__LINE__, rejected == ERR_BUSY && report.cause.native_error == ERR_BUSY && report.service_pending == 0 && report.service_control_serial == 0);
				test.check(__LINE__, intact && before_unit_uses == 2 && remaining.controls == offered.controls && remaining.receipts == offered.receipts && remaining.passes == offered.passes && fixture.driver.retains == before_retains && fixture.driver.releases == before_releases);
				if (rejected != ERR_BUSY || !intact) {
					// A genuine old-implementation RED may already have published a
					// destructive RELEASE. Return that real turn first; cancel only if
					// its reference survives. Never continue through a reset request.
					if (request.control.state == ProductionControlState::PENDING) {
						test.check(__LINE__, registry.production_control_schedule(ref, request.control.serial) == OK);
					}
					if (request.control.state == ProductionControlState::QUEUED) {
						test.rendezvous(6);
					}
					if (request.control.state == ProductionControlState::RETURNED) {
						ProductionControlOutput discarded = {};
						test.check(__LINE__, registry.production_control_consume(ref, request.control.serial, &discarded) == OK);
					}
					if (request.serial == ref.handle_serial) {
						test.check(__LINE__, registry.production_cancel(ref).native_error == OK);
					}
					test.close_before_ack = false; // Original early-failure unit retirement owns its initial retire call.
					test.stop_before_storage();
					return;
				}
			}
			const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, test.upload_prefix ? 128u : 16u, 1, 0, 1, 1, 0 };
			GDExtensionStorageBackingV1 backing = {};
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == ERR_BUSY);
			test.rendezvous(2);
			test.check(__LINE__, registry.production_storage_create(ref, 1, &origin, &backing).native_error == OK);
			test.rendezvous(3); // Fill the real FIFO before accepting SET_CREATE.
			const GDExtensionProductionSetBindingV1 expected = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, test.upload_prefix ? 128u : 16u };
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
				const GDExtensionProductionDispatchV1 dispatch = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 2, 1, 1, 1, 1, 0, 0 };
				if (test.upload_prefix) {
					uint8_t payload[96];
					for (uint32_t i = 0; i < 96; ++i) {
						payload[i] = uint8_t(i ^ 0x5a);
					}
					const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 1, 0, 0, 96 };
					test.check(__LINE__, registry.production_prepare_upload(ref, &upload, payload, sizeof(payload)).native_error == OK);
					test.rendezvous(12); // Original RT staging pool, no upload effect yet.
				} else {
					for (uint32_t i = 0; i < request.action_count; ++i) {
						auto prepared = dispatch;
						prepared.canonical_suboperation = 10 + i;
						test.check(__LINE__, (test.public_port ? fixture.port.prepare_dispatch(ref, &prepared, nullptr, 0) : registry.production_prepare_dispatch(ref, &prepared, nullptr, 0)).native_error == OK);
					}
				}
				test.rendezvous(10); // Saturate the same RS FIFO before the first record turn.
				uint64_t control = 0;
				test.check(__LINE__, test.schedule(ref, &control) == ERR_BUSY && request.control.state == ProductionControlState::PENDING && test.receiver_calls == 0);
				GDExtensionProductionRecordReportV1 pending_record = {};
				test.check(__LINE__, test.consume(ref, control, &pending_record).native_error == ERR_BUSY && request.control.state == ProductionControlState::PENDING && pending_record.abi.abi_version == 0);
				test.rendezvous(9); // Stale queue entries cannot execute a merely Pending cell.
				test.check(__LINE__, request.control.state == ProductionControlState::PENDING && test.receiver_calls == 0);
				test.check(__LINE__, test.schedule(ref, &control) == OK);
				test.rendezvous(9);
				test.check(__LINE__, test.receiver_calls == 1 && request.control.state == ProductionControlState::RETURNED);
				GDExtensionProductionRecordReportV1 returned = {};
				test.check(__LINE__, test.consume(ref, control, &returned).native_error == OK && returned.result.native_error == OK);
				if (test.v1_compat_probe) {
					const auto append_v1 = Access::ordinal_port<GDExtensionInterfaceDeviceSessionProductionAppendV1>("device_session_production_append_v1");
					const auto prepare_v1 = Access::ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1>("device_session_production_prepare_upload_v1");
					if (append_v1 && prepare_v1) {
						GDExtensionProductionAppendV1 expired;
						std::memset(&expired, 0x5a, sizeof(expired));
						test.check(__LINE__, Access::v1_result(append_v1(test.expired_context, ref, 10, request.revision, &expired), ERR_UNAUTHORIZED, 0));
						test.check(__LINE__, expired.abi.abi_version == 0 && expired.canonical_suboperation == 0 && expired.revision == 0 && expired.state == 0);
						test.check(__LINE__, expired.recorded_bytes == 0 && expired.parent_bytes == 0 && expired.graph_fact_count == 0 && Access::v1_result(expired.cause, OK, 0));
						const uint8_t payload[4] = { 1, 2, 3, 4 };
						const GDExtensionProductionUploadV1 below_begin = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 9, 1, 0, 0, 4 };
						test.check(__LINE__, request.frozen && request.control.state == GDExtensionDeviceExit::ProductionControlState::OWNER_CONSUMED);
						test.check(__LINE__, Access::v1_result(prepare_v1(ref, &below_begin, payload, 4), ERR_BUSY, 16));
					}
				}

				if (test.closure_rollback) {
					test.finish_closure_rollback(backing, ref);
					return;
				}

				test.check(__LINE__, returned.abi.struct_size == sizeof(returned) && returned.append.abi.struct_size == sizeof(returned.append) && returned.segment.abi.struct_size == sizeof(returned.segment) && returned.report.abi.struct_size == sizeof(returned.report) && returned.flags == 7);
				GDExtensionProductionRecordReportV1 duplicate_record = {};
				test.check(__LINE__, test.consume(ref, control, &duplicate_record).native_error == ERR_DOES_NOT_EXIST && duplicate_record.abi.abi_version == 0);
				GDExtensionProductionAppendV1 expired = {};
				test.check(__LINE__, test.append(test.expired_context, ref, 10, request.revision, &expired).native_error == ERR_UNAUTHORIZED);
				test.check(__LINE__, !request.compute_list_open && !fixture.rd.compute_list.active && !request.cancelled);
				if (test.keep_list || test.budget_pause || test.upload_prefix) {
					test.recorded_segment = returned.segment;
				}
				test.check(__LINE__, test.recorded_segment.segment != 0);
				if (test.upload_prefix) {
					test.check(__LINE__, returned.pause == uint32_t(ProductionRecordPause::UPLOAD_PREFIX) && returned.append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 && returned.append.recorded_bytes == 64 && returned.append.parent_bytes == 96);
					test.check(__LINE__, test.recorded_segment.first_canonical == 10 && test.recorded_segment.end_canonical == 10 && test.recorded_segment.incomplete_upload_parent == 10);
					GDExtensionProductionReportV1 progress = {};
					auto maintenance = GDExtensionProductionServiceBudgetV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 4, 4, 0 };
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
					test.check(__LINE__, test.schedule(ref, &control) == OK);
					test.rendezvous(9);
					test.check(__LINE__, test.consume(ref, control, &returned).native_error == OK && returned.result.native_error == OK && test.receiver_calls == 2);
					test.recorded_segment = returned.segment;
					test.check(__LINE__, test.recorded_segment.first_canonical == 10 && test.recorded_segment.end_canonical == 11 && test.recorded_segment.incomplete_upload_parent == UINT64_MAX && !request.cancelled);
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
				} else if (test.budget_pause) {
					test.check(__LINE__, returned.pause == uint32_t(ProductionRecordPause::APPEND_LIMIT) && returned.append.state == GDEXTENSION_PRODUCTION_APPENDED_V1);
					test.check(__LINE__, request.next_action == 64 && test.recorded_segment.end_canonical == 74 && !request.cancelled);
					// Typed public entry must preserve the actual recorded prefix, pins,
					// current cursor and the complete original source/error cause.
					const auto release = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionReleaseV1>(GDExtension::get_interface_function("device_session_production_release_v1"));
					test.check(__LINE__, release != nullptr);
					if (release) {
						const auto zero = GDExtensionProductionServiceBudgetV1{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
						GDExtensionProductionReportV1 before = {}, rejected = {};
						const Error queried = Error(registry.production_observe(ref, &zero, &before).native_error);
						test.check(__LINE__, queried == OK || queried == ERR_BUSY);
						const uint64_t revision = request.revision, next_control = request.next_control_serial, control_serial = request.control.serial, maintenance = request.maintenance_serial;
						const uint32_t children = request.attachment->children;
						const uint32_t pins[3] = { request.resources[0].use_count, request.resources[1].use_count, request.resources[2].use_count };
						const auto cursor = fixture.rd.native_disposal_continuation;
						const bool active = fixture.rd.native_disposal_active.load();
						const auto offered = GDExtensionProductionServiceBudgetV1{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
						GDExtensionProductionServiceBudgetV1 remaining = {};
						test.check(__LINE__, release(ref, &offered, &remaining, &rejected).native_error == ERR_BUSY);
						test.check(__LINE__, request.serial == ref.handle_serial && request.next_action == 64 && !request.cancelled && !request.release_started && request.revision == revision && request.maintenance_serial == maintenance && request.attachment->children == children);
						test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && request.control.serial == control_serial && request.next_control_serial == next_control && request.resources[0].use_count == pins[0] && request.resources[1].use_count == pins[1] && request.resources[2].use_count == pins[2]);
						const auto &after_cursor = fixture.rd.native_disposal_continuation;
						test.check(__LINE__, fixture.rd.native_disposal_active.load() == active && after_cursor.serial == cursor.serial && after_cursor.production_owner == cursor.production_owner && after_cursor.phase == cursor.phase && after_cursor.target == cursor.target && after_cursor.end_phase == cursor.end_phase && after_cursor.begin_phase == cursor.begin_phase && after_cursor.slot == cursor.slot && after_cursor.slot_count == cursor.slot_count && after_cursor.command_buffer == cursor.command_buffer);
						test.check(__LINE__, rejected.service_pending == 0 && rejected.service_control_serial == 0 && remaining.controls == offered.controls && remaining.receipts == offered.receipts && remaining.passes == offered.passes);
						test.check(__LINE__, rejected.cause.native_error == before.cause.native_error && rejected.cause.reason == before.cause.reason && rejected.cause.expected == before.cause.expected && rejected.cause.actual == before.cause.actual);
					}
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
					test.check(__LINE__, test.schedule(ref, &control) == OK);
					test.rendezvous(9);
					test.check(__LINE__, test.consume(ref, control, &returned).native_error == OK && returned.result.native_error == OK && test.receiver_calls == 2);
					test.recorded_segment = returned.segment;
					test.check(__LINE__, request.next_action == 65 && test.recorded_segment.first_canonical == 74 && test.recorded_segment.end_canonical == 75 && !request.cancelled && !request.compute_list_open && !fixture.rd.compute_list.active);
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK);
				} else if (test.close_before_ack) {
					fixture.frame.leave();
					test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED);
					test.rendezvous(11); // Original unit owner accepts retirement before the actual close seal.
					test.check(__LINE__, fixture.rd.production_finalization_started.load());
					test.check(__LINE__, registry.production_close(fixture.attachment).native_error == ERR_BUSY && request.cancelled);
					{
						MutexLock lock(registry.mutex);
						registry.records[fixture.reference.slot].quiescing = true;
					}
					test.check(__LINE__, test.schedule(ref, &control) == ERR_BUSY);
					test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED);
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
					test.check(__LINE__, test.ack(ref, test.recorded_segment.segment, test.recorded_segment.segment_revision).native_error == ERR_UNAUTHORIZED); // SceneReady is installation, not the normal metadata ACK frame.
					test.check(__LINE__, owner_ack(fixture, ref, test.recorded_segment).native_error == OK && test.receiver_calls == 1);
				}
			}

			GDExtensionProductionReportV1 report = {};
			auto budget = GDExtensionProductionServiceBudgetV1{ GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
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
					if (test.mesh_directory && !test.directory_capacity_failure) {
						budget.passes = budget.receipts = test.directory_pending_checks ? 1 : 3;
					}
					const Error scheduled = Error(registry.production_observe(ref, &budget, &report).native_error);
					test.check(__LINE__, scheduled == OK || scheduled == ERR_BUSY);
					if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
						test.rendezvous(6); // Same original RS FIFO, never inline execution.
						if (request.control.state == ProductionControlState::RETURNED) {
							test.check(__LINE__, request.control.output.service_usage.receipts <= budget.receipts && request.control.output.service_usage.passes <= budget.passes);
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
				if (test.mesh_directory && !test.directory_capacity_failure) {
					test.check(__LINE__, test.directory_pending_checks > 0);
					budget.passes = budget.receipts = 1;
				}
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
			if (!test.public_port || test.close_before_ack) {
				test.check(__LINE__, registry.production_cancel(ref).native_error == OK);
			} else {
				test.check(__LINE__, !request.cancelled && request.next_action == request.action_count);
			}
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
			if (!test.public_port) {
				test.check(__LINE__, drain_request_cursor(fixture, ref, [&]() { test.rendezvous(6); }));
			}
			if (test.public_port) {
				// One explicit Core maintenance pass/receipt is authorized per slice.
				// This is not the Runtime's outer service_children pass or a grant.
				const uint64_t limit = 64 + uint64_t(request.capacity.native_memberships) * 4 + uint64_t(request.capacity.graph_memberships) * 16 + uint64_t(request.resource_count) * 8;
				// A real native consumer retains this same request arena even after
				// Complete/ACK and storage return. No release preflight may unpin it.
				const uint64_t pinned_serial = request.serial, pinned_revision = request.revision, pinned_next = request.next_control_serial;
				const uint32_t pinned_unit_uses = request.resources[0].use_count;
				const auto blocked_budget = GDExtensionProductionServiceBudgetV1{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				test.check(__LINE__, registry.production_output_unpin(test.output_pin) == ERR_UNAUTHORIZED && !test.output_pin.is_empty());
				if (test.output_borrow) {
					ProductionOutputPin wrong_thread;
					test.check(__LINE__, registry.production_output_pin_borrow(test.output_pin, fixture.rd, wrong_thread) == ERR_UNAUTHORIZED && wrong_thread.is_empty());
				}
				test.check(__LINE__, fixture.port.release(ref, &blocked_budget, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
				test.check(__LINE__, request.serial == pinned_serial && request.revision == pinned_revision && request.next_control_serial == pinned_next && request.resources[0].use_count == pinned_unit_uses && request.control.state == ProductionControlState::OWNER_CONSUMED && test.release_report.service_pending == 0);
				if (test.close_before_ack) {
					test.check(__LINE__, registry.production_close(fixture.attachment).native_error == ERR_BUSY && !test.output_pin.is_empty());
				}
				test.rendezvous(13); // Actual RT consumer's explicit final use return precedes request RELEASE.
				test.check(__LINE__, test.output_pin.is_empty());
				const auto release_zero = GDExtensionProductionServiceBudgetV1{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
				const uint64_t initial_next = request.next_control_serial;
				const uint32_t initial_attempts = fixture.driver.release_attempts;
				const uint32_t initial_issues = fixture.driver.issues, initial_polls = fixture.driver.polls, initial_retains = fixture.driver.retains;
				test.check(__LINE__, fixture.port.release(ref, &release_zero, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
				test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && request.next_control_serial == initial_next && test.release_report.service_pending == 0 && test.release_report.service_control_serial == 0 && fixture.driver.release_attempts == initial_attempts && fixture.driver.issues == initial_issues && fixture.driver.polls == initial_polls && fixture.driver.retains == initial_retains);
				test.rendezvous(10); // Fill first; each Pending deadline is anchored afterwards.
				GDExtensionProductionServiceBudgetV1 pending_budget = {};
				uint64_t pending_serial = 0;
				for (uint32_t admission = 0; admission < 8 && !pending_serial; ++admission) {
					test.release_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 100000, 1, 1, 1, 0 };
					pending_budget = test.release_budget;
					test.check(__LINE__, fixture.port.release(ref, &test.release_budget, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
					if (test.release_report.service_pending) {
						pending_serial = test.release_report.service_control_serial;
						test.check(__LINE__, pending_serial && request.control.state == ProductionControlState::PENDING);
					} else {
						// Descheduling before admission may expire an offered slice.
						test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && request.next_control_serial == initial_next && test.release_report.service_control_serial == 0);
					}
				}
				test.check(__LINE__, pending_serial != 0);
				if (pending_serial) {
					auto sibling_offer = pending_budget;
					sibling_offer.deadline_micros += 10000000;
					test.check(__LINE__, fixture.port.release(ref, &sibling_offer, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
					test.check(__LINE__, test.release_report.service_control_serial == pending_serial && test.release_remaining.deadline_micros == pending_budget.deadline_micros);
					if (test.release_report.service_pending) {
						test.check(__LINE__, request.control.state == ProductionControlState::PENDING && test.release_remaining.controls == 0 && test.release_remaining.passes == 0 && test.release_remaining.receipts == 0);
						// delay_usec may round down to milliseconds on Windows.
						// Expiry is the actual monotonic-clock predicate, not a sleep return.
						while (OS::get_singleton()->get_ticks_usec() <= pending_budget.deadline_micros) {
							OS::get_singleton()->delay_usec(1000);
						}
						test.check(__LINE__, fixture.port.release(ref, &sibling_offer, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
					}
					// A slow retry may have already consumed the expired Pending cell.
					// Both paths must settle the same unpublished old serial, never renew it.
					test.check(__LINE__, test.release_report.service_control_serial == pending_serial && test.release_report.service_pending == 0 && request.control.state == ProductionControlState::OWNER_CONSUMED && request.next_control_serial == initial_next + 1);
					test.check(__LINE__, test.release_remaining.deadline_micros == pending_budget.deadline_micros && test.release_remaining.controls == 1 && test.release_remaining.passes == 1 && test.release_remaining.receipts == 1 && fixture.driver.release_attempts == initial_attempts && fixture.driver.issues == initial_issues && fixture.driver.polls == initial_polls && fixture.driver.retains == initial_retains);
				}
				test.rendezvous(9); // Only stale commands existed; no RELEASE was ever published.
				fixture.driver.release_busy_remaining = fixture.driver.release_error_remaining = 1;
				bool expired_returned = false, saw_driver_busy = false, saw_driver_error = false;
				const uint32_t held_children = request.attachment->children;
				const uint32_t held_code_children = request.attachment->holder->production_children;
				uint64_t turns = 0;
				for (; turns < limit && request.serial; ++turns) {
					test.release_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + (expired_returned ? 10000000 : 250000), 1, 1, 1, 0 };
					const auto original = test.release_budget;
					test.public_release_result = fixture.port.release(ref, &test.release_budget, &test.release_remaining, &test.release_report);
					if (!test.release_report.service_pending) {
						// A scheduler pause before API admission may expire the offered
						// slice. It must be a zero-effect query, not a queued turn.
						test.check(__LINE__, test.public_release_result.native_error == ERR_BUSY && request.control.state == ProductionControlState::OWNER_CONSUMED && test.release_report.service_control_serial == 0);
						continue;
					}
					test.check(__LINE__, test.public_release_result.native_error == ERR_BUSY && test.release_report.service_pending == 1);
					test.release_control = test.release_report.service_control_serial;
					test.check(__LINE__, test.release_control && test.release_remaining.controls == 0 && test.release_remaining.receipts == 0 && test.release_remaining.passes == 0);

					test.rendezvous(6);
					if (request.control.state == ProductionControlState::PENDING) {
						// Genuine queue contention may have kept the original cell
						// unpublished. Retry or settle expiry without inventing RT usage.
						test.check(__LINE__, fixture.port.release(ref, &test.release_budget, &test.release_remaining, &test.release_report).native_error == ERR_BUSY);
						if (!test.release_report.service_pending) {
							test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && test.release_report.service_control_serial == test.release_control && test.release_remaining.deadline_micros == original.deadline_micros && test.release_remaining.controls == 1 && test.release_remaining.passes == 1 && test.release_remaining.receipts == 1);
							continue;
						}
						test.rendezvous(6);
					}
					test.check(__LINE__, request.control.state == ProductionControlState::RETURNED);
					const auto rt_usage = request.control.output.service_usage;
					test.check(__LINE__, rt_usage.controls == 1 && rt_usage.passes <= 1 && rt_usage.receipts <= 1);
					const uint32_t releases_before_owner = fixture.driver.releases, attempts_before_owner = fixture.driver.release_attempts;
					const bool expire_this_return = !expired_returned;
					if (expire_this_return) {
						// Expire a real returned turn, then offer a fresh deadline. The
						// response must still belong to the old slice, with no owner driver work.
						while (OS::get_singleton()->get_ticks_usec() <= original.deadline_micros) {
							OS::get_singleton()->delay_usec(1000);
						}
						test.release_budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
					}
					test.metadata_release = true;
					if (test.close_before_ack) {
						GDExtensionManager::get_singleton()->frame(); // Fresh original owner metadata frame consumes this exact return.
					} else {
						test.check(__LINE__, !request.cancelled && request.next_action == request.action_count && RegistrationContext::current != nullptr);
						test.public_release_result = fixture.port.release(ref, &test.release_budget, &test.release_remaining, &test.release_report);
					}
					if (expire_this_return || rt_usage.passes == 1 || rt_usage.receipts == 1) {
						test.check(__LINE__, fixture.driver.releases == releases_before_owner && fixture.driver.release_attempts == attempts_before_owner);
					}
					test.check(__LINE__, !Registry::is_production_metadata_entry());
					test.check(__LINE__, test.release_report.service_control_serial == test.release_control && test.release_report.service_pending == 0);
					test.check(__LINE__, test.release_remaining.deadline_micros == original.deadline_micros && test.release_remaining.abi.struct_size == sizeof(test.release_remaining));
					test.check(__LINE__, test.release_remaining.controls == 0 && test.release_remaining.receipts <= 1 && test.release_remaining.passes <= 1);
					if (rt_usage.passes == 1) {
						test.check(__LINE__, test.release_remaining.passes == 0); // RT spent the only pass; owner cannot receive another.
					}
					expired_returned |= expire_this_return;
					saw_driver_busy |= fixture.driver.release_busy_remaining == 0;
					saw_driver_error |= test.public_release_result.native_error == FAILED && fixture.driver.release_error_remaining == 0;
					test.check(__LINE__, test.public_release_result.native_error == OK || test.public_release_result.native_error == ERR_BUSY || test.public_release_result.native_error == FAILED);
					if (test.public_release_result.native_error != OK) {
						test.check(__LINE__, test.release_report.cause.native_error == test.public_release_result.native_error && request.serial == ref.handle_serial && request.control.state == ProductionControlState::OWNER_CONSUMED);
						test.check(__LINE__, request.attachment->children == held_children && request.attachment->holder->production_children == held_code_children && request.resources[0].use_count == 2);
					}
				}
				INFO("Public RELEASE turns=", turns, "/", limit, " error=", test.public_release_result.native_error, " retains=", fixture.driver.retains, " releases=", fixture.driver.releases);
				test.check(__LINE__, test.public_release_result.native_error == OK && request.serial == 0 && fixture.driver.retains == fixture.driver.releases);
				test.check(__LINE__, expired_returned && saw_driver_busy && saw_driver_error && fixture.driver.release_busy_remaining == 0 && fixture.driver.release_error_remaining == 0 && fixture.driver.release_attempts == fixture.driver.releases + 2);
			} else if (test.output_closure) {
				test.rendezvous(14);
				GDExtensionProductionReportV1 closure_report = {};
				const uint64_t next_control = request.next_control_serial;
				const uint32_t before_frees = fixture.driver.auxiliary_buffer_frees + fixture.driver.auxiliary_set_frees + fixture.driver.auxiliary_shader_frees + fixture.driver.auxiliary_pipeline_frees;
				const GDExtensionProductionServiceBudgetV1 closure_zero = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
				const GDExtensionProductionServiceBudgetV1 expired = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 1, 1, 1, 1, 0 };
				const Error queried = Error(registry.production_observe(ref, &closure_zero, &closure_report).native_error);
				test.check(__LINE__, queried == OK || queried == ERR_BUSY);
				const Error expired_query = Error(registry.production_observe(ref, &expired, &closure_report).native_error);
				test.check(__LINE__, expired_query == OK || expired_query == ERR_BUSY);
				test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && request.next_control_serial == next_control && before_frees == fixture.driver.auxiliary_buffer_frees + fixture.driver.auxiliary_set_frees + fixture.driver.auxiliary_shader_frees + fixture.driver.auxiliary_pipeline_frees);
				ProductionDisposalObservation wrong_thread;
				test.check(__LINE__, fixture.rd._production_output_hold_returned(test.output_pin, test.owned_group, wrong_thread) == ERR_UNAUTHORIZED);

				if (!test.closure_retired) {
					// A later legal setter invalidates the capture-time callback proof.
					// The old implementation only drops its first buffer counter in this
					// bounded turn, so this RED retains a safe real cleanup path.
					const GDExtensionProductionServiceBudgetV1 callback_slice = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
					// Isolate this existing one-pass callback negative from private
					// directory work; the original native maintenance must reject it.
					test.directory_service_paused = true;
					const Error callback_scheduled = Error(registry.production_observe(ref, &callback_slice, &closure_report).native_error);
					test.check(__LINE__, callback_scheduled == ERR_BUSY && request.control.state == ProductionControlState::QUEUED);
					test.rendezvous(6);
					test.check(__LINE__, request.control.state == ProductionControlState::RETURNED && request.control.output.result.native_error == ERR_UNAVAILABLE && request.control.output.service_usage.passes <= 1 && request.control.output.service_usage.receipts <= 1);
					test.check(__LINE__, registry.production_observe(ref, &callback_slice, &closure_report).native_error == ERR_UNAVAILABLE && request.control.state == ProductionControlState::OWNER_CONSUMED);
					test.directory_service_paused = false;
					test.rendezvous(16); // Actual null replacement permits the original retry.
				}
				const uint64_t limit = 64 + uint64_t(request.capacity.resource_uses) * 16 + uint64_t(request.capacity.native_memberships) * 4;
				uint64_t turn = 0;
				for (; turn < limit && !test.groups_settled; ++turn) {
					const GDExtensionProductionServiceBudgetV1 slice = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
					const Error scheduled = Error(registry.production_observe(ref, &slice, &closure_report).native_error);
					test.check(__LINE__, scheduled == OK || scheduled == ERR_BUSY);
					if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
						test.rendezvous(6);
						if (request.control.state == ProductionControlState::RETURNED) {
							test.check(__LINE__, request.control.output.service_usage.passes <= 1 && request.control.output.service_usage.receipts <= 1);
						}
						const Error consumed = Error(registry.production_observe(ref, &slice, &closure_report).native_error);
						test.check(__LINE__, consumed == OK || consumed == ERR_BUSY);
					}
					test.rendezvous(15);
				}
				INFO("Closure turns=", turn, "/", limit);
				test.check(__LINE__, test.groups_settled && request.control.state == ProductionControlState::OWNER_CONSUMED);
				test.check(__LINE__, drain_request_cursor(fixture, ref, [&]() { test.rendezvous(6); }));
				test.rendezvous(13);
				test.check(__LINE__, test.output_pin.is_empty());
				test.check(__LINE__, registry.production_release(ref).native_error == ERR_BUSY);
				for (uint32_t release_turn = 0; release_turn <= request.native_count && request.serial; ++release_turn) {
					budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
					test.check(__LINE__, registry.production_control_accept(ref, ProductionControlKind::RELEASE, &test.release_control, &budget) == OK);
					test.check(__LINE__, registry.production_control_schedule(ref, test.release_control) == OK);
					test.rendezvous(6);
					test.check(__LINE__, registry.production_control_consume(ref, test.release_control, &test.release_output) == OK);
				}
				test.check(__LINE__, request.serial == 0 && fixture.driver.auxiliary_buffer_frees == 1 && fixture.driver.auxiliary_set_frees == 1);
				if (test.closure_retired) {
					test.check(__LINE__, fixture.driver.auxiliary_shader_frees == 1 && fixture.driver.auxiliary_pipeline_frees == 1);
				}

			} else if (test.record_window) {
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
			if (test.output_closure) {
				if (test.closure_retired) {
					test.check(__LINE__, test.nested_disposal_probes > 0);
				}
				fixture.driver.disposal_probe = nullptr;
				fixture.driver.disposal_probe_data = nullptr;
			}
			if (test.mesh_directory) {
				test.rendezvous(17); // Original ordinary Mesh owner frees after request release.
			}
			test.rendezvous(7); // Actual unit dependency returns before owner closes.
			if (test.output_closure) {
				test.check(__LINE__, fixture.driver.auxiliary_shader_frees == 1 && fixture.driver.auxiliary_pipeline_frees == 1 && fixture.driver.auxiliary_buffer_frees == 1 && fixture.driver.auxiliary_set_frees == 1);
			}
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
	static void output_pin_borrow_checks(SingletonEnrollment &fixture, SplitSet &test) {
		using namespace GDExtensionDeviceExit;
		auto &registry = fixture.registry;
		auto &request = *fixture.request;
		auto &device = fixture.rd;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		const uint64_t revision = request.revision, graphs = request.graph_count, natives = request.native_count;
		const uint32_t issues = fixture.driver.issues, polls = fixture.driver.polls, retains = fixture.driver.retains, releases = fixture.driver.releases;
		CHECK_FALSE(test.output_pin.is_empty());
		CHECK_FALSE(test.borrowed_pin.is_empty());
		if (test.borrowed_pin.is_empty()) {
			return; // A refused borrow still returns the true original pin below.
		}
		ProductionOutputPin refused;
		uint64_t serial = request.next_output_pin_serial;
		if (request.cancelled || request.attachment->closing) {
			// Closing/cancellation is from the original owner path, not a fake slot.
			CHECK(registry.production_output_pin_borrow(test.output_pin, device, refused) == ERR_BUSY);
			CHECK((refused.is_empty() && request.next_output_pin_serial == serial));
			CHECK(registry.production_output_unpin(test.borrowed_pin) == OK);
			return;
		}
		for (auto mode : { RendererRD::ExternalCustodyTestMode::VIEW_RETURN, RendererRD::ExternalCustodyTestMode::READ_CANCEL, RendererRD::ExternalCustodyTestMode::READ_UNKNOWN }) {
			ProductionOutputPin custody_pin;
			REQUIRE(registry.production_output_pin_borrow(test.output_pin, device, custody_pin) == OK);
			RendererRD::external_custody_issued_pin_test(registry, ref, custody_pin, mode);
			if (mode == RendererRD::ExternalCustodyTestMode::READ_UNKNOWN) {
				CHECK(registry.production_output_pin_matches(custody_pin, ref, device));
				CHECK(registry.production_output_unpin(custody_pin) == OK);
			} else {
				CHECK(custody_pin.is_empty());
			}
		}
		serial = request.next_output_pin_serial; // New genuine borrows above advanced the allocator.
		CHECK(registry.production_output_pin_borrow(test.output_pin, device, test.output_pin) == ERR_ALREADY_IN_USE);
		CHECK(registry.production_output_pin_borrow(test.output_pin, device, test.borrowed_pin) == ERR_ALREADY_IN_USE);
		ProductionOutputPin empty;
		CHECK(registry.production_output_pin_borrow(empty, device, refused) == ERR_UNAUTHORIZED);
		static Registry foreign_registry;
		CHECK(foreign_registry.production_output_pin_borrow(test.output_pin, device, refused) == ERR_UNAUTHORIZED);
		// One-field negative corruption at the original native owner; no alternate
		// issuer, fabricated positive pin, or device rebind is constructed.
		auto &native = registry.device_records[request.attachment->device];
		const uint64_t epoch = native.epoch;
		native.epoch = epoch + 1;
		CHECK(registry.production_output_pin_borrow(test.output_pin, device, refused) == ERR_UNAUTHORIZED);
		native.epoch = epoch;
		const bool live = native.live;
		native.live = false;
		CHECK(registry.production_output_pin_borrow(test.output_pin, device, refused) == ERR_UNAVAILABLE);
		native.live = live;
		CHECK((refused.is_empty() && request.next_output_pin_serial == serial));
		CHECK(registry.production_output_pin_matches(test.borrowed_pin, ref, device));
		RendererRD::external_custody_issued_pin_test(registry, ref, test.output_pin, RendererRD::ExternalCustodyTestMode::SLOT_RETURN);
		CHECK(test.output_pin.is_empty());
		CHECK((test.output_pin.is_empty() && !test.borrowed_pin.is_empty()));
		{
			MutexLock lock(registry.mutex);
			CHECK(registry.production_release_locked(request, &device, false).native_error == ERR_BUSY);
			CHECK((request.serial == ref.handle_serial && !request.release_started));
		}
		// Every success occupies a distinct original prequoted use cell. Finite
		// exhaustion is a clean refusal, with no allocation or native operation.
		ProductionOutputPin extra[8];
		uint32_t count = 0;
		Error result = OK;
		while (count < 8) {
			result = registry.production_output_pin_borrow(test.borrowed_pin, device, extra[count]);
			if (result != OK) {
				break;
			}
			CHECK(registry.production_output_pin_matches(extra[count], ref, device));
			++count;
		}
		CHECK((count > 0 && count < 8 && result == ERR_OUT_OF_MEMORY));
		if (count) {
			ProductionOutputPin moved(std::move(extra[0]));
			CHECK((extra[0].is_empty() && registry.production_output_pin_matches(moved, ref, device)));
			CHECK(registry.production_output_pin_borrow(extra[0], device, refused) == ERR_UNAUTHORIZED);
			extra[0] = std::move(moved);
			CHECK((moved.is_empty() && registry.production_output_pin_matches(extra[0], ref, device)));
		}
		const uint64_t exhausted_serial = request.next_output_pin_serial;
		CHECK(registry.production_output_pin_borrow(test.borrowed_pin, device, refused) == ERR_OUT_OF_MEMORY);
		CHECK((refused.is_empty() && request.next_output_pin_serial == exhausted_serial));
		for (uint32_t i = 0; i < count; ++i) {
			CHECK(registry.production_output_unpin(extra[i]) == OK);
		}
		// The real allocator's exhaustion boundary is sticky, never wrapped or
		// reset for another positive. Existing live custody can still return.
		request.next_output_pin_serial = UINT64_MAX;
		ProductionOutputPin last;
		CHECK(registry.production_output_pin_borrow(test.borrowed_pin, device, last) == OK);
		CHECK(request.next_output_pin_serial == 0);
		CHECK(registry.production_output_pin_borrow(test.borrowed_pin, device, refused) == ERR_PARAMETER_RANGE_ERROR);
		CHECK((refused.is_empty() && request.next_output_pin_serial == 0));
		CHECK(registry.production_output_unpin(last) == OK);
		CHECK(registry.production_output_unpin(test.borrowed_pin) == OK);
		CHECK(registry.production_output_pin_borrow(test.borrowed_pin, device, refused) == ERR_UNAUTHORIZED);
		CHECK((test.borrowed_pin.is_empty() && refused.is_empty()));
		CHECK((request.revision == revision && request.graph_count == graphs && request.native_count == natives));
		CHECK((fixture.driver.issues == issues && fixture.driver.polls == polls && fixture.driver.retains == retains && fixture.driver.releases == releases));
	}
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
	static void singleton_split_owner_set(bool record_window = false, bool keep_list = false, bool close_before_ack = false, bool budget_pause = false, bool upload_prefix = false, bool public_port = false, bool output_closure = false, bool closure_retired = false, bool closure_rollback = false, bool output_borrow = false, bool mesh_directory = false, bool directory_capacity_failure = false, bool directory_external = false, bool v1_compat_probe = false) {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		SplitSet test;
		test.v1_compat_probe = v1_compat_probe;
		test.record_window = record_window;
		test.keep_list = keep_list;
		test.close_before_ack = close_before_ack;
		test.budget_pause = budget_pause;
		test.upload_prefix = upload_prefix;
		test.public_port = public_port;
		test.output_closure = output_closure;
		test.closure_retired = closure_retired;
		test.closure_rollback = closure_rollback;
		test.mesh_directory = mesh_directory;
		test.directory_capacity_failure = directory_capacity_failure;
		test.directory_external = directory_external;
		test.output_borrow = output_borrow;
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
			} else if (test.phase == 14) {
				CHECK(fixture.rd._production_output_hold_begin_return(test.output_pin, test.borrowed_group) == OK);
				CHECK(fixture.rd._production_output_hold_begin_return(test.output_pin, test.owned_group) == OK);
				CHECK(fixture.rd._production_output_hold_begin_return(test.output_pin, test.qualified_group) == OK);
				CHECK(fixture.registry.production_output_unpin(test.output_pin) == ERR_BUSY);
				if (test.mesh_directory && !test.directory_capacity_failure) {
					CHECK(fixture.rd._production_output_hold_begin_return(test.output_pin, test.directory_selected_group) == OK);
					CHECK(fixture.rd._production_output_hold_begin_return(test.output_pin, test.neighbor_selected_group) == OK);
					GDExtensionDeviceExit::ProductionServiceUsage copied_usage = {};
					const GDExtensionProductionServiceBudgetV1 copied_budget = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
					CHECK(RendererRD::MeshStorage::_native_hold_return_directory(test.output_pin, test.directory_hold, copied_budget, copied_usage) == ERR_UNAUTHORIZED);
					CHECK((copied_usage.passes == 0 && copied_usage.receipts == 0));
					test.directory_return_started = true;
				}
				if (!test.closure_rollback && !test.closure_retired) {
					CHECK((fixture.rd._native_immutable_counter(test.auxiliary_set) && *fixture.rd._native_immutable_counter(test.auxiliary_set) == 1));
					const auto foreign_callback = +[](void *data) { ++static_cast<SplitSet *>(data)->foreign_callback_calls; };
					fixture.rd.uniform_set_set_invalidation_callback(test.auxiliary_set, foreign_callback, &test);
					const auto *actual_set = fixture.rd.uniform_set_owner.get_or_null(test.auxiliary_set);
					CHECK((actual_set && !actual_set->native_immutable_retire_requested && actual_set->invalidated_callback == foreign_callback && actual_set->invalidated_callback_userdata == &test));
				}

			} else if (test.phase == 16) {
				CHECK((fixture.driver.auxiliary_buffer_frees == 0 && fixture.driver.auxiliary_set_frees == 0 && test.foreign_callback_calls == 0));
				CHECK((fixture.rd._native_immutable_counter(test.auxiliary_buffer) && *fixture.rd._native_immutable_counter(test.auxiliary_buffer) == 1));
				CHECK((fixture.rd._native_immutable_counter(test.auxiliary_set) && *fixture.rd._native_immutable_counter(test.auxiliary_set) == 1));
				fixture.rd.uniform_set_set_invalidation_callback(test.auxiliary_set, nullptr, nullptr);

			} else if (test.phase == 15) {
				GDExtensionDeviceExit::ProductionDisposalObservation borrowed, owned, qualified;
				CHECK(fixture.rd._production_output_hold_returned(test.output_pin, test.borrowed_group, borrowed) == OK);
				CHECK(fixture.rd._production_output_hold_returned(test.output_pin, test.owned_group, owned) == OK);
				CHECK(fixture.rd._production_output_hold_returned(test.output_pin, test.qualified_group, qualified) == OK);
				test.groups_settled = borrowed.settled && owned.settled && qualified.settled;
				if (test.mesh_directory && !test.directory_capacity_failure) {
					GDExtensionDeviceExit::ProductionDisposalObservation selected;
					CHECK(fixture.rd._production_output_hold_returned(test.output_pin, test.directory_selected_group, selected) == OK);
					test.groups_settled &= test.mesh_directory_returned && test.directory_neighbor_returned && test.directory_neighbor_metadata && selected.settled;
					CHECK(fixture.driver.mesh_directory_buffer_frees <= 1);
				}
				if (test.groups_settled) {
					// The unrecorded prefix returns all four qualified borrows before
					// its original storage retirement; this group owns no physical free.
					CHECK((qualified.borrowed_detached == (test.closure_rollback ? 4u : 2u) && qualified.physical_returned == (test.closure_rollback ? 0u : 2u) && !qualified.unknown));
					CHECK((owned.physical_returned == (test.closure_rollback ? 1u : 2u) && owned.uncreated == (test.closure_rollback ? 1u : 0u) && owned.borrowed_detached == 0 && !owned.unknown));
					CHECK(borrowed.physical_returned == (test.closure_rollback ? 0u : (test.closure_retired ? 4u : 2u)));
					CHECK(borrowed.borrowed_detached == (test.closure_retired ? 0u : 2u));
					CHECK((fixture.driver.auxiliary_buffer_frees == 1 && fixture.driver.auxiliary_set_frees == (test.closure_rollback ? 0u : 1u)));
					NativeDisposalWitness::Report buffer_report, set_report;
					CHECK(fixture.rd._native_disposal_observe(test.auxiliary_buffer_witness, buffer_report) == OK);
					CHECK(buffer_report.state == NativeDisposalWitness::State::RETURNED);
					if (!test.closure_rollback) {
						CHECK(fixture.rd._native_disposal_observe(test.auxiliary_set_witness, set_report) == OK);
						CHECK(set_report.state == NativeDisposalWitness::State::RETURNED);
					}
				}

			} else if (test.phase == 13) {
				GDExtensionDeviceExit::ProductionOutputPin expired_refused;
				const uint64_t before_pin_serial = fixture.request->next_output_pin_serial;
				CHECK(fixture.registry.production_output_pin(test.expired_context, { fixture.instance, fixture.request->serial }, fixture.rd, expired_refused) == ERR_UNAUTHORIZED);
				CHECK(expired_refused.is_empty());
				CHECK(fixture.registry.production_output_pin_current(fixture.rd, expired_refused) == ERR_UNAUTHORIZED);
				CHECK(expired_refused.is_empty());
				CHECK(fixture.registry.production_output_pin_current(fixture.rd, test.output_pin) == ERR_ALREADY_IN_USE);
				CHECK(fixture.registry.production_output_pin_matches(test.output_pin, { fixture.instance, fixture.request->serial }, fixture.rd));
				CHECK(fixture.request->next_output_pin_serial == before_pin_serial);
				CHECK(fixture.registry.production_output_pin(test.expired_context, { fixture.instance, fixture.request->serial }, fixture.rd, test.output_pin) == ERR_ALREADY_IN_USE);
				if (test.output_closure && !test.closure_retired) {
					CHECK((fixture.driver.auxiliary_shader_frees == 0 && fixture.driver.auxiliary_pipeline_frees == 0));
					fixture.rd.free_rid(test.auxiliary_shader);
				}

				if (test.mesh_directory && !test.directory_capacity_failure) {
					CHECK(test.mesh_directory_returned);
					CHECK(test.retirement_auth_checked);
					CHECK((test.directory_component_turn >= 3 && test.directory_marked && test.directory_premature_finish_checks == 1));
					CHECK(RendererRD::MeshStorage::_native_hold_begin_return(test.directory_hold) == OK);
					CHECK(RendererRD::MeshStorage::_native_hold_finish_return(test.directory_hold) == OK);
				}
				if (test.output_borrow) {
					output_pin_borrow_checks(fixture, test);
				}
				if (!test.output_pin.is_empty()) {
					CHECK(fixture.registry.production_output_unpin(test.output_pin) == OK);
				}
				CHECK(test.output_pin.is_empty());
				CHECK(fixture.registry.production_output_unpin(test.output_pin) == ERR_DOES_NOT_EXIST);
			} else if (test.phase == 17) {
				CHECK(fixture.request->serial == 0);
				if (test.directory_capacity_failure) {
					CHECK(RendererRD::MeshStorage::mesh_surface_hold_release(test.directory_hold) == OK);
				}
				if (test.directory_capacity_failure) {
					CHECK(RendererRD::MeshStorage::mesh_surface_hold_release(test.directory_neighbor) == OK);
				}
				for (uint32_t f = 0; f < fixture.rd.frames.size(); ++f) {
					fixture.rd._free_pending_resources(f);
				}
				CHECK(fixture.driver.mesh_directory_buffer_frees == 1);
				CHECK_FALSE(fixture.rd.vertex_array_owner.owns(test.directory_owned_roots[0]));
				if (test.directory_external) {
					// Only original Surface-owned Version/LOD objects may retire.
					CHECK_FALSE(fixture.rd.index_buffer_owner.owns(test.directory_owned_roots[1]));
					CHECK_FALSE(fixture.rd.index_array_owner.owns(test.directory_owned_roots[2]));
					// Mutation controls preserve the failed assertions above, then
					// clean any still-live fixture-owned roots through ordinary RD.
					if (fixture.rd.vertex_array_owner.owns(test.directory_owned_roots[0])) {
						fixture.rd.free_rid(test.directory_owned_roots[0]);
					}
					if (fixture.rd.index_array_owner.owns(test.directory_owned_roots[2])) {
						fixture.rd.free_rid(test.directory_owned_roots[2]);
					}
					if (fixture.rd.index_buffer_owner.owns(test.directory_owned_roots[1])) {
						fixture.rd.free_rid(test.directory_owned_roots[1]);
					}
					CHECK(fixture.rd.vertex_buffer_owner.owns(test.directory_external_roots[0]));
					CHECK(fixture.rd.vertex_buffer_owner.owns(test.directory_external_roots[1]));
					CHECK(fixture.rd.index_buffer_owner.owns(test.directory_external_roots[2]));
					CHECK(fixture.rd.index_array_owner.owns(test.directory_external_roots[3]));
					for (uint32_t i = 0; i < 4; ++i) {
						fixture.rd.free_rid(test.directory_external_roots[3 - i]);
					}
					for (uint32_t f = 0; f < fixture.rd.frames.size(); ++f) {
						fixture.rd._free_pending_resources(f);
					}
					CHECK(fixture.driver.mesh_directory_buffer_frees == 4);
				}
				CHECK_FALSE(fixture.driver.mesh_directory_buffer_live);
				SplitSet::directory_component = nullptr;
				fixture.rd.vertex_formats.clear();
				fixture.rd.vertex_format_cache.clear();
				memfree(test.directory_mesh.surfaces);
				test.directory_mesh.surfaces = nullptr;
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
				struct ComponentHookScope {
					decltype(GDExtensionDeviceExit::test_production_before_maintenance) previous = GDExtensionDeviceExit::test_production_before_maintenance;
					~ComponentHookScope() { GDExtensionDeviceExit::test_production_before_maintenance = previous; }
				} component_hook_scope;
				if (test.mesh_directory && !test.directory_capacity_failure) {
					GDExtensionDeviceExit::test_production_before_maintenance = &SplitSet::directory_service;
				}
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
			GDExtensionProductionServiceBudgetV1 zero = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
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
			GDExtensionProductionServiceBudgetV1 public_budget = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
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
		GDExtensionProductionCapacityV1 whole = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
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
		const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 4 };
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
		const GDExtensionProductionCapacityV1 capacity = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
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
		const GDExtensionProductionCapacityV1 whole = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
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
		const GDExtensionProductionCapacityV1 whole = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
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
		const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 4 };
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
		request.operation.canonical_begin = 10;
		request.operation.canonical_end = 12;
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
			actions[i].declaration.action_ordinal = i;
			actions[i].declaration.canonical_suboperation = i + 10;
			actions[i].declaration.action_kind = GDEXTENSION_PRODUCTION_UPLOAD_V1;
			actions[i].declaration.use_count = 1;
		}
		const uint8_t original[4] = { 2, 4, 6, 8 }, changed[4] = { 2, 4, 6, 9 };
		GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 4 };
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
	template <class T>
	static T ordinal_port(const char *name) {
		const auto entry = reinterpret_cast<T>(GDExtension::get_interface_function(name));
		CHECK_MESSAGE(entry != nullptr, "Missing installed ordinal procedure: ", name);
		return entry;
	}
	static RenderingServerDefault *ordinal_server() {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return nullptr;
		}
		server->sync();
		return server;
	}
	static Error ordinal_initialize(SingletonEnrollment &fixture, RenderingServerDefault *server, const OrdinalPlan &plan, GDExtensionDeviceExit::ProductionRecordReceiver receiver = nullptr, void *userdata = nullptr) {
		if (!receiver) {
			receiver = +[](void *, GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {};
		}
		const Error device = fixture.prepare_device(server);
		return device == OK ? fixture.initialize_owner(1, false, false, receiver, userdata, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan) : device;
	}
	static void ordinal_rows_and_prepare() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto upload = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		const auto readback = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!upload || !readback || !observe) {
			return;
		}
		{
			OrdinalPlan plan;
			SingletonEnrollment fixture;
			if (!check_step(ordinal_initialize(fixture, server, plan) == OK)) {
				return;
			}
			auto &request = *fixture.request;
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
			CHECK(request.contract_version == 2);
			CHECK(request.action_count == 3);
			CHECK(request.operation.canonical_begin == 10);
			CHECK(request.operation.canonical_end == 13);
			CHECK(request.actions[0].declaration.action_kind == GDEXTENSION_PRODUCTION_UPLOAD_V1);
			CHECK(request.actions[1].declaration.action_kind == GDEXTENSION_PRODUCTION_DISPATCH_V1);
			CHECK(request.actions[2].declaration.action_kind == GDEXTENSION_PRODUCTION_READBACK_V1);
			for (uint32_t i = 0; i < 3; ++i) {
				CHECK(request.actions[i].declaration.action_ordinal == i);
				CHECK(request.actions[i].declaration.canonical_suboperation == 11);
			}
			CHECK(request.readbacks[0].declaration.producer_action_ordinal == 1);
			CHECK(request.readbacks[0].declaration.copy_action_ordinal == 2);
			const uint8_t payload[4] = { 1, 3, 5, 7 };
			GDExtensionProductionUploadV2 prepared = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 11, 7, 0, 1, 0, 0, 4 };
			++prepared.source_recipe_substep;
			CHECK(upload(ref, &prepared, payload, sizeof(payload)).native_error == ERR_INVALID_DATA);
			CHECK_FALSE(request.actions[0].prepared);
			--prepared.source_recipe_substep;
			CHECK(upload(ref, &prepared, payload, sizeof(payload)).native_error == OK);
			CHECK(readback(ref, 1, 0).native_error != OK); // Dispatch cannot masquerade as the copy row.
			CHECK(readback(ref, 2, 0).native_error == OK);
			const auto no_service = GDExtensionProductionServiceBudgetV1{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
			GDExtensionProductionReportV2 report = {};
			CHECK(observe(ref, &no_service, &report).native_error == OK);
			CHECK(report.action_count == 3);
			CHECK(report.recorded_action_end == 0);
			CHECK(report.operation.canonical_begin == 10);
			CHECK(report.operation.canonical_end == 13);
			CHECK_FALSE(request.frozen); // No native unit is fabricated/imported by this mapping test.
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
		}
		{
			// Same kind and canonical: a first-canonical lookup would select row0
			// when preparing row1 and fail the valid substep/destination below.
			OrdinalPlan plan;
			plan.action_count = 2;
			plan.readback_count = 0;
			plan.actions[1] = plan.actions[0];
			plan.actions[1].action_ordinal = 1;
			plan.actions[1].source_recipe_substep = 2;
			SingletonEnrollment fixture;
			if (!check_step(ordinal_initialize(fixture, server, plan) == OK)) {
				return;
			}
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, fixture.request->serial };
			const uint8_t payload[4] = { 2, 4, 6, 8 };
			GDExtensionProductionUploadV2 prepared = { production_dto_abi<GDExtensionProductionUploadV2>(), 1, 11, 7, 0, 1, 0, 0, 4 };
			CHECK(upload(ref, &prepared, payload, sizeof(payload)).native_error == ERR_INVALID_DATA);
			prepared.source_recipe_substep = 2;
			CHECK(upload(ref, &prepared, payload, sizeof(payload)).native_error == OK);
			CHECK_FALSE(fixture.request->actions[0].prepared);
			CHECK(fixture.request->actions[1].prepared);
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
		}
	}
	struct OrdinalEmptyProbe {
		SingletonEnrollment *fixture = nullptr;
		uint32_t calls = 0;
		static void receive(void *data, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *operation) {
			auto &probe = *static_cast<OrdinalEmptyProbe *>(data);
			auto &fixture = *probe.fixture;
			++probe.calls;
			CHECK(operation != nullptr);
			CHECK(operation->canonical_begin == 10);
			CHECK(operation->canonical_end == 13);
			CHECK(GDExtensionDeviceExit::RegistrationContext::current != nullptr);
			CHECK(fixture.request->control.state == GDExtensionDeviceExit::ProductionControlState::EXECUTING);
			CHECK(fixture.request->action_count == 0);
			GDExtensionProductionAppendV1 append;
			std::memset(&append, 0x5a, sizeof(append));
			const auto before_append = append;
			CHECK(fixture.port.append(context, ref, 10, fixture.request->revision, &append).native_error == ERR_INVALID_DATA);
			CHECK(std::memcmp(&append, &before_append, sizeof(append)) == 0);
			GDExtensionProductionSegmentV1 segment;
			std::memset(&segment, 0x5a, sizeof(segment));
			const auto before_segment = segment;
			CHECK(fixture.port.close_segment(context, ref, &segment).native_error == ERR_INVALID_DATA);
			CHECK(std::memcmp(&segment, &before_segment, sizeof(segment)) == 0);
		}
	};
	struct LiveGeometryProbe {
		using MeshStorage = RendererRD::MeshStorage;
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		MeshStorage::Mesh mesh;
		MeshStorage::NativeSurfaceHold neighbor;
		RID array, buffer, previous_buffer, index_buffer, index_array;
		RD::VertexFormatID format = 0;
		uint64_t group = 0;
		bool cow = false, capacity_failure = false, transferred = false;
		bool clear_early = false, indexed144 = false, generation_root = false;
		Error capture_result = FAILED;
		static void
		receive(void *userdata,
				GDExtensionDeviceExit::ProductionPhysicalContextRef context,
				GDExtensionProductionRequestRefV1 ref,
				const GDExtensionProductionOperationV1 *) {
			auto &test = *static_cast<LiveGeometryProbe *>(userdata);
			auto &fixture = *test.fixture;
			auto &rd = fixture.rd;
			auto &native = *test.native;
			if (test.generation_root) {
				REQUIRE(native.install_generation(context, ref, fixture.registry) == OK);
			}
			native.install(0, fixture.registry, ref);
			REQUIRE(fixture.registry.production_output_pin_current(
							rd, native.pin(0)) == OK);
			REQUIRE(fixture.registry.production_output_owner_bind(
							native.pin(0), rd, native.particle_rid(),
							native.generation, 0, native.lease(0)) == OK);
			// Explicit component metadata, backed by actual original RD
			// factories.
			auto *surface = memnew(MeshStorage::Mesh::Surface);
			surface->format = RSE::ARRAY_FORMAT_VERTEX;
			surface->vertex_count = 3;
			surface->vertex_buffer_size = test.indexed144 ? 432 : 36;
			surface->primitive = RSE::PRIMITIVE_TRIANGLES;
			surface->aabb = AABB(Vector3(1, 2, 3), Vector3(4, 5, 6));
			surface->uv_scale = Vector4(1, 2, 3, 4);
			fixture.driver.mesh_directory_factory = true;
			fixture.driver.mesh_directory_indexed144 = test.indexed144;
			test.buffer = surface->vertex_buffer = rd.vertex_buffer_create(
					surface->vertex_buffer_size, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
			if (test.indexed144) {
				// Explicit Indexed144 component metadata. Original RD factories own
				// all physical buffers; this does not invoke an external issuer.
				surface->external = true;
				surface->current_allocation_generation = 7;
				surface->previous_allocation_generation = 8;
				test.previous_buffer = surface->previous_vertex_buffer = rd.vertex_buffer_create(432, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
				test.index_buffer = surface->index_buffer = rd.index_buffer_create(3, RD::INDEX_BUFFER_FORMAT_UINT32);
				surface->index_count = 3;
				test.index_array = surface->index_array = rd.index_array_create(test.index_buffer, 0, 3);
			}
			fixture.driver.mesh_directory_factory = false;
			fixture.driver.mesh_directory_indexed144 = false;
			RD::VertexAttribute position;
			position.location = 0;
			position.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			position.stride = test.indexed144 ? 144 : 12;
			Vector<RD::VertexAttribute> attributes;
			attributes.push_back(position);
			test.format = rd.vertex_format_create(attributes);
			Vector<RID> buffers;
			buffers.push_back(test.buffer);
			test.array = rd.vertex_array_create(3, test.format, buffers);
			surface->version_count = 1;
			surface->versions =
					static_cast<MeshStorage::Mesh::Surface::Version *>(
							memalloc(sizeof(MeshStorage::Mesh::Surface::Version)));
			::new (surface->versions) MeshStorage::Mesh::Surface::Version;
			surface->versions[0].input_mask = 1;
			surface->versions[0].vertex_format = test.format;
			surface->versions[0].vertex_array = test.array;
			surface->versions[0].current_allocation_generation = surface->current_allocation_generation;
			surface->versions[0].previous_allocation_generation = surface->previous_allocation_generation;
			test.mesh.surfaces = static_cast<MeshStorage::Mesh::Surface **>(
					memalloc(sizeof(MeshStorage::Mesh::Surface *)));
			test.mesh.surfaces[0] = surface;
			test.mesh.surface_count = 1;
			MeshStorage::NativeSurfaceHold selected;
			if (test.indexed144) {
				// A stale external Version cannot acquire another metadata/physical share.
				surface->versions[0].previous_allocation_generation = 9;
				MeshStorage::NativeSurfaceHold stale;
				CHECK(MeshStorage::_mesh_surface_hold_acquire(surface, 1, false, false, 0, stale) == ERR_UNAVAILABLE);
				CHECK_FALSE(stale.is_active());
				CHECK(surface->immutable_holds == 0);
				CHECK(rd.native_immutable_hold_count == 0);
				surface->versions[0].previous_allocation_generation = 8;
			}
			REQUIRE(MeshStorage::_mesh_surface_hold_acquire(
							surface, 1, false, false, 0, selected) == OK);
			if (test.cow) {
				REQUIRE(MeshStorage::_mesh_surface_hold_acquire(
								surface, 1, false, false, 0, test.neighbor) == OK);
			}
			const uint32_t shares = surface->immutable_holds,
						   physical_holds = rd.native_immutable_hold_count;
			const uint32_t buffer_holds =
					*rd._native_immutable_counter(test.buffer);
			const uint64_t next_group =
					fixture.request->next_disposal_group_serial;
			auto *physical =
					MeshStorage::_native_hold_transfer_source(selected);
			CHECK(physical !=
					nullptr); // RED on the old retired-only transfer gate.
			if (!physical) {
				REQUIRE(MeshStorage::mesh_surface_hold_release(selected) == OK);
				return; // Preserve a clean, reviewable RED without fake custody
						// return.
			}
			auto wrong = context;
			++wrong.frame_serial;
			CHECK(rd._production_output_hold_capture(wrong, native.pin(0),
						  *physical, test.group) ==
					ERR_UNAUTHORIZED);
			CHECK(test.group == 0);
			CHECK(selected.is_valid());
			CHECK(surface->immutable_holds == shares);
			CHECK(rd.native_immutable_hold_count == physical_holds);
			CHECK(*rd._native_immutable_counter(test.buffer) == buffer_holds);
			CHECK(MeshStorage::_native_hold_mark_transferred(selected) ==
					ERR_INVALID_DATA);
			test.capture_result = rd._production_output_hold_capture(
					context, native.pin(0), *physical, test.group);
			if (test.capacity_failure) {
				CHECK(test.capture_result == ERR_OUT_OF_MEMORY);
				CHECK(test.group == 0);
				CHECK(fixture.request->next_disposal_group_serial == next_group);
				CHECK(selected.is_valid());
				CHECK(surface->immutable_holds == shares);
				CHECK(rd.native_immutable_hold_count == physical_holds);
				CHECK(*rd._native_immutable_counter(test.buffer) == buffer_holds);
				for (uint32_t i = 0; i < fixture.request->capacity.resource_uses;
						++i) {
					CHECK(fixture.request->uses[i].output_role !=
							GDExtensionDeviceExit::ProductionUseCell::OutputRole::
									DISPOSAL_STAGED);
				}
				REQUIRE(MeshStorage::mesh_surface_hold_release(selected) == OK);
				return;
			}
			REQUIRE(test.capture_result == OK);
			REQUIRE(MeshStorage::_native_hold_mark_transferred(selected) == OK);
			test.transferred = true;
			if (test.indexed144) {
				CHECK(rd._production_output_hold_contains(native.pin(0), test.group, test.array));
				CHECK(rd._production_output_hold_contains(native.pin(0), test.group, test.buffer));
				CHECK(rd._production_output_hold_contains(native.pin(0), test.group, test.previous_buffer));
				CHECK(rd._production_output_hold_contains(native.pin(0), test.group, test.index_buffer));
				CHECK(rd._production_output_hold_contains(native.pin(0), test.group, test.index_array));
			}
			CHECK(surface->immutable_holds == shares - 1);
			CHECK(rd.native_immutable_hold_count == physical_holds);
			CHECK(*rd._native_immutable_counter(test.buffer) == buffer_holds);
			CHECK(MeshStorage::_native_hold_mark_transferred(selected) ==
					ERR_INVALID_DATA);
			CHECK(surface->immutable_holds ==
					shares - 1); // Cannot decrement twice.
			if (test.cow) {
				MeshStorage::_mesh_surface_clear(&test.mesh, 0);
				test.mesh.surface_count = 0;
				CHECK(surface->immutable_retired);
				CHECK(surface->immutable_holds ==
						1); // Neighbor owns old COW metadata.
			}
			if (test.clear_early) {
				// No neighbor metadata share remains. Original owner clear deletes
				// Surface now and requests actual RD retirement while Core holds it.
				CHECK(surface->immutable_holds == 0);
				MeshStorage::_mesh_surface_clear(&test.mesh, 0);
				test.mesh.surface_count = 0;
				bool *retiring = nullptr;
				const auto *counter = rd._native_immutable_counter(test.buffer, &retiring);
				REQUIRE(counter != nullptr);
				REQUIRE(retiring != nullptr);
				CHECK(*counter == buffer_holds);
				CHECK(*retiring);
				CHECK(rd.vertex_array_owner.owns(test.array));
				CHECK(fixture.driver.mesh_directory_buffer_frees == 0);
			}
			// Read after pointer clear and, in COW mode, after original mesh
			// detach.
			CHECK(selected.is_valid());
			CHECK(selected.is_external() == test.indexed144);
			CHECK(selected.get_vertex_array() == test.array);
			CHECK(selected.get_vertex_format() == test.format);
			CHECK(selected.get_index_array() == test.index_array);
			CHECK(selected.get_vertex_count() == 3);
			CHECK(selected.get_index_count() == (test.indexed144 ? 3u : 0u));
			CHECK(selected.get_primitive() == RSE::PRIMITIVE_TRIANGLES);
			CHECK(selected.get_format() == RSE::ARRAY_FORMAT_VERTEX);
			CHECK(selected.get_material_selection().is_null());
			CHECK(selected.get_aabb() ==
					AABB(Vector3(1, 2, 3), Vector3(4, 5, 6)));
			CHECK(selected.get_uv_scale() == Vector4(1, 2, 3, 4));
			MeshStorage::NativeSurfaceHold refused;
			CHECK(MeshStorage::mesh_surface_hold_derive(selected, 1, false,
						  false, 0, refused) ==
					ERR_INVALID_PARAMETER);
			CHECK_FALSE(refused.is_active());
			MeshStorage::NativeSurfaceHold moved(std::move(selected));
			CHECK_FALSE(selected.is_active());
			CHECK_FALSE(selected.is_valid());
			CHECK(moved.is_valid());
			CHECK(moved.get_vertex_array() == test.array);
			native.install_geometry(moved, test.group);
			CHECK_FALSE(moved.is_active());
		}
	};
	static void live_geometry_observe(bool cow = false,
			bool capacity_failure = false, bool clear_early = false, bool indexed144 = false, uint32_t pending_polls = 0, bool generation_root = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server ||
				!check_step(RendererRD::ParticlesStorage::get_singleton() ==
						nullptr)) {
			return;
		}
		const auto consume = ordinal_port<
				GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>(
				"device_session_production_record_consume_v2");
		const auto release = ordinal_port<
				GDExtensionInterfaceDeviceSessionProductionReleaseV2>(
				"device_session_production_release_v2");
		if (!consume || !release) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = {
			production_dto_abi<GDExtensionProductionCapacityV1>(),
			1,
			0,
			0,
			(capacity_failure ? 3u : 16u) + uint32_t(generation_root),
			0,
			0,
			0,
			0,
			0,
			0
		};
		plan.action_count = plan.resource_count = plan.use_count =
				plan.readback_count = 0;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		LiveGeometryProbe probe;
		probe.fixture = &fixture;
		probe.cow = cow;
		probe.capacity_failure = capacity_failure;
		probe.clear_early = clear_early;
		probe.indexed144 = indexed144;
		probe.generation_root = generation_root;
		if (!check_step(ordinal_initialize(fixture, server, plan,
								&LiveGeometryProbe::receive,
								&probe) == OK)) {
			return;
		}
		// Same original CPU-native frame setup used by existing custody
		// cases.
		fixture.rd.draw_graph.initialize(
				&fixture.driver,
				[](RenderingDeviceDriver *,
						VectorView<RenderingDeviceDriver::AttachmentLoadOp>,
						VectorView<RenderingDeviceDriver::AttachmentStoreOp>,
						void *) { return RenderingDeviceDriver::RenderPassID(); },
				1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance,
			request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 record = {};
		REQUIRE(consume(ref, control, &record).native_error == OK);
		CHECK(probe.transferred == !capacity_failure);
		CHECK(request.action_count == 0);
		CHECK(request.native_count ==
				0); // RECORD issued no request native members; later disposal may issue a frame source.
		CHECK(request.graph_count == 0);
		if (probe.transferred) {
			ProductionDisposalObservation held;
			REQUIRE(fixture.rd._production_output_hold_returned(native.pin(0), probe.group, held) == OK);
			CHECK(held.held == (indexed144 ? 5u : 2u));
			CHECK(held.queued == 0);
			CHECK(held.physical_returned == 0);
			CHECK(held.owner_returned == 0);
			CHECK_FALSE(held.settled);
		}
		fixture.driver.directory_pending_polls = pending_polls;
		if (generation_root) {
			native.withdraw_generation();
		} else {
			native.withdraw();
		}
		const uint64_t root_serial = native.generation_serial();
		bool saw_slot_return_before_root = false, saw_terminal = false;
		bool saw_settled = false, saw_finish_after_group = false;
		bool saw_queued = false, saw_physical = false;
		uint32_t pending_seen = 0;
		for (uint32_t turn = 0; turn < 64 && !native.released(); ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = {
				production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
				OS::get_singleton()->get_ticks_usec() + 10000000,
				1,
				1,
				1,
				0
			};
			ProductionControlOutput output = {};
			uint64_t serial = 0;
			const uint32_t before_receipts = fixture.driver.observed_disposal_receipts();
			const uint32_t before_pending = fixture.driver.directory_pending_polls;
			const bool before_group = native.geometry_group_returned();
			const bool before_active = native.geometry_active();
			REQUIRE(fixture.registry.production_control_accept(
							ref, ProductionControlKind::OBSERVE, &serial,
							&budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) ==
					OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial,
							&output) == OK);
			CHECK((output.result.native_error == OK ||
					output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			if (generation_root) {
				if (output.output_generation.native_released) {
					CHECK_FALSE(saw_terminal);
					CHECK(saw_slot_return_before_root);
					CHECK(native.pin(0).is_empty());
					CHECK(native.generation_pin().is_empty());
					CHECK(native.released());
					CHECK(output.output_generation.registry_instance == ref.registry_instance);
					CHECK(output.output_generation.request_serial == ref.handle_serial);
					CHECK(output.output_generation.control_serial == serial);
					CHECK(output.output_generation.root_serial == root_serial);
					CHECK(output.output_generation.particles == native.particle_rid().get_id());
					CHECK(output.output_generation.generation == native.generation);
					CHECK(output.output_generation.unused_slots == 1);
					CHECK(output.output_generation.returned_slots == 1);
					saw_terminal = true;
				} else {
					CHECK_FALSE(native.generation_pin().is_empty());
					CHECK_FALSE(native.released());
					if (native.pin(0).is_empty()) {
						saw_slot_return_before_root = true;
					}
				}
			} else {
				CHECK_FALSE(output.output_generation.native_released);
			}

			CHECK(output.service_usage.receipts <= 1);
			CHECK(output.service_usage.receipts == fixture.driver.observed_disposal_receipts() - before_receipts);
			if (clear_early) {
				CHECK(fixture.driver.issues <= 1);
				CHECK(fixture.driver.polls <= pending_polls + 1);
			} else {
				CHECK(output.service_usage.receipts == 0);
				CHECK(fixture.driver.issues == 0);
				CHECK(fixture.driver.polls == 0);
			}
			CHECK(request.capture_inflight == 0);
			if (probe.transferred && !native.pin(0).is_empty()) {
				ProductionDisposalObservation actual;
				REQUIRE(fixture.rd._production_output_hold_returned(
								native.pin(0), probe.group, actual) == OK);
				if (before_pending > fixture.driver.directory_pending_polls) {
					// The original maintenance cursor polled its actual issued source.
					// A CPU-driver Pending answer must retain this exact native prefix.
					CHECK(before_pending == fixture.driver.directory_pending_polls + 1);
					++pending_seen;
					CHECK(fixture.driver.issues == 1);
					CHECK(actual.queued == 1);
					CHECK(actual.physical_returned == 0);
					CHECK(actual.owner_returned == 1);
					CHECK(actual.borrowed_detached == 0);
					CHECK(actual.uncreated == 0);
					CHECK(actual.unknown == 0);
					CHECK_FALSE(actual.settled);
					CHECK(native.geometry_active());
					CHECK_FALSE(native.geometry_group_returned());
					CHECK_FALSE(native.pin(0).is_empty());
					CHECK(fixture.driver.mesh_directory_buffers_live[0]);
					CHECK(fixture.driver.mesh_directory_buffer_frees == 0);
					CHECK(fixture.rd.frames[0].buffers_to_dispose_of.size() == 1);
					CHECK(fixture.rd.frames[0].maintenance_wait.phase == 0);
					CHECK(fixture.rd.frames[0].maintenance_wait.report == 0);
					CHECK(fixture.rd.frames[0].maintenance_wait.call == 0);
					REQUIRE(fixture.rd.frames[0].chain_reports.size() == 1);
					REQUIRE(fixture.rd.frames[0].chain_reports[0].calls.size() == 1);
					const auto &source = fixture.rd.frames[0].chain_reports[0].calls[0];
					CHECK(source.serial != 0);
					CHECK(source.completion_serial == source.serial + 1000);
					CHECK(source.issue == RDD::IssueFact::KnownAccepted);
				}
				if (clear_early && actual.queued) {
					CHECK(actual.queued == 1);
					CHECK(actual.physical_returned == 0);
					CHECK(actual.borrowed_detached == 0);
					CHECK(fixture.driver.mesh_directory_buffer_frees == 0);
					CHECK_FALSE(actual.settled);
					CHECK(native.geometry_active());
					CHECK_FALSE(native.geometry_group_returned());
					saw_queued = true;
				}
				if (actual.settled) {
					CHECK(actual.dropped);
					CHECK(actual.borrowed_detached == (clear_early ? 0u : (indexed144 ? 5u : 2u)));
					// VA/IA are CPU owner metadata: kind>=10 returns state 7.
					// Only the actual backing-buffer driver free returns state 3.
					CHECK(actual.physical_returned == (clear_early ? 1u : 0u));
					CHECK(actual.owner_returned == (clear_early ? 1u : 0u));
					CHECK(actual.uncreated == 0);
					if (clear_early) {
						CHECK(fixture.driver.mesh_directory_buffer_frees == 1);
						CHECK_FALSE(fixture.rd.vertex_array_owner.owns(probe.array));
						CHECK_FALSE(fixture.rd.vertex_buffer_owner.owns(probe.buffer));
						saw_physical = true;
					}
					CHECK(actual.unknown == 0);
					saw_settled = true;
				}
				if (before_active && !native.geometry_active()) {
					CHECK(before_group);
					CHECK(saw_settled);
					saw_finish_after_group = true;
				}
			}
			if (!clear_early) {
				CHECK(fixture.driver.mesh_directory_buffer_frees == 0);
				CHECK(fixture.rd.vertex_array_owner.owns(probe.array));
				CHECK(fixture.rd.vertex_buffer_owner.owns(probe.buffer));
				if (indexed144) {
					CHECK(fixture.rd.vertex_buffer_owner.owns(probe.previous_buffer));
					CHECK(fixture.rd.index_buffer_owner.owns(probe.index_buffer));
					CHECK(fixture.rd.index_array_owner.owns(probe.index_array));
				}
			}
		}
		CHECK(native.released());
		CHECK(saw_terminal == generation_root);
		if (generation_root) {
			generation_root_drain_cursor(fixture, ref, server);
		}
		if (probe.transferred) {
			CHECK(saw_settled);
			CHECK(saw_finish_after_group);
		}
		if (clear_early) {
			CHECK(saw_queued);
			CHECK(saw_physical);
			CHECK(fixture.driver.issues == 1);
			CHECK(fixture.driver.polls == pending_polls + 1);
			CHECK(pending_seen == pending_polls);
			CHECK(fixture.driver.directory_pending_polls == 0);
			CHECK(fixture.driver.fence_polls == 1);
			CHECK(fixture.driver.fence_recycles == 1);
		}
		// Free the real mesh only after this External use returned. This
		// ordinary owner cleanup is not the witness used by the assertions
		// above.
		if (probe.neighbor.is_active()) {
			REQUIRE(RendererRD::MeshStorage::mesh_surface_hold_release(
							probe.neighbor) == OK);
		}
		if (probe.mesh.surface_count) {
			RendererRD::MeshStorage::_mesh_surface_clear(&probe.mesh, 0);
			probe.mesh.surface_count = 0;
		}
		if (indexed144) {
			// Indexed144 original owner retains external backing after mesh clear.
			// Free only after its actual borrowed Core group returned.
			CHECK(fixture.driver.mesh_directory_buffer_frees == 0);
			fixture.rd.free_rid(probe.buffer);
			fixture.rd.free_rid(probe.previous_buffer);
			fixture.rd.free_rid(probe.index_buffer);
		}
		memfree(probe.mesh.surfaces);
		probe.mesh.surfaces = nullptr;
		fixture.rd._free_pending_resources(0);
		CHECK(fixture.driver.mesh_directory_buffer_frees == (indexed144 ? 3u : 1u));
		if (indexed144) {
			CHECK_FALSE(fixture.rd.vertex_buffer_owner.owns(probe.previous_buffer));
			CHECK_FALSE(fixture.rd.index_buffer_owner.owns(probe.index_buffer));
			CHECK_FALSE(fixture.rd.index_array_owner.owns(probe.index_array));
		}
		CHECK_FALSE(fixture.rd.vertex_array_owner.owns(probe.array));
		CHECK_FALSE(fixture.rd.vertex_buffer_owner.owns(probe.buffer));
		fixture.rd.vertex_formats.clear();
		fixture.rd.vertex_format_cache.clear();
		for (uint32_t turn = 0; turn < 16 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = {
				production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
				OS::get_singleton()->get_ticks_usec() + 10000000,
				1,
				1,
				1,
				0
			};
			GDExtensionProductionServiceBudgetV1 remaining = {};
			GDExtensionProductionReportV2 report = {};
			const Error result =
					Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((result == OK || result == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct OwnedStorageProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		uint32_t capacity = 0, fail_at = 0, calls = 0;
		bool insufficient = false, invalid_extent = false, foreign_before_allocate = false, foreign_turn = false;
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &test = *static_cast<OwnedStorageProbe *>(userdata);
			auto &fixture = *test.fixture;
			auto &native = *test.native;
			++test.calls;
			if (test.foreign_turn) {
				const GDExtensionProductionRequestRefV1 original = { fixture.instance, fixture.request->serial };
				REQUIRE(ref.handle_serial != original.handle_serial);
				CHECK(ref.registry_instance == original.registry_instance);
				const auto next = fixture.request->next_disposal_group_serial;
				const auto attempts = fixture.driver.output_storage_attempts;
				CHECK(native.allocate_storage(context) == ERR_UNAUTHORIZED);
				CHECK(fixture.request->next_disposal_group_serial == next);
				CHECK(fixture.driver.output_storage_attempts == attempts);
				CHECK(fixture.driver.output_storage_frees == 0);
				CHECK(native.storage_group() == 0);
				CHECK(native.storage_total_bytes() == 0);
				CHECK(native.slot_reserved(0));
				CHECK(native.storage_tokens_empty());
				CHECK(fixture.registry.production_output_pin_matches(native.pin(0), original, fixture.rd));
				for (uint32_t role = 0; role < 7; ++role) {
					CHECK(native.storage_rid(role).is_null());
					CHECK(native.storage_bytes(role) == 0);
				}
				return;
			}
			if (native.pin(0).is_empty()) {
				native.install(0, fixture.registry, ref);
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, native.pin(0)) == OK);
				REQUIRE(fixture.registry.production_output_owner_bind(native.pin(0), fixture.rd, native.particle_rid(), native.generation, 0, native.lease(0)) == OK);
			}
			if (test.foreign_before_allocate && test.calls == 1) {
				return; // Keep the actual original slot before any factory attempt.
			}
			const auto next = fixture.request->next_disposal_group_serial;
			if (!test.insufficient && !test.invalid_extent && !test.fail_at) {
				auto foreign_context = context;
				++foreign_context.frame_serial;
				CHECK(native.allocate_storage(foreign_context) == ERR_UNAUTHORIZED);
				CHECK(native.storage_group() == 0);
				CHECK(fixture.request->next_disposal_group_serial == next);
				CHECK(fixture.driver.output_storage_attempts == 0);
				// The group reservation succeeds, then the original native
				// preflight refuses an open list before any driver allocation.
				REQUIRE_FALSE(fixture.rd.compute_list.active);
				fixture.rd.compute_list.active = true;
				const Error busy = native.allocate_storage(context);
				fixture.rd.compute_list.active = false;
				CHECK(busy == ERR_BUSY);
				CHECK(native.storage_group() == 0);
				CHECK(native.storage_total_bytes() == 0);
				CHECK(fixture.request->next_disposal_group_serial == next + 1);
				CHECK(fixture.driver.output_storage_attempts == 0);
				CHECK(native.slot_reserved(0));
				CHECK(native.storage_tokens_empty());
				for (uint32_t role = 0; role < 7; ++role) {
					CHECK(native.storage_rid(role).is_null());
					CHECK(native.storage_bytes(role) == 0);
				}
			}
			const Error result = native.allocate_storage(context);
			if (test.insufficient || test.invalid_extent) {
				CHECK(result == (test.invalid_extent ? ERR_INVALID_PARAMETER : ERR_OUT_OF_MEMORY));
				CHECK(native.storage_total_bytes() == 0);
				CHECK(native.slot_reserved(0));
				CHECK(native.storage_tokens_empty());
				CHECK(fixture.registry.production_output_pin_matches(native.pin(0), ref, fixture.rd));
				for (uint32_t role = 0; role < 7; ++role) {
					CHECK(native.storage_rid(role).is_null());
					CHECK(native.storage_bytes(role) == 0);
				}
				CHECK(native.storage_group() == 0);
				CHECK(fixture.request->next_disposal_group_serial == next);
				CHECK(fixture.driver.output_storage_attempts == 0);
				for (uint32_t row = 0; row < fixture.request->capacity.resource_uses; ++row) {
					const auto &cell = fixture.request->uses[row];
					CHECK(cell.disposal_group_serial == 0);
					CHECK(cell.output_role == (cell.output_consumer == &native.pin(0) ? ProductionUseCell::OutputRole::NATIVE_CONSUMER : ProductionUseCell::OutputRole::UNUSED));
				}
				native.discard_unattempted_storage();
				return;
			}
			CHECK(result == (test.fail_at ? ERR_CANT_CREATE : OK));
			const uint32_t expected_count = test.fail_at ? test.fail_at - 1 : (test.capacity ? 7 : 5);
			CHECK(fixture.driver.output_storage_count == expected_count);
			CHECK(fixture.driver.output_storage_attempts == (test.fail_at ? test.fail_at : expected_count));
			CHECK(native.storage_total_bytes() == (test.capacity ? 1448u : 680u));
			const uint32_t expected_bytes[7] = { test.capacity * 192, test.capacity * 192, 32, 64, 32, 40, 512 };
			uint32_t ordinal = 0;
			for (uint32_t role = 0; role < 7; ++role) {
				CHECK(native.storage_bytes(role) == expected_bytes[role]);
				if (!expected_bytes[role]) {
					CHECK(native.storage_rid(role).is_null());
					continue;
				}
				if (ordinal < expected_count) {
					const RID rid = native.storage_rid(role);
					REQUIRE(rid.is_valid());
					CHECK(fixture.driver.output_storage_bytes[ordinal] == expected_bytes[role]);
					CHECK(fixture.driver.output_storage_indirect[ordinal] == (role == 5));
					CHECK(fixture.rd._production_output_owned_storage_contains(native.pin(0), native.storage_group(), rid));
					CHECK_FALSE(fixture.rd._production_output_hold_contains(native.pin(0), native.storage_group(), rid));
					const auto *buffer = fixture.rd.storage_buffer_owner.get_or_null(rid);
					REQUIRE(buffer != nullptr);
					CHECK(buffer->size == expected_bytes[role]);
					CHECK(buffer->backing_serial != 0);
					CHECK(buffer->driver_id == RDD::BufferID(300 + ordinal));
					for (uint32_t earlier = 0; earlier < role; ++earlier) {
						const RID other = native.storage_rid(earlier);
						if (other.is_valid()) {
							CHECK(other != rid);
							const auto *previous = fixture.rd.storage_buffer_owner.get_or_null(other);
							REQUIRE(previous != nullptr);
							CHECK(previous->backing_serial != buffer->backing_serial);
						}
					}
				} else {
					CHECK(native.storage_rid(role).is_null());
				}
				++ordinal;
			}
			CHECK(native.storage_tokens_empty());
			const auto attempts = fixture.driver.output_storage_attempts;
			CHECK(native.allocate_storage(context) != OK);
			CHECK(fixture.driver.output_storage_attempts == attempts);
			CHECK(fixture.driver.output_storage_frees == 0);
		}
	};
	static void owned_storage_observe(uint32_t capacity, uint32_t fail_at = 0, bool insufficient = false, bool invalid_extent = false, bool foreign_before_allocate = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan plan;
		const uint32_t members = capacity ? 7 : 5;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, members + 2 - uint32_t(insufficient), 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		plan.attachment_requests = foreign_before_allocate ? 2 : 1;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		OwnedStorageProbe probe;
		probe.fixture = &fixture;
		probe.capacity = capacity;
		probe.fail_at = fail_at;
		probe.insufficient = insufficient;
		probe.invalid_extent = invalid_extent;
		probe.foreign_before_allocate = foreign_before_allocate;
		REQUIRE(ordinal_initialize(fixture, server, plan, &OwnedStorageProbe::receive, &probe) == OK);
		fixture.driver.output_storage_factory = true;
		fixture.driver.output_storage_fail_at = fail_at;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		// Finite private component metadata; no source/charge or usable token.
		native.prepare_storage(capacity, 2);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		if (foreign_before_allocate) {
			const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
			REQUIRE(reserve != nullptr);
			GDExtensionProductionCapacityV1 empty_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			auto other_operation = plan.operation;
			++other_operation.owner_request_nonce;
			GDExtensionProductionRequestRefV1 other_ref{};
			REQUIRE(reserve(fixture.attachment, &other_operation, &empty_capacity, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &other_ref).native_error == OK);
			Request *other = nullptr;
			{
				MutexLock lock(fixture.registry.mutex);
				RenderingDevice *same_device = nullptr;
				REQUIRE(fixture.registry.production_request_locked(other_ref, false, other, same_device) == OK);
				REQUIRE(other->attachment == request.attachment);
				CHECK(same_device == &fixture.rd);
			}
			probe.foreign_turn = true;
			uint64_t other_control = 0;
			REQUIRE(fixture.port.record_schedule(other_ref, &other_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 other_report{};
			REQUIRE(consume(other_ref, other_control, &other_report).native_error == OK);
			CHECK(probe.calls == 2);
			probe.foreign_turn = false;
			for (uint32_t turn = 0; turn < 8 && other->serial; ++turn) {
				const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				GDExtensionProductionServiceBudgetV1 remaining{};
				GDExtensionProductionReportV2 report{};
				const Error error = Error(release(other_ref, &budget, &remaining, &report).native_error);
				CHECK((error == OK || error == ERR_BUSY));
				if (other->serial) {
					server->sync();
				}
			}
			REQUIRE(other->serial == 0);
			// Resume the true original RECORD; rejection did not consume its slot.
			uint64_t original_control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &original_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 original_report{};
			REQUIRE(consume(ref, original_control, &original_report).native_error == OK);
			CHECK(probe.calls == 3);
		}
		CHECK(request.native_count == 0);
		CHECK(request.graph_count == 0);
		native.withdraw();
		if (!insufficient && !invalid_extent) {
			const uint64_t group = native.storage_group();
			const auto attempts = fixture.driver.output_storage_attempts;
			const auto frees = fixture.driver.output_storage_frees;
			const auto next_group = request.next_disposal_group_serial;
			void *const arena = request.attachment->arena;
			const auto children = fixture.holder.production_children;
			RID resources[7];
			bool live[7];
			for (uint32_t role = 0; role < 7; ++role) {
				resources[role] = native.storage_rid(role);
				live[role] = fixture.driver.output_storage_live[role];
			}
			for (uint32_t idle = 0; idle < 3; ++idle) {
				GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 0, 0, 0, 0 };
				if (idle == 2) {
					budget.controls = budget.passes = budget.receipts = 1;
					budget.deadline_micros = 1;
				}
				ProductionControlOutput output{};
				uint64_t serial = 0;
				REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
				REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
				server->sync();
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
				CHECK(output.service_usage.controls == 0);
				CHECK(output.service_usage.passes == 0);
				CHECK(output.service_usage.receipts == 0);
				CHECK_FALSE(output.output_generation.native_released);
				CHECK(native.storage_group() == group);
				CHECK_FALSE(native.storage_empty());
				CHECK_FALSE(native.released());
				CHECK(fixture.registry.production_output_pin_matches(native.pin(0), ref, fixture.rd));
				CHECK(request.next_disposal_group_serial == next_group);
				CHECK(request.attachment->arena == arena);
				CHECK(fixture.holder.production_children == children);
				CHECK(children != 0);
				CHECK(fixture.driver.output_storage_attempts == attempts);
				CHECK(fixture.driver.output_storage_frees == frees);
				for (uint32_t role = 0; role < 7; ++role) {
					CHECK(native.storage_rid(role) == resources[role]);
					CHECK(fixture.driver.output_storage_live[role] == live[role]);
				}
			}
		}
		for (uint32_t turn = 0; turn < 256 && !native.released(); ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			ProductionControlOutput output{};
			uint64_t serial = 0;
			const uint32_t frees = fixture.driver.output_storage_frees;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			CHECK(output.service_usage.receipts <= 1);
			CHECK(fixture.driver.output_storage_frees - frees <= 1);
			CHECK_FALSE(output.output_generation.native_released);
		}
		CHECK(native.released());
		CHECK(native.storage_empty());
		CHECK(native.storage_tokens_empty());
		CHECK(fixture.driver.output_storage_frees == fixture.driver.output_storage_count);
		for (uint32_t i = 0; i < 7; ++i) {
			CHECK_FALSE(fixture.driver.output_storage_live[i]);
		}
		generation_root_drain_cursor(fixture, ref, server);
		for (uint32_t turn = 0; turn < 16 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			const Error error = Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct PipelinePrefixProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		void *material_profile = nullptr;
		RID shader, created;
		uint64_t owned_group = 0, material_group = 0;
		uint32_t mode = 0, calls = 0;
		bool generation_root = false;
		// 0: pipeline success, real material capacity failure; 1: material borrow;
		// 2/3: driver failure (ordinary/quiet); 4: original Core capacity failure.
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &test = *static_cast<PipelinePrefixProbe *>(userdata);
			auto &fixture = *test.fixture;
			auto &rd = fixture.rd;
			auto &native = *test.native;
			++test.calls;
			if (test.generation_root) {
				REQUIRE(native.install_generation(context, ref, fixture.registry) == OK);
			}
			native.install(0, fixture.registry, ref);
			REQUIRE(fixture.registry.production_output_pin_current(rd, native.pin(0)) == OK);
			REQUIRE(fixture.registry.production_output_owner_bind(native.pin(0), rd, native.particle_rid(), native.generation, 0, native.lease(0)) == OK);
			const uint32_t attempts = fixture.driver.raster_attempts;
			const uint32_t owner_count = rd.render_pipeline_owner.get_rid_count();
			const Error reserved = rd._production_output_owned_reserve(context, native.pin(0), 1, test.owned_group);
			if (test.mode == 4) {
				CHECK(reserved == ERR_OUT_OF_MEMORY);
				CHECK(test.owned_group == 0);
				CHECK(fixture.driver.raster_attempts == attempts);
				CHECK(rd.render_pipeline_owner.get_rid_count() == owner_count);
				return;
			}
			REQUIRE(reserved == OK);
			// Real SurfaceBinding holds the group and output RID before factory
			// entry. This is private component directory setup, not admission.
			native.install_pipeline(test.owned_group);
			ProductionDisposalUse *original_member = nullptr;
			for (uint32_t i = 0; i < fixture.request->capacity.resource_uses; ++i) {
				auto &cell = fixture.request->uses[i];
				if (cell.disposal_group_serial == test.owned_group && cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
					REQUIRE(original_member == nullptr);
					original_member = &cell.disposal;
				}
			}
			REQUIRE(original_member != nullptr);
			Ref<UnitContainer> container;
			container.instantiate();
			container->initialize_raster();
			test.shader = rd.shader_create_from_bytecode(container->to_bytes());
			REQUIRE(test.shader.is_valid());
			fixture.driver.raster_fail = test.mode == 2 || test.mode == 3;
			fixture.driver.raster_quiet = test.mode == 3;
			const auto format = rd.framebuffer_format_create(Vector<RD::AttachmentFormat>());
			const uint32_t repetitions = fixture.driver.raster_fail ? 3 : 1;
			for (uint32_t i = 0; i < repetitions; ++i) {
				const Error made = rd._production_output_owned_render_pipeline_create(context, native.pin(0), test.owned_group, 0,
						test.shader, format, RD::INVALID_ID, RD::RENDER_PRIMITIVE_TRIANGLES,
						RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState(), 0, 0, {}, native.pipeline_root());
				if (fixture.driver.raster_fail) {
					CHECK(made == ERR_CANT_CREATE);
					CHECK(native.pipeline_root().is_null());
					CHECK(rd.render_pipeline_owner.get_rid_count() == owner_count);
					CHECK_FALSE(fixture.driver.raster_live);
					CHECK(fixture.driver.raster_frees == 0);
				} else {
					REQUIRE(made == OK);
					test.created = native.pipeline_root();
					REQUIRE(test.created.is_valid());
					REQUIRE(rd.render_pipeline_owner.get_rid_count() == owner_count + 1);
					const auto *actual = rd.render_pipeline_owner.get_or_null(test.created);
					REQUIRE(actual != nullptr);
					CHECK(actual->driver_id == RDD::PipelineID(281));
					REQUIRE(actual->production_disposal_uses != nullptr);
					CHECK(actual->production_disposal_uses == original_member);
					CHECK(actual->shader == test.shader);
					ProductionDisposalObservation observed;
					REQUIRE(rd._production_output_hold_returned(native.pin(0), test.owned_group, observed) == OK);
					CHECK(observed.held == 1);
					CHECK((observed.queued == 0 && observed.physical_returned == 0 && observed.uncreated == 0 && observed.unknown == 0));
					CHECK_FALSE(observed.dropped);
					CHECK_FALSE(observed.settled);
				}
			}
			CHECK(fixture.driver.raster_attempts == attempts + repetitions);
			uint32_t members = 0, reserved_members = 0;
			for (uint32_t i = 0; i < fixture.request->capacity.resource_uses; ++i) {
				const auto &cell = fixture.request->uses[i];
				if (cell.disposal_group_serial == test.owned_group) {
					members += cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
					reserved_members += cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED;
				}
			}
			CHECK(members == (fixture.driver.raster_fail ? 0u : 1u));
			CHECK(reserved_members == (fixture.driver.raster_fail ? 1u : 0u));
			if (fixture.driver.raster_fail) {
				return;
			}
			test.material_profile = RendererRD::pipeline_material_profile_create(test.shader);
			REQUIRE(test.material_profile != nullptr);
			// Non-movable logical custody is acquired in its actual owner slot.
			auto &hold = native.material_hold();
			const Error acquired = RendererRD::pipeline_material_profile_acquire(test.material_profile, test.created, test.mode == 0 ? 1u : 2u, hold);
			if (test.mode == 0) {
				CHECK(acquired == ERR_PARAMETER_RANGE_ERROR);
				CHECK_FALSE(hold.is_active());
				CHECK(native.pipeline_root() == test.created);
				CHECK(rd.render_pipeline_is_valid(test.created));
				CHECK(fixture.driver.raster_live);
				CHECK(fixture.driver.raster_frees == 0);
			} else {
				REQUIRE(acquired == OK);
				CHECK(hold.is_active());
				CHECK(hold.get_snapshot().pipeline == test.created);
				CHECK(hold.get_snapshot().shader == test.shader);
				REQUIRE(RendererRD::pipeline_material_profile_capture(context, native.pin(0), hold, test.material_group) == OK);
				CHECK(hold.is_active()); // Logical hold survives physical move.
				CHECK(rd._production_output_hold_contains(native.pin(0), test.material_group, test.created));
				native.install_material(test.material_group);
				CHECK(native.material_active());
				CHECK(&hold == &native.material_hold());
			}
		}
	};
	static void pipeline_prefix_observe(uint32_t mode, bool generation_root = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, (mode == 4 ? 1u : 12u) + uint32_t(generation_root), 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		PipelinePrefixProbe probe;
		probe.fixture = &fixture;
		probe.mode = mode;
		probe.generation_root = generation_root;
		REQUIRE(ordinal_initialize(fixture, server, plan, &PipelinePrefixProbe::receive, &probe) == OK);
		fixture.driver.raster_device = &fixture.rd;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		if (generation_root) {
			native.withdraw_generation();
		} else {
			native.withdraw();
		}
		const uint64_t root_serial = native.generation_serial();
		bool saw_slot_return_before_root = false, saw_terminal = false;
		bool saw_owned = false, saw_borrowed = false, saw_owned_before_borrow = false, saw_material_return = false;
		uint32_t turns = 0;
		for (; turns < 128 && !native.released(); ++turns) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			const uint32_t frees_before = fixture.driver.raster_frees;
			const uint32_t receipts_before = fixture.driver.observed_disposal_receipts() + frees_before;
			const bool material_before = native.material_active();
			ProductionControlOutput output{};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			if (generation_root) {
				if (output.output_generation.native_released) {
					CHECK_FALSE(saw_terminal);
					CHECK(saw_slot_return_before_root);
					CHECK(native.pin(0).is_empty());
					CHECK(native.generation_pin().is_empty());
					CHECK(native.released());
					CHECK(output.output_generation.registry_instance == ref.registry_instance);
					CHECK(output.output_generation.request_serial == ref.handle_serial);
					CHECK(output.output_generation.control_serial == serial);
					CHECK(output.output_generation.root_serial == root_serial);
					CHECK(output.output_generation.particles == native.particle_rid().get_id());
					CHECK(output.output_generation.generation == native.generation);
					CHECK(output.output_generation.unused_slots == 1);
					CHECK(output.output_generation.returned_slots == 1);
					saw_terminal = true;
				} else {
					CHECK_FALSE(native.generation_pin().is_empty());
					CHECK_FALSE(native.released());
					if (native.pin(0).is_empty()) {
						saw_slot_return_before_root = true;
					}
				}
			} else {
				CHECK_FALSE(output.output_generation.native_released);
			}

			CHECK(output.service_usage.receipts <= 1);
			CHECK(fixture.driver.raster_frees - frees_before <= 1);
			CHECK(output.service_usage.receipts == fixture.driver.observed_disposal_receipts() + fixture.driver.raster_frees - receipts_before);
			if (!native.pin(0).is_empty() && probe.owned_group) {
				ProductionDisposalObservation owned;
				REQUIRE(fixture.rd._production_output_hold_returned(native.pin(0), probe.owned_group, owned) == OK);
				CHECK(owned.unknown == 0);
				if (owned.settled) {
					CHECK(owned.physical_returned == (mode < 2 ? 1u : 0u));
					CHECK(owned.uncreated == (mode < 2 ? 0u : 1u));
					CHECK(owned.borrowed_detached == 0);
					saw_owned = true;
				}
				if (probe.material_group) {
					ProductionDisposalObservation borrowed;
					REQUIRE(fixture.rd._production_output_hold_returned(native.pin(0), probe.material_group, borrowed) == OK);
					CHECK(borrowed.unknown == 0);
					bool owned_release = false, borrowed_release = false;
					for (uint32_t i = 0; i < request.capacity.resource_uses; ++i) {
						const auto &cell = request.uses[i];
						if (cell.output_role != ProductionUseCell::OutputRole::DISPOSAL_GROUP) {
							continue;
						}
						if (cell.disposal_group_serial == probe.owned_group) {
							owned_release = cell.disposal_group_release;
						} else if (cell.disposal_group_serial == probe.material_group) {
							borrowed_release = cell.disposal_group_release;
						}
					}
					// The one-pass owned release precedes release of its material borrow.
					// A retiring RID need not survive a whole OBSERVE dispatch.
					if (owned_release && !borrowed_release) {
						CHECK(owned.held == 1);
						// A generation-root OBSERVE may already drop the owned counter.
						// Its original member remains HELD while the material borrow
						// keeps the actual pipeline alive until its separate release.
						const auto *pipeline = fixture.rd.render_pipeline_owner.get_or_null(probe.created);
						REQUIRE(pipeline != nullptr);
						CHECK(pipeline->native_immutable_holds >= 1);
						CHECK(pipeline->native_immutable_holds <= 2);
						if (owned.dropped) {
							CHECK(pipeline->native_immutable_holds == 1);
						}
						CHECK_FALSE(owned.settled);
						CHECK(borrowed.held == 2);
						CHECK_FALSE(borrowed.dropped);
						CHECK_FALSE(borrowed.settled);
						CHECK(native.material_active());
						CHECK(fixture.rd.render_pipeline_is_valid(probe.created));
						CHECK(fixture.driver.raster_live);
						CHECK(fixture.driver.raster_frees == 0);
						saw_owned_before_borrow = true;
					}
					if (borrowed.settled) {
						CHECK(borrowed.borrowed_detached == 1); // Shader remains live.
						CHECK(borrowed.physical_returned == 1); // Shared pipeline really freed.
						saw_borrowed = true;
					}
				}
			}
			if (material_before && !native.material_active()) {
				CHECK(saw_borrowed);
				saw_material_return = true;
			}
		}
		CHECK(native.released());
		CHECK(saw_terminal == generation_root);
		CHECK(turns > 1);
		CHECK(saw_owned == (mode != 4));
		if (mode == 1) {
			CHECK(saw_owned_before_borrow);
			CHECK(saw_borrowed);
			CHECK(saw_material_return);
		}
		CHECK(fixture.driver.raster_frees == (mode < 2 ? 1u : 0u));
		CHECK(fixture.driver.raster_budgeted_frees == fixture.driver.raster_frees);
		CHECK_FALSE(fixture.driver.raster_live);
		CHECK(fixture.rd.render_pipeline_owner.get_rid_count() == 0);
		// Finish the exact original v2 OBSERVE cursor before ordinary cleanup.
		// An unbounded frame drain would invalidate its retained disposal iterator.
		for (uint32_t turn = 0; turn < 128 && request.maintenance_serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			const uint32_t receipts_before = fixture.driver.observed_disposal_receipts() + fixture.driver.raster_frees;
			ProductionControlOutput output{};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			CHECK(output.service_usage.receipts <= 1);
			CHECK(output.service_usage.receipts == fixture.driver.observed_disposal_receipts() + fixture.driver.raster_frees - receipts_before);
		}
		CHECK(request.maintenance_serial == 0);
		// Ordinary shader/profile cleanup follows the proven output and cursor return.
		if (probe.material_profile) {
			RendererRD::pipeline_material_profile_destroy(probe.material_profile);
		} else if (probe.shader.is_valid()) {
			fixture.rd.free_rid(probe.shader);
		}
		fixture.rd._free_pending_resources(0);
		CHECK(fixture.driver.raster_shader_frees == (mode == 4 ? 0u : 1u));
		for (auto &entry : fixture.rd.framebuffer_formats) {
			fixture.driver.render_pass_free(entry.value.render_pass);
		}
		fixture.rd.framebuffer_formats.clear();
		fixture.rd.framebuffer_format_cache.clear();
		for (uint32_t turn = 0; turn < 16 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			const Error error = Error(release(ref, &budget, &remaining, &report).native_error);
			INFO("mode=", mode, " error=", error, " cause=", report.cause.native_error, " control_result=", request.control.output.result.native_error, " state=", uint32_t(request.control.state));
			CHECK((error == OK || error == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct OutputLeafProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		GDExtensionInterfaceDeviceSessionOutputCreateV1 create = nullptr;
		GDExtensionInterfaceDeviceSessionOutputSealV1 seal = nullptr;
		GDExtensionInterfaceDeviceSessionOutputCancelV1 cancel = nullptr;
		GDExtensionInterfaceDeviceSessionOutputWithdrawV1 withdraw = nullptr;
		GDExtensionProductionPhysicalContextRefV1 expired{};
		GDExtensionOutputCreateInputV1 config{};
		GDExtensionOutputSealInputV1 seal_input{};
		GDExtensionOutputCancelInputV1 cancel_input{};
		GDExtensionOutputGenerationInputV1 generation_input{};
		uint32_t calls = 0;
		template <size_t N>
		static void status(const GDExtensionOutputStatusV1 &actual, Error error, const char (&detail)[N]) {
			CHECK(actual.error == error);
			CHECK(actual.detail_length == N - 1);
			REQUIRE(actual.detail != nullptr);
			if (actual.detail_length == N - 1) {
				CHECK(std::memcmp(actual.detail, detail, N - 1) == 0);
			}
		}
		bool load() {
			const char *names[] = { "device_session_output_create_v1", "device_session_output_seal_v1", "device_session_output_cancel_v1", "device_session_output_withdraw_v1" };
			for (const char *name : names) {
				CHECK(gdextension_get_proc_address(name) == nullptr);
				REQUIRE(gdextension_get_guarded_proc_address(name) != nullptr);
			}
			create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV1>(gdextension_get_guarded_proc_address(names[0]));
			seal = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputSealV1>(gdextension_get_guarded_proc_address(names[1]));
			cancel = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCancelV1>(gdextension_get_guarded_proc_address(names[2]));
			withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address(names[3]));
			// The compatibility names remain exactly callable pointer aliases.
			const GDExtensionInterfaceOutputCreateV1 old_create = create;
			const GDExtensionInterfaceOutputSealV1 old_seal = seal;
			const GDExtensionInterfaceOutputCancelV1 old_cancel = cancel;
			const GDExtensionInterfaceOutputWithdrawV1 old_withdraw = withdraw;
			return old_create && old_seal && old_cancel && old_withdraw;
		}
		void initialize_inputs() {
			using namespace GDExtensionDeviceExit;
			config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
			config.profile = 3; // Real schema-valid scene-only syntax, no source grant.
			config.max_views = 1;
			config.max_inflight = 2;
			config.space = 1;
			config.query_enabled = 1;
			for (uint32_t i = 0; i < 3; ++i) {
				config.bounds.size[i] = config.query_bounds.size[i] = 1;
				seal_input.common.bounds.size[i] = 1;
			}
			seal_input.abi = production_dto_abi<GDExtensionOutputSealInputV1>();
			seal_input.common.current_camera = { 1, 1, 1 };
			cancel_input.abi = production_dto_abi<GDExtensionOutputCancelInputV1>();
			cancel_input.particles = native->particle_rid().get_id();
			native->copy_lease_token(0, cancel_input.lease_token.bytes);
			generation_input.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
			generation_input.particles = native->particle_rid().get_id();
			generation_input.generation = native->generation;
		}
		void rejected(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 ref) {
			const bool first = native->slot_reserved(0), second = native->slot_reserved(1), closing = native->withdrawing();
			const auto revision = fixture->request->revision;
			const auto binding = fixture->request->binding_used;
			const auto head = fixture->request->output_owner_head, cursor = fixture->request->output_owner_cursor;
			GDExtensionOutputCreateResultV1 created{};
			GDExtensionOutputSealResultV1 sealed{};
			create(context, ref, &config, &created);
			seal(context, ref, &seal_input, &sealed);
			status(created.status, ERR_UNAUTHORIZED, "original_record_context_required");
			status(sealed.status, ERR_UNAUTHORIZED, "original_record_context_required");
			status(cancel(context, ref, &cancel_input), ERR_UNAUTHORIZED, "original_record_context_required");
			status(withdraw(context, ref, &generation_input), ERR_UNAUTHORIZED, "original_record_context_required");
			CHECK(created.particles == 0);
			CHECK(created.generation == 0);
			CHECK(created.capabilities == 0);
			CHECK(sealed.state == 0);
			CHECK(native->slot_reserved(0) == first);
			CHECK(native->slot_reserved(1) == second);
			CHECK(native->withdrawing() == closing);
			CHECK(fixture->request->revision == revision);
			CHECK(fixture->request->binding_used == binding);
			CHECK(fixture->request->output_owner_head == head);
			CHECK(fixture->request->output_owner_cursor == cursor);
		}
		void invalid_inputs(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 ref) {
			const auto revision = fixture->request->revision;
			const auto binding = fixture->request->binding_used;
			const auto head = fixture->request->output_owner_head, cursor = fixture->request->output_owner_cursor;
			GDExtensionOutputCreateResultV1 created;
			GDExtensionOutputSealResultV1 sealed;
			std::memset(&created, 0x5a, sizeof(created));
			std::memset(&sealed, 0x5a, sizeof(sealed));
			uint8_t before_create[sizeof(created)], before_seal[sizeof(sealed)];
			std::memcpy(before_create, &created, sizeof(created));
			std::memcpy(before_seal, &sealed, sizeof(sealed));
			alignas(GDExtensionOutputCreateInputV1) uint8_t bad_create[sizeof(config) + 1]{};
			alignas(GDExtensionOutputSealInputV1) uint8_t bad_seal[sizeof(seal_input) + 1]{};
			create(context, ref, nullptr, &created);
			create(context, ref, reinterpret_cast<const GDExtensionOutputCreateInputV1 *>(bad_create + 1), &created);
			seal(context, ref, nullptr, &sealed);
			seal(context, ref, reinterpret_cast<const GDExtensionOutputSealInputV1 *>(bad_seal + 1), &sealed);
			CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
			CHECK(std::memcmp(&sealed, before_seal, sizeof(sealed)) == 0);
			create(context, ref, reinterpret_cast<const GDExtensionOutputCreateInputV1 *>(UINTPTR_MAX - alignof(GDExtensionOutputCreateInputV1) + 1), &created);
			seal(context, ref, reinterpret_cast<const GDExtensionOutputSealInputV1 *>(UINTPTR_MAX - alignof(GDExtensionOutputSealInputV1) + 1), &sealed);
			CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
			CHECK(std::memcmp(&sealed, before_seal, sizeof(sealed)) == 0);
			create(context, ref, &config, nullptr);
			seal(context, ref, &seal_input, nullptr);
			alignas(GDExtensionOutputCreateResultV1) uint8_t bad_result[sizeof(created) + 1]{};
			uint8_t result_before[sizeof(bad_result)];
			std::memcpy(result_before, bad_result, sizeof(bad_result));
			create(context, ref, &config, reinterpret_cast<GDExtensionOutputCreateResultV1 *>(bad_result + 1));
			CHECK(std::memcmp(bad_result, result_before, sizeof(bad_result)) == 0);
			alignas(GDExtensionOutputSealResultV1) uint8_t bad_seal_result[sizeof(sealed) + 1]{};
			uint8_t seal_result_before[sizeof(bad_seal_result)];
			std::memcpy(seal_result_before, bad_seal_result, sizeof(bad_seal_result));
			seal(context, ref, &seal_input, reinterpret_cast<GDExtensionOutputSealResultV1 *>(bad_seal_result + 1));
			CHECK(std::memcmp(bad_seal_result, seal_result_before, sizeof(bad_seal_result)) == 0);
			auto aliased_create = config;
			const auto input_before = aliased_create;
			create(context, ref, &aliased_create, reinterpret_cast<GDExtensionOutputCreateResultV1 *>(&aliased_create));
			CHECK(std::memcmp(&aliased_create, &input_before, sizeof(config)) == 0);
			auto aliased_seal = seal_input;
			const auto seal_before = aliased_seal;
			seal(context, ref, &aliased_seal, reinterpret_cast<GDExtensionOutputSealResultV1 *>(&aliased_seal));
			CHECK(std::memcmp(&aliased_seal, &seal_before, sizeof(seal_input)) == 0);
			for (uint32_t which = 0; which < 2; ++which) {
				auto input = config;
				auto &span = which ? input.surface_materials : input.surfaces;
				span.count = 2;
				span.data = nullptr;
				create(context, ref, &input, &created);
				CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
				span.data = reinterpret_cast<const uint64_t *>(bad_create + 1);
				create(context, ref, &input, &created);
				CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
				span.data = reinterpret_cast<const uint64_t *>(UINTPTR_MAX - 7);
				create(context, ref, &input, &created);
				CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
				span.data = reinterpret_cast<const uint64_t *>(&created);
				create(context, ref, &input, &created);
				CHECK(std::memcmp(&created, before_create, sizeof(created)) == 0);
			}
			uint64_t rid = 0;
			auto overlap = config;
			overlap.surfaces = { &rid, 1, 0 };
			overlap.surface_materials = { &rid, 1, 0 };
			create(context, ref, &overlap, &created);
			status(created.status, ERR_INVALID_PARAMETER, "CONTRACT");
			overlap = config;
			overlap.surfaces = { reinterpret_cast<const uint64_t *>(&overlap), 1, 0 };
			create(context, ref, &overlap, &created);
			status(created.status, ERR_INVALID_PARAMETER, "CONTRACT");
			for (uint32_t field = 0; field < 3; ++field) {
				auto bad_config = config;
				auto bad_seal_input = seal_input;
				auto bad_cancel = cancel_input;
				auto bad_generation = generation_input;
				GDExtensionDeviceExitAbi *headers[] = { &bad_config.abi, &bad_seal_input.abi, &bad_cancel.abi, &bad_generation.abi };
				for (auto *header : headers) {
					if (field == 0) {
						header->abi_version = 2;
					}
					if (field == 1) {
						--header->struct_size;
					}
					if (field == 2) {
						header->reserved = 1;
					}
				}
				create(context, ref, &bad_config, &created);
				seal(context, ref, &bad_seal_input, &sealed);
				status(created.status, ERR_INVALID_PARAMETER, "CONTRACT");
				status(sealed.status, ERR_INVALID_PARAMETER, "CONTRACT");
				status(cancel(context, ref, &bad_cancel), ERR_INVALID_PARAMETER, "CONTRACT");
				status(withdraw(context, ref, &bad_generation), ERR_INVALID_PARAMETER, "CONTRACT");
			}
			status(cancel(context, ref, nullptr), ERR_INVALID_PARAMETER, "CONTRACT");
			status(withdraw(context, ref, nullptr), ERR_INVALID_PARAMETER, "CONTRACT");
			status(cancel(context, ref, reinterpret_cast<const GDExtensionOutputCancelInputV1 *>(bad_create + 1)), ERR_INVALID_PARAMETER, "CONTRACT");
			status(withdraw(context, ref, reinterpret_cast<const GDExtensionOutputGenerationInputV1 *>(bad_create + 1)), ERR_INVALID_PARAMETER, "CONTRACT");
			for (uint32_t which = 0; which < 2; ++which) {
				auto bad_span = config;
				(which ? bad_span.surface_materials : bad_span.surfaces).reserved = 1;
				create(context, ref, &bad_span, &created);
				status(created.status, ERR_INVALID_PARAMETER, "CONTRACT");
			}
			CHECK(fixture->request->revision == revision);
			CHECK(fixture->request->binding_used == binding);
			CHECK(fixture->request->output_owner_head == head);
			CHECK(fixture->request->output_owner_cursor == cursor);
			CHECK(native->slot_reserved(0));
			CHECK(native->slot_reserved(1));
			CHECK_FALSE(native->withdrawing());
		}
		static void foreign_receive(void *userdata, GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<OutputLeafProbe *>(userdata);
			++probe.calls;
			CHECK(ref.handle_serial != probe.fixture->request->serial);
			CHECK_FALSE(probe.native->withdrawing());
			CHECK(probe.native->view_open());
			CHECK(probe.native->slot_reserved(1));
			const auto revision = probe.fixture->request->revision;
			const auto head = probe.fixture->request->output_owner_head, cursor = probe.fixture->request->output_owner_cursor;
			auto target = probe.cancel_input;
			probe.native->copy_lease_token(1, target.lease_token.bytes);
			status(probe.cancel(context, ref, &target), ERR_UNAUTHORIZED, "original_output_owner_required");
			status(probe.withdraw(context, ref, &probe.generation_input), ERR_UNAVAILABLE, "original_output_generation_owner_required");
			CHECK(probe.native->slot_reserved(1));
			CHECK_FALSE(probe.native->withdrawing());
			CHECK(probe.native->view_open());
			CHECK_FALSE(probe.native->pin(0).is_empty());
			CHECK_FALSE(probe.native->pin(1).is_empty());
			CHECK(probe.fixture->request->revision == revision);
			CHECK(probe.fixture->request->output_owner_head == head);
			CHECK(probe.fixture->request->output_owner_cursor == cursor);
		}
		static void receive(void *userdata, GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<OutputLeafProbe *>(userdata);
			auto &fixture = *probe.fixture;
			auto &native = *probe.native;
			if (ref.handle_serial != fixture.request->serial) {
				foreign_receive(userdata, context, ref, nullptr);
				return;
			}
			if (probe.calls) {
				++probe.calls;
				status(probe.withdraw(context, ref, &probe.generation_input), ERR_UNAVAILABLE, "original_output_generation_owner_required");
				CHECK_FALSE(native.withdrawing());
				CHECK(native.view_open());
				CHECK(native.slot_reserved(1));
				CHECK_FALSE(native.pin(0).is_empty());
				CHECK_FALSE(native.pin(1).is_empty());
				return;
			}
			++probe.calls;
			probe.expired = context;
			for (uint32_t i = 0; i < 2; ++i) {
				native.install(i, fixture.registry, ref);
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, native.pin(i)) == OK);
				REQUIRE(fixture.registry.production_output_owner_bind(native.pin(i), fixture.rd, native.particle_rid(), native.generation, i, native.lease(i)) == OK);
			}
			probe.initialize_inputs();
			auto wrong = context;
			++wrong.frame_serial;
			probe.rejected(wrong, ref);
			probe.rejected(context, { ref.registry_instance, UINT64_MAX });
			{
				GDExtensionDeviceExit::CallbackScope nested(fixture.reference);
				REQUIRE(nested.get_error() == OK);
				probe.rejected(context, ref);
			}
			probe.invalid_inputs(context, ref);
			GDExtensionOutputCreateResultV1 created{};
			GDExtensionOutputSealResultV1 sealed{};
			probe.create(context, ref, &probe.config, &created);
			status(created.status, ERR_UNAVAILABLE, "original_output_admission_required");
			CHECK(created.particles == 0);
			CHECK(created.generation == 0);
			CHECK(created.capabilities == 0);
			probe.seal(context, ref, &probe.seal_input, &sealed);
			status(sealed.status, ERR_INVALID_PARAMETER, "external_lease_required");
			CHECK(sealed.state == 0);
			auto foreign_cancel = probe.cancel_input;
			foreign_cancel.particles = 0;
			auto foreign_generation = probe.generation_input;
			foreign_generation.generation = native.generation + 1;
			status(probe.cancel(context, ref, &foreign_cancel), ERR_INVALID_PARAMETER, "output_cancel");
			status(probe.withdraw(context, ref, &foreign_generation), ERR_INVALID_PARAMETER, "stale_generation");
			CHECK(native.slot_reserved(0));
			CHECK(native.slot_reserved(1));
			status(probe.cancel(context, ref, &probe.cancel_input), OK, "");
			CHECK_FALSE(native.slot_reserved(0));
			CHECK(native.slot_reserved(1));
			CHECK_FALSE(native.withdrawing());
		}
	};
	static void output_registered_leaves(uint32_t custody_case) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		OutputLeafProbe probe;
		REQUIRE(probe.load());
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 2, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		plan.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		probe.fixture = &fixture;
		REQUIRE(ordinal_initialize(fixture, server, plan, &OutputLeafProbe::receive, &probe) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		native.prepare_lease_tokens(); // Test metadata only, allocated before RECORD.
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		probe.rejected(probe.expired, ref); // Same scalars after the actual RECORD expired.
		const auto reserve_v2 = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		REQUIRE(reserve_v2 != nullptr);
		auto release_empty = [&](GDExtensionProductionRequestRefV1 empty_ref, Request *empty_request) {
			for (uint32_t turn = 0; turn < 8 && empty_request->serial; ++turn) {
				const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				GDExtensionProductionServiceBudgetV1 remaining{};
				GDExtensionProductionReportV2 report{};
				const Error error = Error(release(empty_ref, &budget, &remaining, &report).native_error);
				CHECK((error == OK || error == ERR_BUSY));
				if (empty_request->serial) {
					server->sync();
				}
			}
			CHECK(empty_request->serial == 0);
		};
		if (custody_case == 0) {
			// A distinct real library/producer attachment to the SAME existing RD.
			// It neither swaps the singleton nor fabricates a second request row.
			SingletonEnrollment foreign;
			foreign.public_port = true;
			foreign.server = server;
			OrdinalPlan other_plan;
			other_plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			other_plan.action_count = other_plan.resource_count = other_plan.use_count = other_plan.readback_count = 0;
			REQUIRE(foreign.initialize_owner(1, false, false, &OutputLeafProbe::foreign_receive, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &other_plan, &fixture.rd) == OK);
			REQUIRE(foreign.request->attachment != request.attachment);
			CHECK(foreign.request->attachment->held_device == &fixture.rd);
			const GDExtensionProductionRequestRefV1 foreign_ref = { foreign.instance, foreign.request->serial };
			uint64_t foreign_control = 0;
			REQUIRE(foreign.port.record_schedule(foreign_ref, &foreign_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 foreign_report{};
			REQUIRE(consume(foreign_ref, foreign_control, &foreign_report).native_error == OK);
			CHECK(probe.calls == 2);
			release_empty(foreign_ref, foreign.request);
			foreign.cleanup_owner();
			CHECK(foreign.owner_cleaned);
		}
		if (custody_case == 1) {
			GDExtensionProductionCapacityV1 empty_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			auto successor_operation = plan.operation;
			++successor_operation.owner_request_nonce;
			GDExtensionProductionRequestRefV1 successor_ref{};
			REQUIRE(reserve_v2(fixture.attachment, &successor_operation, &empty_capacity, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &successor_ref).native_error == OK);
			Request *successor = nullptr;
			{
				MutexLock lock(fixture.registry.mutex);
				RenderingDevice *same_device = nullptr;
				REQUIRE(fixture.registry.production_request_locked(successor_ref, false, successor, same_device) == OK);
				REQUIRE(successor->attachment == request.attachment);
				CHECK(same_device == &fixture.rd);
			}
			uint64_t successor_control = 0;
			REQUIRE(fixture.port.record_schedule(successor_ref, &successor_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 successor_report{};
			REQUIRE(consume(successor_ref, successor_control, &successor_report).native_error == OK);
			CHECK(probe.calls == 2);
			release_empty(successor_ref, successor);
		}
		if (custody_case == 2) {
			// A fresh fixture reaches this original RECORD before any destructive
			// RED cleanup leaf; no production owner state is reset or repaired.
			uint64_t final_control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &final_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 final_report{};
			REQUIRE(consume(ref, final_control, &final_report).native_error == OK);
			CHECK(probe.calls == 2);
		}

		// Typed withdraw has no generation-level issuer; use the preexisting
		// private metadata cleanup helper, never report typed withdraw success.
		native.withdraw();
		for (uint32_t turn = 0; turn < 3; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			ProductionControlOutput output{};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK(output.result.native_error == OK);
			CHECK(output.service_usage.controls == 1);
			CHECK(output.service_usage.passes == 1);
			CHECK(output.service_usage.receipts == 0);
			CHECK(request.capture_inflight == 0);
			CHECK(native.pin(0).is_empty() == (turn == 2));
			CHECK(native.pin(1).is_empty() == (turn >= 1));
		}
		CHECK(native.released());
		CHECK(request.output_owner_head == UINT32_MAX);
		CHECK(request.output_owner_cursor == UINT32_MAX);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			const Error error = Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}

	static void generation_root_drain_cursor(SingletonEnrollment &fixture, GDExtensionProductionRequestRefV1 ref, RenderingServerDefault *server) {
		using namespace GDExtensionDeviceExit;
		auto &request = *fixture.request;
		REQUIRE(request.control.state == ProductionControlState::OWNER_CONSUMED);
		// Same original v2 dispatch/consume path as the runtime-corrected prefix.
		// Every turn gets a fresh one-pass budget; no v1 mailbox projection.
		for (uint32_t turn = 0; turn < 128 && request.maintenance_serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			const uint32_t receipts_before = fixture.driver.observed_disposal_receipts() + fixture.driver.raster_frees;
			ProductionControlOutput output{};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			CHECK(output.service_usage.receipts <= 1);
			CHECK(output.service_usage.receipts == fixture.driver.observed_disposal_receipts() + fixture.driver.raster_frees - receipts_before);
			CHECK_FALSE(output.output_generation.native_released);
		}
		CHECK(request.maintenance_serial == 0);
	}
	struct GenerationRootProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		uint32_t slots = 0, calls = 0;
		uint32_t directory_row = UINT32_MAX;
		uint64_t directory_serial = 0;
		bool pressure = false, root_group = false, service_group = false, group_settled = false, directory_hold = false;
		uint64_t group = 0;
		uint32_t group_turns = 0;
		static GenerationRootProbe *&active() {
			static thread_local GenerationRootProbe *probe = nullptr;
			return probe;
		}
		static Error observe_component(GDExtensionDeviceExit::ProductionRequest &request, RenderingDevice &rd) {
			using namespace GDExtensionDeviceExit;
			auto *probe = active();
			if (!probe || &request != probe->fixture->request || &rd != &probe->fixture->rd) {
				return ERR_UNAUTHORIZED;
			}
			if (probe->native->generation_pin().is_empty()) {
				return OK;
			}
			auto &control = request.control;
			const auto before = control.output.service_usage;
			auto copied_budget = control.input.budget;
			ProductionServiceUsage copied_usage{};
			ProductionDisposalObservation report;
			CHECK(rd._production_output_retire_service(probe->native->generation_pin(), 0, copied_budget, control.output.service_usage, report) == ERR_UNAUTHORIZED);
			CHECK(rd._production_output_retire_service(probe->native->generation_pin(), 0, control.input.budget, copied_usage, report) == ERR_UNAUTHORIZED);
			CHECK(control.output.service_usage.controls == before.controls);
			CHECK(control.output.service_usage.passes == before.passes);
			CHECK(control.output.service_usage.receipts == before.receipts);
			CHECK(copied_usage.controls == 0);
			CHECK(copied_usage.passes == 0);
			CHECK(copied_usage.receipts == 0);
			if (!probe->service_group || probe->group_settled) {
				return OK;
			}
			// Existing original OBSERVE test hook; call only the real group
			// service with the exact original references, never write cell state.
			++probe->group_turns;
			const Error error = rd._production_output_retire_service(probe->native->generation_pin(), probe->group, control.input.budget, control.output.service_usage, report);
			if (error == OK) {
				CHECK(report.settled);
				CHECK(report.uncreated == 1);
				CHECK(report.held == 0);
				CHECK(report.queued == 0);
				CHECK(report.physical_returned == 0);
				CHECK(report.unknown == 0);
				probe->group_settled = true;
			}
			return error;
		}
		uint64_t root_serial = 0;
		uint32_t root_index = UINT32_MAX;

		void check_root_unchanged(uint64_t next, uint32_t head, uint32_t cursor) const {
			using namespace GDExtensionDeviceExit;
			const auto &request = *fixture->request;
			CHECK_FALSE(native->generation_pin().is_empty());
			CHECK(native->generation_serial() == root_serial);
			CHECK(request.next_output_pin_serial == next);
			CHECK(request.output_owner_head == head);
			CHECK(request.output_owner_cursor == cursor);
			const auto &row = request.uses[root_index];
			CHECK(row.output_consumer == &native->generation_pin());
			CHECK(row.output_pin_serial == root_serial);
			CHECK(row.output_owner_kind == ProductionOutputOwnerKind::GENERATION);
			CHECK(row.output_particles == native->particle_rid());
			CHECK(row.output_generation == native->generation);
			CHECK(row.output_slot == UINT32_MAX);
			CHECK(row.output_lease == 0);
		}
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef context,
				GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<GenerationRootProbe *>(userdata);
			auto &fixture = *probe.fixture;
			auto &native = *probe.native;
			auto &request = *fixture.request;
			++probe.calls;
			REQUIRE(RegistrationContext::current != nullptr);
			if (ref.handle_serial != request.serial) {
				// A second accepted request in the SAME attachment has its own
				// genuine protected RECORD, but no successor-to-generation grant.
				const auto next = request.next_output_pin_serial;
				const auto head = request.output_owner_head, cursor = request.output_owner_cursor;
				CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, UINT32_MAX, 0, ProductionOutputOwnerKind::GENERATION) == ERR_UNAUTHORIZED);
				CHECK_FALSE(fixture.rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, native.particle_rid(), native.generation, UINT32_MAX, 0, native.generation_pin(), ref));
				probe.check_root_unchanged(next, head, cursor);
				CHECK_FALSE(native.withdrawing());
				return;
			}
			REQUIRE(request.control.state == ProductionControlState::EXECUTING);
			REQUIRE(request.control.input.kind == ProductionControlKind::RECORD);
			const auto before = request.next_output_pin_serial;
			auto wrong = context;
			++wrong.frame_serial;
			CHECK(native.install_generation(wrong, ref, fixture.registry) == ERR_UNAUTHORIZED);
			CHECK(native.install_generation(context, { ref.registry_instance, UINT64_MAX }, fixture.registry) == ERR_UNAUTHORIZED);
			CHECK(native.install_generation(context, { UINT64_MAX, ref.handle_serial }, fixture.registry) == ERR_UNAUTHORIZED);
			CHECK(native.generation_uninstalled());
			CHECK(request.next_output_pin_serial == before);
			CHECK(request.output_owner_head == UINT32_MAX);
			CHECK(request.output_owner_cursor == UINT32_MAX);
			if (probe.pressure) {
				REQUIRE(request.capacity.resource_uses == 1);
				ProductionOutputPin occupied;
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, occupied) == OK);
				const auto full = request.next_output_pin_serial;
				CHECK(native.install_generation(context, ref, fixture.registry) == ERR_OUT_OF_MEMORY);
				CHECK(native.generation_uninstalled());
				CHECK(request.next_output_pin_serial == full);
				CHECK(fixture.registry.production_output_pin_matches(occupied, ref, fixture.rd));
				REQUIRE(fixture.registry.production_output_unpin(occupied) == OK);
				CHECK(occupied.is_empty());
				// An issued pin with a rejected locator returns its exact real row.
				// The consumed pin serial is not rewound or reused.
				CHECK(native.install_generation_at(context, ref, fixture.registry, RID()) == ERR_UNAUTHORIZED);
				CHECK(native.generation_uninstalled());
				CHECK(request.next_output_pin_serial == full + 1);
				CHECK(request.output_owner_head == UINT32_MAX);
				CHECK(request.uses[0].output_role == ProductionUseCell::OutputRole::UNUSED);
				CHECK(request.uses[0].output_owner_kind == ProductionOutputOwnerKind::NONE);
				CHECK(request.uses[0].output_consumer == nullptr);
				CHECK(request.uses[0].output_pin_serial == 0);
			}
			REQUIRE(native.install_generation(context, ref, fixture.registry) == OK);
			probe.root_serial = native.generation_serial();
			REQUIRE(probe.root_serial != 0);
			for (uint32_t i = 0; i < request.capacity.resource_uses; ++i) {
				if (request.uses[i].output_consumer == &native.generation_pin()) {
					probe.root_index = i;
				}
			}
			REQUIRE(probe.root_index != UINT32_MAX);
			const auto next = request.next_output_pin_serial;
			const auto head = request.output_owner_head, cursor = request.output_owner_cursor;
			CHECK(native.install_generation(context, ref, fixture.registry) == ERR_ALREADY_IN_USE);
			CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, RID(), native.generation, UINT32_MAX, 0, ProductionOutputOwnerKind::GENERATION) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation + 1, UINT32_MAX, 0, ProductionOutputOwnerKind::GENERATION) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, 0, 0, ProductionOutputOwnerKind::GENERATION) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, 0, native.lease(0), ProductionOutputOwnerKind::SLOT) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, UINT32_MAX, 0, ProductionOutputOwnerKind::GENERATION) == OK);
			CHECK_FALSE(fixture.rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, native.particle_rid(), native.generation, UINT32_MAX, 0, native.generation_pin(), { ref.registry_instance, UINT64_MAX }));
			CHECK(fixture.registry.production_output_unpin(native.generation_pin()) == ERR_BUSY);
			GDExtensionProductionServiceBudgetV1 copied_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			ProductionServiceUsage copied_usage{};
			CHECK(fixture.registry.production_output_generation_return(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, copied_budget, copied_usage) == ERR_UNAUTHORIZED);
			CHECK(copied_usage.controls == 0);
			CHECK(copied_usage.passes == 0);
			CHECK(copied_usage.receipts == 0);
			CHECK_FALSE(request.control.output.output_generation.native_released);
			probe.check_root_unchanged(next, head, cursor);
			// The private generation root still grants no public Withdraw issuer.
			const auto withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
			REQUIRE(withdraw != nullptr);
			GDExtensionOutputGenerationInputV1 input{};
			input.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
			input.particles = native.particle_rid().get_id();
			input.generation = native.generation;
			OutputLeafProbe::status(withdraw(context, ref, &input), ERR_UNAVAILABLE, "original_output_generation_owner_required");
			CHECK_FALSE(native.withdrawing());
			for (uint32_t i = 0; i < probe.slots; ++i) {
				native.install(i, fixture.registry, ref);
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, native.pin(i)) == OK);
				REQUIRE(fixture.registry.production_output_owner_bind(native.pin(i), fixture.rd, native.particle_rid(), native.generation, i, native.lease(i)) == OK);
			}
			if (probe.root_group) {
				REQUIRE(fixture.rd._production_output_owned_reserve(context, native.generation_pin(), 1, probe.group) == OK);
				REQUIRE(probe.group != 0);
				ProductionDisposalObservation held;
				REQUIRE(fixture.rd._production_output_hold_returned(native.generation_pin(), probe.group, held) == OK);
				CHECK_FALSE(held.dropped);
				CHECK_FALSE(held.settled);
				CHECK(held.uncreated == 0); // The real reservation has not been returned.
			}
			if (probe.directory_hold) {
				// This is a real Registry-issued borrow into the original finite
				// directory, not an acquired view or a fabricated completion.
				REQUIRE(fixture.registry.production_output_pin_borrow(native.generation_pin(), fixture.rd, native.generation_directory_pin()) == OK);
				CHECK(fixture.registry.production_output_pin_matches(native.generation_directory_pin(), ref, fixture.rd));
				for (uint32_t i = 0; i < request.capacity.resource_uses; ++i) {
					const auto &row = request.uses[i];
					if (row.output_role == ProductionUseCell::OutputRole::NATIVE_CONSUMER && row.output_consumer == &native.generation_directory_pin()) {
						REQUIRE(probe.directory_row == UINT32_MAX);
						probe.directory_row = i;
						probe.directory_serial = row.output_pin_serial;
					}
				}
				REQUIRE(probe.directory_row != UINT32_MAX);
				CHECK(probe.directory_serial != 0);
				CHECK(probe.directory_serial != probe.root_serial);
			}
			ProductionOutputPin excess;
			const auto full = request.next_output_pin_serial;
			CHECK(fixture.registry.production_output_pin_current(fixture.rd, excess) == ERR_OUT_OF_MEMORY);
			CHECK(excess.is_empty());
			CHECK(request.next_output_pin_serial == full);
			CHECK(native.generation_serial() == probe.root_serial);
			CHECK_FALSE(native.generation_pin().is_empty());
		}
	};

	static void generation_root_observe(uint32_t slots, bool pressure = false, bool directory = false, bool root_group = false, bool directory_hold = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		REQUIRE((slots == 0 || slots == 2));
		const auto consume_record = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		REQUIRE(consume_record != nullptr);
		REQUIRE(observe != nullptr);
		REQUIRE(release != nullptr);
		REQUIRE(reserve != nullptr);
		OrdinalPlan plan;
		// Root + slots, and only the group row/member actually used by the group case.
		REQUIRE((!directory_hold || (directory && !slots && !root_group && !pressure)));
		const uint32_t cells = 1 + slots + (root_group ? 2 : 0) + uint32_t(directory_hold);
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, cells, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		plan.attachment_requests = 2; // One real same-attachment negative-control request.
		SingletonEnrollment fixture;
		fixture.public_port = true;
		GenerationRootProbe probe;
		probe.fixture = &fixture;
		probe.slots = slots;
		probe.pressure = pressure;
		probe.root_group = root_group;
		probe.directory_hold = directory_hold;
		REQUIRE(ordinal_initialize(fixture, server, plan, &GenerationRootProbe::receive, &probe) == OK);
		if (root_group) {
			// Actual empty graph, so no unrelated timestamp command can masquerade
			// as this original empty request's captured source.
			fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
			fixture.rd.draw_graph.begin();
			fixture.rd.frames[0].fence = RDD::FenceID(93);
			fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
			fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
			fixture.rd.main_queue = RDD::CommandQueueID(1);
		}
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		probe.native = &native;
		if (directory) {
			REQUIRE(native.prepare_generation_directory() == OK);
		}
		auto &request = *fixture.request;
		REQUIRE(request.capacity.resource_uses == cells);
		CHECK(request.attachment->quote.capacity.resource_uses == cells);
		auto total = plan.capacity;
		total.requests = plan.attachment_requests;
		GDExtensionProductionMetadataQuoteV1 actual_quote{};
		REQUIRE(quote(total, actual_quote) == OK);
		CHECK(actual_quote.allocation_bytes == request.attachment->quote.allocation_bytes);
		CHECK(actual_quote.allocation_bytes >= sizeof(ProductionAttachment) + 2 * sizeof(ProductionRequest) + cells * sizeof(ProductionUseCell));
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume_record(ref, control, &recorded).native_error == OK);
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0); // Genuine empty original source, no completion injection.
		CHECK(native.generation_serial() == probe.root_serial);
		CHECK_FALSE(native.generation_pin().is_empty());
		CHECK(request.output_owner_cursor == probe.root_index);
		if (!slots) {
			CHECK(native.pin(0).is_empty());
			CHECK(native.pin(1).is_empty());
			CHECK_FALSE(native.slot_reserved(0));
			CHECK_FALSE(native.slot_reserved(1));
		}
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		auto release_request = [&](GDExtensionProductionRequestRefV1 target, Request *actual) {
			for (uint32_t turn = 0; turn < (root_group ? 16u : 8u) && actual->serial; ++turn) {
				GDExtensionProductionServiceBudgetV1 remaining{};
				GDExtensionProductionReportV2 report{};
				const Error error = Error(release(target, &budget, &remaining, &report).native_error);
				CHECK((error == OK || error == ERR_BUSY));
				if (actual->serial) {
					server->sync();
				}
			}
			CHECK(actual->serial == 0);
		};
		GDExtensionProductionCapacityV1 empty = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		auto other_operation = plan.operation;
		++other_operation.owner_request_nonce;
		GDExtensionProductionRequestRefV1 other_ref{};
		REQUIRE(reserve(fixture.attachment, &other_operation, &empty, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &other_ref).native_error == OK);
		Request *other = nullptr;
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *same_device = nullptr;
			REQUIRE(fixture.registry.production_request_locked(other_ref, false, other, same_device) == OK);
			REQUIRE(other->attachment == request.attachment);
			REQUIRE(same_device == &fixture.rd);
		}
		uint64_t other_control = 0;
		REQUIRE(fixture.port.record_schedule(other_ref, &other_control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 other_record{};
		REQUIRE(consume_record(other_ref, other_control, &other_record).native_error == OK);
		CHECK(probe.calls == 2);
		release_request(other_ref, other);

		native.withdraw_generation();
		native.withdraw_generation(); // Idempotent logical close; no fabricated history.
		CHECK(native.withdrawing());
		CHECK(native.history_cleared());
		struct ComponentHookScope {
			decltype(test_production_before_maintenance) previous = test_production_before_maintenance;
			GenerationRootProbe *prior = GenerationRootProbe::active();
			~ComponentHookScope() {
				test_production_before_maintenance = previous;
				GenerationRootProbe::active() = prior;
			}
		} hook_scope;
		GenerationRootProbe::active() = &probe;
		test_production_before_maintenance = &GenerationRootProbe::observe_component;
		auto dispatch = [&](const GDExtensionProductionServiceBudgetV1 &limit) {
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &limit) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(request.control.state == ProductionControlState::RETURNED);
			CHECK(request.capture_inflight == 0);
			return serial;
		};
		for (uint32_t idle = 0; idle < 2; ++idle) {
			auto limit = budget;
			if (!idle) {
				limit.controls = limit.passes = limit.receipts = 0;
			} else {
				limit.deadline_micros = 1;
			}
			const auto cursor = request.output_owner_cursor;
			const uint64_t serial = dispatch(limit);
			ProductionControlOutput output{};
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK(output.service_usage.controls == 0);
			CHECK(output.service_usage.passes == 0);
			CHECK(output.service_usage.receipts == 0);
			CHECK_FALSE(output.output_generation.native_released);
			CHECK(request.output_owner_cursor == cursor);
			CHECK_FALSE(native.generation_pin().is_empty());
			CHECK_FALSE(native.released());
			CHECK(native.generation_directory_empty() == !directory);
		}
		if (directory_hold) {
			const auto borrowed_serial = probe.directory_serial;
			REQUIRE(probe.directory_row < request.capacity.resource_uses);
			const auto next = request.next_output_pin_serial;
			const auto head = request.output_owner_head, cursor = request.output_owner_cursor;
			void *const arena = request.attachment->arena;
			const auto children = fixture.holder.production_children;
			for (uint32_t blocked = 0; blocked < 3; ++blocked) {
				const uint64_t serial = dispatch(budget);
				const auto &output = request.control.output;
				CHECK(output.result.native_error == ERR_BUSY);
				CHECK(output.service_usage.controls == 1);
				CHECK(output.service_usage.passes == 0);
				CHECK(output.service_usage.receipts == 0);
				CHECK_FALSE(output.output_generation.native_released);
				CHECK(output.output_generation.root_serial == 0);
				probe.check_root_unchanged(next, head, cursor);
				CHECK_FALSE(native.generation_directory_empty());
				CHECK_FALSE(native.released());
				CHECK(request.uses[probe.directory_row].output_pin_serial == borrowed_serial);
				CHECK(request.uses[probe.directory_row].output_role == ProductionUseCell::OutputRole::NATIVE_CONSUMER);
				CHECK(request.uses[probe.directory_row].output_consumer == &native.generation_directory_pin());
				CHECK(fixture.registry.production_output_pin_matches(native.generation_directory_pin(), ref, fixture.rd));
				CHECK(request.attachment->arena == arena);
				CHECK(fixture.holder.production_children == children);
				CHECK(children != 0);
				CHECK(fixture.driver.issues == 0);
				CHECK(fixture.driver.polls == 0);
				CHECK(request.action_count == 0);
				CHECK(request.graph_count == 0);
				CHECK(request.native_count == 0);
				ProductionControlOutput consumed{};
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &consumed) == OK);
				CHECK_FALSE(consumed.output_generation.native_released);
			}
			// Return only the independently issued original borrow. The directory
			// and terminal root still require their separate real OBSERVE passes.
			REQUIRE(fixture.registry.production_output_unpin(native.generation_directory_pin()) == OK);
			CHECK(native.generation_directory_pin().is_empty());
			CHECK_FALSE(native.generation_pin().is_empty());
			CHECK(native.generation_serial() == probe.root_serial);
			CHECK_FALSE(native.generation_directory_empty());
			CHECK_FALSE(native.released());
		}
		if (root_group) {
			// Root readiness reaches original unpin with a real outstanding group.
			// Its tentative terminal result must be erased without unlinking root.
			const auto head = request.output_owner_head, cursor = request.output_owner_cursor;
			const uint64_t blocked = dispatch(budget);
			CHECK(request.control.output.result.native_error == ERR_BUSY);
			CHECK_FALSE(request.control.output.output_generation.native_released);
			CHECK(request.control.output.output_generation.root_serial == 0);
			CHECK(request.control.output.service_usage.passes == 0);
			CHECK(request.control.output.service_usage.receipts == 0);
			CHECK_FALSE(native.generation_pin().is_empty());
			CHECK(request.output_owner_head == head);
			CHECK(request.output_owner_cursor == cursor);
			ProductionDisposalObservation retained;
			REQUIRE(fixture.rd._production_output_hold_returned(native.generation_pin(), probe.group, retained) == OK);
			CHECK_FALSE(retained.dropped);
			CHECK_FALSE(retained.settled);
			ProductionControlOutput blocked_output{};
			REQUIRE(fixture.registry.production_control_consume(ref, blocked, &blocked_output) == OK);
			probe.service_group = true;
		}
		// Root first sees live slots (0 passes), then each slot returns (1 each),
		// then root closes (1). A directory adds its own one-pass phase.
		const uint32_t turns = root_group ? 16 : slots ? 4
				: directory							   ? 2
													   : 1; // Finite retry bound, never extra per-turn credit.
		uint64_t terminal_control = 0;
		for (uint32_t turn = 0; turn < turns; ++turn) {
			const uint64_t serial = dispatch(budget);
			const auto &output = request.control.output;
			CHECK(output.service_usage.controls == 1);
			if (root_group) {
				CHECK(output.service_usage.passes <= 1);
			} else {
				CHECK(output.service_usage.passes == (slots && turn == 0 ? 0u : 1u));
			}
			CHECK(output.service_usage.receipts == 0);
			if (root_group) {
				CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			} else {
				CHECK(output.result.native_error == (slots && turn == 0 ? ERR_BUSY : OK));
			}
			if (root_group ? output.output_generation.native_released : turn + 1 == turns) {
				terminal_control = serial;
				break; // Retain the actual returned result, do not consume yet.
			}
			CHECK_FALSE(output.output_generation.native_released);
			CHECK_FALSE(native.generation_pin().is_empty());
			CHECK_FALSE(native.released());
			if (slots && turn == 2) {
				CHECK(native.pin(0).is_empty());
				CHECK(native.pin(1).is_empty());
				CHECK(request.output_owner_head == probe.root_index);
				CHECK(request.output_owner_cursor == probe.root_index);
			}
			if (directory) {
				CHECK(native.generation_directory_empty());
			}
			ProductionControlOutput consumed{};
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &consumed) == OK);
		}
		REQUIRE(terminal_control != 0);
		if (root_group) {
			CHECK(probe.group_settled);
			CHECK(probe.group_turns >= 3);
		}
		REQUIRE(request.control.output.output_generation.native_released);
		if (root_group) {
			// Original drop started its real native continuation. The terminal
			// pass leaves that continuation no credit, so later maintenance returns
			// Busy without erasing the already-saved generation result.
			CHECK(request.maintenance_serial != 0);
			CHECK(fixture.rd.native_disposal_active.load());
			CHECK(request.control.output.result.native_error == ERR_BUSY);
		}
		CHECK(native.released());
		CHECK(native.generation_pin().is_empty());
		CHECK(native.generation_serial() == probe.root_serial);
		CHECK_FALSE(native.generation_uninstalled()); // Returned correlation is not a never-installed root.
		CHECK(request.output_owner_head == UINT32_MAX);
		CHECK(request.output_owner_cursor == UINT32_MAX);
		CHECK(request.uses[probe.root_index].output_owner_kind == ProductionOutputOwnerKind::NONE);
		CHECK(request.uses[probe.root_index].output_consumer == nullptr);
		const auto &terminal = request.control.output.output_generation;
		CHECK(terminal.registry_instance == fixture.instance);
		CHECK(terminal.request_serial == ref.handle_serial);
		CHECK(terminal.control_serial == terminal_control);
		CHECK(terminal.root_serial == probe.root_serial);
		CHECK(terminal.particles == native.particle_rid().get_id());
		CHECK(terminal.generation == native.generation);
		CHECK(terminal.unused_slots == 2 - slots);
		CHECK(terminal.returned_slots == slots);
		CHECK(terminal.root_source.abi.struct_size == sizeof(GDExtensionProductionReportV2));
		CHECK(terminal.root_source.operation.owner_request_nonce == plan.operation.owner_request_nonce);
		CHECK(terminal.root_source.known_obligations == 0);
		CHECK(terminal.root_source.possible_obligations == 0);
		CHECK(terminal.root_source.complete_obligations == 0);
		CHECK(terminal.root_source.physical_returns_pending == 0);
		CHECK(terminal.root_source.cause.native_error == OK);
		uint8_t frozen[sizeof(ProductionControlOutput)];
		std::memcpy(frozen, &request.control.output, sizeof(frozen));
		void *const arena = request.attachment->arena;
		const auto children = fixture.holder.production_children;
		const auto next_control = request.next_control_serial;
		GDExtensionProductionReportV2 public_report{};
		auto query_budget = budget;
		query_budget.controls = query_budget.receipts = query_budget.passes = 0;
		CHECK(observe(ref, &query_budget, &public_report).native_error == OK);
		CHECK(std::memcmp(frozen, &request.control.output, sizeof(frozen)) == 0);
		CHECK(observe(ref, &budget, &public_report).native_error == ERR_UNAVAILABLE);
		CHECK(request.control.state == ProductionControlState::RETURNED);
		CHECK(std::memcmp(frozen, &request.control.output, sizeof(frozen)) == 0);
		uint64_t rejected = 0;
		CHECK(fixture.registry.production_control_accept(ref, ProductionControlKind::CANCEL, &rejected, &budget) == ERR_BUSY);
		CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
		CHECK(fixture.registry.production_close(fixture.attachment).native_error == ERR_BUSY);
		CHECK(request.attachment->arena == arena);
		CHECK(fixture.holder.production_children == children);
		CHECK(children != 0);
		CHECK(request.next_control_serial == next_control);
		CHECK(std::memcmp(frozen, &request.control.output, sizeof(frozen)) == 0);
		ProductionControlOutput consumed{};
		CHECK(fixture.registry.production_control_consume(ref, terminal_control + 1, &consumed) == ERR_DOES_NOT_EXIST);
		CHECK(fixture.registry.production_control_consume({ UINT64_MAX, ref.handle_serial }, terminal_control, &consumed) != OK);
		CHECK(std::memcmp(frozen, &request.control.output, sizeof(frozen)) == 0);
		REQUIRE(fixture.registry.production_control_consume(ref, terminal_control, &consumed) == OK);
		CHECK(consumed.output_generation.native_released);
		CHECK(consumed.output_generation.registry_instance == fixture.instance);
		CHECK(consumed.output_generation.request_serial == ref.handle_serial);
		CHECK(consumed.output_generation.control_serial == terminal_control);
		CHECK(consumed.output_generation.root_serial == probe.root_serial);
		CHECK(consumed.output_generation.particles == native.particle_rid().get_id());
		CHECK(consumed.output_generation.generation == native.generation);
		CHECK(consumed.output_generation.unused_slots == 2 - slots);
		CHECK(consumed.output_generation.returned_slots == slots);
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(fixture.registry.production_control_consume(ref, terminal_control, &consumed) == ERR_DOES_NOT_EXIST);
		const uint64_t repeat = dispatch(budget);
		REQUIRE(fixture.registry.production_control_consume(ref, repeat, &consumed) == OK);
		CHECK_FALSE(consumed.output_generation.native_released);
		CHECK(consumed.output_generation.root_serial == 0);
		CHECK(consumed.service_usage.passes <= (root_group ? 1u : 0u));
		CHECK(consumed.service_usage.receipts == 0);
		CHECK(fixture.driver.issues == 0);
		CHECK(fixture.driver.polls == 0);
		CHECK(fixture.driver.retains == 0);
		CHECK(fixture.driver.releases == 0);
		if (root_group) {
			generation_root_drain_cursor(fixture, ref, server);
		}
		release_request(ref, &request);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}

	struct LinkedOwnerProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		uint32_t calls = 0;
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<LinkedOwnerProbe *>(userdata);
			auto &fixture = *probe.fixture;
			auto &native = *probe.native;
			++probe.calls;
			for (uint32_t i = 0; i < 2; ++i) {
				native.install(i, fixture.registry, ref);
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, native.pin(i)) == OK);
				CHECK(fixture.registry.production_output_owner_bind(native.pin(i), fixture.rd, native.particle_rid(), native.generation + 1, i, native.lease(i)) == ERR_UNAUTHORIZED);
				REQUIRE(fixture.registry.production_output_owner_bind(native.pin(i), fixture.rd, native.particle_rid(), native.generation, i, native.lease(i)) == OK);
			}
		}
	};
	static void linked_owner_observe() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		if (!consume || !release) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 2, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		LinkedOwnerProbe probe;
		probe.fixture = &fixture;
		if (!check_step(ordinal_initialize(fixture, server, plan, &LinkedOwnerProbe::receive, &probe) == OK)) {
			return;
		}
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 recorded = {};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0); // Empty source is real; no completion is injected.
		uint32_t first = UINT32_MAX, second = UINT32_MAX;
		for (uint32_t i = 0; i < request.capacity.resource_uses; ++i) {
			if (request.uses[i].output_consumer == &native.pin(0)) {
				first = i;
			}
			if (request.uses[i].output_consumer == &native.pin(1)) {
				second = i;
			}
		}
		REQUIRE(first != UINT32_MAX);
		REQUIRE(second != UINT32_MAX);
		REQUIRE(first != second);
		CHECK(request.output_owner_head == second);
		CHECK(request.output_owner_cursor == first);
		CHECK(request.uses[second].output_next == first);
		CHECK(request.uses[first].output_prev == second);
		native.withdraw();
		auto observe_turn = [&](const GDExtensionProductionServiceBudgetV1 &budget) {
			ProductionControlOutput output = {};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK(output.result.native_error == OK);
			CHECK_FALSE(output.output_generation.native_released);
			CHECK(output.output_generation.root_serial == 0);
			CHECK(request.serial == ref.handle_serial);
			CHECK(request.capture_inflight == 0);
			return output.service_usage;
		};
		const GDExtensionProductionServiceBudgetV1 zero = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
		const auto idle = observe_turn(zero);
		CHECK(idle.passes == 0);
		CHECK(request.output_owner_cursor == first);
		CHECK_FALSE(native.history_cleared());
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		for (uint32_t turn = 0; turn < 3; ++turn) {
			const auto usage = observe_turn(budget);
			CHECK(usage.controls == 1);
			CHECK(usage.passes == 1); // Locator dispatch consumes no second pass.
			CHECK(usage.receipts == 0); // Metadata work creates no native witness.
			CHECK(native.history_cleared());
			if (turn == 0) {
				CHECK_FALSE(native.pin(0).is_empty());
				CHECK_FALSE(native.pin(1).is_empty());
				CHECK(request.output_owner_cursor == second);
			} else if (turn == 1) {
				CHECK_FALSE(native.pin(0).is_empty());
				CHECK(native.pin(1).is_empty());
				CHECK(request.uses[second].output_consumer == nullptr);
				CHECK(request.uses[second].output_pin_serial == 0);
				CHECK(request.uses[second].output_generation == 0);
				CHECK(request.output_owner_head == first);
				CHECK(request.output_owner_cursor == first);
				CHECK(request.uses[first].output_prev == UINT32_MAX);
				CHECK(request.uses[first].output_next == UINT32_MAX);
			}
		}
		CHECK(native.released());
		CHECK(request.uses[first].output_consumer == nullptr);
		CHECK(request.uses[first].output_pin_serial == 0);
		CHECK(request.uses[first].output_generation == 0);
		CHECK(request.output_owner_head == UINT32_MAX);
		CHECK(request.output_owner_cursor == UINT32_MAX);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
			GDExtensionProductionServiceBudgetV1 remaining = {};
			GDExtensionProductionReportV2 report = {};
			const Error result = Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((result == OK || result == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void ordinal_zero_control() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto facts = ordinal_port<GDExtensionInterfaceDeviceSessionProductionFactsV2>("device_session_production_facts_v2");
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!release || !consume || !facts || !observe) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		OrdinalEmptyProbe probe;
		probe.fixture = &fixture;
		if (!check_step(ordinal_initialize(fixture, server, plan, &OrdinalEmptyProbe::receive, &probe) == OK)) {
			return;
		}
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		CHECK(request.capacity.suboperations == 0);
		CHECK(request.capacity.graph_memberships == 0);
		CHECK(request.capacity.resources == 0);
		CHECK(request.capacity.resource_uses == 0);
		uint64_t control = 0;
		CHECK(fixture.port.record_schedule(ref, &control) == OK);
		server->sync(); // Actual original RS dispatch and protected receiver.
		CHECK(probe.calls == 1);
		CHECK(request.control.state == ProductionControlState::RETURNED);
		CHECK(request.control.serial == control);
		const GDExtensionProductionDispatchV1 legacy_dispatch = { production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 0, 0, 1, 1, 1, 0, 0 };
		const GDExtensionProductionCopyV1 legacy_copy = { production_dto_abi<GDExtensionProductionCopyV1>(), 10, 0, 0, 0, 0, 4 };
		const GDExtensionProductionUploadV1 legacy_upload = { production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 4 };
		const uint8_t payload[4] = { 1, 2, 3, 4 };
		CHECK(fixture.port.prepare_dispatch(ref, &legacy_dispatch, nullptr, 0).native_error == ERR_INVALID_DATA);
		CHECK(fixture.port.prepare_copy(ref, &legacy_copy).native_error == ERR_INVALID_DATA);
		CHECK(fixture.port.prepare_upload(ref, &legacy_upload, payload, sizeof(payload)).native_error == ERR_INVALID_DATA);
		CHECK(fixture.port.prepare_readback(ref, 10, 0).native_error == ERR_INVALID_DATA);
		const auto state = request.control.state;
		const uint64_t next = request.next_control_serial;
		GDExtensionProductionRecordReportV1 old_record;
		std::memset(&old_record, 0x5a, sizeof(old_record));
		const auto old_record_before = old_record;
		CHECK(fixture.port.record_consume(ref, control, &old_record).native_error == ERR_INVALID_DATA);
		CHECK(std::memcmp(&old_record, &old_record_before, sizeof(old_record)) == 0);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		GDExtensionProductionReportV1 old_report;
		std::memset(&old_report, 0x5a, sizeof(old_report));
		const auto old_report_before = old_report;
		CHECK(fixture.port.observe(ref, &budget, &old_report).native_error == ERR_INVALID_DATA);
		CHECK(std::memcmp(&old_report, &old_report_before, sizeof(old_report)) == 0);
		GDExtensionProductionServiceBudgetV1 remaining;
		std::memset(&remaining, 0x5a, sizeof(remaining));
		const auto remaining_before = remaining;
		CHECK(fixture.port.release(ref, &budget, &remaining, &old_report).native_error == ERR_INVALID_DATA);
		CHECK(std::memcmp(&remaining, &remaining_before, sizeof(remaining)) == 0);
		CHECK(std::memcmp(&old_report, &old_report_before, sizeof(old_report)) == 0);
		GDExtensionProductionFactV1 old_fact;
		std::memset(&old_fact, 0x5a, sizeof(old_fact));
		const auto old_fact_before = old_fact;
		uint32_t written = 0x12345678;
		CHECK(fixture.port.facts(ref, request.revision, 0, &old_fact, 1, &written).native_error == ERR_INVALID_DATA);
		CHECK(written == 0x12345678);
		CHECK(std::memcmp(&old_fact, &old_fact_before, sizeof(old_fact)) == 0);
		CHECK(request.control.state == state);
		CHECK(request.control.serial == control);
		CHECK(request.next_control_serial == next);
		GDExtensionProductionRecordReportV2 returned = {};
		CHECK(consume(ref, control, &returned).native_error == OK);
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(returned.report.operation.canonical_begin == 10);
		CHECK(returned.report.operation.canonical_end == 13);
		CHECK(returned.report.action_count == 0);
		CHECK(returned.report.recorded_action_end == 0);
		CHECK((returned.flags & (GDEXTENSION_PRODUCTION_RECORD_HAS_APPEND_V1 | GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1)) == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		written = UINT32_MAX;
		CHECK(facts(ref, request.revision, 0, nullptr, 0, &written).native_error == OK);
		CHECK(written == 0);
		const GDExtensionProductionServiceBudgetV1 zero = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
		GDExtensionProductionReportV2 report = {};
		CHECK(observe(ref, &zero, &report).native_error == OK);
		CHECK(report.action_count == 0);
		CHECK(report.recorded_action_end == 0);
		CHECK(report.operation.canonical_end == 13); // No physical count substitutes for logical publication.
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
			GDExtensionProductionServiceBudgetV1 returned_budget = {};
			GDExtensionProductionReportV2 released = {};
			const Error result = Error(release(ref, &budget, &returned_budget, &released).native_error);
			CHECK((result == OK || result == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0); // Actual versioned RELEASE, without canceling the empty plan.
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void ordinal_readback_producer() {
		// The valid control and each invalid request go through original accepted
		// arenas and the installed reserve_v2 entry, before any native effects.
		for (uint32_t mutation = 0; mutation < 5; ++mutation) {
			auto *server = ordinal_server();
			if (!server) {
				return;
			}
			OrdinalPlan plan;
			if (mutation == 1) {
				plan.readback.producer_action_ordinal = 2;
			}
			if (mutation == 2) {
				plan.uses[1].access = GDEXTENSION_PRODUCTION_READ_V1;
			}
			if (mutation == 3) {
				plan.uses[1].resource_slot = 2;
			} // Producer writes a different slot.
			if (mutation == 4) {
				plan.uses[1].byte_offset = 4;
				plan.uses[1].byte_size = 12;
			}
			SingletonEnrollment fixture;
			const Error error = ordinal_initialize(fixture, server, plan);
			CHECK_MESSAGE(error == (mutation == 0 ? OK : ERR_INVALID_DATA), "readback producer mutation=", mutation);
			if (mutation == 0) {
				CHECK(fixture.request != nullptr);
			} else {
				CHECK(fixture.request == nullptr);
			}
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
		}
	}
	static bool v1_result(const GDExtensionDeviceSessionResultV1 &result, Error error, uint32_t reason) {
		return result.native_error == error && result.reason == reason && result.expected == 0 && result.actual == 0;
	}
	static void v1_compat_owner_errors() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		SingletonEnrollment fixture;
		fixture.public_port = true;
		if (!check_step(fixture.prepare_device(server) == OK)) {
			return;
		}
		const auto unused_receiver = +[](void *, ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {};
		if (!check_step(fixture.initialize_owner(1, false, false, unused_receiver, nullptr, false, 1, true, +[](void *, GDExtensionProductionAttachmentRefV1) {}) == OK)) {
			return;
		}
		auto &request = *fixture.request;
		auto &port = fixture.port;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		const GDExtensionProductionRequestRefV1 foreign = { UINT64_MAX, 1 };
		GDExtensionProductionDispatchV1 dispatch = { production_dto_abi<GDExtensionProductionDispatchV1>(), 9, 0, 0, 0, 1, 1, 1, 0, 0 };
		GDExtensionProductionCopyV1 copy = { production_dto_abi<GDExtensionProductionCopyV1>(), 9, 0, 0, 0, 0, 4 };
		GDExtensionProductionUploadV1 upload = { production_dto_abi<GDExtensionProductionUploadV1>(), 9, 0, 0, 0, 4 };
		const uint8_t payload[4] = { 2, 3, 5, 7 };
		const GDExtensionProductionServiceBudgetV1 zero_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
		GDExtensionProductionReportV1 report;
		std::memset(&report, 0x5a, sizeof(report));
		const auto report_sentinel = report;
		GDExtensionProductionFactV1 fact = {};
		uint32_t written = 0x12345678;
		// Payload/buffer validation wins over an unrelated reference failure.
		dispatch.groups_x = 0;
		CHECK(v1_result(port.prepare_dispatch(foreign, &dispatch, nullptr, 0), ERR_INVALID_PARAMETER, 2));
		dispatch.groups_x = 1;
		copy.bytes = 0;
		CHECK(v1_result(port.prepare_copy(foreign, &copy), ERR_INVALID_PARAMETER, 2));
		copy.bytes = 4;
		CHECK(v1_result(port.prepare_upload(foreign, &upload, nullptr, 4), ERR_INVALID_PARAMETER, 2));
		auto bad_budget = zero_budget;
		bad_budget.reserved = 1;
		CHECK(v1_result(port.observe(foreign, &bad_budget, &report), ERR_INVALID_PARAMETER, 2));
		CHECK(std::memcmp(&report, &report_sentinel, sizeof(report)) == 0);
		CHECK(v1_result(port.facts(foreign, 0, 0, &fact, 1, nullptr), ERR_INVALID_PARAMETER, 2));
		// Once arguments are sound, original request-resolution diagnostics use16.
		CHECK(v1_result(port.prepare_dispatch(foreign, &dispatch, nullptr, 0), ERR_INVALID_DATA, 16));
		CHECK(v1_result(port.prepare_copy(foreign, &copy), ERR_INVALID_DATA, 16));
		CHECK(v1_result(port.prepare_upload(foreign, &upload, payload, 4), ERR_INVALID_DATA, 16));
		CHECK(v1_result(port.prepare_readback(foreign, 9, 0), ERR_INVALID_DATA, 16));
		CHECK(v1_result(port.observe(foreign, &zero_budget, &report), ERR_INVALID_DATA, 16));
		CHECK(std::memcmp(&report, &report_sentinel, sizeof(report)) == 0);
		CHECK(v1_result(port.facts(foreign, 0, 0, &fact, 1, &written), ERR_INVALID_DATA, 16));
		CHECK(written == 0x12345678);
		// A missing canonical is different from a malformed physical declaration.
		CHECK(v1_result(port.prepare_dispatch(ref, &dispatch, nullptr, 0), ERR_DOES_NOT_EXIST, 16));
		CHECK(v1_result(port.prepare_copy(ref, &copy), ERR_DOES_NOT_EXIST, 16));
		CHECK(v1_result(port.prepare_upload(ref, &upload, payload, 4), ERR_DOES_NOT_EXIST, 16));
		CHECK(v1_result(port.prepare_readback(ref, 9, 0), ERR_DOES_NOT_EXIST, 16));
		CHECK(request.payload_used == 0);
		CHECK_FALSE(request.actions[0].prepared);
		// Installed reserve retains PARAMETER0 for malformed op on real attachment.
		auto bad_operation = request.operation;
		bad_operation.owner_request_nonce = 0;
		const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const auto resource = request.resources[0].declaration;
		const auto use = request.uses[0].declaration;
		GDExtensionProductionRequestRefV1 rejected = {};
		CHECK(v1_result(port.reserve(fixture.attachment, &bad_operation, &request.capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, &rejected), ERR_INVALID_PARAMETER, 0));
		CHECK(rejected.handle_serial == 0);
		GDExtensionProductionRecordReportV1 record;
		std::memset(&record, 0x5a, sizeof(record));
		CHECK(v1_result(port.record_consume(foreign, 1, &record), ERR_INVALID_DATA, 0));
		CHECK(record.abi.abi_version == 0);
		CHECK(record.request_serial == 0);
		CHECK(record.control_serial == 0);
		CHECK(record.flags == 0);
		CHECK(record.report.abi.abi_version == 0);
		CHECK(record.append.abi.abi_version == 0);
		CHECK(record.segment.abi.abi_version == 0);
		GDExtensionProductionServiceBudgetV1 remaining;
		std::memset(&remaining, 0x5a, sizeof(remaining));
		const auto remaining_sentinel = remaining;
		CHECK(v1_result(port.release(foreign, &bad_budget, &remaining, &report), ERR_INVALID_PARAMETER, 19));
		CHECK(std::memcmp(&remaining, &remaining_sentinel, sizeof(remaining)) == 0);
		CHECK(std::memcmp(&report, &report_sentinel, sizeof(report)) == 0);
		CHECK(v1_result(port.release(foreign, &zero_budget, &remaining, &report), ERR_INVALID_DATA, 16));
		CHECK(std::memcmp(&remaining, &zero_budget, sizeof(remaining)) == 0);
		CHECK(report.abi.abi_version == 0);
		CHECK(report.operation.canonical_begin == 0);
		CHECK(report.recorded_end == 0);
		CHECK(report.revision == 0);
		CHECK(report.cause.native_error == OK);
		// Explicit cancellation is real owner state, not a fabricated request.
		CHECK(port.cancel(ref).native_error == OK);
		CHECK(v1_result(port.prepare_dispatch(ref, &dispatch, nullptr, 0), ERR_BUSY, 16));
		CHECK(v1_result(port.prepare_copy(ref, &copy), ERR_BUSY, 16));
		CHECK(v1_result(port.prepare_upload(ref, &upload, payload, 4), ERR_BUSY, 16));
		CHECK(v1_result(port.prepare_readback(ref, 9, 0), ERR_BUSY, 16));
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void v1_compat_partial_report() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		SingletonEnrollment fixture;
		fixture.public_port = true;
		if (!check_step(fixture.initialize(server) == OK)) {
			return;
		}
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		const GDExtensionProductionServiceBudgetV1 query = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
		GDExtensionProductionReportV1 report;
		// Friend fixture scope only: exercise diagnostic branches which public
		// callers cannot construct. Original identity/enrollment is kept intact;
		// restore every field before cleanup, with no scheduling/native effects.
		const auto captures = request.capture_inflight;
		request.capture_inflight = 1;
		std::memset(&report, 0x5a, sizeof(report));
		const auto capture_result = fixture.port.observe(ref, &query, &report);
		request.capture_inflight = captures;
		CHECK(v1_result(capture_result, OK, 0));
		CHECK(report.abi.abi_version == 1);
		CHECK(report.operation.canonical_begin == 10);
		CHECK(report.operation.canonical_end == 11);
		CHECK(report.revision == request.revision);
		CHECK(v1_result(report.cause, ERR_BUSY, 16));
		CHECK(report.recorded_end == 0);
		const auto resources = request.resource_count;
		request.resource_count = request.capacity.resources + 1;
		std::memset(&report, 0x5a, sizeof(report));
		const auto structural_result = fixture.port.observe(ref, &query, &report);
		request.resource_count = resources;
		CHECK(v1_result(structural_result, OK, 0));
		CHECK(report.abi.abi_version == 1);
		CHECK(report.operation.canonical_begin == 10);
		CHECK(report.operation.canonical_end == 11);
		CHECK(v1_result(report.cause, ERR_INVALID_DATA, 16));
		CHECK(report.recorded_end == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void installed_port_rejects_foreign_refs() {
		PublicProductionPort port;
		REQUIRE(port.load()); // Fetch all29 actual installed procedures before any call.
		const auto before = singleton_counts();
		auto &registry = Registry::get_singleton();
		const auto production_serial = registry.production_serials.next;
		const auto frame_serial = registry.frame_serials.next;
		const GDExtensionDeviceExitAbi abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		const GDExtensionProductionAttachmentRefV1 foreign_attachment = { UINT64_MAX, 1 };
		const GDExtensionProductionRequestRefV1 foreign_request = { UINT64_MAX, 1 };
		const GDExtensionProductionPhysicalContextRefV1 foreign_physical = { UINT64_MAX, 1 };
		GDExtensionProductionCapacityV1 capacity = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 1, 1, 1, 1, 16, 0, 1, 4, 0 };
		GDExtensionProductionMetadataQuoteV1 quote = {};
		CHECK(port.quote(&abi, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, &capacity, &quote).native_error != OK);
		quote.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionMetadataQuoteV1>();
		quote.capacity = capacity;
		GDExtensionProductionAttachmentRefV1 attachment = {};
		GDExtensionProductionInstallationReportV1 installation = {};
		CHECK(port.accept(&abi, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, { UINT64_MAX, 1 }, &quote, &attachment, &installation).native_error != OK);
		CHECK(attachment.handle_serial == 0);
		const auto record_receiver = +[](void *, GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) { FAIL_CHECK("Foreign reference entered record callback"); };
		const auto metadata_receiver = +[](void *, GDExtensionProductionAttachmentRefV1) { FAIL_CHECK("Foreign reference entered metadata callback"); };
		CHECK(port.install_record_receiver({ UINT64_MAX, 1 }, foreign_attachment, record_receiver, nullptr, metadata_receiver) != OK);
		CHECK(port.open({ UINT64_MAX, 1 }, foreign_attachment).native_error != OK);
		const GDExtensionProductionOperationV1 operation = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), 91, 1, 10, 11 };
		const GDExtensionProductionActionV1 action = { 10, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 };
		const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
		GDExtensionProductionRequestRefV1 request = {};
		CHECK(port.reserve(foreign_attachment, &operation, &capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, &request).native_error != OK);
		CHECK(request.handle_serial == 0);
		uint64_t control = 0;
		CHECK(port.record_schedule(foreign_request, &control) != OK);
		CHECK(control == 0);
		GDExtensionProductionRecordReportV1 recorded = {};
		CHECK(port.record_consume(foreign_request, 1, &recorded).native_error != OK);
		GDExtensionProductionAppendV1 appended = {};
		CHECK(port.append(foreign_physical, foreign_request, 10, 1, &appended).native_error != OK);
		GDExtensionProductionSegmentV1 segment = {};
		CHECK(port.close_segment(foreign_physical, foreign_request, &segment).native_error != OK);
		CHECK(port.ack_segment(foreign_request, 1, 1).native_error != OK);
		const GDExtensionProductionSetBindingV1 binding = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 0, 0, 16 };
		CHECK(port.set_create(foreign_request, 0, 0, 0, &binding, 1).native_error != OK);
		const GDExtensionStorageCreateRequestV1 create = { abi, 16, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 storage = {};
		CHECK(port.storage_create(foreign_request, 0, &create, &storage).native_error != OK);
		const GDExtensionStorageRetireRequestV1 retire = { abi, { UINT64_MAX, 1 }, 1, 1, 0, 0, 1, 1, 0 };
		GDExtensionStorageRetireRefV1 retired_ref = {};
		GDExtensionStorageRetireReportV1 retired = {};
		CHECK(port.storage_retire(foreign_request, 0, &retire, &retired_ref, &retired).native_error != OK);
		const GDExtensionProductionServiceBudgetV1 zero = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
		CHECK(port.storage_observe(foreign_request, { UINT64_MAX, 1 }, &zero, &retired).native_error != OK);
		CHECK(port.storage_release(foreign_request, { UINT64_MAX, 1 }).native_error != OK);
		GDExtensionNativeUnitBackingV1 unit = {};
		unit.abi = abi;
		CHECK(port.import_unit(foreign_request, 0, &unit).native_error != OK);
		storage.abi = abi;
		CHECK(port.import_storage(foreign_request, 0, &storage).native_error != OK);
		GDExtensionQualificationResourceBackingV1 set = {};
		set.abi = abi;
		CHECK(port.import_set(foreign_request, 0, &set, 0, 0, &binding, 1).native_error != OK);
		const GDExtensionProductionDispatchV1 dispatch = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionDispatchV1>(), 10, 0, 0, 0, 1, 1, 1, 0, 0 };
		CHECK(port.prepare_dispatch(foreign_request, &dispatch, nullptr, 0).native_error != OK);
		const GDExtensionProductionCopyV1 copy = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCopyV1>(), 10, 0, 1, 0, 0, 4 };
		CHECK(port.prepare_copy(foreign_request, &copy).native_error != OK);
		const GDExtensionProductionUploadV1 upload = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV1>(), 10, 0, 0, 0, 4 };
		uint8_t bytes[4] = {};
		CHECK(port.prepare_upload(foreign_request, &upload, bytes, sizeof(bytes)).native_error != OK);
		CHECK(port.prepare_readback(foreign_request, 10, 0).native_error != OK);
		GDExtensionProductionReportV1 report = {};
		CHECK(port.observe(foreign_request, &zero, &report).native_error != OK);
		GDExtensionProductionFactV1 fact = {};
		uint32_t written = 0;
		CHECK(port.facts(foreign_request, 1, 0, &fact, 1, &written).native_error != OK);
		CHECK(written == 0);
		uint64_t copied = 0;
		CHECK(port.copy(foreign_request, 1, 0, bytes, sizeof(bytes), &copied).native_error != OK);
		CHECK(copied == 0);
		CHECK(port.cancel(foreign_request).native_error != OK);
		GDExtensionProductionServiceBudgetV1 remaining = {};
		CHECK(port.release(foreign_request, &zero, &remaining, &report).native_error != OK);
		CHECK(port.close(foreign_attachment).native_error != OK);
		CHECK(port.installation_observe(foreign_attachment, &zero, &installation).native_error != OK);
		const auto after = singleton_counts();
		CHECK(after.instance == before.instance);
		CHECK(after.services == before.services);
		CHECK(after.records == before.records);
		CHECK(after.private_refs == before.private_refs);
		CHECK(after.external_refs == before.external_refs);
		CHECK(after.views == before.views);
		CHECK(after.devices == before.devices);
		CHECK(after.device_refs == before.device_refs);
		CHECK(after.attachments == before.attachments);
		CHECK(registry.production_serials.next == production_serial);
		CHECK(registry.frame_serials.next == frame_serial);
	}
	static Error quote(const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands = 0, uint32_t workers = 0) {
		return GDExtensionDeviceExit::Registry::production_layout(capacity, out, current_commands, workers);
	}
};
TEST_CASE("[ProductionRequest] quote checks dimensions before reserving real cells") {
	GDExtensionProductionCapacityV1 capacity = {};
	capacity.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>();
	GDExtensionProductionMetadataQuoteV1 quote = {};
	CHECK(Access::quote(capacity, quote) == ERR_INVALID_PARAMETER);
	capacity.requests = 1;
	capacity.suboperations = 2;
	capacity.resources = 2;
	capacity.resource_uses = 2;
	capacity.graph_memberships = 2;
	capacity.native_memberships = 2;
	capacity.return_cells = 2;
	capacity.abi.struct_size = sizeof(GDExtensionDeviceExitAbi);
	CHECK(Access::quote(capacity, quote) == ERR_INVALID_PARAMETER);
	CHECK(quote.allocation_bytes == 0);
	capacity.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>();
	REQUIRE(Access::quote(capacity, quote) == OK);
	CHECK(quote.abi.struct_size == sizeof(GDExtensionProductionMetadataQuoteV1));
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
	GDExtensionProductionServiceBudgetV1 budget = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 0, 0, 0, 0 };
	CHECK(registry.production_observe({ UINT64_MAX, 1 }, &budget, &report).native_error != OK);
	CHECK(registry.production_cancel({ UINT64_MAX, 1 }).native_error != OK);
	CHECK(registry.production_release({ UINT64_MAX, 1 }).native_error != OK);
}

TEST_CASE("[SceneTree][ProductionRequest][OrdinalV2] repeated canonical rows preserve original logical gaps and exact prepare identity") {
	Access::ordinal_rows_and_prepare();
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] live selected group returns borrowed custody before wrapper finish") {
	Access::live_geometry_observe();
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] original mesh detach preserves selected cached snapshot and COW neighbor") {
	Access::live_geometry_observe(true);
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] original owner clear without neighbor observes queued then physical return") {
	Access::live_geometry_observe(false, false, true);
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] original disposal source Pending retains queued backing before matching completion") {
	Access::live_geometry_observe(false, false, true, false, 3);
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] live Indexed144 snapshot returns all five actual borrowed roots") {
	Access::live_geometry_observe(false, false, false, true);
}
TEST_CASE("[SceneTree][ProductionRequest][LiveGeometryComponent] failed Core capture retains live metadata and physical source") {
	Access::live_geometry_observe(false, true);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputRegisteredLeaves] guarded original output leaves reject foreign custody and preserve original RECORD cleanup") {
	// Independent original fixtures let all custody probes execute even if an
	// old cleanup body wrongly mutates its target. Each drains normally.
	for (uint32_t custody_case = 0; custody_case < 3; ++custody_case) {
		Access::output_registered_leaves(custody_case);
	}
}
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] exact Instance192 and zero-capacity resources retire through original OBSERVE") {
	Access::owned_storage_observe(2);
	Access::owned_storage_observe(0);
}
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] different accepted request cannot allocate original stable slot") {
	Access::owned_storage_observe(2, 0, false, false, true);
}
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] finite member shortage refuses before native allocation") {
	Access::owned_storage_observe(2, 0, true);
	Access::owned_storage_observe(0, 0, true);
}
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] invalid Instance192 capacity preserves original slot before native effects") {
	Access::owned_storage_observe((1u << 20) + 1, 0, false, true);
}
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] every original native allocation failure retains and retires exact prefix") {
	ERR_PRINT_OFF;
	for (uint32_t fail = 1; fail <= 7; ++fail) {
		Access::owned_storage_observe(2, fail);
	}
	for (uint32_t fail = 1; fail <= 5; ++fail) {
		Access::owned_storage_observe(0, fail);
	}
	ERR_PRINT_ON;
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] Root retains actual pipeline and material prefix") {
	Access::pipeline_prefix_observe(0, true);
	Access::pipeline_prefix_observe(1, true);
	Access::pipeline_prefix_observe(2, true);
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] Root retains actual geometry and pending native disposal") {
	Access::live_geometry_observe(false, false, false, false, 0, true);
	Access::live_geometry_observe(false, false, true, false, 3, true);
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] zero Acquire retains an independent root and immutable terminal result") {
	Access::generation_root_observe(0);
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] last slot return preserves generation root until its own OBSERVE turn") {
	Access::generation_root_observe(2);
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] real quoted cell exhaustion and rejected locator retain original custody") {
	Access::generation_root_observe(0, true);
}
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] metadata directory and terminal root return spend separate one-pass turns") {
	Access::generation_root_observe(0, false, true);
}

TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] original borrowed directory pin blocks root terminal without native work") {
	TestProductionRequest::Access::generation_root_observe(0, false, true, false, true);
}

TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][GenerationRootComponent] unresolved original root group retains pin and rolls back tentative terminal result") {
	Access::generation_root_observe(0, false, false, true);
}

TEST_CASE("[SceneTree][ProductionRequest][LinkedOwnerComponent] original OBSERVE traverses and unlinks two real roots within one-pass slices") {
	Access::linked_owner_observe();
}
TEST_CASE("[SceneTree][ProductionRequest][OrdinalV2] zero arena returns actual protected FIFO control without V1 consumption") {
	Access::ordinal_zero_control();
}
TEST_CASE("[SceneTree][ProductionRequest][OrdinalV2] readback producer must cover exact target storage access and bytes") {
	Access::ordinal_readback_producer();
}

TEST_CASE("[SceneTree][ProductionRequest][V1Compatibility] installed owner port preserves error priority reasons and failure outputs") {
	Access::v1_compat_owner_errors();
}
TEST_CASE("[SceneTree][ProductionRequest][V1Compatibility] partial diagnostic reports do not manufacture a canonical frontier") {
	Access::v1_compat_partial_report();
}
#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest][V1Compatibility] protected append echoes invalid canonical and clears expired context output") {
	Access::singleton_split_owner_set(true, false, false, false, false, false, false, false, false, false, false, false, false, true);
}
#endif

} // namespace TestProductionRequest

TEST_CASE("[ProductionRequest] payload retry is exact and the finite batch freezes before effects") {
	TestProductionRequest::Access::immutable_payload();
}

TEST_CASE("[ProductionRequest] attachment quote retains each request command prefix") {
	using TestProductionRequest::Access;
	GDExtensionProductionCapacityV1 whole = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 0, 0 };
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
TEST_CASE("[SceneTree][ProductionRequest][PipelinePrefix] actual pipeline survives material capacity failure") {
	ERR_PRINT_OFF;
	TestProductionRequest::Access::pipeline_prefix_observe(0);
	ERR_PRINT_ON;
}
TEST_CASE("[SceneTree][ProductionRequest][PipelinePrefix] owned pipeline and material borrow return in one pass slices") {
	ERR_PRINT_OFF;
	TestProductionRequest::Access::pipeline_prefix_observe(1);
	ERR_PRINT_ON;
}
TEST_CASE("[SceneTree][ProductionRequest][PipelinePrefix] driver refusal returns real RID reservation in both reporting modes") {
	ERR_PRINT_OFF;
	TestProductionRequest::Access::pipeline_prefix_observe(2);
	TestProductionRequest::Access::pipeline_prefix_observe(3);
	ERR_PRINT_ON;
}
TEST_CASE("[SceneTree][ProductionRequest][PipelinePrefix] Core capacity refuses before pipeline driver create") {
	ERR_PRINT_OFF;
	TestProductionRequest::Access::pipeline_prefix_observe(4);
	ERR_PRINT_ON;
}

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

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] installed typed production port follows original owner RT FIFO and late ACK") {
	SUBCASE("Post-seal cancellation returns through fresh metadata frames") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, true, false, false, true);
	}
	SUBCASE("Completed uncancelled plan returns through its original owner frame") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, true);
	}
	SUBCASE("Metadata borrow from a true original pin retains the accepted arena after native recording") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, true, false, false, false, true);
	}
	SUBCASE("Actual owner close refuses new metadata borrows while old custody returns") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, true, false, false, true, false, false, false, true);
	}
}
#endif

TEST_CASE("[ProductionRequest] installed typed production procedures reject foreign references without enrollment") {
	TestProductionRequest::Access::installed_port_rejects_foreign_refs();
}

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest] exact native output closure returns through bounded original FIFO") {
	SUBCASE("Retired shader pipeline and owned buffer set each progress with one pass") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, true);
	}
	SUBCASE("Live borrowed owners detach without claiming physical return") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, false);
	}
	SUBCASE("Actual detached Mesh directory binds and moves with original accepted request") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, false, false, false, true);
	}
	SUBCASE("External directory preserves borrowed backing and retires owned Version and LOD roots") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, false, false, false, true, false, true);
	}
	SUBCASE("Actual quoted directory capacity refuses before retaining new shares") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, false, false, false, true, true);
	}
	SUBCASE("Unrecorded actual factory prefix returns after explicit owner cancellation") {
		TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, true, false, true);
	}
}
#endif
