/**************************************************************************/
/*  test_shader_compile_policy.cpp                                        */
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

#include "tests/test_macros.h"

#include <cstdlib>

TEST_FORCE_LINK(test_shader_compile_policy)

#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/shader_rd.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_shader_container.h"
#include "servers/rendering/shader_compile_test_bridge.h"

#include "modules/modules_enabled.gen.h"

#include <thread>

#ifdef METAL_ENABLED
#include "drivers/metal/rendering_shader_container_metal.h"

#include <Metal/Metal.hpp>
#ifdef MODULE_GLSLANG_ENABLED
Vector<uint8_t> compile_glslang_shader(RenderingDeviceCommons::ShaderStage, const String &, RenderingDeviceCommons::ShaderLanguageVersion, RenderingDeviceCommons::ShaderSpirvVersion, String *);
#endif
NS::SharedPtr<MTL::CompileOptions> make_shader_compile_options(uint32_t p_msl_version, bool p_logging, bool p_invariance, bool p_disable_fast_math);
#endif

#ifdef TESTS_ENABLED
class TestShaderCompilePolicyAccess {
public:
	static Vector<uint8_t> compile_ref(RenderingDevice &p_rd, const Ref<RDShaderSPIRV> &p_spirv, const String &p_name, uint64_t p_flags) { return p_rd._shader_compile_binary_from_spirv_with_options(p_spirv, p_name, p_flags); }
	static RID create_ref(RenderingDevice &p_rd, const Ref<RDShaderSPIRV> &p_spirv, const String &p_name, uint64_t p_flags) { return p_rd._shader_create_from_spirv_with_options(p_spirv, p_name, p_flags); }
	static RID create_old_ref(RenderingDevice &p_rd, const Ref<RDShaderSPIRV> &p_spirv, const String &p_name) { return p_rd._shader_create_from_spirv(p_spirv, p_name); }
	static String name(RenderingDevice &p_rd, RID p_rid) { return p_rd.shader_owner.get_or_null(p_rid)->name; }
	static void set_driver(RenderingDevice &p_rd, RenderingDeviceDriver *p_driver) { p_rd.driver = p_driver; }
	static void clear_shader(RenderingDevice &p_rd, RID p_rid) { p_rd.shader_owner.free(p_rid); }
	static void mark_filled(RenderingDevice &p_rd, RID p_rid) { p_rd.shader_owner.get_or_null(p_rid)->driver_id = RenderingDeviceDriver::ShaderID(42); }

	static void rollback(Vector<RID> &p_variants, Vector<Vector<uint8_t>> &p_bytes, const LocalVector<int> &p_group, const LocalVector<int> &p_loaded, Vector<RID> &p_freed) {
		ShaderRD::_rollback_cache_load(p_variants, p_bytes, p_group, p_loaded, [](RID p_rid, void *p_context) { static_cast<Vector<RID> *>(p_context)->push_back(p_rid); }, &p_freed);
	}
};
#endif

