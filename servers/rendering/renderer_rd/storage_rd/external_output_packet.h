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
#include "servers/rendering/renderer_rd/storage_rd/external_output_request.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_viewport.h"

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
class ExternalOutputStorage;

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
	bool operator==(const ExternalPacketPipelineRequest &other) const {
		return backend == other.backend && pass == other.pass && framebuffer_format == other.framebuffer_format && subpass == other.subpass && version == other.version &&
				color_flags == other.color_flags && specialization[0] == other.specialization[0] && specialization[1] == other.specialization[1] && specialization[2] == other.specialization[2] &&
				view_count == other.view_count && wireframe == other.wireframe && reverse_cull == other.reverse_cull && double_sided_shadow == other.double_sided_shadow;
	}
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
	friend class ::RenderingDevice;
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

// Immutable projection of the original covering color-close receipt. This is
// native correlation, not A/G's whole consumed tuple or physical last-use return.
// The associated retained view lease supplies arena/slot/source custody; copying
// this snapshot alone cannot grant a view, native read, or extension capability.
class ExternalConsumedColorProofSnapshot {
	friend class ExternalOutputOwner;
	GDExtensionProductionRequestRefV1 request = {};
	uint64_t action_ordinal = UINT64_MAX;
	RID particles;
	uint64_t generation = 0, lease = 0, view_epoch = 0;
	ExternalOutputRequestCommon common;
	GDExtensionOutputTokenV1 token = {}, lease_token = {};
	Ref<RendererViewport::Viewport::CameraCapture> camera_source;
	Transform3D current_host, previous_host;

public:
	GDExtensionProductionRequestRefV1 get_request() const { return request; }
	uint64_t get_action_ordinal() const { return action_ordinal; }
	RID get_particles() const { return particles; }
	uint64_t get_generation() const { return generation; }
	uint64_t get_lease_serial() const { return lease; }
	const GDExtensionOutputTokenV1 &get_lease_token() const { return lease_token; }
	uint64_t get_view_epoch() const { return view_epoch; }
	const ExternalOutputRequestCommon &get_common() const { return common; }
	const GDExtensionOutputTokenV1 &get_token() const { return token; }
	const Ref<RendererViewport::Viewport::CameraCapture> &get_camera_source() const { return camera_source; }
	const Transform3D &get_current_host() const { return current_host; }
	const Transform3D &get_previous_host() const { return previous_host; }
};

// Strong pre-list borrow of the immutable packet selected by the actual
// native camera. The same lease supplies upload and every subsequent pass;
// a later resolve may not select a newer packet.
class ExternalPacketViewLease {
	friend class ::GDExtensionDeviceExit::Registry;
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
#endif
	friend class ParticlesStorage;
	friend class ExternalOutputOwner;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0;
	uint32_t slot = UINT32_MAX, view_cell = UINT32_MAX;
	uint32_t flattened_surface = UINT32_MAX;
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
	// Same retained original source only; this comparison grants no new view.
	bool matches_native_camera_source(const ::RenderDataRD &) const;
	// Future metadata reference only. Actual native uses remain in owner cells;
	// this cannot perform inline native free or assert last-use completion.
	Error release_checked();
	// Render-thread native metadata only. No public ABI or SDK export.
	Error copy_native_color_consumption(ExternalConsumedColorProofSnapshot &) const;
	// Retain this already-held original view into an existing quoted ViewCell.
	// Must precede cancellation; explicit release_checked() returns the pin.
	Error retain_native_color_consumption(ExternalPacketViewLease &) const;
};

