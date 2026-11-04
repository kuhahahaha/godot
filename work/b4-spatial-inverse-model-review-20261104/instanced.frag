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


// Default to SPECULAR_SCHLICK_GGX.
#if !defined(SPECULAR_DISABLED) && !defined(SPECULAR_SCHLICK_GGX) && !defined(SPECULAR_TOON)
#define SPECULAR_SCHLICK_GGX
#endif

#if !defined(MODE_RENDER_DEPTH) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) ||defined(LIGHT_CLEARCOAT_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#ifdef MODE_UNSHADED
#ifdef USE_ADDITIVE_LIGHTING
#undef USE_ADDITIVE_LIGHTING
#endif
#endif // MODE_UNSHADED

#ifndef MODE_RENDER_DEPTH
layout(std140) uniform TonemapData { //ubo:0
	float exposure;
	int tonemapper;
	int pad;
	int pad2;
	vec4 tonemapper_params;
	float brightness;
	float contrast;
	float saturation;
	int pad3;
};

// This approximation expects non-negative input; negative input is undefined behavior.
vec3 linear_to_srgb(vec3 color) {
	//const vec3 a = vec3(0.055f);
	//return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

// This approximation expects non-negative input; negative input behaves poorly.
vec3 srgb_to_linear(vec3 color) {
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

#ifdef APPLY_TONEMAPPING

// Based on Reinhard's extended formula, see equation 4 in https://doi.org/cjbgrt
vec3 tonemap_reinhard(vec3 color) {
	float white_squared = tonemapper_params.x;
	vec3 white_squared_color = white_squared * color;
	// Equivalent to color * (1 + color / white_squared) / (1 + color)
	return (white_squared_color + color * color) / (white_squared_color + white_squared);
}

vec3 tonemap_filmic(vec3 color) {
	// These constants must match the those in the C++ code that calculates the parameters.
	// exposure_bias: Input scale (color *= bias, env->white *= bias) to make the brightness consistent with other tonemappers.
	// Also useful to scale the input to the range that the tonemapper is designed for (some require very high input values).
	// Has no effect on the curve's general shape or visual properties.
	const float exposure_bias = 2.0f;
	const float A = 0.22f * exposure_bias * exposure_bias; // bias baked into constants for performance
	const float B = 0.30f * exposure_bias;
	const float C = 0.10f;
	const float D = 0.20f;
	const float E = 0.01f;
	const float F = 0.30f;

	vec3 color_tonemapped = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;

	return color_tonemapped / tonemapper_params.x;
}

// Adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// (MIT License).
vec3 tonemap_aces(vec3 color) {
	// These constants must match the those in the C++ code that calculates the parameters.
	const float exposure_bias = 1.8f;
	const float A = 0.0245786f;
	const float B = 0.000090537f;
	const float C = 0.983729f;
	const float D = 0.432951f;
	const float E = 0.238081f;

	// Exposure bias baked into transform to save shader instructions. Equivalent to `color *= exposure_bias`
	const mat3 rgb_to_rrt = mat3(
			vec3(0.59719f * exposure_bias, 0.35458f * exposure_bias, 0.04823f * exposure_bias),
			vec3(0.07600f * exposure_bias, 0.90834f * exposure_bias, 0.01566f * exposure_bias),
			vec3(0.02840f * exposure_bias, 0.13383f * exposure_bias, 0.83777f * exposure_bias));

	const mat3 odt_to_rgb = mat3(
			vec3(1.60475f, -0.53108f, -0.07367f),
			vec3(-0.10208f, 1.10813f, -0.00605f),
			vec3(-0.00327f, -0.07276f, 1.07602f));

	color *= rgb_to_rrt;
	vec3 color_tonemapped = (color * (color + A) - B) / (color * (C * color + D) + E);
	color_tonemapped *= odt_to_rgb;

	return color_tonemapped / tonemapper_params.x;
}

// allenwp tonemapping curve; developed for use in the Godot game engine.
// Source and details: https://allenwp.com/blog/2025/05/29/allenwp-tonemapping-curve/
// Input must be a non-negative linear scene value.
vec3 allenwp_curve(vec3 x) {
	const float output_max_value = 1.0; // SDR always has an output_max_value of 1.0

	// These constants must match the those in the C++ code that calculates the parameters.
	// 18% "middle gray" is perceptually 50% of the brightness of reference white.
	const float awp_crossover_point = 0.18;
	// When output_max_value and/or awp_crossover_point are no longer constant,
	// awp_shoulder_max can be calculated on the CPU and passed in as tonemap_e.
	const float awp_shoulder_max = output_max_value - awp_crossover_point;

	float awp_contrast = tonemapper_params.x;
	float awp_toe_a = tonemapper_params.y;
	float awp_slope = tonemapper_params.z;
	float awp_w = tonemapper_params.w;

	// Reinhard-like shoulder:
	vec3 s = x - awp_crossover_point;
	vec3 slope_s = awp_slope * s;
	s = slope_s * (1.0 + s / awp_w) / (1.0 + (slope_s / awp_shoulder_max));
	s += awp_crossover_point;

	// Sigmoid power function toe:
	vec3 t = pow(x, vec3(awp_contrast));
	t = t / (t + awp_toe_a);

	return mix(s, t, lessThan(x, vec3(awp_crossover_point)));
}

// This is an approximation and simplification of EaryChow's AgX implementation that is used by Blender.
// This code is based off of the script that generates the AgX_Base_sRGB.cube LUT that Blender uses.
// Source: https://github.com/EaryChow/AgX_LUT_Gen/blob/main/AgXBasesRGB.py
// Colorspace transformation source: https://www.colour-science.org:8010/apps/rgb_colourspace_transformation_matrix
vec3 tonemap_agx(vec3 color) {
	// Input color should be non-negative!
	// Large negative values in one channel and large positive values in other
	// channels can result in a colour that appears darker and more saturated than
	// desired after passing it through the inset matrix. For this reason, it is
	// best to prevent negative input values.
	// This is done before the Rec. 2020 transform to allow the Rec. 2020
	// transform to be combined with the AgX inset matrix. This results in a loss
	// of color information that could be correctly interpreted within the
	// Rec. 2020 color space as positive RGB values, but is often not worth
	// the performance cost of an additional matrix multiplication.
	//
	// Additionally, this AgX configuration was created subjectively based on
	// output appearance in the Rec. 709 color gamut, so it is possible that these
	// matrices will not perform well with non-Rec. 709 output (more testing with
	// future wide-gamut displays is be needed).
	// See this comment from the author on the decisions made to create the matrices:
	// https://github.com/godotengine/godot-proposals/issues/12317#issuecomment-2835824250

	// Combined Rec. 709 to Rec. 2020 and Blender AgX inset matrices:
	const mat3 rec709_to_rec2020_agx_inset_matrix = mat3(
			0.544814746488245, 0.140416948464053, 0.0888104196149096,
			0.373787398372697, 0.754137554567394, 0.178871756420858,
			0.0813978551390581, 0.105445496968552, 0.732317823964232);

	// Combined inverse AgX outset matrix and Rec. 2020 to Rec. 709 matrices.
	const mat3 agx_outset_rec2020_to_rec709_matrix = mat3(
			1.96488741169489, -0.299313364904742, -0.164352742528393,
			-0.855988495690215, 1.32639796461980, -0.238183969428088,
			-0.108898916004672, -0.0270845997150571, 1.40253671195648);

	const float output_max_value = 1.0; // SDR always has an output_max_value of 1.0

	// Apply inset matrix.
	color = rec709_to_rec2020_agx_inset_matrix * color;

	// Use the allenwp tonemapping curve to match the Blender AgX curve while
	// providing stability across all variable dyanimc range (SDR, HDR, EDR).
	color = allenwp_curve(color);

	// Clipping to output_max_value is required to address a cyan colour that occurs
	// with very bright inputs.
	color = min(vec3(output_max_value), color);

	// Apply outset to make the result more chroma-laden and then go back to Rec. 709.
	color = agx_outset_rec2020_to_rec709_matrix * color;

	// Blender's lusRGB.compensate_low_side is too complex for this shader, so
	// simply return the color, even if it has negative components. These negative
	// components may be useful for subsequent color adjustments.
	return color;
}

#define TONEMAPPER_LINEAR 0
#define TONEMAPPER_REINHARD 1
#define TONEMAPPER_FILMIC 2
#define TONEMAPPER_ACES 3
#define TONEMAPPER_AGX 4

vec3 apply_tonemapping(vec3 color) { // inputs are LINEAR
	if (tonemapper == TONEMAPPER_LINEAR) {
		return color;
	}

	// Ensure color values passed to tonemappers are positive.
	// They can be negative in the case of negative lights, which leads to undesired behavior.
	color = max(vec3(0.0), color);

	if (tonemapper == TONEMAPPER_REINHARD) {
		return tonemap_reinhard(color);
	} else if (tonemapper == TONEMAPPER_FILMIC) {
		return tonemap_filmic(color);
	} else if (tonemapper == TONEMAPPER_ACES) {
		return tonemap_aces(color);
	} else { // TONEMAPPER_AGX
		return tonemap_agx(color);
	}
}

#endif // APPLY_TONEMAPPING

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


/* texture unit usage, N is max_texture_unit-N

1-color correction // In tonemap_inc.glsl
2-radiance
3-shadow
4-lightmap textures
5-shadowmask textures
6-screen
7-depth
8-reflection probe 1
9-reflection probe 2
10-area light LUT 1
11-area light LUT 2

*/

#define M_PI 3.14159265359
/* clang-format on */

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

#define FLAGS_NON_UNIFORM_SCALE (1 << 4)

/* Varyings */

#if defined(RENDER_MOTION_VECTORS)
in highp vec4 clip_position;
in highp vec4 prev_clip_position;
#endif

#ifndef RENDER_MOTION_VECTORS
#if defined(COLOR_USED)
in vec4 color_interp;
#endif

#if defined(UV_USED)
in vec2 uv_interp;
#endif

#if defined(UV2_USED)
in vec2 uv2_interp;
#else
#ifdef USE_LIGHTMAP
in vec2 uv2_interp;
#endif
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
in vec3 tangent_interp;
in vec3 binormal_interp;
#endif

#ifdef NORMAL_USED
in vec3 normal_interp;
#endif

in highp vec3 vertex_interp;

#ifdef USE_ADDITIVE_LIGHTING
in highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
in highp vec4 shadow_coord2;
#endif

#ifdef LIGHT_USE_PSSM4
in highp vec4 shadow_coord3;
in highp vec4 shadow_coord4;
#endif //LIGHT_USE_PSSM4
#endif

#ifdef USE_RADIANCE_MAP

#define RADIANCE_MAX_LOD 5.0

uniform samplerCube radiance_map; // texunit:-2

#endif // USE_RADIANCE_MAP

#ifndef DISABLE_REFLECTION_PROBE

#define REFLECTION_PROBE_MAX_LOD 8.0

uniform bool refprobe1_use_box_project;
uniform highp vec3 refprobe1_box_extents;
uniform vec3 refprobe1_box_offset;
uniform highp mat4 refprobe1_local_matrix;
uniform bool refprobe1_exterior;
uniform float refprobe1_intensity;
uniform float refprobe1_blend_distance;
uniform int refprobe1_ambient_mode;
uniform vec4 refprobe1_ambient_color;

uniform samplerCube refprobe1_texture; // texunit:-8

#ifdef SECOND_REFLECTION_PROBE

uniform bool refprobe2_use_box_project;
uniform highp vec3 refprobe2_box_extents;
uniform vec3 refprobe2_box_offset;
uniform highp mat4 refprobe2_local_matrix;
uniform bool refprobe2_exterior;
uniform float refprobe2_intensity;
uniform float refprobe2_blend_distance;
uniform int refprobe2_ambient_mode;
uniform vec4 refprobe2_ambient_color;

uniform samplerCube refprobe2_texture; // texunit:-9

#endif // SECOND_REFLECTION_PROBE

#endif // DISABLE_REFLECTION_PROBE

layout(std140) uniform GlobalShaderUniformData { //ubo:1
	vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
};

/* Material Uniforms */
#ifdef MATERIAL_UNIFORMS_USED

/* clang-format off */
layout(std140) uniform MaterialUniforms { // ubo:3



};
/* clang-format on */

#endif

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

layout(std140) uniform SceneDataBlock { // ubo:2
	SceneData data;
}
scene_data_block;

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
#endif

uniform highp mat4 world_transform;
uniform highp uint instance_offset;
uniform highp uint layer_mask;
uniform highp uint model_flags;

#define DIRECTIONAL_LIGHT_ENABLED uint(1 << 0)
#define DIRECTIONAL_LIGHT_BAKE_STATIC uint(1 << 1)
#define DIRECTIONAL_LIGHT_BAKE_DYNAMIC uint(1 << 2)

#ifndef MODE_RENDER_DEPTH
#ifdef USE_VERTEX_LIGHTING
in vec3 diffuse_light_interp;
in vec3 specular_light_interp;

#ifdef USE_ADDITIVE_LIGHTING
in vec3 additive_diffuse_light_interp;
in vec3 additive_specular_light_interp;
#endif // USE_ADDITIVE_LIGHTING
#endif // USE_VERTEX_LIGHTING

// Directional light data.
#if !defined(DISABLE_LIGHT_DIRECTIONAL) || (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT)) || defined(USE_SUN_SCATTER)

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

#if defined(USE_ADDITIVE_LIGHTING) && (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT))
// Directional shadows can be in the base pass or in the additive passes
uniform highp sampler2DShadow directional_shadow_atlas; // texunit:-3
#endif // defined(USE_ADDITIVE_LIGHTING) && (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT))

#endif // !DISABLE_LIGHT_DIRECTIONAL || USE_SUN_SCATTER

// Omni, spot, and area light data.
#if !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || !defined(DISABLE_LIGHT_AREA) || defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)

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
#if defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
uniform uint omni_light_indices[MAX_FORWARD_LIGHTS];
uniform uint omni_light_count;
#endif // defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
#endif // !defined(DISABLE_LIGHT_OMNI) || defined(ADDITIVE_OMNI)

#if !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)
layout(std140) uniform SpotLightData { // ubo:6
	LightData spot_lights[MAX_LIGHT_DATA_STRUCTS];
};
#if defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
uniform uint spot_light_indices[MAX_FORWARD_LIGHTS];
uniform uint spot_light_count;
#endif // defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
#endif // !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)

#if !defined(DISABLE_LIGHT_AREA)
layout(std140) uniform AreaLightData { // ubo:7
	LightData area_lights[MAX_LIGHT_DATA_STRUCTS];
};
uniform highp sampler2D ltc_lut1; // texunit:-10
uniform highp sampler2D ltc_lut2; // texunit:-11

#if defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
uniform uint area_light_indices[MAX_FORWARD_LIGHTS];
uniform uint area_light_count;
#endif // defined(BASE_PASS) && !defined(USE_VERTEX_LIGHTING)
#endif // !defined(DISABLE_LIGHT_AREA)

#endif // !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || !defined(DISABLE_LIGHT_AREA) || defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)

#ifdef USE_ADDITIVE_LIGHTING
#ifdef ADDITIVE_OMNI
uniform highp samplerCubeShadow omni_shadow_texture; // texunit:-3
uniform lowp uint omni_light_index;
#endif
#ifdef ADDITIVE_SPOT
uniform highp sampler2DShadow spot_shadow_texture; // texunit:-3
uniform lowp uint spot_light_index;
#endif

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
#endif // !(defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT))

#if !defined(ADDITIVE_OMNI)
float sample_shadow(highp sampler2DShadow shadow, float shadow_pixel_size, vec4 pos) {
	// Use textureProjLod with LOD set to 0.0 over textureProj, as textureProj not working correctly on ANGLE with Metal backend.
	// https://github.com/godotengine/godot/issues/93537
	float avg = textureProjLod(shadow, pos, 0.0);
#ifdef SHADOW_MODE_PCF_13
	pos /= pos.w;
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(shadow_pixel_size * 2.0, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(-shadow_pixel_size * 2.0, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, shadow_pixel_size * 2.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, -shadow_pixel_size * 2.0), pos.zw), 0.0);

	// Early bail if distant samples are fully shaded (or none are shaded) to improve performance.
	if (avg <= 0.000001) {
		// None shaded at all.
		return 0.0;
	} else if (avg >= 4.999999) {
		// All fully shaded.
		return 1.0;
	}

	avg += textureProjLod(shadow, vec4(pos.xy + vec2(shadow_pixel_size, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(-shadow_pixel_size, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, -shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(shadow_pixel_size, shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(-shadow_pixel_size, shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(shadow_pixel_size, -shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(-shadow_pixel_size, -shadow_pixel_size), pos.zw), 0.0);
	return avg * (1.0 / 13.0);
#endif

#ifdef SHADOW_MODE_PCF_5
	pos /= pos.w;
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(shadow_pixel_size, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(-shadow_pixel_size, 0.0), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, shadow_pixel_size), pos.zw), 0.0);
	avg += textureProjLod(shadow, vec4(pos.xy + vec2(0.0, -shadow_pixel_size), pos.zw), 0.0);
	return avg * (1.0 / 5.0);

#endif

	return avg;
}
#endif //!defined(ADDITIVE_OMNI)
#endif // USE_ADDITIVE_LIGHTING

#endif // !MODE_RENDER_DEPTH

#ifndef DISABLE_LIGHTMAP
#ifdef USE_LIGHTMAP
uniform mediump sampler2DArray lightmap_textures; //texunit:-4
uniform lowp sampler2DArray shadowmask_textures; //texunit:-5
uniform lowp uint lightmap_slice;
uniform highp vec4 lightmap_uv_scale;
uniform float lightmap_exposure_normalization;
uniform uint lightmap_shadowmask_mode;

#define SHADOWMASK_MODE_NONE uint(0)
#define SHADOWMASK_MODE_REPLACE uint(1)
#define SHADOWMASK_MODE_OVERLAY uint(2)
#define SHADOWMASK_MODE_ONLY uint(3)

#ifdef LIGHTMAP_BICUBIC_FILTER
uniform highp vec2 lightmap_texture_size;
#endif

#ifdef USE_SH_LIGHTMAP
uniform mediump mat3 lightmap_normal_xform;
#endif // USE_SH_LIGHTMAP
#endif // USE_LIGHTMAP

#ifdef USE_LIGHTMAP_CAPTURE
uniform mediump vec4[9] lightmap_captures;
#endif // USE_LIGHTMAP_CAPTURE
#endif // !DISABLE_LIGHTMAP

#ifdef USE_MULTIVIEW
uniform highp sampler2DArray depth_buffer; // texunit:-7
uniform highp sampler2DArray color_buffer; // texunit:-6
vec3 multiview_uv(vec2 uv) {
	return vec3(uv, ViewIndex);
}
ivec3 multiview_uv(ivec2 uv) {
	return ivec3(uv, int(ViewIndex));
}
#else
uniform highp sampler2D depth_buffer; // texunit:-7
uniform highp sampler2D color_buffer; // texunit:-6
vec2 multiview_uv(vec2 uv) {
	return uv;
}
ivec2 multiview_uv(ivec2 uv) {
	return uv;
}
#endif

uniform mediump float opaque_prepass_threshold;
#endif // !RENDER_MOTION_VECTORS

#if defined(RENDER_MATERIAL)
layout(location = 0) out vec4 albedo_output_buffer;
layout(location = 1) out vec4 normal_output_buffer;
layout(location = 2) out vec4 orm_output_buffer;
layout(location = 3) out vec4 emission_output_buffer;

#else // !RENDER_MATERIAL

#ifndef RENDER_MOTION_VECTORS
// Normal color rendering.
layout(location = 0) out vec4 frag_color;
#else
layout(location = 0) out vec4 motion_vectors;
#endif // !RENDER_MOTION_VECTORS

#endif // !RENDER_MATERIAL

/* clang-format off */



/* clang-format on */

#ifndef RENDER_MOTION_VECTORS
vec3 F0(float metallic, float specular, vec3 albedo) {
	float dielectric = 0.16 * specular * specular;
	// use albedo * metallic as colored specular reflectance at 0 angle for metallic materials;
	// see https://google.github.io/filament/Filament.md.html
	return mix(vec3(dielectric), albedo, vec3(metallic));
}
#ifndef MODE_RENDER_DEPTH

#ifndef USE_VERTEX_LIGHTING
#if !defined(DISABLE_LIGHT_DIRECTIONAL) || !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || !defined(DISABLE_LIGHT_AREA) || defined(USE_ADDITIVE_LIGHTING)

float D_GGX(float cos_theta_m, float alpha) {
	float a = cos_theta_m * alpha;
	float k = alpha / (1.0 - cos_theta_m * cos_theta_m + a * a);
	return k * k * (1.0 / M_PI);
}

// From Earl Hammon, Jr. "PBR Diffuse Lighting for GGX+Smith Microsurfaces" https://www.gdcvault.com/play/1024478/PBR-Diffuse-Lighting-for-GGX
float V_GGX(float NdotL, float NdotV, float alpha) {
	return 0.5 / mix(2.0 * NdotL * NdotV, NdotL + NdotV, alpha);
}

float D_GGX_anisotropic(float cos_theta_m, float alpha_x, float alpha_y, float cos_phi, float sin_phi) {
	float alpha2 = alpha_x * alpha_y;
	highp vec3 v = vec3(alpha_y * cos_phi, alpha_x * sin_phi, alpha2 * cos_theta_m);
	highp float v2 = dot(v, v);
	float w2 = alpha2 / v2;
	float D = alpha2 * w2 * w2 * (1.0 / M_PI);
	return D;
}

float V_GGX_anisotropic(float alpha_x, float alpha_y, float TdotV, float TdotL, float BdotV, float BdotL, float NdotV, float NdotL) {
	float Lambda_V = NdotL * length(vec3(alpha_x * TdotV, alpha_y * BdotV, NdotV));
	float Lambda_L = NdotV * length(vec3(alpha_x * TdotL, alpha_y * BdotL, NdotL));
	return 0.5 / (Lambda_V + Lambda_L);
}

float SchlickFresnel(float u) {
	float m = 1.0 - u;
	float m2 = m * m;
	return m2 * m2 * m; // pow(m,5)
}

float V_Kelemen(float LdotH) {
	// Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
	return 0.25 / (LdotH * LdotH + 1e-4);
}

void light_compute(vec3 N, vec3 L, vec3 V, float A, vec3 light_color, bool is_directional, float attenuation, vec3 f0, float roughness, float metallic, float specular_amount, vec3 albedo, inout float alpha, vec2 screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
		vec3 backlight,
#endif
#ifdef LIGHT_RIM_USED
		float rim, float rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		float clearcoat, float clearcoat_roughness, vec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		vec3 B, vec3 T, float anisotropy,
#endif
		inout vec3 diffuse_light, inout vec3 specular_light) {

#if defined(LIGHT_CODE_USED)
	// light is written by the light shader

	highp mat4 model_matrix = world_transform;
	mat4 projection_matrix = scene_data_block.data.projection_matrix;
	mat4 inv_projection_matrix = scene_data_block.data.inv_projection_matrix;

	vec3 normal = N;
	vec3 light = L;
	vec3 view = V;
	bool is_area = false;
	float area_diffuse = 1.0;
	float area_specular = 1.0;
	vec3 area_diffuse_tex_color = vec3(1.0);
	vec3 area_specular_tex_color = vec3(1.0);

	/* clang-format off */



	/* clang-format on */

#else
	float NdotL = min(A + dot(N, L), 1.0);
	float cNdotL = max(NdotL, 0.0); // clamped NdotL
	float NdotV = dot(N, V);
	float cNdotV = max(NdotV, 1e-4);

	float cc_attenuation = 1.0;

#if defined(DIFFUSE_BURLEY) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_CLEARCOAT_USED)
	vec3 H = normalize(V + L);
#endif

#if defined(SPECULAR_SCHLICK_GGX)
	float cNdotH = clamp(A + dot(N, H), 0.0, 1.0);
#endif

#if defined(DIFFUSE_BURLEY) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_CLEARCOAT_USED)
	float cLdotH = clamp(A + dot(L, H), 0.0, 1.0);
#endif

#if defined(LIGHT_CLEARCOAT_USED)
	// Clearcoat ignores normal_map, use vertex normal instead
	float ccNdotL = clamp(A + dot(vertex_normal, L), 0.0, 1.0);
	float ccNdotH = clamp(A + dot(vertex_normal, H), 0.0, 1.0);
	float cLdotH5 = SchlickFresnel(cLdotH);

	float Dr = D_GGX(ccNdotH, mix(0.001, 0.1, clearcoat_roughness));
	float Gr = V_Kelemen(cLdotH);
	float Fr = mix(0.04, 1.0, cLdotH5) * clearcoat;
	cc_attenuation = 1.0 - Fr;
	float clearcoat_specular_brdf_NL = Dr * Gr * Fr * ccNdotL;

	specular_light += clearcoat_specular_brdf_NL * light_color * attenuation * specular_amount;
#endif // LIGHT_CLEARCOAT_USED

	if (metallic < 1.0) {
		float diffuse_brdf_NL; // BRDF times N.L for calculating diffuse radiance

#if defined(DIFFUSE_LAMBERT_WRAP)
		// Energy conserving lambert wrap shader.
		// https://web.archive.org/web/20210228210901/http://blog.stevemcauley.com/2011/12/03/energy-conserving-wrapped-diffuse/
		diffuse_brdf_NL = max(0.0, (NdotL + roughness) / ((1.0 + roughness) * (1.0 + roughness))) * (1.0 / M_PI);
#elif defined(DIFFUSE_TOON)
		diffuse_brdf_NL = smoothstep(-roughness, max(roughness, 0.01), NdotL) * (1.0 / M_PI);
#elif defined(DIFFUSE_BURLEY)
		{
			float FD90_minus_1 = 2.0 * cLdotH * cLdotH * roughness - 0.5;
			float FdV = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotV);
			float FdL = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotL);
			diffuse_brdf_NL = (1.0 / M_PI) * FdV * FdL * cNdotL;
		}
#else
		// Lambert
		diffuse_brdf_NL = cNdotL * (1.0 / M_PI);
#endif

		diffuse_light += light_color * diffuse_brdf_NL * attenuation * cc_attenuation;

#if defined(LIGHT_BACKLIGHT_USED)
		diffuse_light += light_color * (vec3(1.0 / M_PI) - diffuse_brdf_NL) * backlight * attenuation * cc_attenuation;
#endif

#if defined(LIGHT_RIM_USED)
		// Epsilon min to prevent pow(0, 0) singularity which results in undefined behavior.
		float rim_light = pow(max(1e-4, 1.0 - cNdotV), max(0.0, (1.0 - roughness) * 16.0));
		diffuse_light += rim_light * rim * mix(vec3(1.0), albedo, rim_tint) * light_color;
#endif
	}

	if (roughness > 0.0) { // FIXME: roughness == 0 should not disable specular light entirely

		// D

#if defined(SPECULAR_TOON)

		vec3 R = normalize(-reflect(L, N));
		float RdotV = dot(R, V);
		float mid = 1.0 - roughness;
		mid *= mid;
		float intensity = smoothstep(mid - roughness * 0.5, mid + roughness * 0.5, RdotV) * mid;
		diffuse_light += light_color * intensity * attenuation * specular_amount; // write to diffuse_light, as in toon shading you generally want no reflection

#elif defined(SPECULAR_DISABLED)
		// none..

#elif defined(SPECULAR_SCHLICK_GGX)
		// shlick+ggx as default
		float alpha_ggx = roughness * roughness;
#if defined(LIGHT_ANISOTROPY_USED)
		float aspect = sqrt(1.0 - anisotropy * 0.9);
		float ax = alpha_ggx / aspect;
		float ay = alpha_ggx * aspect;
		float XdotH = dot(T, H);
		float YdotH = dot(B, H);
		float D = D_GGX_anisotropic(cNdotH, ax, ay, XdotH, YdotH);
		float G = V_GGX_anisotropic(ax, ay, dot(T, V), dot(T, L), dot(B, V), dot(B, L), cNdotV, cNdotL);
#else
		float D = D_GGX(cNdotH, alpha_ggx);
		float G = V_GGX(cNdotL, cNdotV, alpha_ggx);
#endif // LIGHT_ANISOTROPY_USED
	   // F
#if !defined(LIGHT_CLEARCOAT_USED)
		float cLdotH5 = SchlickFresnel(cLdotH);
#endif
		// Calculate Fresnel using cheap approximate specular occlusion term from Filament:
		// https://google.github.io/filament/Filament.html#lighting/occlusion/specularocclusion
		float f90 = clamp(50.0 * f0.g, 0.0, 1.0);
		vec3 F = f0 + (f90 - f0) * cLdotH5;

		vec3 specular_brdf_NL = cNdotL * D * F * G;

		specular_light += specular_brdf_NL * light_color * attenuation * cc_attenuation * specular_amount;
#endif
	}

#ifdef USE_SHADOW_TO_OPACITY
	alpha = min(alpha, clamp(1.0 - attenuation, 0.0, 1.0));
#endif

#endif // LIGHT_CODE_USED
}

float get_omni_spot_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(distance, 0.0001), -decay);
}

