/**************************************************************************/
/*  particles_external_guard_indexed144.glsl                              */
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

#[compute]

#version 450

#VERSION_DEFINES

#ifndef EXTERNAL_MAX_SURFACES
#error The original owner must supply the bounded flattened native surface limit.
#endif

#if defined(MODE_EMPTY) && defined(MODE_VALIDATE)
#error Empty packets have no segment validation dispatch.
#endif

#if defined(MODE_EMPTY) && defined(MOTION_DISABLED)
#error Empty packets already omit segment and motion buffers.
#endif

// K05 INDEXED_144 ribbon/trail external output guard.
// Enforces Float144 finite normal/zero floats, interval-widened world & host-local
// bounds containment, quad index bounds (< 4 * active_segments), and A05
// RIBBON_EXACT_SUCCESSOR endpoint-key motion-vector history validity.
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#ifndef MODE_EMPTY
layout(set = 0, binding = 0, std430) restrict readonly buffer CurrentVertices {
	uint words[];
}
current_vertices;

#ifndef MOTION_DISABLED
layout(set = 0, binding = 1, std430) restrict readonly buffer AlignedPreviousVertices {
	uint words[];
}
previous_vertices;

layout(set = 0, binding = 2, std430) restrict readonly buffer VertexHistoryValidity {
	uint words[];
}
vertex_history_validity;
#endif

layout(set = 0, binding = 3, std430) restrict readonly buffer IndexStaging {
	uint words[];
}
index_staging;
#endif

layout(set = 0, binding = 4, std430) restrict readonly buffer OutputMetadata {
	uint words[];
}
output_meta;

layout(set = 0, binding = 5, std430) readonly buffer SourceCount {
	uint words[];
}
source_count;

layout(set = 0, binding = 6, std430) restrict buffer GuardScratch {
	uint words[];
}
scratch;

layout(set = 0, binding = 7, std430) restrict buffer GuardStatus {
	uint words[];
}
status;

layout(set = 0, binding = 8, std430) restrict buffer ProtectedCommands {
	uint words[];
}
commands;

struct RibbonSurfaceProof {
	vec4 current_displacement_min;
	vec4 current_displacement_max;
	vec4 previous_displacement_min;
	vec4 previous_displacement_max;
	uvec4 command; // command.x = 1 (indexed), command.y = indices_per_segment (6), zw = 0
};

layout(set = 0, binding = 9, std430) restrict readonly buffer GeometryProof {
	vec4 current_decl_min;
	vec4 current_decl_max;
	vec4 previous_decl_min;
	vec4 previous_decl_max;
	vec4 cull_min;
	vec4 cull_max;
	vec4 current_host_inverse_rows[3];
	vec4 previous_host_inverse_rows[3];
	uvec4 source_accessor; // count word, reserved zero, reserved zero, space (1=WORLD, 2=LOCAL)
	uvec4 source_gate_words; // CountValidity, AttributesValidity, DenseIdsValidity, InstanceGate
	uvec4 options; // motion_required (0/1), reserved zero words
	RibbonSurfaceProof surfaces[];
}
proof;

layout(set = 0, binding = 10, std430) readonly buffer SourceCountValidity {
	uint words[];
}
source_count_validity;

layout(set = 0, binding = 11, std430) readonly buffer SourceAttributesValidity {
	uint words[];
}
source_attributes_validity;

layout(set = 0, binding = 12, std430) readonly buffer SourceDenseIdsValidity {
	uint words[];
}
source_dense_ids_validity;

layout(set = 0, binding = 13, std430) readonly buffer SourceInstanceGate {
	uint words[];
}
source_instance_gate;

layout(push_constant, std430) uniform Params {
	uint capacity; // max_segments
	uint sequence_low;
	uint sequence_high;
	uint surface_count;
	uint segment_base;
	uint segment_count;
	uint reserved0;
	uint reserved1;
}
params;

const uint SOURCE_FAULT = 1u;
const uint COUNT_FAULT = 2u;
const uint META_FAULT = 4u;
const uint NUMERIC_FAULT = 16u;
const uint HISTORY_FAULT = 32u;
const uint BOUNDS_FAULT = 64u;

const uint VERTEX_WORDS = 36u; // 144 bytes per Float144 vertex
const uint VERTICES_PER_SEGMENT = 4u;
const uint INDICES_PER_SEGMENT = 6u;
const float ROUNDING_FLOOR = 0.00000095367431640625; // 2^-20

