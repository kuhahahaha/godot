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
#include "servers/rendering/renderer_scene_cull.h"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>
#ifdef THREADS_ENABLED
#include <chrono>
#include <condition_variable>
#include <mutex>
#endif

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
		bool alias_readback_mode = false;
		bool alias_download_map_failure = false;
		uint32_t alias_observed_download_maps = 0, alias_failed_download_maps = 0;
		uint8_t alias_bytes[16][1024] = {}, alias_upload[64] = {}, alias_download[64] = {};
		uint64_t alias_sizes[16] = {};
		bool alias_live[16] = {}, alias_upload_live = false, alias_download_live = false;
		uint32_t alias_allocations = 0, alias_frees = 0, alias_upload_copies = 0, alias_download_copies = 0;
		bool output_storage_factory = false;
		uint32_t output_storage_attempts = 0, output_storage_count = 0, output_storage_frees = 0, output_storage_fail_at = 0;
		uint64_t output_storage_bytes[7] = {};
		bool output_storage_live[7] = {};
		bool output_storage_indirect[7] = {};
		uint32_t allocations = 0, buffer_frees = 0, directory_pending_polls = 0;
		bool buffer_live = false;
		bool upload_mode = false, staging_live = false;
		bool native_output_copy = false;
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
			if (alias_readback_mode) {
				if (id == BufferID(500) && alias_upload_live) {
					return alias_upload;
				}
				if (id == BufferID(501) && alias_download_live) {
					return alias_download;
				}
				FAIL_CHECK("Unknown alias integration staging map");
				return nullptr;
			}
			CHECK((upload_mode && id == BufferID(2) && staging_live));
			return upload_mode && id == BufferID(2) && staging_live ? upload_memory[2] : nullptr;
		}
		uint8_t *buffer_map_observed(BufferID id, int64_t &native_code) override {
			if (alias_readback_mode && id == BufferID(501)) {
				REQUIRE(alias_download_live);
				++alias_observed_download_maps;
				if (alias_download_map_failure) {
					++alias_failed_download_maps;
					native_code = -7001;
					return nullptr;
				}
			}
			native_code = 0;
			return buffer_map(id);
		}
		void buffer_unmap(BufferID id) override {
			if (alias_readback_mode) {
				CHECK(((id == BufferID(500) && alias_upload_live) || (id == BufferID(501) && alias_download_live)));
				return;
			}
			CHECK((upload_mode && id == BufferID(2) && staging_live));
		}
		void command_copy_buffer(CommandBufferID, BufferID source, BufferID destination, VectorView<BufferCopyRegion> regions) override {
			if (alias_readback_mode) {
				const bool upload = source == BufferID(500);
				const uint64_t native_id = upload ? destination.id : source.id;
				REQUIRE(native_id >= 300);
				REQUIRE(native_id < 316);
				const uint32_t index = uint32_t(native_id - 300);
				REQUIRE(alias_live[index]);
				CHECK((upload ? alias_upload_live : (destination == BufferID(501) && alias_download_live)));
				for (uint32_t region_index = 0; region_index < regions.size(); ++region_index) {
					const auto &region = regions[region_index];
					REQUIRE(region.src_offset + region.size <= (upload ? 64 : alias_sizes[index]));
					REQUIRE(region.dst_offset + region.size <= (upload ? alias_sizes[index] : 64));
					memcpy((upload ? alias_bytes[index] : alias_download) + region.dst_offset, (upload ? alias_upload : alias_bytes[index]) + region.src_offset, region.size);
				}
				if (upload) {
					++alias_upload_copies;
				} else {
					++alias_download_copies;
				}
				return;
			}
			if (native_output_copy) {
				CHECK(source == BufferID(300));
				CHECK(destination == BufferID(301));
				CHECK(output_storage_live[0]);
				CHECK(output_storage_live[1]);
				for (uint32_t i = 0; i < regions.size(); ++i) {
					const auto &region = regions[i];
					REQUIRE(region.src_offset + region.size <= 16);
					REQUIRE(region.dst_offset + region.size <= 16);
					memcpy(upload_memory[1] + region.dst_offset, upload_memory[0] + region.src_offset, region.size);
					++upload_copies;
				}
				return;
			}
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
			if (alias_readback_mode) {
				CHECK(Thread::get_caller_id() == execution_thread);
				if (allocation == MEMORY_ALLOCATION_TYPE_CPU) {
					REQUIRE(bytes == 64);
					if (usage.has_flag(BUFFER_USAGE_TRANSFER_FROM_BIT)) {
						REQUIRE_FALSE(alias_upload_live);
						alias_upload_live = true;
						return BufferID(500);
					}
					REQUIRE(usage.has_flag(BUFFER_USAGE_TRANSFER_TO_BIT));
					REQUIRE_FALSE(alias_download_live);
					alias_download_live = true;
					return BufferID(501);
				}
				REQUIRE(allocation == MEMORY_ALLOCATION_TYPE_GPU);
				REQUIRE(usage.has_flag(BUFFER_USAGE_STORAGE_BIT));
				REQUIRE(bytes <= 1024);
				REQUIRE(alias_allocations < 16);
				const uint32_t index = alias_allocations++;
				alias_sizes[index] = bytes;
				alias_live[index] = true;
				return BufferID(300 + index);
			}
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
			if (alias_readback_mode) {
				if (id == BufferID(500)) {
					REQUIRE(alias_upload_live);
					alias_upload_live = false;
					return;
				}
				if (id == BufferID(501)) {
					REQUIRE(alias_download_live);
					alias_download_live = false;
					return;
				}
				REQUIRE(id.id >= 300);
				REQUIRE(id.id < 316);
				REQUIRE(alias_live[id.id - 300]);
				alias_live[id.id - 300] = false;
				++alias_frees;
				return;
			}
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
			if (production_raster_factory) {
				return 0;
			}
			FAIL_CHECK("Unexpected source-service CPU driver trait");
			return 0;
		}
		uint64_t limit_get(Limit p_limit) override {
			if (production_raster_factory) {
				return 8192;
			}
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
		bool production_raster_factory = false, production_depth_live = false;
		uint32_t production_fail_stage = 0, production_quiesce_stage = 0;
		void (*production_factory_probe)(void *) = nullptr;
		void *production_factory_probe_data = nullptr;
		uint32_t production_stage_attempts[4] = {};
		uint32_t production_depth_frees = 0;
		bool production_raster_join = false, production_framebuffer_live = false, production_render_pass_active = false;
		bool production_cache_pass_returned = false;
		uint32_t production_render_pass_creates = 0, production_render_pass_frees = 0;
		uint32_t production_framebuffer_creates = 0, production_framebuffer_frees = 0;
		uint32_t production_render_begins = 0, production_render_ends = 0;
		uint32_t production_viewports = 0, production_scissors = 0, production_depth_clears = 0;
		Request *production_raster_request = nullptr;
		Vector<uint32_t> production_free_order;
		TextureID texture_create(const TextureFormat &value, const TextureView &view) override {
			CHECK(production_raster_factory);
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(value.format == DATA_FORMAT_D32_SFLOAT);
			CHECK(view.format == DATA_FORMAT_D32_SFLOAT);
			CHECK(value.width == 8);
			CHECK(value.height == 4);
			CHECK(value.usage_bits == (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
			++production_stage_attempts[0];
			if (production_fail_stage == 1) {
				return {};
			}
			CHECK_FALSE(production_depth_live);
			production_depth_live = true;
			if (production_quiesce_stage == 1 && production_factory_probe) {
				production_factory_probe(production_factory_probe_data);
			}
			return TextureID(380);
		}
		void texture_free(TextureID id) override {
			CHECK(production_raster_factory);
			CHECK(id == TextureID(380));
			CHECK(production_depth_live);
			REQUIRE(production_raster_request != nullptr);
			const auto &framebuffer = production_raster_request->resources[0];
			CHECK((framebuffer.primary.is_null() || framebuffer.owned_physical.free_event));
			if (production_raster_join) {
				CHECK(production_framebuffer_frees == 1);
				CHECK(production_cache_pass_returned);
			}
			production_depth_live = false;
			++production_depth_frees;
			production_free_order.push_back(1);
		}
		uint64_t texture_get_allocation_size(TextureID id) override {
			CHECK(id == TextureID(380));
			return 128;
		}
		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat value, bool readable) override {
			CHECK(value == DATA_FORMAT_D32_SFLOAT);
			CHECK_FALSE(readable);
			return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		bool raster_fail = false, raster_quiet = false, raster_live = false;
		RenderingDevice *raster_device = nullptr;
		uint32_t raster_budgeted_frees = 0;
		uint32_t raster_attempts = 0, raster_frees = 0, raster_shader_frees = 0;
		DriverWorkarounds get_driver_workarounds() const override {
			DriverWorkarounds value;
			value.dont_print_on_render_pipeline_creation_failure = raster_quiet;
			return value;
		}
		RenderPassID render_pass_create(VectorView<Attachment> attachments, VectorView<Subpass>, VectorView<SubpassDependency>, uint32_t, AttachmentReference) override {
			if (production_raster_factory) {
				++production_stage_attempts[2];
				if (production_fail_stage == 3) {
					return {};
				}
			}
			if (production_raster_join) {
				if (production_render_pass_creates) {
					REQUIRE(attachments.size() == 1);
					CHECK(attachments[0].load_op == ATTACHMENT_LOAD_OP_CLEAR);
				}
				return RenderPassID(280 + production_render_pass_creates++);
			}
			return RenderPassID(280);
		}
		void render_pass_free(RenderPassID id) override {
			if (production_raster_join) {
				CHECK(id.id >= 280);
				CHECK(id.id < 280 + production_render_pass_creates);
				if (id == RenderPassID(281)) {
					CHECK(production_depth_live);
					REQUIRE(production_raster_request != nullptr);
					CHECK(production_raster_request->resources[0].owned_physical.free_event == 0);
					CHECK(production_framebuffer_frees == 1);
					CHECK_FALSE(production_cache_pass_returned);
					production_cache_pass_returned = true;
				}
				++production_render_pass_frees;
				return;
			}
			CHECK(id == RenderPassID(280));
		}
		FramebufferID framebuffer_create(RenderPassID pass, VectorView<TextureID> attachments, uint32_t width, uint32_t height) override {
			CHECK(production_raster_join);
			CHECK(pass == RenderPassID(281));
			REQUIRE(attachments.size() == 1);
			CHECK(attachments[0] == TextureID(380));
			CHECK(width == 8);
			CHECK(height == 4);
			CHECK_FALSE(production_framebuffer_live);
			production_framebuffer_live = true;
			++production_framebuffer_creates;
			return FramebufferID(381);
		}
		void framebuffer_free(FramebufferID id) override {
			CHECK(id == FramebufferID(381));
			CHECK(production_framebuffer_live);
			CHECK_FALSE(production_render_pass_active);
			CHECK(production_depth_live);
			REQUIRE(production_raster_request != nullptr);
			CHECK(production_raster_request->resources[0].owned_physical.free_event == 0);
			CHECK_FALSE(production_cache_pass_returned);
			production_framebuffer_live = false;
			++production_framebuffer_frees;
		}
		void command_begin_render_pass(CommandBufferID command, RenderPassID pass, FramebufferID framebuffer, CommandBufferType, const Rect2i &rect, VectorView<RenderPassClearValue> clears) override {
			CHECK(production_raster_join);
			CHECK(command == CommandBufferID(90));
			CHECK(pass == RenderPassID(281));
			CHECK(framebuffer == FramebufferID(381));
			CHECK(rect == Rect2i(0, 0, 8, 4));
			CHECK(production_framebuffer_live);
			CHECK_FALSE(production_render_pass_active);
			REQUIRE(clears.size() == 1);
			CHECK(clears[0].depth == 1.0f);
			// The real graph callback selected a depth CLEAR load operation.
			++production_depth_clears;
			production_render_pass_active = true;
			++production_render_begins;
		}
		void command_end_render_pass(CommandBufferID command) override {
			CHECK(command == CommandBufferID(90));
			CHECK(production_render_pass_active);
			production_render_pass_active = false;
			++production_render_ends;
		}
		void command_render_set_viewport(CommandBufferID command, VectorView<Rect2i> rects) override {
			CHECK(command == CommandBufferID(90));
			CHECK(production_render_pass_active);
			REQUIRE(rects.size() == 1);
			CHECK(rects[0] == Rect2i(0, 0, 8, 4));
			++production_viewports;
		}
		void command_render_set_scissor(CommandBufferID command, VectorView<Rect2i> rects) override {
			CHECK(command == CommandBufferID(90));
			CHECK(production_render_pass_active);
			REQUIRE(rects.size() == 1);
			CHECK(rects[0] == Rect2i(0, 0, 8, 4));
			++production_scissors;
		}
		void command_render_clear_attachments(CommandBufferID command, VectorView<AttachmentClear> clears, VectorView<Rect2i> rects) override {
			CHECK(command == CommandBufferID(90));
			CHECK(production_render_pass_active);
			REQUIRE(clears.size() == 1);
			CHECK(clears[0].aspect.has_flag(TEXTURE_ASPECT_DEPTH_BIT));
			CHECK(clears[0].value.depth == 1.0f);
			REQUIRE(rects.size() == 1);
			CHECK(rects[0] == Rect2i(0, 0, 8, 4));
			++production_depth_clears;
		}
		void command_clear_depth_stencil_texture(CommandBufferID command, TextureID texture, TextureLayout, float depth, uint8_t stencil, const TextureSubresourceRange &) override {
			CHECK(production_raster_join);
			CHECK(command == CommandBufferID(90));
			CHECK(texture == TextureID(380));
			CHECK(depth == 1.0f);
			CHECK(stencil == 0);
			++production_depth_clears;
		}
		PipelineID render_pipeline_create(ShaderID shader, VertexFormatID, RenderPrimitive, PipelineRasterizationState, PipelineMultisampleState, PipelineDepthStencilState, PipelineColorBlendState, VectorView<int32_t>, BitField<PipelineDynamicStateFlags>, RenderPassID pass, uint32_t, VectorView<PipelineSpecializationConstant>) override {
			CHECK(Thread::get_caller_id() == execution_thread);
			CHECK(shader == ShaderID(280));
			CHECK(pass == RenderPassID(280));
			++raster_attempts;
			if (production_raster_factory) {
				++production_stage_attempts[3];
				if (production_fail_stage == 4) {
					return {};
				}
			}
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
				if (production_raster_factory) {
					++production_stage_attempts[1];
					if (production_fail_stage == 2) {
						return {};
					}
				}
				if (production_quiesce_stage == 2 && production_factory_probe) {
					production_factory_probe(production_factory_probe_data);
				}
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
				if (production_raster_factory) {
					REQUIRE(production_raster_request != nullptr);
					const auto &pipeline = production_raster_request->resources[3];
					CHECK((pipeline.primary.is_null() || pipeline.owned_physical.free_event));
					production_free_order.push_back(2);
				}
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
				if (production_raster_factory) {
					production_free_order.push_back(3);
				}
				return;
			}
			if (id == PipelineID(181)) {
				++auxiliary_pipeline_frees;
				return;
			}
			CHECK(id == PipelineID(81));
			++pipeline_frees;
		}
		mutable uint32_t production_shader_not_ready = 0, production_shader_ready_polls = 0;
		void (*disposal_probe)(void *) = nullptr;
		void *disposal_probe_data = nullptr;
		bool shader_retire_ready_nonblocking(ShaderID) const override {
			if (production_raster_factory) {
				++production_shader_ready_polls;
				if (production_shader_not_ready) {
					--production_shader_not_ready;
					CHECK(raster_shader_frees == 0);
					return false;
				}
			}
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

#ifdef THREADS_ENABLED
	static void quote_lock_contention(bool p_accept) {
		REQUIRE(Thread::is_main_thread()); // Never change the engine's thread identity.
		Enrollment fixture; // Reuse the existing original TLS/device/producer fixture.
		auto &registry = *fixture.registry;
		constexpr auto instance = Enrollment::INSTANCE;
		const GDExtensionProductionCapacityV1 capacity = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 2, 2, 2, 2, 2, 32, 0, 2, 8, 0 };
		GDExtensionProductionMetadataQuoteV1 baseline = {};
		REQUIRE(registry.production_quote(&Enrollment::ABI, { instance, 5 }, { instance, 9 }, { instance, 6 }, { instance, 7 }, &capacity, &baseline).native_error == OK);
		const uint32_t original_children = fixture.holder.production_children;
		const auto original_private_refs = registry.records[0].private_refs;
		struct Contention {
			Enrollment &fixture;
			std::mutex mutex;
			std::condition_variable condition;
			bool held = false, returned = false;
			bool returned_while_held = false, window_expired = false, registry_acquired = false;
			static void run(void *p_data) {
				auto &self = *static_cast<Contention *>(p_data);
				MutexLock rd_lock(self.fixture.rd._thread_safe_);
				{
					std::unique_lock<std::mutex> lock(self.mutex);
					self.held = true;
					self.condition.notify_one();
					// Bounded scheduling window, not a claimed mutex-wait hook.
					// Old source must be run with this test to witness ERR_BUSY while
					// RD is held; a pre-call flag alone cannot prove that interleaving.
					self.returned_while_held = self.condition.wait_for(lock, std::chrono::seconds(1), [&]() { return self.returned; });
					self.window_expired = !self.returned_while_held;
				}
				// Still holding the actual RD lock, acquire Registry in its original
				// order. The repaired main path must not hold Registry while waiting.
				MutexLock registry_lock(self.fixture.registry->mutex);
				self.registry_acquired = true;
			}
		} contention{ fixture };
		Thread worker;
		worker.start(&Contention::run, &contention);
		if (!worker.is_started()) {
			FAIL("Could not start the RD contention worker");
			return;
		}
		{
			std::unique_lock<std::mutex> lock(contention.mutex);
			contention.condition.wait(lock, [&]() { return contention.held; });
		}
		GDExtensionProductionMetadataQuoteV1 observed = {};
		GDExtensionProductionAttachmentRefV1 attachment = {};
		GDExtensionProductionInstallationReportV1 report = {};
		const auto result = p_accept ? registry.production_accept(&Enrollment::ABI, { instance, 5 }, { instance, 9 }, { instance, 6 }, { instance, 7 }, { instance, 3 }, &baseline, &attachment, &report) : registry.production_quote(&Enrollment::ABI, { instance, 5 }, { instance, 9 }, { instance, 6 }, { instance, 7 }, &capacity, &observed);
		{
			std::lock_guard<std::mutex> lock(contention.mutex);
			contention.returned = true;
			contention.condition.notify_one(); // Old failure exits the hold immediately.
		}
		worker.wait_to_finish(); // No assertion/REQUIRE while a worker can be live.
		INFO("quote/accept contention accept=", p_accept, " error=", result.native_error,
				" returned_while_rd_held=", contention.returned_while_held, " window_expired=", contention.window_expired,
				" worker_registry_acquired=", contention.registry_acquired);
		CHECK(result.native_error == OK);
		CHECK_FALSE(contention.returned_while_held);
		CHECK(contention.window_expired);
		CHECK(contention.registry_acquired);
		if (p_accept && result.native_error == OK) {
			CHECK(attachment.registry_instance == instance);
			CHECK(attachment.handle_serial != 0);
			CHECK(report.allocated_bytes == baseline.allocation_bytes);
			CHECK(report.returned_bytes == 0);
			CHECK(fixture.rd.production_entity_holds != nullptr);
			CHECK(registry.production_close(attachment).native_error == OK);
			CHECK(fixture.rd.production_entity_holds == nullptr);
			CHECK(fixture.holder.production_children == original_children);
			CHECK(registry.records[0].private_refs == original_private_refs);
		} else if (!p_accept && result.native_error == OK) {
			CHECK(std::memcmp(&observed, &baseline, sizeof(baseline)) == 0);
		}
		// Same original handles on a foreign thread do not transfer the TLS
		// SceneReady authority. Both output DTOs must remain untouched.
		struct Foreign {
			Enrollment &fixture;
			const GDExtensionProductionCapacityV1 &capacity;
			const GDExtensionProductionMetadataQuoteV1 &input;
			GDExtensionProductionMetadataQuoteV1 output = {};
			GDExtensionProductionAttachmentRefV1 attachment = {};
			GDExtensionProductionInstallationReportV1 report = {};
			int32_t quote_error = OK, accept_error = OK;
			static void run(void *p_data) {
				auto &self = *static_cast<Foreign *>(p_data);
				constexpr auto id = Enrollment::INSTANCE;
				self.quote_error = self.fixture.registry->production_quote(&Enrollment::ABI, { id, 5 }, { id, 9 }, { id, 6 }, { id, 7 }, &self.capacity, &self.output).native_error;
				self.accept_error = self.fixture.registry->production_accept(&Enrollment::ABI, { id, 5 }, { id, 9 }, { id, 6 }, { id, 7 }, { id, 3 }, &self.input, &self.attachment, &self.report).native_error;
			}
		} foreign{ fixture, capacity, baseline };
		std::memset(&foreign.output, 0x5a, sizeof(foreign.output));
		std::memset(&foreign.report, 0x5a, sizeof(foreign.report));
		const auto before_output = foreign.output;
		const auto before_report = foreign.report;
		Thread foreign_thread;
		foreign_thread.start(&Foreign::run, &foreign);
		if (!foreign_thread.is_started()) {
			FAIL("Could not start the foreign caller worker");
			return;
		}
		foreign_thread.wait_to_finish();
		CHECK(foreign.quote_error != OK);
		CHECK(foreign.accept_error != OK);
		CHECK(std::memcmp(&foreign.output, &before_output, sizeof(before_output)) == 0);
		CHECK(std::memcmp(&foreign.report, &before_report, sizeof(before_report)) == 0);
		CHECK(foreign.attachment.registry_instance == 0);
		CHECK(foreign.attachment.handle_serial == 0);
		CHECK(registry.production_quote(&Enrollment::ABI, { instance, 5 }, { instance, UINT64_MAX }, { instance, 6 }, { instance, 7 }, &capacity, &foreign.output).native_error != OK);
		CHECK(std::memcmp(&foreign.output, &before_output, sizeof(before_output)) == 0);
		CHECK(registry.production_accept(&Enrollment::ABI, { instance, 5 }, { instance, 9 }, { instance, 6 }, { instance, 7 }, { instance, UINT64_MAX }, &baseline, &foreign.attachment, &foreign.report).native_error != OK);
		CHECK(std::memcmp(&foreign.report, &before_report, sizeof(before_report)) == 0);
		CHECK(foreign.attachment.handle_serial == 0);
	}
#endif

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
		Error initialize_owner(uint32_t loader_kind = 1, bool owned_storage = false, bool owned_set = false, GDExtensionDeviceExit::ProductionRecordReceiver receiver = nullptr, void *userdata = nullptr, bool keep_list = false, uint32_t record_actions = 1, bool upload_prefix = false, GDExtensionDeviceExit::ProductionMetadataReceiver metadata_receiver = nullptr, bool output_closure = false, bool output_borrow = false, bool mesh_directory = false, bool directory_external = false, const OrdinalPlan *ordinal_plan = nullptr, RenderingDevice *shared_device = nullptr, bool defer_reserve = false) {
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
			if (defer_reserve) {
				return OK; // Accepted arena only; the caller exercises its typed reserve port.
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
		void cleanup_owner(bool runtime = false) {
			const auto check = [runtime](bool passed) {
				if (runtime) {
					CRASH_COND_MSG(!passed, "Original runtime enrollment cleanup failed");
					return passed;
				}
				return Access::check_step(passed);
			};
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
					if (!check(registry.production_control_schedule(ref, request->control.serial) == OK)) {
						return;
					}
					server->sync();
				}
				if (request->control.state == ProductionControlState::RETURNED) {
					ProductionControlOutput output = {};
					if (!check(registry.production_control_consume(ref, request->control.serial, &output) == OK)) {
						return;
					}
				}
				if (request->serial) {
					if (!check(registry.production_cancel(ref).native_error == OK)) {
						return;
					}
					if (!check(registry.production_release(ref).native_error == OK)) {
						return;
					}
				}
			}
			if (attachment.handle_serial) {
				if (!check((public_port ? port.close(attachment) : registry.production_close(attachment)).native_error == OK)) {
					return;
				}
			}
			if (qualification.handle_serial) {
				if (!check(registry.qualification_close(qualification).native_error == OK)) {
					return;
				}
			}
			if (producer.handle_serial) {
				if (!check(registry.release_producer_registration_view(producer) == OK)) {
					return;
				}
			}
			if (context.handle_serial) {
				if (!check(registry.release_device_reference(instance, context.handle_serial, false).native_error == OK)) {
					return;
				}
			}
			if (session.handle_serial) {
				if (!check(registry.release_device_reference(instance, session.handle_serial, true).native_error == OK)) {
					return;
				}
			}
			frame.leave();
			if (reference.is_valid()) {
				MutexLock lock(registry.mutex);
				auto &record = registry.records[reference.slot];
				if (!check(record.private_refs == 2)) {
					return;
				}
				if (!check(record.external_refs == 0)) {
					return;
				}
				if (!check(record.cleanup_reservations == 0)) {
					return;
				}
				if (!check(holder.production_children == 0)) {
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
		uint32_t first_failure_line = 0, last_failure_line = 0, failure_count = 0;
		void check(uint32_t line, bool condition) {
			if (!condition) {
				FAIL_CHECK("SplitOwner failed original source check at line ", line);
				if (!first_failure_line) {
					first_failure_line = line;
				}
				last_failure_line = line;
				++failure_count;
			}
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
			test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(__LINE__, request.next_control_serial == initial_serial);
			test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
			// A saturated FIFO can retain an unpublished turn beyond its original
			// deadline. Retrying that saved budget must settle its no-effect return.
			auto expiring = budget;
			uint64_t expired_serial = 0;
			for (uint32_t admission = 0; admission < 8 && !expired_serial; ++admission) {
				expiring.deadline_micros = OS::get_singleton()->get_ticks_usec() + 100000;
				const auto accepted = registry.production_observe(ref, &expiring, &report);
				if (request.control.state == ProductionControlState::PENDING) {
					test.check(__LINE__, accepted.native_error == ERR_BUSY);
					expired_serial = request.control.serial;
				} else {
					test.check(__LINE__, accepted.native_error == OK && request.next_control_serial == initial_serial);
				}
			}
			test.check(__LINE__, expired_serial != 0);
			if (expired_serial) {
				while (OS::get_singleton()->get_ticks_usec() <= expiring.deadline_micros) {
					OS::get_singleton()->delay_usec(1000);
				}
				test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK);
				test.check(__LINE__, request.control.state == ProductionControlState::PENDING);
				test.check(__LINE__, registry.production_observe(ref, &expiring, &report).native_error == OK);
				test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
				test.check(__LINE__, request.control.serial == expired_serial && request.next_control_serial == expired_serial + 1);
				test.check(__LINE__, fixture.driver.retains == 0 && fixture.driver.polls == 0);
			}
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.first_serial = request.control.serial;
			test.check(__LINE__, request.control.state == ProductionControlState::PENDING);
			auto changed_pending = budget;
			changed_pending.receipts = 2;
			test.check(__LINE__, registry.production_observe(ref, &changed_pending, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.serial == test.first_serial);
			test.check(__LINE__, request.control.input.budget.receipts == 1);
			test.check(__LINE__, fixture.driver.retains == 0 && fixture.driver.polls == 0);
			test.check(__LINE__, registry.production_cancel(ref).native_error == ERR_BUSY);
			test.check(__LINE__, !request.cancelled);
			test.check(__LINE__, registry.production_close(fixture.attachment).native_error == ERR_BUSY);
			test.rendezvous(); // The actual server thread drains the saturated FIFO.
			test.check(__LINE__, request.control.state == ProductionControlState::PENDING);
			auto expired_pending = budget;
			expired_pending.deadline_micros = 1;
			test.check(__LINE__, registry.production_observe(ref, &expired_pending, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.state == ProductionControlState::PENDING && request.control.serial == test.first_serial);
			test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(__LINE__, request.control.state == ProductionControlState::PENDING && fixture.driver.retains == 0 && fixture.driver.polls == 0);
			test.check(__LINE__, registry.production_observe(ref, &changed_pending, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.serial == test.first_serial);
			test.check(__LINE__, request.control.input.budget.receipts == 1);
			test.check(__LINE__, request.control.state == ProductionControlState::QUEUED);
			test.check(__LINE__, registry.production_observe(ref, &expired_pending, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.state == ProductionControlState::QUEUED && request.control.serial == test.first_serial);
			test.rendezvous();
			test.check(__LINE__, request.control.state == ProductionControlState::RETURNED);
			test.check(__LINE__, fixture.driver.retains == 1 && fixture.driver.polls == 0);
			test.check(__LINE__, registry.production_observe(ref, &zero, &report).native_error == OK);
			test.check(__LINE__, request.control.state == ProductionControlState::RETURNED);
			// A caller may refresh both the deadline and the next reduction. This
			// call consumes the original incomplete-source Busy without a new turn.
			test.check(__LINE__, request.control.output.result.native_error == ERR_BUSY);
			auto refreshed = changed_pending;
			refreshed.deadline_micros = OS::get_singleton()->get_ticks_usec() + 20000000;
			test.check(__LINE__, registry.production_observe(ref, &refreshed, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(__LINE__, request.next_control_serial == test.first_serial + 1);
			// Publish the new reduction only after the original return was consumed.
			auto reduced = budget;
			reduced.receipts = 2;
			test.check(__LINE__, registry.production_observe(ref, &reduced, &report).native_error == ERR_BUSY);
			test.second_serial = request.control.serial;
			test.check(__LINE__, test.second_serial > test.first_serial);
			test.check(__LINE__, request.control.input.budget.receipts == 2);
			test.rendezvous();
			// An expired nonzero offer also consumes a returned original turn.
			auto expired = budget;
			expired.deadline_micros = 1;
			test.check(__LINE__, registry.production_observe(ref, &expired, &report).native_error == OK);
			test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(__LINE__, fixture.driver.polls == 1 && report.known_obligations == 1 && report.complete_obligations == 1);
			// Hold the actual RD mutex across FIFO execution to produce a real
			// returned Busy. A changed expired offer must preserve that result,
			// and a second expired retry must not accept a replacement turn.
			fixture.rd._thread_safe_.lock();
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			const uint64_t busy_serial = request.control.serial;
			test.rendezvous();
			fixture.rd._thread_safe_.unlock();
			test.check(__LINE__, request.control.state == ProductionControlState::RETURNED && request.control.output.result.native_error == ERR_BUSY);
			test.check(__LINE__, registry.production_observe(ref, &expired, &report).native_error == ERR_BUSY);
			test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED);
			test.check(__LINE__, registry.production_observe(ref, &expired, &report).native_error == OK);
			test.check(__LINE__, request.control.state == ProductionControlState::OWNER_CONSUMED && request.next_control_serial == busy_serial + 1);
			test.check(__LINE__, fixture.driver.retains == 1 && fixture.driver.polls == 1);
			// Maintenance remains callable after the original owner's seal.
			{
				MutexLock lock(registry.mutex);
				registry.records[fixture.reference.slot].quiescing = true;
			}
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == ERR_BUSY);
			test.rendezvous();
			test.check(__LINE__, registry.production_observe(ref, &budget, &report).native_error == OK);
			test.check(__LINE__, fixture.driver.retains == 1 && fixture.driver.polls == 1);
			uint64_t release = 0;
			test.check(__LINE__, registry.production_control_accept(ref, ProductionControlKind::RELEASE, &release, &budget) == OK);
			test.check(__LINE__, registry.production_control_schedule(ref, release) == OK);
			test.rendezvous();
			ProductionControlOutput returned = {};
			test.check(__LINE__, registry.production_control_consume(ref, release, &returned) == OK);
			test.check(__LINE__, returned.owner_return.native_error == OK && fixture.driver.releases == 1);
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
		bool history_probe = false, history_empty_turn = false;
		uint32_t history_empty_calls = 0;
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
			if (test.history_empty_turn) {
				++test.history_empty_calls;
				return; // A later actual RECORD has no append call of its own.
			}
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
				if (test.history_probe) {
					const auto query = Access::ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1>("device_session_production_record_history_v1");
					test.check(__LINE__, query != nullptr);
					if (query) {
						GDExtensionProductionRecordHistoryV1 history{};
						test.check(__LINE__, query(ref, control, &history).native_error == OK);
						const uint32_t appended = GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1;
						test.check(__LINE__, (history.flags & appended) == appended);
						test.check(__LINE__, (history.flags & GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1) == 0);
						if (test.upload_prefix) {
							test.check(__LINE__, returned.append.state == GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 && returned.append.recorded_bytes == 64 && request.next_action == 0);
						}
						test.history_empty_turn = true;
						uint64_t empty_control = 0;
						test.check(__LINE__, test.schedule(ref, &empty_control) == OK && empty_control != control);
						test.rendezvous(9);
						GDExtensionProductionRecordReportV1 empty_return{};
						test.check(__LINE__, test.consume(ref, empty_control, &empty_return).native_error == OK);
						test.history_empty_turn = false;
						test.check(__LINE__, test.history_empty_calls == 1);
						test.check(__LINE__, query(ref, empty_control, &history).native_error == OK);
						test.check(__LINE__, (history.flags & GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1) != 0);
						test.check(__LINE__, (history.flags & (GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1)) == 0);
					}
				}
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
	static void singleton_split_owner_set(bool record_window = false, bool keep_list = false, bool close_before_ack = false, bool budget_pause = false, bool upload_prefix = false, bool public_port = false, bool output_closure = false, bool closure_retired = false, bool closure_rollback = false, bool output_borrow = false, bool mesh_directory = false, bool directory_capacity_failure = false, bool directory_external = false, bool v1_compat_probe = false, bool history_probe = false) {
		auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
		if (!check_step(server && !server->create_thread && server->server_thread == Thread::get_caller_id() && !RenderingDevice::get_singleton())) {
			return;
		}
		server->sync();
		SplitSet test;
		test.v1_compat_probe = v1_compat_probe;
		test.history_probe = history_probe;
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
			for (uint32_t phase = 0; phase < 6; ++phase) {
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
		Callable relay_callable;
		GDExtensionDeviceExit::NormalRelayToken relay;
		uint32_t relay_calls = 0, relay_frees = 0;
		static void relay_call(void *opaque, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr returned, GDExtensionCallError *error) {
			auto &probe = *static_cast<MetadataCallProbe *>(opaque);
			++probe.relay_calls;
			CHECK_FALSE(GDExtensionDeviceExit::Registry::is_production_metadata_entry());
			*reinterpret_cast<Variant *>(returned) = 42;
			*error = { GDEXTENSION_CALL_OK, 0, 0 };
		}
		static void relay_free(void *opaque) { ++static_cast<MetadataCallProbe *>(opaque)->relay_frees; }
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
			NormalRelaySnapshot relay_before, relay_after;
			CHECK(fixture.registry.observe_normal_relay(probe.relay, relay_before) == OK);
			const auto relay_calls_before = probe.relay_calls;
			Variant relay_returned = "metadata relay output sentinel";
			Callable::CallError relay_error;
			relay_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			relay_error.argument = 17;
			relay_error.expected = 23;
			CHECK(fixture.registry.call_normal_relay(probe.relay, nullptr, 0, relay_returned, relay_error) == ERR_UNAUTHORIZED);
			CHECK(probe.relay_calls == relay_calls_before);
			CHECK(relay_returned == Variant("metadata relay output sentinel"));
			CHECK(relay_error.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT);
			CHECK(relay_error.argument == 17);
			CHECK(relay_error.expected == 23);
			CHECK(fixture.registry.observe_normal_relay(probe.relay, relay_after) == OK);
			CHECK(relay_after.library_generation == relay_before.library_generation);
			CHECK(relay_after.destruction_thread == relay_before.destruction_thread);
			CHECK(relay_after.active_deliveries == relay_before.active_deliveries);
			CHECK(relay_after.cancelled == relay_before.cancelled);
			CHECK(relay_after.release_requested == relay_before.release_requested);
			CHECK(relay_after.cell_released == relay_before.cell_released);
			CHECK(relay_after.holder_closed == relay_before.holder_closed);
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
		info.call_func = &MetadataCallProbe::relay_call;
		info.free_func = &MetadataCallProbe::relay_free;
		if (create) {
			create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&probe.relay_callable), &info);
		}
		CHECK(probe.relay_callable.is_custom());
		CHECK(fixture.registry.register_normal_relay(probe.relay_callable, probe.relay) == OK);
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
		CHECK(probe.relay_calls == 0);
		Variant relay_returned;
		Callable::CallError relay_error;
		CHECK(fixture.registry.call_normal_relay(probe.relay, nullptr, 0, relay_returned, relay_error) == OK);
		CHECK(relay_error.error == Callable::CallError::CALL_OK);
		CHECK(relay_returned == Variant(42));
		CHECK(probe.relay_calls == 1);
		CHECK(fixture.registry.release_normal_relay_holder(probe.relay) == OK);
		GDExtensionDeviceExit::NormalRelaySnapshot relay_snapshot;
		CHECK(fixture.registry.observe_normal_relay(probe.relay, relay_snapshot) == OK);
		CHECK(relay_snapshot.cell_released);
		CHECK_FALSE(relay_snapshot.holder_closed);
		CHECK(probe.relay_frees == 0);
		CHECK(fixture.registry.forget_normal_relay(probe.relay) == ERR_BUSY);
		probe.relay_callable = Callable();
		CHECK(probe.relay_frees == 1);
		CHECK(fixture.registry.observe_normal_relay(probe.relay, relay_snapshot) == OK);
		CHECK(relay_snapshot.holder_closed);
		CHECK(relay_snapshot.active_deliveries == 0);
		CHECK(fixture.registry.forget_normal_relay(probe.relay) == OK);
		CHECK(fixture.registry.observe_normal_relay(probe.relay, relay_snapshot) == ERR_INVALID_DATA);
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
	struct NativeScenePhaseProbe {
		SingletonEnrollment *fixture = nullptr;
		Callable callable;
		GDExtensionDeviceExit::NormalRelayToken relay;
		GDExtensionScenePhaseRegistrationRefV1 registration{};
		GDExtensionSceneOpaque32V1 device_capability{};
		GDExtensionProductionRequestRefV1 requests[2]{};
		GDExtensionProductionPhysicalContextRefV1 contexts[2]{};
		GDExtensionNativeScenePhaseV1 phases[2]{};
		uint32_t typed_calls = 0, ordinary_calls = 0, legacy_calls = 0, frees = 0;
		GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1 unregister_port = nullptr;
		bool close_active = false;
		static uint64_t word(const GDExtensionSceneOpaque32V1 &opaque, uint32_t index) {
			uint64_t value = 0;
			for (uint32_t i = 0; i < 8; ++i) {
				value |= uint64_t(opaque.bytes[index * 8 + i]) << (8 * i);
			}
			return value;
		}
		static void ordinary(void *opaque, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr returned, GDExtensionCallError *error) {
			auto &p = *static_cast<NativeScenePhaseProbe *>(opaque);
			++p.ordinary_calls;
			*reinterpret_cast<Variant *>(returned) = Variant();
			*error = { GDEXTENSION_CALL_OK, 0, 0 };
		}
		static void legacy(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {
			++static_cast<NativeScenePhaseProbe *>(opaque)->legacy_calls;
		}
		static void free(void *opaque) { ++static_cast<NativeScenePhaseProbe *>(opaque)->frees; }
		static void typed(void *opaque, const GDExtensionNativeScenePhaseV1 *phase, GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOperationV1 *operation) {
			using namespace GDExtensionDeviceExit;
			auto &p = *static_cast<NativeScenePhaseProbe *>(opaque);
			REQUIRE(phase != nullptr);
			REQUIRE(operation != nullptr);
			REQUIRE(p.typed_calls < 2);
			const uint32_t index = p.typed_calls++;
			p.phases[index] = *phase; // Observation copy, never later authorization.
			p.contexts[index] = { word(phase->physical_context, 0), word(phase->physical_context, 1) };
			CHECK(phase->abi.abi_version == 1);
			CHECK(phase->abi.struct_size == sizeof(*phase));
			CHECK(phase->abi.reserved == 0);
			CHECK(memcmp(&phase->device_capability, &p.device_capability, sizeof(p.device_capability)) == 0);
			CHECK(word(phase->phase_token, 1) == word(p.registration.token, 1));
			CHECK(phase->occurrence != 0);
			CHECK(phase->expiry_serial == p.contexts[index].frame_serial);
			CHECK(word(phase->phase_token, 3) == phase->expiry_serial);
			CHECK(word(phase->physical_context, 2) == request.handle_serial);
			CHECK(operation->owner_request_nonce != 0);
			CHECK(request.registry_instance == p.fixture->instance);
			CHECK((request.handle_serial == p.requests[0].handle_serial || request.handle_serial == p.requests[1].handle_serial));
			{
				MutexLock lock(p.fixture->registry.mutex);
				ProductionRequest *original = nullptr;
				RenderingDevice *device = nullptr;
				CHECK(p.fixture->registry.production_physical_request_locked(p.contexts[index], request, original, device, false) == OK);
				CHECK(original != nullptr);
				CHECK(device == &p.fixture->rd);
				if (original) {
					CHECK(original->control.state == ProductionControlState::EXECUTING);
					CHECK(operation->owner_request_nonce == original->operation.owner_request_nonce);
					CHECK(operation->record_generation == original->operation.record_generation);
					CHECK(operation->canonical_begin == original->operation.canonical_begin);
					CHECK(operation->canonical_end == original->operation.canonical_end);
					CHECK(original->graph_count == 0);
					CHECK(original->next_action == 0);
				}
			}
			// Actual original APIs cannot free the current attachment/request
			// across dispatch's unlocked callback; do not simulate a pin here.
			CHECK(p.fixture->registry.production_close(p.fixture->attachment).native_error == ERR_BUSY);
			CHECK(p.fixture->registry.production_release(request).native_error == ERR_BUSY);
			NormalRelaySnapshot active;
			REQUIRE(p.fixture->registry.observe_normal_relay(p.relay, active) == OK);
			CHECK(active.active_deliveries == 1);
			CHECK_FALSE(active.holder_closed);
			if (p.close_active && index == 1) {
				GDExtensionScenePhaseCloseV1 close{};
				CHECK(p.unregister_port(p.registration, &close).native_error == ERR_BUSY);
				CHECK(close.registration_closed == 1);
				CHECK(close.active_deliveries == 1);
				CHECK(close.relay_holder_closed == 0);
				CHECK(p.frees == 0);
			}
		}
	};
	static void native_scene_phase_infrastructure(bool close_before_draw = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		plan.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		NativeScenePhaseProbe probe;
		probe.fixture = &fixture;
		probe.close_active = true;
		const auto capability = ordinal_port<GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1>("device_session_scene_phase_capability_v1");
		const auto relay = ordinal_port<GDExtensionInterfaceDeviceSessionScenePhaseRelayV1>("device_session_scene_phase_relay_v1");
		const auto register_phase = ordinal_port<GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1>("device_session_scene_phase_register_v1");
		probe.unregister_port = ordinal_port<GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1>("device_session_scene_phase_unregister_v1");
		REQUIRE(capability != nullptr);
		REQUIRE(relay != nullptr);
		REQUIRE(register_phase != nullptr);
		REQUIRE(probe.unregister_port != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &NativeScenePhaseProbe::legacy, &probe) == OK);
		probe.requests[0] = { fixture.instance, fixture.request->serial };
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		auto second_operation = plan.operation;
		++second_operation.owner_request_nonce;
		REQUIRE(reserve(fixture.attachment, &second_operation, &plan.capacity, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &probe.requests[1]).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			fixture.registry.records[fixture.reference.slot].guard.entry_classes |= 1u << 8;
		}
		GDExtensionCallableCustomInfo2 info{};
		info.callable_userdata = &probe;
		info.call_func = &NativeScenePhaseProbe::ordinary;
		info.free_func = &NativeScenePhaseProbe::free;
		info.token = fixture.registry.records[fixture.reference.slot].original_owner;
		const auto create = reinterpret_cast<GDExtensionInterfaceCallableCustomCreate2>(GDExtension::get_interface_function("callable_custom_create2"));
		REQUIRE(create != nullptr);
		create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&probe.callable), &info);
		REQUIRE(probe.callable.is_custom());
		GDExtensionNormalRelayRefV1 relay_wire{};
		const auto callable = reinterpret_cast<GDExtensionConstTypePtr>(&probe.callable);
		CHECK(relay(nullptr, &NativeScenePhaseProbe::typed, &relay_wire).native_error == ERR_INVALID_PARAMETER);
		CHECK(relay(callable, nullptr, &relay_wire).native_error == ERR_INVALID_PARAMETER);
		CHECK(relay(callable, &NativeScenePhaseProbe::typed, nullptr).native_error == ERR_INVALID_PARAMETER);
		CHECK(NativeScenePhaseProbe::word(relay_wire.token, 1) == 0);
		REQUIRE(relay(callable, &NativeScenePhaseProbe::typed, &relay_wire).native_error == OK);
		GDExtensionNormalRelayRefV1 duplicate_relay{};
		REQUIRE(relay(callable, &NativeScenePhaseProbe::typed, &duplicate_relay).native_error == OK);
		CHECK(memcmp(&duplicate_relay, &relay_wire, sizeof(relay_wire)) == 0);
		CHECK(relay(callable, &NativeScenePhaseProbe::typed, &duplicate_relay).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(&duplicate_relay, &relay_wire, sizeof(relay_wire)) == 0);
		// Obtain the already installed original token only for Registry observers;
		// do not assemble internal authority from exported wire bytes.
		REQUIRE(fixture.registry.scene_phase_install_relay(probe.callable, &NativeScenePhaseProbe::typed, probe.relay) == OK);
		GDExtensionNormalRelayRefV1 observed_relay{};
		REQUIRE(fixture.registry.scene_phase_encode_relay(probe.relay, observed_relay) == OK);
		CHECK(memcmp(&observed_relay, &relay_wire, sizeof(relay_wire)) == 0);
		GDExtensionScenePhaseSinkConfigV1 config{};
		config.abi = { 1, sizeof(config), 0 };
		config.phase = config.max_operations = 1;
		CHECK(capability(fixture.attachment, nullptr).native_error == ERR_INVALID_PARAMETER);
		REQUIRE(capability(fixture.attachment, &config.device_capability).native_error == OK);
		probe.device_capability = config.device_capability;
		GDExtensionScenePhaseRegistrationRefV1 rejected_registration{};
		CHECK(register_phase(relay_wire, fixture.attachment, nullptr, &rejected_registration).native_error == ERR_INVALID_PARAMETER);
		CHECK(register_phase(relay_wire, fixture.attachment, &config, nullptr).native_error == ERR_INVALID_PARAMETER);
		auto wrong_capability = config;
		wrong_capability.device_capability.bytes[0] ^= 0x80;
		CHECK(register_phase(relay_wire, fixture.attachment, &wrong_capability, &rejected_registration).native_error == ERR_INVALID_DATA);
		CHECK(NativeScenePhaseProbe::word(rejected_registration.token, 1) == 0);
		CHECK(probe.unregister_port({}, nullptr).native_error == ERR_INVALID_PARAMETER);
		// A real same-owner normal relay lacks the installed typed phase leaf.
		{
			NativeScenePhaseProbe plain;
			auto plain_info = info;
			plain_info.callable_userdata = &plain;
			create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&plain.callable), &plain_info);
			REQUIRE(plain.callable.is_custom());
			REQUIRE(fixture.registry.register_normal_relay(plain.callable, plain.relay) == OK);
			GDExtensionNormalRelayRefV1 plain_wire{};
			memset(&plain_wire, 0xa5, sizeof(plain_wire));
			const auto unchanged_wire = plain_wire;
			CHECK(fixture.registry.scene_phase_encode_relay(plain.relay, plain_wire) == ERR_UNAUTHORIZED);
			CHECK(memcmp(&plain_wire, &unchanged_wire, sizeof(plain_wire)) == 0);
			REQUIRE(fixture.registry.cancel_normal_relay(plain.relay) == OK);
			CHECK(fixture.registry.release_normal_relay_holder(plain.relay) == OK);
			CHECK(plain.frees == 0);
			plain.callable = Callable();
			CHECK(plain.frees == 1);
			fixture.frame.leave(); // Original outer-frame return acknowledges retired holders.
			NormalRelaySnapshot returned;
			REQUIRE(fixture.registry.observe_normal_relay(plain.relay, returned) == OK);
			CHECK(returned.holder_closed);
			CHECK(returned.cell_released);
			REQUIRE(fixture.registry.forget_normal_relay(plain.relay) == OK);
		}
		// Reject key/output aliasing before either input or output is touched.
		const auto query = ordinal_port<GDExtensionInterfaceDeviceSessionSceneOperationQueryV1>("device_session_scene_operation_query_v1");
		REQUIRE(query != nullptr);
		alignas(GDExtensionSceneOperationKeyV1) alignas(GDExtensionSceneReadOutcomeV1) uint8_t overlap[sizeof(GDExtensionSceneOperationKeyV1) + sizeof(GDExtensionSceneReadOutcomeV1)];
		memset(overlap, 0xa5, sizeof(overlap));
		uint8_t untouched[sizeof(overlap)];
		memcpy(untouched, overlap, sizeof(overlap));
		CHECK(query(probe.requests[0], 0, reinterpret_cast<const GDExtensionSceneOperationKeyV1 *>(overlap), reinterpret_cast<GDExtensionSceneReadOutcomeV1 *>(overlap)).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(overlap, untouched, sizeof(overlap)) == 0);
		memcpy(overlap, untouched, sizeof(overlap));
		CHECK(query({}, 0, reinterpret_cast<const GDExtensionSceneOperationKeyV1 *>(overlap), reinterpret_cast<GDExtensionSceneReadOutcomeV1 *>(overlap)).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(overlap, untouched, sizeof(overlap)) == 0);
		memcpy(overlap, untouched, sizeof(overlap));
		CHECK(query(probe.requests[0], 0, reinterpret_cast<const GDExtensionSceneOperationKeyV1 *>(overlap + 1), reinterpret_cast<GDExtensionSceneReadOutcomeV1 *>(overlap + sizeof(GDExtensionSceneOperationKeyV1))).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(overlap, untouched, sizeof(overlap)) == 0);
		for (uint32_t bad_budget : { 0u, 65u }) {
			auto invalid = config;
			invalid.max_operations = bad_budget;
			GDExtensionScenePhaseRegistrationRefV1 rejected{};
			CHECK(register_phase(relay_wire, fixture.attachment, &invalid, &rejected).native_error == ERR_INVALID_PARAMETER);
			CHECK(NativeScenePhaseProbe::word(rejected.token, 1) == 0);
		}
		REQUIRE(register_phase(relay_wire, fixture.attachment, &config, &probe.registration).native_error == OK);
		GDExtensionScenePhaseRegistrationRefV1 duplicate{};
		REQUIRE(register_phase(relay_wire, fixture.attachment, &config, &duplicate).native_error == OK);
		CHECK(memcmp(&duplicate, &probe.registration, sizeof(duplicate)) == 0);
		auto different = config;
		different.max_operations = 2;
		GDExtensionScenePhaseRegistrationRefV1 conflict{};
		CHECK(register_phase(relay_wire, fixture.attachment, &different, &conflict).native_error == ERR_ALREADY_IN_USE);
		CHECK(NativeScenePhaseProbe::word(conflict.token, 1) == 0);
		CHECK(probe.unregister_port(probe.registration, nullptr).native_error == ERR_INVALID_PARAMETER);
		GDExtensionScenePhaseCloseV1 refused_close{};
		const GDExtensionScenePhaseRegistrationRefV1 wrong_kind{ relay_wire.token };
		CHECK(probe.unregister_port(wrong_kind, &refused_close).native_error == ERR_INVALID_DATA);
		CHECK(refused_close.registration_closed == 0);
		auto foreign_registration = probe.registration;
		foreign_registration.token.bytes[0] ^= 0x80;
		CHECK(probe.unregister_port(foreign_registration, &refused_close).native_error == ERR_INVALID_DATA);
		CHECK(refused_close.registration_closed == 0);
		uint64_t controls[2]{};
		for (uint32_t i = 0; i < 2; ++i) {
			REQUIRE(fixture.port.record_schedule(probe.requests[i], &controls[i]) == OK);
		}
		server->sync(); // Real original FIFO dispatch; never a phase issuer.
		CHECK(probe.typed_calls == 0);
		CHECK(probe.legacy_calls == 0);
		for (uint32_t i = 0; i < 2; ++i) {
			MutexLock lock(fixture.registry.mutex);
			ProductionRequest *original = nullptr;
			RenderingDevice *device = nullptr;
			REQUIRE(fixture.registry.production_request_locked(probe.requests[i], false, original, device) == OK);
			CHECK(original->control.state == ProductionControlState::QUEUED);
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		GDExtensionProductionRecordReportV2 recorded{};
		CHECK(consume(probe.requests[0], controls[0], &recorded).native_error == ERR_BUSY);
		if (close_before_draw) {
			// Close two actual waiting RECORD mailboxes without issuing a phase.
			GDExtensionScenePhaseCloseV1 close{};
			bool consumed[2] = {};
			for (uint32_t i = 0; i < 2; ++i) {
				CHECK(probe.unregister_port(probe.registration, &close).native_error == ERR_BUSY);
				CHECK(close.registration_closed == 1);
				CHECK(close.active_deliveries == 0);
				CHECK(close.relay_cell_released == 0);
				CHECK(close.relay_holder_closed == 0);
				NormalRelaySnapshot waiting;
				REQUIRE(fixture.registry.observe_normal_relay(probe.relay, waiting) == OK);
				CHECK_FALSE(waiting.cancelled);
				CHECK_FALSE(waiting.cell_released);
				CHECK_FALSE(waiting.holder_closed);
				server->sync();
				uint32_t returned_this_turn = 0;
				for (uint32_t j = 0; j < 2; ++j) {
					if (consumed[j]) {
						continue;
					}
					const Error returned = Error(consume(probe.requests[j], controls[j], &recorded).native_error);
					CHECK((returned == OK || returned == ERR_BUSY));
					if (returned == OK) {
						consumed[j] = true;
						++returned_this_turn;
						CHECK(recorded.result.native_error == ERR_UNAVAILABLE);
						CHECK(recorded.control_serial == controls[j]);
						CHECK(recorded.request_serial == probe.requests[j].handle_serial);
					}
				}
				CHECK(returned_this_turn == 1); // Original arena order need not equal reservation order.
				CHECK(probe.typed_calls == 0);
				CHECK(probe.ordinary_calls == 0);
				CHECK(probe.legacy_calls == 0);
				CHECK(probe.frees == 0);
			}
			CHECK(probe.unregister_port(probe.registration, &close).native_error == ERR_BUSY);
			CHECK(close.active_deliveries == 0);
			CHECK(close.relay_cell_released == 1);
			CHECK(close.relay_holder_closed == 0);
			CHECK(fixture.registry.forget_normal_relay(probe.relay) == ERR_BUSY);
			probe.callable = Callable();
			CHECK(probe.frees == 1);
			REQUIRE(probe.unregister_port(probe.registration, &close).native_error == OK);
			CHECK(close.relay_holder_closed == 1);
			NormalRelaySnapshot forgotten;
			CHECK(fixture.registry.observe_normal_relay(probe.relay, forgotten) == ERR_INVALID_DATA);
			for (uint32_t i = 0; i < 2; ++i) {
				REQUIRE(fixture.registry.production_cancel(probe.requests[i]).native_error == OK);
				REQUIRE(fixture.registry.production_release(probe.requests[i]).native_error == OK);
			}
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
			return;
		}
		for (uint32_t occurrence = 0; occurrence < 2; ++occurrence) {
			server->draw(false, 0.0); // Actual production early hook, not TESTS observer or direct dispatcher.
			server->sync();
			CHECK(probe.typed_calls == occurrence + 1);
			if (probe.typed_calls != occurrence + 1) {
				// A missing actual hook must fail with original owners returned.
				// Closing wakes at most one waiting mailbox per original FIFO turn;
				// two requests plus the original relay release need three turns.
				GDExtensionScenePhaseCloseV1 refused{};
				for (uint32_t turn = 0; turn < 3; ++turn) {
					const Error closed = Error(probe.unregister_port(probe.registration, &refused).native_error);
					CHECK(closed == ERR_BUSY);
					CHECK(refused.active_deliveries == 0);
					server->sync();
				}
				for (uint32_t i = 0; i < 2; ++i) {
					CHECK(consume(probe.requests[i], controls[i], &recorded).native_error == OK);
					CHECK((recorded.result.native_error == OK || recorded.result.native_error == ERR_UNAVAILABLE));
					CHECK(fixture.registry.production_cancel(probe.requests[i]).native_error == OK);
					CHECK(fixture.registry.production_release(probe.requests[i]).native_error == OK);
				}
				CHECK(refused.relay_cell_released == 1);
				CHECK(refused.relay_holder_closed == 0);
				probe.callable = Callable();
				CHECK(probe.frees == 1);
				CHECK(probe.unregister_port(probe.registration, &refused).native_error == OK);
				CHECK(refused.relay_holder_closed == 1);
				fixture.cleanup_owner();
				CHECK(fixture.owner_cleaned);
				return;
			}
			CHECK(probe.ordinary_calls == 0);
			CHECK(probe.legacy_calls == 0);
			uint32_t returned = 0;
			for (uint32_t i = 0; i < 2; ++i) {
				MutexLock lock(fixture.registry.mutex);
				ProductionRequest *original = nullptr;
				RenderingDevice *device = nullptr;
				REQUIRE(fixture.registry.production_request_locked(probe.requests[i], false, original, device) == OK);
				returned += original->control.state == ProductionControlState::RETURNED;
				CHECK(original->graph_count == 0);
			}
			CHECK(returned == occurrence + 1); // max_operations=1 applies to real mailbox delivery.
			MutexLock lock(fixture.registry.mutex);
			ProductionRequest *expired = nullptr;
			RenderingDevice *device = nullptr;
			const GDExtensionProductionRequestRefV1 delivered = { NativeScenePhaseProbe::word(probe.phases[occurrence].physical_context, 0), NativeScenePhaseProbe::word(probe.phases[occurrence].physical_context, 2) };
			CHECK(fixture.registry.production_physical_request_locked(probe.contexts[occurrence], delivered, expired, device, false) == ERR_UNAUTHORIZED);
			CHECK(expired == nullptr);
		}
		CHECK(probe.phases[1].occurrence > probe.phases[0].occurrence);
		CHECK(probe.contexts[1].frame_serial != probe.contexts[0].frame_serial);
		for (uint32_t i = 0; i < 2; ++i) {
			REQUIRE(consume(probe.requests[i], controls[i], &recorded).native_error == OK);
			CHECK(recorded.result.native_error == OK);
		}
		GDExtensionScenePhaseCloseV1 close{};
		CHECK(probe.unregister_port(probe.registration, &close).native_error == ERR_BUSY);
		CHECK(close.active_deliveries == 0);
		CHECK(close.relay_cell_released == 1);
		CHECK(close.relay_holder_closed == 0);
		CHECK(probe.frees == 0);
		CHECK(fixture.registry.forget_normal_relay(probe.relay) == ERR_BUSY);
		probe.callable = Callable(); // Original destructor/free acknowledges its actual holder.
		CHECK(probe.frees == 1);
		NormalRelaySnapshot final_relay;
		REQUIRE(fixture.registry.observe_normal_relay(probe.relay, final_relay) == OK);
		CHECK(final_relay.active_deliveries == 0);
		CHECK(final_relay.cancelled);
		CHECK(final_relay.cell_released);
		CHECK(final_relay.holder_closed);
		CHECK(final_relay.destruction_thread == Thread::get_caller_id());
		REQUIRE(probe.unregister_port(probe.registration, &close).native_error == OK);
		CHECK(close.relay_holder_closed == 1);
		CHECK(close.relay_cell_released == 1);
		CHECK(fixture.registry.observe_normal_relay(probe.relay, final_relay) == ERR_INVALID_DATA);
		const auto completed_close = close;
		REQUIRE(probe.unregister_port(probe.registration, &close).native_error == OK);
		CHECK(memcmp(&close, &completed_close, sizeof(close)) == 0);
		// Closed registration returns the original queued control without a draw.
		uint64_t closed_control = 0;
		REQUIRE(fixture.port.record_schedule(probe.requests[0], &closed_control) == OK);
		server->sync();
		REQUIRE(consume(probe.requests[0], closed_control, &recorded).native_error == OK);
		CHECK(recorded.result.native_error == ERR_UNAVAILABLE);
		CHECK(probe.typed_calls == 2);
		server->draw(false, 0.0);
		server->sync();
		CHECK(probe.typed_calls == 2);
		for (uint32_t i = 0; i < 2; ++i) {
			REQUIRE(fixture.registry.production_cancel(probe.requests[i]).native_error == OK);
			REQUIRE(fixture.registry.production_release(probe.requests[i]).native_error == OK);
		}
		CHECK(fixture.registry.forget_normal_relay(probe.relay) == ERR_INVALID_DATA);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void record_history_zero(const GDExtensionProductionRecordHistoryV1 &value) {
		CHECK(value.abi.abi_version == 0);
		CHECK(value.abi.struct_size == 0);
		CHECK(value.abi.reserved == 0);
		CHECK(value.request_serial == 0);
		CHECK(value.control_serial == 0);
		CHECK(value.record_generation == 0);
		CHECK(value.state == 0);
		CHECK(value.flags == 0);
		CHECK(v1_result(value.last_schedule_result, OK, 0));
		CHECK(v1_result(value.record_result, OK, 0));
	}
	static void record_history_flags(const GDExtensionProductionRecordHistoryV1 &value, uint32_t present, uint32_t absent) {
		CHECK(value.abi.abi_version == 1);
		CHECK(value.abi.struct_size == sizeof(value));
		CHECK(value.abi.reserved == 0);
		CHECK((value.flags & present) == present);
		CHECK((value.flags & absent) == 0);
	}
	struct OutputMetadataProbe {
		SingletonEnrollment *fixture = nullptr;
		GDExtensionDeviceExit::ProductionOutputMetadataLease first, second;
		GDExtensionDeviceExit::ProductionPhysicalContextRef expired{};
		uint32_t calls = 0, foreign_calls = 0;
		bool retry_returned = false;
		~OutputMetadataProbe() {
			// Failure cleanup returns only actual factory leases on this original
			// execution thread. It never manufactures a returned row or context.
			if (!second.is_empty()) {
				CHECK(fixture->registry.production_output_metadata_return(second) == OK);
			}
			if (!first.is_empty()) {
				CHECK(fixture->registry.production_output_metadata_return(first) == OK);
			}
		}
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<OutputMetadataProbe *>(opaque);
			auto &fixture = *probe.fixture;
			auto &registry = fixture.registry;
			auto &request = *fixture.request;
			const GDExtensionProductionRequestRefV1 original = { fixture.instance, request.serial };
			ProductionOutputMetadataLease rejected;
			if (ref.handle_serial != original.handle_serial) {
				++probe.foreign_calls;
				CHECK(registry.production_output_metadata_create(context, original, 0, fixture.rd, 64, 8, rejected) == ERR_UNAUTHORIZED);
				CHECK(rejected.is_empty());
				CHECK(registry.production_output_metadata_validate(context, ref, probe.first, fixture.rd, 64, 8) == ERR_INVALID_DATA);
				CHECK_FALSE(probe.first.is_empty());
				return;
			}
			++probe.calls;
			probe.expired = context;
			CHECK(request.frozen);
			CHECK(request.control.state == ProductionControlState::EXECUTING);
			CHECK(request.control.input.kind == ProductionControlKind::RECORD);
			CHECK(request.action_count == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
			if (probe.retry_returned) {
				CHECK(registry.production_output_metadata_create(context, ref, 1, fixture.rd, 1, 8, rejected) == ERR_ALREADY_IN_USE);
				CHECK(rejected.is_empty());
				CHECK_FALSE(probe.first.is_empty());
				return;
			}
			auto refuse = [&](ProductionPhysicalContextRef target_context, GDExtensionProductionRequestRefV1 target_request, uint32_t slot, uint64_t bytes, uint32_t alignment, Error expected) {
				const uint64_t revision = request.revision;
				CHECK(registry.production_output_metadata_create(target_context, target_request, slot, fixture.rd, bytes, alignment, rejected) == expected);
				CHECK(request.revision == revision);
				CHECK(rejected.is_empty());
				CHECK(rejected.get_data() == nullptr);
				CHECK(rejected.get_size() == 0);
			};
			refuse(context, ref, 0, 0, 8, ERR_INVALID_PARAMETER);
			refuse(context, ref, 0, 64, 0, ERR_INVALID_PARAMETER);
			refuse(context, ref, 0, 64, 3, ERR_INVALID_PARAMETER);
			refuse(context, ref, 0, 64, 2 * alignof(std::max_align_t), ERR_INVALID_PARAMETER);
			auto wrong_context = context;
			++wrong_context.frame_serial;
			refuse(wrong_context, ref, 0, 64, 8, ERR_UNAUTHORIZED);
			refuse(context, { ref.registry_instance, UINT64_MAX }, 0, 64, 8, ERR_UNAUTHORIZED);
			refuse(context, { UINT64_MAX, ref.handle_serial }, 0, 64, 8, ERR_UNAUTHORIZED);
			refuse(context, ref, 2, 64, 8, ERR_INVALID_PARAMETER);
			refuse(context, ref, 0, 129, 8, ERR_PARAMETER_RANGE_ERROR);
			{
				RenderingDevice other_device;
				CHECK(registry.production_output_metadata_create(context, ref, 0, other_device, 64, 8, rejected) == ERR_UNAUTHORIZED);
				CHECK(rejected.is_empty());
			}
			const uint64_t first_revision = request.revision;
			REQUIRE(registry.production_output_metadata_create(context, ref, 0, fixture.rd, 64, 8, probe.first) == OK);
			CHECK(request.revision == first_revision + 1);
			CHECK_FALSE(probe.first.is_empty());
			CHECK(probe.first.get_size() == 128); // Full declared extent, not the minimum.
			REQUIRE(probe.first.get_data() != nullptr);
			CHECK(reinterpret_cast<uintptr_t>(probe.first.get_data()) % 8 == 0);
			CHECK(registry.production_output_metadata_validate(context, ref, probe.first, fixture.rd, 128, 8) == OK);
			CHECK(registry.production_output_metadata_validate(context, ref, probe.first, fixture.rd, 129, 8) == ERR_PARAMETER_RANGE_ERROR);
			CHECK(registry.production_output_metadata_validate(context, ref, rejected, fixture.rd, 64, 8) == ERR_INVALID_DATA);
			auto *bytes = static_cast<uint8_t *>(probe.first.get_data());
			for (uint32_t i = 0; i < 128; ++i) {
				bytes[i] = uint8_t(i ^ 0x5a);
			}
			void *const original_data = probe.first.get_data();
			CHECK(registry.production_output_metadata_create({}, {}, UINT32_MAX, fixture.rd, 0, 0, probe.first) == ERR_ALREADY_IN_USE);
			CHECK(probe.first.get_data() == original_data);
			CHECK(probe.first.get_size() == 128);
			refuse(context, ref, 0, 64, 8, ERR_ALREADY_IN_USE);
			ProductionOutputMetadataLease moved(std::move(probe.first));
			CHECK(probe.first.is_empty());
			CHECK(probe.first.get_data() == nullptr);
			CHECK(probe.first.get_size() == 0);
			CHECK(moved.get_data() == original_data);
			CHECK(registry.production_output_metadata_validate(context, ref, moved, fixture.rd, 128, 8) == OK);
			CHECK(registry.production_output_metadata_validate(context, ref, probe.first, fixture.rd, 64, 8) == ERR_INVALID_DATA);
			probe.first = std::move(moved);
			CHECK(moved.is_empty());
			CHECK(probe.first.get_data() == original_data);
			for (uint32_t i = 0; i < 128; ++i) {
				CHECK(static_cast<const uint8_t *>(probe.first.get_data())[i] == uint8_t(i ^ 0x5a));
			}
			const uint64_t second_revision = request.revision;
			REQUIRE(registry.production_output_metadata_create(context, ref, 1, fixture.rd, 48, 8, probe.second) == OK);
			CHECK(request.revision == second_revision + 1);
			CHECK(probe.second.get_size() == 48);
			REQUIRE(probe.second.get_data() != nullptr);
			CHECK(probe.second.get_data() != probe.first.get_data());
			std::memset(probe.second.get_data(), 0xa5, size_t(probe.second.get_size()));
			GDExtensionProductionReportV2 report{};
			REQUIRE(registry.production_report_contents_locked(request, report) == OK);
			CHECK(report.physical_returns_pending == 2);
			CHECK(report.known_obligations == 0);
			CHECK(report.possible_obligations == 0);
			CHECK(report.complete_obligations == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
		}
	};
	static void output_metadata_factory() {
		using namespace GDExtensionDeviceExit;
		static_assert(!std::is_copy_constructible_v<ProductionOutputMetadataLease>);
		static_assert(!std::is_copy_assignable_v<ProductionOutputMetadataLease>);
		static_assert(std::is_move_constructible_v<ProductionOutputMetadataLease>);
		static_assert(std::is_move_assignable_v<ProductionOutputMetadataLease>);
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(reserve != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 2, 2, 0, 0, 0, 2, 0, 0 };
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
		plan.resources[1] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 48, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 2;
		plan.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		OutputMetadataProbe probe;
		probe.fixture = &fixture;
		REQUIRE(ordinal_initialize(fixture, server, plan, &OutputMetadataProbe::receive, &probe) == OK);
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		ProductionOutputMetadataLease expired;
		CHECK(fixture.registry.production_output_metadata_create({}, ref, 0, fixture.rd, 64, 8, expired) == ERR_UNAUTHORIZED);
		CHECK(expired.is_empty());
		auto record = [&]() {
			uint64_t control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 result{};
			REQUIRE(consume(ref, control, &result).native_error == OK);
			CHECK(result.report.action_count == 0);
			CHECK(result.report.physical_returns_pending == (probe.retry_returned ? 1 : 2));
		};
		record();
		REQUIRE(probe.calls == 1);
#ifdef THREADS_ENABLED
		struct ForeignMetadataReturn {
			Registry *registry = nullptr;
			ProductionOutputMetadataLease *lease = nullptr;
			Error error = OK;
			static void run(void *opaque) {
				auto &test = *static_cast<ForeignMetadataReturn *>(opaque);
				test.error = test.registry->production_output_metadata_return(*test.lease);
			}
		} foreign_return;
		foreign_return.registry = &fixture.registry;
		foreign_return.lease = &probe.first;
		Thread foreign_thread;
		foreign_thread.start(&ForeignMetadataReturn::run, &foreign_return);
		REQUIRE(foreign_thread.is_started());
		foreign_thread.wait_to_finish();
		CHECK(foreign_return.error == ERR_UNAUTHORIZED);
		CHECK_FALSE(probe.first.is_empty());
#endif
		CHECK(fixture.registry.production_output_metadata_create(probe.expired, ref, 0, fixture.rd, 64, 8, expired) == ERR_UNAUTHORIZED);
		CHECK(expired.is_empty());
		// A second accepted request has a genuine context in this same attachment,
		// but cannot create against the original request's metadata declaration.
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
			REQUIRE(other != &request);
			REQUIRE(other->attachment == request.attachment);
			REQUIRE(same_device == &fixture.rd);
		}
		uint64_t other_control = 0;
		REQUIRE(fixture.port.record_schedule(other_ref, &other_control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 other_record{};
		REQUIRE(consume(other_ref, other_control, &other_record).native_error == OK);
		CHECK(probe.foreign_calls == 1);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		GDExtensionProductionServiceBudgetV1 remaining{};
		GDExtensionProductionReportV2 released{};
		for (uint32_t turn = 0; turn < 8 && other->serial; ++turn) {
			const Error error = Error(release(other_ref, &budget, &remaining, &released).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (other->serial) {
				server->sync();
			}
		}
		REQUIRE(other->serial == 0);
		CHECK(other->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(request.attachment->children == 1);
		CHECK_FALSE(probe.first.is_empty());
		for (uint32_t i = 0; i < 48; ++i) {
			CHECK(static_cast<const uint8_t *>(probe.second.get_data())[i] == 0xa5);
		}
		const uint64_t second_return_revision = request.revision;
		REQUIRE(fixture.registry.production_output_metadata_return(probe.second) == OK);
		CHECK(request.revision == second_return_revision + 1);
		CHECK(probe.second.is_empty());
		CHECK(probe.second.get_data() == nullptr);
		CHECK(probe.second.get_size() == 0);
		CHECK(fixture.registry.production_output_metadata_return(probe.second) == ERR_DOES_NOT_EXIST);
		CHECK(request.revision == second_return_revision + 1);
		probe.retry_returned = true;
		record();
		CHECK(probe.calls == 2);
		CHECK_FALSE(probe.first.is_empty());
		// Original release/close cannot drop the remaining real allocation.
		CHECK(release(ref, &budget, &remaining, &released).native_error == ERR_BUSY);
		server->sync();
		CHECK(request.serial == ref.handle_serial);
		CHECK_FALSE(probe.first.is_empty());
		CHECK(fixture.registry.production_close(fixture.attachment).native_error == ERR_BUSY);
		CHECK(request.serial == ref.handle_serial);
		CHECK_FALSE(probe.first.is_empty());
		const uint64_t first_return_revision = request.revision;
		REQUIRE(fixture.registry.production_output_metadata_return(probe.first) == OK);
		CHECK(request.revision == first_return_revision + 1);
		CHECK(probe.first.is_empty());
		CHECK(fixture.registry.production_output_metadata_return(probe.first) == ERR_DOES_NOT_EXIST);
		CHECK(request.revision == first_return_revision + 1);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
			const Error error = Error(release(ref, &budget, &remaining, &released).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct MetadataBoundPinProbe {
		SingletonEnrollment *fixture = nullptr;
		GDExtensionDeviceExit::ProductionOutputMetadataLease metadata;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint32_t calls = 0;
		~MetadataBoundPinProbe() {
			if (!pin.is_empty()) {
				CHECK(fixture->registry.production_output_unpin(pin) == OK);
			}
			if (!metadata.is_empty()) {
				CHECK(fixture->registry.production_output_metadata_return(metadata) == OK);
			}
		}
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<MetadataBoundPinProbe *>(opaque);
			auto &fixture = *probe.fixture;
			auto &request = *fixture.request;
			++probe.calls;
			const uint64_t before_create = request.revision;
			REQUIRE(fixture.registry.production_output_metadata_create(context, ref, 0, fixture.rd, 32, 8, probe.metadata) == OK);
			CHECK(request.revision == before_create + 1);
			REQUIRE(fixture.registry.production_output_pin(context, ref, fixture.rd, probe.pin) == OK);
			REQUIRE(fixture.registry.production_output_metadata_bind(probe.metadata, probe.pin) == OK);
			// A real live pin protects its bound allocation even before native
			// owner installation links any generation locator into this request.
			CHECK(request.output_owner_head == UINT32_MAX);
			CHECK(request.output_owner_cursor == UINT32_MAX);
			CHECK(fixture.registry.production_output_pin_matches(probe.pin, ref, fixture.rd));
			void *const data = probe.metadata.get_data();
			const uint64_t before_return = request.revision;
			CHECK(fixture.registry.production_output_metadata_return(probe.metadata) == ERR_BUSY);
			CHECK(request.revision == before_return);
			CHECK(probe.metadata.get_data() == data);
			CHECK(probe.metadata.get_size() == 32);
			GDExtensionProductionReportV2 report{};
			REQUIRE(fixture.registry.production_report_contents_locked(request, report) == OK);
			CHECK(report.physical_returns_pending == 1);
			REQUIRE(fixture.registry.production_output_unpin(probe.pin) == OK);
			CHECK(probe.pin.is_empty());
			const uint64_t before_actual_return = request.revision;
			REQUIRE(fixture.registry.production_output_metadata_return(probe.metadata) == OK);
			CHECK(request.revision == before_actual_return + 1);
			CHECK(probe.metadata.is_empty());
			CHECK(fixture.registry.production_output_metadata_return(probe.metadata) == ERR_DOES_NOT_EXIST);
			CHECK(request.revision == before_actual_return + 1);
			ProductionOutputMetadataLease rejected;
			CHECK(fixture.registry.production_output_metadata_create(context, ref, 0, fixture.rd, 32, 8, rejected) == ERR_ALREADY_IN_USE);
			CHECK(rejected.is_empty());
			CHECK(request.revision == before_actual_return + 1);
			REQUIRE(fixture.registry.production_report_contents_locked(request, report) == OK);
			CHECK(report.physical_returns_pending == 0);
		}
	};
	static void output_metadata_bound_pin() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 32, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		MetadataBoundPinProbe probe;
		probe.fixture = &fixture;
		REQUIRE(ordinal_initialize(fixture, server, plan, &MetadataBoundPinProbe::receive, &probe) == OK);
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(probe.calls == 1);
		CHECK(recorded.report.physical_returns_pending == 0);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
	struct SceneMetadataProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		GDExtensionInterfaceDeviceSessionOutputCreateV2 create = nullptr;
		GDExtensionInterfaceDeviceSessionOutputWithdrawV1 withdraw = nullptr;
		GDExtensionOutputCreateResultV1 created{};
		GDExtensionDeviceExit::ProductionOutputMetadataLease unrelated;
		GDExtensionDeviceExit::ProductionOutputPin unrelated_pin;
		bool extra_metadata = false, bound_unlinked = false, use_v3 = false;
		int metadata_delta = 0;
		uint32_t calls = 0;
		~SceneMetadataProbe() {
			if (!unrelated_pin.is_empty()) {
				CHECK(fixture->registry.production_output_unpin(unrelated_pin) == OK);
			}
			if (!unrelated.is_empty()) {
				CHECK(fixture->registry.production_output_metadata_return(unrelated) == OK);
			}
		}
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<SceneMetadataProbe *>(opaque);
			auto &fixture = *probe.fixture;
			++probe.calls;
			GDExtensionOutputCreateInputV2 input{};
			input.abi = production_dto_abi<GDExtensionOutputCreateInputV2>();
			input.abi.abi_version = 2;
			input.config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
			input.config.profile = 3;
			input.config.max_views = input.config.max_inflight = 1;
			input.config.space = input.config.query_enabled = 1;
			for (uint32_t axis = 0; axis < 3; ++axis) {
				input.config.bounds.size[axis] = input.config.query_bounds.size[axis] = 1;
			}
			input.metadata_resource_slot = 0;
			if (probe.use_v3) {
				auto query = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>(gdextension_get_guarded_proc_address("device_session_output_metadata_query_v1"));
				auto create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV3>(gdextension_get_guarded_proc_address("device_session_output_create_v3"));
				REQUIRE(query != nullptr);
				REQUIRE(create != nullptr);
				GDExtensionOutputMetadataQueryInputV1 q{};
				q.abi = production_dto_abi<GDExtensionOutputMetadataQueryInputV1>();
				q.config = input.config;
				q.limits.abi = production_dto_abi<GDExtensionOutputMetadataLimitsV1>();
				GDExtensionOutputMetadataQueryResultV1 quote{};
				const uint64_t revision = fixture.request->revision;
				q.abi.abi_version = 2;
				query(&q, &quote);
				CHECK(quote.status.error == ERR_INVALID_PARAMETER);
				q.abi.abi_version = 1;
				q.limits.read_lease_capacity = UINT32_MAX;
				query(&q, &quote);
				CHECK(quote.status.error == ERR_INVALID_PARAMETER);
				q.limits.read_lease_capacity = 0;
				query(&q, &quote);
				REQUIRE(quote.status.error == OK);
				CHECK(quote.requirements.required_metadata_bytes == RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes());
				CHECK(quote.requirements.binding_count == 0);
				CHECK(quote.requirements.flattened_surface_count == 0);
				CHECK(fixture.request->revision == revision); // Query cannot allocate or pin.
				GDExtensionOutputCreateInputV3 v3{};
				v3.abi = { 3, sizeof(v3), 0 };
				v3.config = input.config;
				v3.requirements = quote.requirements;
				v3.metadata_resource_slot = 0;
				create({}, {}, &v3, &probe.created);
				CHECK(probe.created.status.error != OK);
				CHECK(probe.created.particles == 0);
				CHECK(fixture.request->revision == revision);
				v3.requirements.required_metadata_bytes = UINT64_MAX;
				create(context, ref, &v3, &probe.created);
				CHECK(probe.created.status.error == ERR_INVALID_PARAMETER);
				CHECK(fixture.request->revision == revision);
				v3.requirements = quote.requirements;
				v3.requirements.layout_version++;
				create(context, ref, &v3, &probe.created);
				CHECK(probe.created.status.error == ERR_INVALID_PARAMETER);
				CHECK(fixture.request->revision == revision);
				v3.requirements = quote.requirements;
				create(context, ref, &v3, &probe.created);
				if (probe.metadata_delta) {
					CHECK(probe.created.status.error == ERR_PARAMETER_RANGE_ERROR);
					CHECK(probe.created.particles == 0);
					CHECK(probe.created.generation == 0);
					CHECK(fixture.request->revision == revision);
					CHECK_FALSE(fixture.request->resources[0].metadata_created);
					CHECK(fixture.request->resources[0].metadata_data == nullptr);
					CHECK(fixture.request->output_owner_head == UINT32_MAX);
					CHECK_FALSE(fixture.request->native_effect_entered);
					return;
				}
			} else {
				probe.create(context, ref, &input, &probe.created);
			}
			REQUIRE(probe.created.status.error == OK);
			CHECK(probe.created.abi.abi_version == 1);
			CHECK(probe.created.abi.struct_size == sizeof(probe.created));
			REQUIRE(probe.created.particles != 0);
			REQUIRE(probe.created.generation != 0);
			CHECK(probe.created.capabilities == 0);
			REQUIRE(probe.native->adopt_created(RID::from_uint64(probe.created.particles)));
			CHECK(probe.native->exists());
			CHECK_FALSE(probe.native->released());
			CHECK_FALSE(probe.native->generation_pin().is_empty());
			CHECK(fixture.registry.production_output_pin_matches(probe.native->generation_pin(), ref, fixture.rd));
			CHECK(fixture.request->output_owner_head != UINT32_MAX);
			if (probe.extra_metadata) {
				const uint64_t revision = fixture.request->revision;
				REQUIRE(fixture.registry.production_output_metadata_create(context, ref, 1, fixture.rd, 32, 8, probe.unrelated) == OK);
				CHECK(fixture.request->revision == revision + 1);
				if (probe.bound_unlinked) {
					REQUIRE(fixture.registry.production_output_pin(context, ref, fixture.rd, probe.unrelated_pin) == OK);
					REQUIRE(fixture.registry.production_output_metadata_bind(probe.unrelated, probe.unrelated_pin) == OK);
					CHECK(fixture.request->resources[1].metadata_root_serial != 0);
				}
				CHECK_FALSE(fixture.request->resources[1].metadata_generation_bound);
			}
			GDExtensionOutputGenerationInputV1 generation{};
			generation.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
			generation.particles = probe.created.particles;
			generation.generation = probe.created.generation;
			REQUIRE(probe.withdraw(context, ref, &generation).error == OK);
			CHECK(probe.withdraw(context, ref, &generation).error == OK);
			CHECK(probe.native->withdrawing());
		}
	};
	static void output_scene_metadata(bool extra_metadata, bool bound_unlinked = false, bool use_v3 = false, int metadata_delta = 0) {
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
		const uint32_t metadata_rows = extra_metadata ? 2u : 1u;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, metadata_rows, metadata_rows, 0, 0, 0, metadata_rows, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = extra_metadata ? 2 : 1;
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, uint64_t(int64_t(RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes()) + metadata_delta), 0 };
		plan.resources[1] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 32, 0 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		SceneMetadataProbe probe;
		probe.fixture = &fixture;
		probe.extra_metadata = extra_metadata;
		probe.bound_unlinked = bound_unlinked;
		probe.use_v3 = use_v3;
		probe.metadata_delta = metadata_delta;
		probe.create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV2>(gdextension_get_guarded_proc_address("device_session_output_create_v2"));
		probe.withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
		REQUIRE(probe.create != nullptr);
		REQUIRE(probe.withdraw != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &SceneMetadataProbe::receive, &probe) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t record_control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &record_control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, record_control, &recorded).native_error == OK);
		REQUIRE(probe.calls == 1);
		if (use_v3 && metadata_delta) {
			CHECK(recorded.report.physical_returns_pending == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
			CHECK_FALSE(native.exists());
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
			return;
		}
		CHECK(recorded.report.physical_returns_pending == (extra_metadata ? 2u : 1u));
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		CHECK(request.resources[0].metadata_generation_bound);
		const uint64_t root_serial = native.generation_serial();
		REQUIRE(root_serial != 0);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		auto dispatch = [&]() {
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(request.control.state == ProductionControlState::RETURNED);
			CHECK(request.capture_inflight == 0);
			return serial;
		};
		if (extra_metadata) {
			const uint32_t head = request.output_owner_head;
			for (uint32_t turn = 0; turn < 2; ++turn) {
				const uint64_t serial = dispatch();
				CHECK(request.control.output.result.native_error == ERR_BUSY);
				CHECK_FALSE(request.control.output.output_generation.native_released);
				CHECK_FALSE(native.generation_pin().is_empty());
				CHECK_FALSE(native.released());
				CHECK(request.output_owner_head == head);
				ProductionControlOutput blocked{};
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &blocked) == OK);
			}
			if (bound_unlinked) {
				CHECK(fixture.registry.production_output_metadata_return(probe.unrelated) == ERR_BUSY);
				REQUIRE(fixture.registry.production_output_unpin(probe.unrelated_pin) == OK);
				CHECK(request.resources[1].metadata_root_serial != 0);
				CHECK_FALSE(request.resources[1].metadata_generation_bound);
				// Removing the uninstalled pin does not turn its allocation into
				// generation-owned metadata. Actual return is still required.
				const uint64_t serial = dispatch();
				CHECK(request.control.output.result.native_error == ERR_BUSY);
				CHECK_FALSE(request.control.output.output_generation.native_released);
				ProductionControlOutput blocked{};
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &blocked) == OK);
			}
			const uint64_t revision = request.revision;
			REQUIRE(fixture.registry.production_output_metadata_return(probe.unrelated) == OK);
			CHECK(request.revision == revision + 1);
		}
		const uint64_t terminal_control = dispatch();
		REQUIRE(request.control.output.output_generation.native_released);
		CHECK(request.control.output.result.native_error == OK);
		CHECK(request.control.output.service_usage.controls == 1);
		CHECK(request.control.output.service_usage.passes == 1);
		CHECK(request.control.output.service_usage.receipts == 0);
		CHECK(native.released());
		CHECK(native.exists());
		CHECK(native.generation_pin().is_empty());
		CHECK(request.output_owner_head == UINT32_MAX);
		CHECK(request.output_owner_cursor == UINT32_MAX);
		const auto terminal = request.control.output.output_generation;
		CHECK(terminal.root_serial == root_serial);
		CHECK(terminal.particles == probe.created.particles);
		CHECK(terminal.generation == probe.created.generation);
		CHECK(terminal.root_source.physical_returns_pending == 1);
		CHECK(terminal.root_source.known_obligations == 0);
		CHECK(terminal.root_source.possible_obligations == 0);
		CHECK(terminal.root_source.complete_obligations == 0);
		ProductionControlOutput consumed{};
		REQUIRE(fixture.registry.production_control_consume(ref, terminal_control, &consumed) == OK);
		CHECK(std::memcmp(&terminal, &consumed.output_generation, sizeof(terminal)) == 0);
		CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
		CHECK(fixture.registry.production_close(fixture.attachment).native_error == ERR_BUSY);
		GDExtensionProductionReportV2 live{};
		REQUIRE(fixture.registry.production_report_contents_locked(request, live) == OK);
		CHECK(live.physical_returns_pending == 1);
		const uint64_t before_free = request.revision;
		native.free_created(); // Actual particles_free destroys placement owner, then returns its allocation.
		CHECK_FALSE(native.exists());
		CHECK(request.revision == before_free + 1);
		REQUIRE(fixture.registry.production_report_contents_locked(request, live) == OK);
		CHECK(live.physical_returns_pending == 0);
		CHECK(std::memcmp(&terminal, &consumed.output_generation, sizeof(terminal)) == 0);
		CHECK(consumed.output_generation.root_source.physical_returns_pending == 1);
		CHECK(fixture.driver.issues == 0);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
	struct TwoSceneMetadataProbe {
		GDExtensionInterfaceDeviceSessionOutputCreateV2 create = nullptr;
		GDExtensionInterfaceDeviceSessionOutputWithdrawV1 withdraw = nullptr;
		GDExtensionOutputCreateResultV1 created[2]{};
		uint32_t calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<TwoSceneMetadataProbe *>(opaque);
			++probe.calls;
			GDExtensionOutputCreateInputV2 input{};
			input.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputCreateInputV2>();
			input.abi.abi_version = 2;
			input.config.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputCreateInputV1>();
			input.config.profile = 3;
			input.config.max_views = input.config.max_inflight = 1;
			input.config.space = input.config.query_enabled = 1;
			for (uint32_t axis = 0; axis < 3; ++axis) {
				input.config.bounds.size[axis] = input.config.query_bounds.size[axis] = 1;
			}
			for (uint32_t i = 0; i < 2; ++i) {
				input.metadata_resource_slot = i;
				probe.create(context, ref, &input, &probe.created[i]);
				REQUIRE(probe.created[i].status.error == OK);
				REQUIRE(probe.created[i].particles != 0);
				REQUIRE(probe.created[i].generation != 0);
			}
			CHECK(probe.created[0].particles != probe.created[1].particles);
			CHECK(probe.created[0].generation != probe.created[1].generation);
			for (uint32_t i = 0; i < 2; ++i) {
				GDExtensionOutputGenerationInputV1 generation{};
				generation.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputGenerationInputV1>();
				generation.particles = probe.created[i].particles;
				generation.generation = probe.created[i].generation;
				REQUIRE(probe.withdraw(context, ref, &generation).error == OK);
			}
		}
	};
	static void output_scene_metadata_two_generations() {
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
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 2, 2, 0, 0, 0, 2, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 2;
		for (uint32_t i = 0; i < 2; ++i) {
			plan.resources[i] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes(), 0 };
		}
		SingletonEnrollment fixture;
		fixture.public_port = true;
		TwoSceneMetadataProbe probe;
		probe.create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV2>(gdextension_get_guarded_proc_address("device_session_output_create_v2"));
		probe.withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
		REQUIRE(probe.create != nullptr);
		REQUIRE(probe.withdraw != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &TwoSceneMetadataProbe::receive, &probe) == OK);
		// One real storage instance, two actual CreateV2 results. No fabricated roots.
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		auto *storage = RendererRD::ParticlesStorage::get_singleton();
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t record_control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &record_control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, record_control, &recorded).native_error == OK);
		REQUIRE(probe.calls == 1);
		CHECK(recorded.report.physical_returns_pending == 2);
		CHECK(request.action_count == 0);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		REQUIRE(request.resources[0].metadata_root_serial != 0);
		REQUIRE(request.resources[1].metadata_root_serial != 0);
		CHECK(request.resources[0].metadata_root_serial != request.resources[1].metadata_root_serial);
		CHECK(request.resources[0].metadata_generation_bound);
		CHECK(request.resources[1].metadata_generation_bound);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		bool returned[2] = {};
		for (uint32_t turn = 0; turn < 2; ++turn) {
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(request.control.state == ProductionControlState::RETURNED);
			ProductionControlOutput output{};
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK(output.result.native_error == OK);
			// Neither owner can wait for the other owner's metadata to be freed.
			// On the second turn the first root pin is already gone, but its RID
			// and real metadata allocation deliberately remain alive.
			REQUIRE_MESSAGE(output.output_generation.native_released, "Two genuine SceneOnly roots must terminal independently before either particles_free");
			const auto &terminal = output.output_generation;
			const uint32_t index = terminal.particles == probe.created[0].particles ? 0 : 1;
			CHECK(terminal.particles == probe.created[index].particles);
			CHECK(terminal.generation == probe.created[index].generation);
			CHECK(terminal.root_serial == request.resources[index].metadata_root_serial);
			CHECK_FALSE(returned[index]);
			returned[index] = true;
			CHECK(terminal.root_source.physical_returns_pending == 2);
			CHECK(output.service_usage.controls == 1);
			CHECK(output.service_usage.passes == 1);
			CHECK(output.service_usage.receipts == 0);
			CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
			CHECK(storage->owns_particles(RID::from_uint64(probe.created[0].particles)));
			CHECK(storage->owns_particles(RID::from_uint64(probe.created[1].particles)));
		}
		CHECK(returned[0]);
		CHECK(returned[1]);
		CHECK(request.output_owner_head == UINT32_MAX);
		CHECK(request.output_owner_cursor == UINT32_MAX);
		for (uint32_t i = 0; i < 2; ++i) {
			const RID rid = RID::from_uint64(probe.created[i].particles);
			storage->particles_free(rid);
			CHECK_FALSE(storage->owns_particles(rid));
			CHECK(request.resources[i].metadata_returned);
			CHECK(request.resources[i].metadata_data == nullptr);
			GDExtensionProductionReportV2 live{};
			REQUIRE(fixture.registry.production_report_contents_locked(request, live) == OK);
			CHECK(live.physical_returns_pending == 1 - i);
			if (i == 0) {
				CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
			}
		}
		CHECK(fixture.driver.issues == 0);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
	struct GenerationSelectionProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		RendererRD::ExternalOutputConfig config, expected_identity;
		uint64_t expected_material_generations[2] = {};
		int failure = 0;
		uint32_t calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &p = *static_cast<GenerationSelectionProbe *>(opaque);
			++p.calls;
			uint64_t bytes = 0;
			uint32_t alignment = 0;
			REQUIRE(RendererRD::ExternalLinkedOwnerTest::selection_metadata_bytes(p.config, bytes, &alignment) == OK);
			GDExtensionDeviceExit::ProductionOutputMetadataLease metadata;
			REQUIRE(p.fixture->registry.production_output_metadata_create(context, ref, 0, p.fixture->rd, bytes, alignment, metadata, true) == OK);
			if (p.failure == 3) {
				const uint64_t revision = p.fixture->request->revision;
				p.native->unprepared_guard_refusal(context, ref, p.config, metadata);
				CHECK(p.fixture->request->revision == revision);
				CHECK(p.fixture->request->output_owner_head == UINT32_MAX);
				CHECK(p.fixture->request->resources[0].metadata_root_serial == 0);
				CHECK_FALSE(p.fixture->request->resources[0].metadata_generation_bound);
				CHECK_FALSE(metadata.is_empty());
				// Public profile refusal remains separate from internal guard status.
				uint64_t meshes[] = { p.config.surfaces[0].get_id() };
				uint64_t materials[] = { p.config.surface_materials[0].get_id(), p.config.surface_materials[1].get_id() };
				GDExtensionOutputMetadataQueryInputV1 input{};
				input.abi = { 1, sizeof(input), 0 };
				input.limits.abi = { 1, sizeof(input.limits), 0 };
				input.config.abi = { 1, sizeof(input.config), 0 };
				input.config.profile = 1;
				input.config.space = 1;
				input.config.max_views = input.config.max_inflight = 1;
				input.config.surfaces.data = meshes;
				input.config.surfaces.count = 1;
				input.config.surface_materials.data = materials;
				input.config.surface_materials.count = 2;
				for (uint32_t axis = 0; axis < 3; ++axis) {
					input.config.bounds.size[axis] = 1;
				}
				auto query = ordinal_port<GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>("device_session_output_metadata_query_v1");
				auto create = ordinal_port<GDExtensionInterfaceDeviceSessionOutputCreateV3>("device_session_output_create_v3");
				REQUIRE(query != nullptr);
				REQUIRE(create != nullptr);
				GDExtensionOutputMetadataQueryResultV1 quoted{};
				query(&input, &quoted);
				CHECK(quoted.status.error == ERR_UNAVAILABLE);
				CHECK(quoted.requirements.required_metadata_bytes == 0);
				GDExtensionOutputCreateInputV3 v3{};
				v3.abi = { 3, sizeof(v3), 0 };
				v3.config = input.config;
				v3.metadata_resource_slot = 0;
				v3.requirements.abi = { 1, sizeof(v3.requirements), 0 };
				v3.requirements.limits = input.limits;
				GDExtensionOutputCreateResultV1 result{};
				create(context, ref, &v3, &result);
				CHECK(result.status.error == ERR_UNAVAILABLE);
				CHECK(result.particles == 0);
				CHECK(result.generation == 0);
				CHECK(p.fixture->request->revision == revision);
				REQUIRE(p.fixture->registry.production_output_metadata_return(metadata) == OK);
				CHECK(metadata.is_empty());
				CHECK(p.fixture->request->revision == revision + 1);
				return;
			}
			const Error result = p.native->create_selection_component(context, ref, p.config, metadata, p.failure);
			CHECK(metadata.is_empty());
			CHECK(result == (p.failure == 0 ? OK : p.failure == 1 ? ERR_INVALID_DATA
																  : ERR_UNAUTHORIZED));
			REQUIRE(p.native->exists());
			CHECK(p.native->selection_inline_matches(p.expected_identity));
			for (uint32_t i = 0; i < 2; ++i) {
				CHECK(p.native->selection_material_generation(i) == p.expected_material_generations[i]);
			}
			// Independent expected bytes have their own allocations. Each changed
			// input tag must disagree without changing the held original identity.
			PackedByteArray *expected_tags[] = { &p.expected_identity.producer_tag, &p.expected_identity.device_tag, &p.expected_identity.layout_digest, &p.expected_identity.mapping_digest, &p.expected_identity.numeric_profile_digest };
			for (auto *tag : expected_tags) {
				const uint8_t original = (*tag)[17];
				tag->set(17, original ^ 0xff);
				CHECK_FALSE(p.native->selection_inline_matches(p.expected_identity));
				tag->set(17, original);
				CHECK(p.native->selection_inline_matches(p.expected_identity));
			}
			CHECK(p.native->selection_meshes() == (p.failure == 2 ? 0u : 1u));
			CHECK(p.native->selection_materials() == (p.failure == 0 ? 2u : p.failure == 1 ? 1u
																						   : 0u));
			CHECK(p.native->selection_faulted() == (p.failure != 0));
			CHECK_FALSE(p.native->generation_pin().is_empty());
			if (p.failure != 2) {
				auto *meshes = RendererRD::MeshStorage::get_singleton();
				auto *materials = RendererRD::MaterialStorage::get_singleton();
				const RID mesh = p.config.surfaces[0], material = p.config.surface_materials[0];
				const uint64_t before = materials->material_get_generation(material);
				ERR_PRINT_OFF;
				meshes->mesh_clear(mesh);
				meshes->mesh_free(mesh);
				materials->material_set_render_priority(material, 7);
				materials->material_free(material);
				ERR_PRINT_ON;
				CHECK(meshes->owns_mesh(mesh));
				CHECK(meshes->mesh_get_surface_count(mesh) == 2);
				CHECK(materials->owns_material(material));
				CHECK(materials->material_get_generation(material) == before);
			}
			if (!p.failure) {
				p.native->withdraw_generation();
			}
		}
	};
	static void generation_selection_lifecycle(int failure) {
		using namespace GDExtensionDeviceExit;
		using MeshStorage = RendererRD::MeshStorage;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		SingletonEnrollment fixture;
		fixture.public_port = true;
		REQUIRE(fixture.prepare_device(server) == OK);
		auto *previous_meshes = MeshStorage::get_singleton();
		auto *meshes = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID mesh = meshes->mesh_allocate();
		meshes->mesh_initialize(mesh);
		auto *mesh_owner = meshes->mesh_owner.get_or_null(mesh);
		// Original CPU mesh/surface owners, deliberately no VA or physical profile.
		mesh_owner->surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(2 * sizeof(MeshStorage::Mesh::Surface *)));
		mesh_owner->surface_count = 2;
		for (uint32_t i = 0; i < 2; ++i) {
			mesh_owner->surfaces[i] = memnew(MeshStorage::Mesh::Surface);
		}
		void *materials = RendererRD::selection_materials_create();
		GenerationSelectionProbe probe;
		probe.failure = failure;
		probe.config.profile = RendererRD::ExternalOutputConfig::INSTANCE_192;
		probe.config.max_views = probe.config.max_inflight = 1;
		probe.config.surfaces.push_back(mesh);
		for (uint32_t i = 0; i < 2; ++i) {
			probe.config.surface_materials.push_back(RendererRD::selection_material_rid(materials, i));
		}
		PackedByteArray *tags[] = { &probe.config.producer_tag, &probe.config.device_tag, &probe.config.layout_digest, &probe.config.mapping_digest, &probe.config.numeric_profile_digest };
		for (uint32_t i = 0; i < 5; ++i) {
			tags[i]->resize(32);
			tags[i]->fill(i + 1);
		}
		// Give the second original material a distinct genuine generation before
		// snapshotting; an index swap or constant generation cannot pass.
		RendererRD::MaterialStorage::get_singleton()->material_set_render_priority(probe.config.surface_materials[1], 4);
		for (const RID &rid : probe.config.surfaces) {
			probe.expected_identity.surfaces.push_back(rid);
		}
		for (uint32_t i = 0; i < 2; ++i) {
			const RID rid = probe.config.surface_materials[i];
			probe.expected_identity.surface_materials.push_back(rid);
			probe.expected_material_generations[i] = RendererRD::MaterialStorage::get_singleton()->material_get_generation(rid);
			REQUIRE(probe.expected_material_generations[i] != 0);
		}
		REQUIRE(probe.expected_material_generations[0] != probe.expected_material_generations[1]);
		PackedByteArray *expected_tags[] = { &probe.expected_identity.producer_tag, &probe.expected_identity.device_tag, &probe.expected_identity.layout_digest, &probe.expected_identity.mapping_digest, &probe.expected_identity.numeric_profile_digest };
		for (uint32_t i = 0; i < 5; ++i) {
			expected_tags[i]->resize(32);
			memcpy(expected_tags[i]->ptrw(), tags[i]->ptr(), 32);
		}
		uint64_t bytes = 0;
		REQUIRE(RendererRD::ExternalLinkedOwnerTest::selection_metadata_bytes(probe.config, bytes) == OK);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, bytes, 0 };
		probe.fixture = &fixture;
		REQUIRE(fixture.initialize_owner(1, false, false, &GenerationSelectionProbe::receive, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan) == OK);
		{
			RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
			probe.native = &native;
			auto &request = *fixture.request;
			const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
			uint64_t serial = 0;
			REQUIRE(fixture.port.record_schedule(ref, &serial) == OK);
			server->sync();
			const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
			GDExtensionProductionRecordReportV2 recorded{};
			REQUIRE(consume(ref, serial, &recorded).native_error == OK);
			REQUIRE(probe.calls == 1);
			CHECK(recorded.report.physical_returns_pending == (failure == 3 ? 0u : 1u));
			if (failure == 3) {
				CHECK_FALSE(native.exists());
				CHECK(request.resources[0].metadata_returned);
				CHECK(request.resources[0].metadata_data == nullptr);
			}
			CHECK(mesh_owner->selection_holds == (failure >= 2 ? 0u : 1u));
			CHECK(RendererRD::selection_material_holds(materials, 0) == (failure >= 2 ? 0u : 1u));
			CHECK(RendererRD::selection_material_holds(materials, 1) == (failure == 0 ? 1u : 0u));
			const uint32_t initial_leases = failure == 0 ? 3 : failure == 1 ? 2
																			: 0;
			// Destroy parser bytes before dropping all references. Expected identity
			// was independently allocated and cannot keep parser storage alive.
			for (auto *tag : tags) {
				tag->fill(0xa5);
			}
			probe.config.surfaces.clear();
			probe.config.surface_materials.clear();
			for (auto *tag : tags) {
				*tag = PackedByteArray();
			}
			if (failure != 3) {
				CHECK(native.selection_inline_matches(probe.expected_identity));
				for (uint32_t i = 0; i < 2; ++i) {
					CHECK(native.selection_material_generation(i) == probe.expected_material_generations[i]);
				}
				CHECK(mesh_owner->selection_holds == (failure == 2 ? 0u : 1u));
				CHECK(RendererRD::selection_material_holds(materials, 0) == (failure == 2 ? 0u : 1u));
				CHECK(RendererRD::selection_material_holds(materials, 1) == (failure == 0 ? 1u : 0u));
			}
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			for (uint32_t turn = 0; turn < (failure == 3 ? 0u : initial_leases + 2); ++turn) {
				REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
				REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
				server->sync();
				ProductionControlOutput out{};
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &out) == OK);
				CHECK(out.result.native_error == OK);
				CHECK(out.service_usage.passes == 1);
				CHECK(out.service_usage.controls == 1);
				CHECK(out.service_usage.receipts == 0);
				if (turn < initial_leases) {
					CHECK_FALSE(out.output_generation.native_released);
					REQUIRE(native.exists());
					CHECK(native.selection_meshes() + native.selection_materials() == initial_leases - turn - 1);
					CHECK(RendererRD::selection_shader_holds(materials) == native.selection_materials());
					const uint32_t initially_retained_materials = failure == 0 ? 2u : 1u;
					const uint32_t remaining_materials = turn + 1 < initially_retained_materials ? initially_retained_materials - turn - 1 : 0;
					// Exact original RID order: material1 returns before material0.
					for (uint32_t i = 0; i < 2; ++i) {
						CHECK(RendererRD::selection_material_holds(materials, i) == (i < remaining_materials ? 1u : 0u));
					}
					CHECK(mesh_owner->selection_holds == native.selection_meshes());
					CHECK_FALSE(request.resources[0].metadata_returned);
				} else if (turn == initial_leases) {
					CHECK_FALSE(out.output_generation.native_released);
					REQUIRE(native.exists());
					CHECK(native.generation_directory_empty());
				} else {
					CHECK(out.output_generation.native_released);
					CHECK(request.output_owner_head == UINT32_MAX);
					if (failure) {
						CHECK_FALSE(native.exists());
						CHECK(request.resources[0].metadata_returned);
						CHECK(request.resources[0].metadata_data == nullptr);
						native.forget_auto_returned();
					} else {
						REQUIRE(native.exists());
						CHECK(native.released());
						CHECK_FALSE(request.resources[0].metadata_returned);
						native.free_created();
						CHECK(request.resources[0].metadata_returned);
					}
				}
			}
			CHECK(mesh_owner->selection_holds == 0);
			CHECK(RendererRD::selection_shader_holds(materials) == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
			CHECK(fixture.driver.issues == 0);
			const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
			for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
		RendererRD::selection_materials_destroy(materials);
		meshes->mesh_free(mesh);
		memdelete(meshes);
		MeshStorage::singleton = previous_meshes;
	}
	struct SceneMetadataDirectoryProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		GDExtensionInterfaceDeviceSessionOutputWithdrawV1 withdraw = nullptr;
		GDExtensionOutputCreateResultV1 created{};
		bool short_layout = false;
		uint32_t calls = 0;
		static constexpr uint32_t views = 2, reads = 3, surfaces = 4;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<SceneMetadataDirectoryProbe *>(opaque);
			auto &fixture = *probe.fixture;
			auto &request = *fixture.request;
			++probe.calls;
			GDExtensionOutputCreateInputV1 config{};
			config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
			config.profile = 3;
			config.max_views = config.max_inflight = 1;
			config.space = config.query_enabled = 1;
			for (uint32_t axis = 0; axis < 3; ++axis) {
				config.bounds.size[axis] = config.query_bounds.size[axis] = 1;
			}
			const uint64_t revision = request.revision;
			const auto &resource = request.resources[0];
			CHECK_FALSE(resource.metadata_created);
			CHECK(resource.metadata_data == nullptr);
			CHECK(resource.metadata_consumer == nullptr);
			// Explicit internal counts enter the complete original typed preflight,
			// factory and receiver. Public V2 retains its zero-count wire contract.
			probe.native->create_with_metadata_counts(context, ref, config, 0, views, reads, surfaces, probe.created);
			if (probe.short_layout) {
				CHECK(probe.created.status.error == ERR_PARAMETER_RANGE_ERROR);
				CHECK(probe.created.particles == 0);
				CHECK(probe.created.generation == 0);
				CHECK(probe.created.capabilities == 0);
				CHECK(request.revision == revision);
				CHECK_FALSE(resource.metadata_created);
				CHECK_FALSE(resource.metadata_returned);
				CHECK(resource.metadata_data == nullptr);
				CHECK(resource.metadata_consumer == nullptr);
				CHECK(resource.metadata_root_serial == 0);
				CHECK(request.output_owner_head == UINT32_MAX);
				return;
			}
			REQUIRE(probe.created.status.error == OK);
			CHECK(probe.created.capabilities == 0);
			REQUIRE(probe.created.particles != 0);
			REQUIRE(probe.created.generation != 0);
			REQUIRE(probe.native->adopt_created(RID::from_uint64(probe.created.particles)));
			CHECK(probe.native->metadata_directory_in_slab(views, reads, surfaces));
			CHECK_FALSE(probe.native->generation_directory_empty());
			CHECK(resource.metadata_created);
			CHECK_FALSE(resource.metadata_returned);
			CHECK(resource.metadata_data != nullptr);
			CHECK(resource.metadata_consumer != nullptr);
			CHECK(resource.metadata_root_serial == probe.native->generation_serial());
			GDExtensionOutputGenerationInputV1 generation{};
			generation.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
			generation.particles = probe.created.particles;
			generation.generation = probe.created.generation;
			REQUIRE(probe.withdraw(context, ref, &generation).error == OK);
			CHECK(probe.native->withdrawing());
		}
	};
	static void output_scene_metadata_directory(bool short_layout) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		uint64_t full_bytes = 0, owner_bytes = 0;
		REQUIRE(RendererRD::ExternalLinkedOwnerTest::metadata_directory_bytes(SceneMetadataDirectoryProbe::views, SceneMetadataDirectoryProbe::reads, SceneMetadataDirectoryProbe::surfaces, full_bytes) == OK);
		REQUIRE(RendererRD::ExternalLinkedOwnerTest::metadata_directory_bytes(0, 0, 0, owner_bytes) == OK);
		REQUIRE(full_bytes > owner_bytes);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, short_layout ? owner_bytes : full_bytes, 0 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		SceneMetadataDirectoryProbe probe;
		probe.fixture = &fixture;
		probe.short_layout = short_layout;
		probe.withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
		REQUIRE(probe.withdraw != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &SceneMetadataDirectoryProbe::receive, &probe) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t record_control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &record_control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, record_control, &recorded).native_error == OK);
		CHECK(probe.calls == 1);
		CHECK(recorded.report.physical_returns_pending == (short_layout ? 0u : 1u));
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		if (!short_layout) {
			void *const slab = request.resources[0].metadata_data;
			const auto *const lease = request.resources[0].metadata_consumer;
			const uint64_t root_serial = native.generation_serial();
			auto dispatch = [&]() {
				uint64_t serial = 0;
				REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
				REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
				server->sync();
				REQUIRE(request.control.state == ProductionControlState::RETURNED);
				ProductionControlOutput output{};
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
				return output;
			};
			const auto directory = dispatch();
			CHECK(directory.result.native_error == OK);
			CHECK(directory.service_usage.controls == 1);
			CHECK(directory.service_usage.passes == 1);
			CHECK(directory.service_usage.receipts == 0);
			CHECK_FALSE(directory.output_generation.native_released);
			CHECK(native.generation_directory_empty());
			CHECK_FALSE(native.generation_pin().is_empty());
			CHECK_FALSE(native.released());
			CHECK(native.exists());
			CHECK(request.resources[0].metadata_data == slab);
			CHECK(request.resources[0].metadata_consumer == lease);
			CHECK_FALSE(request.resources[0].metadata_returned);
			GDExtensionProductionReportV2 current{};
			REQUIRE(fixture.registry.production_report_contents_locked(request, current) == OK);
			CHECK(current.physical_returns_pending == 1);
			const auto terminal = dispatch();
			REQUIRE(terminal.output_generation.native_released);
			CHECK(terminal.result.native_error == OK);
			CHECK(terminal.service_usage.passes == 1);
			CHECK(terminal.service_usage.receipts == 0);
			CHECK(terminal.output_generation.root_serial == root_serial);
			CHECK(terminal.output_generation.particles == probe.created.particles);
			CHECK(terminal.output_generation.generation == probe.created.generation);
			CHECK(terminal.output_generation.root_source.physical_returns_pending == 1);
			CHECK(native.released());
			CHECK(native.exists());
			CHECK(native.generation_pin().is_empty());
			CHECK(request.resources[0].metadata_data == slab);
			CHECK(request.resources[0].metadata_consumer == lease);
			CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
			CHECK(fixture.registry.production_close(fixture.attachment).native_error == ERR_BUSY);
			const auto frozen = terminal.output_generation;
			const uint64_t revision = request.revision;
			native.free_created();
			CHECK_FALSE(native.exists());
			CHECK(request.revision == revision + 1);
			REQUIRE(fixture.registry.production_report_contents_locked(request, current) == OK);
			CHECK(current.physical_returns_pending == 0);
			CHECK(request.resources[0].metadata_data == nullptr);
			CHECK(request.resources[0].metadata_consumer == nullptr);
			CHECK(request.resources[0].metadata_returned);
			CHECK(std::memcmp(&frozen, &terminal.output_generation, sizeof(frozen)) == 0);
			CHECK(std::memcmp(&frozen, &request.control.output.output_generation, sizeof(frozen)) == 0);
		} else {
			CHECK_FALSE(native.exists());
		}
		CHECK(fixture.driver.issues == 0);
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
	struct SceneMetadataRefundProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		GDExtensionInterfaceDeviceSessionOutputCreateV2 create = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin occupied;
		bool directories = false;
		uint32_t calls = 0;
		~SceneMetadataRefundProbe() {
			if (!occupied.is_empty()) {
				CHECK(fixture->registry.production_output_unpin(occupied) == OK);
			}
		}
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<SceneMetadataRefundProbe *>(opaque);
			auto &fixture = *probe.fixture;
			auto &request = *fixture.request;
			++probe.calls;
			REQUIRE(request.capacity.resource_uses == 1);
			REQUIRE(fixture.registry.production_output_pin(context, ref, fixture.rd, probe.occupied) == OK);
			CHECK(request.output_owner_head == UINT32_MAX);
			GDExtensionOutputCreateInputV2 input{};
			input.abi = production_dto_abi<GDExtensionOutputCreateInputV2>();
			input.abi.abi_version = 2;
			input.config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
			input.config.profile = 3;
			input.config.max_views = input.config.max_inflight = 1;
			input.config.space = input.config.query_enabled = 1;
			for (uint32_t axis = 0; axis < 3; ++axis) {
				input.config.bounds.size[axis] = input.config.query_bounds.size[axis] = 1;
			}
			input.metadata_resource_slot = 0;
			GDExtensionOutputCreateResultV1 created{};
			const uint64_t revision = request.revision;
			// Metadata allocation succeeds first. The original finite pin pool
			// then refuses root installation, so the real Create rollback must
			// destroy its placement owner/RID and explicitly return that block.
			if (probe.directories) {
				probe.native->create_with_metadata_counts(context, ref, input.config, input.metadata_resource_slot, 2, 3, 4, created);
			} else {
				probe.create(context, ref, &input, &created);
			}
			CHECK(created.status.error == ERR_OUT_OF_MEMORY);
			CHECK(created.particles == 0);
			CHECK(created.generation == 0);
			CHECK(created.capabilities == 0);
			CHECK(request.revision == revision + 2); // Actual create plus actual return.
			CHECK(request.resources[0].metadata_created);
			CHECK(request.resources[0].metadata_returned);
			CHECK(request.resources[0].metadata_data == nullptr);
			CHECK(request.resources[0].metadata_consumer == nullptr);
			CHECK(request.resources[0].metadata_root_serial == 0);
			CHECK_FALSE(probe.occupied.is_empty());
			CHECK(fixture.registry.production_output_pin_matches(probe.occupied, ref, fixture.rd));
			CHECK(request.output_owner_head == UINT32_MAX);
			CHECK(request.output_owner_cursor == UINT32_MAX);
			GDExtensionProductionReportV2 report{};
			REQUIRE(fixture.registry.production_report_contents_locked(request, report) == OK);
			CHECK(report.physical_returns_pending == 0);
			CHECK(report.known_obligations == 0);
			CHECK(report.possible_obligations == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
		}
	};
	static void output_scene_metadata_refund(bool directories = false) {
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
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		uint64_t metadata_bytes = RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes();
		if (directories) {
			REQUIRE(RendererRD::ExternalLinkedOwnerTest::metadata_directory_bytes(2, 3, 4, metadata_bytes) == OK);
		}
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, metadata_bytes, 0 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		SceneMetadataRefundProbe probe;
		probe.fixture = &fixture;
		probe.directories = directories;
		probe.create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV2>(gdextension_get_guarded_proc_address("device_session_output_create_v2"));
		REQUIRE(probe.create != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &SceneMetadataRefundProbe::receive, &probe) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.native = &native;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(probe.calls == 1);
		CHECK(recorded.report.physical_returns_pending == 0);
		CHECK_FALSE(native.exists());
		CHECK_FALSE(probe.occupied.is_empty());
		CHECK(fixture.registry.production_release(ref).native_error == ERR_BUSY);
		REQUIRE(fixture.registry.production_output_unpin(probe.occupied) == OK);
		CHECK(probe.occupied.is_empty());
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		for (uint32_t turn = 0; turn < 8 && request.serial; ++turn) {
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
		// The empty-storage bridge destructor independently rejects any actual
		// RID left behind by failed Create; it never fabricates owner cleanup.
	}
	static void output_metadata_wrong_kind() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 16, 0, 1, 0, 0 };
		plan.resources[0] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		struct Probe {
			SingletonEnrollment *fixture = nullptr;
			uint32_t calls = 0;
			static void receive(void *opaque, ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
				auto &test = *static_cast<Probe *>(opaque);
				++test.calls;
				ProductionOutputMetadataLease rejected;
				CHECK(test.fixture->request->frozen);
				CHECK(test.fixture->request->resources[0].imported);
				CHECK(test.fixture->registry.production_output_metadata_create(context, ref, 0, test.fixture->rd, 16, 8, rejected) == ERR_INVALID_DATA);
				CHECK(rejected.is_empty());
			}
		} probe;
		probe.fixture = &fixture;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		const Error initialized = ordinal_initialize(fixture, server, plan, &Probe::receive, &probe);
		CHECK(initialized == OK);
		if (initialized != OK) {
			fixture.cleanup_owner();
			return;
		}
		// Replace prepare_device's quoted timestamp prefix with the actual empty
		// controlled frame, matching SplitStorage before its original create FIFO.
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		// The ordinary storage row reaches frozen only through its real owner
		// factory. No resource kind/imported bit or protected frame is edited.
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		fixture.rd.frames_drawn = 3;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backing{};
		CHECK(fixture.registry.production_storage_create(ref, 0, &origin, &backing).native_error == ERR_BUSY);
		server->sync();
		REQUIRE(fixture.registry.production_storage_create(ref, 0, &origin, &backing).native_error == OK);
		CHECK(backing.rid != 0);
		CHECK(backing.backing_serial != 0);
		CHECK(backing.byte_size == 16);
		CHECK(fixture.driver.allocations == 1);
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(fixture.registry.production_record_consume_v2(ref, control, &recorded).native_error == OK);
		CHECK(probe.calls == 1);
		CHECK(fixture.driver.allocations == 1); // Metadata rejection created nothing.
		const GDExtensionStorageRetireRequestV1 retire_origin = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 0, 0, 1, 1, 0 };
		GDExtensionStorageRetireRefV1 retire{};
		GDExtensionStorageRetireReportV1 report{};
		REQUIRE(fixture.registry.production_storage_retire(ref, 0, &retire_origin, &retire, &report).native_error == OK);
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		for (uint32_t turn = 0; turn < 32 && !report.state; ++turn) {
			const Error scheduled = Error(fixture.registry.production_storage_observe(ref, retire, &budget, &report).native_error);
			CHECK((scheduled == OK || scheduled == ERR_BUSY));
			server->sync();
			const Error observed = Error(fixture.registry.production_storage_observe(ref, retire, &budget, &report).native_error);
			CHECK((observed == OK || observed == ERR_BUSY));
		}
		CHECK(report.state == 1);
		CHECK(report.return_status == 1);
		CHECK(report.event_seq != 0);
		CHECK(fixture.driver.buffer_frees == 1);
		CHECK_FALSE(fixture.driver.buffer_live);
		REQUIRE(fixture.registry.production_storage_release(ref, retire).native_error == OK);
		// This is an ordinal V2 request. The SplitStorage drain helper uses
		// V1 observe, which correctly rejects this request before accepting a
		// maintenance control and therefore cannot finish its original tail.
		CHECK(request.contract_version == 2);
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		REQUIRE(observe);
		auto tail_budget = budget;
		GDExtensionProductionReportV2 tail_report{};
		for (uint32_t turn = 0; turn < 64 && request.maintenance_serial; ++turn) {
			{
				MutexLock lock(fixture.rd._thread_safe_);
				CHECK(fixture.rd.native_disposal_active);
				CHECK(fixture.rd.native_disposal_continuation.production_owner == &request);
				CHECK(fixture.rd.native_disposal_continuation.serial == request.maintenance_serial);
			}
			if (request.control.state == ProductionControlState::OWNER_CONSUMED) {
				tail_budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			}
			const Error scheduled = Error(observe(ref, &tail_budget, &tail_report).native_error);
			CHECK((scheduled == OK || scheduled == ERR_BUSY));
			if (request.control.state != ProductionControlState::OWNER_CONSUMED) {
				server->sync();
				if (request.control.state == ProductionControlState::RETURNED) {
					CHECK(request.control.output.service_usage.receipts <= 1);
					CHECK(request.control.output.service_usage.passes <= 1);
				}
				const Error consumed = Error(observe(ref, &tail_budget, &tail_report).native_error);
				CHECK((consumed == OK || consumed == ERR_BUSY));
			}
			CHECK(request.serial == ref.handle_serial);
		}
		{
			MutexLock lock(fixture.rd._thread_safe_);
			CHECK_FALSE((fixture.rd.native_disposal_active && fixture.rd.native_disposal_continuation.production_owner == &request));
			CHECK(request.maintenance_serial == 0);
		}
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	static void output_metadata_declarations() {
		using namespace GDExtensionDeviceExit;
		for (uint32_t mutation = 0; mutation < 9; ++mutation) {
			auto *server = ordinal_server();
			if (!server) {
				return;
			}
			OrdinalPlan plan;
			plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 2, 1, 1, 2, 2048, 1, 1, 0, 4 };
			plan.action_count = plan.use_count = plan.readback_count = 0;
			plan.resource_count = 1;
			plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 128, 0 };
			switch (mutation) {
				case 0:
					plan.resources[0].ownership = GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1;
					break;
				case 1:
					plan.resources[0].required_usage = 1;
					break;
				case 2:
					plan.resources[0].physical_bytes = 0;
					break;
				case 3:
					plan.resources[0].physical_bytes = uint64_t(UINT32_MAX) + 1;
					break;
				case 4:
					plan.use_count = 1;
					plan.uses[0] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 4, 0 };
					break;
				case 5:
					plan.action_count = 1;
					plan.actions[0] = { 0, 11, 0, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 0, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
					break;
				case 6:
					plan.action_count = plan.use_count = 1;
					plan.uses[0] = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 };
					plan.actions[0] = { 0, 11, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, 0, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
					break;
				case 7:
					plan.action_count = 2;
					plan.actions[0] = { 0, 11, 0, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
					plan.actions[1] = { 1, 12, 0, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
					plan.readback_count = 1;
					plan.readback = { 0, 0, 0, 1, 0, 4 };
					break;
				case 8:
					break; // Same valid capacity and lawful metadata declaration.
			}
			SingletonEnrollment fixture;
			fixture.public_port = true;
			GDExtensionProductionMetadataQuoteV1 layout{};
			REQUIRE(fixture.registry.production_layout(plan.capacity, layout) == OK);
			const Error error = ordinal_initialize(fixture, server, plan);
			if (mutation == 8) {
				REQUIRE(error == OK);
				REQUIRE(fixture.request != nullptr);
				const GDExtensionProductionRequestRefV1 ref = { fixture.instance, fixture.request->serial };
				CHECK(drain_request_cursor(fixture, ref, [&]() { server->sync(); }));
			} else {
				CHECK_MESSAGE(error == ERR_INVALID_DATA, "metadata declaration mutation=", mutation);
				CHECK(fixture.request == nullptr);
			}
			CHECK(fixture.driver.issues == 0);
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
		}
	}
	static void output_native_admission() {
		using namespace GDExtensionDeviceExit;
		static_assert(GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 == 6);
		static_assert(GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 == 3);
		for (uint32_t mutation = 0; mutation < 12; ++mutation) {
			auto *server = ordinal_server();
			if (!server) {
				return;
			}
			OrdinalPlan plan;
			plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 1, 1, 1, 2, 2048, 0, 1, 0, 0 };
			plan.operation.canonical_end = plan.operation.canonical_begin + 1;
			plan.action_count = plan.resource_count = plan.use_count = 1;
			plan.readback_count = 0;
			plan.actions[0] = { 0, 10, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
			plan.resources[0] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 1, 16, 0 };
			plan.uses[0] = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 16, 0 };
			switch (mutation) {
				case 0:
					break; // Declaration admission only, not an output/physical grant.
				case 1:
					plan.resources[0].resource_kind = GDEXTENSION_PRODUCTION_UNIT_V1;
					break;
				case 2:
					plan.resources[0].resource_kind = GDEXTENSION_PRODUCTION_SET_V1;
					break;
				case 3:
					plan.resources[0].resource_kind = GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1;
					plan.use_count = plan.actions[0].use_count = 0;
					break;
				case 4:
					plan.actions[0].physical_unit = 0;
					break;
				case 5:
					plan.actions[0].readback_slot = 0;
					break;
				case 6:
					plan.actions[0].list_boundary = GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1;
					break;
				case 7:
					plan.actions[0].list_boundary = GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 + 1;
					break;
				case 8:
					plan.resources[0].physical_bytes = 0;
					plan.use_count = plan.actions[0].use_count = 0;
					break;
				case 9:
					plan.resources[0].original_generation = 0;
					break;
				case 10:
					// Isolate V1's new-action rejection from alias ownership.
					plan.resources[0].ownership = GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1;
					break;
				case 11:
					// Isolate V1's alias rejection using an existing action kind.
					plan.actions[0].action_kind = GDEXTENSION_PRODUCTION_UPLOAD_V1;
					break;
			}
			SingletonEnrollment fixture;
			fixture.public_port = true;
			Error error;
			if (mutation >= 10) {
				REQUIRE(fixture.prepare_device(server) == OK);
				REQUIRE(fixture.initialize_owner(1, false, false, +[](void *, ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {}, nullptr, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan, nullptr, true) == OK);
				const auto &a = plan.actions[0];
				const GDExtensionProductionActionV1 action = { a.canonical_suboperation, a.action_kind, a.physical_unit, a.first_use, a.use_count, a.list_boundary, a.readback_slot };
				GDExtensionProductionRequestRefV1 rejected{};
				error = Error(fixture.port.reserve(fixture.attachment, &plan.operation, &plan.capacity, &action, 1, plan.resources, 1, plan.uses, 1, nullptr, 0, &rejected).native_error);
				CHECK(rejected.handle_serial == 0);
			} else {
				error = ordinal_initialize(fixture, server, plan);
			}
			if (mutation == 0) {
				REQUIRE(error == OK);
				REQUIRE(fixture.request != nullptr);
				auto &request = *fixture.request;
				const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
				CHECK(request.contract_version == 2);
				CHECK(request.actions[0].declaration.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1);
				CHECK(request.resources[0].declaration.ownership == GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1);
				CHECK_FALSE(request.actions[0].prepared);
				CHECK_FALSE(request.resources[0].output_alias_prepared);
				CHECK_FALSE(request.resources[0].imported);
				CHECK(request.resources[0].primary.is_null());
				CHECK(request.resources[0].use_count == 0);
				CHECK(request.resources[0].owned_serial == 0);
				// Well-shaped claims must be refused by ordinary ownership gates;
				// neither call is a real backing or native output preparation.
				GDExtensionStorageBackingV1 claimed{};
				claimed.abi = Enrollment::ABI;
				claimed.backing = { fixture.instance, 1 };
				CHECK(fixture.port.import_storage(ref, 0, &claimed).native_error == ERR_INVALID_DATA);
				const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
				GDExtensionStorageBackingV1 created{};
				CHECK(fixture.port.storage_create(ref, 0, &origin, &created).native_error == ERR_INVALID_DATA);
				CHECK(created.rid == 0);
				CHECK_FALSE(request.resources[0].imported);
				CHECK(request.resources[0].owned_serial == 0);
				CHECK(request.resources[0].use_count == 0);
				CHECK(request.resources[0].primary.is_null());
				CHECK_FALSE(request.actions[0].prepared);
				CHECK(drain_request_cursor(fixture, ref, [&]() { server->sync(); }));
			} else {
				CHECK_MESSAGE(error == ERR_INVALID_DATA, "native output declaration mutation=", mutation);
				CHECK(fixture.request == nullptr);
			}
			CHECK(fixture.driver.issues == 0);
			fixture.cleanup_owner();
			CHECK(fixture.owner_cleaned);
		}
	}
	struct NativeSceneAcquireProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		GDExtensionProductionRequestRefV1 create_ref{};
		GDExtensionOutputCreateResultV1 created{};
		GDExtensionOutputAcquireInputV1 input{};
		GDExtensionInterfaceDeviceSessionOutputCreateV2 create = nullptr;
		GDExtensionInterfaceDeviceSessionOutputAcquireV1 acquire = nullptr;
		GDExtensionInterfaceDeviceSessionOutputCancelV1 cancel = nullptr;
		GDExtensionInterfaceDeviceSessionOutputWithdrawV1 withdraw = nullptr;
		GDExtensionInterfaceDeviceSessionProductionAppendV2 append = nullptr;
		bool withdrawing = false;
		uint32_t create_calls = 0, acquire_calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &probe = *static_cast<NativeSceneAcquireProbe *>(opaque);
			auto &fixture = *probe.fixture;
			if (ref.handle_serial == probe.create_ref.handle_serial) {
				if (probe.withdrawing) {
					GDExtensionOutputGenerationInputV1 generation{ production_dto_abi<GDExtensionOutputGenerationInputV1>(), probe.created.particles, probe.created.generation };
					REQUIRE(probe.withdraw(context, ref, &generation).error == OK);
					return;
				}
				++probe.create_calls;
				GDExtensionOutputCreateInputV2 config{};
				config.abi = production_dto_abi<GDExtensionOutputCreateInputV2>();
				config.abi.abi_version = 2;
				config.config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
				config.config.profile = 3;
				config.config.max_views = config.config.max_inflight = 1;
				config.config.space = config.config.query_enabled = 1;
				for (uint32_t axis = 0; axis < 3; ++axis) {
					config.config.bounds.size[axis] = config.config.query_bounds.size[axis] = 1;
				}
				config.metadata_resource_slot = 0;
				probe.create(context, ref, &config, &probe.created);
				REQUIRE(probe.created.status.error == OK);
				REQUIRE(probe.native->adopt_created(RID::from_uint64(probe.created.particles)));
				return;
			}
			++probe.acquire_calls;
			auto &request = *fixture.request;
			CHECK(request.next_action == 0);
			GDExtensionOutputAcquireResultV1 wrong{};
			probe.acquire(context, ref, 0, &probe.input, &wrong);
			CHECK(wrong.status.error != OK);
			CHECK(wrong.writable_count == 0);
			CHECK(request.next_action == 0);
			GDExtensionProductionAppendV2 copied{};
			REQUIRE(probe.append(context, ref, 0, request.revision, &copied).native_error == OK);
			CHECK(request.next_action == 1);
			CHECK(request.graph_count == 1);
			GDExtensionOutputAcquireResultV1 acquired{};
			probe.acquire(context, ref, 1, &probe.input, &acquired);
			REQUIRE(acquired.status.error == OK);
			CHECK(acquired.request_serial == ref.handle_serial);
			CHECK(acquired.action_ordinal == 1);
			CHECK(acquired.custody == GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1);
			CHECK(acquired.writable_count == 0); // Actual SceneOnly, no raster profile.
			CHECK(request.next_action == 2);
			const uint64_t revision = request.revision;
			GDExtensionOutputAcquireResultV1 duplicate{};
			probe.acquire(context, ref, 1, &probe.input, &duplicate);
			CHECK(duplicate.status.error == OK);
			CHECK(memcmp(duplicate.lease_token.bytes, acquired.lease_token.bytes, 32) == 0);
			CHECK(request.next_action == 2);
			CHECK(request.revision == revision);
			auto changed = probe.input;
			++changed.content_version;
			probe.acquire(context, ref, 1, &changed, &wrong);
			CHECK(wrong.status.error == ERR_INVALID_DATA);
			CHECK(wrong.writable_count == 0);
			const GDExtensionOutputCancelInputV1 cancellation{ production_dto_abi<GDExtensionOutputCancelInputV1>(), probe.input.particles, acquired.lease_token };
			REQUIRE(probe.cancel(context, ref, &cancellation).error == OK);
			probe.acquire(context, ref, 1, &probe.input, &duplicate);
			CHECK(duplicate.status.error != OK);
			CHECK(duplicate.writable_count == 0);
			CHECK(duplicate.custody == GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1);
			CHECK(request.next_action == 2);
		}
	};
	static void output_native_scene_acquire() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		const auto prepare = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1>("device_session_production_prepare_output_v1");
		const auto prepare_copy = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareCopyV2>("device_session_production_prepare_copy_v2");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(reserve != nullptr);
		REQUIRE(prepare != nullptr);
		REQUIRE(prepare_copy != nullptr);
		REQUIRE(consume != nullptr);
		REQUIRE(observe != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan pool;
		pool.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 2, 3, 6, 4, 2048, 0, 3, 0, 0 };
		pool.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		NativeSceneAcquireProbe probe;
		probe.fixture = &fixture;
		probe.create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV2>(gdextension_get_guarded_proc_address("device_session_output_create_v2"));
		probe.acquire = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputAcquireV1>(gdextension_get_guarded_proc_address("device_session_output_acquire_v1"));
		probe.cancel = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCancelV1>(gdextension_get_guarded_proc_address("device_session_output_cancel_v1"));
		probe.withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
		probe.append = ordinal_port<GDExtensionInterfaceDeviceSessionProductionAppendV2>("device_session_production_append_v2");
		REQUIRE(probe.create != nullptr);
		REQUIRE(probe.acquire != nullptr);
		REQUIRE(probe.cancel != nullptr);
		REQUIRE(probe.withdraw != nullptr);
		REQUIRE(probe.append != nullptr);
		REQUIRE(fixture.prepare_device(server) == OK);
		REQUIRE(fixture.initialize_owner(1, false, false, &NativeSceneAcquireProbe::receive, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, nullptr, true) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.native = &native;
		auto operation = pool.operation;
		const GDExtensionProductionCapacityV1 create_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		const GDExtensionProductionResourceV1 metadata = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes(), 0 };
		REQUIRE(reserve(fixture.attachment, &operation, &create_capacity, nullptr, 0, &metadata, 1, nullptr, 0, nullptr, 0, &probe.create_ref).native_error == OK);
		Request *create_request = nullptr;
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(fixture.registry.production_request_locked(probe.create_ref, false, create_request, rd) == OK);
		}
		fixture.request = create_request;
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(probe.create_ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(probe.create_ref, control, &recorded).native_error == OK);
		REQUIRE(probe.create_calls == 1);
		REQUIRE(native.exists());
		// The second request uses the actual first Create result, never a predicted RID.
		++operation.owner_request_nonce;
		const GDExtensionProductionCapacityV1 capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 2, 2, 5, 4, 2048, 0, 2, 0, 0 };
		const GDExtensionProductionActionV2 actions[] = {
			{ 0, 11, 3, 0, GDEXTENSION_PRODUCTION_COPY_V1, UINT32_MAX, 0, 2, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 1, 11, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 2, 2, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX }
		};
		const GDExtensionProductionResourceV1 sources[] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 }
		};
		const GDExtensionProductionUseV1 uses[] = {
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 4, 0 }, { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 4, 4, 0 },
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 4, 0 }, { 1, GDEXTENSION_PRODUCTION_READ_V1, 4, 4, 0 }
		};
		GDExtensionProductionRequestRefV1 ref{};
		REQUIRE(reserve(fixture.attachment, &operation, &capacity, actions, 2, sources, 2, uses, 4, nullptr, 0, &ref).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(fixture.registry.production_request_locked(ref, false, fixture.request, rd) == OK);
		}
		auto &request = *fixture.request;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		fixture.rd.frames_drawn = 3;
		fixture.driver.native_output_copy = true;
		fixture.driver.output_storage_factory = true;
		const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backings[2]{};
		for (uint32_t slot = 0; slot < 2; ++slot) {
			CHECK(fixture.port.storage_create(ref, slot, &origin, &backings[slot]).native_error == ERR_BUSY);
			server->sync();
			REQUIRE(fixture.port.storage_create(ref, slot, &origin, &backings[slot]).native_error == OK);
		}
		REQUIRE(backings[0].rid != backings[1].rid);
		REQUIRE(backings[0].backing_serial != backings[1].backing_serial);
		const auto *source_buffer = fixture.rd.storage_buffer_owner.get_or_null(RID::from_uint64(backings[0].rid));
		const auto *destination_buffer = fixture.rd.storage_buffer_owner.get_or_null(RID::from_uint64(backings[1].rid));
		REQUIRE(source_buffer != nullptr);
		REQUIRE(destination_buffer != nullptr);
		CHECK(source_buffer->driver_id == RDD::BufferID(300));
		CHECK(destination_buffer->driver_id == RDD::BufferID(301));
		REQUIRE(source_buffer->draw_tracker != nullptr);
		REQUIRE(destination_buffer->draw_tracker != nullptr);
		CHECK(source_buffer->draw_tracker != destination_buffer->draw_tracker);
		for (uint32_t i = 0; i < 4; ++i) {
			fixture.driver.upload_memory[0][i] = uint8_t(17 + i);
		}
		const GDExtensionProductionCopyV2 copy = { production_dto_abi<GDExtensionProductionCopyV2>(), 0, 11, 3, 0, 0, 1, 0, 4, 4 };
		REQUIRE(prepare_copy(ref, &copy).native_error == OK);
		probe.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		probe.input.particles = probe.created.particles;
		probe.input.common.generation = probe.created.generation;
		probe.input.common.present_seq = probe.input.content_version = probe.input.view_epoch = 1;
		probe.input.common.current_camera = { 1, 1, 1 };
		for (uint32_t axis = 0; axis < 3; ++axis) {
			probe.input.common.bounds.size[axis] = 1;
		}
		GDExtensionProductionOutputPrepareV1 prepared{};
		prepared.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		prepared.action_ordinal = 1;
		prepared.canonical_suboperation = 11;
		prepared.source_recipe_substep = 7;
		prepared.source_resource_slot = 0;
		prepared.source_use_index = prepared.first_source_use = 2;
		prepared.source_use_count = 2;
		prepared.source_generation = 1;
		prepared.input = probe.input;
		const uint32_t roles[] = { 1, 2, 7 };
		for (uint32_t i = 0; i < 3; ++i) {
			prepared.roles[i] = { roles[i], UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		}
		auto missing = prepared;
		missing.source_use_count = 1;
		CHECK(prepare(ref, &missing).native_error == ERR_INVALID_DATA);
		CHECK_FALSE(request.actions[1].prepared);
		auto crossed = prepared;
		crossed.source_use_index = 0; // Valid READ row, but belongs to producer0.
		CHECK(prepare(ref, &crossed).native_error == ERR_INVALID_DATA);
		CHECK_FALSE(request.actions[1].prepared);
		REQUIRE(prepare(ref, &prepared).native_error == OK);
		CHECK(prepare(ref, &prepared).native_error == OK);
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(probe.acquire_calls == 1);
		CHECK(request.next_action == 2);
		// Both actual owners retire through original controls, without repairing flags.
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		GDExtensionProductionReportV2 report{};
		for (uint32_t turn = 0; turn < 64 && request.output_owner_head != UINT32_MAX; ++turn) {
			auto offered = budget;
			offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			const Error e = Error(observe(ref, &offered, &report).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
			if (request.control.state == ProductionControlState::RETURNED) {
				const Error consumed = Error(observe(ref, &offered, &report).native_error);
				CHECK((consumed == OK || consumed == ERR_BUSY));
			}
		}
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(request.output_owner_head == UINT32_MAX);
		probe.withdrawing = true;
		REQUIRE(fixture.port.record_schedule(probe.create_ref, &control) == OK);
		server->sync();
		REQUIRE(consume(probe.create_ref, control, &recorded).native_error == OK);
		for (uint32_t turn = 0; turn < 64 && !native.released(); ++turn) {
			auto offered = budget;
			offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			// The public report cannot carry native_released; preserve and consume
			// the original complete generation result through its original control.
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(probe.create_ref, ProductionControlKind::OBSERVE, &serial, &offered) == OK);
			REQUIRE(fixture.registry.production_control_schedule(probe.create_ref, serial) == OK);
			server->sync();
			REQUIRE(create_request->control.state == ProductionControlState::RETURNED);
			ProductionControlOutput output{};
			REQUIRE(fixture.registry.production_control_consume(probe.create_ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
		}
		REQUIRE(native.released());
		native.free_created();
		for (uint32_t slot = 0; slot < 2; ++slot) {
			const auto &backing = backings[slot];
			const GDExtensionStorageRetireRequestV1 retirement = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 0, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 retired{};
			GDExtensionStorageRetireReportV1 returned{};
			REQUIRE(fixture.port.storage_retire(ref, slot, &retirement, &retired, &returned).native_error == OK);
			for (uint32_t turn = 0; turn < 64 && !returned.state; ++turn) {
				auto offered = budget;
				offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				const Error e = Error(fixture.port.storage_observe(ref, retired, &offered, &returned).native_error);
				CHECK((e == OK || e == ERR_BUSY));
				server->sync();
			}
			REQUIRE(returned.state == 1);
			REQUIRE(fixture.port.storage_release(ref, retired).native_error == OK);
		}
		for (const auto target : { ref, probe.create_ref }) {
			Request *original = target.handle_serial == ref.handle_serial ? &request : create_request;
			for (uint32_t turn = 0; turn < 64 && original->serial; ++turn) {
				auto offered = budget;
				offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				GDExtensionProductionServiceBudgetV1 remaining{};
				const Error e = Error(release(target, &offered, &remaining, &report).native_error);
				CHECK((e == OK || e == ERR_BUSY));
				server->sync();
			}
			CHECK(original->serial == 0);
		}
		CHECK(fixture.driver.upload_copies == 1);
		for (uint32_t i = 0; i < 4; ++i) {
			CHECK(fixture.driver.upload_memory[1][4 + i] == uint8_t(17 + i));
		}
		CHECK(fixture.driver.output_storage_count == 2);
		CHECK(fixture.driver.output_storage_frees == 2);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct NativeAliasReadbackProbe {
		SingletonEnrollment *fixture = nullptr;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		RendererRD::ExternalOutputConfig config;
		GDExtensionProductionRequestRefV1 create_ref{};
		GDExtensionOutputAcquireInputV1 input{};
		GDExtensionOutputAcquireResultV1 acquired{};
		bool closing = false;
		uint32_t create_calls = 0, acquire_calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &p = *static_cast<NativeAliasReadbackProbe *>(opaque);
			auto &f = *p.fixture;
			if (ref.handle_serial == p.create_ref.handle_serial) {
				if (p.closing) {
					p.native->withdraw_generation();
					return;
				}
				uint64_t bytes = 0;
				uint32_t alignment = 0;
				REQUIRE(RendererRD::ExternalLinkedOwnerTest::selection_metadata_bytes(p.config, bytes, &alignment) == OK);
				ProductionOutputMetadataLease metadata;
				REQUIRE(f.registry.production_output_metadata_create(context, ref, 0, f.rd, bytes, alignment, metadata, true) == OK);
				REQUIRE(p.native->create_selection_component(context, ref, p.config, metadata, 0) == OK);
				CHECK(metadata.is_empty());
				++p.create_calls;
				return;
			}
			auto cancel = ordinal_port<GDExtensionInterfaceDeviceSessionOutputCancelV1>("device_session_output_cancel_v1");
			if (p.closing) {
				GDExtensionOutputCancelInputV1 input{ production_dto_abi<GDExtensionOutputCancelInputV1>(), p.input.particles, p.acquired.lease_token };
				REQUIRE(cancel(context, ref, &input).error == OK);
				return;
			}
			auto acquire = ordinal_port<GDExtensionInterfaceDeviceSessionOutputAcquireV1>("device_session_output_acquire_v1");
			auto append = ordinal_port<GDExtensionInterfaceDeviceSessionProductionAppendV2>("device_session_production_append_v2");
			acquire(context, ref, 0, &p.input, &p.acquired);
			REQUIRE(p.acquired.status.error == OK);
			CHECK(p.acquired.writable_count == 3);
			++p.acquire_calls;
			CHECK(f.request->next_action == 1);
			GDExtensionProductionAppendV2 uploaded{};
			REQUIRE(append(context, ref, 1, f.request->revision, &uploaded).native_error == OK);
			auto &request = *f.request;
			auto &writer = request.actions[0];
			auto &source = request.resources[0];
			auto &alias = request.resources[1];
			auto &readback = request.readbacks[0];
			REQUIRE(request.next_action == 2);
			REQUIRE(writer.output_state == ProductionActionCell::OutputState::BOUND);
			REQUIRE(readback.source.issued == 0);
			// Corrupt only an existing association for one rejected append, then
			// restore it before assertions or the next operation. No factory cell,
			// owner state, readiness or completion is manufactured by these cases.
			auto reject_readback = [&](const char *label, auto &field, auto replacement, Error expected = ERR_UNAUTHORIZED) {
				INFO(label);
				const auto revision = request.revision;
				const auto graphs = request.graph_count, natives = request.native_count, frontier = request.next_action, captures = request.capture_inflight;
				const auto commands = f.rd.draw_graph.qualification_command_count();
				const auto downloads = f.rd.frames[0].download_buffer_get_data_requests.size();
				const auto staging_fill = f.rd.download_staging_buffers.blocks[0].fill_amount;
				const auto issued = readback.source.issued, completed = readback.source.completed, returned = readback.source.returned;
				const auto segment_count = readback.capture.count, segment_capacity = readback.capture.capacity;
				const auto *segments = readback.capture.segments;
				const bool native_effect = request.native_effect_entered, append_effect = request.append_effect_entered, pending = request.effects_pending;
				const bool control_effect = request.control.record_history.control_effect_entered, control_append = request.control.record_history.control_append_entered;
				const auto driver_issues = f.driver.issues, driver_allocations = f.driver.alias_allocations, driver_frees = f.driver.alias_frees;
				const auto uploads = f.driver.alias_upload_copies, download_copies = f.driver.alias_download_copies;
				GDExtensionProductionAppendV2 refused{};
				Error error = OK;
				{
					struct Restore {
						decltype(field) target;
						decltype(replacement) original;
						~Restore() { target = original; }
					} restore{ field, field };
					field = replacement;
					error = Error(append(context, ref, 2, revision, &refused).native_error);
				}
				CHECK(error == expected);
				CHECK(refused.state == GDEXTENSION_PRODUCTION_REJECTED_V1);
				CHECK(refused.graph_fact_count == 0);
				CHECK(request.revision == revision);
				CHECK(request.graph_count == graphs);
				CHECK(request.native_count == natives);
				CHECK(request.next_action == frontier);
				CHECK(request.capture_inflight == captures);
				CHECK(f.rd.draw_graph.qualification_command_count() == commands);
				CHECK(f.rd.frames[0].download_buffer_get_data_requests.size() == downloads);
				CHECK(f.rd.download_staging_buffers.blocks[0].fill_amount == staging_fill);
				CHECK(readback.source.issued == issued);
				CHECK(readback.source.completed == completed);
				CHECK(readback.source.returned == returned);
				CHECK_FALSE(readback.source.delivered);
				CHECK(readback.capture.count == segment_count);
				CHECK(readback.capture.capacity == segment_capacity);
				CHECK(readback.capture.segments == segments);
				CHECK(request.native_effect_entered == native_effect);
				CHECK(request.append_effect_entered == append_effect);
				CHECK(request.effects_pending == pending);
				CHECK(request.control.record_history.control_effect_entered == control_effect);
				CHECK(request.control.record_history.control_append_entered == control_append);
				CHECK(f.driver.issues == driver_issues);
				CHECK(f.driver.alias_allocations == driver_allocations);
				CHECK(f.driver.alias_frees == driver_frees);
				CHECK(f.driver.alias_upload_copies == uploads);
				CHECK(f.driver.alias_download_copies == download_copies);
				REQUIRE(error == expected);
				REQUIRE(request.next_action == frontier);
			};
			// RESERVING still passes the shared alias matcher; READBACK itself
			// must require the original acquire's completed BOUND association.
			reject_readback("writer not BOUND", writer.output_state, ProductionActionCell::OutputState::RESERVING);
			reject_readback("source generation mismatch", writer.output_source_generation, writer.output_source_generation + 1);
			reject_readback("source backing mismatch", source.backing_serial, source.backing_serial + 1);
			reject_readback("other actual alias witness", alias.output_witness, request.resources[2].output_witness);
			reject_readback("alias root mismatch", alias.output_root_serial, alias.output_root_serial + 1);
			reject_readback("alias group mismatch", alias.output_group_serial, alias.output_group_serial + 1);
			reject_readback("alias member mismatch", alias.output_member_ordinal, UINT32_MAX);
			reject_readback("alias backing mismatch", alias.backing_serial, alias.backing_serial + 1);
			reject_readback("alias required usage absent", alias.declaration.required_usage, UINT64_MAX);
			reject_readback("readback use is not READ", request.uses[5].declaration.access, uint32_t(GDEXTENSION_PRODUCTION_WRITE_V1), ERR_INVALID_DATA);
			GDExtensionProductionAppendV2 downloaded{};
			REQUIRE(append(context, ref, 2, request.revision, &downloaded).native_error == OK);
			CHECK(f.request->next_action == 3);
			CHECK(f.request->readbacks[0].source.issued == 4);
			CHECK_FALSE(f.request->readbacks[0].source.delivered);
		}
	};
	static void output_alias_readback_complete(bool map_failure = false) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		const auto prepare = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1>("device_session_production_prepare_output_v1");
		const auto prepare_copy = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareCopyV2>("device_session_production_prepare_copy_v2");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto observe = ordinal_port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(reserve != nullptr);
		REQUIRE(prepare != nullptr);
		REQUIRE(prepare_copy != nullptr);
		REQUIRE(consume != nullptr);
		REQUIRE(observe != nullptr);
		REQUIRE(release != nullptr);
		OrdinalPlan pool;
		pool.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 3, 5, 64, 16, 4096, 1, 5, 4, 4 };
		pool.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		NativeAliasReadbackProbe probe;
		probe.fixture = &fixture;
		REQUIRE(fixture.prepare_device(server) == OK);
		using MeshStorage = RendererRD::MeshStorage;
		auto *previous_meshes = MeshStorage::get_singleton();
		auto *meshes = memnew(MeshStorage(MeshStorage::NativeHoldTestTag{}));
		const RID mesh = meshes->mesh_allocate();
		meshes->mesh_initialize(mesh);
		auto *mesh_row = meshes->mesh_owner.get_or_null(mesh);
		mesh_row->surfaces = static_cast<MeshStorage::Mesh::Surface **>(memalloc(2 * sizeof(MeshStorage::Mesh::Surface *)));
		mesh_row->surface_count = 2;
		for (uint32_t i = 0; i < 2; ++i) {
			mesh_row->surfaces[i] = memnew(MeshStorage::Mesh::Surface);
		}
		void *materials = RendererRD::selection_materials_create();
		probe.config.profile = RendererRD::ExternalOutputConfig::INSTANCE_192;
		probe.config.capacity = 1;
		probe.config.max_views = probe.config.max_inflight = 1;
		probe.config.bounds = AABB(Vector3(), Vector3(1, 1, 1));
		probe.config.surfaces.push_back(mesh);
		for (uint32_t i = 0; i < 2; ++i) {
			probe.config.surface_materials.push_back(RendererRD::selection_material_rid(materials, i));
		}
		PackedByteArray *tags[] = { &probe.config.producer_tag, &probe.config.device_tag, &probe.config.layout_digest, &probe.config.mapping_digest, &probe.config.numeric_profile_digest };
		for (auto *tag : tags) {
			tag->resize(32);
			tag->fill(0);
		}
		uint64_t metadata_bytes = 0;
		REQUIRE(RendererRD::ExternalLinkedOwnerTest::selection_metadata_bytes(probe.config, metadata_bytes) == OK);

		REQUIRE(fixture.initialize_owner(1, false, false, &NativeAliasReadbackProbe::receive, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, nullptr, true) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.native = &native;
		auto operation = pool.operation;
		const GDExtensionProductionCapacityV1 create_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		const GDExtensionProductionResourceV1 metadata = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, metadata_bytes, 0 };
		REQUIRE(reserve(fixture.attachment, &operation, &create_capacity, nullptr, 0, &metadata, 1, nullptr, 0, nullptr, 0, &probe.create_ref).native_error == OK);
		Request *create_request = nullptr;
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(fixture.registry.production_request_locked(probe.create_ref, false, create_request, rd) == OK);
		}
		fixture.request = create_request;
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(probe.create_ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(probe.create_ref, control, &recorded).native_error == OK);
		REQUIRE(probe.create_calls == 1);
		REQUIRE(native.exists());
		// The second request uses the actual first Create result, never a predicted RID.
		++operation.owner_request_nonce;
		const GDExtensionProductionCapacityV1 capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 3, 4, 63, 16, 4096, 1, 4, 4, 4 };
		const GDExtensionProductionActionV2 actions[] = {
			{ 0, 11, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 4, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 11, 8, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 4, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 2, 11, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 5, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 }
		};
		const GDExtensionProductionResourceV1 sources[] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 3, 192, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 3, 192, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 3, 32, 0 }
		};
		const GDExtensionProductionUseV1 uses[] = {
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 }, { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 }, { 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 4, 4, 0 }, { 1, GDEXTENSION_PRODUCTION_READ_V1, 4, 4, 0 }
		};
		const GDExtensionProductionReadbackV2 readback = { 1, 0, 0, 2, 4, 4 };
		GDExtensionProductionRequestRefV1 ref{};
		REQUIRE(reserve(fixture.attachment, &operation, &capacity, actions, 3, sources, 4, uses, 6, &readback, 1, &ref).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *rd = nullptr;
			REQUIRE(fixture.registry.production_request_locked(ref, false, fixture.request, rd) == OK);
		}
		auto &request = *fixture.request;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		fixture.rd.frames_drawn = 3;
		fixture.driver.alias_readback_mode = true;
		fixture.driver.alias_download_map_failure = map_failure;
		fixture.rd.upload_staging_buffers.block_size = fixture.rd.upload_staging_buffers.max_size = 64;
		fixture.rd.upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;
		fixture.rd.download_staging_buffers.block_size = fixture.rd.download_staging_buffers.max_size = 64;
		fixture.rd.download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;
		REQUIRE(fixture.rd._insert_staging_block(fixture.rd.upload_staging_buffers) == OK);
		REQUIRE(fixture.rd._insert_staging_block(fixture.rd.download_staging_buffers) == OK);
		const GDExtensionStorageCreateRequestV1 origin = { Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backings[1]{};
		for (uint32_t slot = 0; slot < 1; ++slot) {
			CHECK(fixture.port.storage_create(ref, slot, &origin, &backings[slot]).native_error == ERR_BUSY);
			server->sync();
			REQUIRE(fixture.port.storage_create(ref, slot, &origin, &backings[slot]).native_error == OK);
		}
		const uint8_t pattern[] = { 17, 18, 19, 20 };
		const auto upload = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		const auto prepare_read = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		const GDExtensionProductionUploadV2 uploading{ production_dto_abi<GDExtensionProductionUploadV2>(), 1, 11, 8, 0, 1, 0, 4, 4 };
		REQUIRE(upload(ref, &uploading, pattern, 4).native_error == OK);
		REQUIRE(prepare_read(ref, 2, 0).native_error == OK);
		probe.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		probe.input.particles = native.particle_rid().get_id();
		probe.input.common.generation = RendererRD::ExternalLinkedOwnerTest::generation;
		probe.input.common.present_seq = probe.input.content_version = probe.input.view_epoch = 1;
		probe.input.common.current_camera = { 1, 1, 1 };
		for (uint32_t axis = 0; axis < 3; ++axis) {
			probe.input.common.bounds.size[axis] = 1;
		}
		GDExtensionProductionOutputPrepareV1 prepared{};
		prepared.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		prepared.action_ordinal = 0;
		prepared.canonical_suboperation = 11;
		prepared.source_recipe_substep = 7;
		prepared.source_resource_slot = 0;
		prepared.source_use_index = prepared.first_source_use = 0;
		prepared.source_use_count = 1;
		prepared.source_generation = 1;
		prepared.input = probe.input;
		prepared.roles[0] = { 1, 1, 1, 0, 3, 192 };
		prepared.roles[1] = { 2, 2, 2, 0, 3, 192 };
		prepared.roles[2] = { 7, 3, 3, 0, 3, 32 };
		REQUIRE(prepare(ref, &prepared).native_error == OK);
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		CHECK(probe.acquire_calls == 1);
		CHECK(request.next_action == 3);
		// Both actual owners retire through original controls, without repairing flags.
		const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
		GDExtensionProductionReportV2 report{};
		const auto copy_bytes = ordinal_port<GDExtensionInterfaceDeviceSessionProductionCopyV1>("device_session_production_copy_v1");
		uint8_t returned_bytes[4] = {};
		uint64_t written = 0;
		CHECK(copy_bytes(ref, request.revision, 0, returned_bytes, 4, &written).native_error == ERR_BUSY);
		CHECK(written == 0);
		for (uint32_t turn = 0; turn < 128 && (map_failure ? request.readbacks[0].source.returned != 4 : written != 4); ++turn) {
			auto offered = budget;
			offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			const Error e = Error(observe(ref, &offered, &report).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
			if (request.control.state == ProductionControlState::RETURNED) {
				const Error consumed = Error(observe(ref, &offered, &report).native_error);
				CHECK((consumed == OK || consumed == ERR_BUSY));
			}
			const Error copied = Error(copy_bytes(ref, request.revision, 0, returned_bytes, 4, &written).native_error);
			if (map_failure) {
				CHECK((copied == ERR_BUSY || copied == ERR_CANT_ACQUIRE_RESOURCE));
				CHECK(written == 0);
			} else {
				CHECK((copied == OK || copied == ERR_BUSY));
			}
		}
		if (map_failure) {
			// Actual original Frame delivery attempted the driver map. Its
			// failed return closes capture custody, not CPU byte readiness.
			REQUIRE(request.readbacks[0].source.returned == 4);
			CHECK(request.readbacks[0].source.issued == 4);
			CHECK(request.readbacks[0].source.completed == 0);
			CHECK_FALSE(request.readbacks[0].source.delivered);
			CHECK(request.readbacks[0].source.error == ERR_CANT_ACQUIRE_RESOURCE);
			CHECK(request.readbacks[0].capture.native_code == -7001);
			CHECK(request.capture_inflight == 0);
			CHECK_FALSE(request.effects_pending);
			CHECK_FALSE(request.source_device_lost);
			CHECK(fixture.rd.frames[0].download_buffer_get_data_requests.is_empty());
			CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
			CHECK(fixture.driver.alias_observed_download_maps == 1);
			CHECK(fixture.driver.alias_failed_download_maps == 1);
			CHECK(report.ready_readbacks == 0);
			CHECK(report.cause.native_error == ERR_CANT_ACQUIRE_RESOURCE);
			const auto error_revision = request.revision;
			const auto graphs = request.graph_count, natives = request.native_count;
			const auto driver_issues = fixture.driver.issues, driver_polls = fixture.driver.polls;
			uint8_t untouched[] = { 0xa1, 0xb2, 0xc3, 0xd4 };
			const uint8_t sentinel[] = { 0xa1, 0xb2, 0xc3, 0xd4 };
			uint64_t untouched_written = 99;
			for (uint32_t repeat = 0; repeat < 2; ++repeat) {
				CHECK(copy_bytes(ref, error_revision, 0, untouched, 4, &untouched_written).native_error == ERR_CANT_ACQUIRE_RESOURCE);
				CHECK(untouched_written == 99);
				CHECK(memcmp(untouched, sentinel, 4) == 0);
			}
			CHECK(request.revision == error_revision);
			CHECK(request.graph_count == graphs);
			CHECK(request.native_count == natives);
			CHECK(fixture.driver.issues == driver_issues);
			CHECK(fixture.driver.polls == driver_polls);
			CHECK(fixture.driver.alias_observed_download_maps == 1);
		} else {
			REQUIRE(request.readbacks[0].source.delivered);
			CHECK(request.readbacks[0].source.completed == 4);
			CHECK(request.readbacks[0].source.returned == 4);
			REQUIRE(copy_bytes(ref, request.revision, 0, returned_bytes, 4, &written).native_error == OK);
			CHECK(written == 4);
			CHECK(memcmp(returned_bytes, pattern, 4) == 0);
			// Public copy failures are pure observations: neither destination nor
			// written is reset by stale-revision or insufficient-capacity rejection.
			const auto copy_revision = request.revision;
			const auto copy_graphs = request.graph_count, copy_natives = request.native_count;
			const auto copy_issues = fixture.driver.issues, copy_polls = fixture.driver.polls;
			uint8_t unchanged_bytes[] = { 0xa1, 0xb2, 0xc3, 0xd4 };
			const uint8_t sentinel_bytes[] = { 0xa1, 0xb2, 0xc3, 0xd4 };
			uint64_t unchanged_written = 99;
			REQUIRE(copy_revision > 0);
			CHECK(copy_bytes(ref, copy_revision - 1, 0, unchanged_bytes, 4, &unchanged_written).native_error == ERR_DOES_NOT_EXIST);
			CHECK(unchanged_written == 99);
			CHECK(memcmp(unchanged_bytes, sentinel_bytes, 4) == 0);
			CHECK(copy_bytes(ref, copy_revision, 0, unchanged_bytes, 3, &unchanged_written).native_error == ERR_PARAMETER_RANGE_ERROR);
			CHECK(unchanged_written == 99);
			CHECK(memcmp(unchanged_bytes, sentinel_bytes, 4) == 0);
			CHECK(request.revision == copy_revision);
			CHECK(request.graph_count == copy_graphs);
			CHECK(request.native_count == copy_natives);
			CHECK(fixture.driver.issues == copy_issues);
			CHECK(fixture.driver.polls == copy_polls);
			CHECK(request.capture_inflight == 0);
			CHECK(fixture.driver.alias_observed_download_maps == 1);
			CHECK(fixture.driver.alias_failed_download_maps == 0);
		}
		CHECK(fixture.driver.alias_upload_copies == 1);
		CHECK(fixture.driver.alias_download_copies == 1);
		probe.closing = true;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		REQUIRE(consume(ref, control, &recorded).native_error == OK);

		for (uint32_t turn = 0; turn < 64 && request.output_owner_head != UINT32_MAX; ++turn) {
			auto offered = budget;
			offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			const Error e = Error(observe(ref, &offered, &report).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
			if (request.control.state == ProductionControlState::RETURNED) {
				const Error consumed = Error(observe(ref, &offered, &report).native_error);
				CHECK((consumed == OK || consumed == ERR_BUSY));
			}
		}
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK(request.output_owner_head == UINT32_MAX);
		probe.closing = true;
		REQUIRE(fixture.port.record_schedule(probe.create_ref, &control) == OK);
		server->sync();
		REQUIRE(consume(probe.create_ref, control, &recorded).native_error == OK);
		for (uint32_t turn = 0; turn < 64 && !native.released(); ++turn) {
			auto offered = budget;
			offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			// The public report cannot carry native_released; preserve and consume
			// the original complete generation result through its original control.
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(probe.create_ref, ProductionControlKind::OBSERVE, &serial, &offered) == OK);
			REQUIRE(fixture.registry.production_control_schedule(probe.create_ref, serial) == OK);
			server->sync();
			REQUIRE(create_request->control.state == ProductionControlState::RETURNED);
			ProductionControlOutput output{};
			REQUIRE(fixture.registry.production_control_consume(probe.create_ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
		}
		REQUIRE(native.released());
		native.free_created();
		if (map_failure) {
			// Actual cancellation/owner retirement did not rewrite the failed
			// readback into a completed result or retry its failed map.
			CHECK(request.readbacks[0].source.error == ERR_CANT_ACQUIRE_RESOURCE);
			CHECK(request.readbacks[0].source.returned == 4);
			CHECK(request.readbacks[0].source.completed == 0);
			CHECK_FALSE(request.readbacks[0].source.delivered);
			CHECK(fixture.driver.alias_failed_download_maps == 1);
		}

		for (uint32_t slot = 0; slot < 1; ++slot) {
			const auto &backing = backings[slot];
			const GDExtensionStorageRetireRequestV1 retirement = { Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 0, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 retired{};
			GDExtensionStorageRetireReportV1 returned{};
			REQUIRE(fixture.port.storage_retire(ref, slot, &retirement, &retired, &returned).native_error == OK);
			for (uint32_t turn = 0; turn < 64 && !returned.state; ++turn) {
				auto offered = budget;
				offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				const Error e = Error(fixture.port.storage_observe(ref, retired, &offered, &returned).native_error);
				CHECK((e == OK || e == ERR_BUSY));
				server->sync();
			}
			REQUIRE(returned.state == 1);
			REQUIRE(fixture.port.storage_release(ref, retired).native_error == OK);
		}
		if (map_failure) {
			REQUIRE(fixture.rd.native_disposal_active);
			REQUIRE(fixture.rd.native_disposal_continuation.production_owner == &request);
			REQUIRE(fixture.rd.native_disposal_continuation.serial == request.maintenance_serial);
			REQUIRE(request.natives[0].owner_index == 0);
			auto &owner = request.natives[0];
			REQUIRE(owner.observation.state == NativeSourceObservation::State::Complete);
			// Each reversible contrast must refuse publication of the original
			// RELEASE cell. A returned map error grants no unknown/native work.
			for (uint32_t factor = 0; factor < 8; ++factor) {
				const auto saved_observation = owner.observation;
				const auto saved_issue = owner.member.source.issue;
				const auto saved_readback = request.readbacks[0].source;
				const bool saved_pending = request.graphs[0].pending;
				auto *saved_owner = fixture.rd.native_disposal_continuation.production_owner;
				switch (factor) {
					case 0:
						owner.observation.state = NativeSourceObservation::State::Pending;
						break;
					case 1:
						owner.observation.state = NativeSourceObservation::State::Unknown;
						break;
					case 2:
						owner.observation.completion_error = ERR_UNAVAILABLE;
						break;
					case 3:
						--request.readbacks[0].source.returned;
						break;
					case 4:
						request.readbacks[0].source.error = ERR_UNAVAILABLE;
						break;
					case 5:
						request.graphs[0].pending = true;
						break;
					case 6:
						fixture.rd.native_disposal_continuation.production_owner = nullptr;
						break;
					case 7:
						for (uint32_t i = 0; i < request.native_count; ++i) {
							if (request.natives[i].owner_index == 0) {
								request.natives[i].member.source.issue = RDD::IssueFact::IssuedUnknown;
							}
						}
						break;
				}
				auto offered = budget;
				offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				GDExtensionProductionServiceBudgetV1 remaining{};
				CHECK(release(ref, &offered, &remaining, &report).native_error == ERR_BUSY);
				CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
				CHECK(request.serial == ref.handle_serial);
				CHECK(fixture.rd.native_disposal_active);
				CHECK(fixture.rd.native_disposal_continuation.serial == request.maintenance_serial);
				owner.observation = saved_observation;
				for (uint32_t i = 0; i < request.native_count; ++i) {
					if (request.natives[i].owner_index == 0) {
						request.natives[i].member.source.issue = saved_issue;
					}
				}
				request.readbacks[0].source = saved_readback;
				request.graphs[0].pending = saved_pending;
				fixture.rd.native_disposal_continuation.production_owner = saved_owner;
			}
		}
		for (const auto target : { ref, probe.create_ref }) {
			Request *original = target.handle_serial == ref.handle_serial ? &request : create_request;
			for (uint32_t turn = 0; turn < 64 && original->serial; ++turn) {
				auto offered = budget;
				offered.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
				GDExtensionProductionServiceBudgetV1 remaining{};
				const Error e = Error(release(target, &offered, &remaining, &report).native_error);
				if (map_failure && target.handle_serial == ref.handle_serial) {
					// The control report may describe bounded cleanup as Busy;
					// its retained readback result must remain the map failure.
					if (original->serial) {
						CHECK(original->readbacks[0].source.error == ERR_CANT_ACQUIRE_RESOURCE);
						CHECK(original->readbacks[0].source.returned == 4);
						CHECK(original->readbacks[0].source.completed == 0);
						CHECK_FALSE(original->readbacks[0].source.delivered);
					}
					if (e == OK) {
						CHECK(report.cause.native_error == ERR_CANT_ACQUIRE_RESOURCE);
					}
				}
				CHECK((e == OK || e == ERR_BUSY));
				server->sync();
			}
			CHECK(original->serial == 0);
		}
		CHECK(fixture.driver.alias_allocations == 8);
		CHECK(fixture.driver.alias_frees == 8);
		for (auto &block : fixture.rd.upload_staging_buffers.blocks) {
			fixture.driver.buffer_unmap(block.driver_id);
			fixture.driver.buffer_free(block.driver_id);
		}
		fixture.rd.upload_staging_buffers.blocks.clear();
		for (auto &block : fixture.rd.download_staging_buffers.blocks) {
			fixture.driver.buffer_unmap(block.driver_id);
			fixture.driver.buffer_free(block.driver_id);
		}
		fixture.rd.download_staging_buffers.blocks.clear();
		CHECK_FALSE(fixture.driver.alias_upload_live);
		CHECK_FALSE(fixture.driver.alias_download_live);
		RendererRD::selection_materials_destroy(materials);
		meshes->mesh_free(mesh);
		memdelete(meshes);
		MeshStorage::singleton = previous_meshes;
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct OutputPollAggregationProbe {
		NativeSceneAcquireProbe original;
		GDExtensionInterfaceDeviceSessionOutputPollV1 poll = nullptr;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *operation) {
			using namespace GDExtensionDeviceExit;
			auto &test = *static_cast<OutputPollAggregationProbe *>(opaque);
			// Real public Create and real generation pin/metadata ownership.
			test.original.create_ref = ref;
			NativeSceneAcquireProbe::receive(&test.original, context, ref, operation);
			auto &fixture = *test.original.fixture;
			auto &request = *fixture.request;
			auto &native = *test.original.native;
			const GDExtensionOutputGenerationInputV1 input{ production_dto_abi<GDExtensionOutputGenerationInputV1>(), test.original.created.particles, test.original.created.generation };
			auto check_poll = [&](uint64_t expected) {
				const uint64_t revision = request.revision;
				const uint32_t graphs = request.graph_count, natives = request.native_count;
				const auto next = request.next_action;
				const uint32_t attempts = fixture.driver.output_storage_attempts, frees = fixture.driver.output_storage_frees;
				GDExtensionOutputPollResultV1 result{};
				test.poll(context, ref, &input, &result);
				CHECK(result.status.error == OK);
				CHECK(result.physical_returned == expected);
				CHECK(result.request_serial == request.serial);
				CHECK(result.action_ordinal == UINT64_MAX);
				CHECK(request.revision == revision);
				CHECK(request.next_action == next);
				CHECK(request.graph_count == graphs);
				CHECK(request.native_count == natives);
				CHECK(fixture.driver.output_storage_attempts == attempts);
				CHECK(fixture.driver.output_storage_frees == frees);
			};
			check_poll(0);
			// CPU projection fixture only: two storage incarnations occupy the same
			// slot. Cursor/cumulative receipt metadata models original retirement's
			// ordering. No native Acquire or Instance192 readiness is manufactured.
			// Every witness RETURNED fact below comes from the actual RD free path.
			for (uint32_t incarnation = 0; incarnation < 2; ++incarnation) {
				native.poll_model_begin_storage();
				for (uint32_t i = 0; i < 2; ++i) {
					REQUIRE(fixture.rd._native_disposal_storage_create(16, {}, native.poll_model_resource_rid(i), native.storage_witness(i)) == OK);
				}
				check_poll(incarnation * 2); // Reuse must retain all previous returns.
				for (uint32_t i = 0; i < 2; ++i) {
					REQUIRE(fixture.rd._native_disposal_free(native.storage_witness(i)) == OK);
				}
				fixture.rd._free_pending_resources(0);
				for (uint32_t i = 0; i < 2; ++i) {
					NativeDisposalWitness::Report returned;
					REQUIRE(fixture.rd._native_disposal_observe(native.storage_witness(i), returned) == OK);
					REQUIRE(returned.state == NativeDisposalWitness::State::RETURNED);
				}
				check_poll((incarnation + 1) * 2);
				// Original retire increments the cumulative count before storage is
				// destroyed. The already consumed prefix must not be counted twice.
				native.poll_model_consume_receipt(0);
				check_poll((incarnation + 1) * 2);
				native.poll_model_consume_receipt(1);
				check_poll((incarnation + 1) * 2);
				native.poll_model_end_storage(2);
				check_poll((incarnation + 1) * 2);
			}
			REQUIRE(test.original.withdraw(context, ref, &input).error == OK);
		}
	};
	static void output_poll_aggregation() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server || !check_step(RendererRD::ParticlesStorage::get_singleton() == nullptr)) {
			return;
		}
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
		plan.action_count = plan.use_count = plan.readback_count = 0;
		plan.resource_count = 1;
		plan.resources[0] = { GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes(), 0 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		OutputPollAggregationProbe probe;
		probe.original.fixture = &fixture;
		probe.original.create = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputCreateV2>(gdextension_get_guarded_proc_address("device_session_output_create_v2"));
		probe.original.withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
		probe.poll = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputPollV1>(gdextension_get_guarded_proc_address("device_session_output_poll_v1"));
		REQUIRE(probe.original.create != nullptr);
		REQUIRE(probe.original.withdraw != nullptr);
		REQUIRE(probe.poll != nullptr);
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		REQUIRE(ordinal_initialize(fixture, server, plan, &OutputPollAggregationProbe::receive, &probe) == OK);
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, true);
		probe.original.native = &native;
		fixture.driver.output_storage_factory = true;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref{ fixture.instance, request.serial };
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
		server->sync();
		GDExtensionProductionRecordReportV2 recorded{};
		REQUIRE(consume(ref, control, &recorded).native_error == OK);
		for (uint32_t turn = 0; turn < 64 && !native.released(); ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &control, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, control) == OK);
			server->sync();
			ProductionControlOutput output{};
			REQUIRE(fixture.registry.production_control_consume(ref, control, &output) == OK);
		}
		REQUIRE(native.released());
		native.free_created();
		for (uint32_t turn = 0; turn < 64 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			const Error e = Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
		}
		CHECK(request.serial == 0);
		CHECK(fixture.driver.output_storage_count == 4);
		CHECK(fixture.driver.output_storage_frees == 4);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct RecordHistoryProbe {
		SingletonEnrollment *fixture = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1 query = nullptr;
		uint32_t calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<RecordHistoryProbe *>(opaque);
			++probe.calls;
			auto &request = *probe.fixture->request;
			GDExtensionProductionRecordHistoryV1 history{};
			REQUIRE(probe.query(ref, request.control.serial, &history).native_error == OK);
			CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_EXECUTING_V1);
			CHECK(history.request_serial == ref.handle_serial);
			CHECK(history.control_serial == request.control.serial);
			record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1,
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1);
		}
	};
	static void record_history_empty_lifecycle(bool refuse_first) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto query = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1>("device_session_production_record_history_v1");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		REQUIRE(query != nullptr);
		REQUIRE(consume != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		RecordHistoryProbe probe;
		probe.fixture = &fixture;
		probe.query = query;
		REQUIRE(ordinal_initialize(fixture, server, plan, &RecordHistoryProbe::receive, &probe) == OK);
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref = { fixture.instance, request.serial };
		auto rejected = [&](GDExtensionProductionRequestRefV1 bad_ref, uint64_t bad_serial) {
			GDExtensionProductionRecordHistoryV1 history;
			std::memset(&history, 0x5a, sizeof(history));
			const auto state = request.control.state;
			const auto serial = request.control.serial;
			const auto revision = request.revision;
			CHECK(query(bad_ref, bad_serial, &history).native_error != OK);
			record_history_zero(history);
			CHECK(request.control.state == state);
			CHECK(request.control.serial == serial);
			CHECK(request.revision == revision);
		};
		rejected(ref, 0); // A reservation is not an accepted RECORD control.
		if (refuse_first) {
			uint32_t filled = 0;
			while (filled < (1u << 20) && server->_try_production_control_dispatch(0, 0, 0, 0)) {
				++filled;
			}
			REQUIRE(filled > 0);
			REQUIRE(filled < (1u << 20));
		}
		uint64_t control = 0;
		const auto scheduled = fixture.port.record_schedule(ref, &control);
		REQUIRE(scheduled == (refuse_first ? ERR_BUSY : OK));
		REQUIRE(control != 0);
		const uint32_t scheduled_flags = GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_ATTEMPTED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_RESULT_VALID_V1;
		const uint32_t effect_flags = GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1;
		const uint32_t terminal_flags = GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1;
		GDExtensionProductionRecordHistoryV1 history{};
		REQUIRE(query(ref, control, &history).native_error == OK);
		CHECK(history.request_serial == ref.handle_serial);
		CHECK(history.control_serial == control);
		CHECK(history.record_generation == plan.operation.record_generation);
		if (refuse_first) {
			CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_PENDING_V1);
			CHECK(history.last_schedule_result.native_error == ERR_BUSY);
			record_history_flags(history, scheduled_flags, GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 | terminal_flags | effect_flags);
			server->sync(); // Drain actual rejected-admission pressure, not a substitute queue.
			uint64_t retried = 0;
			REQUIRE(fixture.port.record_schedule(ref, &retried) == OK);
			CHECK(retried == control);
			REQUIRE(query(ref, control, &history).native_error == OK);
		}
		CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUED_V1);
		CHECK(history.last_schedule_result.native_error == OK);
		record_history_flags(history, scheduled_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1,
				terminal_flags | effect_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1);
		rejected({ ref.registry_instance, UINT64_MAX }, control);
		rejected({ UINT64_MAX, ref.handle_serial }, control);
		rejected(ref, control + 1);
		server->sync();
		REQUIRE(probe.calls == 1);
		REQUIRE(query(ref, control, &history).native_error == OK);
		CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_STATE_V1);
		CHECK(history.record_result.native_error == OK);
		record_history_flags(history, scheduled_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | terminal_flags,
				GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1 | effect_flags);
		GDExtensionProductionRecordReportV2 returned{};
		REQUIRE(consume(ref, control, &returned).native_error == OK);
		REQUIRE(query(ref, control, &history).native_error == OK);
		CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_STATE_V1);
		record_history_flags(history, scheduled_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | terminal_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1,
				GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 | effect_flags);
		CHECK(history.record_result.native_error == returned.result.native_error);
		const auto consumed_history = history;
		CHECK(fixture.port.record_schedule(ref, nullptr) == ERR_INVALID_PARAMETER);
		REQUIRE(query(ref, control, &history).native_error == OK);
		CHECK(history.flags == consumed_history.flags);
		CHECK(history.control_serial == consumed_history.control_serial);
		uint64_t successor = 0;
		REQUIRE(fixture.port.record_schedule(ref, &successor) == OK);
		REQUIRE(successor != control);
		rejected(ref, control); // Accepting the next real control expires the old identity.
		REQUIRE(query(ref, successor, &history).native_error == OK);
		CHECK(history.control_serial == successor);
		CHECK(history.state == GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUED_V1);
		record_history_flags(history, scheduled_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1,
				terminal_flags | effect_flags | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1);
		server->sync();
		REQUIRE(probe.calls == 2);
		REQUIRE(consume(ref, successor, &returned).native_error == OK);
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct RasterReserveProbe {
		SingletonEnrollment *fixture = nullptr;
		Request *raster = nullptr;
		GDExtensionProductionRequestRefV1 raster_ref{};
		GDExtensionProductionPhysicalContextRefV1 expired{};
		GDExtensionInterfaceDeviceSessionProductionRasterBeginV1 begin = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRasterEndV1 end = nullptr;
		uint32_t calls = 0;
		void rejected(GDExtensionProductionPhysicalContextRefV1 context) {
			const uint64_t revision = raster->revision;
			const uint32_t graphs = raster->graph_count, natives = raster->native_count;
			CHECK(begin(context, raster_ref, revision).native_error == ERR_UNAUTHORIZED);
			CHECK(end(context, raster_ref).native_error == ERR_UNAUTHORIZED);
			CHECK(raster->revision == revision);
			CHECK(raster->graph_count == graphs);
			CHECK(raster->native_count == natives);
			CHECK_FALSE(raster->raster_started);
			CHECK_FALSE(raster->draw_list_open);
			CHECK_FALSE(fixture->rd.draw_list.active);
		}
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<RasterReserveProbe *>(opaque);
			++probe.calls;
			CHECK(ref.handle_serial == probe.fixture->request->serial);
			CHECK(ref.handle_serial != probe.raster_ref.handle_serial);
			probe.expired = context;
			probe.rejected(context); // Genuine live RECORD context, but for another request.
		}
	};
	static void singleton_raster_reserve() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterReserveV1>("device_session_production_raster_reserve_v1");
		const auto reserve_empty = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		RasterReserveProbe probe;
		probe.begin = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterBeginV1>("device_session_production_raster_begin_v1");
		probe.end = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterEndV1>("device_session_production_raster_end_v1");
		REQUIRE(reserve != nullptr);
		REQUIRE(reserve_empty != nullptr);
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		REQUIRE(probe.begin != nullptr);
		REQUIRE(probe.end != nullptr);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 2, 4, 4, 8, 2048, 0, 4, 0, 0 };
		plan.attachment_requests = 2;
		SingletonEnrollment fixture;
		fixture.public_port = true;
		probe.fixture = &fixture;
		struct RasterCleanup {
			RasterReserveProbe &probe;
			~RasterCleanup() {
				if (probe.raster && probe.raster->serial) {
					CHECK(probe.fixture->registry.production_cancel(probe.raster_ref).native_error == OK);
					CHECK(probe.fixture->registry.production_release(probe.raster_ref).native_error == OK);
				}
			}
		} raster_cleanup{ probe };
		REQUIRE(fixture.prepare_device(server) == OK);
		REQUIRE(fixture.initialize_owner(1, false, false, &RasterReserveProbe::receive, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan, nullptr, true) == OK);
		REQUIRE(fixture.request == nullptr);
		REQUIRE(fixture.attachment.handle_serial != 0);
		CHECK(fixture.qualification.handle_serial == 0);
		// Native identities are supplied only by independent preparation.
		const GDExtensionProductionRasterActionV1 action = { production_dto_abi<GDExtensionProductionRasterActionV1>(), 0, plan.operation.canonical_begin, 0, 1, 0, 0, 0, 0, 0 };
		for (uint32_t insufficient = 0; insufficient < 7; ++insufficient) {
			auto capacity = plan.capacity;
			auto refused_action = action;
			if (insufficient == 0) {
				capacity.resources = 3;
			} else if (insufficient == 1) {
				capacity.graph_memberships = 7;
			} else if (insufficient == 2) {
				capacity.graph_memberships = 9; // Schema-valid, exceeds the accepted arena.
			} else if (insufficient == 3) {
				capacity.return_cells = 3;
			} else if (insufficient == 4) {
				refused_action.framebuffer_serial = 31;
			} else if (insufficient == 5) {
				refused_action.shader_serial = 32;
			} else {
				refused_action.pipeline_serial = 33;
			}
			GDExtensionProductionRequestRefV1 refused = { UINT64_MAX, UINT64_MAX };
			CHECK(reserve(fixture.attachment, &plan.operation, &capacity, &refused_action, &refused).native_error == ERR_INVALID_PARAMETER);
			CHECK(refused.registry_instance == 0);
			CHECK(refused.handle_serial == 0);
			MutexLock lock(fixture.registry.mutex);
			ProductionAttachment *accepted = nullptr;
			RenderingDevice *device = nullptr;
			REQUIRE(fixture.registry.production_attachment_locked(fixture.attachment, true, accepted, device) == OK);
			for (uint32_t i = 0; i < accepted->quote.capacity.requests; ++i) {
				CHECK(accepted->requests[i].serial == 0);
			}
		}
		REQUIRE(reserve(fixture.attachment, &plan.operation, &plan.capacity, &action, &probe.raster_ref).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *device = nullptr;
			REQUIRE(fixture.registry.production_request_locked(probe.raster_ref, false, probe.raster, device) == OK);
			fixture.request = probe.raster; // Existing cleanup owns the actual issued cell on early return.
			REQUIRE(device == &fixture.rd);
			CHECK(probe.raster->attachment->serial == fixture.attachment.handle_serial);
			CHECK(probe.raster->raster_reserved);
			REQUIRE(probe.raster->action_count == 1);
			CHECK(probe.raster->actions[0].declaration.action_kind == GDEXTENSION_PRODUCTION_RASTER_FRAME_V1);
			CHECK_FALSE(probe.raster->actions[0].prepared);
			REQUIRE(probe.raster->resource_count == 4);
			for (uint32_t i = 0; i < probe.raster->resource_count; ++i) {
				CHECK_FALSE(probe.raster->resources[i].imported);
				CHECK(probe.raster->resources[i].use_count == 0);
			}
			CHECK(fixture.registry.production_freeze_locked(*probe.raster) == ERR_UNCONFIGURED);
			CHECK_FALSE(probe.raster->frozen);
		}
		const uint64_t next_control = probe.raster->next_control_serial;
		uint64_t refused_control = 0;
		CHECK(fixture.port.record_schedule(probe.raster_ref, &refused_control) == ERR_UNCONFIGURED);
		CHECK(refused_control == 0);
		CHECK(probe.raster->next_control_serial == next_control);
		CHECK(probe.raster->control.state == ProductionControlState::OWNER_CONSUMED);
		CHECK_FALSE(probe.raster->frozen);
		probe.rejected({});
		GDExtensionProductionCapacityV1 empty_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		auto empty_operation = plan.operation;
		++empty_operation.owner_request_nonce;
		GDExtensionProductionRequestRefV1 empty_ref{};
		REQUIRE(reserve_empty(fixture.attachment, &empty_operation, &empty_capacity, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &empty_ref).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *device = nullptr;
			REQUIRE(fixture.registry.production_request_locked(empty_ref, false, fixture.request, device) == OK);
			CHECK(fixture.request != probe.raster);
		}
		uint64_t control = 0;
		REQUIRE(fixture.port.record_schedule(empty_ref, &control) == OK);
		server->sync();
		REQUIRE(probe.calls == 1);
		GDExtensionProductionRecordReportV2 report{};
		REQUIRE(consume(empty_ref, control, &report).native_error == OK);
		probe.rejected(probe.expired); // Same actually-issued scalars after the original RECORD returned.
		CHECK(probe.raster->graph_count == 0);
		CHECK(probe.raster->native_count == 0);
		CHECK(fixture.registry.production_cancel(probe.raster_ref).native_error == OK);
		for (uint32_t turn = 0; turn < 8 && probe.raster->serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 released{};
			const Error error = Error(release(probe.raster_ref, &budget, &remaining, &released).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (probe.raster->serial) {
				server->sync();
			}
		}
		REQUIRE(probe.raster->serial == 0);
		probe.raster = nullptr; // The attachment arena is released by cleanup_owner below.
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}
	struct RasterJoinProbe {
		SingletonEnrollment *fixture = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRasterBeginV1 begin = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRasterEndV1 end = nullptr;
		GDExtensionInterfaceDeviceSessionProductionRasterPrepareReportV1 query = nullptr;
		bool automatic_end = false;
		uint32_t calls = 0;
		static void receive(void *opaque, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			auto &probe = *static_cast<RasterJoinProbe *>(opaque);
			++probe.calls;
			auto &request = *probe.fixture->request;
			CHECK(ref.handle_serial == request.serial);
			CHECK(ref.registry_instance == probe.fixture->instance);
			GDExtensionProductionRasterPrepareReportV1 prepared{};
			REQUIRE(probe.query(ref, &prepared).native_error == OK);
			REQUIRE(prepared.known_mask == 15);
			REQUIRE(probe.begin(context, ref, prepared.revision).native_error == OK);
			CHECK(request.raster_started);
			CHECK(request.draw_list_open);
			CHECK(probe.fixture->rd.draw_list.active);
			CHECK(request.next_action == 0);
			CHECK_FALSE(request.effects_pending);
			if (!probe.automatic_end) {
				REQUIRE(probe.end(context, ref).native_error == OK);
				CHECK_FALSE(request.draw_list_open);
				CHECK_FALSE(probe.fixture->rd.draw_list.active);
				CHECK(request.next_action == 1);
			}
		}
	};
	static void singleton_raster_prepare(uint32_t fail_stage, uint32_t quiesce_stage = 0, uint32_t record_mode = 0) {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto reserve = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterReserveV1>("device_session_production_raster_reserve_v1");
		const auto prepare = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterPrepareV1>("device_session_production_raster_prepare_v1");
		const auto query = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterPrepareReportV1>("device_session_production_raster_prepare_report_v1");
		const auto history_query = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1>("device_session_production_record_history_v1");
		const auto consume = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(reserve != nullptr);
		REQUIRE(prepare != nullptr);
		REQUIRE(query != nullptr);
		REQUIRE(history_query != nullptr);
		REQUIRE(consume != nullptr);
		REQUIRE(release != nullptr);
		Ref<UnitContainer> container;
		container.instantiate();
		container->initialize_raster();
		const Vector<uint8_t> binary = container->to_bytes();
		REQUIRE(!binary.is_empty());
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 1, 4, 4, 8, 2048, 0, 4, uint64_t(binary.size()), 0 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		RasterJoinProbe probe;
		probe.fixture = &fixture;
		probe.begin = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterBeginV1>("device_session_production_raster_begin_v1");
		probe.end = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRasterEndV1>("device_session_production_raster_end_v1");
		probe.query = query;
		probe.automatic_end = record_mode == 2;
		REQUIRE(probe.begin != nullptr);
		REQUIRE(probe.end != nullptr);
		REQUIRE(fixture.prepare_device(server) == OK);
		const ProductionRecordReceiver receiver = record_mode ? &RasterJoinProbe::receive : +[](void *, ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {};
		REQUIRE(fixture.initialize_owner(1, false, false, receiver, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan, nullptr, true) == OK);
		CHECK(fixture.qualification.handle_serial == 0);
		fixture.driver.production_raster_factory = true;
		fixture.driver.production_raster_join = record_mode != 0;
		fixture.driver.production_fail_stage = fail_stage;
		fixture.driver.production_quiesce_stage = quiesce_stage;
		fixture.driver.production_factory_probe_data = &fixture;
		fixture.driver.production_factory_probe = +[](void *opaque) {
			auto &owner = *static_cast<SingletonEnrollment *>(opaque);
			REQUIRE(owner.port.cancel != nullptr);
			CHECK(owner.port.cancel({ owner.instance, owner.request->serial }).native_error == ERR_BUSY);
			CHECK_FALSE(owner.request->cancelled);
			// This Core factory has no active DSO callback or binding holder.
			// Quiesce succeeds and seals admission; the separate physical close
			// gate still retains every original resource until native retirement.
			CHECK(owner.registry.begin_quiesce(owner.reference) == OK);
		};
		fixture.driver.raster_quiet = true;
		fixture.driver.raster_device = &fixture.rd;
		// Real RD factories and owner cells, with a CPU driver: no GPU draw claim.
		fixture.rd.draw_graph.initialize(&fixture.driver, &RenderingDevice::_render_pass_create_from_graph, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		// Match the existing empty CPU frame fixtures: an original frame has
		// a nonzero epoch before its native factories may enqueue retirement.
		// This establishes frame provenance, never a completion/free receipt.
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		if (record_mode) {
			fixture.driver.begin_segment(0, 0);
			fixture.rd.frames[0].primary_command_buffer_begun = fixture.driver.command_buffer_begin(fixture.rd.frames[0].command_buffer);
			REQUIRE(fixture.rd.frames[0].primary_command_buffer_begun);
		}
		const GDExtensionProductionRasterActionV1 action = { production_dto_abi<GDExtensionProductionRasterActionV1>(), 0, plan.operation.canonical_begin, 0, 1, 0, 0, 0, 0, 0 };
		GDExtensionProductionRequestRefV1 ref{};
		REQUIRE(reserve(fixture.attachment, &plan.operation, &plan.capacity, &action, &ref).native_error == OK);
		{
			MutexLock lock(fixture.registry.mutex);
			RenderingDevice *device = nullptr;
			REQUIRE(fixture.registry.production_request_locked(ref, false, fixture.request, device) == OK);
			REQUIRE(device == &fixture.rd);
		}
		auto &request = *fixture.request;
		fixture.driver.production_raster_request = &request;
		GDExtensionProductionRasterPrepareReportV1 report{};
		CHECK(query(ref, &report).native_error == ERR_DOES_NOT_EXIST);
		const GDExtensionProductionRasterPrepareV1 input = { production_dto_abi<GDExtensionProductionRasterPrepareV1>(), 17, 8, 4, uint64_t(binary.size()), 0 };
		Vector<uint8_t> oversized = binary;
		oversized.push_back(0);
		auto oversized_input = input;
		oversized_input.shader_bytes = oversized.size();
		const uint64_t before_admission = request.next_control_serial;
		CHECK(prepare(ref, &oversized_input, oversized.ptr(), oversized.size(), &report).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(request.next_control_serial == before_admission);
		CHECK(request.raster_prepare_control == 0);
		CHECK(request.raster_prepare_origin.create_attempt == 0);
		CHECK(request.payload_used == 0);
		CHECK_FALSE(request.native_effect_entered);
		CHECK_FALSE(request.append_effect_entered);
		CHECK(query(ref, &report).native_error == ERR_DOES_NOT_EXIST);
		REQUIRE(prepare(ref, &input, binary.ptr(), binary.size(), &report).native_error == ERR_BUSY);
		const uint64_t create_control = report.control_serial;
		REQUIRE(create_control != 0);
		CHECK(report.create_attempt == input.create_attempt);
		CHECK(report.request_serial == request.serial);
		// Read-only queries retain the original FIFO control without consuming it.
		const auto state_before_query = request.control.state;
		REQUIRE(query(ref, &report).native_error == OK);
		CHECK(request.control.state == state_before_query);
		CHECK(report.control_serial == create_control);
		CHECK(prepare(ref, &input, binary.ptr(), binary.size(), &report).native_error == ERR_BUSY);
		CHECK(report.control_serial == create_control);
		for (uint32_t mutation = 0; mutation < 3; ++mutation) {
			auto mismatched = input;
			Vector<uint8_t> changed = binary;
			if (mutation == 0) {
				++mismatched.create_attempt;
			} else if (mutation == 1) {
				++mismatched.width;
			} else {
				changed.write[changed.size() - 1] ^= 1;
			}
			CHECK(prepare(ref, &mismatched, changed.ptr(), changed.size(), &report).native_error == ERR_INVALID_DATA);
			CHECK(request.raster_prepare_control == create_control);
		}
		server->sync();
		REQUIRE(query(ref, &report).native_error == OK);
		CHECK(request.control.state == ProductionControlState::RETURNED);
		const Error expected = quiesce_stage ? ERR_UNAVAILABLE : fail_stage ? ERR_CANT_CREATE
																			: OK;
		CHECK(report.result.native_error == expected);
		const uint32_t prefixes[5] = { 15, 0, 2, 6, 7 };
		const uint32_t known = quiesce_stage ? (quiesce_stage == 1 ? 2u : 6u) : prefixes[fail_stage];
		CHECK(report.phase == (quiesce_stage ? quiesce_stage : fail_stage ? fail_stage
																		  : 5u));
		CHECK(report.known_mask == known);
		CHECK(report.returned_mask == 0);
		REQUIRE(prepare(ref, &input, binary.ptr(), binary.size(), &report).native_error == expected);
		CHECK(request.control.state == ProductionControlState::OWNER_CONSUMED);
		const uint64_t revision = report.revision;
		CHECK(prepare(ref, &input, binary.ptr(), binary.size(), &report).native_error == expected);
		CHECK(report.revision == revision);
		CHECK(report.control_serial == create_control);
		CHECK(request.actions[0].prepared == (expected == OK));
		CHECK(request.native_effect_entered);
		CHECK_FALSE(request.append_effect_entered);
		CHECK(request.graph_count == 0);
		CHECK(request.native_count == 0);
		for (uint32_t stage = 0; stage < 4; ++stage) {
			CHECK(fixture.driver.production_stage_attempts[stage] == ((quiesce_stage ? stage < quiesce_stage : !fail_stage || stage < fail_stage) ? 1u : 0u));
		}
		const uint64_t serials[4] = { report.framebuffer_serial, report.depth_serial, report.shader_serial, report.pipeline_serial };
		for (uint32_t i = 0; i < 4; ++i) {
			const auto &slot = request.resources[i];
			CHECK(slot.owned_serial == request.resources[0].owned_serial);
			CHECK(slot.owned_physical.bridge == fixture.attachment.handle_serial);
			CHECK(slot.owned_physical.child == slot.owned_serial);
			CHECK(slot.imported == bool(known & (1u << i)));
			CHECK(slot.use_count == ((known & (1u << i)) ? 1u : 0u));
			CHECK(serials[i] == ((known & (1u << i)) ? slot.owned_physical.serial : 0));
			for (uint32_t j = 0; j < i; ++j) {
				CHECK(slot.owned_physical.serial != request.resources[j].owned_physical.serial);
				CHECK(slot.retire_serial != request.resources[j].retire_serial);
			}
		}
		if (known & 2u) {
			const auto *depth = fixture.rd.texture_owner.get_or_null(request.resources[1].primary);
			REQUIRE(depth != nullptr);
			CHECK(depth->qualification_return == &request.resources[1].owned_physical);
		}
		if (known & 4u) {
			const auto *shader = fixture.rd.shader_owner.get_or_null(request.resources[2].primary);
			REQUIRE(shader != nullptr);
			CHECK(shader->qualification_return == &request.resources[2].owned_physical);
		}
		if (known & 1u) {
			const auto *framebuffer = fixture.rd.framebuffer_owner.get_or_null(request.resources[0].primary);
			REQUIRE(framebuffer != nullptr);
			CHECK(framebuffer->qualification_return == &request.resources[0].owned_physical);
			CHECK(framebuffer->qualification_dependency == &request.resources[1].owned_physical);
			CHECK(request.resources[1].owned_physical.dependent_children.load() == 1);
		}
		if (known & 8u) {
			const auto *pipeline = fixture.rd.render_pipeline_owner.get_or_null(request.resources[3].primary);
			REQUIRE(pipeline != nullptr);
			CHECK(pipeline->qualification_return == &request.resources[3].owned_physical);
			CHECK(pipeline->qualification_dependency == &request.resources[2].owned_physical);
			CHECK(request.resources[2].owned_physical.dependent_children.load() == 1);
		}
		if (expected == OK) {
			// Each original RECORD sees the earlier native preparation; the join
			// variants additionally append begin/end through their actual receiver.
			uint64_t control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 recorded{};
			REQUIRE(consume(ref, control, &recorded).native_error == OK);
			GDExtensionProductionRecordHistoryV1 history{};
			REQUIRE(history_query(ref, control, &history).native_error == OK);
			if (record_mode) {
				CHECK(probe.calls == 1);
				CHECK(request.next_action == 1);
				CHECK_FALSE(request.draw_list_open);
				CHECK_FALSE(fixture.rd.draw_list.active);
				CHECK_FALSE(request.effects_pending);
				// This driver needs no initial texture clear (kind 1): preserve
				// both state instructions, begin, clear, and the closed list.
				REQUIRE(request.graph_count == 5);
				CHECK(request.native_count == 0); // Original issue happens in OBSERVE.
				CHECK(fixture.driver.production_render_begins == 0);
				CHECK(fixture.driver.production_framebuffer_creates == 0);
				CHECK(fixture.driver.issues == 0);
				uint32_t kinds[8] = {};
				for (uint32_t g = 0; g < request.graph_count; ++g) {
					const auto &graph = request.graphs[g];
					CHECK(graph.pending);
					CHECK(graph.handle.is_valid());
					REQUIRE(graph.handle.kind < 8);
					++kinds[graph.handle.kind];
					CHECK(graph.fact.action_ordinal == 0);
					CHECK(graph.fact.canonical_suboperation == plan.operation.canonical_begin);
					CHECK(graph.fact.graph_kind == graph.handle.kind);
					CHECK(graph.fact.graph_instance == graph.handle.graph_instance);
					CHECK(graph.fact.graph_frame == graph.handle.graph_frame);
					CHECK(graph.fact.graph_index == graph.handle.index);
					CHECK(graph.fact.graph_offset == graph.handle.offset);
				}
				const uint32_t expected_kinds[8] = { 0, 0, 0, 2, 0, 1, 1, 1 };
				for (uint32_t kind = 0; kind < 8; ++kind) {
					CHECK(kinds[kind] == expected_kinds[kind]);
				}
				REQUIRE(recorded.segment.segment != 0);
				{
					CallbackScope owner_entry(fixture.reference);
					REQUIRE(owner_entry.get_error() == OK);
					REQUIRE(fixture.port.ack_segment(ref, recorded.segment.segment, recorded.segment.segment_revision).native_error == OK);
				}
				record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1,
						GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1);
			} else {
				record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1,
						GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1);
			}
		}
		CHECK(fixture.registry.production_cancel(ref).native_error == OK);
		CHECK(fixture.driver.production_depth_frees == 0);
		CHECK(fixture.driver.raster_shader_frees == 0);
		CHECK(fixture.driver.raster_frees == 0);
		fixture.driver.production_shader_not_ready = (known & 4u) ? 2u : 0u;
		uint32_t returned = 0;
		for (uint32_t turn = 0; turn < 192 && (returned != known || request.maintenance_serial); ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			const uint32_t frees_before = fixture.driver.production_free_order.size();
			ProductionControlOutput output{};
			uint64_t serial = 0;
			REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
			REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
			server->sync();
			REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
			CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY));
			CHECK(output.service_usage.passes <= 1);
			CHECK(output.service_usage.receipts <= 1);
			CHECK(fixture.driver.production_free_order.size() - frees_before <= 1);
			REQUIRE(query(ref, &report).native_error == OK);
			CHECK(report.control_serial == create_control);
			CHECK(report.known_mask == known);
			CHECK((report.returned_mask & returned) == returned);
			CHECK((report.returned_mask & ~known) == 0);
			returned = report.returned_mask;
			if (fixture.driver.production_shader_ready_polls && fixture.driver.production_shader_ready_polls <= 2) {
				CHECK_FALSE(request.raster_shader_retire_ready);
				CHECK((returned & 4u) == 0);
				CHECK(fixture.driver.raster_shader_frees == 0);
				CHECK_FALSE(request.resources[2].owned_physical.enqueued);
			}
			for (uint32_t i = 0; i < 4; ++i) {
				if (returned & (1u << i)) {
					CHECK(request.resources[i].owned_physical.free_event != 0);
					CHECK(request.resources[i].use_count == 0);
				}
			}
		}
		CHECK(returned == known);
		CHECK(fixture.driver.production_shader_not_ready == 0);
		if (known & 4u) {
			CHECK(fixture.driver.production_shader_ready_polls >= 3);
		} else {
			CHECK(fixture.driver.production_shader_ready_polls == 0);
		}
		CHECK(request.maintenance_serial == 0);
		CHECK(fixture.driver.production_depth_frees == ((known & 2u) ? 1u : 0u));
		CHECK(fixture.driver.raster_shader_frees == ((known & 4u) ? 1u : 0u));
		CHECK(fixture.driver.raster_frees == ((known & 8u) ? 1u : 0u));
		CHECK_FALSE(fixture.driver.production_depth_live);
		CHECK_FALSE(fixture.driver.raster_live);
		CHECK(request.native_effect_entered);
		CHECK(request.append_effect_entered == (record_mode != 0));
		if (record_mode) {
			CHECK(fixture.driver.production_render_begins == 1);
			CHECK(fixture.driver.production_render_ends == 1);
			CHECK(fixture.driver.production_viewports == 1);
			CHECK(fixture.driver.production_scissors == 1);
			CHECK(fixture.driver.production_depth_clears >= 1);
			CHECK(fixture.driver.production_framebuffer_creates == 1);
			CHECK(fixture.driver.production_framebuffer_frees == 1);
			CHECK_FALSE(fixture.driver.production_framebuffer_live);
			CHECK(fixture.driver.issues > 0);
			CHECK(fixture.driver.polls > 0);
			CHECK(fixture.driver.retains > 0);
			CHECK(fixture.driver.releases < fixture.driver.retains); // Original owner refs survive until final release.
			REQUIRE(request.native_count >= request.graph_count);
			for (uint32_t g = 0; g < request.graph_count; ++g) {
				CHECK_FALSE(request.graphs[g].pending);
			}
			bool covered[8] = {};
			for (uint32_t n = 0; n < request.native_count; ++n) {
				const auto &native = request.natives[n];
				REQUIRE(native.fact.membership_parent < request.graph_count);
				covered[native.fact.membership_parent] = true;
				const auto &parent = request.graphs[native.fact.membership_parent];
				CHECK(native.fact.action_ordinal == 0);
				CHECK(native.fact.canonical_suboperation == plan.operation.canonical_begin);
				CHECK(native.fact.graph_kind == parent.handle.kind);
				CHECK(native.fact.graph_index == parent.handle.index);
				CHECK(native.fact.graph_offset == parent.handle.offset);
				REQUIRE(native.owner_index < request.native_count);
				CHECK(native.member.source.issue == RDD::IssueFact::KnownAccepted);
				CHECK(native.member.source.serial != 0);
				CHECK(request.natives[native.owner_index].observation.state == NativeSourceObservation::State::Complete);
			}
			for (uint32_t g = 0; g < request.graph_count; ++g) {
				CHECK(covered[g]); // Preserve every original graph-to-source alias.
			}
		}
		CHECK(fixture.qualification.handle_serial == 0);
		// Framebuffer format passes belong to the RD cache, not the four roots.
		for (auto &entry : fixture.rd.framebuffer_formats) {
			fixture.driver.render_pass_free(entry.value.render_pass);
		}
		fixture.rd.framebuffer_formats.clear();
		fixture.rd.framebuffer_format_cache.clear();
		for (uint32_t turn = 0; turn < 32 && request.serial; ++turn) {
			const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 released{};
			const Error error = Error(release(ref, &budget, &remaining, &released).native_error);
			CHECK((error == OK || error == ERR_BUSY));
			if (request.serial) {
				server->sync();
			}
		}
		CHECK(request.serial == 0);
		if (record_mode) {
			CHECK(fixture.driver.releases == fixture.driver.retains);
			CHECK(fixture.driver.production_render_pass_frees == fixture.driver.production_render_pass_creates);
		}
		fixture.driver.production_raster_request = nullptr;
		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
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
		bool integrated_unknown = false, restore_unknown = false;
		NativeDisposalWitness::Report poisoned;
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &test = *static_cast<OwnedStorageProbe *>(userdata);
			auto &fixture = *test.fixture;
			auto &native = *test.native;
			++test.calls;
			if (test.restore_unknown) {
				REQUIRE(RendererRD::external_storage_restore_poison(fixture.rd, native.storage_witness(2), test.poisoned) == OK);
				CHECK(fixture.rd._production_output_owned_storage_contains(native.pin(0), native.storage_group(), native.storage_rid(2)));
				return;
			}
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
			if (test.integrated_unknown) {
				REQUIRE(RendererRD::external_storage_poison_and_free(fixture.rd, native.storage_witness(2), test.poisoned) == ERR_INVALID_DATA);
				CHECK(test.poisoned.rid == native.storage_rid(2).get_id());
				CHECK(test.poisoned.serial != 0);
				CHECK_FALSE(fixture.rd._production_output_owned_storage_contains(native.pin(0), native.storage_group(), native.storage_rid(2)));
				CHECK(fixture.driver.output_storage_frees == 0);
			}
		}
	};
	static void owned_storage_observe(uint32_t capacity, uint32_t fail_at = 0, bool insufficient = false, bool invalid_extent = false, bool foreign_before_allocate = false, bool integrated_unknown = false) {
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
		probe.integrated_unknown = integrated_unknown;
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
		if (integrated_unknown) {
			const uint64_t group = native.storage_group();
			const auto next_group = request.next_disposal_group_serial;
			void *const arena = request.attachment->arena;
			const auto children = fixture.holder.production_children;
			const auto attempts = fixture.driver.output_storage_attempts;
			RID resources[7];
			bool live[7];
			for (uint32_t role = 0; role < 7; ++role) {
				resources[role] = native.storage_rid(role);
				live[role] = fixture.driver.output_storage_live[role];
			}
			uint32_t mismatch_turns = 0;
			for (uint32_t turn = 0; turn < 32 && mismatch_turns < 2; ++turn) {
				const GDExtensionProductionServiceBudgetV1 budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
				ProductionControlOutput output{};
				uint64_t serial = 0;
				REQUIRE(fixture.registry.production_control_accept(ref, ProductionControlKind::OBSERVE, &serial, &budget) == OK);
				REQUIRE(fixture.registry.production_control_schedule(ref, serial) == OK);
				server->sync();
				REQUIRE(fixture.registry.production_control_consume(ref, serial, &output) == OK);
				CHECK((output.result.native_error == OK || output.result.native_error == ERR_BUSY || output.result.native_error == ERR_INVALID_DATA));
				mismatch_turns += output.result.native_error == ERR_INVALID_DATA;
				CHECK_FALSE(output.output_generation.native_released);
				CHECK(output.service_usage.passes <= 1);
				CHECK(output.service_usage.receipts <= 1);
				CHECK(native.storage_group() == group);
				CHECK_FALSE(native.storage_empty());
				CHECK_FALSE(native.released());
				CHECK(fixture.registry.production_output_pin_matches(native.pin(0), ref, fixture.rd));
				CHECK(request.next_disposal_group_serial == next_group);
				CHECK(request.attachment->arena == arena);
				CHECK(fixture.holder.production_children == children);
				CHECK(children != 0);
				CHECK(fixture.driver.output_storage_attempts == attempts);
				CHECK(fixture.driver.output_storage_frees == 0);
				for (uint32_t role = 0; role < 7; ++role) {
					CHECK(native.storage_rid(role) == resources[role]);
					CHECK(fixture.driver.output_storage_live[role] == live[role]);
				}
			}
			CHECK(mismatch_turns == 2);
			// Re-enter the real original RT solely for exact test injection undo.
			// No production recovery API, membership repair or custody reset.
			probe.restore_unknown = true;
			uint64_t restore_control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &restore_control) == OK);
			server->sync();
			GDExtensionProductionRecordReportV2 restore_report{};
			REQUIRE(consume(ref, restore_control, &restore_report).native_error == OK);
			CHECK(probe.calls == 2);
			CHECK(native.storage_group() == group);
			CHECK(fixture.driver.output_storage_frees == 0);
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
		bool generation_root = false, history_empty_turn = false;
		// 0: pipeline success, real material capacity failure; 1: material borrow;
		// 2/3: driver failure (ordinary/quiet); 4: original Core capacity failure.
		static void receive(void *userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionOperationV1 *) {
			using namespace GDExtensionDeviceExit;
			auto &test = *static_cast<PipelinePrefixProbe *>(userdata);
			auto &fixture = *test.fixture;
			auto &rd = fixture.rd;
			auto &native = *test.native;
			++test.calls;
			if (test.history_empty_turn) {
				return; // A later real RECORD intentionally performs no factory attempt.
			}
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
	static void pipeline_prefix_observe(uint32_t mode, bool generation_root = false, bool check_history = false) {
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
		if (check_history) {
			const auto query = ordinal_port<GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1>("device_session_production_record_history_v1");
			REQUIRE(query != nullptr);
			GDExtensionProductionRecordHistoryV1 history{};
			REQUIRE(query(ref, control, &history).native_error == OK);
			const uint32_t entered = GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1;
			const uint32_t returned_flags = GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1;
			record_history_flags(history, returned_flags | (mode == 4 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1) : entered),
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 | (mode == 4 ? entered : uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1)));
			record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1,
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1);
			// These factory attempts have no graph/native fact rows. Zero counts
			// cannot erase attempted work, including a refused driver create.
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
			probe.history_empty_turn = true;
			const auto attempts = fixture.driver.raster_attempts;
			uint64_t empty_control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &empty_control) == OK);
			REQUIRE(empty_control != control);
			server->sync();
			REQUIRE(probe.calls == 2);
			GDExtensionProductionRecordReportV2 empty_return{};
			REQUIRE(consume(ref, empty_control, &empty_return).native_error == OK);
			REQUIRE(query(ref, empty_control, &history).native_error == OK);
			record_history_flags(history, returned_flags | (mode == 4 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1) : uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1)),
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 | (mode == 4 ? uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1) : uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1)));
			CHECK(fixture.driver.raster_attempts == attempts);
			record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1,
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1);
			CHECK(fixture.registry.production_cancel(ref).native_error == OK);
			REQUIRE(query(ref, empty_control, &history).native_error == OK);
			record_history_flags(history, GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1 | (mode == 4 ? 0u : uint32_t(GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1)),
					GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 | GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1);
			CHECK(fixture.driver.raster_frees == 0); // No-append never returns native factory ownership.
			if (mode == 0) {
				CHECK(native.pipeline_root().is_valid());
				CHECK(fixture.rd.render_pipeline_is_valid(native.pipeline_root()));
				CHECK(fixture.driver.raster_live);
			}
		}
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
		bool scene_query = false;
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
		bool terminal_retry = false;

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
			if (probe.terminal_retry) {
				REQUIRE(ref.handle_serial == request.serial);
				REQUIRE(native.released());
				REQUIRE(native.generation_pin().is_empty());
				const auto withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
				REQUIRE(withdraw != nullptr);
				GDExtensionOutputGenerationInputV1 input{};
				input.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
				input.particles = native.particle_rid().get_id();
				input.generation = native.generation;
				auto wrong = context;
				++wrong.frame_serial;
				OutputLeafProbe::status(withdraw(wrong, ref, &input), ERR_UNAUTHORIZED, "original_record_context_required");
				OutputLeafProbe::status(withdraw(context, { ref.registry_instance, UINT64_MAX }, &input), ERR_UNAUTHORIZED, "original_record_context_required");
				auto stale = input;
				stale.particles = 0;
				OutputLeafProbe::status(withdraw(context, ref, &stale), ERR_INVALID_PARAMETER, "stale_generation");
				stale = input;
				++stale.generation;
				OutputLeafProbe::status(withdraw(context, ref, &stale), ERR_INVALID_PARAMETER, "stale_generation");
				OutputLeafProbe::status(withdraw(context, ref, &input), OK, "");
				OutputLeafProbe::status(withdraw(context, ref, &input), OK, "");
				CHECK(native.released());
				CHECK(native.generation_pin().is_empty());
				CHECK(native.generation_serial() == probe.root_serial);
				return;
			}
			if (ref.handle_serial != request.serial) {
				// A second accepted request in the SAME attachment has its own
				// genuine protected RECORD, but no successor-to-generation grant.
				const auto next = request.next_output_pin_serial;
				const auto head = request.output_owner_head, cursor = request.output_owner_cursor;
				CHECK(fixture.registry.production_output_owner_bind(native.generation_pin(), fixture.rd, native.particle_rid(), native.generation, UINT32_MAX, 0, ProductionOutputOwnerKind::GENERATION) == ERR_UNAUTHORIZED);
				CHECK_FALSE(fixture.rd._production_output_owner_matches(ProductionOutputOwnerKind::GENERATION, native.particle_rid(), native.generation, UINT32_MAX, 0, native.generation_pin(), ref));
				const auto withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
				REQUIRE(withdraw != nullptr);
				GDExtensionOutputGenerationInputV1 input{};
				input.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
				input.particles = native.particle_rid().get_id();
				input.generation = native.generation;
				OutputLeafProbe::status(withdraw(context, ref, &input), ERR_UNAUTHORIZED, "original_output_generation_owner_required");
				probe.check_root_unchanged(next, head, cursor);
				CHECK(native.withdrawing());
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
			if (probe.scene_query) {
				CHECK(RendererRD::ParticlesStorage::get_singleton()->particles_scene_validate_receiver(
						native.particle_rid(), native.generation, &fixture.rd) == ERR_UNAVAILABLE);
			}
			REQUIRE(native.install_generation(context, ref, fixture.registry) == OK);
			// Original Registry-issued root, original device and configuration.
			// This tests receiver owner data admission, not Scene phase permission.
			auto *storage = RendererRD::ParticlesStorage::get_singleton();
			REQUIRE(storage != nullptr);
			CHECK(storage->particles_scene_validate_receiver(native.particle_rid(), native.generation, &fixture.rd) ==
					(probe.scene_query ? OK : ERR_INVALID_DATA));
			CHECK(storage->particles_scene_validate_receiver(native.particle_rid(), native.generation + 1, &fixture.rd) == ERR_INVALID_DATA);
			CHECK(storage->particles_scene_validate_receiver(native.particle_rid(), native.generation, nullptr) == ERR_INVALID_PARAMETER);
			{
				RenderingDevice foreign;
				CHECK(storage->particles_scene_validate_receiver(native.particle_rid(), native.generation, &foreign) == ERR_INVALID_DATA);
			}
			CHECK(storage->particles_scene_validate_receiver(RID(), native.generation, &fixture.rd) == ERR_INVALID_PARAMETER);
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
			// The original root authenticates cleanup once all fixture preparation
			// is complete. Withdrawal then closes new work for this generation.
			const auto withdraw = reinterpret_cast<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>(gdextension_get_guarded_proc_address("device_session_output_withdraw_v1"));
			REQUIRE(withdraw != nullptr);
			GDExtensionOutputGenerationInputV1 input{};
			input.abi = production_dto_abi<GDExtensionOutputGenerationInputV1>();
			input.particles = native.particle_rid().get_id();
			input.generation = native.generation;
			CHECK(fixture.registry.production_output_generation_cleanup_context(context, ref, native.generation_pin(), fixture.rd, RID(), native.generation) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_generation_cleanup_context(context, ref, native.generation_pin(), fixture.rd, native.particle_rid(), native.generation + 1) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_generation_cleanup_context(context, { ref.registry_instance, UINT64_MAX }, native.generation_pin(), fixture.rd, native.particle_rid(), native.generation) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_generation_cleanup_context(wrong, ref, native.generation_pin(), fixture.rd, native.particle_rid(), native.generation) == ERR_UNAUTHORIZED);
			CHECK(fixture.registry.production_output_generation_cleanup_context(context, ref, native.generation_pin(), fixture.rd, native.particle_rid(), native.generation) == OK);
			for (uint32_t i = 0; i < probe.slots; ++i) {
				native.install(i, fixture.registry, ref);
				REQUIRE(fixture.registry.production_output_pin_current(fixture.rd, native.pin(i)) == OK);
				REQUIRE(fixture.registry.production_output_owner_bind(native.pin(i), fixture.rd, native.particle_rid(), native.generation, i, native.lease(i)) == OK);
				CHECK(fixture.registry.production_output_generation_cleanup_context(context, ref, native.pin(i), fixture.rd, native.particle_rid(), native.generation) == ERR_UNAUTHORIZED);
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
			auto foreign_rid = input;
			foreign_rid.particles = 0;
			OutputLeafProbe::status(withdraw(context, ref, &foreign_rid), ERR_INVALID_PARAMETER, "stale_generation");
			auto foreign_generation = input;
			++foreign_generation.generation;
			OutputLeafProbe::status(withdraw(context, ref, &foreign_generation), ERR_INVALID_PARAMETER, "stale_generation");
			CHECK_FALSE(native.withdrawing());
			OutputLeafProbe::status(withdraw(context, ref, &input), OK, "");
			OutputLeafProbe::status(withdraw(context, ref, &input), OK, "");
			CHECK(native.withdrawing());
		}
	};

	static void generation_root_observe(uint32_t slots, bool pressure = false, bool directory = false, bool root_group = false, bool directory_hold = false, bool scene_query = false) {
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
		probe.scene_query = scene_query;
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
		RendererRD::ExternalLinkedOwnerTest native(fixture.rd, false, scene_query);
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

		// The original public withdrawal already ran inside protected RECORD.
		CHECK(RendererRD::ParticlesStorage::get_singleton()->particles_scene_validate_receiver(
				native.particle_rid(), native.generation, &fixture.rd) == ERR_UNAVAILABLE);
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
		if (!slots && !pressure && !directory && !root_group) {
			probe.terminal_retry = true;
			uint64_t retry_control = 0;
			REQUIRE(fixture.port.record_schedule(ref, &retry_control) == OK);
			server->sync();
			CHECK(probe.calls == 3);
			GDExtensionProductionRecordReportV2 retried{};
			REQUIRE(consume_record(ref, retry_control, &retried).native_error == OK);
			CHECK(native.released());
			CHECK(native.generation_pin().is_empty());
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
	static void output_alias_readback_freeze() {
		using namespace GDExtensionDeviceExit;
		auto *server = ordinal_server();
		if (!server) {
			return;
		}
		const auto prepare = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1>("device_session_production_prepare_output_v1");
		const auto prepare_read = ordinal_port<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		const auto release = ordinal_port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		REQUIRE(prepare);
		REQUIRE(prepare_read);
		REQUIRE(release);
		OrdinalPlan plan;
		plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 2, 2, 3, 8, 2048, 1, 2, 0, 4 };
		plan.action_count = plan.resource_count = 2;
		plan.use_count = 3;
		plan.readback_count = 1;
		plan.actions[0] = { 0, 11, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 2, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
		plan.actions[1] = { 1, 11, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 2, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
		plan.resources[0] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 };
		plan.resources[1] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, 3, 16, 0 };
		plan.uses[0] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 16, 0 };
		plan.uses[1] = { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 16, 0 };
		plan.uses[2] = { 1, GDEXTENSION_PRODUCTION_READ_V1, 4, 4, 0 };
		plan.readback = { 1, 0, 0, 1, 4, 4 };
		SingletonEnrollment fixture;
		fixture.public_port = true;
		REQUIRE(ordinal_initialize(fixture, server, plan) == OK);
		fixture.driver.output_storage_factory = true;
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		fixture.rd.frames_drawn = 3;
		auto &request = *fixture.request;
		const GDExtensionProductionRequestRefV1 ref{ fixture.instance, request.serial };
		const GDExtensionStorageCreateRequestV1 origin{ Enrollment::ABI, 16, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 backing{};
		REQUIRE(fixture.port.storage_create(ref, 0, &origin, &backing).native_error == ERR_BUSY);
		server->sync();
		REQUIRE(fixture.port.storage_create(ref, 0, &origin, &backing).native_error == OK);
		GDExtensionProductionOutputPrepareV1 prepared{};
		prepared.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		prepared.action_ordinal = 0;
		prepared.canonical_suboperation = 11;
		prepared.source_recipe_substep = 7;
		prepared.source_resource_slot = prepared.source_use_index = prepared.first_source_use = 0;
		prepared.source_use_count = prepared.source_generation = 1;
		prepared.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		// Schema-only metadata identity: no Create/acquire or usable RID is claimed.
		prepared.input.particles = prepared.input.common.generation = 1;
		prepared.input.common.current_camera = { 1, 1, 1 };
		for (uint32_t axis = 0; axis < 3; ++axis) {
			prepared.input.common.bounds.size[axis] = 1;
		}
		prepared.roles[0] = { 1, 1, 1, 0, 3, 16 };
		prepared.roles[1] = { 2, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		prepared.roles[2] = { 7, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		REQUIRE(prepare(ref, &prepared).native_error == OK);
		REQUIRE(prepare_read(ref, 1, 0).native_error == OK);
		{
			MutexLock rd_lock(fixture.rd._thread_safe_);
			MutexLock lock(fixture.registry.mutex);
			auto &source = request.resources[0];
			CHECK(fixture.registry.production_output_source_matches_locked(request, request.actions[0], fixture.rd));
			const uint64_t serial = source.backing_serial;
			++source.backing_serial;
			CHECK_FALSE(fixture.registry.production_output_source_matches_locked(request, request.actions[0], fixture.rd));
			source.backing_serial = serial;
			const uint64_t generation = request.actions[0].output_source_generation;
			++request.actions[0].output_source_generation;
			CHECK_FALSE(fixture.registry.production_output_source_matches_locked(request, request.actions[0], fixture.rd));
			request.actions[0].output_source_generation = generation;
			request.uses[0].declaration.access = GDEXTENSION_PRODUCTION_WRITE_V1;
			CHECK_FALSE(fixture.registry.production_output_source_matches_locked(request, request.actions[0], fixture.rd));
			request.uses[0].declaration.access = GDEXTENSION_PRODUCTION_READ_V1;
			CHECK(fixture.registry.production_output_source_matches_locked(request, request.actions[0], fixture.rd));
			// An unbound plan is not an actual witness/root, even after lawful prepare.
			CHECK_FALSE(fixture.registry.production_output_alias_matches_locked(request, request.resources[1], fixture.rd));
		}
		const uint32_t attempts = fixture.driver.output_storage_attempts;
		// Negative-only corruption probes operate on real accepted/prepared rows.
		// None installs a witness, changes action progress or manufactures readiness.
		for (uint32_t mutation = 0; mutation <= 10; ++mutation) {
			MutexLock lock(fixture.registry.mutex);
			auto &writer = request.actions[0];
			auto &copy = request.actions[1];
			auto &read = request.readbacks[0].declaration;
			const auto old_writer = writer.declaration, old_copy = copy.declaration;
			const auto old_role = writer.output_roles[0];
			const auto old_read = read;
			const auto old_use = request.uses[2].declaration;
			const uint32_t old_copy_slot = copy.readback_slot;
			const bool old_source_prepared = writer.output_source_prepared;
			switch (mutation) {
				case 0:
					++writer.output_roles[0].generation;
					break;
				case 1:
					--writer.output_roles[0].bytes;
					break;
				case 2:
					writer.output_roles[0].use_index = 0;
					break;
				case 3:
					writer.declaration.action_kind = GDEXTENSION_PRODUCTION_DISPATCH_V1;
					break;
				case 4:
					writer.output_source_prepared = false;
					break;
				case 5:
					copy.readback_slot = UINT32_MAX;
					break;
				case 6:
					copy.declaration.readback_slot = UINT32_MAX;
					break;
				case 7:
					request.uses[2].declaration.access = GDEXTENSION_PRODUCTION_WRITE_V1;
					break;
				case 8:
					read.byte_offset = 16;
					break;
				case 9:
					read.producer_action_ordinal = 1;
					break;
				case 10:
					break; // Valid unbound plan freezes without a fabricated physical cell.
			}
			const Error error = fixture.registry.production_freeze_locked(request);
			CHECK_MESSAGE(error == (mutation == 10 ? OK : ERR_INVALID_DATA), "alias freeze mutation=", mutation);
			CHECK(request.frozen == (mutation == 10));
			CHECK(request.next_action == 0);
			CHECK(request.graph_count == 0);
			CHECK(request.native_count == 0);
			CHECK(request.readbacks[0].source.issued == 0);
			CHECK(request.readbacks[0].source.returned == 0);
			NativeDisposalWitness::Report witness_report;
			CHECK(fixture.rd._native_disposal_observe(request.resources[1].output_witness, witness_report) == ERR_UNAUTHORIZED);
			CHECK(witness_report.state == NativeDisposalWitness::State::EMPTY);
			CHECK_FALSE(request.resources[1].imported);
			writer.declaration = old_writer;
			copy.declaration = old_copy;
			writer.output_roles[0] = old_role;
			read = old_read;
			request.uses[2].declaration = old_use;
			copy.readback_slot = old_copy_slot;
			writer.output_source_prepared = old_source_prepared;
		}
		CHECK(fixture.driver.output_storage_attempts == attempts);
		CHECK(fixture.driver.issues == 0);
		// No alias ever bound; return the real source allocation through original custody.
		REQUIRE(fixture.port.cancel(ref).native_error == OK);
		const GDExtensionStorageRetireRequestV1 retirement{ Enrollment::ABI, backing.backing, backing.rid, backing.backing_serial, 0, 0, 1, 1, 0 };
		GDExtensionStorageRetireRefV1 retired{};
		GDExtensionStorageRetireReportV1 returned{};
		REQUIRE(fixture.port.storage_retire(ref, 0, &retirement, &retired, &returned).native_error == OK);
		GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), 0, 1, 1, 1, 0 };
		for (uint32_t turn = 0; turn < 64 && !returned.state; ++turn) {
			budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			const Error e = Error(fixture.port.storage_observe(ref, retired, &budget, &returned).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
		}
		REQUIRE(returned.state == 1);
		REQUIRE(fixture.port.storage_release(ref, retired).native_error == OK);
		for (uint32_t turn = 0; turn < 64 && request.serial; ++turn) {
			budget.deadline_micros = OS::get_singleton()->get_ticks_usec() + 10000000;
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			const Error e = Error(release(ref, &budget, &remaining, &report).native_error);
			CHECK((e == OK || e == ERR_BUSY));
			server->sync();
		}
		REQUIRE(request.serial == 0);
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
	// Runtime-only test bridge: controls always use the original queued FIFO.
	struct ManagedSceneHarness {
		SingletonEnrollment enrollment;
		RendererRD::ExternalLinkedOwnerTest *native = nullptr;
		RenderingDevice *device = nullptr;
		Dictionary checks;
		GDExtensionProductionRequestRefV1 ref{};
		uint64_t control = 0;
		uint32_t stage = 0;
		Error record_error = ERR_UNAVAILABLE;
		bool root_returned = false;
		bool phase_mode = false;
		bool viewport_mode = false;
		RID viewport_scenario, viewport_receiver, viewport_source, viewport_collision;
		RendererSceneCull::SceneSphereSnapshot viewport_snapshot;
		Callable phase_callable;
		GDExtensionScenePhaseRegistrationRefV1 phase_registration{};
		GDExtensionProductionRequestRefV1 waiting_request{};
		uint64_t waiting_control = 0;
		bool waiting_close_started = false;
		GDExtensionSceneOpaque32V1 phase_capability{};
		GDExtensionProductionPhysicalContextRefV1 phase_context{};
		uint32_t phase_calls = 0, ordinary_calls = 0, legacy_calls = 0, callable_frees = 0;
		GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1 unregister_phase = nullptr;
		static void ordinary(void *data, const GDExtensionConstVariantPtr *, GDExtensionInt,
				GDExtensionVariantPtr returned, GDExtensionCallError *error) {
			++static_cast<ManagedSceneHarness *>(data)->ordinary_calls;
			*reinterpret_cast<Variant *>(returned) = Variant();*error = { GDEXTENSION_CALL_OK, 0, 0 };
		}
		static void free_callable(void *data) { ++static_cast<ManagedSceneHarness *>(data)->callable_frees; }
		static void typed(void *data, const GDExtensionNativeScenePhaseV1 *phase,
				GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOperationV1 *operation) {
			using namespace GDExtensionDeviceExit;
			auto &self = *static_cast<ManagedSceneHarness *>(data);
			if (!phase || !operation) {
				self.check("managed_phase_callback_payload", false);
				return;
			}
			if (self.phase_calls != 0) {
				self.check("managed_phase_single_delivery", false);
				return;
			}
			++self.phase_calls;
			self.phase_context = { NativeScenePhaseProbe::word(phase->physical_context, 0), NativeScenePhaseProbe::word(phase->physical_context, 1) };
			self.check("managed_phase_original_occurrence", phase->occurrence != 0 && phase->expiry_serial == self.phase_context.frame_serial);
			self.check("managed_phase_original_device", memcmp(&phase->device_capability, &self.phase_capability, sizeof(self.phase_capability)) == 0);
			{
				MutexLock lock(self.enrollment.registry.mutex);
				ProductionRequest *original = nullptr;RenderingDevice *device = nullptr;
				const Error live = self.enrollment.registry.production_physical_request_locked(self.phase_context, request, original, device, false);
				self.check("managed_phase_live_original_frame", live == OK && original == self.enrollment.request && device == self.device &&
						original->control.state == ProductionControlState::EXECUTING && operation->owner_request_nonce == original->operation.owner_request_nonce);
			}
			self.record_error = self.native->install_generation(self.phase_context, request, self.enrollment.registry);
			if (self.record_error == OK) {
				if (self.viewport_mode) self.exercise_viewport_receiver();
				else self.exercise_scene();
			}
		}
		Error install_phase() {
			using namespace GDExtensionDeviceExit;
			// Same original adopted Callable enrollment as D's native phase fixture.
			{
				MutexLock lock(enrollment.registry.mutex);
				enrollment.registry.records[enrollment.reference.slot].guard.entry_classes |= 1u << 8;
			}
			GDExtensionCallableCustomInfo2 info{};info.callable_userdata = this;
			info.call_func = &ordinary;info.free_func = &free_callable;
			info.token = enrollment.registry.records[enrollment.reference.slot].original_owner;
			const auto create = reinterpret_cast<GDExtensionInterfaceCallableCustomCreate2>(GDExtension::get_interface_function("callable_custom_create2"));
			CRASH_COND(!create);
			create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&phase_callable), &info);
			const auto relay = reinterpret_cast<GDExtensionInterfaceDeviceSessionScenePhaseRelayV1>(gdextension_get_guarded_proc_address("device_session_scene_phase_relay_v1"));
			const auto capability = reinterpret_cast<GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1>(gdextension_get_guarded_proc_address("device_session_scene_phase_capability_v1"));
			const auto registration = reinterpret_cast<GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1>(gdextension_get_guarded_proc_address("device_session_scene_phase_register_v1"));
			unregister_phase = reinterpret_cast<GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1>(gdextension_get_guarded_proc_address("device_session_scene_phase_unregister_v1"));
			CRASH_COND(!relay || !capability || !registration || !unregister_phase);
			GDExtensionNormalRelayRefV1 wire{};
			Error error = Error(relay(reinterpret_cast<GDExtensionConstTypePtr>(&phase_callable), &typed, &wire).native_error);
			if (error != OK) return error;
			GDExtensionScenePhaseSinkConfigV1 config{};config.abi = { 1, sizeof(config), 0 };
			config.phase = config.max_operations = 1;
			error = Error(capability(enrollment.attachment, &config.device_capability).native_error);
			if (error != OK) return error;
			phase_capability = config.device_capability;
			return Error(registration(wire, enrollment.attachment, &config, &phase_registration).native_error);
		}
		void close_phase() {
			using namespace GDExtensionDeviceExit;
			check("managed_phase_typed_only", phase_calls == 1 && ordinary_calls == 0 && legacy_calls == 0);
			{
				MutexLock lock(enrollment.registry.mutex);
				ProductionRequest *expired = nullptr;RenderingDevice *device = nullptr;
				check("managed_phase_original_expiry", enrollment.registry.production_physical_request_locked(phase_context, ref, expired, device, false) == ERR_UNAUTHORIZED && !expired);
			}
			GDExtensionScenePhaseCloseV1 close{};
			const Error closing = Error(unregister_phase(phase_registration, &close).native_error);
			check("managed_phase_close_waits_holder", closing == ERR_BUSY && close.registration_closed == 1 && close.active_deliveries == 0 && close.relay_cell_released == 1 && close.relay_holder_closed == 0);
			phase_callable = Callable();
			check("managed_phase_original_free", callable_frees == 1);
			const Error closed = Error(unregister_phase(phase_registration, &close).native_error);
			check("managed_phase_original_close", closed == OK && close.registration_closed == 1 && close.active_deliveries == 0 && close.relay_cell_released == 1 && close.relay_holder_closed == 1);
			CRASH_COND(closed != OK);
		}
		static void record(void *data, GDExtensionDeviceExit::ProductionPhysicalContextRef context,
				GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOperationV1 *) {
			auto &self = *static_cast<ManagedSceneHarness *>(data);
			++self.legacy_calls;
			self.record_error = self.native->install_generation(context, request, self.enrollment.registry);
		}
		void check(const char *name, bool passed) { checks[name] = passed; }
		Dictionary advance_phase_close() {
			using namespace GDExtensionDeviceExit;
			if (!waiting_close_started) {
				// Wait until original FIFO dispatch has really deferred this RECORD.
				MutexLock lock(enrollment.registry.mutex);
				ProductionRequest *pending = nullptr;RenderingDevice *device = nullptr;
				CRASH_COND(enrollment.registry.production_request_locked(waiting_request, false, pending, device) != OK);
				if (!pending->control.scene_waiting || pending->control.state != ProductionControlState::QUEUED) return {};
				waiting_close_started = true;
			}
			GDExtensionScenePhaseCloseV1 close{};
			const Error closing = Error(unregister_phase(phase_registration, &close).native_error);
			CRASH_COND(closing != ERR_BUSY);
			check("managed_phase_no_draw_admission_closed", close.registration_closed == 1 && close.active_deliveries == 0);
			ProductionControlOutput refused{};
			const Error returned = enrollment.registry.production_control_consume(waiting_request, waiting_control, &refused);
			if (returned == ERR_BUSY) return {};
			CRASH_COND(returned != OK);
			check("managed_phase_waiting_return_without_draw", refused.result.native_error == ERR_UNAVAILABLE &&
					refused.request_serial == waiting_request.handle_serial && refused.control_serial == waiting_control && phase_calls == 1);
			// This never-delivered, empty request has no pin/native source to retire;
			// use D's exact original no-draw mailbox cancellation/release path.
			CRASH_COND(enrollment.registry.production_cancel(waiting_request).native_error != OK);
			CRASH_COND(enrollment.registry.production_release(waiting_request).native_error != OK);
			waiting_request = {};
			close_phase();native->withdraw_generation();stage = 1;
			CRASH_COND(schedule(ProductionControlKind::OBSERVE) != OK);
			return {};
		}
		Error schedule(GDExtensionDeviceExit::ProductionControlKind kind) {
			using namespace GDExtensionDeviceExit;
			if (kind == ProductionControlKind::RECORD) return enrollment.registry.production_record_schedule(ref, &control);
			const GDExtensionProductionServiceBudgetV1 budget = {
				production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0
			};
			Error error = enrollment.registry.production_control_accept(ref, kind, &control,
					&budget);
			return error == OK ? enrollment.registry.production_control_schedule(ref, control) : error;
		}
		Error begin(bool p_phase_mode) {
			phase_mode = p_phase_mode;
			device = RenderingDevice::get_singleton();
			auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
			auto *storage = RendererRD::ParticlesStorage::get_singleton();
			if (!device || !storage || !server || !server->is_on_render_thread()) return ERR_UNAVAILABLE;
			enrollment.server = server;
			OrdinalPlan plan;
			plan.capacity = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
			plan.action_count = plan.resource_count = plan.use_count = plan.readback_count = 0;
			plan.attachment_requests = phase_mode ? 2 : 1;
			const Error error = enrollment.initialize_owner(1, false, false, &record, this, false, 1, false,
					+[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan, device);
			if (error == OK && phase_mode) {
				const Error installed = install_phase();CRASH_COND(installed != OK);
				auto waiting_operation = plan.operation;++waiting_operation.owner_request_nonce;
				auto waiting_capacity = plan.capacity;waiting_capacity.resource_uses = 0;
				const auto reserve = reinterpret_cast<GDExtensionInterfaceDeviceSessionProductionReserveV2>(
						gdextension_get_guarded_proc_address("device_session_production_reserve_v2"));
				CRASH_COND(!reserve || reserve(enrollment.attachment, &waiting_operation, &waiting_capacity,
						nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, &waiting_request).native_error != OK);
			}
			// Never retain a SceneReady TLS frame between runtime callbacks.
			enrollment.frame.leave();
			if (error != OK) return error;
			native = memnew(RendererRD::ExternalLinkedOwnerTest(*device, *storage, true));
			ref = { enrollment.instance, enrollment.request->serial };
			return schedule(GDExtensionDeviceExit::ProductionControlKind::RECORD);
		}
		void exercise_viewport_receiver() {
			auto *scene = RendererSceneCull::singleton;
			auto *storage = RendererRD::ParticlesStorage::get_singleton();
			viewport_collision = storage->particles_collision_allocate();
			storage->particles_collision_initialize(viewport_collision);
			storage->particles_collision_set_collision_type(viewport_collision, RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
			storage->particles_collision_set_sphere_radius(viewport_collision, 0.1f);
			viewport_receiver = scene->instance_allocate();scene->instance_initialize(viewport_receiver);
			scene->instance_set_base(viewport_receiver, native->particle_rid());
			scene->instance_set_scenario(viewport_receiver, viewport_scenario);
			viewport_source = scene->instance_allocate();scene->instance_initialize(viewport_source);
			scene->instance_set_base(viewport_source, viewport_collision);
			scene->instance_set_scenario(viewport_source, viewport_scenario);
			const AABB query(Vector3(-1, -1, -1), Vector3(2, 2, 2));
			check("viewport_receiver_query_committed", scene->scene_receiver_commit_query(viewport_receiver, native->generation, device, Transform3D(), query, query, 1) == OK);
			scene->update_dirty_instances();
			check("viewport_receiver_snapshot", scene->scene_sphere_snapshot(viewport_receiver, native->generation, device, 4, viewport_snapshot) == OK && viewport_snapshot.sources.size() == 1);
			check("viewport_original_root_live", !native->generation_pin().is_empty());
		}
		void finish_viewport_observation() {
			CRASH_COND(!viewport_mode || stage != 4);
			auto *scene = RendererSceneCull::singleton;
			check("viewport_receiver_retained_until_observation", viewport_receiver.is_valid() && !native->generation_pin().is_empty());
			RendererSceneCull::SceneSphereSnapshot observed;
			const bool snapshot_valid = scene->scene_sphere_snapshot(viewport_receiver, native->generation, device, 4, observed) == OK && observed.sources.size() == 1 && viewport_snapshot.sources.size() == 1;
			check("viewport_post_draw_snapshot", snapshot_valid);
			check("viewport_post_draw_source_unchanged", snapshot_valid &&
					observed.sources[0].source_key == viewport_snapshot.sources[0].source_key &&
					observed.sources[0].shape_revision == viewport_snapshot.sources[0].shape_revision &&
					observed.sources[0].transform == viewport_snapshot.sources[0].transform &&
					observed.sources[0].radius == viewport_snapshot.sources[0].radius);
			check("viewport_post_draw_query_unchanged", snapshot_valid &&
					observed.receiver_transform == viewport_snapshot.receiver_transform &&
					observed.query_bounds == viewport_snapshot.query_bounds);
			check("viewport_post_draw_revisions_unchanged", snapshot_valid &&
					observed.pose_revision == viewport_snapshot.pose_revision &&
					observed.bounds_revision == viewport_snapshot.bounds_revision &&
					observed.pair_revision == viewport_snapshot.pair_revision);
			scene->free(viewport_source);viewport_source = RID();
			scene->free(viewport_receiver);viewport_receiver = RID();
			RendererRD::ParticlesStorage::get_singleton()->particles_collision_free(viewport_collision);viewport_collision = RID();
			// The script has completed actual viewport readback. Resume the same
			// original no-future-draw mailbox/holder/owner close sequence.
			RenderingServer::get_singleton()->set_render_loop_enabled(false);
			CRASH_COND(enrollment.registry.production_record_schedule(waiting_request, &waiting_control) != OK);
			stage = 3;
		}
		void exercise_scene() {
			auto *scene = RendererSceneCull::singleton;
			auto *storage = RendererRD::ParticlesStorage::get_singleton();
			const AABB envelope(Vector3(-2, -2, -2), Vector3(4, 4, 4));
			const AABB query_a(Vector3(-1, -1, -1), Vector3(1, 2, 2));
			const AABB query_b(Vector3(0, -1, -1), Vector3(1, 2, 2));
			RID scenario = scene->scenario_allocate();scene->scenario_initialize(scenario);
			RID sphere = storage->particles_collision_allocate();storage->particles_collision_initialize(sphere);
			storage->particles_collision_set_collision_type(sphere, RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
			storage->particles_collision_set_sphere_radius(sphere, 0.1f);
			auto instance = [&](RID base, Vector3 position) {
				RID id = scene->instance_allocate();scene->instance_initialize(id);
				scene->instance_set_base(id, base);scene->instance_set_scenario(id, scenario);
				scene->instance_set_transform(id, Transform3D(Basis(), position));return id;
			};
			const RID left = instance(native->particle_rid(), Vector3());
			const RID right = instance(native->particle_rid(), Vector3());
			const RID source_a = instance(sphere, Vector3(-0.5f, 0, 0));
			RID source_b = instance(sphere, Vector3(0.5f, 0, 0));
			check("managed_commit_left", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, query_a, 1) == OK);
			check("managed_commit_right", scene->scene_receiver_commit_query(right, native->generation, device, Transform3D(), envelope, query_b, 1) == OK);
			RendererSceneCull::SceneSphereSnapshot a,b,rejected;
			check("managed_dirty_rejected", scene->scene_sphere_snapshot(left, native->generation, device, 4, rejected) == ERR_UNAVAILABLE);
			scene->update_dirty_instances();
			check("managed_same_base_left", scene->scene_sphere_snapshot(left, native->generation, device, 4, a) == OK && a.sources.size() == 1 && a.sources[0].transform.origin.x < 0 && a.query_bounds == query_a && a.bounds_revision == 1 && a.pose_revision > 0);
			check("managed_same_base_right", scene->scene_sphere_snapshot(right, native->generation, device, 4, b) == OK && b.sources.size() == 1 && b.sources[0].transform.origin.x > 0 && b.query_bounds == query_b && b.bounds_revision == 1);
			check("managed_distinct_source", a.sources.size() == 1 && b.sources.size() == 1 && a.sources[0].source_key != b.sources[0].source_key);
			check("managed_bad_generation", scene->scene_receiver_commit_query(left, native->generation + 1, device, Transform3D(), envelope, query_a, 2) == ERR_INVALID_DATA);
			{
				RenderingDevice foreign;
				check("managed_foreign_device", scene->scene_receiver_commit_query(left, native->generation, &foreign, Transform3D(), envelope, query_a, 2) == ERR_INVALID_DATA);
			}
			check("managed_outside_envelope", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, AABB(Vector3(20, 0, 0), Vector3(1, 1, 1)), 2) == ERR_INVALID_PARAMETER);
			check("managed_stale_revision", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, query_a, 1) == ERR_INVALID_PARAMETER);
			rejected = b;
			check("managed_rejected_snapshot_zero", scene->scene_sphere_snapshot(right, native->generation + 1, device, 4, rejected) == ERR_INVALID_DATA && rejected.sources.is_empty() && rejected.bounds_revision == 0);
			check("managed_commit_requery", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, query_b, 2) == OK);
			scene->update_dirty_instances();
			RendererSceneCull::SceneSphereSnapshot changed,unchanged;
			check("managed_left_revision", scene->scene_sphere_snapshot(left, native->generation, device, 4, changed) == OK && changed.bounds_revision == 2 && changed.pose_revision > a.pose_revision && changed.sources.size() == 1 && b.sources.size() == 1 && changed.sources[0].source_key == b.sources[0].source_key);
			check("managed_right_unchanged", scene->scene_sphere_snapshot(right, native->generation, device, 4, unchanged) == OK && unchanged.bounds_revision == b.bounds_revision && unchanged.pose_revision == b.pose_revision && unchanged.pair_revision == b.pair_revision && unchanged.query_bounds == b.query_bounds);
			// Retain only the value snapshot. It carries no Scene sample/borrow
			// authorization and must not follow later source-owner mutations or ABA.
			const RendererSceneCull::SceneSphereSnapshot retained = b;
			const RendererSceneCull::SceneSphereSource retained_source = b.sources.size() == 1 ? b.sources[0] : RendererSceneCull::SceneSphereSource();
			const uint64_t retained_pose = b.pose_revision, retained_bounds = b.bounds_revision, retained_pairs = b.pair_revision;
			auto retained_unchanged = [&]() {
				return retained.sources.size() == 1 && retained.sources[0].source_key == retained_source.source_key &&
						retained.sources[0].shape_revision == retained_source.shape_revision && retained.sources[0].transform == retained_source.transform &&
						retained.sources[0].radius == retained_source.radius && retained.receiver_transform == Transform3D() && retained.query_bounds == query_b &&
						retained.pose_revision == retained_pose && retained.bounds_revision == retained_bounds && retained.pair_revision == retained_pairs;
			};
			scene->instance_set_transform(source_b, Transform3D(Basis(), Vector3(0.75f, 0, 0)));
			storage->particles_collision_set_sphere_radius(sphere, 0.125f);
			scene->update_dirty_instances();
			RendererSceneCull::SceneSphereSnapshot moved, removed, replacement;
			check("managed_source_mutation_snapshot", scene->scene_sphere_snapshot(right, native->generation, device, 4, moved) == OK &&
					moved.sources.size() == 1 && moved.sources[0].source_key == retained_source.source_key && moved.sources[0].shape_revision > retained_source.shape_revision &&
					moved.sources[0].transform.origin == Vector3(0.75f, 0, 0) && moved.sources[0].radius == 0.125f);
			check("managed_value_snapshot_frozen_after_source_mutation", retained_unchanged());
			scene->free(source_b);source_b = RID();
			scene->update_dirty_instances();
			check("managed_source_free_snapshot", scene->scene_sphere_snapshot(right, native->generation, device, 4, removed) == OK &&
					removed.sources.is_empty() && removed.pair_revision > moved.pair_revision);
			check("managed_value_snapshot_frozen_after_source_free", retained_unchanged());
			source_b = instance(sphere, Vector3(0.75f, 0, 0));
			scene->update_dirty_instances();
			check("managed_recreated_source_distinct_identity", scene->scene_sphere_snapshot(right, native->generation, device, 4, replacement) == OK &&
					replacement.sources.size() == 1 && replacement.sources[0].source_key != 0 && replacement.sources[0].source_key != retained_source.source_key &&
					replacement.sources[0].transform.origin == Vector3(0.75f, 0, 0) && replacement.sources[0].radius == 0.125f && replacement.pair_revision > removed.pair_revision);
			check("managed_value_snapshot_frozen_after_source_recreate", retained_unchanged());
			// Same broad envelope, different committed queries: source selection
			// belongs to the scene receiver instance, for height as for sphere.
			RID height = storage->particles_collision_allocate();storage->particles_collision_initialize(height);
			storage->particles_collision_set_collision_type(height, RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE);
			storage->particles_collision_set_box_extents(height, Vector3(0.1f, 0.1f, 0.1f));
			const RID height_a = instance(height, Vector3(-0.5f, 0, 0));
			const RID height_b = instance(height, Vector3(0.5f, 0, 0));
			check("managed_height_restore_left_query", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, query_a, 3) == OK);
			scene->update_dirty_instances();
			RendererSceneCull::SceneHeightfieldSource height_left, height_right;
			bool height_present = false;
			check("managed_height_query_left", scene->scene_heightfield_snapshot_pair(left, height_left, height_present) && height_present && height_left.transform.origin.x == -0.5f);
			height_present = false;
			check("managed_height_query_right", scene->scene_heightfield_snapshot_pair(right, height_right, height_present) && height_present && height_right.transform.origin.x == 0.5f);
			check("managed_height_distinct_source", height_left.source_key != 0 && height_right.source_key != 0 && height_left.source_key != height_right.source_key);
			const RID height_extra = instance(height, Vector3(0.75f, 0, 0));
			scene->update_dirty_instances();
			height_present = false;
			check("managed_height_query_ambiguous", !scene->scene_heightfield_snapshot_pair(right, height_right, height_present) && !height_present);
			height_present = false;
			check("managed_height_other_query_unaffected", scene->scene_heightfield_snapshot_pair(left, height_left, height_present) && height_present && height_left.transform.origin.x == -0.5f);
			const AABB empty_query(Vector3(-1, 1, -1), Vector3(2, 0.5f, 2));
			check("managed_height_commit_empty_query", scene->scene_receiver_commit_query(left, native->generation, device, Transform3D(), envelope, empty_query, 4) == OK);
			scene->update_dirty_instances();
			height_present = true;
			check("managed_height_query_empty", scene->scene_heightfield_snapshot_pair(left, height_left, height_present) && !height_present);
			scene->free(height_a);scene->free(height_b);scene->free(height_extra);
			storage->particles_collision_free(height);
			scene->free(source_a);scene->free(source_b);scene->free(left);scene->free(right);scene->free(scenario);
			storage->particles_collision_free(sphere);
			check("managed_value_snapshot_readable_after_receiver_free", retained_unchanged());
		}
		// Empty dictionary means the original queued control has not returned yet.
		Dictionary advance() {
			using namespace GDExtensionDeviceExit;
			if (stage == 4) return {}; // Retain the receiver during actual viewport observations.
			if (stage == 3) return advance_phase_close();
			ProductionControlOutput output{};
			const Error consumed = enrollment.registry.production_control_consume(ref, control, &output);
			if (consumed == ERR_BUSY) return {};
			CRASH_COND(consumed != OK); // Preserve real custody on unexpected issuer failure.
			if (stage == 0) {
				check("managed_original_record", output.result.native_error == OK && record_error == OK && !native->generation_pin().is_empty());
				if (viewport_mode) { stage = 4;return {}; }
				if (phase_mode) {
					// The one positive delivery came from actual draw. Future waiting
					// refusal and holder teardown must no longer depend on a draw.
					RenderingServer::get_singleton()->set_render_loop_enabled(false);
					CRASH_COND(enrollment.registry.production_record_schedule(waiting_request, &waiting_control) != OK);
					stage = 3;return {};
				} else if (record_error == OK) exercise_scene();
				native->withdraw_generation();stage = 1;
			} else if (stage == 1 && native->released() && enrollment.request->maintenance_serial == 0) {
				root_returned |= output.output_generation.native_released;
				check("managed_original_root_return", root_returned);
				stage = 2;
			} else if (stage == 2 && output.result.native_error == OK && output.owner_return.native_error == OK) {
				enrollment.request = nullptr; // RELEASE consume may reclaim its arena.
				check("managed_original_request_release", output.owner_return.native_error == OK);
				memdelete(native);native = nullptr;
				// All queued controls and custody have returned. This closes only
				// fixture handles; never sync the currently executing RT callback.
				enrollment.server = nullptr;enrollment.cleanup_owner(true);
				check("managed_enrollment_cleanup", enrollment.owner_cleaned);
				return checks;
			}
			if (stage == 1) root_returned |= output.output_generation.native_released;
			const Error queued = schedule(stage == 2 ? ProductionControlKind::RELEASE : ProductionControlKind::OBSERVE);
			CRASH_COND(queued != OK);
			return {};
		}
	};

	static Error quote(const GDExtensionProductionCapacityV1 &capacity, GDExtensionProductionMetadataQuoteV1 &out, uint32_t current_commands = 0, uint32_t workers = 0) {
		return GDExtensionDeviceExit::Registry::production_layout(capacity, out, current_commands, workers);
	}
};
void *managed_scene_begin(bool phase_mode) {
	auto *harness = memnew(Access::ManagedSceneHarness);
	const Error error = harness->begin(phase_mode);
	CRASH_COND_MSG(error != OK, "Managed Scene original test enrollment failed: " + itos(error));
	return harness;
}
void *managed_scene_begin_viewport(RID scenario) {
	auto *harness = memnew(Access::ManagedSceneHarness);
	harness->viewport_mode = true;
	harness->viewport_scenario = scenario; // Borrowed from the live SubViewport world.
	const Error error = harness->begin(true);
	CRASH_COND_MSG(error != OK, "Viewport managed Scene enrollment failed: " + itos(error));
	return harness;
}
bool managed_scene_viewport_ready(void *opaque) {
	return static_cast<Access::ManagedSceneHarness *>(opaque)->stage == 4;
}
void managed_scene_finish_viewport(void *opaque) {
	static_cast<Access::ManagedSceneHarness *>(opaque)->finish_viewport_observation();
}
Dictionary managed_scene_advance(void *opaque) {
	auto *harness = static_cast<Access::ManagedSceneHarness *>(opaque);
	Dictionary result = harness->advance();
	if (!result.is_empty()) memdelete(harness);
	return result;
}
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
TEST_CASE("[SceneTree][ProductionRequest][OutputMetadataFactory] protected original RECORD owns bounded actual CPU metadata and returns once") {
	Access::output_metadata_factory();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputMetadataFactory] bound live unlinked pin blocks actual metadata return") {
	Access::output_metadata_bound_pin();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] real SceneOnly root terminal retains metadata until actual particles free") {
	Access::output_scene_metadata(false);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] two actual SceneOnly generations terminal before either metadata allocation returns") {
	Access::output_scene_metadata_two_generations();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] unrelated metadata blocks SceneOnly terminal until actual return") {
	Access::output_scene_metadata(true);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] metadata bound only to an uninstalled pin still blocks generation terminal") {
	Access::output_scene_metadata(true, true);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] exhausted original pin capacity refunds real SceneOnly metadata allocation") {
	Access::output_scene_metadata_refund();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] exhausted original pin capacity destroys placement directories before refunding slab") {
	Access::output_scene_metadata_refund(true);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] actual owner slab directories return before terminal and slab returns only at particles free") {
	Access::output_scene_metadata_directory(false);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputSceneMetadata] short declared owner slab rejects finite directories before allocation") {
	Access::output_scene_metadata_directory(true);
}
TEST_CASE("[SceneTree][ProductionRequest][OutputMetadataFactory] real owner-created storage cannot become a metadata allocation") {
	Access::output_metadata_wrong_kind();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputMetadataFactory] metadata declarations reject ordinary resource use before native effects") {
	Access::output_metadata_declarations();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputPollAggregation] CPU slot reuse and partial receipt projection retain actual physical returns without double counting") {
	Access::output_poll_aggregation();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputNativeSceneAcquire] original SceneOnly Create and physical COPY precede native ordinal1 acquire and cancellation") {
	Access::output_native_scene_acquire();
}
TEST_CASE("[SceneTree][ProductionRequest][OutputNativeAdmission] native output declarations require V2 and exact alias ownership without physical grants") {
	Access::output_native_admission();
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
TEST_CASE("[SceneTree][ProductionRequest][OwnedStorageComponent] actual backing mismatch retains original group storage and custody across OBSERVE") {
	Access::owned_storage_observe(2, 0, false, false, false, true);
	Access::owned_storage_observe(0, 0, false, false, false, true);
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
TEST_CASE("[SceneTree][ProductionRequest][CoreProtected][SceneReceiverOwner] original generation root admits query enabled receiver data") {
	Access::generation_root_observe(0, false, false, false, false, true);
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
TEST_CASE("[SceneTree][ProductionRequest][OutputAliasReadbackFreeze] original accepted alias plan validates exact producer role and read span without physical effects") {
	Access::output_alias_readback_freeze();
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

TEST_CASE("[SceneTree][ProductionRequest][Raster] accepted raster reservation requires independent resources and rejects foreign expired frames") {
	TestProductionRequest::Access::singleton_raster_reserve();
}

TEST_CASE("[SceneTree][ProductionRequest][RasterPrepare] independent four-resource factories retain exact failure prefixes and retire through bounded original FIFO") {
	ERR_PRINT_OFF;
	SUBCASE("All four production roots are ready without qualification imports") {
		TestProductionRequest::Access::singleton_raster_prepare(0);
	}
	SUBCASE("Depth factory fails before any root exists") {
		TestProductionRequest::Access::singleton_raster_prepare(1);
	}
	SUBCASE("Shader factory preserves the original depth prefix") {
		TestProductionRequest::Access::singleton_raster_prepare(2);
	}
	SUBCASE("Framebuffer factory preserves depth and shader") {
		TestProductionRequest::Access::singleton_raster_prepare(3);
	}
	SUBCASE("Pipeline factory preserves the original framebuffer dependency") {
		TestProductionRequest::Access::singleton_raster_prepare(4);
	}
	SUBCASE("Actual quiesce seals admission inside the original depth factory") {
		TestProductionRequest::Access::singleton_raster_prepare(0, 1);
	}
	SUBCASE("Actual quiesce preserves depth and shader without creating later roots") {
		TestProductionRequest::Access::singleton_raster_prepare(0, 2);
	}
	ERR_PRINT_ON;
}

TEST_CASE("[SceneTree][ProductionRequest][RasterJoin] original prepared raster begin and end map issue complete and physically retire") {
	SUBCASE("Explicit end closes the actual RECORD draw list") {
		TestProductionRequest::Access::singleton_raster_prepare(0, 0, 1);
	}
	SUBCASE("Original callback return automatically closes the actual draw list") {
		TestProductionRequest::Access::singleton_raster_prepare(0, 0, 2);
	}
}

TEST_CASE("[SceneTree][ProductionRequest][RecordHistory] exact empty RECORD history survives consume and expires on replacement") {
	SUBCASE("Original FIFO accepts the exact control") {
		TestProductionRequest::Access::record_history_empty_lifecycle(false);
	}
	SUBCASE("Original bounded FIFO refuses and retries the same control") {
		TestProductionRequest::Access::record_history_empty_lifecycle(true);
	}
}

#ifdef THREADS_ENABLED
TEST_CASE("[SceneTree][ProductionRequest][RecordHistory] original compute append history survives a later empty RECORD") {
	TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, false, false, false, false, false, false, false, false, false, false, true);
}

TEST_CASE("[SceneTree][ProductionRequest][RecordHistory] original partial upload history survives a later empty RECORD") {
	TestProductionRequest::Access::singleton_split_owner_set(true, false, false, false, true, false, false, false, false, false, false, false, false, false, true);
}
#endif

TEST_CASE("[SceneTree][ProductionRequest][RecordHistory] native factory history prevents zero-prefix no-effect certificates") {
	ERR_PRINT_OFF;
	SUBCASE("Successful original pipeline factory") {
		TestProductionRequest::Access::pipeline_prefix_observe(0, false, true);
	}
	SUBCASE("Original driver refuses after effect entry") {
		TestProductionRequest::Access::pipeline_prefix_observe(2, false, true);
	}
	SUBCASE("Pure Core capacity preflight never enters the factory") {
		TestProductionRequest::Access::pipeline_prefix_observe(4, false, true);
	}
	ERR_PRINT_ON;
}

TEST_CASE("[ProductionRequest][Raster] typed reserve bridge validates its independent action contract") {
	using RasterReserve = GDExtensionInterfaceDeviceSessionProductionRasterReserveV1;
	const auto reserve = reinterpret_cast<RasterReserve>(GDExtension::get_interface_function("device_session_production_raster_reserve_v1"));
	REQUIRE(reserve != nullptr);
	GDExtensionProductionRequestRefV1 out = { UINT64_MAX, UINT64_MAX };
	const auto result = reserve({}, nullptr, nullptr, nullptr, &out);
	CHECK(result.native_error == ERR_INVALID_PARAMETER);
	CHECK(out.registry_instance == 0);
	CHECK(out.handle_serial == 0);
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

#ifdef THREADS_ENABLED
TEST_CASE("[ProductionRequest][QuoteLockOrder] main SceneReady quote survives bounded original RD contention") {
	TestProductionRequest::Access::quote_lock_contention(false);
}
TEST_CASE("[ProductionRequest][QuoteLockOrder] main SceneReady accept survives bounded original RD contention") {
	TestProductionRequest::Access::quote_lock_contention(true);
}
#endif

TEST_CASE("[SceneTree][ProductionRequest][MetadataV3] native SceneOnly requirements recompute before original Create") {
	TestProductionRequest::Access::output_scene_metadata(false, false, true);
}

TEST_CASE("[SceneTree][ProductionRequest][MetadataV3] exact accepted row size rejects undersize and oversize before effects") {
	TestProductionRequest::Access::output_scene_metadata(false, false, true, -1);
	TestProductionRequest::Access::output_scene_metadata(false, false, true, 1);
}
TEST_CASE("[SceneTree][ProductionRequest][MetadataV3] V2 preserves larger accepted metadata semantics") {
	TestProductionRequest::Access::output_scene_metadata(false, false, false, 1);
}

TEST_CASE("[SceneTree][ProductionRequest][GenerationSelection] original slab and root retain CPU selection through bounded return") {
	TestProductionRequest::Access::generation_selection_lifecycle(0);
}
TEST_CASE("[SceneTree][ProductionRequest][GenerationSelection] stale second material retains prefix until unpublished auto drain") {
	TestProductionRequest::Access::generation_selection_lifecycle(1);
}
TEST_CASE("[SceneTree][ProductionRequest][GenerationSelection] transaction closes post root refusal before first selection lease") {
	TestProductionRequest::Access::generation_selection_lifecycle(2);
}

TEST_CASE("[SceneTree][ProductionRequest][GenerationGuardRefusal] actual unprepared engine component refuses private and public Instance192 paths") {
	TestProductionRequest::Access::generation_selection_lifecycle(3);
}

TEST_CASE("[SceneTree][ProductionRequest][AliasReadbackIntegration] actual internal acquire upload and single download preserve byte custody") {
	TestProductionRequest::Access::output_alias_readback_complete();
}

TEST_CASE("[SceneTree][ProductionRequest][AliasReadbackMapFailure] actual download map failure returns custody and retires original owners") {
	TestProductionRequest::Access::output_alias_readback_complete(true);
}

TEST_CASE("[SceneTree][ProductionRequest][NativeScenePhase] actual early hook owns bounded typed delivery expiry and original relay close") {
	TestProductionRequest::Access::native_scene_phase_infrastructure();
}

TEST_CASE("[SceneTree][ProductionRequest][NativeScenePhaseCloseWaiting] actual unregister returns two original waiting controls without a draw") {
	TestProductionRequest::Access::native_scene_phase_infrastructure(true);
}
