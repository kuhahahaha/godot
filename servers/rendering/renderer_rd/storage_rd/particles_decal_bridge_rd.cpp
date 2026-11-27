/**************************************************************************/
/*  particles_decal_bridge_rd.cpp                                         */
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

#include "particles_decal_bridge_rd.h"

#include "core/math/math_funcs.h"

#include <algorithm>
#include <cstring>

namespace RendererRD {

namespace {

static const char *kDecalHarvestComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer SlotMeta {
	uint words[];
} slot_meta;

layout(set = 0, binding = 1, std430) readonly buffer SlotRecords {
	uint words[];
} slot_records;

layout(set = 0, binding = 2, std430) buffer CandidateStaging {
	uint words[];
} staging;

layout(push_constant, std430) uniform PushParams {
	uint slot_cap_and_offset; // low 16 = slot_capacity (<= 256), high 16 = emitter_offset (partitioned) or 0 (global)
	uint max_staged_and_ord;  // low 16 = max_staged (<= 8192), high 16 = emitter_slot_idx (< 256)
	uint seq_lo;
	uint seq_hi;
} push_params;

shared uint s_active_count;
shared uint s_base_offset;
shared uint s_slot_valid;
shared uint s_prefix[256];

bool is_finite_u32(uint u) {
	return (u & 0x7F800000u) != 0x7F800000u;
}

uint flush_subnormal_u32(uint u) {
	uint mag = u & 0x7FFFFFFFu;
	return (mag < 0x00800000u) ? 0u : u;
}

float clean_f32(uint u) {
	return uintBitsToFloat(flush_subnormal_u32(u));
}

void main() {
	uint tid = gl_LocalInvocationID.x;
	uint slot_capacity = push_params.slot_cap_and_offset & 0xFFFFu;
	uint emitter_offset = (push_params.slot_cap_and_offset >> 16u) & 0xFFu;
	uint max_staged = push_params.max_staged_and_ord & 0xFFFFu;
	uint emitter_slot_idx = (push_params.max_staged_and_ord >> 16u) & 0xFFu;

	if (tid == 0u) {
		uint active_count = slot_meta.words[0];
		uint res1 = slot_meta.words[1];
		uint res2 = slot_meta.words[2];
		uint fault_bits = slot_meta.words[3];
		uint meta_cap = slot_meta.words[4];
		uint schema_id = slot_meta.words[5];
		uint meta_seq_lo = slot_meta.words[6];
		uint meta_seq_hi = slot_meta.words[7];
		if (res1 == 0u && res2 == 0u && fault_bits == 0u && schema_id == 5u &&
				meta_cap == slot_capacity && active_count <= slot_capacity &&
				meta_seq_lo == push_params.seq_lo && meta_seq_hi == push_params.seq_hi) {
			s_active_count = active_count;
		} else {
			s_active_count = 0u;
		}
	}
	barrier();

	uint w[32];
	bool valid = false;
	if (tid < s_active_count) {
		uint src_base = tid * 32u;
		for (uint i = 0u; i < 32u; ++i) {
			w[i] = slot_records.words[src_base + i];
		}

		bool finite_all = is_finite_u32(w[0]) && is_finite_u32(w[1]) && is_finite_u32(w[2]) &&
				is_finite_u32(w[3]) &&
				is_finite_u32(w[4]) && is_finite_u32(w[5]) && is_finite_u32(w[6]) && is_finite_u32(w[7]) &&
				is_finite_u32(w[8]) && is_finite_u32(w[9]) && is_finite_u32(w[10]) &&
				is_finite_u32(w[11]) &&
				is_finite_u32(w[12]) && is_finite_u32(w[13]) && is_finite_u32(w[14]) && is_finite_u32(w[15]) &&
				is_finite_u32(w[16]) && is_finite_u32(w[17]) && is_finite_u32(w[18]) &&
				is_finite_u32(w[24]) && is_finite_u32(w[25]) && is_finite_u32(w[26]) && is_finite_u32(w[27]) &&
				is_finite_u32(w[28]) && is_finite_u32(w[29]);

		float albedo_mix = clean_f32(w[3]);
		vec4 q = vec4(clean_f32(w[4]), clean_f32(w[5]), clean_f32(w[6]), clean_f32(w[7]));
		vec3 sz = vec3(clean_f32(w[8]), clean_f32(w[9]), clean_f32(w[10]));
		float emission_energy = clean_f32(w[11]);
		vec4 col = vec4(clean_f32(w[12]), clean_f32(w[13]), clean_f32(w[14]), clean_f32(w[15]));
		float upper_fade = clean_f32(w[16]);
		float lower_fade = clean_f32(w[17]);
		float normal_fade = clean_f32(w[18]);
		uint acquire_tag = w[21];
		uint flags = w[22];
		uint importance = w[23];
		vec4 subrect = vec4(clean_f32(w[24]), clean_f32(w[25]), clean_f32(w[26]), clean_f32(w[27]));
		float fade_begin = clean_f32(w[28]);
		float fade_length = clean_f32(w[29]);
		uint res0 = w[31];

		bool subuv_ok = ((flags & 2u) == 0u) || (subrect.z > 0.0 && subrect.w > 0.0);

		if (finite_all &&
				albedo_mix >= 0.0 && albedo_mix <= 1.0 &&
				dot(q, q) > 1e-12 &&
				sz.x > 0.0 && sz.y > 0.0 && sz.z > 0.0 &&
				emission_energy >= 0.0 &&
				col.r >= 0.0 && col.g >= 0.0 && col.b >= 0.0 && col.a >= 0.0 &&
				upper_fade >= 1e-5 && lower_fade >= 1e-5 &&
				normal_fade >= 0.0 && normal_fade <= 1.0 &&
				fade_begin >= 0.0 && fade_length >= 0.0 &&
				subuv_ok &&
				res0 == 0u && (flags & ~3u) == 0u && (flags & 1u) != 0u &&
				acquire_tag != 0u && importance > 0u && importance <= 65535u) {
			for (uint i = 0u; i <= 18u; ++i) {
				w[i] = flush_subnormal_u32(w[i]);
			}
			for (uint i = 24u; i <= 29u; ++i) {
				w[i] = flush_subnormal_u32(w[i]);
			}
			valid = true;
		}
	}

	s_prefix[tid] = valid ? 1u : 0u;
	barrier();

	if (tid == 0u) {
		uint sum = 0u;
		for (uint i = 0u; i < 256u; ++i) {
			uint v = (i < s_active_count) ? s_prefix[i] : 0u;
			s_prefix[i] = sum;
			sum += v;
		}
		s_slot_valid = sum;
		if (sum > 0u) {
			s_base_offset = atomicAdd(staging.words[0], sum);
		} else {
			s_base_offset = 0u;
		}
	}
	barrier();

	if (s_slot_valid == 0u) {
		return;
	}

	if (valid) {
		uint dst = s_base_offset + s_prefix[tid];
		if (dst < max_staged) {
			uint dst_base = 4u + dst * 32u;
			for (uint i = 0u; i < 31u; ++i) {
				staging.words[dst_base + i] = w[i];
			}
			// Deterministic provenance word:
			// bits [23:16] = emitter_offset (partitioned) or 0 (global),
			// bits [15:8]  = emitter_slot_idx (< 256),
			// bits [7:0]   = tid.
			staging.words[dst_base + 31u] = (emitter_offset << 16u) | (emitter_slot_idx << 8u) | tid;
		}
	}
}
)GLSL";

static const char *kDecalTopKEmitComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer CandidateStaging {
	uint words[];
} staging;

layout(set = 0, binding = 1, std430) buffer DecalsOut {
	uint words[];
} decals_out;

layout(set = 0, binding = 2, std430) buffer ClusterElements {
	uint words[];
} cluster_elements;

layout(set = 0, binding = 3, std140) uniform EmitParams {
	vec4 cam_xform_col0;    // xyz = cam_xform.basis col 0
	vec4 cam_xform_col1;    // xyz = cam_xform.basis col 1
	vec4 cam_xform_col2;    // xyz = cam_xform.basis col 2
	vec4 cam_origin;        // xyz = cam_xform.origin
	vec4 cam_inv_row0;      // xyz = cam_inv.basis.rows[0]
	vec4 cam_inv_row1;      // xyz = cam_inv.basis.rows[1]
	vec4 cam_inv_row2;      // xyz = cam_inv.basis.rows[2]
	vec4 cluster_view_row0; // xyz = cluster_view.basis.rows[0], w = cluster_view.origin.x
	vec4 cluster_view_row1; // xyz = cluster_view.basis.rows[1], w = cluster_view.origin.y
	vec4 cluster_view_row2; // xyz = cluster_view.basis.rows[2], w = cluster_view.origin.z
	vec4 misc_floats;       // x = z_near, y = z_far, z = 0, w = 0
	uvec4 counts;           // x = base_decal_index, y = reserved_count, z = max_staged, w = flags (bit0=ortho, bit1=partitioned)
} params;

layout(set = 0, binding = 4, std430) readonly buffer EmitterAtlasRects {
	vec4 rects[]; // 4 vec4s per emitter_slot_idx: [albedo, normal, orm, emission]
} emitter_atlas;

shared uint s_total_candidates;
shared uvec4 s_keys[256];

bool beats(uvec4 b, uvec4 a) {
	if (b.x != a.x) {
		return b.x > a.x;
	}
	if (b.y != a.y) {
		return b.y < a.y;
	}
	if (b.z != a.z) {
		return b.z < a.z;
	}
	return b.w < a.w;
}

vec4 compose_atlas_rect(vec4 base_rect, vec4 subrect, bool has_subuv) {
	if (base_rect.z <= 0.0 || base_rect.w <= 0.0) {
		return vec4(0.0);
	}
	if (!has_subuv) {
		return base_rect;
	}
	return vec4(base_rect.xy + subrect.xy * base_rect.zw, base_rect.zw * subrect.zw);
}

void write_vec4(uint base, vec4 v) {
	decals_out.words[base + 0u] = floatBitsToUint(v.x);
	decals_out.words[base + 1u] = floatBitsToUint(v.y);
	decals_out.words[base + 2u] = floatBitsToUint(v.z);
	decals_out.words[base + 3u] = floatBitsToUint(v.w);
}

void main() {
	uint tid = gl_LocalInvocationID.x;
	uint base_decal = params.counts.x;
	uint reserved = params.counts.y;

	if (tid == 0u) {
		s_total_candidates = min(staging.words[0], params.counts.z);
	}

	if (tid < reserved) {
		uint dbase = (base_decal + tid) * 60u;
		for (uint i = 0u; i < 60u; ++i) {
			decals_out.words[dbase + i] = 0u;
		}
		uint elem_base = tid * 20u;
		for (uint i = 0u; i < 20u; ++i) {
			cluster_elements.words[elem_base + i] = 0u;
		}
		cluster_elements.words[elem_base + 0u] = 3u; // ELEMENT_TYPE_DECAL
		cluster_elements.words[elem_base + 3u] = base_decal + tid;
		cluster_elements.words[elem_base + 4u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 9u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 14u] = floatBitsToUint(1.0);
		cluster_elements.words[elem_base + 15u] = floatBitsToUint(params.misc_floats.x + 16.0);
	}
	memoryBarrierBuffer();
	barrier();

	uint N = s_total_candidates;
	if (N == 0u || reserved == 0u) {
		return;
	}
	bool camera_orthogonal = (params.counts.w & 1u) != 0u;
	bool partitioned_mode = (params.counts.w & 2u) != 0u;

	for (uint outer_base = 0u; outer_base < N; outer_base += 256u) {
		uint my_idx = outer_base + tid;
		uvec4 my_key = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
		if (my_idx < N) {
			uint cbase = 4u + my_idx * 32u;
			my_key = uvec4(staging.words[cbase + 23u], staging.words[cbase + 21u], staging.words[cbase + 20u], staging.words[cbase + 31u]);
		}
		uint my_owner = (my_key.w >> 8u) & 0xFFu;
		uint rank = 0u;
		for (uint tile_base = 0u; tile_base < N; tile_base += 256u) {
			uint load_idx = tile_base + tid;
			if (load_idx < N) {
				uint lbase = 4u + load_idx * 32u;
				s_keys[tid] = uvec4(staging.words[lbase + 23u], staging.words[lbase + 21u], staging.words[lbase + 20u], staging.words[lbase + 31u]);
			} else {
				s_keys[tid] = uvec4(0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
			}
			barrier();

			if (my_idx < N) {
				uint tile_cnt = min(256u, N - tile_base);
				if (partitioned_mode) {
					for (uint j = 0u; j < tile_cnt; ++j) {
						if (((s_keys[j].w >> 8u) & 0xFFu) == my_owner && beats(s_keys[j], my_key)) {
							rank++;
						}
					}
				} else {
					for (uint j = 0u; j < tile_cnt; ++j) {
						if (beats(s_keys[j], my_key)) {
							rank++;
						}
					}
				}
			}
			barrier();
		}

		uint out_slot = partitioned_mode ? (((my_key.w >> 16u) & 0xFFu) + rank) : rank;
		if (my_idx < N && out_slot < reserved) {
			uint cbase = 4u + my_idx * 32u;
			vec3 world_pos = vec3(
					uintBitsToFloat(staging.words[cbase + 0u]),
					uintBitsToFloat(staging.words[cbase + 1u]),
					uintBitsToFloat(staging.words[cbase + 2u]));
			float albedo_mix = uintBitsToFloat(staging.words[cbase + 3u]);
			vec4 q = vec4(
					uintBitsToFloat(staging.words[cbase + 4u]),
					uintBitsToFloat(staging.words[cbase + 5u]),
					uintBitsToFloat(staging.words[cbase + 6u]),
					uintBitsToFloat(staging.words[cbase + 7u]));
			q *= inversesqrt(max(dot(q, q), 1e-12));

			vec3 size_world = max(vec3(1e-4), vec3(
					uintBitsToFloat(staging.words[cbase + 8u]),
					uintBitsToFloat(staging.words[cbase + 9u]),
					uintBitsToFloat(staging.words[cbase + 10u])));
			float emission_energy = uintBitsToFloat(staging.words[cbase + 11u]);
			vec4 color_rgba = vec4(
					uintBitsToFloat(staging.words[cbase + 12u]),
					uintBitsToFloat(staging.words[cbase + 13u]),
					uintBitsToFloat(staging.words[cbase + 14u]),
					uintBitsToFloat(staging.words[cbase + 15u]));
			float upper_fade = uintBitsToFloat(staging.words[cbase + 16u]);
			float lower_fade = uintBitsToFloat(staging.words[cbase + 17u]);
			float normal_fade = uintBitsToFloat(staging.words[cbase + 18u]);
			uint cull_mask = staging.words[cbase + 19u];
			uint flags = staging.words[cbase + 22u];
			vec4 uv_subrect = vec4(
					uintBitsToFloat(staging.words[cbase + 24u]),
					uintBitsToFloat(staging.words[cbase + 25u]),
					uintBitsToFloat(staging.words[cbase + 26u]),
					uintBitsToFloat(staging.words[cbase + 27u]));
			float fade_begin = uintBitsToFloat(staging.words[cbase + 28u]);
			float fade_length = uintBitsToFloat(staging.words[cbase + 29u]);

			float fade = 1.0;
			if (fade_begin > 0.0 && fade_length > 0.0) {
				float dist = length(world_pos - params.cam_origin.xyz);
				if (dist > fade_begin) {
					if (dist >= fade_begin + fade_length) {
						fade = 0.0;
					} else {
						fade = smoothstep(0.0, 1.0, 1.0 - (dist - fade_begin) / fade_length);
					}
				}
			}

			if (fade <= 0.0) {
				continue;
			}

			bool has_subuv = (flags & 2u) != 0u;
			uint atlas_base = my_owner * 4u;
			vec4 albedo_rect = compose_atlas_rect(emitter_atlas.rects[atlas_base + 0u], uv_subrect, has_subuv);
			vec4 normal_rect = compose_atlas_rect(emitter_atlas.rects[atlas_base + 1u], uv_subrect, has_subuv);
			vec4 orm_rect = compose_atlas_rect(emitter_atlas.rects[atlas_base + 2u], uv_subrect, has_subuv);
			vec4 emission_rect = compose_atlas_rect(emitter_atlas.rects[atlas_base + 3u], uv_subrect, has_subuv);

			// Unit quaternion (x, y, z, w) -> orthonormal rotation matrix R columns:
			float xx = q.x * q.x;
			float yy = q.y * q.y;
			float zz = q.z * q.z;
			float xy = q.x * q.y;
			float xz = q.x * q.z;
			float yz = q.y * q.z;
			float wx = q.w * q.x;
			float wy = q.w * q.y;
			float wz = q.w * q.z;

			vec3 r_col0 = vec3(1.0 - 2.0 * (yy + zz), 2.0 * (xy + wz), 2.0 * (xz - wy));
			vec3 r_col1 = vec3(2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz + wx));
			vec3 r_col2 = vec3(2.0 * (xz + wy), 2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy));

			// Exact analytic to_decal_xform = inv(uv_xform) * inv(scale_xform) * inv(xform) * cam_xform:
			// inv(uv_xform) * inv(scale_xform) = [ diag(1/sx, 2/sy, 1/sz) | (0.5, 0.0, 0.5) ]
			vec3 inv_s = vec3(1.0 / size_world.x, 2.0 / size_world.y, 1.0 / size_world.z);
			vec3 c0 = params.cam_xform_col0.xyz;
			vec3 c1 = params.cam_xform_col1.xyz;
			vec3 c2 = params.cam_xform_col2.xyz;
			vec3 delta = params.cam_origin.xyz - world_pos;

			vec4 xform_col0 = vec4(
					inv_s.x * dot(r_col0, c0),
					inv_s.y * dot(r_col1, c0),
					inv_s.z * dot(r_col2, c0),
					0.0);
			vec4 xform_col1 = vec4(
					inv_s.x * dot(r_col0, c1),
					inv_s.y * dot(r_col1, c1),
					inv_s.z * dot(r_col2, c1),
					0.0);
			vec4 xform_col2 = vec4(
					inv_s.x * dot(r_col0, c2),
					inv_s.y * dot(r_col1, c2),
					inv_s.z * dot(r_col2, c2),
					0.0);
			vec4 xform_col3 = vec4(
					inv_s.x * dot(r_col0, delta) + 0.5,
					inv_s.y * dot(r_col1, delta),
					inv_s.z * dot(r_col2, delta) + 0.5,
					1.0);

			// normal_xform = cam_inv_basis * R (3x4 column-major):
			vec3 nx_col0 = vec3(
					dot(params.cam_inv_row0.xyz, r_col0),
					dot(params.cam_inv_row1.xyz, r_col0),
					dot(params.cam_inv_row2.xyz, r_col0));
			vec3 nx_col1 = vec3(
					dot(params.cam_inv_row0.xyz, r_col1),
					dot(params.cam_inv_row1.xyz, r_col1),
					dot(params.cam_inv_row2.xyz, r_col1));
			vec3 nx_col2 = vec3(
					dot(params.cam_inv_row0.xyz, r_col2),
					dot(params.cam_inv_row1.xyz, r_col2),
					dot(params.cam_inv_row2.xyz, r_col2));

			uint decal_idx = base_decal + out_slot;
			uint dbase = decal_idx * 60u;
			write_vec4(dbase + 0u, xform_col0);
			write_vec4(dbase + 4u, xform_col1);
			write_vec4(dbase + 8u, xform_col2);
			write_vec4(dbase + 12u, xform_col3);
			decals_out.words[dbase + 16u] = floatBitsToUint(2.0 / size_world.x);
			decals_out.words[dbase + 17u] = floatBitsToUint(2.0 / size_world.y);
			decals_out.words[dbase + 18u] = floatBitsToUint(2.0 / size_world.z);
			decals_out.words[dbase + 19u] = floatBitsToUint(albedo_mix);
			write_vec4(dbase + 20u, albedo_rect);
			write_vec4(dbase + 24u, normal_rect);
			write_vec4(dbase + 28u, orm_rect);
			write_vec4(dbase + 32u, emission_rect);
			write_vec4(dbase + 36u, vec4(color_rgba.rgb, color_rgba.a * fade));
			decals_out.words[dbase + 40u] = floatBitsToUint(emission_energy * fade);
			decals_out.words[dbase + 41u] = cull_mask;
			decals_out.words[dbase + 42u] = floatBitsToUint(upper_fade);
			decals_out.words[dbase + 43u] = floatBitsToUint(lower_fade);
			write_vec4(dbase + 44u, vec4(nx_col0, 0.0));
			write_vec4(dbase + 48u, vec4(nx_col1, 0.0));
			write_vec4(dbase + 52u, vec4(nx_col2, 0.0));
			decals_out.words[dbase + 56u] = floatBitsToUint(nx_col1.x);
			decals_out.words[dbase + 57u] = floatBitsToUint(nx_col1.y);
			decals_out.words[dbase + 58u] = floatBitsToUint(nx_col1.z);
			decals_out.words[dbase + 59u] = floatBitsToUint(normal_fade);

			// ClusterBuilderRD::RenderElementData for ELEMENT_TYPE_DECAL (3u), matching add_box():
			vec3 bx_origin = vec3(
					dot(params.cluster_view_row0.xyz, world_pos) + params.cluster_view_row0.w,
					dot(params.cluster_view_row1.xyz, world_pos) + params.cluster_view_row1.w,
					dot(params.cluster_view_row2.xyz, world_pos) + params.cluster_view_row2.w);
			vec3 bx_row0 = vec3(
					dot(params.cluster_view_row0.xyz, r_col0),
					dot(params.cluster_view_row0.xyz, r_col1),
					dot(params.cluster_view_row0.xyz, r_col2));
			vec3 bx_row1 = vec3(
					dot(params.cluster_view_row1.xyz, r_col0),
					dot(params.cluster_view_row1.xyz, r_col1),
					dot(params.cluster_view_row1.xyz, r_col2));
			vec3 bx_row2 = vec3(
					dot(params.cluster_view_row2.xyz, r_col0),
					dot(params.cluster_view_row2.xyz, r_col1),
					dot(params.cluster_view_row2.xyz, r_col2));

			vec3 box_scale = size_world * 0.5;
			float s0 = length(bx_row0);
			if (s0 > 1e-6) {
				box_scale.x *= s0;
				bx_row0 /= s0;
			}
			float s1 = length(bx_row1);
			if (s1 > 1e-6) {
				box_scale.y *= s1;
				bx_row1 /= s1;
			}
			float s2 = length(bx_row2);
			if (s2 > 1e-6) {
				box_scale.z *= s2;
				bx_row2 /= s2;
			}

			float box_depth = dot(abs(bx_row2), box_scale);
			float depth = -bx_origin.z;
			bool touches_near = false;
			if (camera_orthogonal) {
				touches_near = (depth - box_depth) < params.misc_floats.x;
			} else {
				vec3 bx_col0 = vec3(bx_row0.x, bx_row1.x, bx_row2.x);
				vec3 bx_col1 = vec3(bx_row0.y, bx_row1.y, bx_row2.y);
				vec3 bx_col2 = vec3(bx_row0.z, bx_row1.z, bx_row2.z);
				vec3 inside = abs(vec3(dot(bx_col0, bx_origin), dot(bx_col1, bx_origin), dot(bx_col2, bx_origin)));
				touches_near = inside.x < box_scale.x && inside.y < box_scale.y && inside.z < box_scale.z;
			}
			bool touches_far = (depth + box_depth) > params.misc_floats.y;

			uint ebase = out_slot * 20u;
			cluster_elements.words[ebase + 0u] = 3u; // ELEMENT_TYPE_DECAL
			cluster_elements.words[ebase + 1u] = touches_near ? 1u : 0u;
			cluster_elements.words[ebase + 2u] = touches_far ? 1u : 0u;
			cluster_elements.words[ebase + 3u] = decal_idx;
			cluster_elements.words[ebase + 4u] = floatBitsToUint(bx_row0.x);
			cluster_elements.words[ebase + 5u] = floatBitsToUint(bx_row0.y);
			cluster_elements.words[ebase + 6u] = floatBitsToUint(bx_row0.z);
			cluster_elements.words[ebase + 7u] = floatBitsToUint(bx_origin.x);
			cluster_elements.words[ebase + 8u] = floatBitsToUint(bx_row1.x);
			cluster_elements.words[ebase + 9u] = floatBitsToUint(bx_row1.y);
			cluster_elements.words[ebase + 10u] = floatBitsToUint(bx_row1.z);
			cluster_elements.words[ebase + 11u] = floatBitsToUint(bx_origin.y);
			cluster_elements.words[ebase + 12u] = floatBitsToUint(bx_row2.x);
			cluster_elements.words[ebase + 13u] = floatBitsToUint(bx_row2.y);
			cluster_elements.words[ebase + 14u] = floatBitsToUint(bx_row2.z);
			cluster_elements.words[ebase + 15u] = floatBitsToUint(bx_origin.z);
			cluster_elements.words[ebase + 16u] = floatBitsToUint(box_scale.x);
			cluster_elements.words[ebase + 17u] = floatBitsToUint(box_scale.y);
			cluster_elements.words[ebase + 18u] = floatBitsToUint(box_scale.z);
			cluster_elements.words[ebase + 19u] = 0u;
		}
	}
}
)GLSL";

