/**************************************************************************/
/*  particles_external_guard.h                                            */
/**************************************************************************/

#pragma once

#include "servers/rendering/renderer_rd/shaders/particles_external_guard.glsl.gen.h"

#include <cstddef>

namespace RendererRD {

// Instance192 component preparation. The existing K/E09 owner must reserve
// budget and retain/associate the original native uses before binding/recording.
// Neither a prepared pipeline nor a LayoutPlan is a production capability.
class ParticlesExternalGuard {
public:
	enum Mode {
		INIT,
		VALIDATE,
		FINALIZE,
		INIT_EMPTY,
		FINALIZE_EMPTY,
		MODE_COUNT,
	};
	enum Binding {
		CURRENT_RECORDS,
		PREVIOUS_RECORDS,
		OUTPUT_META,
		SOURCE_COUNT,
		GUARD_SCRATCH,
		GUARD_STATUS,
		PROTECTED_COMMANDS,
		GEOMETRY_PROOF,
		SOURCE_COUNT_VALIDITY,
		SOURCE_ATTRIBUTES_VALIDITY,
		SOURCE_DENSE_IDS_VALIDITY,
		SOURCE_INSTANCE_GATE,
		BINDING_COUNT,
	};
	struct PushConstant {
		uint32_t capacity = 0;
		uint32_t sequence_low = 0;
		uint32_t sequence_high = 0;
		uint32_t surface_count = 0;
		uint32_t record_base = 0;
		uint32_t record_count = 0;
		uint32_t reserved0 = 0;
		uint32_t reserved1 = 0;
	};
	struct GeometryProofHeader {
		float current_decl_min[4] = {};
		float current_decl_max[4] = {};
		float previous_decl_min[4] = {};
		float previous_decl_max[4] = {};
		float cull_min[4] = {};
		float cull_max[4] = {};
		float current_host_inverse_rows[3][4] = {};
		float previous_host_inverse_rows[3][4] = {};
		uint32_t source_accessor[4] = {}; // count word, reserved zero, reserved zero, space
		uint32_t source_gate_words[4] = {}; // CountValidity, AttributesValidity, DenseIdsValidity, InstanceGate; fault==0
		uint32_t options[4] = {}; // history permitted, reserved zero
	};
	struct SurfaceProof {
		float current_mesh_min[4] = {};
		float current_mesh_max[4] = {};
		float current_displacement_min[4] = {};
		float current_displacement_max[4] = {};
		float previous_mesh_min[4] = {};
		float previous_mesh_max[4] = {};
		float previous_displacement_min[4] = {};
		float previous_displacement_max[4] = {};
		uint32_t command[4] = {}; // indexed (0/1), native element count, reserved
	};
	struct BufferBinding {
		RID rid;
		uint64_t byte_length = 0;
	};
	struct Bindings {
		BufferBinding buffers[BINDING_COUNT];
	};
	struct LayoutPlan {
		Mode initialize_mode = INIT;
		Mode finalize_mode = FINALIZE;
		PushConstant initialize;
		Vector<PushConstant> validation_chunks;
		Vector<uint32_t> validation_groups_x;
		PushConstant finalize;
		Vector<RD::Uniform> uniforms;
	};

private:
	ParticlesExternalGuardShaderRD *shader = nullptr;
	RID shader_version;
	RID pipelines[MODE_COUNT];
	bool initialized = false;
	bool ready = false;
	Error _require_main_render_thread() const;
	Error _clear_pipeline_prefix();

public:
	ParticlesExternalGuard() = default;
	ParticlesExternalGuard(const ParticlesExternalGuard &) = delete;
	ParticlesExternalGuard &operator=(const ParticlesExternalGuard &) = delete;
	~ParticlesExternalGuard();

	// Creates all five actual shaders/pipelines or leaves no ready bundle.
	// No buffer/uniform-set allocation, dispatch or checked-record claim occurs.
	Error prepare_pipelines();
	bool is_prepared() const { return ready; }
	RID get_pipeline(Mode p_mode) const;
	RID get_shader(Mode p_mode);
	// Call on main RT only, once the original owner has retired all native uses.
	Error release_pipelines();

	// Pure layout/extent preflight. Exact bytes and CPU proof must come from the
	// retained original K/E09 owner; this cannot verify their source authority or
	// inspect private RD allocation extents. uniform_set_create must run later
	// under that owner's recorded-use obligation (it can append synchronization).
	// Request base/count must cover the entire capacity; chunks cover it once.
	static Error plan_layout(const PushConstant &p_request, const Bindings &p_bindings, const GeometryProofHeader &p_proof, uint32_t p_max_groups_x, LayoutPlan &r_plan);
};

static_assert(ParticlesExternalGuard::SOURCE_COUNT == 3);
static_assert(ParticlesExternalGuard::GUARD_SCRATCH == 4 && ParticlesExternalGuard::GEOMETRY_PROOF == 7);
static_assert(ParticlesExternalGuard::SOURCE_COUNT_VALIDITY == 8 && ParticlesExternalGuard::SOURCE_INSTANCE_GATE == 11);
static_assert(ParticlesExternalGuard::BINDING_COUNT == 12);
static_assert(sizeof(ParticlesExternalGuard::PushConstant) == 32);
static_assert(sizeof(ParticlesExternalGuard::GeometryProofHeader) == 240);
static_assert(offsetof(ParticlesExternalGuard::GeometryProofHeader, source_accessor) == 192);
static_assert(offsetof(ParticlesExternalGuard::GeometryProofHeader, source_gate_words) == 208);
static_assert(offsetof(ParticlesExternalGuard::GeometryProofHeader, options) == 224);
static_assert(sizeof(ParticlesExternalGuard::SurfaceProof) == 144);
static_assert(offsetof(ParticlesExternalGuard::SurfaceProof, command) == 128);

} // namespace RendererRD