namespace TestShaderCompilePolicy {
#ifdef TESTS_ENABLED
static PackedByteArray policy_spirv(uint32_t p_model, bool p_multi = false);
#endif
class CodecContainer : public RenderingShaderContainer {
protected:
	uint32_t _format() const override { return 0x12345678; }
	uint32_t _format_version() const override { return 1; }
	bool _from_bytes_header_extra_data(ContainerReader &) override {
		header_calls++;
		return true;
	}
	bool _set_code_from_spirv(const ReflectShader &p_shader) override {
		compile_calls++;
		for (const auto &stage : p_shader.shader_stages) {
			Shader shader;
			shader.shader_stage = stage.shader_stage;
			shader.code_compressed_bytes.push_back(42);
			shaders.push_back(shader);
		}
		return true;
	}

public:
	int compile_calls = 0;
	int header_calls = 0;
	void set_fixture_name(const char *p_name) {
		shader_name = CharString(p_name);
		reflection_data.shader_name_len = shader_name.length();
	}
	void initialize_fixture(uint32_t p_flags) {
		compile_flags = p_flags;
		reflection_data = {};
		reflection_data.pipeline_type = RenderingDeviceCommons::PIPELINE_TYPE_COMPUTE;
		reflection_data.stage_count = 1;
		reflection_data.compute_local_size[0] = 1;
		reflection_data.compute_local_size[1] = 1;
		reflection_data.compute_local_size[2] = 1;
		reflection_shader_stages.push_back(RenderingDeviceCommons::SHADER_STAGE_COMPUTE);
		Shader shader;
		shader.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		shader.code_compressed_bytes.push_back(42);
		shaders.push_back(shader);
	}
};

#ifdef TESTS_ENABLED
class PolicyFormat : public RenderingShaderContainerFormat {
public:
	mutable int creates = 0;
	uint64_t supported = 0;
	BitField<ShaderCompileFlags> supported_compile_flags() const override { return supported; }
	Ref<RenderingShaderContainer> create_container() const override {
		creates++;
		return memnew(CodecContainer);
	}
	ShaderLanguageVersion get_shader_language_version() const override { return SHADER_LANGUAGE_VULKAN_VERSION_1_1; }
	ShaderSpirvVersion get_shader_spirv_version() const override { return SHADER_SPIRV_VERSION_1_3; }
};

// This driver never initializes a device. Unexpected calls fail immediately;
// the exercised shader boundary records native admission without making GPU claims.
class PolicyDriver : public RenderingDeviceDriver {
	[[noreturn]] static void unexpected_driver_call() {
		CRASH_NOW();
		std::abort();
	}

public:
	PolicyFormat format;
	int native_creates = 0;
	bool native_accepts = false;
	Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override {
		FAIL("Unexpected driver call: initialize");
		unexpected_driver_call();
	}
	BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn) override {
		FAIL("Unexpected driver call: buffer_create");
		unexpected_driver_call();
	}
	bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override {
		FAIL("Unexpected driver call: buffer_set_texel_format");
		unexpected_driver_call();
	}
	void buffer_free(BufferID p_buffer) override {
		FAIL("Unexpected driver call: buffer_free");
		unexpected_driver_call();
	}
	uint64_t buffer_get_allocation_size(BufferID p_buffer) override {
		FAIL("Unexpected driver call: buffer_get_allocation_size");
		unexpected_driver_call();
	}
	uint8_t *buffer_map(BufferID p_buffer) override {
		FAIL("Unexpected driver call: buffer_map");
		unexpected_driver_call();
	}
	void buffer_unmap(BufferID p_buffer) override {
		FAIL("Unexpected driver call: buffer_unmap");
		unexpected_driver_call();
	}
	uint8_t *buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) override {
		FAIL("Unexpected driver call: buffer_persistent_map_advance");
		unexpected_driver_call();
	}
	uint64_t buffer_get_dynamic_offsets(Span<BufferID> p_buffers) override {
		FAIL("Unexpected driver call: buffer_get_dynamic_offsets");
		unexpected_driver_call();
	}
	uint64_t buffer_get_device_address(BufferID p_buffer) override {
		FAIL("Unexpected driver call: buffer_get_device_address");
		unexpected_driver_call();
	}
	TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view) override {
		FAIL("Unexpected driver call: texture_create");
		unexpected_driver_call();
	}
	TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps) override {
		FAIL("Unexpected driver call: texture_create_from_extension");
		unexpected_driver_call();
	}
	TextureID texture_create_shared(TextureID p_original_texture, const TextureView &p_view) override {
		FAIL("Unexpected driver call: texture_create_shared");
		unexpected_driver_call();
	}
	TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) override {
		FAIL("Unexpected driver call: texture_create_shared_from_slice");
		unexpected_driver_call();
	}
	void texture_free(TextureID p_texture) override {
		FAIL("Unexpected driver call: texture_free");
		unexpected_driver_call();
	}
	uint64_t texture_get_allocation_size(TextureID p_texture) override {
		FAIL("Unexpected driver call: texture_get_allocation_size");
		unexpected_driver_call();
	}
	void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) override {
		FAIL("Unexpected driver call: texture_get_copyable_layout");
		unexpected_driver_call();
	}
	Vector<uint8_t> texture_get_data(TextureID p_texture, uint32_t p_layer) override {
		FAIL("Unexpected driver call: texture_get_data");
		unexpected_driver_call();
	}
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override {
		FAIL("Unexpected driver call: texture_get_usages_supported_by_format");
		unexpected_driver_call();
	}
	bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) override {
		FAIL("Unexpected driver call: texture_can_make_shared_with_format");
		unexpected_driver_call();
	}
	SamplerID sampler_create(const SamplerState &p_state) override {
		FAIL("Unexpected driver call: sampler_create");
		unexpected_driver_call();
	}
	void sampler_free(SamplerID p_sampler) override {
		FAIL("Unexpected driver call: sampler_free");
		unexpected_driver_call();
	}
	bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override {
		FAIL("Unexpected driver call: sampler_is_format_supported_for_filter");
		unexpected_driver_call();
	}
	VertexFormatID vertex_format_create(Span<VertexAttribute> p_vertex_attribs, const VertexAttributeBindingsMap &p_vertex_bindings) override {
		FAIL("Unexpected driver call: vertex_format_create");
		unexpected_driver_call();
	}
	void vertex_format_free(VertexFormatID p_vertex_format) override {
		FAIL("Unexpected driver call: vertex_format_free");
		unexpected_driver_call();
	}
	void command_pipeline_barrier(CommandBufferID p_cmd_buffer, BitField<PipelineStageBits> p_src_stages, BitField<PipelineStageBits> p_dst_stages, VectorView<MemoryAccessBarrier> p_memory_barriers, VectorView<BufferBarrier> p_buffer_barriers, VectorView<TextureBarrier> p_texture_barriers, VectorView<AccelerationStructureBarrier> p_acceleration_structure_barriers) override {
		FAIL("Unexpected driver call: command_pipeline_barrier");
		unexpected_driver_call();
	}
	FenceID fence_create() override {
		FAIL("Unexpected driver call: fence_create");
		unexpected_driver_call();
	}
	Error fence_wait(FenceID p_fence) override {
		FAIL("Unexpected driver call: fence_wait");
		unexpected_driver_call();
	}
	void fence_free(FenceID p_fence) override {
		FAIL("Unexpected driver call: fence_free");
		unexpected_driver_call();
	}
	SemaphoreID semaphore_create() override {
		FAIL("Unexpected driver call: semaphore_create");
		unexpected_driver_call();
	}
	void semaphore_free(SemaphoreID p_semaphore) override {
		FAIL("Unexpected driver call: semaphore_free");
		unexpected_driver_call();
	}
	CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) override {
		FAIL("Unexpected driver call: command_queue_family_get");
		unexpected_driver_call();
	}
	CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) override {
		FAIL("Unexpected driver call: command_queue_create");
		unexpected_driver_call();
	}
	Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) override {
		FAIL("Unexpected driver call: command_queue_execute_and_present");
		unexpected_driver_call();
	}
	void command_queue_free(CommandQueueID p_cmd_queue) override {
		FAIL("Unexpected driver call: command_queue_free");
		unexpected_driver_call();
	}
	CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override {
		FAIL("Unexpected driver call: command_pool_create");
		unexpected_driver_call();
	}
	bool command_pool_reset(CommandPoolID p_cmd_pool) override {
		FAIL("Unexpected driver call: command_pool_reset");
		unexpected_driver_call();
	}
	void command_pool_free(CommandPoolID p_cmd_pool) override {
		FAIL("Unexpected driver call: command_pool_free");
		unexpected_driver_call();
	}
	CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override {
		FAIL("Unexpected driver call: command_buffer_create");
		unexpected_driver_call();
	}
	bool command_buffer_begin(CommandBufferID p_cmd_buffer) override {
		FAIL("Unexpected driver call: command_buffer_begin");
		unexpected_driver_call();
	}
	bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override {
		FAIL("Unexpected driver call: command_buffer_begin_secondary");
		unexpected_driver_call();
	}
	void command_buffer_end(CommandBufferID p_cmd_buffer) override {
		FAIL("Unexpected driver call: command_buffer_end");
		unexpected_driver_call();
	}
	void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override {
		FAIL("Unexpected driver call: command_buffer_execute_secondary");
		unexpected_driver_call();
	}
	SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) override {
		FAIL("Unexpected driver call: swap_chain_create");
		unexpected_driver_call();
	}
	Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override {
		FAIL("Unexpected driver call: swap_chain_resize");
		unexpected_driver_call();
	}
	FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) override {
		FAIL("Unexpected driver call: swap_chain_acquire_framebuffer");
		unexpected_driver_call();
	}
	RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override {
		FAIL("Unexpected driver call: swap_chain_get_render_pass");
		unexpected_driver_call();
	}
	DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override {
		FAIL("Unexpected driver call: swap_chain_get_format");
		unexpected_driver_call();
	}
	ColorSpace swap_chain_get_color_space(SwapChainID p_swap_chain) override {
		FAIL("Unexpected driver call: swap_chain_get_color_space");
		unexpected_driver_call();
	}
	bool swap_chain_get_hdr_output_supported(SwapChainID p_swap_chain) override {
		FAIL("Unexpected driver call: swap_chain_get_hdr_output_supported");
		unexpected_driver_call();
	}
	void swap_chain_free(SwapChainID p_swap_chain) override {
		FAIL("Unexpected driver call: swap_chain_free");
		unexpected_driver_call();
	}
	FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override {
		FAIL("Unexpected driver call: framebuffer_create");
		unexpected_driver_call();
	}
	void framebuffer_free(FramebufferID p_framebuffer) override {
		FAIL("Unexpected driver call: framebuffer_free");
		unexpected_driver_call();
	}
	ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &p_shader_container, const Vector<ImmutableSampler> &p_immutable_samplers) override {
		native_creates++;
		return native_accepts ? ShaderID(42) : ShaderID();
	}
	void shader_free(ShaderID p_shader) override {
		FAIL("Unexpected driver call: shader_free");
		unexpected_driver_call();
	}
	void shader_destroy_modules(ShaderID p_shader) override {
		FAIL("Unexpected driver call: shader_destroy_modules");
		unexpected_driver_call();
	}
	UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) override {
		FAIL("Unexpected driver call: uniform_set_create");
		unexpected_driver_call();
	}
	void uniform_set_free(UniformSetID p_uniform_set) override {
		FAIL("Unexpected driver call: uniform_set_free");
		unexpected_driver_call();
	}
	uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) const override {
		FAIL("Unexpected driver call: uniform_sets_get_dynamic_offsets");
		unexpected_driver_call();
	}
	void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override {
		FAIL("Unexpected driver call: command_uniform_set_prepare_for_use");
		unexpected_driver_call();
	}
	void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override {
		FAIL("Unexpected driver call: command_clear_buffer");
		unexpected_driver_call();
	}
	void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override {
		FAIL("Unexpected driver call: command_copy_buffer");
		unexpected_driver_call();
	}
	void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override {
		FAIL("Unexpected driver call: command_copy_texture");
		unexpected_driver_call();
	}
	void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override {
		FAIL("Unexpected driver call: command_resolve_texture");
		unexpected_driver_call();
	}
	void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override {
		FAIL("Unexpected driver call: command_clear_color_texture");
		unexpected_driver_call();
	}
	void command_clear_depth_stencil_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, float p_depth, uint8_t p_stencil, const TextureSubresourceRange &p_subresources) override {
		FAIL("Unexpected driver call: command_clear_depth_stencil_texture");
		unexpected_driver_call();
	}
	void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override {
		FAIL("Unexpected driver call: command_copy_buffer_to_texture");
		unexpected_driver_call();
	}
	void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override {
		FAIL("Unexpected driver call: command_copy_texture_to_buffer");
		unexpected_driver_call();
	}
	void pipeline_free(PipelineID p_pipeline) override {
		FAIL("Unexpected driver call: pipeline_free");
		unexpected_driver_call();
	}
	void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) override {
		FAIL("Unexpected driver call: command_bind_push_constants");
		unexpected_driver_call();
	}
	bool pipeline_cache_create(const Vector<uint8_t> &p_data) override {
		FAIL("Unexpected driver call: pipeline_cache_create");
		unexpected_driver_call();
	}
	void pipeline_cache_free() override {
		FAIL("Unexpected driver call: pipeline_cache_free");
		unexpected_driver_call();
	}
	size_t pipeline_cache_query_size() override {
		FAIL("Unexpected driver call: pipeline_cache_query_size");
		unexpected_driver_call();
	}
	Vector<uint8_t> pipeline_cache_serialize() override {
		FAIL("Unexpected driver call: pipeline_cache_serialize");
		unexpected_driver_call();
	}
	RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment) override {
		FAIL("Unexpected driver call: render_pass_create");
		unexpected_driver_call();
	}
	void render_pass_free(RenderPassID p_render_pass) override {
		FAIL("Unexpected driver call: render_pass_free");
		unexpected_driver_call();
	}
	void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override {
		FAIL("Unexpected driver call: command_begin_render_pass");
		unexpected_driver_call();
	}
	void command_end_render_pass(CommandBufferID p_cmd_buffer) override {
		FAIL("Unexpected driver call: command_end_render_pass");
		unexpected_driver_call();
	}
	void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override {
		FAIL("Unexpected driver call: command_next_render_subpass");
		unexpected_driver_call();
	}
	void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) override {
		FAIL("Unexpected driver call: command_render_set_viewport");
		unexpected_driver_call();
	}
	void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) override {
		FAIL("Unexpected driver call: command_render_set_scissor");
		unexpected_driver_call();
	}
	void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) override {
		FAIL("Unexpected driver call: command_render_clear_attachments");
		unexpected_driver_call();
	}
	void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override {
		FAIL("Unexpected driver call: command_bind_render_pipeline");
		unexpected_driver_call();
	}
	void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) override {
		FAIL("Unexpected driver call: command_bind_render_uniform_sets");
		unexpected_driver_call();
	}
	void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) override {
		FAIL("Unexpected driver call: command_render_draw");
		unexpected_driver_call();
	}
	void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override {
		FAIL("Unexpected driver call: command_render_draw_indexed");
		unexpected_driver_call();
	}
	void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override {
		FAIL("Unexpected driver call: command_render_draw_indexed_indirect");
		unexpected_driver_call();
	}
	void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override {
		FAIL("Unexpected driver call: command_render_draw_indexed_indirect_count");
		unexpected_driver_call();
	}
	void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override {
		FAIL("Unexpected driver call: command_render_draw_indirect");
		unexpected_driver_call();
	}
	void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override {
		FAIL("Unexpected driver call: command_render_draw_indirect_count");
		unexpected_driver_call();
	}
	void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets, uint64_t p_dynamic_offsets) override {
		FAIL("Unexpected driver call: command_render_bind_vertex_buffers");
		unexpected_driver_call();
	}
	void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override {
		FAIL("Unexpected driver call: command_render_bind_index_buffer");
		unexpected_driver_call();
	}
	void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) override {
		FAIL("Unexpected driver call: command_render_set_blend_constants");
		unexpected_driver_call();
	}
	void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override {
		FAIL("Unexpected driver call: command_render_set_line_width");
		unexpected_driver_call();
	}
	PipelineID render_pipeline_create(ShaderID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, PipelineRasterizationState p_rasterization_state, PipelineMultisampleState p_multisample_state, PipelineDepthStencilState p_depth_stencil_state, PipelineColorBlendState p_blend_state, VectorView<int32_t> p_color_attachments, BitField<PipelineDynamicStateFlags> p_dynamic_state, RenderPassID p_render_pass, uint32_t p_render_subpass, VectorView<PipelineSpecializationConstant> p_specialization_constants) override {
		FAIL("Unexpected driver call: render_pipeline_create");
		unexpected_driver_call();
	}
	void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override {
		FAIL("Unexpected driver call: command_bind_compute_pipeline");
		unexpected_driver_call();
	}
	void command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) override {
		FAIL("Unexpected driver call: command_bind_compute_uniform_sets");
		unexpected_driver_call();
	}
	void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) override {
		FAIL("Unexpected driver call: command_compute_dispatch");
		unexpected_driver_call();
	}
	void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) override {
		FAIL("Unexpected driver call: command_compute_dispatch_indirect");
		unexpected_driver_call();
	}
	PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) override {
		FAIL("Unexpected driver call: compute_pipeline_create");
		unexpected_driver_call();
	}
	AccelerationStructureID blas_create(VectorView<AccelerationStructureGeometry> p_geometries, BitField<AccelerationStructureFlagBits> p_flags) override {
		FAIL("Unexpected driver call: blas_create");
		unexpected_driver_call();
	}
	AccelerationStructureID tlas_create(uint32_t p_max_instance_count, BitField<AccelerationStructureFlagBits> p_flags) override {
		FAIL("Unexpected driver call: tlas_create");
		unexpected_driver_call();
	}
	void acceleration_structure_instance_write(uint8_t *r_driver_instance, const AccelerationStructureInstance &p_instance) override {
		FAIL("Unexpected driver call: acceleration_structure_instance_write");
		unexpected_driver_call();
	}
	void acceleration_structure_free(AccelerationStructureID p_acceleration_structure) override {
		FAIL("Unexpected driver call: acceleration_structure_free");
		unexpected_driver_call();
	}
	uint32_t acceleration_structure_get_scratch_size_bytes(AccelerationStructureID p_acceleration_structure) override {
		FAIL("Unexpected driver call: acceleration_structure_get_scratch_size_bytes");
		unexpected_driver_call();
	}
	RaytracingPipelineID raytracing_pipeline_create(VectorView<PipelineShader> p_shaders, VectorView<uint32_t> p_raygen_shader_indices, VectorView<uint32_t> p_miss_shader_indices, VectorView<HitGroup> p_hit_groups, uint32_t p_max_trace_recursion_depth, ShaderID p_layout_defining_shader) override {
		FAIL("Unexpected driver call: raytracing_pipeline_create");
		unexpected_driver_call();
	}
	void raytracing_pipeline_free(RaytracingPipelineID p_pipeline) override {
		FAIL("Unexpected driver call: raytracing_pipeline_free");
		unexpected_driver_call();
	}
	bool raytracing_pipeline_get_shader_group_handles(RaytracingPipelineID p_pipeline, uint32_t p_group_index_offset, VectorView<uint32_t> p_group_indices, uint8_t *r_data, uint32_t p_data_stride_bytes) override {
		FAIL("Unexpected driver call: raytracing_pipeline_get_shader_group_handles");
		unexpected_driver_call();
	}
	void command_build_blas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer) override {
		FAIL("Unexpected driver call: command_build_blas");
		unexpected_driver_call();
	}
	void command_build_tlas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer, BufferID p_instance_buffer, uint32_t p_instance_offset, uint32_t p_instance_count) override {
		FAIL("Unexpected driver call: command_build_tlas");
		unexpected_driver_call();
	}
	void command_bind_raytracing_pipeline(CommandBufferID p_cmd_buffer, RaytracingPipelineID p_pipeline) override {
		FAIL("Unexpected driver call: command_bind_raytracing_pipeline");
		unexpected_driver_call();
	}
	void command_bind_raytracing_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override {
		FAIL("Unexpected driver call: command_bind_raytracing_uniform_set");
		unexpected_driver_call();
	}
	void command_trace_rays(CommandBufferID p_cmd_buffer, const ShaderBindingTable &p_raygen_sbt, const ShaderBindingTable &p_miss_sbt, const ShaderBindingTable &p_hit_sbt, uint32_t p_width, uint32_t p_height, uint32_t p_depth) override {
		FAIL("Unexpected driver call: command_trace_rays");
		unexpected_driver_call();
	}
	QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override {
		FAIL("Unexpected driver call: timestamp_query_pool_create");
		unexpected_driver_call();
	}
	void timestamp_query_pool_free(QueryPoolID p_pool_id) override {
		FAIL("Unexpected driver call: timestamp_query_pool_free");
		unexpected_driver_call();
	}
	void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) override {
		FAIL("Unexpected driver call: timestamp_query_pool_get_results");
		unexpected_driver_call();
	}
	uint64_t timestamp_query_result_to_time(uint64_t p_result) override {
		FAIL("Unexpected driver call: timestamp_query_result_to_time");
		unexpected_driver_call();
	}
	void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override {
		FAIL("Unexpected driver call: command_timestamp_query_pool_reset");
		unexpected_driver_call();
	}
	void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override {
		FAIL("Unexpected driver call: command_timestamp_write");
		unexpected_driver_call();
	}
	void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) override {
		FAIL("Unexpected driver call: command_begin_label");
		unexpected_driver_call();
	}
	void command_end_label(CommandBufferID p_cmd_buffer) override {
		FAIL("Unexpected driver call: command_end_label");
		unexpected_driver_call();
	}
	void command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) override {
		FAIL("Unexpected driver call: command_insert_breadcrumb");
		unexpected_driver_call();
	}
	void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) override {
		FAIL("Unexpected driver call: begin_segment");
		unexpected_driver_call();
	}
	void end_segment() override {
		FAIL("Unexpected driver call: end_segment");
		unexpected_driver_call();
	}
	void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override {}
	uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override {
		FAIL("Unexpected driver call: get_resource_native_handle");
		unexpected_driver_call();
	}
	uint64_t get_total_memory_used() override {
		FAIL("Unexpected driver call: get_total_memory_used");
		unexpected_driver_call();
	}
	uint64_t get_lazily_memory_used() override {
		FAIL("Unexpected driver call: get_lazily_memory_used");
		unexpected_driver_call();
	}
	uint64_t limit_get(Limit p_limit) override {
		FAIL("Unexpected driver call: limit_get");
		unexpected_driver_call();
	}
	bool has_feature(Features p_feature) override {
		FAIL("Unexpected driver call: has_feature");
		unexpected_driver_call();
	}
	const MultiviewCapabilities &get_multiview_capabilities() override {
		FAIL("Unexpected driver call: get_multiview_capabilities");
		unexpected_driver_call();
	}
	const FragmentShadingRateCapabilities &get_fragment_shading_rate_capabilities() override {
		FAIL("Unexpected driver call: get_fragment_shading_rate_capabilities");
		unexpected_driver_call();
	}
	const FragmentDensityMapCapabilities &get_fragment_density_map_capabilities() override {
		FAIL("Unexpected driver call: get_fragment_density_map_capabilities");
		unexpected_driver_call();
	}
	String get_api_name() const override {
		FAIL("Unexpected driver call: get_api_name");
		unexpected_driver_call();
	}
	String get_api_version() const override {
		FAIL("Unexpected driver call: get_api_version");
		unexpected_driver_call();
	}
	String get_pipeline_cache_uuid() const override {
		FAIL("Unexpected driver call: get_pipeline_cache_uuid");
		unexpected_driver_call();
	}
	const Capabilities &get_capabilities() const override {
		FAIL("Unexpected driver call: get_capabilities");
		unexpected_driver_call();
	}
	const RenderingShaderContainerFormat &get_shader_container_format() const override { return format; }
};
#endif