#if !defined(DISABLE_LIGHT_OMNI) || defined(ADDITIVE_OMNI)
void light_process_omni(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, vec3 f0, float roughness, float metallic, float shadow, vec3 albedo, inout float alpha, vec2 screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
		vec3 backlight,
#endif
#ifdef LIGHT_RIM_USED
		float rim, float rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		float clearcoat, float clearcoat_roughness, vec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		vec3 binormal, vec3 tangent, float anisotropy,
#endif
		inout vec3 diffuse_light, inout vec3 specular_light) {
	vec3 light_rel_vec = omni_lights[idx].position - vertex;
	float light_length = length(light_rel_vec);
	float omni_attenuation = get_omni_spot_attenuation(light_length, omni_lights[idx].inv_radius, omni_lights[idx].attenuation);
	vec3 color = omni_lights[idx].color;
	float size_A = 0.0;

	if (omni_lights[idx].size > 0.0) {
		float t = omni_lights[idx].size / max(0.001, light_length);
		size_A = max(0.0, 1.0 - 1.0 / sqrt(1.0 + t * t));
	}

	omni_attenuation *= shadow;

	light_compute(normal, normalize(light_rel_vec), eye_vec, size_A, color, false, omni_attenuation, f0, roughness, metallic, omni_lights[idx].specular_amount, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_RIM_USED
			rim * omni_attenuation, rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
			clearcoat, clearcoat_roughness, vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
			binormal, tangent, anisotropy,
#endif
			diffuse_light,
			specular_light);
}
#endif // !DISABLE_LIGHT_OMNI