void fault(uint bits, uint ordinal) {
	atomicOr(scratch.words[8], bits);
	atomicMin(scratch.words[9], ordinal);
}

bool finite_normal_or_zero(float value) {
	uint magnitude = floatBitsToUint(value) & 0x7fffffffu;
	return magnitude < 0x7f800000u && (magnitude == 0u || magnitude >= 0x00800000u);
}

bool finite_vec(vec3 value) {
	return finite_normal_or_zero(value.x) && finite_normal_or_zero(value.y) && finite_normal_or_zero(value.z);
}

float next_up(float value) {
	uint bits = floatBitsToUint(value);
	if ((bits & 0x7fffffffu) == 0u) {
		return uintBitsToFloat(1u);
	}
	return uintBitsToFloat(value > 0.0 ? bits + 1u : bits - 1u);
}

float next_down(float value) {
	return -next_up(-value);
}

vec2 rounded_interval(float value, inout bool numeric_valid) {
	if (isnan(value) || isinf(value)) {
		numeric_valid = false;
		return vec2(0.0);
	}
	uint exponent = (floatBitsToUint(value) >> 23u) & 255u;
	uint spacing_bits = exponent > 23u ? (exponent - 23u) << 23u : (exponent == 0u ? 1u : 1u << (exponent - 1u));
	precise float width = uintBitsToFloat(max(spacing_bits, floatBitsToUint(ROUNDING_FLOOR)));
	precise float low = value - width;
	precise float high = value + width;
	if (isnan(low) || isinf(low) || isnan(high) || isinf(high)) {
		numeric_valid = false;
		return vec2(0.0);
	}
	vec2 result = vec2(next_down(low), next_up(high));
	if (any(isnan(result)) || any(isinf(result))) {
		numeric_valid = false;
	}
	return result;
}

vec2 add_interval(vec2 left, vec2 right, inout bool numeric_valid) {
	precise float low = left.x + right.x;
	precise float high = left.y + right.y;
	return vec2(rounded_interval(low, numeric_valid).x, rounded_interval(high, numeric_valid).y);
}

vec2 multiply_interval(vec2 left, vec2 right, inout bool numeric_valid) {
	precise float p0 = left.x * right.x;
	precise float p1 = left.x * right.y;
	precise float p2 = left.y * right.x;
	precise float p3 = left.y * right.y;
	vec2 a = rounded_interval(p0, numeric_valid);
	vec2 b = rounded_interval(p1, numeric_valid);
	vec2 c = rounded_interval(p2, numeric_valid);
	vec2 d = rounded_interval(p3, numeric_valid);
	return vec2(min(min(a.x, b.x), min(c.x, d.x)), max(max(a.y, b.y), max(c.y, d.y)));
}

vec2 affine_interval(vec4 row, vec2 x, vec2 y, vec2 z, inout bool numeric_valid) {
	vec2 result = multiply_interval(vec2(row.x), x, numeric_valid);
	result = add_interval(result, multiply_interval(vec2(row.y), y, numeric_valid), numeric_valid);
	result = add_interval(result, multiply_interval(vec2(row.z), z, numeric_valid), numeric_valid);
	return add_interval(result, vec2(row.w), numeric_valid);
}

bool bounds_shape(vec4 lower, vec4 upper, bool strict) {
	return finite_vec(lower.xyz) && finite_vec(upper.xyz) &&
			floatBitsToUint(lower.w) == 0u && floatBitsToUint(upper.w) == 0u &&
			(strict ? all(lessThan(lower.xyz, upper.xyz)) : all(lessThanEqual(lower.xyz, upper.xyz)));
}

bool contained(vec2 x, vec2 y, vec2 z, vec3 lower, vec3 upper) {
	return x.x >= lower.x && y.x >= lower.y && z.x >= lower.z &&
			x.y <= upper.x && y.y <= upper.y && z.y <= upper.z;
}

uint failure_ordinal(uint seg_ordinal, bool previous) {
	uint stride = max(8u, params.surface_count);
	return seg_ordinal * (2u * stride) + (previous ? stride : 0u);
}

#ifndef MODE_EMPTY
uint read_vertex_word(uint v_base, uint offset, bool previous) {
#ifdef MOTION_DISABLED
	return current_vertices.words[v_base + offset];
#else
	return previous ? previous_vertices.words[v_base + offset] : current_vertices.words[v_base + offset];
#endif
}

