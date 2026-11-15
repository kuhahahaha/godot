/**************************************************************************/
/*  particles_ribbon_rd.h                                                 */
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

#include "core/error/error_list.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"

#include <cstdint>
#include <type_traits>

class RenderingDevice;

namespace RendererRD {

struct RibbonSegmentEndpointKey32 {
	uint64_t emitter_id = 0;
	uint64_t trail_id = 0;
	uint64_t head_particle_id = 0;
	uint64_t tail_particle_id = 0;

	bool operator==(const RibbonSegmentEndpointKey32 &p_other) const {
		return emitter_id == p_other.emitter_id &&
				trail_id == p_other.trail_id &&
				head_particle_id == p_other.head_particle_id &&
				tail_particle_id == p_other.tail_particle_id;
	}
	bool operator!=(const RibbonSegmentEndpointKey32 &p_other) const {
		return !(*this == p_other);
	}
};

struct Float144Vertex {
	float position[3] = {};
	uint32_t pad0 = 0;
	float normal[3] = {};
	uint32_t pad1 = 0;
	float tangent[4] = {};
	float color[4] = {};
	float uv0_uv1[4] = {};
	float custom0[4] = {};
	float aux_params[4] = {};
	uint32_t endpoint_key_words[8] = {};

	void set_endpoint_key(const RibbonSegmentEndpointKey32 &p_key);
	RibbonSegmentEndpointKey32 get_endpoint_key() const;
};

static_assert(std::is_standard_layout_v<RibbonSegmentEndpointKey32> && sizeof(RibbonSegmentEndpointKey32) == 32);
static_assert(std::is_standard_layout_v<Float144Vertex> && sizeof(Float144Vertex) == 144);

class ParticlesRibbonRD {
public:
	static constexpr uint32_t MAX_SEGMENTS = (1u << 18u);
	static constexpr uint32_t VERTICES_PER_SEGMENT = 4u;
	static constexpr uint32_t INDICES_PER_SEGMENT = 6u;
	static constexpr uint32_t VERTEX_STRIDE = 144u;

	enum Role : uint32_t {
		CURRENT_VERTICES = 0,
		ALIGNED_PREVIOUS_VERTICES = 1,
		VERTEX_HISTORY_VALIDITY = 2,
		INDEX_STAGING = 3,
		OUTPUT_META = 4,
		GUARD_SCRATCH = 5,
		GUARD_STATUS = 6,
		PROTECTED_COMMANDS = 7,
		GEOMETRY_PROOF = 8,
		RESOURCE_COUNT = 9,
		WRITABLE_ROLE_SLOTS = 5,
	};

	struct Indexed144Layout {
		uint32_t bytes[RESOURCE_COUNT] = {};
		uint64_t total_bytes = 0;
		uint32_t resource_count = 0;
		uint32_t writable_count = 0;
		uint32_t vertex_capacity = 0;
		uint32_t index_capacity = 0;
		bool motion_required = false;
	};

	struct AllocatedBuffers {
		RID rids[RESOURCE_COUNT];
		uint32_t bytes[RESOURCE_COUNT] = {};
		uint32_t live_count = 0;
	};

	static constexpr uint32_t segment_count_for_chain_particles(uint32_t p_particle_count) {
		return p_particle_count >= 2u ? (p_particle_count - 1u) : 0u;
	}

	static Error plan_indexed144(
			uint32_t p_max_segments,
			bool p_motion_required,
			uint32_t p_surface_count,
			Indexed144Layout &r_layout);

	static Error allocate_buffers_rd(
			RenderingDevice &p_rd,
			const Indexed144Layout &p_layout,
			AllocatedBuffers &r_buffers);

	static void free_buffers_rd(
			RenderingDevice &p_rd,
			AllocatedBuffers &r_buffers);

	// Subnormal-free float canonicalizer matching ParticlesExternalGuardIndexed144:
	static float canonicalize_finite_float(float p_value);

	// RIB-04 camera-facing quad tessellation + non-uniform scale pullback normal:
	static void build_ribbon_segment_quad(
			uint32_t p_segment_ordinal,
			const Vector3 &p_head_world,
			const Vector3 &p_tail_world,
			const Vector3 &p_camera_world,
			float p_half_width,
			float p_u_head,
			float p_u_tail,
			const Color &p_color,
			const Basis &p_host_local_to_world,
			const RibbonSegmentEndpointKey32 &p_endpoint_key,
			Float144Vertex r_vertices[4],
			uint32_t r_indices[6]);

	// Safe 0 / 1 / N-particle chain builder (never underflows count - 1):
	static Error build_ribbon_chain_segments(
			uint64_t p_emitter_id,
			uint64_t p_trail_id,
			const LocalVector<Vector3> &p_particle_positions_world,
			const LocalVector<uint64_t> &p_particle_ids,
			const Vector3 &p_camera_world,
			float p_half_width,
			const Color &p_color,
			const Basis &p_host_local_to_world,
			LocalVector<Float144Vertex> &r_vertices,
			LocalVector<uint32_t> &r_indices);

	// A05 RIBBON_EXACT_SUCCESSOR motion-vector history alignment:
	static void align_previous_ribbon_vertices(
			const LocalVector<Float144Vertex> &p_current_vertices,
			const LocalVector<Float144Vertex> &p_previous_frame_vertices,
			LocalVector<Float144Vertex> &r_aligned_previous_vertices,
			LocalVector<uint32_t> &r_vertex_history_validity);
};

} // namespace RendererRD