#if !defined(DISABLE_LIGHT_AREA)
// COPIED FROM servers\rendering\renderer_rd\shaders\area_lights_inc.glsl
// Partially modified
// Functions related to area lights
#define M_PI 3.14159265359
#define M_TAU 6.28318530718

float acos_approx(float p_x) {
	float x = abs(p_x);
	float res = -0.156583f * x + (M_PI / 2.0);
	res *= sqrt(1.0f - x);
	return (p_x >= 0.0) ? res : M_PI - res;
}

vec3 integrate_edge_hill(vec3 p0, vec3 p1) {
	// Approximation suggested by Hill and Heitz, calculating the integral of the spherical cosine distribution over the line between p0 and p1.
	// Runs faster than the exact formula of Baum et al. (1989).
	float cosTheta = dot(p0, p1);

	float x = cosTheta;
	float y = abs(x);
	float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
	float b = 3.45068 + (4.18814 + y) * y;
	float theta_sintheta = a / b;

	if (x < 0.0) {
		theta_sintheta = M_PI * inversesqrt(1.0 - x * x) - theta_sintheta; // original paper: 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - theta_sintheta
	}
	return theta_sintheta * cross(p0, p1);
}

float integrate_edge(vec3 p_proj0, vec3 p_proj1, vec3 p0, vec3 p1) {
	float epsilon = 0.00001;
	bool opposite_sides = dot(p_proj0, p_proj1) < -1.0 + epsilon;
	if (opposite_sides) {
		// calculate the point on the line p0 to p1 that is closest to the vertex (origin)
		vec3 half_point_t = p0 + normalize(p1 - p0) * dot(p0, normalize(p0 - p1));
		vec3 half_point = normalize(half_point_t);
		return integrate_edge_hill(p_proj0, half_point).y + integrate_edge_hill(half_point, p_proj1).y;
	}
	return integrate_edge_hill(p_proj0, p_proj1).y;
}

// Form factor function for area light, taken from Urena, Fajardo, et.al. (2013): An Area-Preserving Parametrization for Spherical Rectangles
float quad_solid_angle(vec3 L[4]) {
	// The solid angle of a spherical rectangle is the difference of the sum of its angles
	// and the sum of the angles of a plane rectangle (2*PI)
	vec3 c1 = cross(L[0], L[1]);
	vec3 c2 = cross(L[1], L[2]);
	vec3 c3 = cross(L[2], L[3]);
	vec3 c4 = cross(L[3], L[0]);
	vec3 n0 = normalize(c1);
	vec3 n1 = normalize(c2);
	vec3 n2 = normalize(c3);
	vec3 n3 = normalize(c4);
	float g0 = acos(clamp(dot(-n0, n1), -1.0, 1.0));
	float g1 = acos(clamp(dot(-n1, n2), -1.0, 1.0));
	float g2 = acos(clamp(dot(-n2, n3), -1.0, 1.0));
	float g3 = acos(clamp(dot(-n3, n0), -1.0, 1.0));

	float angle_sum = g0 + g1 + g2 + g3;

	return clamp(angle_sum - M_TAU, 0.0, M_TAU);
}

void clip_quad_to_horizon(inout vec3 L[5], out int vertex_count) {
	// detect clipping config
	int config = 0;
	if (L[0].y > 0.0) {
		config += 1;
	}
	if (L[1].y > 0.0) {
		config += 2;
	}
	if (L[2].y > 0.0) {
		config += 4;
	}
	if (L[3].y > 0.0) {
		config += 8;
	}

	// clip
	vertex_count = 0;

	if (config == 0) {
		// clip all
	} else if (config == 1) { // V1 clip V2 V3 V4
		vertex_count = 3;
		L[1] = -L[1].y * L[0] + L[0].y * L[1];
		L[2] = -L[3].y * L[0] + L[0].y * L[3];
	} else if (config == 2) { // V2 clip V1 V3 V4
		vertex_count = 3;
		L[0] = -L[0].y * L[1] + L[1].y * L[0];
		L[2] = -L[2].y * L[1] + L[1].y * L[2];
	} else if (config == 3) { // V1 V2 clip V3 V4
		vertex_count = 4;
		L[2] = -L[2].y * L[1] + L[1].y * L[2];
		L[3] = -L[3].y * L[0] + L[0].y * L[3];
	} else if (config == 4) { // V3 clip V1 V2 V4
		vertex_count = 3;
		L[0] = -L[3].y * L[2] + L[2].y * L[3];
		L[1] = -L[1].y * L[2] + L[2].y * L[1];
	} else if (config == 5) { // V1 V3 clip V2 V4) impossible
		vertex_count = 0;
	} else if (config == 6) { // V2 V3 clip V1 V4
		vertex_count = 4;
		L[0] = -L[0].y * L[1] + L[1].y * L[0];
		L[3] = -L[3].y * L[2] + L[2].y * L[3];
	} else if (config == 7) { // V1 V2 V3 clip V4
		vertex_count = 5;
		L[4] = -L[3].y * L[0] + L[0].y * L[3];
		L[3] = -L[3].y * L[2] + L[2].y * L[3];
	} else if (config == 8) { // V4 clip V1 V2 V3
		vertex_count = 3;
		L[0] = -L[0].y * L[3] + L[3].y * L[0];
		L[1] = -L[2].y * L[3] + L[3].y * L[2];
		L[2] = L[3];
	} else if (config == 9) { // V1 V4 clip V2 V3
		vertex_count = 4;
		L[1] = -L[1].y * L[0] + L[0].y * L[1];
		L[2] = -L[2].y * L[3] + L[3].y * L[2];
	} else if (config == 10) { // V2 V4 clip V1 V3) impossible
		vertex_count = 0;
	} else if (config == 11) { // V1 V2 V4 clip V3
		vertex_count = 5;
		L[4] = L[3];
		L[3] = -L[2].y * L[3] + L[3].y * L[2];
		L[2] = -L[2].y * L[1] + L[1].y * L[2];
	} else if (config == 12) { // V3 V4 clip V1 V2
		vertex_count = 4;
		L[1] = -L[1].y * L[2] + L[2].y * L[1];
		L[0] = -L[0].y * L[3] + L[3].y * L[0];
	} else if (config == 13) { // V1 V3 V4 clip V2
		vertex_count = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].y * L[2] + L[2].y * L[1];
		L[1] = -L[1].y * L[0] + L[0].y * L[1];
	} else if (config == 14) { // V2 V3 V4 clip V1
		vertex_count = 5;
		L[4] = -L[0].y * L[3] + L[3].y * L[0];
		L[0] = -L[0].y * L[1] + L[1].y * L[0];
	} else if (config == 15) { // V1 V2 V3 V4
		vertex_count = 4;
	}

	if (vertex_count == 3) {
		L[3] = L[0];
	}
	if (vertex_count == 4) {
		L[4] = L[0];
	}
}

