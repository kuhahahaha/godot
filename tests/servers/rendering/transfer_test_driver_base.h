/**************************************************************************/
/*  transfer_test_driver_base.h                                           */
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

#pragma once

#include "servers/rendering/rendering_device_driver.h"

#include <cstdlib>

// Test-only native boundary. Unexpected driver operations fail immediately;
// only the operations used by the transfer fixture are implemented below.
class TransferTestDriverBase : public RenderingDeviceDriver {
	[[noreturn]] static void _unexpected_operation(const char *p_operation) {
		CRASH_NOW_MSG(String("Unexpected transfer test driver operation: ") + p_operation);
		// MSVC's debugger break can return when execution is resumed.
		std::abort();
	}

public:
	Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override { _unexpected_operation(__func__); }
	BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn) override { _unexpected_operation(__func__); }
	bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override { _unexpected_operation(__func__); }
	void buffer_free(BufferID p_buffer) override { _unexpected_operation(__func__); }
	uint64_t buffer_get_allocation_size(BufferID p_buffer) override { _unexpected_operation(__func__); }
	uint8_t *buffer_map(BufferID p_buffer) override { _unexpected_operation(__func__); }
	void buffer_unmap(BufferID p_buffer) override { _unexpected_operation(__func__); }
	uint8_t *buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) override { _unexpected_operation(__func__); }
	uint64_t buffer_get_dynamic_offsets(Span<BufferID> p_buffers) override { _unexpected_operation(__func__); }
	uint64_t buffer_get_device_address(BufferID p_buffer) override { _unexpected_operation(__func__); }
	TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view) override { _unexpected_operation(__func__); }
	TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps) override { _unexpected_operation(__func__); }
	TextureID texture_create_shared(TextureID p_original_texture, const TextureView &p_view) override { _unexpected_operation(__func__); }
	TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) override { _unexpected_operation(__func__); }
	void texture_free(TextureID p_texture) override { _unexpected_operation(__func__); }
	uint64_t texture_get_allocation_size(TextureID p_texture) override { _unexpected_operation(__func__); }
	void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) override { _unexpected_operation(__func__); }
	Vector<uint8_t> texture_get_data(TextureID p_texture, uint32_t p_layer) override { _unexpected_operation(__func__); }
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override { _unexpected_operation(__func__); }
	bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) override { _unexpected_operation(__func__); }
	SamplerID sampler_create(const SamplerState &p_state) override { _unexpected_operation(__func__); }
	void sampler_free(SamplerID p_sampler) override { _unexpected_operation(__func__); }
	bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override { _unexpected_operation(__func__); }
	VertexFormatID vertex_format_create(Span<VertexAttribute> p_vertex_attribs, const VertexAttributeBindingsMap &p_vertex_bindings) override { _unexpected_operation(__func__); }
	void vertex_format_free(VertexFormatID p_vertex_format) override { _unexpected_operation(__func__); }
	void command_pipeline_barrier(CommandBufferID p_cmd_buffer, BitField<PipelineStageBits> p_src_stages, BitField<PipelineStageBits> p_dst_stages, VectorView<MemoryAccessBarrier> p_memory_barriers, VectorView<BufferBarrier> p_buffer_barriers, VectorView<TextureBarrier> p_texture_barriers, VectorView<AccelerationStructureBarrier> p_acceleration_structure_barriers) override { _unexpected_operation(__func__); }
	FenceID fence_create() override { _unexpected_operation(__func__); }
	Error fence_wait(FenceID p_fence) override { _unexpected_operation(__func__); }
	void fence_free(FenceID p_fence) override { _unexpected_operation(__func__); }
	SemaphoreID semaphore_create() override { _unexpected_operation(__func__); }
	void semaphore_free(SemaphoreID p_semaphore) override { _unexpected_operation(__func__); }
	CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) override { _unexpected_operation(__func__); }
	CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) override { _unexpected_operation(__func__); }
	Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) override { _unexpected_operation(__func__); }
	void command_queue_free(CommandQueueID p_cmd_queue) override { _unexpected_operation(__func__); }
	CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override { _unexpected_operation(__func__); }
	bool command_pool_reset(CommandPoolID p_cmd_pool) override { _unexpected_operation(__func__); }
	void command_pool_free(CommandPoolID p_cmd_pool) override { _unexpected_operation(__func__); }
	CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override { _unexpected_operation(__func__); }
	bool command_buffer_begin(CommandBufferID p_cmd_buffer) override { _unexpected_operation(__func__); }
	bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override { _unexpected_operation(__func__); }
	void command_buffer_end(CommandBufferID p_cmd_buffer) override { _unexpected_operation(__func__); }
	void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override { _unexpected_operation(__func__); }
	SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) override { _unexpected_operation(__func__); }
	Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override { _unexpected_operation(__func__); }
	FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) override { _unexpected_operation(__func__); }
	RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override { _unexpected_operation(__func__); }
	DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override { _unexpected_operation(__func__); }
	ColorSpace swap_chain_get_color_space(SwapChainID p_swap_chain) override { _unexpected_operation(__func__); }
	bool swap_chain_get_hdr_output_supported(SwapChainID p_swap_chain) override { _unexpected_operation(__func__); }
	void swap_chain_free(SwapChainID p_swap_chain) override { _unexpected_operation(__func__); }
	FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override { _unexpected_operation(__func__); }
	void framebuffer_free(FramebufferID p_framebuffer) override { _unexpected_operation(__func__); }
	ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &p_shader_container, const Vector<ImmutableSampler> &p_immutable_samplers) override { _unexpected_operation(__func__); }
	void shader_free(ShaderID p_shader) override { _unexpected_operation(__func__); }
	void shader_destroy_modules(ShaderID p_shader) override { _unexpected_operation(__func__); }
	UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) override { _unexpected_operation(__func__); }
	void uniform_set_free(UniformSetID p_uniform_set) override { _unexpected_operation(__func__); }
	uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) const override { _unexpected_operation(__func__); }
	void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override { _unexpected_operation(__func__); }
	void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override { _unexpected_operation(__func__); }
	void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override { _unexpected_operation(__func__); }
	void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override { _unexpected_operation(__func__); }
	void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override { _unexpected_operation(__func__); }
	void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override { _unexpected_operation(__func__); }
	void command_clear_depth_stencil_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, float p_depth, uint8_t p_stencil, const TextureSubresourceRange &p_subresources) override { _unexpected_operation(__func__); }
	void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override { _unexpected_operation(__func__); }
	void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override { _unexpected_operation(__func__); }
	void pipeline_free(PipelineID p_pipeline) override { _unexpected_operation(__func__); }
	void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) override { _unexpected_operation(__func__); }
	bool pipeline_cache_create(const Vector<uint8_t> &p_data) override { _unexpected_operation(__func__); }
	void pipeline_cache_free() override { _unexpected_operation(__func__); }
	size_t pipeline_cache_query_size() override { _unexpected_operation(__func__); }
	Vector<uint8_t> pipeline_cache_serialize() override { _unexpected_operation(__func__); }
	RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment) override { _unexpected_operation(__func__); }
	void render_pass_free(RenderPassID p_render_pass) override { _unexpected_operation(__func__); }
	void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override { _unexpected_operation(__func__); }
	void command_end_render_pass(CommandBufferID p_cmd_buffer) override { _unexpected_operation(__func__); }
	void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override { _unexpected_operation(__func__); }
	void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) override { _unexpected_operation(__func__); }
	void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) override { _unexpected_operation(__func__); }
	void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) override { _unexpected_operation(__func__); }
	void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override { _unexpected_operation(__func__); }
	void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) override { _unexpected_operation(__func__); }
	void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) override { _unexpected_operation(__func__); }
	void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override { _unexpected_operation(__func__); }
	void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override { _unexpected_operation(__func__); }
	void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override { _unexpected_operation(__func__); }
	void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override { _unexpected_operation(__func__); }
	void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override { _unexpected_operation(__func__); }
	void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets, uint64_t p_dynamic_offsets) override { _unexpected_operation(__func__); }
	void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override { _unexpected_operation(__func__); }
	void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) override { _unexpected_operation(__func__); }
	void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override { _unexpected_operation(__func__); }
	PipelineID render_pipeline_create(ShaderID p_shader, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, PipelineRasterizationState p_rasterization_state, PipelineMultisampleState p_multisample_state, PipelineDepthStencilState p_depth_stencil_state, PipelineColorBlendState p_blend_state, VectorView<int32_t> p_color_attachments, BitField<PipelineDynamicStateFlags> p_dynamic_state, RenderPassID p_render_pass, uint32_t p_render_subpass, VectorView<PipelineSpecializationConstant> p_specialization_constants) override { _unexpected_operation(__func__); }
	void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override { _unexpected_operation(__func__); }
	void command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) override { _unexpected_operation(__func__); }
	void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) override { _unexpected_operation(__func__); }
	void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) override { _unexpected_operation(__func__); }
	PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) override { _unexpected_operation(__func__); }
	AccelerationStructureID blas_create(VectorView<AccelerationStructureGeometry> p_geometries, BitField<AccelerationStructureFlagBits> p_flags) override { _unexpected_operation(__func__); }
	AccelerationStructureID tlas_create(uint32_t p_max_instance_count, BitField<AccelerationStructureFlagBits> p_flags) override { _unexpected_operation(__func__); }
	void acceleration_structure_instance_write(uint8_t *r_driver_instance, const AccelerationStructureInstance &p_instance) override { _unexpected_operation(__func__); }
	void acceleration_structure_free(AccelerationStructureID p_acceleration_structure) override { _unexpected_operation(__func__); }
	uint32_t acceleration_structure_get_scratch_size_bytes(AccelerationStructureID p_acceleration_structure) override { _unexpected_operation(__func__); }
	RaytracingPipelineID raytracing_pipeline_create(VectorView<PipelineShader> p_shaders, VectorView<uint32_t> p_raygen_shader_indices, VectorView<uint32_t> p_miss_shader_indices, VectorView<HitGroup> p_hit_groups, uint32_t p_max_trace_recursion_depth, ShaderID p_layout_defining_shader) override { _unexpected_operation(__func__); }
	void raytracing_pipeline_free(RaytracingPipelineID p_pipeline) override { _unexpected_operation(__func__); }
	bool raytracing_pipeline_get_shader_group_handles(RaytracingPipelineID p_pipeline, uint32_t p_group_index_offset, VectorView<uint32_t> p_group_indices, uint8_t *r_data, uint32_t p_data_stride_bytes) override { _unexpected_operation(__func__); }
	void command_build_blas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer) override { _unexpected_operation(__func__); }
	void command_build_tlas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer, BufferID p_instance_buffer, uint32_t p_instance_offset, uint32_t p_instance_count) override { _unexpected_operation(__func__); }
	void command_bind_raytracing_pipeline(CommandBufferID p_cmd_buffer, RaytracingPipelineID p_pipeline) override { _unexpected_operation(__func__); }
	void command_bind_raytracing_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override { _unexpected_operation(__func__); }
	void command_trace_rays(CommandBufferID p_cmd_buffer, const ShaderBindingTable &p_raygen_sbt, const ShaderBindingTable &p_miss_sbt, const ShaderBindingTable &p_hit_sbt, uint32_t p_width, uint32_t p_height, uint32_t p_depth) override { _unexpected_operation(__func__); }
	QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override { _unexpected_operation(__func__); }
	void timestamp_query_pool_free(QueryPoolID p_pool_id) override { _unexpected_operation(__func__); }
	void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) override { _unexpected_operation(__func__); }
	uint64_t timestamp_query_result_to_time(uint64_t p_result) override { _unexpected_operation(__func__); }
	void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override { _unexpected_operation(__func__); }
	void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override { _unexpected_operation(__func__); }
	void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) override { _unexpected_operation(__func__); }
	void command_end_label(CommandBufferID p_cmd_buffer) override { _unexpected_operation(__func__); }
	void command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) override { _unexpected_operation(__func__); }
	void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) override { _unexpected_operation(__func__); }
	void end_segment() override { _unexpected_operation(__func__); }
	void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override { _unexpected_operation(__func__); }
	uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override { _unexpected_operation(__func__); }
	uint64_t get_total_memory_used() override { _unexpected_operation(__func__); }
	uint64_t get_lazily_memory_used() override { _unexpected_operation(__func__); }
	uint64_t limit_get(Limit p_limit) override { _unexpected_operation(__func__); }
	bool has_feature(Features p_feature) override { _unexpected_operation(__func__); }
	const MultiviewCapabilities &get_multiview_capabilities() override { _unexpected_operation(__func__); }
	const FragmentShadingRateCapabilities &get_fragment_shading_rate_capabilities() override { _unexpected_operation(__func__); }
	const FragmentDensityMapCapabilities &get_fragment_density_map_capabilities() override { _unexpected_operation(__func__); }
	String get_api_name() const override { _unexpected_operation(__func__); }
	String get_api_version() const override { _unexpected_operation(__func__); }
	String get_pipeline_cache_uuid() const override { _unexpected_operation(__func__); }
	const Capabilities &get_capabilities() const override { _unexpected_operation(__func__); }
	const RenderingShaderContainerFormat &get_shader_container_format() const override { _unexpected_operation(__func__); }
};
