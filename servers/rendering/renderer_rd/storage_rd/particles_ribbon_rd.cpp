/**************************************************************************/
/*  particles_ribbon_rd.cpp                                               */
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

#include "particles_ribbon_rd.h"

#include "core/templates/hash_map.h"
#include "core/templates/hashfuncs.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard_indexed144.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/rendering_device.h"

#include <cstring>

namespace RendererRD {

void Float144Vertex::set_endpoint_key(const RibbonSegmentEndpointKey32 &p_key) {
	endpoint_key_words[0] = p_key.start_id_index;
	endpoint_key_words[1] = p_key.start_id_tag;
	endpoint_key_words[2] = p_key.end_id_index;
	endpoint_key_words[3] = p_key.end_id_tag;
	endpoint_key_words[4] = p_key.group_lo;
	endpoint_key_words[5] = p_key.group_hi;
	endpoint_key_words[6] = p_key.subsegment_index;
	endpoint_key_words[7] = p_key.valid_mask;
}

RibbonSegmentEndpointKey32 Float144Vertex::get_endpoint_key() const {
	RibbonSegmentEndpointKey32 out;
	out.start_id_index = endpoint_key_words[0];
	out.start_id_tag = endpoint_key_words[1];
	out.end_id_index = endpoint_key_words[2];
	out.end_id_tag = endpoint_key_words[3];
	out.group_lo = endpoint_key_words[4];
	out.group_hi = endpoint_key_words[5];
	out.subsegment_index = endpoint_key_words[6];
	out.valid_mask = endpoint_key_words[7];
	return out;
}

Error ParticlesRibbonRD::plan_indexed144(
		uint32_t p_max_segments,
		bool p_motion_required,
		uint32_t p_surface_count,
		Indexed144Layout &r_layout) {
	if (p_max_segments > MAX_SEGMENTS || p_surface_count == 0 || p_surface_count > ExternalOutputConfig::MAX_FLATTENED_SURFACES) {
		return ERR_INVALID_PARAMETER;
	}

	Indexed144Layout layout;
	layout.motion_required = p_motion_required;
	layout.vertex_capacity = p_max_segments * VERTICES_PER_SEGMENT;
	layout.index_capacity = p_max_segments * INDICES_PER_SEGMENT;

	const uint64_t counts[RESOURCE_COUNT] = {
		layout.vertex_capacity, // CURRENT_VERTICES
		p_motion_required ? layout.vertex_capacity : 0u, // ALIGNED_PREVIOUS_VERTICES
		p_motion_required ? layout.vertex_capacity : 0u, // VERTEX_HISTORY_VALIDITY
		layout.index_capacity, // INDEX_STAGING
		8u, // OUTPUT_META (32B)
		16u, // GUARD_SCRATCH (64B)
		8u, // GUARD_STATUS (32B)
		p_surface_count, // PROTECTED_COMMANDS (20B * surfaces)
		sizeof(ParticlesExternalGuardIndexed144::GeometryProofHeader) +
				uint64_t(p_surface_count) * sizeof(ParticlesExternalGuardIndexed144::RibbonSurfaceProof), // GEOMETRY_PROOF
	};
	const uint64_t strides[RESOURCE_COUNT] = {
		VERTEX_STRIDE,
		VERTEX_STRIDE,
		4u,
		4u,
		4u,
		4u,
		4u,
		20u,
		1u,
	};

	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		const Error size_err = ExternalOutputConfig::checked_buffer_bytes(counts[i], strides[i], UINT32_MAX, layout.bytes[i]);
		if (size_err != OK) {
			return size_err;
		}
		layout.total_bytes += layout.bytes[i];
		if (layout.bytes[i] != 0) {
			++layout.resource_count;
			if (i < WRITABLE_ROLE_SLOTS) {
				++layout.writable_count;
			}
		}
	}

	r_layout = layout;
	return OK;
}

Error ParticlesRibbonRD::allocate_buffers_rd(
		RenderingDevice &p_rd,
		const Indexed144Layout &p_layout,
		AllocatedBuffers &r_buffers) {
	r_buffers = AllocatedBuffers();
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		r_buffers.bytes[i] = p_layout.bytes[i];
		if (p_layout.bytes[i] == 0) {
			continue;
		}
		const BitField<RenderingDevice::StorageBufferUsage> usage = (i == PROTECTED_COMMANDS)
				? BitField<RenderingDevice::StorageBufferUsage>(RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT)
				: BitField<RenderingDevice::StorageBufferUsage>();
		r_buffers.rids[i] = p_rd.storage_buffer_create(p_layout.bytes[i], Vector<uint8_t>(), usage);
		if (r_buffers.rids[i].is_null()) {
			free_buffers_rd(p_rd, r_buffers);
			return ERR_CANT_CREATE;
		}
		++r_buffers.live_count;
	}
	return OK;
}