float ltc_integrate_clipped_quad(vec3 L[5], vec3 L_proj[5], int vertices_above_horizon) {
	float I;
	I = integrate_edge(L_proj[0], L_proj[1], L[0], L[1]);
	I += integrate_edge(L_proj[1], L_proj[2], L[1], L[2]);
	I += integrate_edge(L_proj[2], L_proj[3], L[2], L[3]);
	if (vertices_above_horizon >= 4) {
		I += integrate_edge(L_proj[3], L_proj[4], L[3], L[4]);
	}
	if (vertices_above_horizon == 5) {
		I += integrate_edge(L_proj[4], L_proj[0], L[4], L[0]);
	}
	return abs(I);
}

void ltc_evaluate(vec3 normal, vec3 eye_vec, mat3 M_inv, vec3 points[4], out float integral) {
	// construct the orthonormal basis around the normal vector
	vec3 x, z;
	z = -normalize(eye_vec - normal * dot(eye_vec, normal)); // expanding the angle between view and normal vector to 90 degrees, this gives a normal vector
	x = cross(normal, z);

	// rotate area light in (T1, normal, T2) basis
	M_inv = M_inv * transpose(mat3(x, normal, z));

	vec3 L[5];
	L[0] = M_inv * points[0];
	L[1] = M_inv * points[1];
	L[2] = M_inv * points[2];
	L[3] = M_inv * points[3];

	vec3 L_unclipped[4];
	L_unclipped[0] = L[0];
	L_unclipped[1] = L[1];
	L_unclipped[2] = L[2];
	L_unclipped[3] = L[3];

	int n;
	clip_quad_to_horizon(L, n);
	if (n == 0) {
		integral = 0.0;
		return;
	}

	// project onto unit sphere
	vec3 L_proj[5];
	L_proj[0] = normalize(L[0]);
	L_proj[1] = normalize(L[1]);
	L_proj[2] = normalize(L[2]);
	L_proj[3] = normalize(L[3]);
	L_proj[4] = normalize(L[4]);

	// Prevent abnormal values when the light goes through (or close to) the fragment
	vec3 pnorm = normalize(cross(L_proj[0] - L_proj[1], L_proj[2] - L_proj[1]));
	if (abs(dot(pnorm, L_proj[0])) < 1e-10) {
		// we could just return black, but that would lead to some black pixels in front of the light.
		// Better, we check if the fragment is on the light, and return white if so.
		vec3 r10 = points[0] - points[1];
		vec3 r12 = points[2] - points[1];
		float alpha = -dot(points[1], r10) / dot(r10, r10);
		float beta = -dot(points[1], r12) / dot(r12, r12);
		if (0.0 < alpha && alpha < 1.0 && 0.0 < beta && beta < 1.0) { // fragment is on light {
			integral = 1.0;
			return;
		} else {
			integral = 0.0;
			return;
		}
	}

	float I = ltc_integrate_clipped_quad(L, L_proj, n);
	integral = I / (2.0 * M_PI);
}

void ltc_evaluate_specular(vec3 normal, vec3 eye_vec, float roughness, vec3 points[4], sampler2D ltc_lut1, sampler2D ltc_lut2, out float ltc_specular, out vec2 fresnel) {
	float theta = acos_approx(dot(normal, eye_vec));
	const float LTC_LUT_SIZE = float(64.0);
	vec2 lut_pos = vec2(max(roughness, float(0.02)), theta / float(0.5 * M_PI));
	vec2 lut_uv = vec2(lut_pos * (float(63.0) / LTC_LUT_SIZE) + vec2(float(0.5) / LTC_LUT_SIZE)); // offset by 1 pixel
	vec4 M_brdf_abcd = texture(ltc_lut1, lut_uv);
	vec3 M_brdf_e_mag_fres = texture(ltc_lut2, lut_uv).xyz;
	float scale = 1.0 / (M_brdf_abcd.x * M_brdf_e_mag_fres.x - M_brdf_abcd.y * M_brdf_abcd.w);

	mat3 M_inv = mat3(
			vec3(0, 0, 1.0 / M_brdf_abcd.z),
			vec3(-M_brdf_abcd.w * scale, M_brdf_abcd.x * scale, 0),
			vec3(-M_brdf_e_mag_fres.x * scale, M_brdf_abcd.y * scale, 0));

	ltc_evaluate(normal, eye_vec, M_inv, points, ltc_specular);
	fresnel = vec2(M_brdf_e_mag_fres.yz);
}


// implementation of area lights with Linearly Transformed Cosines (LTC): https://eheitzresearch.wordpress.com/415-2/
void light_process_area(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, vec3 f0, float roughness, float metallic, float shadow, vec3 albedo, inout float alpha, vec2 screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
		vec3 backlight,
#endif
#ifdef LIGHT_RIM_USED
		float rim, float rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		float clearcoat, float clearcoat_roughness, vec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		vec3 binormal, vec3 tangent, float anisotropy,
#endif
		inout vec3 diffuse_light, inout vec3 specular_light) {
	float EPSILON = 1e-7f;
	vec3 area_width = area_lights[idx].area_width.xyz;
	vec3 area_height = area_lights[idx].area_height.xyz;
	vec3 area_direction = area_lights[idx].direction;

	if (dot(area_width, area_width) < EPSILON || dot(area_height, area_height) < EPSILON) { // area is 0
		return;
	}
	if (dot(area_direction, vertex - area_lights[idx].position) <= 0.0) {
		return; // vertex is behind light
	}

	float a_len = length(area_width);
	float b_len = length(area_height);
	float a_half_len = a_len / 2.0;
	float b_half_len = b_len / 2.0;
	vec3 light_center = area_lights[idx].position;
	vec3 light_to_vert = vertex - light_center;
	vec3 pos_local_to_light = vec3(dot(light_to_vert, normalize(area_width)), dot(light_to_vert, normalize(area_height)), dot(light_to_vert, -area_direction));

	vec3 closest_point_local_to_light = vec3(clamp(pos_local_to_light.x, -a_half_len, a_half_len), clamp(pos_local_to_light.y, -b_half_len, b_half_len), 0);
	float dist = length(closest_point_local_to_light - pos_local_to_light);

	vec3 points[4];
	vec3 h_area_width = area_width / 2.0;
	vec3 h_area_height = area_height / 2.0;
	points[0] = area_lights[idx].position - h_area_width - h_area_height - vertex;
	points[1] = area_lights[idx].position + h_area_width - h_area_height - vertex;
	points[2] = area_lights[idx].position + h_area_width + h_area_height - vertex;
	points[3] = area_lights[idx].position - h_area_width + h_area_height - vertex;

	float ltc_diffuse = 0.0;
	float ltc_specular = 0.0;
	vec2 ltc_fresnel = vec2(0.0);
	ltc_evaluate(normal, eye_vec, mat3(1), points, ltc_diffuse);
	ltc_evaluate_specular(normal, eye_vec, roughness, points, ltc_lut1, ltc_lut2, ltc_specular, ltc_fresnel);

	float f90 = clamp(dot(f0, vec3(50.0 * 0.33)), metallic, 1.0);
	vec3 fresnel_color = f0 * max(ltc_fresnel.x, 0.0) + (f90 - f0) * max(ltc_fresnel.y, 0.0);

	float light_length = max(0.0, dist);
	float light_attenuation_raw = get_omni_spot_attenuation(light_length, area_lights[idx].inv_radius, area_lights[idx].attenuation);
	float light_attenuation_ltc = light_attenuation_raw * light_length * light_length; // solid angle already decreases by inverse square, so attenuation power is 2.0 by default -> subtract 2.0

	vec3 light_color = area_lights[idx].color;
	light_attenuation_ltc = light_attenuation_ltc * shadow;
	float light_attenuation = light_attenuation_raw * shadow;
	float specular_amount = area_lights[idx].specular_amount;

#if defined(LIGHT_CODE_USED) && defined(AREA_LIGHT_CODE_USED)
	// light is written by the light shader

	highp mat4 model_matrix = world_transform;
	mat4 projection_matrix = scene_data_block.data.projection_matrix;
	mat4 inv_projection_matrix = scene_data_block.data.inv_projection_matrix;

	vec3 light = (light_center - vertex) / light_length;
	vec3 view = eye_vec;

	bool is_area = true;
	vec3 area_diffuse = vec3(ltc_diffuse);
	vec3 area_specular = ltc_specular * fresnel_color;
	float attenuation = light_attenuation_ltc;

	/* clang-format off */



	/* clang-format on */

#else
	float area = a_len * b_len;
	float cc_attenuation = 1.0;

#if defined(LIGHT_BACKLIGHT_USED) || defined(LIGHT_RIM_USED) || defined(DIFFUSE_TOON) || defined(SPECULAR_TOON)
	float solid_angle = quad_solid_angle(points);
#endif

#if defined(LIGHT_CLEARCOAT_USED)
	float cc_specular_ltc = 0.0;
	vec2 cc_fresnel = vec2(0.0);
	ltc_evaluate_specular(vertex_normal, eye_vec, sqrt(mix(0.001, 0.1, float(clearcoat_roughness))), points, ltc_lut1, ltc_lut2, cc_specular_ltc, cc_fresnel);
	float Fr = (0.04 * max(cc_fresnel.x, 0.0) + (1.0 - 0.04) * max(cc_fresnel.y, 0.0)) * clearcoat;
	cc_attenuation = 1.0 - Fr;
	specular_light += cc_specular_ltc * Fr * light_color * light_attenuation_ltc * specular_amount;
#endif // LIGHT_CLEARCOAT_USED

	if (metallic < 1.0) {
#if defined(DIFFUSE_TOON)
		float backface_ltc_diffuse = 0.0;
		ltc_evaluate(vec3(-normal), vec3(eye_vec), mat3(1), points, backface_ltc_diffuse);
		float NdotL = (ltc_diffuse - backface_ltc_diffuse) / (max(solid_angle, 0.001) / M_PI);
		float diffuse_brdf_NL = smoothstep(-roughness, max(roughness, 0.01), NdotL) * (1.0 / M_PI);
		diffuse_light += diffuse_brdf_NL * light_color * area * light_attenuation * cc_attenuation;
#else
		diffuse_light += ltc_diffuse * light_color * light_attenuation_ltc * cc_attenuation;
#endif // DIFFUSE_TOON

#if defined(LIGHT_BACKLIGHT_USED)
		diffuse_light += light_color * max((solid_angle / M_PI) - ltc_diffuse, 0.0) * backlight * light_attenuation_ltc;
#endif
	}

#if defined(LIGHT_RIM_USED) // same as for point lights
	float cNdotV = max(dot(normal, eye_vec), 1e-4);
	float rim_light = pow(max(1e-4, 1.0 - cNdotV), max(0.0, (1.0 - roughness) * 16.0));
	diffuse_light += rim_light * rim * mix(vec3(1.0), albedo, rim_tint) * light_color * solid_angle * light_attenuation_ltc;
#endif

#if defined(SPECULAR_TOON)
	// If ltc_specular turns out to be not similar enough to RdotV, since its based on GGX, toon shading would need its own lookup-table.
	float mid = 1.0 - roughness;
	mid *= mid;

	float RdotV = ltc_specular / (max(solid_angle, 0.001) / (M_PI));
	float intensity = smoothstep(mid - roughness * 0.5, mid + roughness * 0.5, RdotV) * mid; // should we use specular tex color here?? or diffuse? or white?
	diffuse_light += intensity * light_color * area * light_attenuation * specular_amount; // write to diffuse_light, as in toon shading you generally want no reflection
#elif defined(SPECULAR_DISABLED)
	// do nothing
#else
	vec3 spec = ltc_specular * light_color * fresnel_color;
	specular_light += spec * specular_amount * light_attenuation_ltc * cc_attenuation;
#endif // SPECULAR_TOON
#endif // LIGHT_CODE_USED
}
#endif // !DISABLE_LIGHT_AREA

