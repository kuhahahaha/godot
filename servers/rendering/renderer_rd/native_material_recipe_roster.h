#pragma once

#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

namespace RendererSceneRenderImplementation {
class RenderForwardMobile;
class RenderForwardClustered;
}

namespace RendererRD {
// Original retained requirements. This class has no accepted operation, factory
// sink, append/transfer/receipt bound, permit, or numeric F. A complete resource
// roster remains insufficient to execute a Material qualification action.
class NativeMaterialRecipeRoster {
	friend class RendererSceneRenderImplementation::RenderForwardMobile;
	friend class RendererSceneRenderImplementation::RenderForwardClustered;
#ifdef TESTS_ENABLED
	friend class TestMaterialStorageNativeHoldAccess;
#endif
	using RD = RenderingDevice;
	enum class Backend { MOBILE, CLUSTERED };
	enum class Role { PIPELINE, VERTEX_ARRAY, SCENE, INSTANCE, IMPLEMENTATION, CLUSTER, CURRENT, PREVIOUS, SET, TARGET, FRAMEBUFFER };
	struct FixedData {
		Vector<uint8_t> scene, instance, implementation, cluster, current, previous, push;
	};
	struct Owned {
		Role role = Role::PIPELINE;
		uint32_t symbol = UINT32_MAX;
		Vector<uint8_t> bytes;
		RD::TextureFormat texture;
	};
	struct Binding {
		uint32_t set = 0, binding = 0;
		RD::UniformType type = RD::UNIFORM_TYPE_MAX;
		bool immutable_sampler = false;
		Vector<RD::NativeProspectiveKey> inputs; // Raw ordered descriptor entries, including duplicates.
	};
	MaterialStorage *material_owner = nullptr;
	MeshStorage *mesh_owner = nullptr;
	RD *device = nullptr;
	MaterialStorage::NativeRecipeSource material;
	MeshStorage::NativeSelectionLease mesh;
	MeshStorage::NativeSurfaceHold surface;
	RD::NativeImmutableHold borrowed_hold;
	RD::NativeFramebufferLayout framebuffer;
	RD::FramebufferFormatID framebuffer_format = 0;
	MeshStorage::NativeVertexArrayPlan geometry;
	RD::RenderPrimitive primitive = RD::RENDER_PRIMITIVE_MAX;
	RD::PipelineRasterizationState raster;
	RD::PipelineMultisampleState multisample;
	RD::PipelineDepthStencilState depth_stencil;
	RD::PipelineColorBlendState blend;
	Vector<RD::PipelineSpecializationConstant> specialization;
	Vector<uint8_t> push;

	Vector<RD::NativeProspectiveSymbol> symbols;
	Vector<Owned> owned;
	Vector<RID> borrowed; // Exact raw roots before closure deduplication.
	Vector<Binding> bindings;
	Vector<RD::NativeProspectiveKey> roots, closure;
	uint32_t edge_count = 0;
	bool roster_complete = false;

	Error _begin_material(MaterialStorage &, RID, uint32_t);
	Error _begin(MaterialStorage &, MeshStorage &, RID, RID, uint32_t, RD::FramebufferFormatID, uint32_t);
	Error _material_data(MaterialStorage::MaterialData *&) const;
	Error _join_bindings(Backend, const Vector<RD::Uniform> &, const Vector<RD::Uniform> &, const FixedData &, uint32_t);
	Error _finish(Backend, const MeshStorage::NativeVertexArrayPlan &, const Vector<RD::Uniform> &, const Vector<RD::Uniform> &, const FixedData &, uint32_t);
	Error _add_symbol(const RD::NativeProspectiveSymbol &, Role, const Vector<uint8_t> &, RD::NativeProspectiveKey &, uint32_t);
	Error _validate_borrowed(const RD::ShaderUniform &, const RD::Uniform &) const;
	static Error _input_kind(RD::UniformType, uint32_t, RD::NativeProspectiveInput::Kind &);

public:
	NativeMaterialRecipeRoster() = default;
	NativeMaterialRecipeRoster(const NativeMaterialRecipeRoster &) = delete;
	NativeMaterialRecipeRoster &operator=(const NativeMaterialRecipeRoster &) = delete;
	~NativeMaterialRecipeRoster() { CRASH_COND(material_owner || mesh_owner || device); }
	bool is_active() const { return device != nullptr; }
	bool has_resource_roster() const { return roster_complete; } // Observation only.
	uint32_t get_raster_owned_factory_count() const { return roster_complete ? symbols.size() : 0; }
	uint32_t get_raw_borrowed_count() const { return roster_complete ? borrowed.size() : 0; }
	uint32_t get_owned_dependency_edge_count() const { return roster_complete ? edge_count : 0; }
	// The actual payer/caller and operation/worker plan do not exist here.
	Error get_operation_bounds() const { return ERR_UNCONFIGURED; }
	Error release(); // Original RT only; failed return prefix remains owned here.
};
} // namespace RendererRD