void ParticlesRibbonRD::free_buffers_rd(
		RenderingDevice &p_rd,
		AllocatedBuffers &r_buffers) {
	for (uint32_t i = RESOURCE_COUNT; i > 0; --i) {
		RID &rid = r_buffers.rids[i - 1];
		if (rid.is_valid()) {
			p_rd.free_rid(rid);
			rid = RID();
			if (r_buffers.live_count > 0) {
				--r_buffers.live_count;
			}
		}
		r_buffers.bytes[i - 1] = 0;
	}
}

float ParticlesRibbonRD::canonicalize_finite_float(float p_value) {
	uint32_t bits = 0;
	memcpy(&bits, &p_value, sizeof(uint32_t));
	const uint32_t mag = bits & 0x7fffffffu;
	if (mag >= 0x7f800000u || mag < 0x00800000u) {
		uint32_t zero_bits = 0u;
		float zero_f = 0.0f;
		memcpy(&zero_f, &zero_bits, sizeof(float));
		return zero_f;
	}
	return p_value;
}

void ParticlesRibbonRD::build_ribbon_segment_quad(
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
		uint32_t r_indices[6]) {
	if (!Math::is_finite(p_half_width) || !p_head_world.is_finite() || !p_tail_world.is_finite() || !p_camera_world.is_finite()) {
		memset(r_vertices, 0, sizeof(Float144Vertex) * 4u);
		memset(r_indices, 0, sizeof(uint32_t) * 6u);
		return;
	}

	const Vector3 head_world(
			canonicalize_finite_float(p_head_world.x),
			canonicalize_finite_float(p_head_world.y),
			canonicalize_finite_float(p_head_world.z));
	const Vector3 tail_world(
			canonicalize_finite_float(p_tail_world.x),
			canonicalize_finite_float(p_tail_world.y),
			canonicalize_finite_float(p_tail_world.z));
	const Vector3 camera_world(
			canonicalize_finite_float(p_camera_world.x),
			canonicalize_finite_float(p_camera_world.y),
			canonicalize_finite_float(p_camera_world.z));

	Vector3 seg_delta = tail_world - head_world;
	Vector3 tangent;
	const float seg_len_sq = seg_delta.length_squared();
	if (Math::is_finite(seg_len_sq) && seg_len_sq > 1e-12f) {
		tangent = seg_delta.normalized();
	} else {
		tangent = Vector3(0.0f, 0.0f, 1.0f);
	}

	const Vector3 midpoint = (head_world + tail_world) * 0.5f;
	Vector3 view_dir = camera_world - midpoint;
	const float view_len_sq = view_dir.length_squared();
	if (Math::is_finite(view_len_sq) && view_len_sq > 1e-12f) {
		view_dir.normalize();
	} else {
		view_dir = Vector3(0.0f, 1.0f, 0.0f);
	}

	Vector3 side = tangent.cross(view_dir);
	const float side_len_sq = side.length_squared();
	if (Math::is_finite(side_len_sq) && side_len_sq > 1e-10f) {
		side.normalize();
	} else {
		// Deterministic perpendicular fallback when camera ray is collinear with tangent:
		const Vector3 ref_axis = (Math::abs(tangent.y) < 0.9f) ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
		side = tangent.cross(ref_axis).normalized();
	}

	Vector3 world_normal = side.cross(tangent).normalized();
	if (world_normal.dot(view_dir) < 0.0f) {
		world_normal = -world_normal;
	}

	// RIB-04: Pull back world normal via transpose(A) so downstream renderer's
	// inverse-transpose(A) restores the exact camera-facing unit normal even under
	// non-uniform host scale!
	bool basis_finite = true;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			basis_finite = basis_finite && Math::is_finite(p_host_local_to_world.rows[r][c]);
		}
	}
	Vector3 pullback_normal = basis_finite ? p_host_local_to_world.transposed().xform(world_normal) : world_normal;
	const float pullback_len_sq = pullback_normal.length_squared();
	if (Math::is_finite(pullback_len_sq) && pullback_len_sq > 1e-12f) {
		pullback_normal.normalize();
	} else {
		pullback_normal = world_normal;
	}

	const float w = (p_half_width <= 0.0f) ? 0.0f : canonicalize_finite_float(p_half_width);
	const Vector3 positions[4] = {
		head_world - side * w,
		head_world + side * w,
		tail_world - side * w,
		tail_world + side * w,
	};
	const float u_head = canonicalize_finite_float(p_u_head);
	const float u_tail = canonicalize_finite_float(p_u_tail);
	const float u_coords[4] = { u_head, u_head, u_tail, u_tail };
	const float v_coords[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

	for (uint32_t k = 0; k < 4u; ++k) {
		Float144Vertex vtx{};
		vtx.position[0] = canonicalize_finite_float(positions[k].x);
		vtx.position[1] = canonicalize_finite_float(positions[k].y);
		vtx.position[2] = canonicalize_finite_float(positions[k].z);
		vtx.pad0 = 0u;
		vtx.normal[0] = canonicalize_finite_float(pullback_normal.x);
		vtx.normal[1] = canonicalize_finite_float(pullback_normal.y);
		vtx.normal[2] = canonicalize_finite_float(pullback_normal.z);
		vtx.pad1 = 0u;
		vtx.tangent[0] = canonicalize_finite_float(tangent.x);
		vtx.tangent[1] = canonicalize_finite_float(tangent.y);
		vtx.tangent[2] = canonicalize_finite_float(tangent.z);
		vtx.tangent[3] = 1.0f;
		vtx.color[0] = canonicalize_finite_float(p_color.r);
		vtx.color[1] = canonicalize_finite_float(p_color.g);
		vtx.color[2] = canonicalize_finite_float(p_color.b);
		vtx.color[3] = canonicalize_finite_float(p_color.a);
		vtx.uv0_uv1[0] = u_coords[k];
		vtx.uv0_uv1[1] = v_coords[k];
		vtx.uv0_uv1[2] = u_coords[k];
		vtx.uv0_uv1[3] = v_coords[k];
		vtx.set_endpoint_key(p_endpoint_key);
		r_vertices[k] = vtx;
	}

	const uint32_t v_base = p_segment_ordinal * VERTICES_PER_SEGMENT;
	r_indices[0] = v_base + 0u;
	r_indices[1] = v_base + 1u;
	r_indices[2] = v_base + 2u;
	r_indices[3] = v_base + 2u;
	r_indices[4] = v_base + 1u;
	r_indices[5] = v_base + 3u;
}

