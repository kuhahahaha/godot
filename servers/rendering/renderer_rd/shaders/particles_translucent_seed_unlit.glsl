/**************************************************************************/
/*  particles_translucent_seed_unlit.glsl                                 */
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

#[vertex]

#version 450

#VERSION_DEFINES

layout(location = 0) in highp vec3 vertex_attrib;
layout(location = 1) in highp vec2 uv_attrib;

layout(set = 0, binding = 0, std430) restrict readonly buffer Instance192Buffer {
	uint words[];
}
instances;

layout(push_constant, std430) uniform Params {
	mat4 view_projection;
	vec2 inv_grid;
	uint grid_cols;
	uint grid_rows;
	uint subuv_mode; // 0 = none, 1 = stepped, 2 = linear_blend
	uint wrap_mode; // 0 = clamp, 1 = repeat
	uint pad0;
	uint pad1;
}
params;

layout(location = 0) out highp vec4 v_color;
layout(location = 1) out highp vec2 v_uv0;
layout(location = 2) out highp vec2 v_uv1;
layout(location = 3) out highp float v_blend;

const uint RECORD_WORDS = 48u;

vec4 read_vec4(uint base, uint offset) {
	return uintBitsToFloat(uvec4(
			instances.words[base + offset + 0u],
			instances.words[base + offset + 1u],
			instances.words[base + offset + 2u],
			instances.words[base + offset + 3u]));
}

uint wrap_frame_index(int raw_idx, uint total_frames, uint wrap_mode) {
	if (total_frames <= 1u) {
		return 0u;
	}
	if (wrap_mode == 1u) {
		int m = raw_idx % int(total_frames);
		if (m < 0) {
			m += int(total_frames);
		}
		return uint(m);
	}
	return uint(clamp(raw_idx, 0, int(total_frames) - 1));
}

vec2 compute_subuv(vec2 base_uv, uint frame_idx, uint cols, vec2 inv_grid) {
	uint col = frame_idx % max(1u, cols);
	uint row = frame_idx / max(1u, cols);
	return (vec2(float(col), float(row)) + clamp(base_uv, vec2(0.0), vec2(1.0))) * inv_grid;
}

void main() {
	uint total_words = uint(instances.words.length());
	uint base = uint(gl_InstanceIndex) * RECORD_WORDS;
	if (base + RECORD_WORDS > total_words) {
		v_color = vec4(0.0);
		v_uv0 = vec2(0.0);
		v_uv1 = vec2(0.0);
		v_blend = 0.0;
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	vec4 row0 = read_vec4(base, 0u);
	vec4 row1 = read_vec4(base, 4u);
	vec4 row2 = read_vec4(base, 8u);
	vec4 inst_color = read_vec4(base, 36u);
	vec4 custom0 = read_vec4(base, 40u);

	vec3 world_pos = vec3(
			dot(row0.xyz, vertex_attrib) + row0.w,
			dot(row1.xyz, vertex_attrib) + row1.w,
			dot(row2.xyz, vertex_attrib) + row2.w);

	v_color = inst_color;

	uint cols = max(1u, params.grid_cols);
	uint rows = max(1u, params.grid_rows);
	uint total_frames = max(1u, cols * rows);
	float safe_frame = (isnan(custom0.y) || isinf(custom0.y))
			? 0.0
			: clamp(custom0.y, -16777216.0, 16777216.0);
	float inv_x = (params.inv_grid.x > 0.0 && !isnan(params.inv_grid.x) && !isinf(params.inv_grid.x))
			? params.inv_grid.x
			: (1.0 / float(cols));
	float inv_y = (params.inv_grid.y > 0.0 && !isnan(params.inv_grid.y) && !isinf(params.inv_grid.y))
			? params.inv_grid.y
			: (1.0 / float(rows));
	vec2 safe_inv_grid = vec2(inv_x, inv_y);

#if defined(MODE_SUBUV_LINEAR_BLEND)
	float fl = floor(safe_frame);
	int f0_raw = int(fl);
	uint f0 = wrap_frame_index(f0_raw, total_frames, params.wrap_mode);
	uint f1 = wrap_frame_index(f0_raw + 1, total_frames, params.wrap_mode);
	v_uv0 = compute_subuv(uv_attrib, f0, cols, safe_inv_grid);
	v_uv1 = compute_subuv(uv_attrib, f1, cols, safe_inv_grid);
	v_blend = (params.wrap_mode == 0u && (safe_frame <= 0.0 || safe_frame >= float(total_frames - 1u)))
			? 0.0
			: clamp(safe_frame - fl, 0.0, 1.0);
#elif defined(MODE_SUBUV_STEPPED)
	int f0_raw = int(floor(safe_frame));
	uint f0 = wrap_frame_index(f0_raw, total_frames, params.wrap_mode);
	v_uv0 = compute_subuv(uv_attrib, f0, cols, safe_inv_grid);
	v_uv1 = v_uv0;
	v_blend = 0.0;
#else
	if (params.subuv_mode == 2u) {
		float fl = floor(safe_frame);
		int f0_raw = int(fl);
		uint f0 = wrap_frame_index(f0_raw, total_frames, params.wrap_mode);
		uint f1 = wrap_frame_index(f0_raw + 1, total_frames, params.wrap_mode);
		v_uv0 = compute_subuv(uv_attrib, f0, cols, safe_inv_grid);
		v_uv1 = compute_subuv(uv_attrib, f1, cols, safe_inv_grid);
		v_blend = (params.wrap_mode == 0u && (safe_frame <= 0.0 || safe_frame >= float(total_frames - 1u)))
				? 0.0
				: clamp(safe_frame - fl, 0.0, 1.0);
	} else if (params.subuv_mode == 1u) {
		int f0_raw = int(floor(safe_frame));
		uint f0 = wrap_frame_index(f0_raw, total_frames, params.wrap_mode);
		v_uv0 = compute_subuv(uv_attrib, f0, cols, safe_inv_grid);
		v_uv1 = v_uv0;
		v_blend = 0.0;
	} else {
		vec2 clamped_uv = clamp(uv_attrib, vec2(0.0), vec2(1.0));
		v_uv0 = clamped_uv;
		v_uv1 = clamped_uv;
		v_blend = 0.0;
	}
#endif

	gl_Position = params.view_projection * vec4(world_pos, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

layout(set = 0, binding = 1) uniform sampler2D albedo_tex;

layout(location = 0) in highp vec4 v_color;
layout(location = 1) in highp vec2 v_uv0;
layout(location = 2) in highp vec2 v_uv1;
layout(location = 3) in highp float v_blend;

layout(location = 0) out highp vec4 frag_color;

void main() {
	vec4 sample0 = texture(albedo_tex, v_uv0);
#if defined(MODE_SUBUV_LINEAR_BLEND)
	vec4 sample1 = texture(albedo_tex, v_uv1);
	vec4 tex_color = mix(sample0, sample1, clamp(v_blend, 0.0, 1.0));
#elif defined(MODE_SUBUV_STEPPED)
	vec4 tex_color = sample0;
#else
	vec4 tex_color = sample0;
	if (v_blend > 0.0) {
		vec4 sample1 = texture(albedo_tex, v_uv1);
		tex_color = mix(sample0, sample1, clamp(v_blend, 0.0, 1.0));
	}
#endif
	frag_color = tex_color * v_color;
}