#ifdef TESTS_ENABLED
struct AuditIOState {
	PackedByteArray visible;
	int failure = 0;
};

class BufferedAuditFile : public FileAccessMemory {
	AuditIOState *state;
	PackedByteArray pending;

public:
	explicit BufferedAuditFile(AuditIOState *p_state) : state(p_state) {}
	uint64_t get_length() const override { return state->visible.size(); }
	void seek(uint64_t) override {}
	bool store_buffer(const uint8_t *p_src, uint64_t p_length) override {
		if (state->failure == 5) {
			return false;
		}
		pending.resize(p_length);
		memcpy(pending.ptrw(), p_src, p_length);
		return true;
	}
	Error get_error() const override { return state->failure == 6 ? ERR_FILE_CANT_WRITE : OK; }
	void flush() override {
		state->visible.append_array(pending);
		if (state->failure == 9) {
			state->visible.push_back(0);
		}
		if (state->failure == 1 && !state->visible.is_empty()) {
			state->visible.resize(state->visible.size() - 1);
		}
		if (state->failure == 4 && !state->visible.is_empty()) {
			state->visible.write[0] ^= 1;
		}
	}
};

class AuditMemoryReader : public FileAccessMemory {
public:
	int failure = 0;
	Error get_error() const override { return failure == 8 ? ERR_FILE_CANT_READ : OK; }
	uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override {
		return FileAccessMemory::get_buffer(p_dst, failure == 7 && p_length ? p_length - 1 : p_length);
	}
};