Error ParticlesRibbonRD::build_ribbon_segment_quad_checked(
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
		uint32_t r_indices[6]) {
	bool basis_finite = true;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			basis_finite = basis_finite && Math::is_finite(p_host_local_to_world.rows[r][c]);
		}
	}
	if (!Math::is_finite(p_half_width) ||
			!p_head_world.is_finite() ||
			!p_tail_world.is_finite() ||
			!p_camera_world.is_finite() ||
			!Math::is_finite(p_u_head) ||
			!Math::is_finite(p_u_tail) ||
			!Math::is_finite(p_color.r) ||
			!Math::is_finite(p_color.g) ||
			!Math::is_finite(p_color.b) ||
			!Math::is_finite(p_color.a) ||
			!basis_finite) {
		memset(r_vertices, 0, sizeof(Float144Vertex) * 4u);
		memset(r_indices, 0, sizeof(uint32_t) * 6u);
		return ERR_INVALID_PARAMETER;
	}
	build_ribbon_segment_quad(
			p_segment_ordinal,
			p_head_world,
			p_tail_world,
			p_camera_world,
			p_half_width,
			p_u_head,
			p_u_tail,
			p_color,
			p_host_local_to_world,
			p_endpoint_key,
			r_vertices,
			r_indices);
	return OK;
}