uint validate_vertex(uint v_index, bool previous, uint seg_ordinal, out uint first_failure) {
	first_failure = 0xffffffffu;
	uint f_base = failure_ordinal(seg_ordinal, previous);
	uint v_base = v_index * VERTEX_WORDS;
	uint v_fault = 0u;

	// Words 0..27 are float attributes (with pad words 3 and 7 required zero);
	// Words 28..35 store RibbonSegmentEndpointKey32 (8 x uint32).
	for (uint i = 0u; i < 28u; i++) {
		uint raw = read_vertex_word(v_base, i, previous);
		if (i == 3u || i == 7u) {
			if (raw != 0u) {
				v_fault |= NUMERIC_FAULT;
			}
		} else if (!finite_normal_or_zero(uintBitsToFloat(raw))) {
			v_fault |= NUMERIC_FAULT;
		}
	}

#ifndef MOTION_DISABLED
	if (previous) {
		// Enforce A05 RIBBON_EXACT_SUCCESSOR: previous vertex endpoint key (words 28..35)
		// must match current vertex endpoint key bit-for-bit when history_valid == 1.
		uint cur_base = v_index * VERTEX_WORDS;
		for (uint k = 28u; k < 36u; k++) {
			if (previous_vertices.words[v_base + k] != current_vertices.words[cur_base + k]) {
				v_fault |= HISTORY_FAULT;
			}
		}
	}
#endif

	if (v_fault != 0u) {
		first_failure = f_base;
		return v_fault;
	}

	vec3 pos = vec3(
			uintBitsToFloat(read_vertex_word(v_base, 0u, previous)),
			uintBitsToFloat(read_vertex_word(v_base, 1u, previous)),
			uintBitsToFloat(read_vertex_word(v_base, 2u, previous)));
	vec3 declaration_min = previous ? proof.previous_decl_min.xyz : proof.current_decl_min.xyz;
	vec3 declaration_max = previous ? proof.previous_decl_max.xyz : proof.current_decl_max.xyz;

	for (uint surface = 0u; surface < params.surface_count; surface++) {
		RibbonSurfaceProof bound = proof.surfaces[surface];
		vec3 disp_min = previous ? bound.previous_displacement_min.xyz : bound.current_displacement_min.xyz;
		vec3 disp_max = previous ? bound.previous_displacement_max.xyz : bound.current_displacement_max.xyz;
		bool numeric_valid = true;
		vec2 wx = add_interval(rounded_interval(pos.x, numeric_valid), vec2(disp_min.x, disp_max.x), numeric_valid);
		vec2 wy = add_interval(rounded_interval(pos.y, numeric_valid), vec2(disp_min.y, disp_max.y), numeric_valid);
		vec2 wz = add_interval(rounded_interval(pos.z, numeric_valid), vec2(disp_min.z, disp_max.z), numeric_valid);
		bool inside_cull = contained(wx, wy, wz, proof.cull_min.xyz, proof.cull_max.xyz);

		vec2 lx = wx;
		vec2 ly = wy;
		vec2 lz = wz;
		if (proof.source_accessor.w == 2u) {
			vec4 inv0 = previous ? proof.previous_host_inverse_rows[0] : proof.current_host_inverse_rows[0];
			vec4 inv1 = previous ? proof.previous_host_inverse_rows[1] : proof.current_host_inverse_rows[1];
			vec4 inv2 = previous ? proof.previous_host_inverse_rows[2] : proof.current_host_inverse_rows[2];
			lx = affine_interval(inv0, wx, wy, wz, numeric_valid);
			ly = affine_interval(inv1, wx, wy, wz, numeric_valid);
			lz = affine_interval(inv2, wx, wy, wz, numeric_valid);
		}
		if (!numeric_valid) {
			v_fault |= NUMERIC_FAULT;
			first_failure = min(first_failure, f_base + surface);
		} else if (!inside_cull || !contained(lx, ly, lz, declaration_min, declaration_max)) {
			v_fault |= BOUNDS_FAULT;
			first_failure = min(first_failure, f_base + surface);
		}
	}
	return v_fault;
}
#endif

bool valid_surface_span() {
	return params.surface_count > 0u && params.surface_count <= EXTERNAL_MAX_SURFACES &&
			params.surface_count <= uint(commands.words.length()) / 5u &&
			params.surface_count <= uint(proof.surfaces.length());
}