static const char *kDecalClusterBlitComputeGLSL = R"GLSL(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer SrcElements {
	uint words[];
} src_elements;

layout(set = 0, binding = 1, std430) buffer DstElements {
	uint words[];
} dst_elements;

layout(push_constant, std430) uniform BlitPush {
	uint dst_element_offset;
	uint element_count;
	uint pad0;
	uint pad1;
} push_params;

void main() {
	uint tid = gl_LocalInvocationID.x;
	if (tid >= push_params.element_count) {
		return;
	}
	uint src_base = tid * 20u;
	uint dst_base = (push_params.dst_element_offset + tid) * 20u;
	for (uint i = 0u; i < 20u; ++i) {
		dst_elements.words[dst_base + i] = src_elements.words[src_base + i];
	}
}
)GLSL";

static Error compile_decal_bridge_compute_pipeline(RenderingDevice &p_rd, const char *p_glsl, const char *p_name, RID &r_shader, RID &r_pipeline) {
	String err;
	RD::ShaderStageSPIRVData stage;
	stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	stage.spirv = p_rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, p_glsl, RD::SHADER_LANGUAGE_GLSL, &err);
	ERR_FAIL_COND_V_MSG(stage.spirv.is_empty(), ERR_COMPILATION_FAILED, String("ParticleDecal128 GPU bridge shader compile failed: ") + err);
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	r_shader = p_rd.shader_create_from_spirv(stages, p_name);
	ERR_FAIL_COND_V(r_shader.is_null(), ERR_CANT_CREATE);
	r_pipeline = p_rd.compute_pipeline_create(r_shader);
	if (r_pipeline.is_null()) {
		p_rd.free_rid(r_shader);
		r_shader = RID();
		return ERR_CANT_CREATE;
	}
	return OK;
}

