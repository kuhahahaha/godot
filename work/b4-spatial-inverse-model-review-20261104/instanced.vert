#version 300 es
precision highp float;
precision highp int;
#define MODE_UNSHADED
#define MAX_LIGHT_DATA_STRUCTS 8
#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8
#define MAX_ROUGHNESS_LOD 7.0
#define MAX_GLOBAL_SHADER_UNIFORMS 256
#define MAX_FORWARD_LIGHTS 8
#define RADIANCE_MAX_LOD 7.0
#define USE_INSTANCING


#define M_PI 3.14159265359

#define SHADER_IS_SRGB true
#define SHADER_SPACE_FAR -1.0

#ifdef USE_MULTIVIEW
#define OUTPUT_IS_MULTIVIEW true
#else
#define OUTPUT_IS_MULTIVIEW false
#endif

#if defined(RENDER_SHADOWS) || defined(RENDER_SHADOWS_LINEAR)
#define IN_SHADOW_PASS true
#else
#define IN_SHADOW_PASS false
#endif
// Compatibility renames. These are exposed with the "godot_" prefix
// to work around two distinct Adreno bugs:
// 1. Some Adreno devices expose ES310 functions in ES300 shaders.
//    Internally, we must use the "godot_" prefix, but user shaders
//    will be mapped automatically.
// 2. Adreno 3XX devices have poor implementations of the other packing
//    functions, so we just use our own there to keep it simple.

#ifdef USE_HALF2FLOAT
// Floating point pack/unpack functions are part of the GLSL ES 300 specification used by web and mobile.
// It appears to be safe to expose these on mobile, but when running through ANGLE this appears to break.
uint float2half(uint f) {
	uint e = f & uint(0x7f800000);
	if (e <= uint(0x38000000)) {
		return uint(0);
	} else {
		return ((f >> uint(16)) & uint(0x8000)) |
				(((e - uint(0x38000000)) >> uint(13)) & uint(0x7c00)) |
				((f >> uint(13)) & uint(0x03ff));
	}
}

uint half2float(uint h) {
	uint h_e = h & uint(0x7c00);
	return ((h & uint(0x8000)) << uint(16)) | uint((h_e >> uint(10)) != uint(0)) * (((h_e + uint(0x1c000)) << uint(13)) | ((h & uint(0x03ff)) << uint(13)));
}

uint godot_packHalf2x16(vec2 v) {
	return float2half(floatBitsToUint(v.x)) | float2half(floatBitsToUint(v.y)) << uint(16);
}

vec2 godot_unpackHalf2x16(uint v) {
	return vec2(uintBitsToFloat(half2float(v & uint(0xffff))),
			uintBitsToFloat(half2float(v >> uint(16))));
}

uint godot_packUnorm2x16(vec2 v) {
	uvec2 uv = uvec2(round(clamp(v, vec2(0.0), vec2(1.0)) * 65535.0));
	return uv.x | uv.y << uint(16);
}

vec2 godot_unpackUnorm2x16(uint p) {
	return vec2(float(p & uint(0xffff)), float(p >> uint(16))) * 0.000015259021; // 1.0 / 65535.0 optimization
}

uint godot_packSnorm2x16(vec2 v) {
	uvec2 uv = uvec2(round(clamp(v, vec2(-1.0), vec2(1.0)) * 32767.0) + 32767.0);
	return uv.x | uv.y << uint(16);
}

vec2 godot_unpackSnorm2x16(uint p) {
	vec2 v = vec2(float(p & uint(0xffff)), float(p >> uint(16)));
	return clamp((v - 32767.0) * vec2(0.00003051851), vec2(-1.0), vec2(1.0));
}

#define packHalf2x16 godot_packHalf2x16
#define unpackHalf2x16 godot_unpackHalf2x16
#define packUnorm2x16 godot_packUnorm2x16
#define unpackUnorm2x16 godot_unpackUnorm2x16
#define packSnorm2x16 godot_packSnorm2x16
#define unpackSnorm2x16 godot_unpackSnorm2x16

#endif // USE_HALF2FLOAT

// Always expose these as they are ES310 functions and not available in ES300 or GLSL 330.

uint godot_packUnorm4x8(vec4 v) {
	uvec4 uv = uvec4(round(clamp(v, vec4(0.0), vec4(1.0)) * 255.0));
	return uv.x | (uv.y << uint(8)) | (uv.z << uint(16)) | (uv.w << uint(24));
}

vec4 godot_unpackUnorm4x8(uint p) {
	return vec4(float(p & uint(0xff)), float((p >> uint(8)) & uint(0xff)), float((p >> uint(16)) & uint(0xff)), float(p >> uint(24))) * 0.00392156862; // 1.0 / 255.0
}

uint godot_packSnorm4x8(vec4 v) {
	uvec4 uv = uvec4(round(clamp(v, vec4(-1.0), vec4(1.0)) * 127.0) + 127.0);
	return uv.x | uv.y << uint(8) | uv.z << uint(16) | uv.w << uint(24);
}