#if !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)
void light_process_spot(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, vec3 f0, float roughness, float metallic, float shadow, vec3 albedo, inout float alpha, vec2 screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
		vec3 backlight,
#endif
#ifdef LIGHT_RIM_USED
		float rim, float rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		float clearcoat, float clearcoat_roughness, vec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		vec3 binormal, vec3 tangent, float anisotropy,
#endif
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

	vec3 color = spot_lights[idx].color;

	float size_A = 0.0;

	if (spot_lights[idx].size > 0.0) {
		float t = spot_lights[idx].size / max(0.001, light_length);
		size_A = max(0.0, 1.0 - 1.0 / sqrt(1.0 + t * t));
	}

	spot_attenuation *= shadow;

	light_compute(normal, normalize(light_rel_vec), eye_vec, size_A, color, false, spot_attenuation, f0, roughness, metallic, spot_lights[idx].specular_amount, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_RIM_USED
			rim * spot_attenuation, rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
			clearcoat, clearcoat_roughness, vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
			binormal, tangent, anisotropy,
#endif
			diffuse_light, specular_light);
}
#endif // !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)

#endif // !defined(DISABLE_LIGHT_DIRECTIONAL) || !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT)
#endif // !USE_VERTEX_LIGHTING

vec4 fog_process(vec3 vertex) {
	vec3 fog_color = scene_data_block.data.fog_light_color;

#ifdef USE_RADIANCE_MAP
/*
		if (scene_data_block.data.fog_aerial_perspective > 0.0) {
		vec3 sky_fog_color = vec3(0.0);
		vec3 cube_view = scene_data_block.data.radiance_inverse_xform * vertex;
		// mip_level always reads from the second mipmap and higher so the fog is always slightly blurred
		float mip_level = mix(1.0 / MAX_ROUGHNESS_LOD, 1.0, 1.0 - (abs(vertex.z) - scene_data_block.data.z_near) / (scene_data_block.data.z_far - scene_data_block.data.z_near));

		sky_fog_color = textureLod(radiance_map, cube_view, mip_level * RADIANCE_MAX_LOD).rgb;

		fog_color = mix(fog_color, sky_fog_color, scene_data_block.data.fog_aerial_perspective);
	}
	*/
#endif

#ifdef USE_SUN_SCATTER
	vec4 sun_scatter = vec4(0.0);
	float sun_total = 0.0;
	vec3 view = normalize(vertex);
	for (uint i = uint(0); i < scene_data_block.data.directional_light_count; i++) {
		vec3 light_color = directional_lights[i].color * directional_lights[i].energy;
		float light_amount = pow(max(dot(view, directional_lights[i].direction), 0.0), 8.0);
		fog_color += light_color * light_amount * scene_data_block.data.fog_sun_scatter;
	}
	for (uint i = uint(MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS) - uint(scene_data_block.data.directional_shadow_count); i < uint(MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS); i++) {
		vec3 light_color = directional_lights[i].color * directional_lights[i].energy;
		float light_amount = pow(max(dot(view, directional_lights[i].direction), 0.0), 8.0);
		fog_color += light_color * light_amount * scene_data_block.data.fog_sun_scatter;
	}
#endif // USE_SUN_SCATTER

	float fog_amount = 0.0;

#ifdef USE_DEPTH_FOG
	float fog_z = smoothstep(scene_data_block.data.fog_depth_begin, scene_data_block.data.fog_depth_end, length(vertex));
	fog_amount = pow(fog_z, scene_data_block.data.fog_depth_curve) * scene_data_block.data.fog_density;
#else
	fog_amount = 1.0 - exp(min(0.0, -length(vertex) * scene_data_block.data.fog_density));
#endif // USE_DEPTH_FOG

	if (abs(scene_data_block.data.fog_height_density) >= 0.0001) {
		float y = (scene_data_block.data.inv_view_matrix * vec4(vertex, 1.0)).y;

		float y_dist = y - scene_data_block.data.fog_height;

		float vfog_amount = 1.0 - exp(min(0.0, y_dist * scene_data_block.data.fog_height_density));

		fog_amount = max(vfog_amount, fog_amount);
	}

	return vec4(fog_color, fog_amount);
}

#ifndef DISABLE_REFLECTION_PROBE

#define REFLECTION_AMBIENT_DISABLED 0
#define REFLECTION_AMBIENT_ENVIRONMENT 1
#define REFLECTION_AMBIENT_COLOR 2

void reflection_process(samplerCube reflection_map,
		vec3 normal, vec3 vertex,
		mat4 local_matrix,
		bool use_box_project, vec3 box_extents, vec3 box_offset,
		bool exterior, float intensity, float blend_distance, int ref_ambient_mode, vec4 ref_ambient_color,
		float roughness, vec3 ambient, vec3 skybox,
		inout highp vec4 reflection_accum, inout highp vec4 ambient_accum) {
	vec4 reflection;

	vec3 local_pos = (local_matrix * vec4(vertex, 1.0)).xyz;

	if (any(greaterThan(abs(local_pos), box_extents))) { //out of the reflection box
		return;
	}

	float blend = 1.0;
	if (blend_distance != 0.0) {
		vec3 axis_blend_distance = min(vec3(blend_distance), box_extents);
		vec3 blend_axes = abs(local_pos) - box_extents + axis_blend_distance;
		blend_axes /= axis_blend_distance;
		blend_axes = clamp(1.0 - blend_axes, vec3(0.0), vec3(1.0));

		blend = pow(blend_axes.x * blend_axes.y * blend_axes.z, 2.0);
	}

	//reflect and make local
	vec3 ref_normal = normalize(reflect(vertex, normal));
	ref_normal = (local_matrix * vec4(ref_normal, 0.0)).xyz;

	if (use_box_project) { //box project

		vec3 nrdir = normalize(ref_normal);
		vec3 rbmax = (box_extents - local_pos) / nrdir;
		vec3 rbmin = (-box_extents - local_pos) / nrdir;

		vec3 rbminmax = mix(rbmin, rbmax, vec3(greaterThan(nrdir, vec3(0.0, 0.0, 0.0))));

		float fa = min(min(rbminmax.x, rbminmax.y), rbminmax.z);
		vec3 posonbox = local_pos + nrdir * fa;
		ref_normal = posonbox - box_offset.xyz;
	}

	reflection.rgb = srgb_to_linear(textureLod(reflection_map, ref_normal, roughness * MAX_ROUGHNESS_LOD).rgb);

	if (exterior) {
		reflection.rgb = mix(skybox, reflection.rgb, blend);
	}
	reflection.rgb *= intensity;
	reflection.a = blend;
	reflection.rgb *= blend;

	reflection_accum += reflection;

#ifndef USE_LIGHTMAP
	if (ref_ambient_mode == REFLECTION_AMBIENT_ENVIRONMENT) {
		vec4 ambient_out;
		vec3 amb_normal = (local_matrix * vec4(normal, 0.0)).xyz;

		ambient_out.rgb = srgb_to_linear(textureLod(reflection_map, amb_normal, MAX_ROUGHNESS_LOD).rgb);
		if (exterior) {
			ambient_out.rgb = mix(ambient, ambient_out.rgb, blend);
		}

		ambient_out.a = blend;
		ambient_out.rgb *= blend;
		ambient_accum += ambient_out;
	} else if (ref_ambient_mode == REFLECTION_AMBIENT_COLOR) {
		vec4 ambient_out;
		ambient_out.rgb = ref_ambient_color.rgb;
		if (exterior) {
			ambient_out.rgb = mix(ambient, ambient_out.rgb, blend);
		}

		ambient_out.a = blend;
		ambient_out.rgb *= blend;
		ambient_accum += ambient_out;
	}
#endif // USE_LIGHTMAP
}

#endif // DISABLE_REFLECTION_PROBE

#endif // !MODE_RENDER_DEPTH

#ifdef LIGHTMAP_BICUBIC_FILTER
// w0, w1, w2, and w3 are the four cubic B-spline basis functions
float w0(float a) {
	return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a) {
	return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a) {
	return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a) {
	return (1.0 / 6.0) * (a * a * a);
}

// g0 and g1 are the two amplitude functions
float g0(float a) {
	return w0(a) + w1(a);
}

float g1(float a) {
	return w2(a) + w3(a);
}

// h0 and h1 are the two offset functions
float h0(float a) {
	return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a) {
	return 1.0 + w3(a) / (w2(a) + w3(a));
}