void initialize_guard() {
	for (uint i = 0u; i < 16u; i++) {
		scratch.words[i] = 0u;
	}
	scratch.words[9] = 0xffffffffu;
	scratch.words[11] = params.surface_count;
	for (uint i = 0u; i < 8u; i++) {
		status.words[i] = 0u;
	}
	status.words[4] = params.sequence_low;
	status.words[5] = params.sequence_high;
	status.words[6] = 2u; // Profile 2 = INDEXED_144
	for (uint i = 0u; i < min(EXTERNAL_MAX_SURFACES * 5u, uint(commands.words.length())); i++) {
		commands.words[i] = 0u;
	}

	uint initial_fault = 0u;
#ifdef MODE_EMPTY
	if (params.capacity != 0u) {
		initial_fault |= COUNT_FAULT;
	}
#endif
#ifdef MOTION_DISABLED
	if (proof.options.x != 0u) {
		initial_fault |= META_FAULT;
	}
#endif
	if (!valid_surface_span() || params.capacity > (1u << 18u) ||
			params.reserved0 != 0u || params.reserved1 != 0u) {
		initial_fault |= META_FAULT;
	}
	if (uint(output_meta.words.length()) < 8u) {
		initial_fault |= META_FAULT;
	} else {
		for (uint i = 0u; i < 8u; i++) {
			scratch.words[i] = output_meta.words[i];
		}
		if (scratch.words[3] != 0u) {
			initial_fault |= SOURCE_FAULT;
		}
		if (scratch.words[0] > params.capacity || scratch.words[4] != params.capacity) {
			initial_fault |= COUNT_FAULT;
		}
		// output_meta[5] == 2u identifies INDEXED_144 profile.
		if (scratch.words[1] != 0u || scratch.words[2] != 0u || scratch.words[5] != 2u ||
				scratch.words[6] != params.sequence_low || scratch.words[7] != params.sequence_high) {
			initial_fault |= META_FAULT;
		}
	}
	if (proof.source_accessor.x >= uint(source_count.words.length())) {
		initial_fault |= SOURCE_FAULT;
	} else if (source_count.words[proof.source_accessor.x] != scratch.words[0]) {
		initial_fault |= COUNT_FAULT;
	}
	if (proof.source_gate_words.x >= uint(source_count_validity.words.length()) || source_count_validity.words[proof.source_gate_words.x] != 0u) {
		initial_fault |= SOURCE_FAULT;
	}
	if (proof.source_gate_words.y >= uint(source_attributes_validity.words.length()) || source_attributes_validity.words[proof.source_gate_words.y] != 0u) {
		initial_fault |= SOURCE_FAULT;
	}
	if (proof.source_gate_words.z >= uint(source_dense_ids_validity.words.length()) || source_dense_ids_validity.words[proof.source_gate_words.z] != 0u) {
		initial_fault |= SOURCE_FAULT;
	}
	if (proof.source_gate_words.w >= uint(source_instance_gate.words.length()) || source_instance_gate.words[proof.source_gate_words.w] != 0u) {
		initial_fault |= SOURCE_FAULT;
	}
	if (proof.source_accessor.y != 0u || proof.source_accessor.z != 0u || proof.source_accessor.w < 1u || proof.source_accessor.w > 2u ||
			proof.options.x > 1u || any(notEqual(proof.options.yzw, uvec3(0u))) ||
			!bounds_shape(proof.current_decl_min, proof.current_decl_max, true) ||
			(proof.options.x == 1u && !bounds_shape(proof.previous_decl_min, proof.previous_decl_max, true)) ||
			!bounds_shape(proof.cull_min, proof.cull_max, true)) {
		initial_fault |= META_FAULT;
	}
	for (uint row = 0u; row < 3u; row++) {
		vec4 current_row = proof.current_host_inverse_rows[row];
		vec4 previous_row = proof.previous_host_inverse_rows[row];
		if (!finite_vec(current_row.xyz) || !finite_normal_or_zero(current_row.w) ||
				(proof.options.x == 1u && (!finite_vec(previous_row.xyz) || !finite_normal_or_zero(previous_row.w)))) {
			initial_fault |= NUMERIC_FAULT;
		}
	}
	for (uint i = 0u; i < min(min(params.surface_count, EXTERNAL_MAX_SURFACES), uint(proof.surfaces.length())); i++) {
		RibbonSurfaceProof bound = proof.surfaces[i];
		if (!bounds_shape(bound.current_displacement_min, bound.current_displacement_max, false) ||
				(proof.options.x == 1u && !bounds_shape(bound.previous_displacement_min, bound.previous_displacement_max, false)) ||
				bound.command.x != 1u || bound.command.y != INDICES_PER_SEGMENT || any(notEqual(bound.command.zw, uvec2(0u)))) {
			initial_fault |= META_FAULT;
		}
	}

	scratch.words[10] = initial_fault;
	if (initial_fault != 0u) {
		fault(initial_fault, 0u);
	}
}