// Core-only grouped strong receipt. Only an already-held original view with an
// actual native color receipt can populate it. No constructor accepts a token.
// The original view pin keeps accepted request and slot incarnation alive; the
// CameraCapture Ref and frozen host values remain paired with that same proof.
class ExternalConsumedColorProofLease {
	friend class ::GDExtensionDeviceExit::Registry;
	ExternalPacketViewLease retained;
	ExternalConsumedColorProofSnapshot snapshot;

public:
	ExternalConsumedColorProofLease() = default;
	ExternalConsumedColorProofLease(const ExternalConsumedColorProofLease &) = delete;
	ExternalConsumedColorProofLease &operator=(const ExternalConsumedColorProofLease &) = delete;
	ExternalConsumedColorProofLease(ExternalConsumedColorProofLease &&) noexcept = default;
	ExternalConsumedColorProofLease &operator=(ExternalConsumedColorProofLease &&) noexcept = default;
	bool is_empty() const { return retained.is_empty(); }
	Error retain_from(const ExternalPacketViewLease &p_original) {
		if (!is_empty()) { return ERR_ALREADY_IN_USE; }
		ExternalConsumedColorProofSnapshot proof;
		const Error observed = p_original.copy_native_color_consumption(proof);
		if (observed != OK) { return observed; }
		const Error held = p_original.retain_native_color_consumption(retained);
		if (held != OK) { return held; }
		snapshot = std::move(proof);
		return OK;
	}
	const ExternalConsumedColorProofSnapshot *get_snapshot() const { return is_empty() ? nullptr : &snapshot; }
	// Original owner read boundary for a future POD projection. This rechecks
	// the existing retained pin/incarnation on its native thread, including
	// cleanup after cancellation, without borrowing new work or a new camera.
	Error read_checked(ExternalConsumedColorProofSnapshot &r_proof) const {
		r_proof = {};
		ExternalConsumedColorProofSnapshot current;
		const Error observed = retained.copy_native_color_consumption(current);
		if (observed != OK) { return observed; }
		if (current.get_request().registry_instance != snapshot.get_request().registry_instance ||
				current.get_request().handle_serial != snapshot.get_request().handle_serial ||
				current.get_action_ordinal() != snapshot.get_action_ordinal() ||
				current.get_particles() != snapshot.get_particles() || current.get_generation() != snapshot.get_generation() ||
				current.get_lease_serial() != snapshot.get_lease_serial() || current.get_view_epoch() != snapshot.get_view_epoch() ||
				memcmp(current.get_lease_token().bytes, snapshot.get_lease_token().bytes, sizeof(current.get_lease_token().bytes)) ||
				memcmp(current.get_token().bytes, snapshot.get_token().bytes, sizeof(current.get_token().bytes)) ||
				!current.get_common().matches(snapshot.get_common()) || current.get_camera_source() != snapshot.get_camera_source() ||
				current.get_current_host() != snapshot.get_current_host() || current.get_previous_host() != snapshot.get_previous_host()) {
			return ERR_INVALID_DATA;
		}
		r_proof = snapshot;
		return OK;
	}

	Error release_checked() {
		const Error returned = retained.release_checked();
		if (returned == OK) { snapshot = {}; }
		return returned;
	}
};