vec4 textureArray_bicubic(sampler2DArray tex, vec3 uv, vec2 texture_size) {
	vec2 texel_size = vec2(1.0) / texture_size;

	uv.xy = uv.xy * texture_size + vec2(0.5);

	vec2 iuv = floor(uv.xy);
	vec2 fuv = fract(uv.xy);

	float g0x = g0(fuv.x);
	float g1x = g1(fuv.x);
	float h0x = h0(fuv.x);
	float h1x = h1(fuv.x);
	float h0y = h0(fuv.y);
	float h1y = h1(fuv.y);

	vec2 p0 = (vec2(iuv.x + h0x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p1 = (vec2(iuv.x + h1x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p2 = (vec2(iuv.x + h0x, iuv.y + h1y) - vec2(0.5)) * texel_size;
	vec2 p3 = (vec2(iuv.x + h1x, iuv.y + h1y) - vec2(0.5)) * texel_size;

	return (g0(fuv.y) * (g0x * texture(tex, vec3(p0, uv.z)) + g1x * texture(tex, vec3(p1, uv.z)))) +
			(g1(fuv.y) * (g0x * texture(tex, vec3(p2, uv.z)) + g1x * texture(tex, vec3(p3, uv.z))));
}
#endif //LIGHTMAP_BICUBIC_FILTER
#endif // RENDER_MOTION_VECTORS

void main() {
#ifndef RENDER_MOTION_VECTORS
	//lay out everything, whatever is unused is optimized away anyway
	vec3 vertex = vertex_interp;
#ifdef USE_MULTIVIEW
	vec3 eye_offset = multiview_data_block.data.eye_offset[ViewIndex].xyz;
	vec3 view = -normalize(vertex_interp - eye_offset);
	mat4 projection_matrix = multiview_data_block.data.projection_matrix_view[ViewIndex];
	mat4 inv_projection_matrix = multiview_data_block.data.inv_projection_matrix_view[ViewIndex];
#else
	vec3 eye_offset = vec3(0.0, 0.0, 0.0);
	vec3 view = -normalize(vertex_interp);
	mat4 projection_matrix = scene_data_block.data.projection_matrix;
	mat4 inv_projection_matrix = scene_data_block.data.inv_projection_matrix;
#endif
	highp mat4 model_matrix = world_transform;
	vec3 albedo = vec3(1.0);
	vec3 backlight = vec3(0.0);
	vec4 transmittance_color = vec4(0.0, 0.0, 0.0, 1.0);
	float transmittance_depth = 0.0;
	float transmittance_boost = 0.0;
	float metallic = 0.0;
	float specular = 0.5;
	vec3 emission = vec3(0.0);
	float roughness = 1.0;
	float rim = 0.0;
	float rim_tint = 0.0;
	float clearcoat = 0.0;
	float clearcoat_roughness = 0.0;
	float anisotropy = 0.0;
	vec2 anisotropy_flow = vec2(1.0, 0.0);
#ifdef PREMUL_ALPHA_USED
	float premul_alpha = 1.0;
#endif // PREMUL_ALPHA_USED
	vec4 fog = vec4(0.0);
#if defined(CUSTOM_RADIANCE_USED)
	vec4 custom_radiance = vec4(0.0);
#endif
#if defined(CUSTOM_IRRADIANCE_USED)
	vec4 custom_irradiance = vec4(0.0);
#endif

	float ao = 1.0;
	float ao_light_affect = 0.0;

	float alpha = 1.0;

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
	vec3 binormal = binormal_interp;
	vec3 tangent = tangent_interp;
#else
	vec3 binormal = vec3(0.0);
	vec3 tangent = vec3(0.0);
#endif

#ifdef NORMAL_USED
	vec3 normal = normal_interp;
#if defined(DO_SIDE_CHECK)
	if (!gl_FrontFacing) {
		normal = -normal;
	}
#endif // DO_SIDE_CHECK
#endif // NORMAL_USED

#ifdef UV_USED
	vec2 uv = uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
	vec2 uv2 = uv2_interp;
#endif

#if defined(COLOR_USED)
	vec4 color = color_interp;
#endif

#if defined(NORMAL_MAP_USED)

	vec3 normal_map = vec3(0.5);
#endif

#if defined(BENT_NORMAL_MAP_USED)
	vec3 bent_normal_vector;
	vec3 bent_normal_map = vec3(0.5);
#endif

	float normal_map_depth = 1.0;

	vec2 screen_uv = gl_FragCoord.xy * scene_data_block.data.screen_pixel_size;

	float sss_strength = 0.0;

#ifdef ALPHA_SCISSOR_USED
	float alpha_scissor_threshold = 1.0;
#endif // ALPHA_SCISSOR_USED

#ifdef ALPHA_HASH_USED
	float alpha_hash_scale = 1.0;
#endif // ALPHA_HASH_USED

#ifdef ALPHA_ANTIALIASING_EDGE_USED
	float alpha_antialiasing_edge = 0.0;
	vec2 alpha_texture_coordinate = vec2(0.0, 0.0);
#endif // ALPHA_ANTIALIASING_EDGE_USED

#ifdef LIGHT_VERTEX_USED
	vec3 light_vertex = vertex;
#endif //LIGHT_VERTEX_USED

	highp mat3 model_normal_matrix;
	if (bool(model_flags & uint(FLAGS_NON_UNIFORM_SCALE))) {
		model_normal_matrix = transpose(inverse(mat3(model_matrix)));
	} else {
		model_normal_matrix = mat3(model_matrix);
	}

	{
albedo=abs((inverse(model_matrix)*vec4(vertex,1.0)).xyz);
	}

	// Keep albedo values in positive number range as negative values "wraparound" into positive numbers resulting in wrong colors
	albedo = max(albedo, vec3(0.0));

#ifdef LIGHT_VERTEX_USED
	vertex = light_vertex;
#ifdef USE_MULTIVIEW
	view = -normalize(vertex - eye_offset);
#else
	view = -normalize(vertex);
#endif //USE_MULTIVIEW
#endif //LIGHT_VERTEX_USED

#ifdef NORMAL_USED
	vec3 geo_normal = normalize(normal);
#endif // NORMAL_USED

#ifndef USE_SHADOW_TO_OPACITY

#if defined(ALPHA_SCISSOR_USED)
#ifdef RENDER_MATERIAL
	if (alpha < alpha_scissor_threshold) {
		alpha = 0.0;
	} else {
		alpha = 1.0;
	}
#else
	if (alpha < alpha_scissor_threshold) {
		discard;
	}
	alpha = 1.0;
#endif // RENDER_MATERIAL
#else
#ifdef MODE_RENDER_DEPTH
#ifdef USE_OPAQUE_PREPASS

	if (alpha < opaque_prepass_threshold) {
		discard;
	}
#endif // USE_OPAQUE_PREPASS
#endif // MODE_RENDER_DEPTH
#endif // !ALPHA_SCISSOR_USED

#endif // !USE_SHADOW_TO_OPACITY

#ifdef BENT_NORMAL_MAP_USED
	bent_normal_map.xy = bent_normal_map.xy * 2.0 - 1.0;
	bent_normal_map.z = sqrt(max(0.0, 1.0 - dot(bent_normal_map.xy, bent_normal_map.xy)));

	bent_normal_vector = normalize(tangent * bent_normal_map.x + binormal * bent_normal_map.y + normal * bent_normal_map.z);
#endif

#if defined(NORMAL_MAP_USED)
	normal_map.xy = normal_map.xy * 2.0 - 1.0;
	normal_map.z = sqrt(max(0.0, 1.0 - dot(normal_map.xy, normal_map.xy))); //always ignore Z, as it can be RG packed, Z may be pos/neg, etc.

	// Tangent-space transformation is performed using unnormalized TBN vectors, per MikkTSpace.
	// See: http://www.mikktspace.com/
	normal = normalize(mix(normal, tangent * normal_map.x + binormal * normal_map.y + normal * normal_map.z, normal_map_depth));
#elif defined(NORMAL_USED)
	normal = geo_normal;
#endif // NORMAL_MAP_USED

#ifdef LIGHT_ANISOTROPY_USED

	if (anisotropy > 0.01) {
		mat3 rot = mat3(normalize(tangent), normalize(binormal), normal);
		// Make local to space.
		tangent = normalize(rot * vec3(anisotropy_flow.x, anisotropy_flow.y, 0.0));
		binormal = normalize(rot * vec3(-anisotropy_flow.y, anisotropy_flow.x, 0.0));
	}

#endif

#ifndef MODE_RENDER_DEPTH

#ifndef FOG_DISABLED
#ifndef CUSTOM_FOG_USED
#ifndef DISABLE_FOG
	// fog must be processed as early as possible and then packed.
	// to maximize VGPR usage

	if (scene_data_block.data.fog_enabled) {
		fog = fog_process(vertex);
	}
#endif // !DISABLE_FOG
#endif // !CUSTOM_FOG_USED

	uint fog_rg = packHalf2x16(fog.rg);
	uint fog_ba = packHalf2x16(fog.ba);
#endif // !FOG_DISABLED

	// Convert colors to linear
	albedo = srgb_to_linear(albedo);
	emission = srgb_to_linear(emission);
	// TODO Backlight and transmittance when used
#ifndef MODE_UNSHADED
	vec3 f0 = F0(metallic, specular, albedo);
	vec3 specular_light = vec3(0.0, 0.0, 0.0);
	vec3 diffuse_light = vec3(0.0, 0.0, 0.0);
	vec3 ambient_light = vec3(0.0, 0.0, 0.0);

#ifdef BASE_PASS
	/////////////////////// LIGHTING //////////////////////////////

#ifndef AMBIENT_LIGHT_DISABLED
#ifdef BENT_NORMAL_MAP_USED
	vec3 indirect_normal = bent_normal_vector;
#else
	vec3 indirect_normal = normal;
#endif // BENT_NORMAL_MAP_USED
	// IBL precalculations
	float ndotv = clamp(dot(indirect_normal, view), 0.0, 1.0);
	vec3 F = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - ndotv, 5.0);

#ifdef USE_RADIANCE_MAP
	if (scene_data_block.data.use_reflection_cubemap) {
#ifdef LIGHT_ANISOTROPY_USED
		// https://google.github.io/filament/Filament.html#lighting/imagebasedlights/anisotropy
		vec3 anisotropic_direction = anisotropy >= 0.0 ? binormal : tangent;
		vec3 anisotropic_tangent = cross(anisotropic_direction, view);
		vec3 anisotropic_normal = cross(anisotropic_tangent, anisotropic_direction);
		vec3 bent_normal = normalize(mix(indirect_normal, anisotropic_normal, abs(anisotropy) * clamp(5.0 * roughness, 0.0, 1.0)));
		vec3 ref_vec = reflect(-view, bent_normal);
#else
		vec3 ref_vec = reflect(-view, indirect_normal);
#endif
		ref_vec = mix(ref_vec, indirect_normal, roughness * roughness);
		float horizon = min(1.0 + dot(ref_vec, indirect_normal), 1.0);
		ref_vec = mat3(scene_data_block.data.radiance_inverse_xform) * ref_vec;
		specular_light = textureLod(radiance_map, ref_vec, sqrt(roughness) * RADIANCE_MAX_LOD).rgb;
		specular_light = srgb_to_linear(specular_light);
		specular_light *= horizon * horizon;
		specular_light *= scene_data_block.data.ambient_light_color_energy.a;
	}
#endif // USE_RADIANCE_MAP

	// Calculate Reflection probes
#ifndef DISABLE_REFLECTION_PROBE
	vec4 ambient_accum = vec4(0.0);
	{
		vec4 reflection_accum = vec4(0.0);

		reflection_process(refprobe1_texture, indirect_normal, vertex_interp, refprobe1_local_matrix,
				refprobe1_use_box_project, refprobe1_box_extents, refprobe1_box_offset,
				refprobe1_exterior, refprobe1_intensity, refprobe1_blend_distance, refprobe1_ambient_mode, refprobe1_ambient_color,
				roughness, ambient_light, specular_light, reflection_accum, ambient_accum);

#ifdef SECOND_REFLECTION_PROBE

		reflection_process(refprobe2_texture, indirect_normal, vertex_interp, refprobe2_local_matrix,
				refprobe2_use_box_project, refprobe2_box_extents, refprobe2_box_offset,
				refprobe2_exterior, refprobe2_intensity, refprobe2_blend_distance, refprobe2_ambient_mode, refprobe2_ambient_color,
				roughness, ambient_light, specular_light, reflection_accum, ambient_accum);

#endif // SECOND_REFLECTION_PROBE

		if (reflection_accum.a > 0.0) {
			specular_light = reflection_accum.rgb / reflection_accum.a;
		}
	}
#endif // DISABLE_REFLECTION_PROBE

#if defined(CUSTOM_RADIANCE_USED)
	specular_light = mix(specular_light, custom_radiance.rgb, custom_radiance.a);
#endif // CUSTOM_RADIANCE_USED

#if !defined(USE_LIGHTMAP) && !defined(USE_LIGHTMAP_CAPTURE)
	//lightmap overrides everything
	if (scene_data_block.data.use_ambient_light) {
		ambient_light = scene_data_block.data.ambient_light_color_energy.rgb;

#ifdef USE_RADIANCE_MAP
		if (scene_data_block.data.use_ambient_cubemap) {
			vec3 ambient_dir = mat3(scene_data_block.data.radiance_inverse_xform) * indirect_normal;
			vec3 cubemap_ambient = textureLod(radiance_map, ambient_dir, RADIANCE_MAX_LOD).rgb;
			cubemap_ambient = srgb_to_linear(cubemap_ambient);
			ambient_light = mix(ambient_light, cubemap_ambient * scene_data_block.data.ambient_light_color_energy.a, scene_data_block.data.ambient_color_sky_mix);
		}
#endif // USE_RADIANCE_MAP

#ifndef DISABLE_REFLECTION_PROBE
		if (ambient_accum.a > 0.0) {
			ambient_light = mix(ambient_light, (ambient_accum.rgb / ambient_accum.a) * scene_data_block.data.ambient_light_color_energy.a, scene_data_block.data.ambient_color_sky_mix);
		}
#endif // DISABLE_REFLECTION_PROBE
	}
#endif // !USE_LIGHTMAP && !USE_LIGHTMAP_CAPTURE

#if defined(CUSTOM_IRRADIANCE_USED)
	ambient_light = mix(ambient_light, custom_irradiance.rgb, custom_irradiance.a);
#endif // CUSTOM_IRRADIANCE_USED

#ifndef DISABLE_LIGHTMAP
#ifdef USE_LIGHTMAP_CAPTURE
	{
		// The world normal.
		vec3 wnormal = mat3(scene_data_block.data.inv_view_matrix) * indirect_normal;

		// The SH coefficients used for evaluating diffuse data from SH probes.
		const float c0 = 0.886227; // l0				sqrt(1.0/(4.0*PI)) 	* PI
		const float c1 = 1.023327; // l1				sqrt(3.0/(4.0*PI)) 	* PI*2.0/3.0
		const float c2 = 0.858086; // l2n2, l2n1, l2p1	sqrt(15.0/(4.0*PI)) * PI*1.0/4.0
		const float c3 = 0.247708; // l20				sqrt(5.0/(16.0*PI)) * PI*1.0/4.0
		const float c4 = 0.429043; // l2p2				sqrt(15.0/(16.0*PI))* PI*1.0/4.0

		ambient_light += (c0 * lightmap_captures[0].rgb +
								 c1 * lightmap_captures[1].rgb * wnormal.y +
								 c1 * lightmap_captures[2].rgb * wnormal.z +
								 c1 * lightmap_captures[3].rgb * wnormal.x +
								 c2 * lightmap_captures[4].rgb * wnormal.x * wnormal.y +
								 c2 * lightmap_captures[5].rgb * wnormal.y * wnormal.z +
								 c3 * lightmap_captures[6].rgb * (3.0 * wnormal.z * wnormal.z - 1.0) +
								 c2 * lightmap_captures[7].rgb * wnormal.x * wnormal.z +
								 c4 * lightmap_captures[8].rgb * (wnormal.x * wnormal.x - wnormal.y * wnormal.y)) *
				scene_data_block.data.IBL_exposure_normalization;
	}
#else
#ifdef USE_LIGHTMAP
	{
		vec3 uvw;
		uvw.xy = uv2 * lightmap_uv_scale.zw + lightmap_uv_scale.xy;
		uvw.z = float(lightmap_slice);

#ifdef USE_SH_LIGHTMAP
		uvw.z *= 4.0; // SH textures use 4 times more data.

#ifdef LIGHTMAP_BICUBIC_FILTER
		vec3 lm_light_l0 = textureArray_bicubic(lightmap_textures, uvw + vec3(0.0, 0.0, 0.0), lightmap_texture_size).rgb;
		vec3 lm_light_l1n1 = (textureArray_bicubic(lightmap_textures, uvw + vec3(0.0, 0.0, 1.0), lightmap_texture_size).rgb - vec3(0.5)) * 2.0;
		vec3 lm_light_l1_0 = (textureArray_bicubic(lightmap_textures, uvw + vec3(0.0, 0.0, 2.0), lightmap_texture_size).rgb - vec3(0.5)) * 2.0;
		vec3 lm_light_l1p1 = (textureArray_bicubic(lightmap_textures, uvw + vec3(0.0, 0.0, 3.0), lightmap_texture_size).rgb - vec3(0.5)) * 2.0;
#else
		vec3 lm_light_l0 = textureLod(lightmap_textures, uvw + vec3(0.0, 0.0, 0.0), 0.0).rgb;
		vec3 lm_light_l1n1 = (textureLod(lightmap_textures, uvw + vec3(0.0, 0.0, 1.0), 0.0).rgb - vec3(0.5)) * 2.0;
		vec3 lm_light_l1_0 = (textureLod(lightmap_textures, uvw + vec3(0.0, 0.0, 2.0), 0.0).rgb - vec3(0.5)) * 2.0;
		vec3 lm_light_l1p1 = (textureLod(lightmap_textures, uvw + vec3(0.0, 0.0, 3.0), 0.0).rgb - vec3(0.5)) * 2.0;
#endif

		vec3 n = normalize(lightmap_normal_xform * indirect_normal);

		ambient_light += lm_light_l0 * lightmap_exposure_normalization;
		ambient_light += lm_light_l1n1 * n.y * (lm_light_l0 * lightmap_exposure_normalization * 4.0);
		ambient_light += lm_light_l1_0 * n.z * (lm_light_l0 * lightmap_exposure_normalization * 4.0);
		ambient_light += lm_light_l1p1 * n.x * (lm_light_l0 * lightmap_exposure_normalization * 4.0);
#else
#ifdef LIGHTMAP_BICUBIC_FILTER
		ambient_light += textureArray_bicubic(lightmap_textures, uvw, lightmap_texture_size).rgb * lightmap_exposure_normalization;
#else
		ambient_light += textureLod(lightmap_textures, uvw, 0.0).rgb * lightmap_exposure_normalization;
#endif
#endif
	}
#endif // USE_LIGHTMAP
#endif // USE_LIGHTMAP_CAPTURE
#endif // !DISABLE_LIGHTMAP

	ambient_light *= ao;
#ifndef SPECULAR_OCCLUSION_DISABLED
#ifdef BENT_NORMAL_MAP_USED
	float cos_b = max(dot(reflect(-view, normal), bent_normal_vector), 0.0);
	float specular_occlusion = clamp((ao - (1.0 - cos_b)) / roughness, 0.0, 1.0);
	specular_occlusion = mix(specular_occlusion, cos_b * (1.0 - ao), roughness);
	specular_light *= specular_occlusion;
#else // BENT_NORMAL_MAP_USED
	float specular_occlusion = (ambient_light.r * 0.3 + ambient_light.g * 0.59 + ambient_light.b * 0.11) * 2.0; // Luminance of ambient light.
	specular_occlusion = min(specular_occlusion * 4.0, 1.0); // This multiplication preserves speculars on bright areas.

	float reflective_f = (1.0 - roughness) * metallic;
	// 10.0 is a magic number, it gives the intended effect in most scenarios.
	// Low enough for occlusion, high enough for reaction to lights and shadows.
	specular_occlusion = max(min(reflective_f * specular_occlusion * 10.0, 1.0), specular_occlusion);
	specular_light *= specular_occlusion;
#endif // BENT_NORMAL_MAP_USED
#endif // !SPECULAR_OCCLUSION_DISABLED
	ambient_light *= albedo.rgb;

#endif // !AMBIENT_LIGHT_DISABLED

	// convert ao to direct light ao
	ao = mix(1.0, ao, ao_light_affect);
#ifndef AMBIENT_LIGHT_DISABLED
	{
#if defined(DIFFUSE_TOON)
		//simplify for toon, as
		specular_light *= specular * metallic * albedo * 2.0;
#else

		// scales the specular reflections, needs to be be computed before lighting happens,
		// but after environment, GI, and reflection probes are added
		// Environment brdf approximation (Lazarov 2013)
		// see https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
		const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
		const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
		vec4 r = roughness * c0 + c1;
		float ndotv = clamp(dot(indirect_normal, view), 0.0, 1.0);

		float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
		vec2 env = vec2(-1.04, 1.04) * a004 + r.zw;
		specular_light *= env.x * f0 + env.y * clamp(50.0 * f0.g, metallic, 1.0);
#endif
	}
#endif // !AMBIENT_LIGHT_DISABLED

#ifdef USE_VERTEX_LIGHTING
	specular_light += specular_light_interp * f0;
	diffuse_light += diffuse_light_interp;
#else

#ifndef DISABLE_LIGHT_DIRECTIONAL
	for (uint i = uint(0); i < scene_data_block.data.directional_light_count; i++) {
		if (!bool(directional_lights[i].mask & layer_mask)) {
			continue;
		}
#if defined(USE_LIGHTMAP) && !defined(DISABLE_LIGHTMAP)
		if (bool(directional_lights[i].enabled_bake_mode & DIRECTIONAL_LIGHT_BAKE_STATIC)) {
			continue;
		}
#endif
		light_compute(normal, normalize(directional_lights[i].direction), normalize(view), directional_lights[i].size, directional_lights[i].color * directional_lights[i].energy, true, 1.0, f0, roughness, metallic, directional_lights[i].specular, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
#ifdef LIGHT_RIM_USED
				rim, rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
				clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
				binormal,
				tangent, anisotropy,
#endif
				diffuse_light,
				specular_light);
	}
#endif // !DISABLE_LIGHT_DIRECTIONAL

#ifndef DISABLE_LIGHT_OMNI
	for (uint i = 0u; i < MAX_FORWARD_LIGHTS; i++) {
		if (i >= omni_light_count) {
			break;
		}

		light_process_omni(omni_light_indices[i], vertex, view, normal, f0, roughness, metallic, 1.0, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
#ifdef LIGHT_RIM_USED
				rim,
				rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
				clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
				binormal, tangent, anisotropy,
#endif
				diffuse_light, specular_light);
	}
#endif // !DISABLE_LIGHT_OMNI

#ifndef DISABLE_LIGHT_SPOT
	for (uint i = 0u; i < MAX_FORWARD_LIGHTS; i++) {
		if (i >= spot_light_count) {
			break;
		}

		light_process_spot(spot_light_indices[i], vertex, view, normal, f0, roughness, metallic, 1.0, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
#ifdef LIGHT_RIM_USED
				rim,
				rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
				clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
				tangent,
				binormal, anisotropy,
#endif
				diffuse_light, specular_light);
	}
#endif // !DISABLE_LIGHT_SPOT

#ifndef DISABLE_LIGHT_AREA
	for (uint i = 0u; i < MAX_FORWARD_LIGHTS; i++) {
		if (i >= area_light_count) {
			break;
		}

		light_process_area(area_light_indices[i], vertex, view, normal, f0, roughness, metallic, 1.0, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
#ifdef LIGHT_RIM_USED
				rim,
				rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
				clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
				tangent,
				binormal, anisotropy,
#endif
				diffuse_light, specular_light);
	}
#endif // !DISABLE_LIGHT_AREA

#endif // !USE_VERTEX_LIGHTING
#endif // BASE_PASS
#endif // !MODE_UNSHADED

#endif // !MODE_RENDER_DEPTH

#if defined(USE_SHADOW_TO_OPACITY)
#ifndef MODE_RENDER_DEPTH
#ifndef MODE_UNSHADED
	alpha = min(alpha, clamp(length(ambient_light), 0.0, 1.0));
#else
	alpha = 0.0;
#endif

#if defined(ALPHA_SCISSOR_USED)
#ifdef RENDER_MATERIAL
	if (alpha < alpha_scissor_threshold) {
		alpha = 0.0;
	} else {
		alpha = 1.0;
	}
#else
	if (alpha < alpha_scissor_threshold) {
		discard;
	}
#endif // RENDER_MATERIAL
#endif // !ALPHA_SCISSOR_USED

#endif // !MODE_RENDER_DEPTH
#endif // USE_SHADOW_TO_OPACITY

#ifdef MODE_RENDER_DEPTH
#ifdef RENDER_SHADOWS_LINEAR
	// Linearize the depth buffer if rendering cubemap shadows.
	gl_FragDepth = (scene_data_block.data.z_far - (length(vertex) + scene_data_block.data.shadow_bias)) / scene_data_block.data.z_far;
#endif

// Nothing happens, so a tree-ssa optimizer will result in no fragment shader :)
#else // !MODE_RENDER_DEPTH

#ifdef RENDER_MATERIAL

	albedo_output_buffer.rgb = albedo;
	albedo_output_buffer.a = alpha;

	normal_output_buffer.rgb = normal * 0.5 + 0.5;
	normal_output_buffer.a = 0.0;

	orm_output_buffer.r = ao;
	orm_output_buffer.g = roughness;
	orm_output_buffer.b = metallic;
	orm_output_buffer.a = 1.0;

	emission_output_buffer.rgb = emission;
	emission_output_buffer.a = 0.0;
#else // !RENDER_MATERIAL
#ifdef BASE_PASS
#ifdef MODE_UNSHADED
	frag_color = vec4(albedo, alpha);
#else

	diffuse_light *= albedo;
	diffuse_light *= 1.0 - metallic;
	ambient_light *= 1.0 - metallic;

	frag_color = vec4(diffuse_light + specular_light, alpha);
	frag_color.rgb += emission + ambient_light;
#endif //!MODE_UNSHADED

#ifndef FOG_DISABLED
	fog.xy = unpackHalf2x16(fog_rg);
	fog.zw = unpackHalf2x16(fog_ba);

	frag_color.rgb = mix(frag_color.rgb, fog.rgb, fog.a);
#endif // !FOG_DISABLED

	// Tonemap before writing as we are writing to an sRGB framebuffer
	frag_color.rgb *= exposure;
#ifdef APPLY_TONEMAPPING
	frag_color.rgb = apply_tonemapping(frag_color.rgb);
#endif
	frag_color.rgb = linear_to_srgb(frag_color.rgb);

#else // !BASE_PASS
	frag_color = vec4(0.0, 0.0, 0.0, alpha);
#endif // !BASE_PASS

/* ADDITIVE LIGHTING PASS */
#ifdef USE_ADDITIVE_LIGHTING
	diffuse_light = vec3(0.0);
	specular_light = vec3(0.0);

#ifdef USE_VERTEX_LIGHTING
	diffuse_light = additive_diffuse_light_interp;
	specular_light = additive_specular_light_interp * f0;
#endif // USE_VERTEX_LIGHTING

#if !defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT)

#ifndef SHADOWS_DISABLED
// Baked shadowmasks
#ifdef USE_LIGHTMAP
	float shadowmask = 1.0f;

	if (lightmap_shadowmask_mode != SHADOWMASK_MODE_NONE) {
		vec3 uvw;
		uvw.xy = uv2 * lightmap_uv_scale.zw + lightmap_uv_scale.xy;
		uvw.z = float(lightmap_slice);

#ifdef LIGHTMAP_BICUBIC_FILTER
		shadowmask = textureArray_bicubic(shadowmask_textures, uvw, lightmap_texture_size).x;
#else
		shadowmask = textureLod(shadowmask_textures, uvw, 0.0).x;
#endif
	}
#endif //USE_LIGHTMAP

	float directional_shadow = 1.0;

#ifdef USE_LIGHTMAP
	if (lightmap_shadowmask_mode != SHADOWMASK_MODE_ONLY) {
#endif

// Orthogonal shadows
#if !defined(LIGHT_USE_PSSM2) && !defined(LIGHT_USE_PSSM4)
		directional_shadow = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord);
#endif // !defined(LIGHT_USE_PSSM2) && !defined(LIGHT_USE_PSSM4)

// PSSM2 shadows
#ifdef LIGHT_USE_PSSM2
		float depth_z = -vertex.z;
		vec4 light_split_offsets = directional_shadows[directional_shadow_index].shadow_split_offsets;
		//take advantage of prefetch
		float shadow1 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord);
		float shadow2 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord2);

		if (depth_z < light_split_offsets.y) {

#ifdef LIGHT_USE_PSSM_BLEND
			float directional_shadow2 = 1.0;
			float pssm_blend = 0.0;
			bool use_blend = true;
#endif
			if (depth_z < light_split_offsets.x) {
				directional_shadow = shadow1;

#ifdef LIGHT_USE_PSSM_BLEND
				directional_shadow2 = shadow2;
				pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
			} else {
				directional_shadow = shadow2;
#ifdef LIGHT_USE_PSSM_BLEND
				use_blend = false;
#endif
			}
#ifdef LIGHT_USE_PSSM_BLEND
			if (use_blend) {
				directional_shadow = mix(directional_shadow, directional_shadow2, pssm_blend);
			}
#endif
		}

#endif //LIGHT_USE_PSSM2
// PSSM4 shadows
#ifdef LIGHT_USE_PSSM4
		float depth_z = -vertex.z;
		vec4 light_split_offsets = directional_shadows[directional_shadow_index].shadow_split_offsets;

		float shadow1 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord);
		float shadow2 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord2);
		float shadow3 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord3);
		float shadow4 = sample_shadow(directional_shadow_atlas, directional_shadows[directional_shadow_index].shadow_atlas_pixel_size, shadow_coord4);

		if (depth_z < light_split_offsets.w) {
#ifdef LIGHT_USE_PSSM_BLEND
			float directional_shadow2 = 1.0;
			float pssm_blend = 0.0;
			bool use_blend = true;
#endif
			if (depth_z < light_split_offsets.y) {
				if (depth_z < light_split_offsets.x) {
					directional_shadow = shadow1;

#ifdef LIGHT_USE_PSSM_BLEND
					directional_shadow2 = shadow2;

					pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
				} else {
					directional_shadow = shadow2;

#ifdef LIGHT_USE_PSSM_BLEND
					directional_shadow2 = shadow3;

					pssm_blend = smoothstep(light_split_offsets.x, light_split_offsets.y, depth_z);
#endif
				}
			} else {
				if (depth_z < light_split_offsets.z) {
					directional_shadow = shadow3;

#if defined(LIGHT_USE_PSSM_BLEND)
					directional_shadow2 = shadow4;
					pssm_blend = smoothstep(light_split_offsets.y, light_split_offsets.z, depth_z);
#endif

				} else {
					directional_shadow = shadow4;

#if defined(LIGHT_USE_PSSM_BLEND)
					use_blend = false;
#endif
				}
			}
#if defined(LIGHT_USE_PSSM_BLEND)
			if (use_blend) {
				directional_shadow = mix(directional_shadow, directional_shadow2, pssm_blend);
			}
#endif
		}