struct DecalEmitParamsStd140 {
	float cam_xform_col0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float cam_xform_col1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	float cam_xform_col2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float cam_origin[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float cam_inv_row0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float cam_inv_row1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	float cam_inv_row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float cluster_view_row0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float cluster_view_row1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	float cluster_view_row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float misc_floats[4] = { 0.05f, 4000.0f, 0.0f, 0.0f };
	uint32_t counts[4] = { 0u, 0u, 0u, 0u };
};
static_assert(sizeof(DecalEmitParamsStd140) == 192, "DecalEmitParamsStd140 ABI");

} // namespace

ParticlesDecalBridgeRD::~ParticlesDecalBridgeRD() {
	if (decal_gpu_bridge_rd && decal_gpu_bridge_rd == RenderingDevice::get_singleton()) {
		_release_decal_gpu_bridge(decal_gpu_bridge_rd);
	}
}

Error ParticlesDecalBridgeRD::compact_particle_decals_deterministic(
		const LocalVector<ParticleDecalRecord128> &p_candidates,
		uint32_t p_max_decals,
		LocalVector<ParticleDecalRecord128> &r_selected) {
	if (p_max_decals == 0 || p_max_decals > MAX_PARTICLE_DECALS) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<ParticleDecalRecord128> valid;
	valid.reserve(p_candidates.size());
	for (uint32_t i = 0; i < p_candidates.size(); ++i) {
		const ParticleDecalRecord128 &c = p_candidates[i];
		const float q_len2 = c.orientation_quat[0] * c.orientation_quat[0] +
				c.orientation_quat[1] * c.orientation_quat[1] +
				c.orientation_quat[2] * c.orientation_quat[2] +
				c.orientation_quat[3] * c.orientation_quat[3];
		const bool subuv_ok = ((c.flags & ParticleDecalRecord128::FLAG_HAS_SUBUV) == 0u) ||
				(c.uv_subrect[2] > 0.0f && c.uv_subrect[3] > 0.0f);
		if (!Math::is_finite(c.world_position[0]) || !Math::is_finite(c.world_position[1]) || !Math::is_finite(c.world_position[2]) ||
				!Math::is_finite(c.albedo_mix) || c.albedo_mix < 0.0f || c.albedo_mix > 1.0f ||
				!Math::is_finite(c.orientation_quat[0]) || !Math::is_finite(c.orientation_quat[1]) ||
				!Math::is_finite(c.orientation_quat[2]) || !Math::is_finite(c.orientation_quat[3]) ||
				!Math::is_finite(q_len2) || q_len2 <= 1e-12f ||
				!Math::is_finite(c.size_world[0]) || c.size_world[0] <= 0.0f ||
				!Math::is_finite(c.size_world[1]) || c.size_world[1] <= 0.0f ||
				!Math::is_finite(c.size_world[2]) || c.size_world[2] <= 0.0f ||
				!Math::is_finite(c.emission_energy) || c.emission_energy < 0.0f ||
				!Math::is_finite(c.color_rgba[0]) || c.color_rgba[0] < 0.0f ||
				!Math::is_finite(c.color_rgba[1]) || c.color_rgba[1] < 0.0f ||
				!Math::is_finite(c.color_rgba[2]) || c.color_rgba[2] < 0.0f ||
				!Math::is_finite(c.color_rgba[3]) || c.color_rgba[3] < 0.0f ||
				!Math::is_finite(c.upper_fade) || c.upper_fade < 1e-5f ||
				!Math::is_finite(c.lower_fade) || c.lower_fade < 1e-5f ||
				!Math::is_finite(c.normal_fade) || c.normal_fade < 0.0f || c.normal_fade > 1.0f ||
				!Math::is_finite(c.uv_subrect[0]) || !Math::is_finite(c.uv_subrect[1]) ||
				!Math::is_finite(c.uv_subrect[2]) || !Math::is_finite(c.uv_subrect[3]) ||
				!subuv_ok ||
				!Math::is_finite(c.distance_fade_begin) || c.distance_fade_begin < 0.0f ||
				!Math::is_finite(c.distance_fade_length) || c.distance_fade_length < 0.0f ||
				c.reserved0 != 0u || (c.flags & ~ParticleDecalRecord128::FLAG_KNOWN_MASK) != 0u ||
				(c.flags & ParticleDecalRecord128::FLAG_ACTIVE) == 0u ||
				c.id_acquire_tag == 0u || c.screen_importance_q16 == 0u || c.screen_importance_q16 > 65535u) {
			continue;
		}
		valid.push_back(c);
	}

	std::sort(valid.ptr(), valid.ptr() + valid.size(), [](const ParticleDecalRecord128 &a, const ParticleDecalRecord128 &b) {
		if (a.screen_importance_q16 != b.screen_importance_q16) {
			return a.screen_importance_q16 > b.screen_importance_q16;
		}
		if (a.id_acquire_tag != b.id_acquire_tag) {
			return a.id_acquire_tag < b.id_acquire_tag;
		}
		return a.id_index < b.id_index;
	});

	const uint32_t take = MIN(uint32_t(valid.size()), p_max_decals);
	r_selected.resize(take);
	for (uint32_t i = 0; i < take; ++i) {
		r_selected[i] = valid[i];
	}
	return OK;
}