vec4 godot_unpackSnorm4x8(uint p) {
	vec4 v = vec4(float(p & uint(0xff)), float((p >> uint(8)) & uint(0xff)), float((p >> uint(16)) & uint(0xff)), float(p >> uint(24)));
	return clamp((v - vec4(127.0)) * vec4(0.00787401574), vec4(-1.0), vec4(1.0));
}

#define packUnorm4x8 godot_packUnorm4x8
#define unpackUnorm4x8 godot_unpackUnorm4x8
#define packSnorm4x8 godot_packSnorm4x8
#define unpackSnorm4x8 godot_unpackSnorm4x8


#if !defined(MODE_RENDER_DEPTH) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) || defined(LIGHT_CLEARCOAT_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#ifdef MODE_UNSHADED
#ifdef USE_ADDITIVE_LIGHTING
#undef USE_ADDITIVE_LIGHTING
#endif
#endif // MODE_UNSHADED

/*
from RenderingServer:
ARRAY_VERTEX = 0, // RGB32F or RGBA16
ARRAY_NORMAL = 1, // RG16 octahedral compression or RGBA16 normal + angle
ARRAY_TANGENT = 2, // RG16 octahedral compression, sign stored in sign of G
ARRAY_COLOR = 3, // RGBA8
ARRAY_TEX_UV = 4, // RG32F
ARRAY_TEX_UV2 = 5, // RG32F
ARRAY_CUSTOM0 = 6, // Depends on ArrayCustomFormat.
ARRAY_CUSTOM1 = 7,
ARRAY_CUSTOM2 = 8,
ARRAY_CUSTOM3 = 9,
ARRAY_BONES = 10, // RGBA16UI (x2 if 8 weights)
ARRAY_WEIGHTS = 11, // RGBA16UNORM (x2 if 8 weights)
*/

/* INPUT ATTRIBS */

// Always contains vertex position in XYZ, can contain tangent angle in W.
layout(location = 0) in highp vec4 vertex_angle_attrib;
/* clang-format on */

#ifdef NORMAL_USED
// Contains Normal/Axis in RG, can contain tangent in BA.
layout(location = 1) in vec4 axis_tangent_attrib;
#endif

// location 2 is unused.

#if defined(COLOR_USED)
layout(location = 3) in vec4 color_attrib;
#endif

#ifdef UV_USED
layout(location = 4) in vec2 uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(RENDER_MATERIAL)
layout(location = 5) in vec2 uv2_attrib;
#endif

#if defined(CUSTOM0_USED)
layout(location = 6) in vec4 custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
layout(location = 7) in vec4 custom1_attrib;
#endif

#if defined(CUSTOM2_USED)
layout(location = 8) in vec4 custom2_attrib;
#endif

#if defined(CUSTOM3_USED)
layout(location = 9) in vec4 custom3_attrib;
#endif

#if defined(BONES_USED)
layout(location = 10) in uvec4 bone_attrib;
#endif

#if defined(WEIGHTS_USED)
layout(location = 11) in vec4 weight_attrib;
#endif

vec3 oct_to_vec3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	float t = max(-v.z, 0.0);
	v.xy += t * -sign(v.xy);
	return normalize(v);
}

void axis_angle_to_tbn(vec3 axis, float angle, out vec3 tangent, out vec3 binormal, out vec3 normal) {
	float c = cos(angle);
	float s = sin(angle);
	vec3 omc_axis = (1.0 - c) * axis;
	vec3 s_axis = s * axis;
	tangent = omc_axis.xxx * axis + vec3(c, -s_axis.z, s_axis.y);
	binormal = omc_axis.yyy * axis + vec3(s_axis.z, c, -s_axis.x);
	normal = omc_axis.zzz * axis + vec3(-s_axis.y, s_axis.x, c);
}

#ifdef USE_INSTANCING
layout(location = 12) in highp vec4 instance_xform0;
layout(location = 13) in highp vec4 instance_xform1;
layout(location = 14) in highp vec4 instance_xform2;
layout(location = 15) in highp uvec4 instance_color_custom_data; // Color packed into xy, Custom data into zw.
#endif

#if defined(RENDER_MOTION_VECTORS)
layout(location = 16) in highp vec4 prev_vertex_attrib;
layout(location = 17) in highp vec4 prev_normal_attrib;
#ifdef USE_INSTANCING
layout(location = 18) in highp vec4 prev_instance_xform0;
layout(location = 19) in highp vec4 prev_instance_xform1;
layout(location = 20) in highp vec4 prev_instance_xform2;
layout(location = 21) in highp uvec4 prev_instance_color_custom_data;
#endif // USE_INSTANCING
#endif // RENDER_MOTION_VECTORS

#define FLAGS_NON_UNIFORM_SCALE (1 << 4)