#endif //LIGHT_USE_PSSM4

#ifdef USE_LIGHTMAP
		if (lightmap_shadowmask_mode == SHADOWMASK_MODE_REPLACE) {
			directional_shadow = mix(directional_shadow, shadowmask, smoothstep(directional_shadows[directional_shadow_index].fade_from, directional_shadows[directional_shadow_index].fade_to, vertex.z));
		} else if (lightmap_shadowmask_mode == SHADOWMASK_MODE_OVERLAY) {
			directional_shadow = shadowmask * mix(directional_shadow, 1.0, smoothstep(directional_shadows[directional_shadow_index].fade_from, directional_shadows[directional_shadow_index].fade_to, vertex.z));
		} else {
#endif
			directional_shadow = mix(directional_shadow, 1.0, smoothstep(directional_shadows[directional_shadow_index].fade_from, directional_shadows[directional_shadow_index].fade_to, vertex.z));
#ifdef USE_LIGHTMAP
		}

	} else { // lightmap_shadowmask_mode == SHADOWMASK_MODE_ONLY
		directional_shadow = shadowmask;
	}
#endif

	directional_shadow = mix(1.0, directional_shadow, directional_lights[directional_shadow_index].shadow_opacity);

#else
	float directional_shadow = 1.0f;
#endif // SHADOWS_DISABLED

