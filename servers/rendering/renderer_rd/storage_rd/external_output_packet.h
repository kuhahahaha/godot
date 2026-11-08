/**************************************************************************/
/*  external_output_packet.h                                              */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

class RenderDataRD;
namespace RendererSceneRenderImplementation {
class RenderForwardClustered;
class RenderForwardMobile;
class SceneShaderForwardClustered;
class SceneShaderForwardMobile;
} //namespace RendererSceneRenderImplementation

namespace RendererRD {
class ParticlesStorage;
class ExternalOutputOwner;

// Native categories, not the schema1 pass-mask wire values.
enum class ExternalPacketPass : uint8_t { OPAQUE,
	TRANSPARENT,
	DEPTH,
	SHADOW,
	PROBE,
	MOTION };

// Exact native pipeline requirements. No hash alone authenticates a selection.
struct ExternalPacketPipelineRequest {
	enum class Backend : uint8_t { CLUSTERED,
		MOBILE };
	Backend backend = Backend::CLUSTERED;
	ExternalPacketPass pass = ExternalPacketPass::OPAQUE;
	RenderingDevice::FramebufferFormatID framebuffer_format = 0;
	uint32_t subpass = 0, version = 0, color_flags = 0;
	uint32_t specialization[3] = {};
	uint32_t view_count = 1;
	bool wireframe = false, reverse_cull = false, double_sided_shadow = false;
};

// Frozen by the original native material classifier while its C++ source is
// alive. These are native facts, not schema1 pass-mask bits or author input.
struct ExternalMaterialPassClassification {
	bool opaque = false, transparent = false, depth = false, shadow = false;
	bool subsurface = false, screen_texture = false, depth_texture = false;
	bool normal_texture = false, double_sided_shadow = false, motion = false;
	bool stencil = false, uses_time = false;
};

// Strong future-cache lease: its reverse edge occupies an original finite
// owner cell. Selectors are diagnostics, never authority to issue a packet.
class ExternalSurfaceSelection {
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
#endif
	friend class RendererSceneRenderImplementation::RenderForwardClustered;
	friend class RendererSceneRenderImplementation::RenderForwardMobile;
	friend class ParticlesStorage;
	friend class ExternalOutputOwner;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0, generation = 0;
	RID mesh, material_selection;
	const MeshStorage::NativeSurfaceHold *geometry_source = nullptr; // Stable original owner row, retained by this lease.
	uint32_t cell = UINT32_MAX, native_surface = 0, flattened_surface = 0;
	ExternalOutputConfig::Profile profile = ExternalOutputConfig::INSTANCE_192;
	RSE::PrimitiveType primitive = RSE::PRIMITIVE_MAX;
	uint32_t pass_mask = 0;
	uint64_t surface_format = 0;
	AABB surface_aabb;
	Vector4 surface_uv_scale;
	ExternalMaterialPassClassification classification;
	uint64_t material_generation = 0;
	int32_t priority = 0;

public:
	ExternalSurfaceSelection() = default;
	ExternalSurfaceSelection(const ExternalSurfaceSelection &) = delete;
	ExternalSurfaceSelection &operator=(const ExternalSurfaceSelection &) = delete;
	ExternalSurfaceSelection(ExternalSurfaceSelection &&) noexcept;
	ExternalSurfaceSelection &operator=(ExternalSurfaceSelection &&) noexcept;
	~ExternalSurfaceSelection(); // Live custody requires explicit return.
	bool is_empty() const { return owner == nullptr; }
	uint64_t get_generation() const { return generation; }
	uint32_t get_native_surface() const { return native_surface; }
	uint32_t get_flattened_surface() const { return flattened_surface; }
	ExternalOutputConfig::Profile get_profile() const { return profile; }
	RSE::PrimitiveType get_primitive() const { return primitive; }
	uint32_t get_pass_mask() const { return pass_mask; }
	uint64_t get_surface_format() const { return surface_format; }
	const AABB &get_surface_aabb() const { return surface_aabb; }
	const Vector4 &get_surface_uv_scale() const { return surface_uv_scale; }
	const ExternalMaterialPassClassification &get_classification() const { return classification; }
	uint64_t get_material_generation() const { return material_generation; }
	int32_t get_priority() const { return priority; }
};

// Strong pre-list borrow of the immutable packet selected by the actual
// native camera. The same lease supplies upload and every subsequent pass;
// a later resolve may not select a newer packet.
class ExternalPacketViewLease {
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
#endif
	friend class ParticlesStorage;
	friend class ExternalOutputOwner;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0;
	uint32_t slot = UINT32_MAX, view_cell = UINT32_MAX;
	ExternalOutputConfig::Profile profile = ExternalOutputConfig::INSTANCE_192;
	ExternalOutputConfig::Space space = ExternalOutputConfig::WORLD;
	Transform3D current_host, previous_host;
	bool retained_host_snapshot = false;

public:
	ExternalPacketViewLease() = default;
	ExternalPacketViewLease(const ExternalPacketViewLease &) = delete;
	ExternalPacketViewLease &operator=(const ExternalPacketViewLease &) = delete;
	ExternalPacketViewLease(ExternalPacketViewLease &&) noexcept;
	ExternalPacketViewLease &operator=(ExternalPacketViewLease &&) noexcept;
	~ExternalPacketViewLease(); // Active custody requires explicit return.
	bool is_empty() const { return owner == nullptr; }
	ExternalOutputConfig::Profile get_profile() const { return profile; }
	ExternalOutputConfig::Space get_space() const { return space; }
	bool has_retained_host_snapshot() const { return retained_host_snapshot; }
	const Transform3D &get_current_host() const { return current_host; }
	const Transform3D &get_previous_host() const { return previous_host; }
	// Future metadata reference only. Actual native uses remain in owner cells;
	// this cannot perform inline native free or assert last-use completion.
	Error release_checked();
};

// Stable row in the native owner's already admitted finite preparation table.
// This is not a source grant: the caller has validated its original selection,
// preparation opportunity and metadata/backing reservations before native work.
class ExternalPipelinePreparation {
#ifdef TESTS_ENABLED
	friend class ExternalLinkedOwnerTest;
#endif
	friend class ParticlesStorage;
	friend class RendererSceneRenderImplementation::SceneShaderForwardClustered;
	friend class RendererSceneRenderImplementation::SceneShaderForwardMobile;
	friend class ExternalOutputOwner;
	friend class RendererSceneRenderImplementation::RenderForwardClustered;
	friend class RendererSceneRenderImplementation::RenderForwardMobile;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0, generation = 0;
	uint32_t max_resources = 0;
	bool source_sealed = true; // Only the owner opens an admitted pre-seal preparation row.
	ExternalPacketPipelineRequest selection;
	// Exact generation source selected when this original slot row was bound.
	// The generation outlives all its slots; this pointer creates no new grant.
	const MeshStorage::NativeSurfaceHold *source_geometry = nullptr;
	RID source_material;
	uint64_t source_material_generation = 0;
	MeshStorage::NativeSurfaceHold geometry[2];
	uint32_t selected_geometry = UINT32_MAX;
	MaterialStorage::NativeMaterialHold material;
	ExternalMaterialPassClassification classification;
	bool ubershader = false;
	RenderingDevice::PolygonCullMode cull_mode = RenderingDevice::POLYGON_CULL_DISABLED;
	Error last_error = ERR_UNCONFIGURED;
	bool native_cache_queried = false;
	// Selected RID copy only. The original SurfaceBinding owns the actual
	// pipeline root and group, including failed material-hold prefixes.
	RID created_pipeline;
	GDExtensionProductionPhysicalContextRefV1 pipeline_context{};
	GDExtensionDeviceExit::ProductionOutputPin *pipeline_pin = nullptr;
	uint64_t pipeline_owned_group = 0;
	RID *pipeline_original_root = nullptr;
	Error _pipeline_preflight();
	RID _create_pipeline(RID p_shader, RenderingDevice::FramebufferFormatID p_framebuffer_format, RenderingDevice::VertexFormatID p_vertex_format, RenderingDevice::RenderPrimitive p_render_primitive, const RenderingDevice::PipelineRasterizationState &p_rasterization_state, const RenderingDevice::PipelineMultisampleState &p_multisample_state, const RenderingDevice::PipelineDepthStencilState &p_depth_stencil_state, const RenderingDevice::PipelineColorBlendState &p_blend_state, BitField<RenderingDevice::PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<RenderingDevice::PipelineSpecializationConstant> &p_specialization_constants);
	ExternalPipelinePreparation() = default;

public:
	ExternalPipelinePreparation(const ExternalPipelinePreparation &) = delete;
	ExternalPipelinePreparation &operator=(const ExternalPipelinePreparation &) = delete;
	~ExternalPipelinePreparation(); // Original owner service must detach the row first.
};

// Short borrow of one actual sealed packet's preaccepted native read cell.
// Its owner retains all source/physical uses past this borrow through actual
// list closure and last use, including failed descriptor/draw prefixes.
class ExternalPacketBinding {
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
#endif
	friend class ParticlesStorage;
	friend class ExternalOutputOwner;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0;
	uint32_t slot = UINT32_MAX, read_cell = UINT32_MAX;
	ExternalPacketPipelineRequest selection;
	RID pipeline, vertex_array, index_array, transforms_uniform_set;
	MaterialStorage::NativeMaterialSnapshot material;
	// Resolved from the sealed/consumed source tuple, never live instance state.
	Transform3D current_host, previous_host;
	ExternalOutputConfig::Space space = ExternalOutputConfig::WORLD;
	bool retained_host_snapshot = false;
	RenderingDevice::VertexFormatID vertex_format = 0;
	RID commands;
	uint64_t command_bytes = 0, command_offset = 0;
	bool ubershader = false;
	RenderingDevice::PolygonCullMode cull_mode = RenderingDevice::POLYGON_CULL_DISABLED;

public:
	ExternalPacketBinding() = default;
	ExternalPacketBinding(const ExternalPacketBinding &) = delete;
	ExternalPacketBinding &operator=(const ExternalPacketBinding &) = delete;
	ExternalPacketBinding(ExternalPacketBinding &&) noexcept;
	ExternalPacketBinding &operator=(ExternalPacketBinding &&) noexcept;
	~ExternalPacketBinding(); // Never invent native last-use return.
	bool is_empty() const { return owner == nullptr; }
	RID get_pipeline() const { return pipeline; }
	RID get_vertex_array() const { return vertex_array; }
	RID get_index_array() const { return index_array; }
	RID get_transforms_uniform_set() const { return transforms_uniform_set; }
	const MaterialStorage::NativeMaterialSnapshot &get_material() const { return material; }
	const ExternalPacketPipelineRequest &get_pipeline_selection() const { return selection; }
	bool uses_ubershader() const { return ubershader; }
	bool has_retained_host_snapshot() const { return retained_host_snapshot; }
	const Transform3D &get_current_host() const { return current_host; }
	const Transform3D &get_previous_host() const { return previous_host; }
	ExternalOutputConfig::Space get_space() const { return space; }
	RenderingDevice::PolygonCullMode get_cull_mode() const { return cull_mode; }
	// Both consume this short borrow only. Recorded prefix obligations stay in
	// its original finite read cell; neither method certifies physical return.
	Error record_checked(RenderingDevice::DrawListID p_list);
	Error cancel_before_draw();
};
} // namespace RendererRD