layout(std140) uniform GlobalShaderUniformData { //ubo:1
	vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
};

struct SceneData {
	highp mat4 projection_matrix;
	highp mat4 inv_projection_matrix;
	highp mat4 inv_view_matrix;
	highp mat4 view_matrix;

	// Used for billboards to cast correct shadows.
	highp mat4 main_cam_inv_view_matrix;

	vec2 viewport_size;
	vec2 screen_pixel_size;

	mediump vec4 ambient_light_color_energy;

	mediump float ambient_color_sky_mix;
	uint directional_shadow_count;
	float emissive_exposure_normalization;
	bool use_ambient_light;

	bool use_ambient_cubemap;
	bool use_reflection_cubemap;
	float fog_aerial_perspective;
	float time;

	mat3x4 radiance_inverse_xform;

	uint directional_light_count;
	float z_far;
	float z_near;
	float IBL_exposure_normalization;

	bool fog_enabled;
	uint fog_mode;
	float fog_density;
	float fog_height;

	float fog_height_density;
	float fog_depth_curve;
	float fog_sun_scatter;
	float fog_depth_begin;

	vec3 fog_light_color;
	float fog_depth_end;

	float shadow_bias;
	float luminance_multiplier;
	uint camera_visible_layers;
	bool pancake_shadows;
};

// The containing data block is for historic reasons.
layout(std140) uniform SceneDataBlock { // ubo:2
	SceneData data;
}
scene_data_block;

#ifdef RENDER_MOTION_VECTORS
layout(std140) uniform PrevSceneDataBlock { // ubo:13
	SceneData data;
}
prev_scene_data_block;
#endif

#ifndef RENDER_MOTION_VECTORS
#ifdef USE_ADDITIVE_LIGHTING

#if defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
struct PositionalShadowData {
	highp mat4 shadow_matrix;
	highp vec3 light_position;
	highp float shadow_normal_bias;
	vec3 pad;
	highp float shadow_atlas_pixel_size;
};

layout(std140) uniform PositionalShadows { // ubo:10
	PositionalShadowData positional_shadows[MAX_LIGHT_DATA_STRUCTS];
};

uniform lowp uint positional_shadow_index;

#else // ADDITIVE_DIRECTIONAL

struct DirectionalShadowData {
	highp vec3 direction;
	highp float shadow_atlas_pixel_size;
	highp vec4 shadow_normal_bias;
	highp vec4 shadow_split_offsets;
	highp mat4 shadow_matrix1;
	highp mat4 shadow_matrix2;
	highp mat4 shadow_matrix3;
	highp mat4 shadow_matrix4;
	mediump float fade_from;
	mediump float fade_to;
	mediump vec2 pad;
};