#ifndef USE_VERTEX_LIGHTING
	if (bool(directional_lights[directional_shadow_index].mask & layer_mask)) {
		light_compute(normal, normalize(directional_lights[directional_shadow_index].direction), normalize(view), directional_lights[directional_shadow_index].size, directional_lights[directional_shadow_index].color * directional_lights[directional_shadow_index].energy, true, directional_shadow, f0, roughness, metallic, directional_lights[directional_shadow_index].specular, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
#ifdef LIGHT_RIM_USED
				rim, rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
				clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
				binormal,
				tangent, anisotropy,
#endif
				diffuse_light,
				specular_light);
	} else {
#endif // !USE_VERTEX_LIGHTING
	   // Just apply shadows to vertex lighting.
		diffuse_light *= directional_shadow;
		specular_light *= directional_shadow;
#ifndef USE_VERTEX_LIGHTING
	}
#endif // !USE_VERTEX_LIGHTING
#endif // !defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT)

#ifdef ADDITIVE_OMNI
	float omni_shadow = 1.0f;
#ifndef SHADOWS_DISABLED
	vec3 light_ray = ((positional_shadows[positional_shadow_index].shadow_matrix * vec4(shadow_coord.xyz, 1.0))).xyz;
	omni_shadow = texture(omni_shadow_texture, vec4(light_ray, 1.0 - length(light_ray) * omni_lights[omni_light_index].inv_radius));
	omni_shadow = mix(1.0, omni_shadow, omni_lights[omni_light_index].shadow_opacity);
#endif // SHADOWS_DISABLED

#ifndef USE_VERTEX_LIGHTING
	light_process_omni(omni_light_index, vertex, view, normal, f0, roughness, metallic, omni_shadow, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_RIM_USED
			rim,
			rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
			clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
			binormal, tangent, anisotropy,
#endif
			diffuse_light, specular_light);
#else
	// Just apply shadows to vertex lighting.
	diffuse_light *= omni_shadow;
	specular_light *= omni_shadow;
#endif // !USE_VERTEX_LIGHTING
#endif // ADDITIVE_OMNI

#ifdef ADDITIVE_SPOT
	float spot_shadow = 1.0f;
#ifndef SHADOWS_DISABLED
	spot_shadow = sample_shadow(spot_shadow_texture, positional_shadows[positional_shadow_index].shadow_atlas_pixel_size, shadow_coord);
	spot_shadow = mix(1.0, spot_shadow, spot_lights[spot_light_index].shadow_opacity);
#endif // SHADOWS_DISABLED

#ifndef USE_VERTEX_LIGHTING
	light_process_spot(spot_light_index, vertex, view, normal, f0, roughness, metallic, spot_shadow, albedo, alpha, screen_uv,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_RIM_USED
			rim,
			rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
			clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_RIM_USED
#ifdef LIGHT_ANISOTROPY_USED
			tangent,
			binormal, anisotropy,
#endif
			diffuse_light, specular_light);
#else
	// Just apply shadows to vertex lighting.
	diffuse_light *= spot_shadow;
	specular_light *= spot_shadow;
#endif // !USE_VERTEX_LIGHTING

#endif // ADDITIVE_SPOT

	diffuse_light *= albedo;
	diffuse_light *= 1.0 - metallic;
	vec3 additive_light_color = diffuse_light + specular_light;

#ifndef FOG_DISABLED
	fog.xy = unpackHalf2x16(fog_rg);
	fog.zw = unpackHalf2x16(fog_ba);

	additive_light_color *= (1.0 - fog.a);
#endif // !FOG_DISABLED

	// Tonemap before writing as we are writing to an sRGB framebuffer
	additive_light_color *= exposure;
#ifdef APPLY_TONEMAPPING
	additive_light_color = apply_tonemapping(additive_light_color);
#endif
	additive_light_color = linear_to_srgb(additive_light_color);

	frag_color.rgb += additive_light_color;
#endif // USE_ADDITIVE_LIGHTING
	frag_color.rgb *= scene_data_block.data.luminance_multiplier;

#endif // !RENDER_MATERIAL
#endif // !MODE_RENDER_DEPTH

#ifdef PREMUL_ALPHA_USED
	frag_color.rgb *= premul_alpha;
#endif // PREMUL_ALPHA_USED
#endif // !RENDER_MOTION_VECTORS

#if defined(RENDER_MOTION_VECTORS)
	// These motion vectors are in NDC space (as opposed to screen space) to fit the OpenXR XR_FB_space_warp specification.
	// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_space_warp

	vec3 ndc = clip_position.xyz / clip_position.w;
	vec3 prev_ndc = prev_clip_position.xyz / prev_clip_position.w;
	motion_vectors = vec4(ndc - prev_ndc, 0.0);
#endif // RENDER_MOTION_VECTORS
}