static Ref<FileAccess> audit_test_open(const String &, FileAccess::ModeFlags p_mode, Error *r_error, void *p_context) {
	auto *state = static_cast<AuditIOState *>(p_context);
	if ((p_mode == FileAccess::READ_WRITE && state->failure == 2) || (p_mode == FileAccess::READ && state->failure == 3)) {
		*r_error = ERR_CANT_OPEN;
		return {};
	}
	*r_error = OK;
	if (p_mode == FileAccess::READ_WRITE) {
		return memnew(BufferedAuditFile(state));
	}
	Ref<AuditMemoryReader> reader;
	reader.instantiate();
	reader->failure = state->failure;
	reader->open_custom(state->visible.ptr(), state->visible.size());
	return reader;
}

static String new_audit_path() {
	Error error = OK;
	Ref<FileAccess> temporary = FileAccess::create_temp(FileAccess::READ_WRITE, "godot_policy_audit", "jsonl", false, &error);
	REQUIRE(error == OK);
	String path = temporary->get_path_absolute();
	temporary.unref();
	return path;
}
#endif

TEST_CASE("policy_flags [ShaderCompilePolicy]") {
	SUBCASE("Unknown and unsupported flags are never narrowed away") {
		const uint64_t requests[] = { 0, 1, 2, 3, uint64_t(1) << 40, uint64_t(UINT64_MAX) };
		for (uint64_t supported : { uint64_t(0), uint64_t(1) }) {
			for (uint64_t requested : requests) {
				CAPTURE(requested);
				CAPTURE(supported);
				CHECK(ShaderCompilePolicy::flags_valid(requested, supported) == (requested <= 1 && requested <= supported));
			}
		}
	}
	SUBCASE("Public policy methods are discoverable") {
		CHECK(ClassDB::has_method("RenderingDevice", "shader_compile_binary_from_spirv_with_options"));
		CHECK(ClassDB::has_method("RenderingDevice", "shader_create_from_spirv_with_options"));
		CHECK(ClassDB::has_method("RenderingDevice", "shader_get_compile_flags"));
	}
#ifdef TESTS_ENABLED
	SUBCASE("Unsupported bytecode never invokes the native backend") {
		RenderingDevice rd;
		PolicyDriver driver;
		TestShaderCompilePolicyAccess::set_driver(rd, &driver);
		Ref<CodecContainer> encoder;
		encoder.instantiate();
		encoder->initialize_fixture(1);
		ERR_PRINT_OFF;
		const RID shader = rd.shader_create_from_bytecode(encoder->to_bytes());
		ERR_PRINT_ON;
		TestShaderCompilePolicyAccess::set_driver(rd, nullptr);
		CHECK(shader.is_null());
		CHECK(driver.native_creates == 0);
	}
	SUBCASE("Real RD Ref/native and bytecode admission rejects before backend side effects") {
		RenderingDevice rd;
		PolicyDriver driver;
		TestShaderCompilePolicyAccess::set_driver(rd, &driver);
		Vector<RenderingDeviceCommons::ShaderStageSPIRVData> empty;
		Ref<RDShaderSPIRV> spirv;
		ERR_PRINT_OFF;
		CHECK(TestShaderCompilePolicyAccess::compile_ref(rd, spirv, "null", 0).is_empty());
		CHECK(TestShaderCompilePolicyAccess::create_ref(rd, spirv, "null", 0).is_null());
		spirv.instantiate();
		CHECK(TestShaderCompilePolicyAccess::compile_ref(rd, spirv, "empty", 0).is_empty());
		CHECK(TestShaderCompilePolicyAccess::create_ref(rd, spirv, "empty", 0).is_null());
		CHECK(rd.shader_compile_binary_from_spirv_with_options(empty, "empty", 0).is_empty());
		CHECK(rd.shader_create_from_spirv_with_options(empty, "empty", 0).is_null());
		ERR_PRINT_ON;
		CHECK(driver.format.creates == 0);
		RenderingDeviceCommons::ShaderStageSPIRVData stage;
		stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		stage.spirv.push_back(0);
		Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages{ stage };
		for (uint64_t flags : { uint64_t(1), uint64_t(2), uint64_t(3), uint64_t(1) << 40, uint64_t(UINT64_MAX) }) {
			ERR_PRINT_OFF;
			CHECK(rd.shader_compile_binary_from_spirv_with_options(stages, "unsupported", flags).is_empty());
			CHECK(rd.shader_create_from_spirv_with_options(stages, "unsupported", flags).is_null());
			ERR_PRINT_ON;
		}
		CHECK(driver.format.creates == 0);
		RID placeholder = rd.shader_create_placeholder();
		Ref<CodecContainer> encoder;
		encoder.instantiate();
		encoder->initialize_fixture(1);
		Vector<RenderingDevice::PipelineImmutableSampler> bad_samplers;
		RenderingDevice::PipelineImmutableSampler bad_sampler;
		bad_sampler.append_id(RID::from_uint64(99999));
		bad_samplers.push_back(bad_sampler);
		ERR_PRINT_OFF;
		CHECK(rd.shader_create_from_bytecode_with_samplers(encoder->to_bytes(), placeholder, bad_samplers).is_null());
		CHECK(static_cast<uint64_t>(rd.shader_get_compile_flags(placeholder)) == 0);
		CHECK(static_cast<uint64_t>(rd.shader_get_compile_flags(RID())) == 0);
		ERR_PRINT_ON;
		CHECK(driver.native_creates == 0);
		driver.format.supported = 1;
		ERR_PRINT_OFF;
		CHECK(rd.shader_create_from_bytecode_with_samplers(encoder->to_bytes(), RID::from_uint64(99999), bad_samplers).is_null());
		ERR_PRINT_ON;
		TestShaderCompilePolicyAccess::mark_filled(rd, placeholder);
		ERR_PRINT_OFF;
		CHECK(rd.shader_create_from_bytecode_with_samplers(encoder->to_bytes(), placeholder, bad_samplers).is_null());
		ERR_PRINT_ON;
		CHECK(driver.native_creates == 0);
		TestShaderCompilePolicyAccess::clear_shader(rd, placeholder);
		placeholder = rd.shader_create_placeholder();
		ERR_PRINT_OFF;
		CHECK(rd.shader_create_from_bytecode(encoder->to_bytes(), placeholder).is_null());
		CHECK(static_cast<uint64_t>(rd.shader_get_compile_flags(placeholder)) == 0);
		ERR_PRINT_ON;
		CHECK(driver.native_creates == 1);
		driver.native_accepts = true;
		CHECK(rd.shader_create_from_bytecode(encoder->to_bytes(), placeholder) == placeholder);
		CHECK(static_cast<uint64_t>(rd.shader_get_compile_flags(placeholder)) == 1);
		CHECK(driver.native_creates == 2);
		TestShaderCompilePolicyAccess::clear_shader(rd, placeholder);
		spirv->set_stage_compile_error(RenderingDeviceCommons::SHADER_STAGE_COMPUTE, "fixture error");
		const int before_error = driver.format.creates;
		ERR_PRINT_OFF;
		CHECK(TestShaderCompilePolicyAccess::compile_ref(rd, spirv, "error", 0).is_empty());
		CHECK(TestShaderCompilePolicyAccess::create_ref(rd, spirv, "error", 0).is_null());
		ERR_PRINT_ON;
		CHECK(driver.format.creates == before_error);
		spirv->set_stage_compile_error(RenderingDeviceCommons::SHADER_STAGE_COMPUTE, "");
		spirv->set_stage_bytecode(RenderingDeviceCommons::SHADER_STAGE_COMPUTE, policy_spirv(5));
		RID old = TestShaderCompilePolicyAccess::create_old_ref(rd, spirv, "legacy-nonempty-name");
		CHECK(old.is_valid());
		if (old.is_valid()) {
			CHECK(TestShaderCompilePolicyAccess::name(rd, old) == "legacy-nonempty-name");
			CHECK(static_cast<uint64_t>(rd.shader_get_compile_flags(old)) == 0);
			TestShaderCompilePolicyAccess::clear_shader(rd, old);
		}
		TestShaderCompilePolicyAccess::set_driver(rd, nullptr);
	}
#endif
	SUBCASE("Empty stage collection never reaches backend compilation") {
		Ref<CodecContainer> container;
		container.instantiate();
		Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages;
		ERR_PRINT_OFF;
		CHECK_FALSE(container->set_code_from_spirv("empty", stages));
		ERR_PRINT_ON;
		CHECK(container->compile_calls == 0);
	}
}