void ParticlesDecalBridgeRD::register_sealed_external_decal_slot(const ExternalDecalSlotRegistration &p_slot) {
	if (p_slot.owner_key == 0 || p_slot.capacity > MAX_PARTICLE_DECALS) {
		return;
	}
	MutexLock lock(active_decals_mutex);
	ExternalOwnerDecalState &st = external_owner_decals[p_slot.owner_key];
	bool existing_live = st.has_pending_rd_slot;
	if (existing_live && st.pending_slot.owner_ptr != nullptr && st.pending_slot.is_slot_live_fn != nullptr) {
		existing_live = st.pending_slot.is_slot_live_fn(
				st.pending_slot.owner_ptr,
				st.pending_slot.generation,
				st.pending_slot.slot_index,
				st.pending_slot.lease_serial);
	}
	if (!existing_live || p_slot.generation != st.pending_slot.generation || p_slot.present_seq >= st.pending_slot.present_seq) {
		st.pending_slot = p_slot;
		st.has_pending_rd_slot = true;
	}
}

void ParticlesDecalBridgeRD::upsert_external_decal_slot(const ExternalDecalSlotRegistration &p_slot) {
	if (p_slot.owner_key == 0 || p_slot.capacity > MAX_PARTICLE_DECALS || p_slot.present_seq == 0) {
		return;
	}
	MutexLock lock(active_decals_mutex);
	ExternalOwnerDecalState &st = external_owner_decals[p_slot.owner_key];
	if (st.has_pending_rd_slot &&
			st.pending_slot.generation == p_slot.generation &&
			st.pending_slot.slot_index == p_slot.slot_index &&
			st.pending_slot.lease_serial == p_slot.lease_serial) {
		st.pending_slot.present_seq = p_slot.present_seq;
		if (p_slot.world_bounds.is_finite() && p_slot.world_bounds.get_end().is_finite() && p_slot.world_bounds.has_volume()) {
			st.pending_slot.world_bounds = p_slot.world_bounds;
		}
		if (p_slot.harvest_uniform_set.is_valid()) {
			st.pending_slot.harvest_uniform_set = p_slot.harvest_uniform_set;
		}
		for (uint32_t ti = 0; ti < TEXTURE_SLOT_COUNT; ++ti) {
			if (p_slot.textures[ti].is_valid()) {
				st.pending_slot.textures[ti] = p_slot.textures[ti];
			}
		}
		return;
	}
	st.pending_slot = p_slot;
	st.has_pending_rd_slot = true;
}