layout(std140) uniform DirectionalShadows { // ubo:11
	DirectionalShadowData directional_shadows[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
};

uniform lowp uint directional_shadow_index;

#endif // !(defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
#endif // USE_ADDITIVE_LIGHTING

#ifdef USE_VERTEX_LIGHTING

out vec3 diffuse_light_interp;
out vec3 specular_light_interp;

#ifdef USE_ADDITIVE_LIGHTING
out vec3 additive_diffuse_light_interp;
out vec3 additive_specular_light_interp;
#endif // USE_ADDITIVE_LIGHTING

// Directional light data.
#if !defined(DISABLE_LIGHT_DIRECTIONAL) || (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT) && defined(USE_ADDITIVE_LIGHTING))

struct DirectionalLightData {
	mediump vec3 direction;
	mediump float energy;
	mediump vec3 color;
	mediump float size;
	lowp uint enabled_bake_mode;
	mediump float shadow_opacity;
	mediump float specular;
	highp uint mask;
};

layout(std140) uniform DirectionalLights { // ubo:8
	DirectionalLightData directional_lights[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
};

#define DIRECTIONAL_LIGHT_ENABLED uint(1 << 0)
#define DIRECTIONAL_LIGHT_BAKE_STATIC uint(1 << 1)
#define DIRECTIONAL_LIGHT_BAKE_DYNAMIC uint(1 << 2)
#endif // !DISABLE_LIGHT_DIRECTIONAL

// Omni, spot, and area light data.
#if !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || !defined(DISABLE_LIGHT_AREA) || (defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT) && defined(USE_ADDITIVE_LIGHTING))

struct LightData { // This structure needs to be as packed as possible.
	highp vec3 position;
	highp float inv_radius;

	mediump vec3 direction;
	highp float size;

	mediump vec3 color;
	mediump float attenuation;

	mediump float cone_attenuation;
	mediump float cone_angle;
	mediump float specular_amount;
	mediump float shadow_opacity;

	lowp vec3 pad;
	lowp uint bake_mode;

	mediump vec4 area_width;
	mediump vec4 area_height;
};

#if !defined(DISABLE_LIGHT_OMNI) || defined(ADDITIVE_OMNI)
layout(std140) uniform OmniLightData { // ubo:5
	LightData omni_lights[MAX_LIGHT_DATA_STRUCTS];
};
#ifdef BASE_PASS
uniform uint omni_light_indices[MAX_FORWARD_LIGHTS];
uniform uint omni_light_count;
#endif // BASE_PASS
#endif // DISABLE_LIGHT_OMNI

#if !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)
layout(std140) uniform SpotLightData { // ubo:6
	LightData spot_lights[MAX_LIGHT_DATA_STRUCTS];
};
#ifdef BASE_PASS
uniform uint spot_light_indices[MAX_FORWARD_LIGHTS];
uniform uint spot_light_count;
#endif // BASE_PASS
#endif // DISABLE_LIGHT_SPOT

#if !defined(DISABLE_LIGHT_AREA)
layout(std140) uniform AreaLightData { // ubo:7
	LightData area_lights[MAX_LIGHT_DATA_STRUCTS];
};
#if defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
uniform uint area_light_indices[MAX_FORWARD_LIGHTS];
uniform uint area_light_count;
#endif // defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
#endif // !defined(DISABLE_LIGHT_AREA)

#endif // !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || (!defined(DISABLE_LIGHT_AREA) || defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT) && defined(USE_ADDITIVE_LIGHTING))

#ifdef USE_ADDITIVE_LIGHTING
#ifdef ADDITIVE_OMNI
uniform lowp uint omni_light_index;
#endif
#ifdef ADDITIVE_SPOT
uniform lowp uint spot_light_index;
#endif
#endif // USE_ADDITIVE_LIGHTING

#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)

// Eyeballed approximation of `exp2(15.0 * (1.0 - roughness) + 1.0) * 0.25`.
// Uses slightly more FMA instructions (2x rate) to avoid special instructions (0.25x rate).
// Range is reduced to [0.64,4977] from [068,2,221,528] which makes mediump feasible for the rest of the shader.
mediump float roughness_to_shininess(mediump float roughness) {
	mediump float r = 1.2 - roughness;
	mediump float r2 = r * r;
	return r * r2 * r2 * 2000.0;
}

void light_compute(vec3 N, vec3 L, vec3 V, vec3 light_color, bool is_directional, float roughness,
		inout vec3 diffuse_light, inout vec3 specular_light) {
	float NdotL = min(dot(N, L), 1.0);
	float cNdotL = max(NdotL, 0.0); // clamped NdotL

#if defined(DIFFUSE_LAMBERT_WRAP)
	// Energy conserving lambert wrap shader.
	// https://web.archive.org/web/20210228210901/http://blog.stevemcauley.com/2011/12/03/energy-conserving-wrapped-diffuse/
	float diffuse_brdf_NL = max(0.0, (cNdotL + roughness) / ((1.0 + roughness) * (1.0 + roughness))) * (1.0 / M_PI);
#else
	// lambert
	float diffuse_brdf_NL = cNdotL * (1.0 / M_PI);
#endif

	diffuse_light += light_color * diffuse_brdf_NL;

#if !defined(SPECULAR_DISABLED)
	float specular_brdf_NL = 0.0;
	// Normalized blinn always unless disabled.
	vec3 H = normalize(V + L);
	float cNdotH = clamp(dot(N, H), 0.0, 1.0);
	float shininess = roughness_to_shininess(roughness);
	float blinn = pow(cNdotH, shininess);
	blinn *= (shininess + 2.0) * (1.0 / (8.0 * M_PI)) * cNdotL;
	specular_brdf_NL = blinn;
	specular_light += specular_brdf_NL * light_color;
#endif
}

float get_omni_spot_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(distance, 0.0001), -decay);
}

#if !defined(DISABLE_LIGHT_OMNI) || (defined(ADDITIVE_OMNI) && defined(USE_ADDITIVE_LIGHTING))
void light_process_omni(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, float roughness,
		inout vec3 diffuse_light, inout vec3 specular_light) {
	vec3 light_rel_vec = omni_lights[idx].position - vertex;
	float light_length = length(light_rel_vec);
	float omni_attenuation = get_omni_spot_attenuation(light_length, omni_lights[idx].inv_radius, omni_lights[idx].attenuation);
	vec3 color = omni_lights[idx].color * omni_attenuation; // No light shaders here, so combine.

	light_compute(normal, normalize(light_rel_vec), eye_vec, color, false, roughness,
			diffuse_light,
			specular_light);
}
#endif // !defined(DISABLE_LIGHT_OMNI) || (defined(ADDITIVE_OMNI) && defined(USE_ADDITIVE_LIGHTING))