TEST_CASE("policy_container [ShaderCompilePolicy]") {
#ifdef TESTS_ENABLED
	SUBCASE("ShaderRD cache writer emits version five") {
		LocalVector<int> indices{ 0 };
		Vector<Vector<uint8_t>> data;
		data.resize(1);
		data.write[0].push_back(42);
		PackedByteArray bytes = ShaderRD::save_shader_cache_bytes(indices, data);
		REQUIRE(bytes.size() >= 8);
		CHECK(memcmp(bytes.ptr(), "GDSC", 4) == 0);
		CHECK(decode_uint32(bytes.ptr() + 4) == 5);
	}
	SUBCASE("Partial cache rollback clears loaded RIDs and only current group bytes") {
		for (int loaded_count : { 1, 2 }) {
			Vector<RID> variants;
			Vector<Vector<uint8_t>> bytes;
			variants.resize(6);
			bytes.resize(6);
			for (int i = 0; i < 6; i++) {
				variants.write[i] = RID::from_uint64(100 + i);
				bytes.write[i].push_back(i);
			}
			variants.write[1] = RID(); // Disabled variant between enabled variants.
			LocalVector<int> group{ 0, 1, 2, 3, 4 };
			LocalVector<int> loaded;
			loaded.push_back(0);
			if (loaded_count == 2) {
				loaded.push_back(2);
			}
			Vector<RID> freed;
			TestShaderCompilePolicyAccess::rollback(variants, bytes, group, loaded, freed);
			CHECK(freed.size() == loaded_count);
			CHECK(variants[0].is_null());
			CHECK(variants[1].is_null());
			CHECK(variants[2] == (loaded_count == 2 ? RID() : RID::from_uint64(102)));
			CHECK(variants[3] == RID::from_uint64(103));
			CHECK(variants[4] == RID::from_uint64(104));
			CHECK(variants[5] == RID::from_uint64(105));
			CHECK(bytes[5].size() == 1);
			for (int index : group) {
				CHECK(bytes[index].is_empty());
			}
		}
	}
#endif

	Ref<CodecContainer> container;
	container.instantiate();
	SUBCASE("Version two is rejected before backend callbacks") {
		PackedByteArray bytes;
		bytes.resize_initialized(20);
		encode_uint32(0x43535247, bytes.ptrw());
		encode_uint32(2, bytes.ptrw() + 4);
		encode_uint32(0x12345678, bytes.ptrw() + 8);
		encode_uint32(1, bytes.ptrw() + 12);
		ERR_PRINT_OFF;
		CHECK_FALSE(container->from_bytes(bytes));
		ERR_PRINT_ON;
		CHECK(container->header_calls == 0);
	}
	SUBCASE("Version three independent header and complete bounded roundtrip") {
		for (uint32_t flags : { 0U, 1U }) {
			Ref<CodecContainer> encoder;
			encoder.instantiate();
			encoder->initialize_fixture(flags);
			PackedByteArray bytes = encoder->to_bytes();
			const uint32_t expected[] = { 0x43535247, 3, 0x12345678, 1, 1, flags };
			for (int i = 0; i < 6; i++) {
				CHECK(decode_uint32(bytes.ptr() + i * 4) == expected[i]);
			}
			REQUIRE(container->from_bytes(bytes));
			CHECK(static_cast<uint64_t>(container->get_compile_flags()) == flags);
			CHECK(container->shaders[0].code_compressed_bytes[0] == 42);
			CHECK(container->shaders[0].code_decompressed_size == 0);
			for (int64_t size = 0; size < bytes.size(); size++) {
				CAPTURE(size);
				Ref<CodecContainer> decoder;
				decoder.instantiate();
				ERR_PRINT_OFF;
				CHECK_FALSE(decoder->from_bytes(bytes.slice(0, size)));
				ERR_PRINT_ON;
			}
			for (int field = 0; field < 6; field++) {
				PackedByteArray invalid = bytes;
				const uint32_t bad[] = { 0, 4, 0, 2, UINT32_MAX, 2 };
				encode_uint32(bad[field], invalid.ptrw() + field * 4);
				ERR_PRINT_OFF;
				CHECK_FALSE(container->from_bytes(invalid));
				ERR_PRINT_ON;
			}
			bytes.push_back(0);
			ERR_PRINT_OFF;
			CHECK_FALSE(container->from_bytes(bytes));
			ERR_PRINT_ON;
		}
	}
	SUBCASE("Noncanonical and malformed UTF-8 names are rejected before reserialization") {
		for (const char *name : { "\xef\xbb\xbf"
								  "e06:name",
					 "\xff"
					 "e06:name" }) {
			Ref<CodecContainer> encoder;
			encoder.instantiate();
			encoder->initialize_fixture(0);
			encoder->set_fixture_name(name);
			PackedByteArray bytes = encoder->to_bytes();
			ERR_PRINT_OFF;
			CHECK_FALSE(container->from_bytes(bytes));
			ERR_PRINT_ON;
		}
	}
#ifdef TESTS_ENABLED
	SUBCASE("Audit digest preserves the actual input including nonzero padding") {
		Ref<CodecContainer> encoder;
		encoder.instantiate();
		encoder->initialize_fixture(0);
		encoder->set_fixture_name("abc");
		PackedByteArray bytes = encoder->to_bytes();
		bytes.write[bytes.size() - 1] = 0xa5;
		REQUIRE(container->from_bytes(bytes));
		uint8_t digest[32];
		CryptoCore::sha256(bytes.ptr(), bytes.size(), digest);
		CHECK(container->get_loaded_container_digest() == String::hex_encode_buffer(digest, sizeof(digest)));
		CHECK(container->get_loaded_container_size() == uint64_t(bytes.size()));
		CHECK(container->to_bytes() != bytes);
		ERR_PRINT_OFF;
		CHECK_FALSE(container->from_bytes(bytes.slice(0, 8)));
		ERR_PRINT_ON;
		CHECK(container->get_loaded_container_digest().is_empty());
		CHECK(container->get_loaded_container_size() == 0);
	}
#endif
	const uint8_t source[] = { 1, 2, 3, 4 };
	uint8_t output[8] = {};
	SUBCASE("Raw payload must fill the complete declared output") {
		CHECK(container->decompress_code(source, 4, 0, output, 4));
		CHECK(memcmp(source, output, 4) == 0);
		ERR_PRINT_OFF;
		CHECK_FALSE(container->decompress_code(source, 4, 0, output, 8));
		ERR_PRINT_ON;
	}
	SUBCASE("Corrupt Zstd data must be rejected") {
		ERR_PRINT_OFF;
		CHECK_FALSE(container->decompress_code(source, 4, RenderingShaderContainer::COMPRESSION_FLAG_ZSTD, output, 4));
		ERR_PRINT_ON;
	}
	SUBCASE("Zstd short output must be rejected") {
		uint8_t compressed[256] = {};
		int size = Compression::compress(compressed, source, 4, Compression::MODE_ZSTD);
		REQUIRE(size > 0);
		CHECK(container->decompress_code(compressed, size, RenderingShaderContainer::COMPRESSION_FLAG_ZSTD, output, 4));
		ERR_PRINT_OFF;
		CHECK_FALSE(container->decompress_code(compressed, size, RenderingShaderContainer::COMPRESSION_FLAG_ZSTD, output, 8));
		ERR_PRINT_ON;
	}
}
TEST_CASE("policy_library_key [ShaderCompilePolicy]") {
	ShaderCompilePolicy::LibraryKeyInput input;
	input.stage = 4;
	input.source = String("kernel void main0() {}").to_utf8_buffer();
	input.msl_version = 30000;
	input.compiled_gpu_family = 1007;
	input.profile_min_os_version = 140000;
	input.actual_gpu_family = 1009;
	input.actual_os_version = 150000;
	const String expected = "d07836a6da961890cba3d6ec1717a85c8a805987e16476fa35d2d6fb07dcf75a";
	const auto digest = ShaderCompilePolicy::library_key(input);
	CHECK(String::hex_encode_buffer(digest.ptr(), digest.size()) == expected);
	CHECK(ShaderCompilePolicy::library_key(input) == ShaderCompilePolicy::library_key(input));
	const auto baseline = ShaderCompilePolicy::library_key(input);
	for (int dimension = 0; dimension < 19; dimension++) {
		CAPTURE(dimension);
		auto changed = input;
		switch (dimension) {
			case 0:
				changed.compile_path_version++;
				break;
			case 1:
				changed.stage++;
				break;
			case 2:
				changed.source.write[0] ^= 1;
				break;
			case 3:
				changed.source.push_back(0);
				break;
			case 4:
				changed.requested_flags++;
				break;
			case 5:
				changed.effective_disable++;
				break;
			case 6:
				changed.msl_version++;
				break;
			case 7:
				changed.position_invariance++;
				break;
			case 8:
				changed.enable_logging++;
				break;
			case 9:
				changed.compiled_platform++;
				break;
			case 10:
				changed.compiled_gpu_family++;
				break;
			case 11:
				changed.profile_min_os_version++;
				break;
			case 12:
				changed.binary_os_min_version--;
				break;
			case 13:
				changed.actual_platform++;
				break;
			case 14:
				changed.actual_gpu_family++;
				break;
			case 15:
				changed.actual_os_version++;
				break;
			case 16:
				changed.origin++;
				break;
			case 17:
				changed.library_size++;
				break;
			case 18:
				changed.metallib_sha256[0]++;
				break;
		}
		CHECK(ShaderCompilePolicy::library_key(changed) != baseline);
	}
}