void ParticlesDecalBridgeRD::upsert_external_decal_slots_batch(const ExternalDecalSlotRegistration *p_slots, uint32_t p_count) {
	if (!p_slots || p_count == 0) {
		return;
	}
	MutexLock lock(active_decals_mutex);
	for (uint32_t i = 0; i < p_count; ++i) {
		const ExternalDecalSlotRegistration &slot = p_slots[i];
		if (slot.owner_key == 0 || slot.capacity > MAX_PARTICLE_DECALS || slot.present_seq == 0) {
			continue;
		}
		ExternalOwnerDecalState &st = external_owner_decals[slot.owner_key];
		if (st.has_pending_rd_slot &&
				st.pending_slot.generation == slot.generation &&
				st.pending_slot.slot_index == slot.slot_index &&
				st.pending_slot.lease_serial == slot.lease_serial) {
			st.pending_slot.present_seq = slot.present_seq;
			if (slot.world_bounds.is_finite() && slot.world_bounds.get_end().is_finite() && slot.world_bounds.has_volume()) {
				st.pending_slot.world_bounds = slot.world_bounds;
			}
			if (slot.harvest_uniform_set.is_valid()) {
				st.pending_slot.harvest_uniform_set = slot.harvest_uniform_set;
			}
			for (uint32_t ti = 0; ti < TEXTURE_SLOT_COUNT; ++ti) {
				if (slot.textures[ti].is_valid()) {
					st.pending_slot.textures[ti] = slot.textures[ti];
				}
			}
		} else {
			st.pending_slot = slot;
			st.has_pending_rd_slot = true;
		}
	}
}

bool ParticlesDecalBridgeRD::update_external_decal_slot_present_seq(
		uint64_t p_owner_key,
		uint64_t p_generation,
		uint32_t p_slot_index,
		uint64_t p_lease_serial,
		uint64_t p_next_present_seq,
		const AABB *p_world_bounds) {
	if (p_owner_key == 0 || p_next_present_seq == 0) {
		return false;
	}
	MutexLock lock(active_decals_mutex);
	HashMap<uint64_t, ExternalOwnerDecalState>::Iterator it = external_owner_decals.find(p_owner_key);
	if (!it || !it->value.has_pending_rd_slot) {
		return false;
	}
	ExternalDecalSlotRegistration &reg = it->value.pending_slot;
	if (reg.generation != p_generation || reg.slot_index != p_slot_index || reg.lease_serial != p_lease_serial) {
		return false;
	}
	reg.present_seq = p_next_present_seq;
	if (p_world_bounds && p_world_bounds->is_finite() && p_world_bounds->get_end().is_finite() && p_world_bounds->has_volume()) {
		reg.world_bounds = *p_world_bounds;
	}
	return true;
}

uint32_t ParticlesDecalBridgeRD::update_external_decal_slots_present_seq_batch(
		const ExternalDecalSlotPresentSeqUpdate *p_updates,
		uint32_t p_count) {
	if (!p_updates || p_count == 0) {
		return 0;
	}
	uint32_t updated = 0;
	MutexLock lock(active_decals_mutex);
	for (uint32_t i = 0; i < p_count; ++i) {
		const ExternalDecalSlotPresentSeqUpdate &u = p_updates[i];
		if (u.owner_key == 0 || u.next_present_seq == 0) {
			continue;
		}
		HashMap<uint64_t, ExternalOwnerDecalState>::Iterator it = external_owner_decals.find(u.owner_key);
		if (!it || !it->value.has_pending_rd_slot) {
			continue;
		}
		ExternalDecalSlotRegistration &reg = it->value.pending_slot;
		if (reg.generation != u.generation || reg.slot_index != u.slot_index || reg.lease_serial != u.lease_serial) {
			continue;
		}
		reg.present_seq = u.next_present_seq;
		if (u.has_world_bounds && u.world_bounds.is_finite() && u.world_bounds.get_end().is_finite() && u.world_bounds.has_volume()) {
			reg.world_bounds = u.world_bounds;
		}
		++updated;
	}
	return updated;
}

void ParticlesDecalBridgeRD::unregister_external_decal_slot(uint64_t p_owner_key, uint64_t p_generation, uint32_t p_slot_index) {
	if (p_owner_key == 0) {
		return;
	}
	MutexLock lock(active_decals_mutex);
	HashMap<uint64_t, ExternalOwnerDecalState>::Iterator it = external_owner_decals.find(p_owner_key);
	if (!it) {
		return;
	}
	if (it->value.has_pending_rd_slot &&
			(p_generation == 0 || it->value.pending_slot.generation == p_generation) &&
			(p_slot_index == UINT32_MAX || it->value.pending_slot.slot_index == p_slot_index)) {
		external_owner_decals.remove(it);
	}
}

void ParticlesDecalBridgeRD::unregister_external_decal_owner(uint64_t p_owner_key) {
	if (p_owner_key == 0) {
		return;
	}
	MutexLock lock(active_decals_mutex);
	external_owner_decals.erase(p_owner_key);
}

void ParticlesDecalBridgeRD::clear_registered_external_decals() {
	MutexLock lock(active_decals_mutex);
	external_owner_decals.clear();
}