// Stable row in the native owner's already admitted finite preparation table.
// This is not a source grant: the caller has validated its original selection,
// preparation opportunity and metadata/backing reservations before native work.
class ExternalPipelinePreparation {
#ifdef TESTS_ENABLED
	friend class ExternalLinkedOwnerTest;
	friend struct ::TestProductionRequest::Access;
#endif
	friend class ParticlesStorage;
	friend class ::GDExtensionDeviceExit::Registry;
	friend class ::RenderingDevice;
	friend class RendererSceneRenderImplementation::SceneShaderForwardClustered;
	friend class RendererSceneRenderImplementation::SceneShaderForwardMobile;
	friend class ExternalOutputOwner;
	friend class RendererSceneRenderImplementation::RenderForwardClustered;
	friend class RendererSceneRenderImplementation::RenderForwardMobile;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0, generation = 0;
	uint32_t owner_slot = UINT32_MAX, owner_surface = UINT32_MAX;
	uint32_t max_resources = 0;
	bool source_sealed = true; // Only the owner opens an admitted pre-seal preparation row.
	ExternalPacketPipelineRequest selection;
	enum class TupleState : uint8_t { UNSELECTED,
		SELECTED,
		READY,
		FAILED };
	TupleState tuple_state = TupleState::UNSELECTED;
	uint32_t tuple_surface = UINT32_MAX, tuple_native_surface = UINT32_MAX;
	RID tuple_mesh;
	Error backend_error = ERR_UNCONFIGURED, finish_error = ERR_UNCONFIGURED;
	bool finish_complete = false, geometry_adopted = false, material_adopted = false, pipeline_sealed = false;
	bool geometry_transferred[2] = {}, material_transferred = false;
	bool geometry_metadata_returned[2] = {}, material_metadata_returned = false;
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
	// Selected RID copy only. Bank reservations own their actual stable root
	// and all three original groups below, including failed native prefixes.
	RID created_pipeline;
	GDExtensionProductionPhysicalContextRefV1 pipeline_context{};
	GDExtensionDeviceExit::ProductionOutputPreparationScope *pipeline_scope = nullptr;
	GDExtensionDeviceExit::ProductionOutputPin *pipeline_pin = nullptr;
	uint64_t pipeline_owned_group = 0;
	RID owned_pipeline_root;
	// The real Instance192 set uses current/previous slot-owned records at
	// set 2 bindings 0/1 and this exact selected material shader layout.
	RID transforms_uniform_set;
	NativeDisposalWitness transforms_uniform_witness;
	uint64_t transforms_owned_group = 0;
	uint64_t geometry_reserved_group = 0, material_reserved_group = 0;
	bool bank_groups_reserved = false;
	RID *pipeline_original_root = nullptr;
	// Scratch observation only. Original owner keeps its retained lease private.
	Error _prospective_execution_geometry_matches(const MeshStorage::NativeVertexArrayPlan &);
	Error _prospective_admission(RID, uint64_t, const MeshStorage::NativeVertexArrayPlan &, RID, RD::FramebufferFormatID, RD::RenderPrimitive, const RD::PipelineRasterizationState &, const RD::PipelineMultisampleState &, const RD::PipelineDepthStencilState &, const RD::PipelineColorBlendState &, uint32_t, const Vector<RD::PipelineSpecializationConstant> &, MaterialStorage::NativeMaterialFuturePipelinePlan &);
	Error _material_future_pipeline_plan(RID p_shader, RID p_uniform_set, MaterialStorage::NativeMaterialFuturePipelinePlan &r_plan);
	Error _pipeline_preflight();
	// Only mandatory prefix capture/adopt/seal may use this exact cleanup proof.
	Error _cleanup_preflight();
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
	friend class ::RenderingDevice;
	ExternalOutputOwner *owner = nullptr;
	uint64_t serial = 0;
	uint32_t slot = UINT32_MAX, read_cell = UINT32_MAX;
	ExternalPacketPipelineRequest selection;
	bool empty_packet = false; // Authenticated capacity-zero color-list participation.
	RID pipeline, vertex_array, index_array, transforms_uniform_set;
	const MaterialStorage::NativeMaterialSnapshot *material = nullptr;
	// Resolved from the sealed/consumed source tuple, never live instance state.
	Transform3D current_host, previous_host;
	Ref<RendererViewport::Viewport::CameraCapture> previous_camera_source;
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
	const MaterialStorage::NativeMaterialSnapshot &get_material() const { return *material; }
	const ExternalPacketPipelineRequest &get_pipeline_selection() const { return selection; }
	bool uses_ubershader() const { return ubershader; }
	bool has_retained_host_snapshot() const { return retained_host_snapshot; }
	bool has_retained_previous_source() const { return previous_camera_source.is_valid(); }
	const Ref<RendererViewport::Viewport::CameraCapture> &get_previous_camera_source() const { return previous_camera_source; }
	// Validated predecessor storage only; this does not select or alias a
	// shader binding. The owner keeps its original pin until this read settles.
	ExternalOutputStorage *get_previous_storage() const;
	const Transform3D &get_current_host() const { return current_host; }
	const Transform3D &get_previous_host() const { return previous_host; }
	ExternalOutputConfig::Space get_space() const { return space; }
	RenderingDevice::PolygonCullMode get_cull_mode() const { return cull_mode; }
	// Uses the original read cell retained at pre-list view selection.
	bool matches_native_camera_source(const ::RenderDataRD &) const;
	// Both consume this short borrow only. Recorded prefix obligations stay in
	// its original finite read cell; neither method certifies physical return.
	Error record_checked(RenderingDevice::DrawListID p_list);
	Error record_checked(RenderingDevice::DrawListID p_list, const void *p_push, uint32_t p_push_bytes);
	Error cancel_before_draw();
};
} // namespace RendererRD