#ifdef TESTS_ENABLED
static PackedByteArray policy_spirv(uint32_t p_model, bool p_multi) {
	Vector<uint32_t> words{ 0x07230203, 0x00010000, 0, 5, 0,
		0x00020011, 1, 0x0003000e, 0, 1,
		0x0005000f, p_model, 3, 0x6e69616d, 0 };
	if (p_multi) {
		const uint32_t other[] = { 0x0005000f, p_model, 3, 0x72656874, 0 };
		for (uint32_t word : other) {
			words.push_back(word);
		}
	}
	if (p_model == 5) {
		const uint32_t mode[] = { 0x00060010, 3, 17, 1, 1, 1 };
		for (uint32_t word : mode) {
			words.push_back(word);
		}
	} else if (p_model == 4) {
		words.push_back(0x00030010);
		words.push_back(3);
		words.push_back(7);
	}
	const uint32_t body[] = { 0x00020013, 1, 0x00030021, 2, 1, 0x00050036, 1, 3, 0, 2, 0x000200f8, 4, 0x000100fd, 0x00010038 };
	for (uint32_t word : body) {
		words.push_back(word);
	}
	PackedByteArray bytes;
	bytes.resize(words.size() * 4);
	for (int i = 0; i < words.size(); i++) {
		encode_uint32(words[i], bytes.ptrw() + i * 4);
	}
	return bytes;
}
#endif