Error ParticlesDecalBridgeRD::_ensure_decal_gpu_bridge(RenderingDevice *p_rd) {
	if (!p_rd) {
		return ERR_UNAVAILABLE;
	}
	if (decal_gpu_bridge_rd == p_rd && decal_harvest_pipeline.is_valid() && decal_topk_emit_pipeline.is_valid() && decal_cluster_blit_pipeline.is_valid()) {
		return OK;
	}
	if (decal_gpu_bridge_rd != nullptr && decal_gpu_bridge_rd != p_rd) {
		for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
			decal_uniform_set_cache[i] = CachedDecalUniformSet{};
		}
		decal_harvest_shader = decal_harvest_pipeline = RID();
		decal_topk_emit_shader = decal_topk_emit_pipeline = RID();
		decal_cluster_blit_shader = decal_cluster_blit_pipeline = RID();
		decal_candidate_staging_buffer = decal_cluster_elements_buffer = decal_emit_params_ubo = decal_emitter_atlas_rects_buffer = RID();
		decal_gpu_bridge_rd = nullptr;
	}

	Error err = compile_decal_bridge_compute_pipeline(*p_rd, kDecalHarvestComputeGLSL, "particle_decal128_harvest", decal_harvest_shader, decal_harvest_pipeline);
	if (err == OK) {
		err = compile_decal_bridge_compute_pipeline(*p_rd, kDecalTopKEmitComputeGLSL, "particle_decal128_topk_emit", decal_topk_emit_shader, decal_topk_emit_pipeline);
	}
	if (err == OK) {
		err = compile_decal_bridge_compute_pipeline(*p_rd, kDecalClusterBlitComputeGLSL, "particle_decal128_cluster_blit", decal_cluster_blit_shader, decal_cluster_blit_pipeline);
	}
	if (err == OK) {
		const uint32_t staging_bytes = 16u + MAX_STAGED_DECAL_CANDIDATES * uint32_t(sizeof(ParticleDecalRecord128));
		const uint32_t cluster_bytes = MAX_PARTICLE_DECALS * CLUSTER_ELEMENT_WORD_STRIDE * uint32_t(sizeof(uint32_t));
		const uint32_t atlas_rects_bytes = MAX_DISPATCH_EMITTERS * TEXTURE_SLOT_COUNT * 4u * uint32_t(sizeof(float));
		decal_candidate_staging_buffer = p_rd->storage_buffer_create(staging_bytes);
		decal_cluster_elements_buffer = p_rd->storage_buffer_create(cluster_bytes);
		decal_emit_params_ubo = p_rd->uniform_buffer_create(sizeof(DecalEmitParamsStd140));
		decal_emitter_atlas_rects_buffer = p_rd->storage_buffer_create(atlas_rects_bytes);
		if (decal_candidate_staging_buffer.is_null() || decal_cluster_elements_buffer.is_null() ||
				decal_emit_params_ubo.is_null() || decal_emitter_atlas_rects_buffer.is_null()) {
			err = ERR_CANT_CREATE;
		}
	}
	decal_gpu_bridge_rd = p_rd;
	if (err != OK) {
		_release_decal_gpu_bridge(p_rd);
		return err;
	}
	return OK;
}

void ParticlesDecalBridgeRD::_release_decal_gpu_bridge(RenderingDevice *p_rd) {
	if (!p_rd || decal_gpu_bridge_rd != p_rd) {
		return;
	}
	for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
		if (decal_uniform_set_cache[i].uniform_set.is_valid() && p_rd->uniform_set_is_valid(decal_uniform_set_cache[i].uniform_set)) {
			p_rd->free_rid(decal_uniform_set_cache[i].uniform_set);
		}
		decal_uniform_set_cache[i] = CachedDecalUniformSet{};
	}
	const RID to_free[] = {
		decal_harvest_pipeline,
		decal_harvest_shader,
		decal_topk_emit_pipeline,
		decal_topk_emit_shader,
		decal_cluster_blit_pipeline,
		decal_cluster_blit_shader,
		decal_candidate_staging_buffer,
		decal_cluster_elements_buffer,
		decal_emit_params_ubo,
		decal_emitter_atlas_rects_buffer,
	};
	for (uint32_t i = 0; i < sizeof(to_free) / sizeof(to_free[0]); ++i) {
		if (to_free[i].is_valid()) {
			p_rd->free_rid(to_free[i]);
		}
	}
	decal_harvest_pipeline = decal_harvest_shader = RID();
	decal_topk_emit_pipeline = decal_topk_emit_shader = RID();
	decal_cluster_blit_pipeline = decal_cluster_blit_shader = RID();
	decal_candidate_staging_buffer = decal_cluster_elements_buffer = decal_emit_params_ubo = decal_emitter_atlas_rects_buffer = RID();
	decal_gpu_bridge_rd = nullptr;
}

RID ParticlesDecalBridgeRD::_acquire_cached_uniform_set(RenderingDevice *p_rd, RID p_shader, const RD::Uniform *p_uniforms, uint32_t p_count) {
	if (!p_rd || p_shader.is_null() || !p_uniforms || p_count == 0 || p_count > 5u) {
		return RID();
	}
	RID key_rids[5] = {};
	for (uint32_t i = 0; i < p_count; ++i) {
		key_rids[i] = p_uniforms[i].get_id_count() > 0 ? p_uniforms[i].get_id(0) : RID();
	}
	++decal_uniform_set_tick;
	uint32_t free_idx = UINT32_MAX;
	uint32_t oldest_idx = 0;
	uint64_t oldest_tick = UINT64_MAX;
	for (uint32_t i = 0; i < UNIFORM_SET_CACHE_CAPACITY; ++i) {
		CachedDecalUniformSet &entry = decal_uniform_set_cache[i];
		if (entry.uniform_set.is_valid() && !p_rd->uniform_set_is_valid(entry.uniform_set)) {
			entry = CachedDecalUniformSet{};
		}
		if (entry.uniform_set.is_null()) {
			if (free_idx == UINT32_MAX) {
				free_idx = i;
			}
			continue;
		}
		if (entry.shader == p_shader && entry.rids[0] == key_rids[0] && entry.rids[1] == key_rids[1] &&
				entry.rids[2] == key_rids[2] && entry.rids[3] == key_rids[3] && entry.rids[4] == key_rids[4]) {
			entry.last_used_tick = decal_uniform_set_tick;
			return entry.uniform_set;
		}
		if (entry.last_used_tick < oldest_tick) {
			oldest_tick = entry.last_used_tick;
			oldest_idx = i;
		}
	}

	Vector<RD::Uniform> uvec;
	uvec.resize(p_count);
	for (uint32_t i = 0; i < p_count; ++i) {
		uvec.write[i] = p_uniforms[i];
	}
	const RID created = p_rd->uniform_set_create(uvec, p_shader, 0);
	if (created.is_null()) {
		return RID();
	}
	const uint32_t slot = (free_idx != UINT32_MAX) ? free_idx : oldest_idx;
	if (decal_uniform_set_cache[slot].uniform_set.is_valid() && p_rd->uniform_set_is_valid(decal_uniform_set_cache[slot].uniform_set)) {
		p_rd->free_rid(decal_uniform_set_cache[slot].uniform_set);
	}
	decal_uniform_set_cache[slot].shader = p_shader;
	for (uint32_t i = 0; i < 5u; ++i) {
		decal_uniform_set_cache[slot].rids[i] = key_rids[i];
	}
	decal_uniform_set_cache[slot].uniform_set = created;
	decal_uniform_set_cache[slot].last_used_tick = decal_uniform_set_tick;
	return created;
}

void ParticlesDecalBridgeRD::begin_cluster_builder_frame(
		const ClusterBuilderRD *p_builder,
		const Transform3D &p_view_xform,
		float p_z_near,
		float p_z_far,
		bool p_camera_orthogonal,
		bool p_flip_y,
		uint32_t p_max_elements_by_type) {
	active_cluster_context.builder = p_builder;
	active_cluster_context.view_xform = p_view_xform;
	active_cluster_context.z_near = p_z_near;
	active_cluster_context.z_far = p_z_far;
	active_cluster_context.camera_orthogonal = p_camera_orthogonal;
	active_cluster_context.flip_y = p_flip_y;
	active_cluster_context.max_elements_by_type = p_max_elements_by_type;
	active_cluster_context.pending_gpu_decal_count = 0;
	active_cluster_context.active = (p_builder != nullptr);
}

uint32_t ParticlesDecalBridgeRD::get_pending_cluster_gpu_decal_count(const ClusterBuilderRD *p_builder) const {
	if (!active_cluster_context.active || active_cluster_context.builder != p_builder) {
		return 0u;
	}
	return active_cluster_context.pending_gpu_decal_count;
}

void ParticlesDecalBridgeRD::clear_cluster_builder_frame(const ClusterBuilderRD *p_builder) {
	if (p_builder == nullptr || active_cluster_context.builder == p_builder) {
		active_cluster_context = ClusterFrameContext{};
	}
}