Error ParticlesRibbonRD::build_ribbon_chain_segments(
		uint64_t p_emitter_id,
		uint64_t p_trail_id,
		const LocalVector<Vector3> &p_particle_positions_world,
		const LocalVector<uint64_t> &p_particle_ids,
		const Vector3 &p_camera_world,
		float p_half_width,
		const Color &p_color,
		const Basis &p_host_local_to_world,
		LocalVector<Float144Vertex> &r_vertices,
		LocalVector<uint32_t> &r_indices) {
	if (p_particle_positions_world.size() != p_particle_ids.size()) {
		return ERR_INVALID_PARAMETER;
	}
	const uint32_t seg_count = segment_count_for_chain_particles(p_particle_positions_world.size());
	if (seg_count > MAX_SEGMENTS) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	r_vertices.resize(seg_count * VERTICES_PER_SEGMENT);
	r_indices.resize(seg_count * INDICES_PER_SEGMENT);
	if (seg_count == 0u) {
		return OK;
	}
	const float inv_segs = 1.0f / float(seg_count);
	for (uint32_t s = 0; s < seg_count; ++s) {
		const RibbonSegmentEndpointKey32 key = RibbonSegmentEndpointKey32::from_legacy_ids(
				p_emitter_id,
				p_trail_id,
				p_particle_ids[s],
				p_particle_ids[s + 1u],
				0u);
		build_ribbon_segment_quad(
				s,
				p_particle_positions_world[s],
				p_particle_positions_world[s + 1u],
				p_camera_world,
				p_half_width,
				float(s) * inv_segs,
				float(s + 1u) * inv_segs,
				p_color,
				p_host_local_to_world,
				key,
				r_vertices.ptr() + s * VERTICES_PER_SEGMENT,
				r_indices.ptr() + s * INDICES_PER_SEGMENT);
	}
	return OK;
}

void ParticlesRibbonRD::align_previous_ribbon_vertices(
		const LocalVector<Float144Vertex> &p_current_vertices,
		const LocalVector<Float144Vertex> &p_previous_frame_vertices,
		LocalVector<Float144Vertex> &r_aligned_previous_vertices,
		LocalVector<uint32_t> &r_vertex_history_validity) {
	const uint32_t cur_v_count = p_current_vertices.size();
	r_aligned_previous_vertices.resize(cur_v_count);
	r_vertex_history_validity.resize(cur_v_count);

	const uint32_t cur_seg_count = cur_v_count / VERTICES_PER_SEGMENT;
	const uint32_t prev_seg_count = p_previous_frame_vertices.size() / VERTICES_PER_SEGMENT;

	for (uint32_t i = 0; i < cur_v_count; ++i) {
		r_aligned_previous_vertices[i] = p_current_vertices[i];
		r_vertex_history_validity[i] = 0u;
	}

	if (cur_seg_count == 0 || prev_seg_count == 0) {
		return;
	}

	struct EndpointKeyHasher {
		static _FORCE_INLINE_ uint32_t hash(const RibbonSegmentEndpointKey32 &p_key) {
			uint32_t h = hash_murmur3_one_32(p_key.start_id_index);
			h = hash_murmur3_one_32(p_key.start_id_tag, h);
			h = hash_murmur3_one_32(p_key.end_id_index, h);
			h = hash_murmur3_one_32(p_key.end_id_tag, h);
			h = hash_murmur3_one_32(p_key.group_lo, h);
			h = hash_murmur3_one_32(p_key.group_hi, h);
			h = hash_murmur3_one_32(p_key.subsegment_index, h);
			h = hash_murmur3_one_32(p_key.valid_mask, h);
			return hash_fmix32(h);
		}
	};

	thread_local HashMap<RibbonSegmentEndpointKey32, uint32_t, EndpointKeyHasher> prev_seg_by_key;
	prev_seg_by_key.clear();
	prev_seg_by_key.reserve(prev_seg_count);
	for (uint32_t pseg = 0; pseg < prev_seg_count; ++pseg) {
		const RibbonSegmentEndpointKey32 prev_key = p_previous_frame_vertices[pseg * VERTICES_PER_SEGMENT].get_endpoint_key();
		if (prev_key.valid_mask == 0u) {
			continue;
		}
		if (!prev_seg_by_key.has(prev_key)) {
			prev_seg_by_key.insert(prev_key, pseg);
		}
	}

	for (uint32_t seg = 0; seg < cur_seg_count; ++seg) {
		const RibbonSegmentEndpointKey32 cur_key = p_current_vertices[seg * VERTICES_PER_SEGMENT].get_endpoint_key();
		if (cur_key.valid_mask == 0u) {
			continue;
		}
		const uint32_t *matched_prev_seg = prev_seg_by_key.getptr(cur_key);
		if (matched_prev_seg != nullptr) {
			for (uint32_t k = 0; k < VERTICES_PER_SEGMENT; ++k) {
				r_aligned_previous_vertices[seg * VERTICES_PER_SEGMENT + k] =
						p_previous_frame_vertices[(*matched_prev_seg) * VERTICES_PER_SEGMENT + k];
				r_vertex_history_validity[seg * VERTICES_PER_SEGMENT + k] = 1u;
			}
		}
	}
}

} // namespace RendererRD