#if !defined(DISABLE_LIGHT_SPOT) || (defined(ADDITIVE_SPOT) && defined(USE_ADDITIVE_LIGHTING))
void light_process_spot(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, float roughness,
		inout vec3 diffuse_light,
		inout vec3 specular_light) {
	vec3 light_rel_vec = spot_lights[idx].position - vertex;
	float light_length = length(light_rel_vec);
	float spot_attenuation = get_omni_spot_attenuation(light_length, spot_lights[idx].inv_radius, spot_lights[idx].attenuation);
	vec3 spot_dir = spot_lights[idx].direction;
	float scos = max(dot(-normalize(light_rel_vec), spot_dir), spot_lights[idx].cone_angle);
	float spot_rim = max(0.0001, (1.0 - scos) / (1.0 - spot_lights[idx].cone_angle));

	mediump float cone_attenuation = spot_lights[idx].cone_attenuation;
	spot_attenuation *= 1.0 - pow(spot_rim, cone_attenuation);

	vec3 color = spot_lights[idx].color * spot_attenuation;

	light_compute(normal, normalize(light_rel_vec), eye_vec, color, false, roughness,
			diffuse_light, specular_light);
}
#endif // !defined(DISABLE_LIGHT_SPOT) || (defined(ADDITIVE_SPOT) && defined(USE_ADDITIVE_LIGHTING))

#endif // !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)

#endif // USE_VERTEX_LIGHTING
#endif // RENDER_MOTION_VECTORS

#ifdef USE_MULTIVIEW
struct MultiviewData {
	highp mat4 projection_matrix_view[MAX_VIEWS];
	highp mat4 inv_projection_matrix_view[MAX_VIEWS];
	highp vec4 eye_offset[MAX_VIEWS];
};

layout(std140) uniform MultiviewDataBlock { // ubo:9
	MultiviewData data;
}
multiview_data_block;

#ifdef RENDER_MOTION_VECTORS
layout(std140) uniform PrevMultiviewDataBlock { // ubo:14
	MultiviewData data;
}
prev_multiview_data_block;
#endif // RENDER_MOTION_VECTORS

#endif // USE_MULTIVIEW

uniform highp mat4 world_transform;
uniform highp vec3 compressed_aabb_position;
uniform highp vec3 compressed_aabb_size;
uniform highp vec4 uv_scale;
uniform highp uint instance_offset;
uniform highp uint layer_mask;

#if defined(RENDER_MOTION_VECTORS)
uniform highp mat4 prev_world_transform;
#endif

uniform highp uint model_flags;

#ifdef RENDER_MATERIAL
uniform mediump vec2 uv_offset;
#endif

/* Varyings */

out highp vec3 vertex_interp;
#ifdef NORMAL_USED
out vec3 normal_interp;
#endif

#if defined(COLOR_USED)
out vec4 color_interp;
#endif

#if defined(UV_USED)
out vec2 uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
out vec2 uv2_interp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
out vec3 tangent_interp;
out vec3 binormal_interp;
#endif

#ifndef RENDER_MOTION_VECTORS
#ifdef USE_ADDITIVE_LIGHTING
out highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
out highp vec4 shadow_coord2;
#endif

#ifdef LIGHT_USE_PSSM4
out highp vec4 shadow_coord3;
out highp vec4 shadow_coord4;
#endif // LIGHT_USE_PSSM4
#endif // USE_ADDITIVE_LIGHTING
#endif // RENDER_MOTION_VECTORS

#ifdef MATERIAL_UNIFORMS_USED

/* clang-format off */
layout(std140) uniform MaterialUniforms { // ubo:3



};
/* clang-format on */

#endif

/* clang-format off */

#if defined(RENDER_MOTION_VECTORS)
out highp vec4 clip_position;
out highp vec4 prev_clip_position;
#endif



/* clang-format on */
invariant gl_Position;