void ParticlesDecalBridgeRD::blit_cluster_gpu_decal_elements(
		RenderingDevice *p_rd,
		const ClusterBuilderRD *p_builder,
		RID p_dst_element_buffer,
		uint32_t p_dst_element_offset,
		uint32_t p_count) {
	if (!p_rd || !active_cluster_context.active || active_cluster_context.builder != p_builder || p_count == 0u ||
			p_dst_element_buffer.is_null() || decal_cluster_elements_buffer.is_null() || decal_cluster_blit_pipeline.is_null()) {
		active_cluster_context.pending_gpu_decal_count = 0u;
		return;
	}
	RD::Uniform uniforms[2];
	uniforms[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	uniforms[0].binding = 0;
	uniforms[0].append_id(decal_cluster_elements_buffer);
	uniforms[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	uniforms[1].binding = 1;
	uniforms[1].append_id(p_dst_element_buffer);
	const RID uset = _acquire_cached_uniform_set(p_rd, decal_cluster_blit_shader, uniforms, 2u);
	if (uset.is_valid()) {
		const uint32_t push[4] = { p_dst_element_offset, MIN(p_count, MAX_PARTICLE_DECALS), 0u, 0u };
		RD::ComputeListID cl = p_rd->compute_list_begin();
		p_rd->compute_list_bind_compute_pipeline(cl, decal_cluster_blit_pipeline);
		p_rd->compute_list_bind_uniform_set(cl, uset, 0);
		p_rd->compute_list_set_push_constant(cl, push, sizeof(push));
		p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
		p_rd->compute_list_end();
	}
	active_cluster_context.pending_gpu_decal_count = 0u;
}

uint32_t ParticlesDecalBridgeRD::dispatch_registered_external_decals_gpu(
		RenderingDevice *p_rd,
		RID p_source_viewport,
		RID p_source_scenario,
		bool p_is_reflection_probe,
		const Transform3D &p_camera_xform,
		RID p_decal_buffer,
		uint32_t p_base_decal_index,
		uint32_t p_max_decals,
		AtlasRectLookupFn p_atlas_rect_lookup,
		void *p_atlas_rect_ctx,
		LocalVector<ReservedGpuDecalBounds> &r_mobile_bounds_out) {
	r_mobile_bounds_out.clear();
	last_dispatch_barrier_count = 0u;
	if (active_cluster_context.active) {
		active_cluster_context.pending_gpu_decal_count = 0u;
	}
	if (!p_rd || p_decal_buffer.is_null() || p_base_decal_index >= p_max_decals) {
		return 0u;
	}

	const bool effective_reflection_probe = p_is_reflection_probe ||
			(p_source_viewport.is_null() && active_cluster_context.active && !active_cluster_context.flip_y);

	LocalVector<ExternalDecalSlotRegistration> matched_slots;
	LocalVector<uint64_t> stale_owners;
	{
		MutexLock lock(active_decals_mutex);
		for (KeyValue<uint64_t, ExternalOwnerDecalState> &E : external_owner_decals) {
			const ExternalOwnerDecalState &st = E.value;
			if (!st.has_pending_rd_slot) {
				continue;
			}
			const ExternalDecalSlotRegistration &reg = st.pending_slot;
			if (reg.capacity == 0u) {
				continue;
			}
			if (reg.meta_rid.is_null() || reg.records_rid.is_null() || reg.capacity > MAX_PARTICLE_DECALS) {
				stale_owners.push_back(E.key);
				continue;
			}
			if (reg.owner_ptr != nullptr && reg.is_slot_live_fn != nullptr) {
				if (!reg.is_slot_live_fn(reg.owner_ptr, reg.generation, reg.slot_index, reg.lease_serial)) {
					stale_owners.push_back(E.key);
					continue;
				}
			}
			if (reg.owner_ptr != nullptr && reg.is_slot_visible_fn != nullptr) {
				if (!reg.is_slot_visible_fn(reg.owner_ptr, reg.generation, reg.slot_index, reg.lease_serial)) {
					continue;
				}
			}
			if (reg.viewport_rid.is_valid()) {
				if (effective_reflection_probe) {
					continue;
				}
				if (p_source_viewport.is_valid() && p_source_viewport != reg.viewport_rid) {
					continue;
				}
			}
			if (reg.scenario_rid.is_valid() && p_source_scenario.is_valid() && p_source_scenario != reg.scenario_rid) {
				continue;
			}
			matched_slots.push_back(reg);
		}
		for (uint32_t i = 0; i < stale_owners.size(); ++i) {
			external_owner_decals.erase(stale_owners[i]);
		}
	}

	if (matched_slots.is_empty()) {
		return 0u;
	}

	std::sort(matched_slots.ptr(), matched_slots.ptr() + matched_slots.size(),
			[](const ExternalDecalSlotRegistration &a, const ExternalDecalSlotRegistration &b) {
				return a.owner_key < b.owner_key;
			});

	uint32_t avail_decals = p_max_decals - p_base_decal_index;
	if (active_cluster_context.active && active_cluster_context.max_elements_by_type > p_base_decal_index) {
		avail_decals = MIN(avail_decals, active_cluster_context.max_elements_by_type - p_base_decal_index);
	} else if (active_cluster_context.active) {
		return 0u;
	}
	if (avail_decals == 0u) {
		return 0u;
	}

	if (_ensure_decal_gpu_bridge(p_rd) != OK) {
		return 0u;
	}

	struct ValidHarvestSlot {
		ExternalDecalSlotRegistration reg;
		RID uset;
		uint32_t emitter_offset = 0u;
	};
	LocalVector<ValidHarvestSlot> valid_slots;
	valid_slots.reserve(MIN(uint32_t(matched_slots.size()), MAX_DISPATCH_EMITTERS));
	uint32_t total_valid_cap = 0u;
	for (uint32_t i = 0; i < matched_slots.size() && valid_slots.size() < MAX_DISPATCH_EMITTERS; ++i) {
		const ExternalDecalSlotRegistration &reg = matched_slots[i];
		RID uset;
		if (reg.harvest_uniform_set.is_valid() && p_rd->uniform_set_is_valid(reg.harvest_uniform_set)) {
			uset = reg.harvest_uniform_set;
		} else {
			RD::Uniform u[3];
			u[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u[0].binding = 0;
			u[0].append_id(reg.meta_rid);
			u[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u[1].binding = 1;
			u[1].append_id(reg.records_rid);
			u[2].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u[2].binding = 2;
			u[2].append_id(decal_candidate_staging_buffer);
			uset = _acquire_cached_uniform_set(p_rd, decal_harvest_shader, u, 3u);
		}
		if (uset.is_null()) {
			continue;
		}
		valid_slots.push_back({ reg, uset, total_valid_cap });
		total_valid_cap += reg.capacity;
	}
	if (valid_slots.is_empty() || total_valid_cap == 0u) {
		return 0u;
	}

	const uint32_t reserved_decals = MIN(total_valid_cap, MIN(MAX_PARTICLE_DECALS, avail_decals));
	if (reserved_decals == 0u) {
		return 0u;
	}
	const bool partitioned_mode = (total_valid_cap <= reserved_decals) && (valid_slots.size() <= MAX_DISPATCH_EMITTERS);

	RD::Uniform emit_u[5];
	emit_u[0].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[0].binding = 0;
	emit_u[0].append_id(decal_candidate_staging_buffer);
	emit_u[1].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[1].binding = 1;
	emit_u[1].append_id(p_decal_buffer);
	emit_u[2].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[2].binding = 2;
	emit_u[2].append_id(decal_cluster_elements_buffer);
	emit_u[3].uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
	emit_u[3].binding = 3;
	emit_u[3].append_id(decal_emit_params_ubo);
	emit_u[4].uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	emit_u[4].binding = 4;
	emit_u[4].append_id(decal_emitter_atlas_rects_buffer);
	const RID emit_uset = _acquire_cached_uniform_set(p_rd, decal_topk_emit_shader, emit_u, 5u);
	if (emit_uset.is_null()) {
		return 0u;
	}

	// Upload per-emitter atlas rects [albedo, normal, orm, emission]:
	LocalVector<float> atlas_rect_floats;
	atlas_rect_floats.resize(valid_slots.size() * TEXTURE_SLOT_COUNT * 4u);
	for (uint32_t i = 0; i < valid_slots.size(); ++i) {
		const ExternalDecalSlotRegistration &reg = valid_slots[i].reg;
		for (uint32_t t = 0; t < TEXTURE_SLOT_COUNT; ++t) {
			Rect2 rect;
			if (p_atlas_rect_lookup && reg.textures[t].is_valid()) {
				rect = p_atlas_rect_lookup(p_atlas_rect_ctx, reg.textures[t]);
			}
			const uint32_t fbase = (i * TEXTURE_SLOT_COUNT + t) * 4u;
			atlas_rect_floats[fbase + 0u] = float(rect.position.x);
			atlas_rect_floats[fbase + 1u] = float(rect.position.y);
			atlas_rect_floats[fbase + 2u] = float(rect.size.x);
			atlas_rect_floats[fbase + 3u] = float(rect.size.y);
		}
	}

	const Transform3D cam_inv = p_camera_xform.affine_inverse();
	const Transform3D cluster_view = active_cluster_context.active ? active_cluster_context.view_xform : cam_inv;

	DecalEmitParamsStd140 emit_params;
	const Vector3 cam_c0 = p_camera_xform.basis.get_column(0);
	const Vector3 cam_c1 = p_camera_xform.basis.get_column(1);
	const Vector3 cam_c2 = p_camera_xform.basis.get_column(2);
	emit_params.cam_xform_col0[0] = float(cam_c0.x);
	emit_params.cam_xform_col0[1] = float(cam_c0.y);
	emit_params.cam_xform_col0[2] = float(cam_c0.z);
	emit_params.cam_xform_col1[0] = float(cam_c1.x);
	emit_params.cam_xform_col1[1] = float(cam_c1.y);
	emit_params.cam_xform_col1[2] = float(cam_c1.z);
	emit_params.cam_xform_col2[0] = float(cam_c2.x);
	emit_params.cam_xform_col2[1] = float(cam_c2.y);
	emit_params.cam_xform_col2[2] = float(cam_c2.z);
	emit_params.cam_origin[0] = float(p_camera_xform.origin.x);
	emit_params.cam_origin[1] = float(p_camera_xform.origin.y);
	emit_params.cam_origin[2] = float(p_camera_xform.origin.z);

	emit_params.cam_inv_row0[0] = float(cam_inv.basis.rows[0][0]);
	emit_params.cam_inv_row0[1] = float(cam_inv.basis.rows[0][1]);
	emit_params.cam_inv_row0[2] = float(cam_inv.basis.rows[0][2]);
	emit_params.cam_inv_row1[0] = float(cam_inv.basis.rows[1][0]);
	emit_params.cam_inv_row1[1] = float(cam_inv.basis.rows[1][1]);
	emit_params.cam_inv_row1[2] = float(cam_inv.basis.rows[1][2]);
	emit_params.cam_inv_row2[0] = float(cam_inv.basis.rows[2][0]);
	emit_params.cam_inv_row2[1] = float(cam_inv.basis.rows[2][1]);
	emit_params.cam_inv_row2[2] = float(cam_inv.basis.rows[2][2]);

	emit_params.cluster_view_row0[0] = float(cluster_view.basis.rows[0][0]);
	emit_params.cluster_view_row0[1] = float(cluster_view.basis.rows[0][1]);
	emit_params.cluster_view_row0[2] = float(cluster_view.basis.rows[0][2]);
	emit_params.cluster_view_row0[3] = float(cluster_view.origin.x);
	emit_params.cluster_view_row1[0] = float(cluster_view.basis.rows[1][0]);
	emit_params.cluster_view_row1[1] = float(cluster_view.basis.rows[1][1]);
	emit_params.cluster_view_row1[2] = float(cluster_view.basis.rows[1][2]);
	emit_params.cluster_view_row1[3] = float(cluster_view.origin.y);
	emit_params.cluster_view_row2[0] = float(cluster_view.basis.rows[2][0]);
	emit_params.cluster_view_row2[1] = float(cluster_view.basis.rows[2][1]);
	emit_params.cluster_view_row2[2] = float(cluster_view.basis.rows[2][2]);
	emit_params.cluster_view_row2[3] = float(cluster_view.origin.z);

	emit_params.misc_floats[0] = active_cluster_context.active ? active_cluster_context.z_near : 0.05f;
	emit_params.misc_floats[1] = active_cluster_context.active ? active_cluster_context.z_far : 4000.0f;
	emit_params.counts[0] = p_base_decal_index;
	emit_params.counts[1] = reserved_decals;
	emit_params.counts[2] = MAX_STAGED_DECAL_CANDIDATES;
	emit_params.counts[3] = ((active_cluster_context.active && active_cluster_context.camera_orthogonal) ? 1u : 0u) | (partitioned_mode ? 2u : 0u);

	p_rd->buffer_clear(decal_candidate_staging_buffer, 0, 16u);
	p_rd->buffer_update(decal_emitter_atlas_rects_buffer, 0, atlas_rect_floats.size() * sizeof(float), atlas_rect_floats.ptr());
	p_rd->buffer_update(decal_emit_params_ubo, 0, sizeof(emit_params), &emit_params);

	{
		RD::ComputeListID cl = p_rd->compute_list_begin();
		p_rd->compute_list_bind_compute_pipeline(cl, decal_harvest_pipeline);
		for (uint32_t i = 0; i < valid_slots.size(); ++i) {
			const ValidHarvestSlot &vs = valid_slots[i];
			const uint32_t offset_u16 = partitioned_mode ? vs.emitter_offset : 0u;
			const uint32_t push[4] = {
				(vs.reg.capacity & 0xFFFFu) | ((offset_u16 & 0xFFFFu) << 16u),
				(MAX_STAGED_DECAL_CANDIDATES & 0xFFFFu) | ((i & 0xFFFFu) << 16u),
				uint32_t(vs.reg.present_seq & 0xFFFFFFFFull),
				uint32_t(vs.reg.present_seq >> 32u),
			};
			p_rd->compute_list_bind_uniform_set(cl, vs.uset, 0);
			p_rd->compute_list_set_push_constant(cl, push, sizeof(push));
			p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
		}
		p_rd->compute_list_add_barrier(cl);
		last_dispatch_barrier_count = 1u;
		p_rd->compute_list_bind_compute_pipeline(cl, decal_topk_emit_pipeline);
		p_rd->compute_list_bind_uniform_set(cl, emit_uset, 0);
		p_rd->compute_list_dispatch(cl, 1u, 1u, 1u);
		p_rd->compute_list_end();
	}

	if (active_cluster_context.active) {
		active_cluster_context.pending_gpu_decal_count = reserved_decals;
	}
	if (partitioned_mode) {
		if (valid_slots.size() == 1u) {
			const ValidHarvestSlot &vs = valid_slots[0];
			if (vs.reg.world_bounds.size != Vector3()) {
				r_mobile_bounds_out.push_back({ vs.reg.world_bounds, p_base_decal_index + vs.emitter_offset, vs.reg.capacity });
			}
		} else {
			// Round-robin rank interleaving across partitioned emitters ensures that
			// Forward Mobile's per-instance MAX_RDL_CULL (8) decal slots never starve
			// secondary emitters when geometry AABBs intersect multiple decal zones.
			uint32_t max_emitter_cap = 0u;
			for (uint32_t i = 0; i < valid_slots.size(); ++i) {
				if (valid_slots[i].reg.world_bounds.size != Vector3()) {
					max_emitter_cap = MAX(max_emitter_cap, valid_slots[i].reg.capacity);
				}
			}
			for (uint32_t rank = 0; rank < max_emitter_cap; ++rank) {
				for (uint32_t i = 0; i < valid_slots.size(); ++i) {
					const ValidHarvestSlot &vs = valid_slots[i];
					if (rank < vs.reg.capacity && vs.reg.world_bounds.size != Vector3()) {
						r_mobile_bounds_out.push_back({ vs.reg.world_bounds, p_base_decal_index + vs.emitter_offset + rank, 1u });
					}
				}
			}
		}
	} else {
		AABB union_world_bounds;
		bool has_bounds = false;
		for (uint32_t i = 0; i < valid_slots.size(); ++i) {
			if (valid_slots[i].reg.world_bounds.size != Vector3()) {
				union_world_bounds = has_bounds ? union_world_bounds.merge(valid_slots[i].reg.world_bounds) : valid_slots[i].reg.world_bounds;
				has_bounds = true;
			}
		}
		if (has_bounds) {
			r_mobile_bounds_out.push_back({ union_world_bounds, p_base_decal_index, reserved_decals });
		}
	}
	return reserved_decals;
}

} // namespace RendererRD