void main() {
	if (uint(scratch.words.length()) < 16u || uint(status.words.length()) < 8u) {
		return;
	}
#ifdef MODE_INIT
	if (gl_GlobalInvocationID.x == 0u) {
		initialize_guard();
	}
#endif
#ifdef MODE_VALIDATE
	if (!valid_surface_span() || params.surface_count != scratch.words[11]) {
		if (gl_GlobalInvocationID.x == 0u) {
			fault(META_FAULT, 0u);
		}
		return;
	}
	uint lane = gl_GlobalInvocationID.x;
	if (params.segment_base > params.capacity || params.segment_count > params.capacity - params.segment_base) {
		if (lane == 0u) {
			fault(COUNT_FAULT, 0u);
		}
		return;
	}
	if (lane >= params.segment_count) {
		return;
	}
	uint seg_ordinal = params.segment_base + lane;
	uint active_segments = scratch.words[0];
	if (seg_ordinal >= active_segments || scratch.words[10] != 0u) {
		return;
	}

	uint active_vertices = active_segments * VERTICES_PER_SEGMENT;
	uint current_vertex_cap = uint(current_vertices.words.length()) / VERTEX_WORDS;
	uint index_cap = uint(index_staging.words.length());
	if ((seg_ordinal + 1u) * VERTICES_PER_SEGMENT > current_vertex_cap ||
			(seg_ordinal + 1u) * INDICES_PER_SEGMENT > index_cap) {
		fault(COUNT_FAULT, failure_ordinal(seg_ordinal, false));
		return;
	}

	uint bits = 0u;
	uint first_failure = 0xffffffffu;

	// Validate all 6 quad indices in [0, active_vertices).
	for (uint m = 0u; m < INDICES_PER_SEGMENT; m++) {
		uint idx_val = index_staging.words[seg_ordinal * INDICES_PER_SEGMENT + m];
		if (idx_val >= active_vertices) {
			bits |= BOUNDS_FAULT;
			first_failure = min(first_failure, failure_ordinal(seg_ordinal, false));
		}
	}

	// Validate all 4 segment vertices and motion-vector history when required.
	for (uint k = 0u; k < VERTICES_PER_SEGMENT; k++) {
		uint v_index = seg_ordinal * VERTICES_PER_SEGMENT + k;
		uint cur_fail;
		bits |= validate_vertex(v_index, false, seg_ordinal, cur_fail);
		first_failure = min(first_failure, cur_fail);

#ifndef MOTION_DISABLED
		if (proof.options.x == 1u) {
			if (v_index >= uint(vertex_history_validity.words.length()) ||
					(v_index + 1u) * VERTEX_WORDS > uint(previous_vertices.words.length())) {
				bits |= HISTORY_FAULT;
				first_failure = min(first_failure, failure_ordinal(seg_ordinal, true));
			} else {
				uint h_flag = vertex_history_validity.words[v_index];
				if (h_flag > 1u) {
					bits |= HISTORY_FAULT;
					first_failure = min(first_failure, failure_ordinal(seg_ordinal, true));
				} else if (h_flag == 1u) {
					uint prev_fail;
					bits |= validate_vertex(v_index, true, seg_ordinal, prev_fail);
					first_failure = min(first_failure, prev_fail);
				}
			}
		}
#endif
	}

	if (bits != 0u) {
		fault(bits, first_failure);
	}
#endif
#ifdef MODE_FINALIZE
	if (gl_GlobalInvocationID.x != 0u) {
		return;
	}
	if (!valid_surface_span() || params.surface_count != scratch.words[11]) {
		fault(META_FAULT, 0u);
	}
	uint bits = scratch.words[8];
	uint active_segments = scratch.words[0];
	status.words[0] = bits;
	status.words[1] = bits == 0u ? active_segments : 0u;
	status.words[2] = 0u;
	status.words[3] = 0u;
	if (bits == 0u && active_segments != 0u) {
		for (uint i = 0u; i < params.surface_count; i++) {
			commands.words[i * 5u + 0u] = active_segments * INDICES_PER_SEGMENT; // indexCount
			commands.words[i * 5u + 1u] = 1u; // instanceCount
			commands.words[i * 5u + 2u] = 0u; // firstIndex
			commands.words[i * 5u + 3u] = 0u; // vertexOffset
			commands.words[i * 5u + 4u] = 0u; // firstInstance
		}
	}
#endif
}