void vertex_shader(vec4 vertex_angle_attrib_input,
		vec3 compressed_aabb_size_input,
		vec3 compressed_aabb_position_input,
		mat4 world_transform_input,
		uint model_flags_input,
		SceneData scene_data_input,
#ifdef USE_INSTANCING
		vec4 instance_xform0_input, vec4 instance_xform1_input, vec4 instance_xform2_input,
		uvec4 instance_color_custom_data_input,
#endif
#ifdef NORMAL_USED
		vec4 axis_tangent_attrib_input,
#endif
#if defined(COLOR_USED)
		vec4 color_attrib_input,
#endif
#if defined(UV_USED)
		vec2 uv_attrib_input,
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(RENDER_MATERIAL)
		vec2 uv2_attrib_input,
#endif
#ifdef USE_MULTIVIEW
		mat4 projection_matrix,
		mat4 inv_projection_matrix,
		vec3 eye_offset,
#endif
		vec4 uv_scale_input,
		out vec4 clip_position_output) {
	highp vec3 vertex = vertex_angle_attrib_input.xyz * compressed_aabb_size_input + compressed_aabb_position_input;

	highp mat4 model_matrix = world_transform_input;
#ifdef USE_INSTANCING
	highp mat4 m = mat4(instance_xform0_input, instance_xform1_input, instance_xform2_input, vec4(0.0, 0.0, 0.0, 1.0));
	model_matrix = model_matrix * transpose(m);
#endif

#ifdef NORMAL_USED
	vec3 normal = oct_to_vec3(axis_tangent_attrib_input.xy * 2.0 - 1.0);
#endif

	highp mat3 model_normal_matrix;

	if (bool(model_flags_input & uint(FLAGS_NON_UNIFORM_SCALE))) {
		model_normal_matrix = transpose(inverse(mat3(model_matrix)));
	} else {
		model_normal_matrix = mat3(model_matrix);
	}

#if defined(NORMAL_USED) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	vec3 binormal;
	float binormal_sign;
	vec3 tangent;
	if (axis_tangent_attrib.z > 0.0 || axis_tangent_attrib.w < 1.0) {
		// Uncompressed format.
		vec2 signed_tangent_attrib = axis_tangent_attrib.zw * 2.0 - 1.0;
		tangent = oct_to_vec3(vec2(signed_tangent_attrib.x, abs(signed_tangent_attrib.y) * 2.0 - 1.0));
		binormal_sign = sign(signed_tangent_attrib.y);
		binormal = normalize(cross(normal, tangent) * binormal_sign);
	} else {
		// Compressed format.
		float angle = vertex_angle_attrib_input.w;
		binormal_sign = angle > 0.5 ? 1.0 : -1.0; // 0.5 does not exist in UNORM16, so values are either greater or smaller.
		angle = abs(angle * 2.0 - 1.0) * M_PI; // 0.5 is basically zero, allowing to encode both signs reliably.
		vec3 axis = normal;
		axis_angle_to_tbn(axis, angle, tangent, binormal, normal);
		binormal *= binormal_sign;
	}
#endif

#if defined(COLOR_USED)
	color_interp = color_attrib_input;
#ifdef USE_INSTANCING
	vec4 instance_color;
	instance_color.xy = unpackHalf2x16(instance_color_custom_data_input.x);
	instance_color.zw = unpackHalf2x16(instance_color_custom_data_input.y);
	color_interp *= instance_color;
#endif
#endif

#if defined(UV_USED)
	uv_interp = uv_attrib_input;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
	uv2_interp = uv2_attrib_input;
#endif

	if (uv_scale_input != vec4(0.0)) { // Compression enabled
#ifdef UV_USED
		uv_interp = (uv_interp - 0.5) * uv_scale_input.xy;
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP)
		uv2_interp = (uv2_interp - 0.5) * uv_scale_input.zw;
#endif
	}

#if defined(OVERRIDE_POSITION)
	highp vec4 position;
#endif

#ifndef USE_MULTIVIEW
	mat4 projection_matrix = scene_data_input.projection_matrix;
	mat4 inv_projection_matrix = scene_data_input.inv_projection_matrix;
	vec3 eye_offset = vec3(0.0, 0.0, 0.0);
#endif //!USE_MULTIVIEW

#ifdef USE_INSTANCING
	vec4 instance_custom;
	instance_custom.xy = unpackHalf2x16(instance_color_custom_data_input.z);
	instance_custom.zw = unpackHalf2x16(instance_color_custom_data_input.w);
#else
	vec4 instance_custom = vec4(0.0);
#endif

	// Using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)

	vertex = (model_matrix * vec4(vertex, 1.0)).xyz;

#ifdef NORMAL_USED
	normal = model_normal_matrix * normal;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	tangent = model_normal_matrix * tangent;
	binormal = model_normal_matrix * binormal;

#endif
#endif

#ifdef Z_CLIP_SCALE_USED
	float z_clip_scale = 1.0;
#endif

	float roughness = 1.0;

	highp mat4 modelview = scene_data_input.view_matrix * model_matrix;
	highp mat3 modelview_normal = mat3(scene_data_input.view_matrix) * model_normal_matrix;

	float point_size = 1.0;

	{
vertex=(inverse(model_matrix)*model_matrix*vec4(vertex,1.0)).xyz;
	}

	gl_PointSize = point_size;

	// Using local coordinates (default)