TEST_CASE("policy_options [ShaderCompilePolicy]") {
#ifndef METAL_ENABLED
	CHECK_FALSE(ClassDB::class_exists("RenderingShaderCompileTestBridge"));
#endif
#ifdef TESTS_ENABLED
	SUBCASE("Audit exclusive creation, schema and latched failure") {
		String path = new_audit_path();
		Dictionary record;
		record["shader_name"] = "e06:unit";
		record["phase"] = "runtime_options";
		ShaderCompileAudit disabled("");
		CHECK(disabled.append(record) == 0);
		CHECK_FALSE(disabled.has_failed());
		ShaderCompileAudit sink(path);
		CHECK(sink.append(record) == 1);
		CHECK(sink.append(record) == 2);
		PackedByteArray before = FileAccess::get_file_as_bytes(path);
		ShaderCompileAudit duplicate(path);
		ERR_PRINT_OFF;
		CHECK(duplicate.append(record) == 0);
		ERR_PRINT_ON;
		CHECK(duplicate.has_failed());
		CHECK(FileAccess::get_file_as_bytes(path) == before);
		Dictionary parsed = JSON::parse_string(FileAccess::get_file_as_string(path).get_slice("\n", 0));
		CHECK(int(parsed["schema_version"]) == 1);
		CHECK(int(parsed["seq"]) == 1);
		CHECK(parsed.has("actual_options"));
		CHECK(DirAccess::remove_absolute(path) == OK);
		ShaderCompileAudit relative("relative.jsonl");
		ERR_PRINT_OFF;
		CHECK(relative.append(record) == 0);
		ERR_PRINT_ON;
		CHECK(relative.has_failed());
	}
	SUBCASE("Audit row and UTF-8 byte bounds are exact") {
		Dictionary record;
		record["shader_name"] = "e06:unit";
		String path = new_audit_path();
		AuditIOState state;
		ShaderCompileAudit sink(path, audit_test_open, &state);
		for (int i = 1; i <= 4096; i++) {
			REQUIRE(sink.append(record) == uint64_t(i));
		}
		ERR_PRINT_OFF;
		CHECK(sink.append(record) == 0);
		ERR_PRINT_ON;
		CHECK(sink.has_failed());
		CHECK(DirAccess::remove_absolute(path) == OK);

		// Mirror only the public schema, not the writer's byte-count implementation.
		Dictionary expected;
		expected["shader_name"] = "e06:unit";
		expected["schema_version"] = 1;
		expected["seq"] = 1;
		for (const String &field : { String("stage"), String("phase"), String("source_digest"), String("library_key"), String("required_flags"), String("effective_disable"), String("compile_path"), String("actual_options"), String("cache_hit") }) {
			expected[field] = Variant();
		}
		expected["payload"] = "";
		const int overhead = (JSON::stringify(expected) + "\n").utf8().length();
		for (int excess : { 0, 1 }) {
			String limit_path = new_audit_path();
			AuditIOState limit_state;
			ShaderCompileAudit limit_sink(limit_path, audit_test_open, &limit_state);
			Dictionary at_limit = record.duplicate();
			at_limit["payload"] = String("x").repeat(4194304 - overhead - 3 + excess) + String::utf8("中");
			ERR_PRINT_OFF;
			CHECK(limit_sink.append(at_limit) == (excess ? 0 : 1));
			ERR_PRINT_ON;
			CHECK(limit_sink.has_failed() == bool(excess));
			CHECK(DirAccess::remove_absolute(limit_path) == OK);
		}
	}
	SUBCASE("Concurrent audit sinks never truncate the winner") {
		String path = new_audit_path();
		ShaderCompileAudit first(path);
		ShaderCompileAudit second(path);
		Dictionary first_record;
		first_record["shader_name"] = "e06:first";
		Dictionary second_record;
		second_record["shader_name"] = "e06:second";
		uint64_t one = 0;
		uint64_t two = 0;
		ERR_PRINT_OFF;
		std::thread a([&] { one = first.append(first_record); });
		std::thread b([&] { two = second.append(second_record); });
		a.join();
		b.join();
		ERR_PRINT_ON;
		CHECK(one + two == 1);
		CHECK(first.has_failed() != second.has_failed());
		Dictionary parsed = JSON::parse_string(FileAccess::get_file_as_string(path).strip_edges());
		CHECK(String(parsed["shader_name"]) == (one ? "e06:first" : "e06:second"));
		CHECK(DirAccess::remove_absolute(path) == OK);
	}
	SUBCASE("Audit verifies independently readable bytes after apparently successful flush") {
		for (int failure : { 1, 2, 3, 4, 5, 6, 7, 8, 9 }) {
			CAPTURE(failure);
			AuditIOState state;
			state.failure = failure;
			String path = new_audit_path();
			ShaderCompileAudit sink(path, audit_test_open, &state);
			Dictionary record;
			record["shader_name"] = "e06:unit";
			ERR_PRINT_OFF;
			CHECK(sink.append(record) == 0);
			ERR_PRINT_ON;
			CHECK(sink.has_failed());
			state.failure = 0;
			CHECK(sink.append(record) == 0);
			CHECK(DirAccess::remove_absolute(path) == OK);
		}
	}
#if defined(METAL_ENABLED)
#ifdef MODULE_GLSLANG_ENABLED
	SUBCASE("Unsupported export targets fail for both explicit and implicit safe policy before tools") {
		String error;
		RenderingDeviceCommons::ShaderStageSPIRVData stage;
		stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		stage.spirv = compile_glslang_shader(stage.shader_stage, "#version 450\nlayout(local_size_x=1) in; layout(set=0,binding=0,r32ui) uniform uimage2D target; void main(){ imageAtomicAdd(target,ivec2(0),1u); }", RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1, RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3, &error);
		CAPTURE(error);
		REQUIRE_FALSE(stage.spirv.is_empty());
		const auto *profile = MetalDeviceProfile::get_profile(MetalDeviceProfile::Platform::macOS, MetalDeviceProfile::GPU::Apple5, MinOsVersion(12, 0));
		RenderingShaderContainerFormatMetal format(profile, true);
		for (bool preserve : { false, true }) {
			auto controlled_stage = stage;
			if (preserve) {
				// Insert a genuine SignedZeroInfNanPreserve execution mode beside LocalSize.
				for (int offset = 20; offset < controlled_stage.spirv.size();) {
					const uint32_t instruction = decode_uint32(controlled_stage.spirv.ptr() + offset);
					if ((instruction & 0xffff) == 16) {
						PackedByteArray mode;
						mode.resize(16);
						encode_uint32(0x00040010, mode.ptrw());
						encode_uint32(decode_uint32(controlled_stage.spirv.ptr() + offset + 4), mode.ptrw() + 4);
						encode_uint32(4461, mode.ptrw() + 8);
						encode_uint32(32, mode.ptrw() + 12);
						mode.append_array(controlled_stage.spirv.slice(offset));
						controlled_stage.spirv = controlled_stage.spirv.slice(0, offset);
						controlled_stage.spirv.append_array(mode);
						break;
					}
					offset += (instruction >> 16) * 4;
				}
			}
			for (uint64_t flags : { uint64_t(0), uint64_t(1) }) {
				CAPTURE(flags);
				CAPTURE(preserve);
				Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages{ controlled_stage };
				Ref<RenderingShaderContainerMetal> container = format.create_container();
				ERR_PRINT_OFF;
				CHECK(container->set_code_from_spirv("e06:unsupported", stages, flags) == (!preserve && !flags));
				ERR_PRINT_ON;
				if (!preserve && !flags) {
					CHECK(container->is_invalid());
					PackedByteArray invalid_cache = container->to_bytes();
					CHECK_FALSE(invalid_cache.is_empty());
					Ref<RenderingShaderContainerMetal> reload = format.create_container();
					ERR_PRINT_OFF;
					CHECK_FALSE(reload->from_bytes(invalid_cache));
					ERR_PRINT_ON;
				}
				const auto &tools = container->get_offline_test_result();
				CHECK_FALSE(tools.property_probe.executed);
				CHECK_FALSE(tools.metal.executed);
				CHECK_FALSE(tools.strip.executed);
			}
		}
	}
	SUBCASE("NoContraction conversion retains precise operations without fast pragmas") {
		String error;
		RenderingDeviceCommons::ShaderStageSPIRVData stage;
		stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		stage.spirv = compile_glslang_shader(stage.shader_stage, "#version 450\nlayout(local_size_x=1) in; layout(set=0,binding=0,std430) buffer Data { float a; float b; float c; float result; } data; void main(){ precise float value=data.a*data.b+data.c; data.result=value; }", RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1, RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3, &error);
		CAPTURE(error);
		REQUIRE_FALSE(stage.spirv.is_empty());
		bool no_contraction = false;
		for (int offset = 20; offset < stage.spirv.size();) {
			const uint32_t instruction = decode_uint32(stage.spirv.ptr() + offset);
			if ((instruction & 0xffff) == 71 && (instruction >> 16) >= 3) {
				no_contraction |= decode_uint32(stage.spirv.ptr() + offset + 8) == 42;
			}
			offset += (instruction >> 16) * 4;
		}
		REQUIRE(no_contraction);
		const auto *profile = MetalDeviceProfile::get_profile(MetalDeviceProfile::Platform::macOS, MetalDeviceProfile::GPU::Apple7, MinOsVersion(14, 0));
		RenderingShaderContainerFormatMetal format(profile, false);
		Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages{ stage };
		for (uint64_t flags : { uint64_t(0), uint64_t(1) }) {
			Ref<RenderingShaderContainerMetal> container = format.create_container();
			REQUIRE(container->set_code_from_spirv("e06:precise", stages, flags));
			const auto &shader = container->shaders[0];
			PackedByteArray code;
			code.resize(shader.code_decompressed_size);
			REQUIRE(container->decompress_code(shader.code_compressed_bytes.ptr(), shader.code_compressed_bytes.size(), shader.code_compression_flags, code.ptrw(), code.size()));
			String source = String::utf8(reinterpret_cast<const char *>(code.ptr()), container->mtl_shaders[0].source_size);
			CHECK_FALSE(source.contains("#pragma clang fp contract(fast)"));
			CHECK_FALSE(source.contains("#pragma clang fp reassociate(on)"));
			CHECK(source.contains("spvFMul"));
			CHECK(source.contains("spvFAdd"));
		}
	}
	SUBCASE("Real Metal reflection extras and malicious profile/count encodings") {
		const auto *profile = MetalDeviceProfile::get_profile(MetalDeviceProfile::Platform::macOS, MetalDeviceProfile::GPU::Apple7, MinOsVersion(14, 0));
		RenderingShaderContainerFormatMetal format(profile, false);
		String error;
		RenderingDeviceCommons::ShaderStageSPIRVData stage;
		stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		stage.spirv = compile_glslang_shader(stage.shader_stage, "#version 450\nlayout(local_size_x=1) in; layout(constant_id=0) const uint factor=2; layout(set=0,binding=0,std430) buffer Data { uint value; } data; void main(){ data.value *= factor; }", RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1, RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3, &error);
		CAPTURE(error);
		REQUIRE_FALSE(stage.spirv.is_empty());
		Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages{ stage };
		Ref<RenderingShaderContainerMetal> encoder = format.create_container();
		REQUIRE(encoder->set_code_from_spirv("e06:codec", stages, 1));
		const auto reflection = encoder->get_shader_reflection();
		REQUIRE(reflection.uniform_sets.size() == 1);
		REQUIRE(reflection.specialization_constants.size() == 1);
		PackedByteArray bytes = encoder->to_bytes();
		Ref<RenderingShaderContainerMetal> decoder = format.create_container();
		REQUIRE(decoder->from_bytes(bytes));
		CHECK(decoder->to_bytes() == bytes);
		// v3's fixed base reflection record starts immediately after its 24-byte header.
		for (uint32_t offset : { 24U + 12U, 24U + 40U, 24U + 52U, 24U + 56U }) {
			PackedByteArray malformed = bytes;
			encode_uint32(UINT32_MAX, malformed.ptrw() + offset);
			ERR_PRINT_OFF;
			CHECK_FALSE(decoder->from_bytes(malformed));
			ERR_PRINT_ON;
		}

		for (int64_t size = 0; size < bytes.size(); size++) {
			CAPTURE(size);
			ERR_PRINT_OFF;
			CHECK_FALSE(decoder->from_bytes(bytes.slice(0, size)));
			ERR_PRINT_ON;
		}
		for (uint8_t invalid_bool : { uint8_t(2), uint8_t(255) }) {
			for (size_t member : { offsetof(MetalDeviceProfile::Features, use_argument_buffers), offsetof(MetalDeviceProfile::Features, simdPermute) }) {
				PackedByteArray malformed = bytes;
				const size_t offset = 24 + 64 + offsetof(RenderingShaderContainerMetal::HeaderData, profile) + offsetof(MetalDeviceProfile, features) + member;
				malformed.write[offset] = invalid_bool;
				ERR_PRINT_OFF;
				CHECK_FALSE(decoder->from_bytes(malformed));
				ERR_PRINT_ON;
			}
		}
		const auto original = encoder->mtl_reflection_data;
		for (bool lower_target : { false, true }) {
			for (bool argument_buffers : { false, true }) {
				encoder->mtl_reflection_data = original;
				encoder->mtl_reflection_data.profile.features.msl_version = lower_target ? 30000 : original.profile.features.msl_version;
				encoder->mtl_reflection_data.msl_version = encoder->mtl_reflection_data.profile.features.msl_version;
				encoder->mtl_reflection_data.profile.features.use_argument_buffers = argument_buffers;
				CHECK(decoder->from_bytes(encoder->to_bytes()));
			}
		}

		for (int invalid = 0; invalid < 8; invalid++) {
			encoder->mtl_reflection_data = original;
			auto &header = encoder->mtl_reflection_data;
			switch (invalid) {
				case 0:
					header.msl_version = 0;
					break;
				case 1:
					header.msl_version = 30001;
					break;
				case 2:
					header.profile.platform = MetalDeviceProfile::Platform(3);
					break;
				case 3:
					header.profile.gpu = MetalDeviceProfile::GPU(1010);
					break;
				case 4:
					header.profile.min_os_version = MinOsVersion(0);
					break;
				case 5:
					header.profile.min_os_version = MinOsVersion();
					break;
				case 6:
					header.profile.features.msl_version = 30001;
					break;
				case 7:
					header.flags = 8;
					break;
			}
			CAPTURE(invalid);
			ERR_PRINT_OFF;
			CHECK_FALSE(decoder->from_bytes(encoder->to_bytes()));
			ERR_PRINT_ON;
		}
	}
#endif
	SUBCASE("Production offline arguments use effective policy and invariance") {
		for (uint64_t flags : { uint64_t(0), uint64_t(1) }) {
			for (bool supports_fast : { false, true }) {
				for (bool invariant : { false, true }) {
					RenderingShaderContainerMetal::StageData stage;
					stage.effective_disable_fast_math = ShaderCompilePolicy::effective_disable(flags, supports_fast);
					stage.is_position_invariant = invariant;
					List<String> args;
					RenderingShaderContainerMetal::append_compile_policy_arguments(args, stage);
					CHECK(args.size() == (invariant ? 2 : 1));
					CHECK(args.back()->get() == (flags || !supports_fast ? "-fmetal-math-mode=safe" : "-fmetal-math-mode=fast"));
					if (invariant) {
						CHECK(args.front()->get() == "-fpreserve-invariance");
					}
				}
			}
		}
	}
	SUBCASE("Offline tool success requires execution, OS success, raw zero and nonempty product") {
		using Result = RenderingShaderContainerMetal::OfflineCompileTestResult::ToolResult;
		CHECK_FALSE(Result{}.succeeded());
		CHECK_FALSE((Result{ true, FAILED, 0, 10 }).succeeded());
		CHECK_FALSE((Result{ true, OK, 1, 10 }).succeeded());
		CHECK_FALSE((Result{ true, OK, 0, 0 }).succeeded());
		CHECK((Result{ true, OK, 0, 10 }).succeeded());
	}
	SUBCASE("Actual SPIR-V execution model is checked before tools") {
		const auto *profile = MetalDeviceProfile::get_profile(MetalDeviceProfile::Platform::macOS, MetalDeviceProfile::GPU::Apple7, MinOsVersion(14, 0));
		RenderingShaderContainerFormatMetal format(profile, true);
		for (int model : { 0, 4, 5 }) {
			for (bool multi : { false, true }) {
				Ref<RenderingShaderContainerMetal> container = format.create_container();
				Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages;
				RenderingDeviceCommons::ShaderStageSPIRVData stage;
				stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
				stage.spirv = policy_spirv(model, multi);
				stages.push_back(stage);
				CHECK(container->validate_test_compute_spirv("e06:unit", stages) == (model == 5 && !multi));
				if (model != 5 || multi) {
					Ref<RenderingShaderCompileTestBridge> bridge;
					bridge.instantiate();
					const Dictionary result = bridge->bake_metal_compute(stage.spirv, "e06:01234567-89ab-cdef-0123-456789abcdef:test:1", 0);
					CHECK(String(result["status"]) == "Failed");
					CHECK(String(result["cause"]) == "InvalidComputeModule");
				}

				const auto &results = container->get_offline_test_result();
				CHECK_FALSE(results.property_probe.executed);
				CHECK_FALSE(results.metal.executed);
				CHECK_FALSE(results.strip.executed);
			}
		}
	}
	SUBCASE("Bridge independently rejects input bounds and returns fixed profile schema") {
		CHECK(ClassDB::has_method("RenderingShaderCompileTestBridge", "bake_metal_compute"));
		Ref<RenderingShaderCompileTestBridge> bridge;
		bridge.instantiate();
		const String prefix = "e06:01234567-89ab-cdef-0123-456789abcdef:";
		const String valid_name = prefix + "test:1";
		const PackedByteArray valid_spirv = policy_spirv(5);
		auto rejected = [&](const PackedByteArray &p_spirv, const String &p_name, int64_t p_flags) {
			Dictionary result = bridge->bake_metal_compute(p_spirv, p_name, p_flags);
			CHECK(String(result["status"]) == "Failed");
			CHECK(String(result["cause"]) == "InvalidInput");
			CHECK(int(result["schema_version"]) == 1);
			CHECK(String(result["shader_name"]) == p_name);
			CHECK(int64_t(result["flags"]) == p_flags);
			CHECK(PackedByteArray(result["container_bytes"]).is_empty());
			CHECK(int(result["library_size"]) == 0);
			Dictionary profile = result["profile"];
			CHECK(int(profile["platform"]) == 0);
			CHECK(int(profile["gpu"]) == 1007);
			CHECK(int(profile["min_os_version"]) == 140000);
		};
		rejected({}, valid_name, 0);
		PackedByteArray unaligned = valid_spirv;
		unaligned.push_back(0);
		rejected(unaligned, valid_name, 0);
		PackedByteArray oversize;
		oversize.resize_initialized(1048580);
		rejected(oversize, valid_name, 0);
		for (int64_t flags : { int64_t(-1), int64_t(2), int64_t(3), int64_t(1) << 40 }) {
			rejected(valid_spirv, valid_name, flags);
		}
		for (const String &name : {
					 String("e07:01234567-89ab-cdef-0123-456789abcdef:test:1"),
					 String("e06:0123456789ab-cdef-0123-456789abcdef:test:1"),
					 String("e06:g1234567-89ab-cdef-0123-456789abcdef:test:1"),
					 prefix + ":1", prefix + String("a").repeat(65) + ":1", prefix + "bad.artifact:1", prefix + "中:1",
					 prefix + "test:", prefix + "test:0", prefix + "test:-1", prefix + "test:4294967296", prefix + "test:1x",
					 prefix + "test:1:extra", prefix + "test:" + String("0").repeat(256) + "1" }) {
			CAPTURE(name);
			rejected(valid_spirv, name, 0);
		}
		Dictionary threaded;
		std::thread worker([&] { threaded = bridge->bake_metal_compute(valid_spirv, valid_name, 0); });
		worker.join();
		CHECK(String(threaded["cause"]) == "InvalidInput");
		CHECK(PackedByteArray(threaded["container_bytes"]).is_empty());
		// Valid boundary names/flags use a noncompute module so no tool can start.
		for (const String &name : { prefix + "A_9-:4294967295", prefix + String("a").repeat(64) + ":1" }) {
			for (int flags : { 0, 1 }) {
				Dictionary result = bridge->bake_metal_compute(policy_spirv(0), name, flags);
				CHECK(String(result["cause"]) == "InvalidComputeModule");
			}
		}
	}
#endif
#endif

#ifdef METAL_ENABLED
	SUBCASE("Actual stage options are independent and preserve safe mode") {
		auto safe = make_shader_compile_options(30000, false, true, true);
		auto fast = make_shader_compile_options(30000, false, false, false);
		CHECK(safe.get() != fast.get());
		CHECK(safe->preserveInvariance());
		CHECK_FALSE(fast->preserveInvariance());
		CHECK(safe->languageVersion() == MTL::LanguageVersion3_0);
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
		if (__builtin_available(macOS 15.0, *)) {
			CHECK(safe->mathMode() == MTL::MathModeSafe);
			CHECK(fast->mathMode() == MTL::MathModeFast);
		} else
#endif
		{
			CHECK_FALSE(safe->fastMathEnabled());
			CHECK(fast->fastMathEnabled());
		}
	}
#endif

	for (uint64_t requested : { uint64_t(0), uint64_t(1) }) {
		for (bool supports_fast_math : { false, true }) {
			CAPTURE(requested);
			CAPTURE(supports_fast_math);
			CHECK(ShaderCompilePolicy::effective_disable(requested, supports_fast_math) == (requested == 1 || !supports_fast_math));
		}
	}
}

} // namespace TestShaderCompilePolicy