#if !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

	vertex = (modelview * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
	normal = modelview_normal * normal;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	binormal = modelview_normal * binormal;
	tangent = modelview_normal * tangent;
#endif
#endif // !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

	// Using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)

	vertex = (scene_data_input.view_matrix * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
	normal = (scene_data_input.view_matrix * vec4(normal, 0.0)).xyz;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
	binormal = (scene_data_input.view_matrix * vec4(binormal, 0.0)).xyz;
	tangent = (scene_data_input.view_matrix * vec4(tangent, 0.0)).xyz;
#endif
#endif

	vertex_interp = vertex;

	// Normalize TBN vectors before interpolation, per MikkTSpace.
	// See: http://www.mikktspace.com/
#ifdef NORMAL_USED
	normal_interp = normalize(normal);
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
	tangent_interp = normalize(tangent);
	binormal_interp = normalize(binormal);
#endif

#ifndef RENDER_MOTION_VECTORS
	// Calculate shadows.
#ifdef USE_ADDITIVE_LIGHTING
#if defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
	// Apply normal bias at draw time to avoid issues with scaling non-fused geometry.
	vec3 light_rel_vec = positional_shadows[positional_shadow_index].light_position - vertex_interp;
	float light_length = length(light_rel_vec);
	float aNdotL = abs(dot(normalize(normal_interp), normalize(light_rel_vec)));
	vec3 normal_offset = (1.0 - aNdotL) * positional_shadows[positional_shadow_index].shadow_normal_bias * light_length * normal_interp;

#ifdef ADDITIVE_SPOT
	// Calculate coord here so we can take advantage of prefetch.
	shadow_coord = positional_shadows[positional_shadow_index].shadow_matrix * vec4(vertex_interp + normal_offset, 1.0);
#endif

#ifdef ADDITIVE_OMNI
	// Can't interpolate unit direction nicely, so forget about prefetch.
	shadow_coord = vec4(vertex_interp + normal_offset, 1.0);
#endif
#else // ADDITIVE_DIRECTIONAL
	vec3 base_normal_bias = normalize(normal_interp) * (1.0 - max(0.0, dot(directional_shadows[directional_shadow_index].direction, -normalize(normal_interp))));
	vec3 normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.x;
	shadow_coord = directional_shadows[directional_shadow_index].shadow_matrix1 * vec4(vertex_interp + normal_offset, 1.0);

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
	normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.y;
	shadow_coord2 = directional_shadows[directional_shadow_index].shadow_matrix2 * vec4(vertex_interp + normal_offset, 1.0);
#endif

#ifdef LIGHT_USE_PSSM4
	normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.z;
	shadow_coord3 = directional_shadows[directional_shadow_index].shadow_matrix3 * vec4(vertex_interp + normal_offset, 1.0);
	normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.w;
	shadow_coord4 = directional_shadows[directional_shadow_index].shadow_matrix4 * vec4(vertex_interp + normal_offset, 1.0);
#endif //LIGHT_USE_PSSM4

#endif // !(defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT))
#endif // USE_ADDITIVE_LIGHTING

#if defined(RENDER_SHADOWS) && !defined(RENDER_SHADOWS_LINEAR)
	// This is an optimized version of normalize(vertex_interp) * scene_data_input.shadow_bias / length(vertex_interp).
	float light_length_sq = dot(vertex_interp, vertex_interp);
	vertex_interp += vertex_interp * scene_data_input.shadow_bias / light_length_sq;
#endif
#endif // RENDER_MOTION_VECTORS

#if defined(OVERRIDE_POSITION)
	clip_position_output = position;
#else
	clip_position_output = projection_matrix * vec4(vertex_interp, 1.0);
#endif

#if !defined(RENDER_SHADOWS) && !defined(RENDER_SHADOWS_LINEAR)
#ifdef Z_CLIP_SCALE_USED
	clip_position_output.z = mix(clip_position_output.w, clip_position_output.z, z_clip_scale);
#endif
#endif

#ifdef RENDER_MATERIAL
	vec2 uv_dest_attrib;
	if (uv_scale_input != vec4(0.0)) {
		uv_dest_attrib = (uv2_attrib_input.xy - 0.5) * uv_scale_input.zw;
	} else {
		uv_dest_attrib = uv2_attrib_input.xy;
	}

	clip_position_output.xy = (uv_dest_attrib + uv_offset) * 2.0 - 1.0;
	clip_position_output.z = 0.00001;
	clip_position_output.w = 1.0;
#endif

#ifndef RENDER_MOTION_VECTORS
#ifdef USE_VERTEX_LIGHTING
#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)
#ifdef USE_MULTIVIEW
	vec3 view = -normalize(vertex_interp - eye_offset);
#else
	vec3 view = -normalize(vertex_interp);
#endif
	diffuse_light_interp = vec3(0.0);
	specular_light_interp = vec3(0.0);
#ifdef BASE_PASS
#ifndef DISABLE_LIGHT_DIRECTIONAL
	for (uint i = uint(0); i < scene_data_input.directional_light_count; i++) {
		if (!bool(directional_lights[i].mask & layer_mask)) {
			continue;
		}
#if defined(USE_LIGHTMAP) && !defined(DISABLE_LIGHTMAP)
		if (bool(directional_lights[i].enabled_bake_mode & DIRECTIONAL_LIGHT_BAKE_STATIC)) {
			continue;
		}
#endif
		light_compute(normal_interp, normalize(directional_lights[i].direction), normalize(view), directional_lights[i].color * directional_lights[i].energy, true, roughness,
				diffuse_light_interp.rgb,
				specular_light_interp.rgb);
	}
#endif // !DISABLE_LIGHT_DIRECTIONAL

#ifndef DISABLE_LIGHT_OMNI
	for (uint i = 0u; i < omni_light_count; i++) {
		light_process_omni(omni_light_indices[i], vertex_interp, view, normal_interp, roughness,
				diffuse_light_interp.rgb, specular_light_interp.rgb);
	}
#endif // !DISABLE_LIGHT_OMNI

#ifndef DISABLE_LIGHT_SPOT
	for (uint i = 0u; i < spot_light_count; i++) {
		light_process_spot(spot_light_indices[i], vertex_interp, view, normal_interp, roughness,
				diffuse_light_interp.rgb, specular_light_interp.rgb);
	}
#endif // !DISABLE_LIGHT_SPOT
#endif // BASE_PASS

/* ADDITIVE LIGHTING PASS */
#ifdef USE_ADDITIVE_LIGHTING
	additive_diffuse_light_interp = vec3(0.0);
	additive_specular_light_interp = vec3(0.0);
#if !defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT)

	if (bool(directional_lights[directional_shadow_index].mask & layer_mask)) {
		light_compute(normal_interp, normalize(directional_lights[directional_shadow_index].direction), normalize(view), directional_lights[directional_shadow_index].color * directional_lights[directional_shadow_index].energy, true, roughness,
				additive_diffuse_light_interp.rgb,
				additive_specular_light_interp.rgb);
	}
#endif // !defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT)

#ifdef ADDITIVE_OMNI
	light_process_omni(omni_light_index, vertex_interp, view, normal_interp, roughness,
			additive_diffuse_light_interp.rgb, additive_specular_light_interp.rgb);
#endif // ADDITIVE_OMNI

#ifdef ADDITIVE_SPOT
	light_process_spot(spot_light_index, vertex_interp, view, normal_interp, roughness,
			additive_diffuse_light_interp.rgb, additive_specular_light_interp.rgb);
#endif // ADDITIVE_SPOT

#endif // USE_ADDITIVE_LIGHTING
#endif // !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)
#endif // USE_VERTEX_LIGHTING
#endif // RENDER_MOTION_VECTORS
}

void main() {
#if defined(RENDER_MOTION_VECTORS)

#ifdef USE_INSTANCING
	// Check for inactive particle instances.
	highp vec4 input_instance_xform0;
	highp vec4 input_instance_xform1;
	highp vec4 input_instance_xform2;
	highp uvec4 input_instance_color_custom_data;
	if (prev_instance_xform0.xyz == vec3(0.0, 0.0, 0.0)) {
		input_instance_xform0 = instance_xform0;
		input_instance_xform1 = instance_xform1;
		input_instance_xform2 = instance_xform2;
		input_instance_color_custom_data = instance_color_custom_data;
	} else {
		input_instance_xform0 = prev_instance_xform0;
		input_instance_xform1 = prev_instance_xform1;
		input_instance_xform2 = prev_instance_xform2;
		input_instance_color_custom_data = prev_instance_color_custom_data;
	}
#endif

	vertex_shader(prev_vertex_attrib,
			compressed_aabb_size,
			compressed_aabb_position,
			prev_world_transform,
			model_flags,
			prev_scene_data_block.data,
#ifdef USE_INSTANCING
			input_instance_xform0, input_instance_xform1, input_instance_xform2,
			input_instance_color_custom_data,
#endif
#ifdef NORMAL_USED
			prev_normal_attrib,
#endif
#if defined(COLOR_USED)
			color_attrib,
#endif
#if defined(UV_USED)
			uv_attrib,
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(RENDER_MATERIAL)
			uv2_attrib,
#endif
#ifdef USE_MULTIVIEW
			prev_multiview_data_block.data.projection_matrix_view[ViewIndex],
			prev_multiview_data_block.data.inv_projection_matrix_view[ViewIndex],
			prev_multiview_data_block.data.eye_offset[ViewIndex].xyz,
#endif
			uv_scale,
			prev_clip_position);
#else
	vec4 clip_position;
#endif // defined(RENDER_MOTION_VECTORS)

	vertex_shader(vertex_angle_attrib,
			compressed_aabb_size,
			compressed_aabb_position,
			world_transform,
			model_flags,
			scene_data_block.data,
#ifdef USE_INSTANCING
			instance_xform0, instance_xform1, instance_xform2,
			instance_color_custom_data,
#endif
#ifdef NORMAL_USED
			axis_tangent_attrib,
#endif
#if defined(COLOR_USED)
			color_attrib,
#endif
#if defined(UV_USED)
			uv_attrib,
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(RENDER_MATERIAL)
			uv2_attrib,
#endif
#ifdef USE_MULTIVIEW
			multiview_data_block.data.projection_matrix_view[ViewIndex],
			multiview_data_block.data.inv_projection_matrix_view[ViewIndex],
			multiview_data_block.data.eye_offset[ViewIndex].xyz,
#endif
			uv_scale,
			clip_position);

	gl_Position = clip_position;
}

/* clang-format off */
