

#version 450

#define MAX_ROUGHNESS_LOD 7.0
#define SDFGI_OCT_SIZE 6
#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8
#define MAX_LIGHTMAP_TEXTURES 8
#define MAX_LIGHTMAPS 8
#define MATERIAL_UNIFORM_SET 3
#define USE_INSTANCE192_OUTPUT
#define INV_MODEL_MATRIX_USED
#define MODEL_MATRIX_USED
#define COLOR_USED
#define CUSTOM0_USED
#define CUSTOM1_USED
#define CUSTOM2_USED
#define CUSTOM3_USED
#define UV_USED
#define UV2_USED
#define DO_SIDE_CHECK
#define TANGENT_USED
#define MODE_RENDER_DEPTH
#define MODE_DUAL_PARABOLOID
#define SHADOW_PASS
#define UBERSHADER


#define SHADER_IS_SRGB false
#define SHADER_SPACE_FAR 0.0

#ifdef SHADOW_PASS
#define IN_SHADOW_PASS true
#else
#define IN_SHADOW_PASS false
#endif

#ifdef USE_MULTIVIEW
#define OUTPUT_IS_MULTIVIEW true
#else
#define OUTPUT_IS_MULTIVIEW false
#endif

/* Include half precision types. */
// Use of FP16 in Godot is done explicitly through the types half and hvec.
// The extensions must be supported by the system to use this shader.
//
// If EXPLICIT_FP16 is not defined, all operations will use full precision
// floats instead and all casting operations will not be performed.

#ifndef HALF_INC_H
#define HALF_INC_H

#ifdef EXPLICIT_FP16

#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_16bit_storage : require

#define HALF_FLT_MIN float16_t(6.10352e-5)
#define HALF_FLT_MAX float16_t(65504.0)

#define half float16_t
#define hvec2 f16vec2
#define hvec3 f16vec3
#define hvec4 f16vec4
#define hmat2 f16mat2
#define hmat3 f16mat3
#define hmat4 f16mat4
#define saturateHalf(x) min(float16_t(x), HALF_FLT_MAX)

#else

#define HALF_FLT_MIN float(1.175494351e-38F)
#define HALF_FLT_MAX float(3.402823466e+38F)

#define half float
#define hvec2 vec2
#define hvec3 vec3
#define hvec4 vec4
#define hmat2 mat2
#define hmat3 mat3
#define hmat4 mat4
#define saturateHalf(x) (x)

#endif

#endif // HALF_INC_H


/* Include our forward mobile UBOs definitions etc. */
#define M_PI 3.14159265359
#define MAX_VIEWS 2
struct DecalData {
	mat4 xform; //to decal transform
	vec3 inv_extents;
	float albedo_mix;
	vec4 albedo_rect;
	vec4 normal_rect;
	vec4 orm_rect;
	vec4 emission_rect;
	vec4 modulate;
	float emission_energy;
	uint mask;
	float upper_fade;
	float lower_fade;
	mat3x4 normal_xform;
	vec3 normal;
	float normal_fade;
};


vec3 oct_to_vec3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	float t = max(-v.z, 0.0);
	v.xy += t * -sign(v.xy);
	return normalize(v);
}

// border_size: 1.0 - padding_in_uv_space * 2.0.
vec3 oct_to_vec3_with_border(vec2 uv, float border_size) {
	// Convert into [-1,1] space and add border which extends beyond [-1,1].
	uv = (uv - 0.5) * (2.0 / border_size);
	// Calculate octahedral mirroring for values outside of [-1,1].
	// Inspired by Timothy Lottes' code here: https://gpuopen.com/learn/fetching-from-cubes-and-octahedrons/
	vec2 mask = step(vec2(1.0), abs(uv));
	uv = 2.0 * clamp(uv, -1.0, 1.0) - uv;
	uv = mix(uv, -uv, mask.yx);
	return oct_to_vec3(uv);
}

vec2 oct_wrap(vec2 v) {
	vec2 signVal;
	signVal.x = v.x >= 0.0 ? 1.0 : -1.0;
	signVal.y = v.y >= 0.0 ? 1.0 : -1.0;
	return (1.0 - abs(v.yx)) * signVal;
}

vec2 vec3_to_oct(vec3 n) {
	// Reference: https://twitter.com/Stubbesaurus/status/937994790553227264
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = (n.z >= 0.0) ? n.xy : oct_wrap(n.xy);
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}

// border_size.x: padding_in_uv_space
// border_size.y: 1.0 - padding_in_uv_space * 2.0
vec2 vec3_to_oct_with_border(vec3 n, vec2 border_size) {
	vec2 uv = vec3_to_oct(n);
	return uv * border_size.y + border_size.x;
}

float vec3_to_oct_lod(vec3 n_ddx, vec3 n_ddy, float pixel_size) {
	// Approximate UV space derivatives by a factor of 0.5 because
	// vec3_to_oct maps from [-1,1] to [0,1].
	float pixel_size_sqr = 4.0 * pixel_size * pixel_size;
	float ddx = dot(n_ddx, n_ddx) / pixel_size_sqr;
	float ddy = dot(n_ddy, n_ddy) / pixel_size_sqr;
	float dd_sqr = max(ddx, ddy);
	return 0.25 * log2(dd_sqr + 1e-6f);
}

#ifndef HEIGHT_FOG_INC_GLSL
#define HEIGHT_FOG_INC_GLSL

// Canonical linear radiance. CPU transport is RendererEnvironmentStorage::HeightFogData.
struct HeightFogData {
	vec4 density;
	vec4 control;
	vec4 source;
	vec4 options;
	vec4 direction0;
	vec4 color0;
	vec4 direction1;
	vec4 color1;
};

float height_fog_log_phi(float x) {
	if (x < 0.001) {
		return log(1.0 - x * 0.5 + x * x / 6.0 - x * x * x / 24.0);
	}
	return x > 50.0 ? -log(min(x, 1e30)) : log((1.0 - exp(-x)) / x);
}

// Use the denser endpoint and a positive phi argument; this avoids cancellation
// of two overflowing exponentials on long downward rays.
float height_fog_log_tau(HeightFogData fog, vec3 origin, vec3 ray, float distance_to_surface, bool infinite_ray) {
	float beta = fog.density.x;
	float falloff = fog.density.y;
	float start = fog.density.w;
	if (beta <= 0.0 || (!infinite_ray && distance_to_surface <= start)) {
		return -1e30;
	}
	if (falloff <= 0.0) {
		return infinite_ray ? 1e30 : log(beta) + log(distance_to_surface - start);
	}
	float start_height = origin.y + ray.y * start;
	float log_density = log(beta);
	float log_tau;
	if (infinite_ray) {
		if (falloff <= 0.0 || ray.y <= 0.0) {
			return 1e30;
		}
		log_tau = log_density - clamp(falloff * (start_height - fog.density.z), -1e30, 1e30) - log(falloff) - log(ray.y);
	} else {
		float remaining = distance_to_surface - start;
		float end_height = origin.y + ray.y * distance_to_surface;
		float dense_height = min(start_height, end_height);
		float x = min(falloff * abs(ray.y) * remaining, 1e30);
		log_tau = log_density - clamp(falloff * (dense_height - fog.density.z), -1e30, 1e30) + log(remaining) + height_fog_log_phi(x);
	}
	return log_tau;
}

// Returns accumulated in-scattering L and transmittance T. Optional capture
// scattering is disabled by the caller for every reflection capture view.
vec4 height_fog_integrate(HeightFogData fog, vec3 origin, vec3 ray, float distance_to_surface, bool infinite_ray, vec3 capture_color, bool capture_ready) {
	if (fog.control.w == 0.0 || fog.control.x == 0.0) {
		return vec4(0.0, 0.0, 0.0, 1.0);
	}
	float log_tau = height_fog_log_tau(fog, origin, ray, distance_to_surface, infinite_ray);
	float tau = exp(clamp(log_tau, -80.0, 69.0));
	float transmittance = max(exp(-tau), 1.0 - fog.control.x);
	float strength = capture_ready ? fog.source.w : 0.0;
	vec3 source = mix(fog.source.rgb, capture_color, strength);
	vec3 scattering = source * (1.0 - transmittance);
	float remaining = max(distance_to_surface - fog.density.w, 0.0);
	float directional_fraction = remaining > 0.0 ? max(1.0 - fog.control.y / remaining, 0.0) : 0.0;
	float directional_tau = infinite_ray ? tau : (directional_fraction > 0.0 ? exp(clamp(log_tau + log(directional_fraction), -80.0, 69.0)) : 0.0);
	float directional_opacity = (1.0 - exp(-directional_tau)) * (1.0 - strength);
	vec3 directional = fog.color0.rgb * pow(max(dot(ray, fog.direction0.xyz), 0.0), fog.control.z);
	directional += fog.color1.rgb * pow(max(dot(ray, fog.direction1.xyz), 0.0), fog.control.z);
	scattering += directional * directional_opacity * (1.0 / (4.0 * 3.141592653589793));
	return vec4(scattering, transmittance);
}

#endif


// Scene data stores all our 3D rendering globals for a frame such as our matrices
// where this information is independent of the different RD implementations.
// This enables us to use this UBO in our main scene render shaders but also in
// effects that need access to this data.

#define SCENE_DATA_FLAGS_USE_AMBIENT_LIGHT (1 << 0)
#define SCENE_DATA_FLAGS_USE_AMBIENT_CUBEMAP (1 << 1)
#define SCENE_DATA_FLAGS_USE_REFLECTION_CUBEMAP (1 << 2)
#define SCENE_DATA_FLAGS_USE_ROUGHNESS_LIMITER (1 << 3)
#define SCENE_DATA_FLAGS_USE_FOG (1 << 4)
#define SCENE_DATA_FLAGS_USE_UV2_MATERIAL (1 << 5)
#define SCENE_DATA_FLAGS_USE_PANCAKE_SHADOWS (1 << 6)
#define SCENE_DATA_FLAGS_IN_SHADOW_PASS (1 << 7)

struct SceneData {
	mat4 projection_matrix;
	mat4 inv_projection_matrix;
	mat3x4 inv_view_matrix;
	mat3x4 view_matrix;

#ifdef USE_DOUBLE_PRECISION
	vec4 inv_view_precision;
#endif

	// only used for multiview
	mat4 projection_matrix_view[MAX_VIEWS];
	mat4 inv_projection_matrix_view[MAX_VIEWS];
	vec4 eye_offset[MAX_VIEWS];

	// Used for billboards to cast correct shadows.
	mat4 main_cam_inv_view_matrix;

	vec2 viewport_size;
	vec2 screen_pixel_size;

	// Use vec4s because std140 doesn't play nice with vec2s, z and w are wasted.
	vec4 directional_penumbra_shadow_kernel[32];
	vec4 directional_soft_shadow_kernel[32];
	vec4 penumbra_shadow_kernel[32];
	vec4 soft_shadow_kernel[32];

	vec2 shadow_atlas_pixel_size;
	vec2 directional_shadow_pixel_size;

	float radiance_pixel_size;
	float radiance_border_size;
	vec2 reflection_atlas_border_size;

	uint directional_light_count;
	float dual_paraboloid_side;
	float z_far;
	float z_near;

	float roughness_limiter_amount;
	float roughness_limiter_limit;
	float opaque_prepass_threshold;
	uint flags;

	mat3 radiance_inverse_xform;

	vec4 ambient_light_color_energy;

	float ambient_color_sky_mix;
	float fog_density;
	float fog_height;
	float fog_height_density;

	float fog_depth_curve;
	float fog_depth_begin;
	float fog_depth_end;
	float fog_sun_scatter;

	vec3 fog_light_color;
	float fog_aerial_perspective;

	float time;
	float taa_frame_count;
	vec2 taa_jitter;

	float emissive_exposure_normalization;
	float IBL_exposure_normalization;
	uint camera_visible_layers;
	float pass_alpha_multiplier;
	vec4 sky_capture_data;
	vec4 sky_capture_fallback;
	mat3 sky_capture_old_xform;
	mat3 sky_capture_next_xform;
	HeightFogData height_fog;
	vec4 height_fog_view;
	vec4 aerial_perspective;
};


#if !defined(MODE_RENDER_DEPTH) || defined(MODE_RENDER_MATERIAL) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#define USING_MOBILE_RENDERER

layout(push_constant, std430) uniform DrawCall {
	uint uv_offset;
	uint instance_index;
	uint multimesh_motion_vectors_current_offset;
	uint multimesh_motion_vectors_previous_offset;
#ifdef UBERSHADER
	uint sc_packed_0;
	uint sc_packed_1;
	float sc_packed_2;
	uint uc_packed_0;
#endif
}
draw_call;

/* Specialization Constants */

#ifdef UBERSHADER

#define POLYGON_CULL_DISABLED 0
#define POLYGON_CULL_FRONT 1
#define POLYGON_CULL_BACK 2

// Pull the constants from the draw call's push constants.
uint sc_packed_0() {
	return draw_call.sc_packed_0;
}

uint sc_packed_1() {
	return draw_call.sc_packed_1;
}

float sc_packed_2() {
	return draw_call.sc_packed_2;
}

uint uc_cull_mode() {
	return (draw_call.uc_packed_0 >> 0) & 3U;
}

#else

// Pull the constants from the pipeline's specialization constants.
layout(constant_id = 0) const uint pso_sc_packed_0 = 0;
layout(constant_id = 1) const uint pso_sc_packed_1 = 0;
layout(constant_id = 2) const float pso_sc_packed_2 = 2.0;

uint sc_packed_0() {
	return pso_sc_packed_0;
}

uint sc_packed_1() {
	return pso_sc_packed_1;
}

float sc_packed_2() {
	return pso_sc_packed_2;
}

#endif

bool sc_use_light_projector() {
	return ((sc_packed_0() >> 0) & 1U) != 0;
}

bool sc_use_light_soft_shadows() {
	return ((sc_packed_0() >> 1) & 1U) != 0;
}

bool sc_use_directional_soft_shadows() {
	return ((sc_packed_0() >> 2) & 1U) != 0;
}

bool sc_decal_use_mipmaps() {
	return ((sc_packed_0() >> 3) & 1U) != 0;
}

bool sc_projector_use_mipmaps() {
	return ((sc_packed_0() >> 4) & 1U) != 0;
}

bool sc_disable_fog() {
	return ((sc_packed_0() >> 5) & 1U) != 0;
}

bool sc_use_depth_fog() {
	return ((sc_packed_0() >> 6) & 1U) != 0;
}

bool sc_use_fog_aerial_perspective() {
	return ((sc_packed_0() >> 7) & 1U) != 0;
}

bool sc_use_fog_sun_scatter() {
	return ((sc_packed_0() >> 8) & 1U) != 0;
}

bool sc_use_fog_height_density() {
	return ((sc_packed_0() >> 9) & 1U) != 0;
}

bool sc_use_lightmap_bicubic_filter() {
	return ((sc_packed_0() >> 10) & 1U) != 0;
}

bool sc_use_material_debanding() {
	return ((sc_packed_0() >> 11) & 1U) != 0;
}

bool sc_multimesh() {
	return ((sc_packed_0() >> 12) & 1U) != 0;
}

bool sc_multimesh_format_2d() {
	return ((sc_packed_0() >> 13) & 1U) != 0;
}

bool sc_multimesh_has_color() {
	return ((sc_packed_0() >> 14) & 1U) != 0;
}

bool sc_multimesh_has_custom_data() {
	return ((sc_packed_0() >> 15) & 1U) != 0;
}

bool sc_scene_use_ambient_cubemap() {
	return ((sc_packed_0() >> 16) & 1U) != 0;
}

bool sc_scene_use_reflection_cubemap() {
	return ((sc_packed_0() >> 17) & 1U) != 0;
}

bool sc_scene_roughness_limiter_enabled() {
	return ((sc_packed_0() >> 18) & 1U) != 0;
}

bool sc_material_feedback() {
	return ((sc_packed_0() >> 19) & 1U) != 0;
}

uint sc_soft_shadow_samples() {
	return (sc_packed_0() >> 20) & 63U;
}

uint sc_penumbra_shadow_samples() {
	return (sc_packed_0() >> 26) & 63U;
}

uint sc_directional_soft_shadow_samples() {
	return (sc_packed_1() >> 0) & 63U;
}

uint sc_directional_penumbra_shadow_samples() {
	return (sc_packed_1() >> 6) & 63U;
}

#define SHADER_COUNT_NONE 0
#define SHADER_COUNT_SINGLE 1
#define SHADER_COUNT_MULTIPLE 2

uint option_to_count(uint option, uint bound) {
	switch (option) {
		case SHADER_COUNT_NONE:
			return 0;
		case SHADER_COUNT_SINGLE:
			return 1;
		case SHADER_COUNT_MULTIPLE:
			return bound;
	}
}

uint sc_omni_lights(uint bound) {
	uint option = (sc_packed_1() >> 12) & 3U;
	return option_to_count(option, bound);
}

uint sc_spot_lights(uint bound) {
	uint option = (sc_packed_1() >> 14) & 3U;
	return option_to_count(option, bound);
}

uint sc_area_lights(uint bound) {
	uint option = (sc_packed_1() >> 16) & 3U;
	return option_to_count(option, bound);
}

uint sc_reflection_probes(uint bound) {
	uint option = (sc_packed_1() >> 18) & 3U;
	return option_to_count(option, bound);
}

uint sc_directional_lights(uint bound) {
	uint option = (sc_packed_1() >> 20) & 3U;
	return option_to_count(option, bound);
}

uint sc_decals(uint bound) {
	if (((sc_packed_1() >> 22) & 1U) != 0) {
		return bound;
	} else {
		return 0;
	}
}

bool sc_directional_light_blend_split(uint i) {
	return ((sc_packed_1() >> (23 + i)) & 1U) != 0;
}

bool sc_use_lightmap_specular() {
	return ((sc_packed_1() >> 31) & 1U) != 0;
}

half sc_luminance_multiplier() {
	return half(sc_packed_2());
}

layout(constant_id = 3) const bool sc_emulate_point_size = false;

#ifdef POINT_SIZE_USED

#define VERTEX_INDEX (sc_emulate_point_size ? gl_InstanceIndex : gl_VertexIndex)
#define INSTANCE_INDEX (sc_emulate_point_size ? (gl_VertexIndex / 6) : gl_InstanceIndex)

#else

#define VERTEX_INDEX gl_VertexIndex
#define INSTANCE_INDEX gl_InstanceIndex

#endif

// Like the luminance multiplier, but it is only for sky and reflection probes
// since they are always LDR.
#define REFLECTION_MULTIPLIER half(2.0)

/* Set 0: Base Pass (never changes) */
#define LIGHT_BAKE_DISABLED 0
#define LIGHT_BAKE_STATIC 1
#define LIGHT_BAKE_DYNAMIC 2

struct LightData { //this structure needs to be as packed as possible
	vec3 position;
	float inv_radius;

	vec3 direction;
	float size;

	vec3 color;
	float attenuation;

	mediump vec3 area_width;
	float cone_attenuation; // area lights: 1 / (range + diagonal/2)

	mediump vec3 area_height;
	float cone_angle; // area lights: max mipmaps

	float specular_amount;
	float shadow_opacity;
	float pad[2];

	vec4 atlas_rect; // rect in the shadow atlas
	mat4 shadow_matrix;
	float shadow_bias;
	float shadow_normal_bias;
	float transmittance_bias;
	float soft_shadow_size; // for spot, it's the size in uv coordinates of the light, for omni it's the span angle
	float soft_shadow_scale; // scales the shadow kernel for blurrier shadows
	uint mask;
	float volumetric_fog_energy;
	uint bake_mode;
	vec4 projector_rect; //projector rect in srgb decal atlas
};

#define REFLECTION_AMBIENT_DISABLED 0
#define REFLECTION_AMBIENT_ENVIRONMENT 1
#define REFLECTION_AMBIENT_COLOR 2

struct ReflectionData {
	vec3 box_extents;
	float index;
	vec3 box_offset;
	uint mask;
	vec3 ambient; // ambient color
	float intensity;
	float blend_distance;
	bool exterior;
	bool box_project;
	uint ambient_mode;
	float exposure_normalization;
	float pad0;
	float pad1;
	float pad2;
	//0-8 is intensity,8-9 is ambient, mode
	mat4 local_matrix; // up to here for spot and omni, rest is for directional
	// notes: for ambientblend, use distance to edge to blend between already existing global environment
};

struct DirectionalLightData {
	vec3 direction;
	float energy; // needs to be highp to avoid NaNs being created with high energy values (i.e. when using physical light units and over-exposing the image)
	vec3 color;
	float size;
	float specular;
	uint mask;
	float softshadow_angle;
	float soft_shadow_scale;
	bool blend_splits;
	float shadow_opacity;
	float fade_from;
	float fade_to;
	uvec2 pad;
	uint bake_mode;
	float volumetric_fog_energy;
	vec4 shadow_bias;
	vec4 shadow_normal_bias;
	vec4 shadow_transmittance_bias;
	vec4 shadow_z_range;
	vec4 shadow_range_begin;
	vec4 shadow_split_offsets;
	mat4 shadow_matrix1;
	mat4 shadow_matrix2;
	mat4 shadow_matrix3;
	mat4 shadow_matrix4;
	vec2 uv_scale1;
	vec2 uv_scale2;
	vec2 uv_scale3;
	vec2 uv_scale4;
};


layout(set = 0, binding = 2) uniform sampler shadow_sampler;

#define INSTANCE_FLAGS_DYNAMIC (1 << 3)
#define INSTANCE_FLAGS_NON_UNIFORM_SCALE (1 << 4)
#define INSTANCE_FLAGS_USE_GI_BUFFERS (1 << 5)
#define INSTANCE_FLAGS_USE_SDFGI (1 << 6)
#define INSTANCE_FLAGS_USE_LIGHTMAP_CAPTURE (1 << 7)
#define INSTANCE_FLAGS_USE_LIGHTMAP (1 << 8)
#define INSTANCE_FLAGS_USE_SH_LIGHTMAP (1 << 9)
#define INSTANCE_FLAGS_USE_VOXEL_GI (1 << 10)
#define INSTANCE_FLAGS_PARTICLES (1 << 11)
#define INSTANCE_FLAGS_PARTICLE_TRAIL_SHIFT 16
//3 bits of stride
#define INSTANCE_FLAGS_PARTICLE_TRAIL_MASK 0xFF

layout(set = 0, binding = 3, std430) restrict readonly buffer OmniLights {
	LightData data[];
}
omni_lights;

layout(set = 0, binding = 4, std430) restrict readonly buffer SpotLights {
	LightData data[];
}
spot_lights;

layout(set = 0, binding = 5, std430) restrict readonly buffer AreaLights {
	LightData data[];
}
area_lights;

layout(set = 0, binding = 6, std430) restrict readonly buffer ReflectionProbeData {
	ReflectionData data[];
}
reflections;

layout(set = 0, binding = 7, std140) uniform DirectionalLights {
	DirectionalLightData data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
}
directional_lights;

#define LIGHTMAP_FLAG_USE_DIRECTION 1
#define LIGHTMAP_FLAG_USE_SPECULAR_DIRECTION 2

#define LIGHTMAP_SHADOWMASK_MODE_NONE 0
#define LIGHTMAP_SHADOWMASK_MODE_REPLACE 1
#define LIGHTMAP_SHADOWMASK_MODE_OVERLAY 2
#define LIGHTMAP_SHADOWMASK_MODE_ONLY 3

struct Lightmap {
	mat3x4 normal_xform_and_specular_intensity; // "normal_xform" is the 3x3 matrix. "specular_intensity" is the 4th row of the 1st column.
	vec2 light_texture_size;
	float exposure_normalization;
	uint flags;
};

layout(set = 0, binding = 8, std140) restrict readonly buffer Lightmaps {
	Lightmap data[];
}
lightmaps;

struct LightmapCapture {
	vec4 sh[9];
};

layout(set = 0, binding = 9, std140) restrict readonly buffer LightmapCaptures {
	LightmapCapture data[];
}
lightmap_captures;

layout(set = 0, binding = 10) uniform texture2D decal_atlas;
layout(set = 0, binding = 11) uniform texture2D decal_atlas_srgb;

layout(set = 0, binding = 12, std430) restrict readonly buffer Decals {
	DecalData data[];
}
decals;

layout(set = 0, binding = 13, std430) restrict readonly buffer GlobalShaderUniformData {
	vec4 data[];
}
global_shader_uniforms;

layout(set = 0, binding = 14) uniform sampler DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP;

layout(set = 0, binding = 15) uniform texture2D ltc_lut1;

layout(set = 0, binding = 16) uniform texture2D ltc_lut2;

layout(set = 0, binding = 17) uniform texture2D area_light_atlas;

/* Set 1: Render Pass (changes per render pass) */

layout(set = 1, binding = 0, std140) uniform SceneDataBlock {
	SceneData data;
	SceneData prev_data;
}
scene_data_block;

struct InstanceData {
	highp mat3x4 transform;
	vec4 compressed_aabb_position_pad; // Only .xyz is used. .w is padding.
	vec3 compressed_aabb_size_pad; // Only .xyz is used.
	uint material_feedback_index; // Index into the material feedback buffer.
	vec4 uv_scale;
	uint flags;
	uint instance_uniforms_ofs; // Base offset in global buffer for instance variables.
	uint gi_offset; // GI information when using lightmapping (VCT or lightmap index).
	uint layer_mask;
	highp mat3x4 prev_transform;

	vec4 lightmap_uv_scale; // Doubles as uv_offset when needed.
	uvec2 reflection_probes;
	uvec2 omni_lights;
	uvec2 spot_lights;
	uvec2 area_lights;
	uvec2 decals;
	uvec2 padding;
#ifdef USE_DOUBLE_PRECISION
	vec4 model_precision;
	vec4 prev_model_precision;
#endif
};

layout(set = 1, binding = 1, std430) buffer restrict readonly InstanceDataBuffer {
	InstanceData data[];
}
instances;

#ifdef USE_RADIANCE_OCTMAP_ARRAY

layout(set = 1, binding = 2) uniform texture2DArray radiance_octmap;

#else

layout(set = 1, binding = 2) uniform texture2D radiance_octmap;

#endif

layout(set = 1, binding = 3) uniform texture2DArray reflection_atlas;

layout(set = 1, binding = 4) uniform texture2D shadow_atlas;

layout(set = 1, binding = 5) uniform texture2D directional_shadow_atlas;

// this needs to change to providing just the lightmap we're using..
layout(set = 1, binding = 6) uniform texture2DArray lightmap_textures[MAX_LIGHTMAP_TEXTURES * 2];

#ifdef USE_MULTIVIEW
layout(set = 1, binding = 9) uniform texture2DArray depth_buffer;
layout(set = 1, binding = 10) uniform texture2DArray color_buffer;
#define multiviewSampler sampler2DArray
#else
layout(set = 1, binding = 9) uniform texture2D depth_buffer;
layout(set = 1, binding = 10) uniform texture2D color_buffer;
#define multiviewSampler sampler2D
#endif // USE_MULTIVIEW

layout(set = 1, binding = 11) uniform sampler decal_sampler;

layout(set = 1, binding = 12) uniform sampler light_projector_sampler;

layout(set = 1, binding = 13 + 0) uniform sampler SAMPLER_NEAREST_CLAMP;
layout(set = 1, binding = 13 + 1) uniform sampler SAMPLER_LINEAR_CLAMP;
layout(set = 1, binding = 13 + 2) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_CLAMP;
layout(set = 1, binding = 13 + 3) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP;
layout(set = 1, binding = 13 + 4) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_CLAMP;
layout(set = 1, binding = 13 + 5) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_CLAMP;
layout(set = 1, binding = 13 + 6) uniform sampler SAMPLER_NEAREST_REPEAT;
layout(set = 1, binding = 13 + 7) uniform sampler SAMPLER_LINEAR_REPEAT;
layout(set = 1, binding = 13 + 8) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_REPEAT;
layout(set = 1, binding = 13 + 9) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_REPEAT;
layout(set = 1, binding = 13 + 10) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_REPEAT;
layout(set = 1, binding = 13 + 11) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_REPEAT;

#ifdef TEXTURE_STREAMING
// Texture streaming material feedback buffer access
layout(set = 1, binding = 25, std430) buffer restrict MaterialFeedbackBuffer {
	uint data[];
}
material_feedback;
#endif

/* Set 2 Skeleton & Instancing (can change per item) */

layout(set = 2, binding = 0, std430) restrict readonly buffer Transforms {
	vec4 data[];
}
transforms;

#ifdef USE_INSTANCE192_OUTPUT
// Private Instance192 raster binding. G cannot publish this set directly: K
// retains the guarded current/previous records and exact packet read lease.
#ifdef USE_DOUBLE_PRECISION
#error Instance192 raster requires a separately qualified double/origin path
#endif
#ifdef USE_INDEXED144_VERTEX_INPUT
#error Instance192 and Indexed144 are distinct raster profiles
#endif
// ShaderRD enables a group for every material version. Native trails
// materials must remain compilable in that group, although the C++ material
// gate refuses selecting this profile for them. Never #error on material flags.

layout(set = 2, binding = 1, std430) restrict readonly buffer Instance192Previous {
	vec4 data[];
} instance192_previous;

// Match RD PolygonCullMode (disabled=0/front=1/back=2). Pipeline hardware cull
// is disabled; this is the original material/pass cull request, never a bucket.
layout(constant_id = 4) const uint instance192_cull_mode = 0u;

struct Instance192Record {
	mat4 model;
	mat3 normal;
	mat4 inverse_model;
	vec4 color;
	vec4 custom;
	uint flags;
};

uint instance192_flags(uint index) {
	return floatBitsToUint(transforms.data[index * 12u + 11u].z);
}

bool instance192_has_history(uint flags) {
	return (flags & 2u) != 0u;
}

vec4 instance192_row(uint index, uint row, bool previous) {
	uint offset = index * 12u + row;
	if (previous) {
		return instance192_previous.data[offset];
	}
	return transforms.data[offset];
}

Instance192Record instance192_read(uint index, bool previous) {
	// Invalid history uses current geometry and never fetches stale previous.
	previous = previous && instance192_has_history(instance192_flags(index));
	Instance192Record record;
	record.model = transpose(mat4(instance192_row(index, 0u, previous), instance192_row(index, 1u, previous), instance192_row(index, 2u, previous), vec4(0.0, 0.0, 0.0, 1.0)));
	record.normal = transpose(mat3(instance192_row(index, 3u, previous).xyz, instance192_row(index, 4u, previous).xyz, instance192_row(index, 5u, previous).xyz));
	record.inverse_model = transpose(mat4(instance192_row(index, 6u, previous), instance192_row(index, 7u, previous), instance192_row(index, 8u, previous), vec4(0.0, 0.0, 0.0, 1.0)));
	record.color = instance192_row(index, 9u, previous);
	record.custom = instance192_row(index, 10u, previous);
	record.flags = floatBitsToUint(instance192_row(index, 11u, previous).z);
	return record;
}

bool instance192_front_facing(bool raster_front, uint flags) {
	return raster_front != ((flags & 4u) != 0u);
}

bool instance192_discard_face(bool logical_front) {
	#ifdef UBERSHADER
	uint material_cull = uc_cull_mode();
#else
	uint material_cull = instance192_cull_mode;
#endif
	return (material_cull == 1u && logical_front) || (material_cull == 2u && !logical_front);
}

#define MATERIAL_FRONT_FACING instance192_front_facing(gl_FrontFacing, instance192_interp.y)
#else
#define MATERIAL_FRONT_FACING gl_FrontFacing
#endif

/* Set 3 User Material */

#ifdef USE_RADIANCE_OCTMAP_ARRAY
layout(set = 1, binding = 26) uniform texture2DArray radiance_octmap_next;
#else
layout(set = 1, binding = 26) uniform texture2D radiance_octmap_next;
#endif

layout(set = 1, binding = 27) uniform texture3D aerial_perspective_volume;
#ifdef USE_MULTIVIEW
layout(set = 1, binding = 28) uniform texture3D aerial_perspective_volume_eye1;
#endif

#ifdef USE_INSTANCE192_OUTPUT
layout(location = 10) in flat uvec2 instance192_interp;
#endif


/* Varyings */

// All interpolators are intentionally kept at full precision as storageInputOutput16 is not
// checked for support. Devices with Adreno GPUs don't usually support this capability.

layout(location = 0) in vec3 vertex_interp;

#ifdef NORMAL_USED
layout(location = 1) in vec3 normal_interp;
#endif

#if defined(COLOR_USED)
layout(location = 2) in vec4 color_interp;
#endif

#ifdef UV_USED
layout(location = 3) in vec2 uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
layout(location = 4) in vec2 uv2_interp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
layout(location = 5) in vec3 tangent_interp;
layout(location = 6) in vec3 binormal_interp;
#endif

#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)
layout(location = 7) in vec4 diffuse_light_interp;
layout(location = 8) in vec4 specular_light_interp;
#endif

#ifdef MODE_DUAL_PARABOLOID

layout(location = 9) in float dp_clip;

#endif

#if defined(MODE_RENDER_MOTION_VECTORS)
layout(location = 12) in highp vec4 screen_position;
layout(location = 13) in highp vec4 prev_screen_position;
#endif

#ifdef USE_LIGHTMAP
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

vec4 textureArray_bicubic(texture2DArray tex, vec3 uv, vec2 texture_size) {
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

	return (g0(fuv.y) * (g0x * texture(sampler2DArray(tex, SAMPLER_LINEAR_CLAMP), vec3(p0, uv.z)) + g1x * texture(sampler2DArray(tex, SAMPLER_LINEAR_CLAMP), vec3(p1, uv.z)))) +
			(g1(fuv.y) * (g0x * texture(sampler2DArray(tex, SAMPLER_LINEAR_CLAMP), vec3(p2, uv.z)) + g1x * texture(sampler2DArray(tex, SAMPLER_LINEAR_CLAMP), vec3(p3, uv.z))));
}
#endif //USE_LIGHTMAP

#ifdef USE_MULTIVIEW
#extension GL_EXT_multiview : enable
#define ViewIndex gl_ViewIndex
vec3 multiview_uv(vec2 uv) {
	return vec3(uv, ViewIndex);
}
ivec3 multiview_uv(ivec2 uv) {
	return ivec3(uv, int(ViewIndex));
}
#else // USE_MULTIVIEW
#define ViewIndex 0
vec2 multiview_uv(vec2 uv) {
	return uv;
}
ivec2 multiview_uv(ivec2 uv) {
	return uv;
}
#endif // !USE_MULTIVIEW

#if defined(POINT_SIZE_USED) && defined(POINT_COORD_USED)
layout(location = 14) in vec2 point_coord_interp;
#endif

//defines to keep compatibility with vertex

#ifdef USE_MULTIVIEW
#define projection_matrix scene_data.projection_matrix_view[ViewIndex]
#define inv_projection_matrix scene_data.inv_projection_matrix_view[ViewIndex]
#else
#define projection_matrix scene_data.projection_matrix
#define inv_projection_matrix scene_data.inv_projection_matrix
#endif

#if defined(ENABLE_SSS) && defined(ENABLE_TRANSMITTANCE)
//both required for transmittance to be enabled
#define LIGHT_TRANSMITTANCE_USED
#endif

#ifdef MATERIAL_UNIFORMS_USED
/* clang-format off */
layout(set = MATERIAL_UNIFORM_SET, binding = 0, std140) uniform MaterialUniforms {

} material;
/* clang-format on */
#endif

layout(location=15) in vec4 witness;

#define scene_data scene_data_block.data

/* clang-format on */

#ifdef MODE_RENDER_DEPTH

#ifdef MODE_RENDER_MATERIAL

layout(location = 0) out vec4 albedo_output_buffer;
layout(location = 1) out vec4 normal_output_buffer;
layout(location = 2) out vec4 orm_output_buffer;
layout(location = 3) out vec4 emission_output_buffer;
layout(location = 4) out float depth_output_buffer;

#endif // MODE_RENDER_MATERIAL

#else // RENDER DEPTH

#ifdef MODE_MULTIPLE_RENDER_TARGETS

layout(location = 0) out vec4 diffuse_buffer; //diffuse (rgb) and roughness
layout(location = 1) out vec4 specular_buffer; //specular and SSS (subsurface scatter)
#else

layout(location = 0) out vec4 frag_color;
#ifdef DEFER_OPAQUE_FOG
layout(location = 1) out float fog_eligibility;
#endif
#endif // MODE_MULTIPLE_RENDER_TARGETS

#endif // RENDER DEPTH
#ifdef ALPHA_HASH_USED

float hash_2d(vec2 p) {
	return fract(1.0e4 * sin(17.0 * p.x + 0.1 * p.y) *
			(0.1 + abs(sin(13.0 * p.y + p.x))));
}

float hash_3d(vec3 p) {
	return hash_2d(vec2(hash_2d(p.xy), p.z));
}

half compute_alpha_hash_threshold(vec3 pos, float hash_scale) {
	vec3 dx = dFdx(pos);
	vec3 dy = dFdy(pos);

	float delta_max_sqr = max(length(dx), length(dy));
	float pix_scale = 1.0 / (hash_scale * delta_max_sqr);

	vec2 pix_scales =
			vec2(exp2(floor(log2(pix_scale))), exp2(ceil(log2(pix_scale))));

	vec2 a_thresh = vec2(hash_3d(floor(pix_scales.x * pos.xyz)),
			hash_3d(floor(pix_scales.y * pos.xyz)));

	float lerp_factor = fract(log2(pix_scale));

	float a_interp = (1.0 - lerp_factor) * a_thresh.x + lerp_factor * a_thresh.y;

	float min_lerp = min(lerp_factor, 1.0 - lerp_factor);

	vec3 cases = vec3(a_interp * a_interp / (2.0 * min_lerp * (1.0 - min_lerp)),
			(a_interp - 0.5 * min_lerp) / (1.0 - min_lerp),
			1.0 - ((1.0 - a_interp) * (1.0 - a_interp) / (2.0 * min_lerp * (1.0 - min_lerp))));

	float alpha_hash_threshold =
			(a_interp < (1.0 - min_lerp)) ? ((a_interp < min_lerp) ? cases.x : cases.y) : cases.z;

	return half(clamp(alpha_hash_threshold, 0.00001, 1.0));
}

#endif // ALPHA_HASH_USED

#ifdef ALPHA_ANTIALIASING_EDGE_USED

half calc_mip_level(vec2 texture_coord) {
	vec2 dx = dFdx(texture_coord);
	vec2 dy = dFdy(texture_coord);
	float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
	return max(half(0.0), half(0.5) * half(log2(delta_max_sqr)));
}

half compute_alpha_antialiasing_edge(half input_alpha, vec2 texture_coord, half alpha_edge) {
	input_alpha *= half(1.0) + calc_mip_level(texture_coord) * half(0.25);
	input_alpha = (input_alpha - alpha_edge) / max(fwidth(input_alpha), half(0.0001)) + half(0.5);
	return clamp(input_alpha, half(0.0), half(1.0));
}

#endif // ALPHA_ANTIALIASING_USED


#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) // && !defined(USE_VERTEX_LIGHTING)

// Default to SPECULAR_SCHLICK_GGX.
#if !defined(SPECULAR_DISABLED) && !defined(SPECULAR_SCHLICK_GGX) && !defined(SPECULAR_TOON)
#define SPECULAR_SCHLICK_GGX
#endif
// Functions related to lighting

#extension GL_EXT_control_flow_attributes : require
// Functions related to area lights

#define M_PI 3.14159265359
#define M_TAU 6.28318530718

float acos_approx(float p_x) {
	float x = abs(p_x);
	float res = -0.156583f * x + (M_PI / 2.0);
	res *= sqrt(1.0f - x);
	return (p_x >= 0) ? res : M_PI - res;
}

vec3 fetch_ltc_lod(vec2 uv, vec4 texture_rect, float lod, float max_mipmap, texture2D area_light_atlas, sampler texture_sampler) {
	float low = min(max(floor(lod), 0.0), max_mipmap - 1.0);
	float high = min(max(floor(lod + 1.0), 1.0), max_mipmap);
	vec2 sample_pos = texture_rect.xy + clamp(uv, 0.0, 1.0) * texture_rect.zw; // take border into account
	vec4 sample_col_low = textureLod(sampler2D(area_light_atlas, texture_sampler), sample_pos, low);
	vec4 sample_col_high = textureLod(sampler2D(area_light_atlas, texture_sampler), sample_pos, high);

	float blend = high - clamp(lod, high - 1.0, high);
	vec4 sample_col = mix(sample_col_high, sample_col_low, blend);
	return sample_col.rgb * sample_col.a; // premultiply alpha channel
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

		// Note: "integrate_edge_hill(p_proj0, half_point).y + integrate_edge_hill(half_point, p_proj1).y" code inlined to work around Adreno 650/660 shader compiler bug. Keep in sync with "modules/lightmapper_rd/lm_area_lights_inc.glsl" and "integrate_edge_hill" method.
		float theta_sintheta = 0.0;
		{
			float cosTheta = dot(p_proj0, half_point);

			float x = cosTheta;
			float y = abs(x);
			float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
			float b = 3.45068 + (4.18814 + y) * y;
			theta_sintheta = a / b;
			if (x < 0.0) {
				theta_sintheta = M_PI * inversesqrt(1.0 - x * x) - theta_sintheta; // original paper: 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - theta_sintheta
			}
		}
		float aa = (theta_sintheta * cross(p_proj0, half_point)).y;
		{
			float cosTheta = dot(half_point, p_proj1);

			float x = cosTheta;
			float y = abs(x);
			float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
			float b = 3.45068 + (4.18814 + y) * y;
			theta_sintheta = a / b;
			if (x < 0.0) {
				theta_sintheta = M_PI * inversesqrt(1.0 - x * x) - theta_sintheta; // original paper: 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - theta_sintheta
			}
		}
		float bb = (theta_sintheta * cross(half_point, p_proj1)).y;
		return aa + bb;
	} else {
		// Note: "integrate_edge_hill(p_proj0, p_proj1).y" code inlined to work around Adreno 650/660 shader compiler bug. Keep in sync with "modules/lightmapper_rd/lm_area_lights_inc.glsl" and "integrate_edge_hill" method.
		float cosTheta = dot(p_proj0, p_proj1);

		float x = cosTheta;
		float y = abs(x);
		float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
		float b = 3.45068 + (4.18814 + y) * y;
		float theta_sintheta = a / b;

		if (x < 0.0) {
			theta_sintheta = M_PI * inversesqrt(1.0 - x * x) - theta_sintheta; // original paper: 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - theta_sintheta
		}
		return (theta_sintheta * cross(p_proj0, p_proj1)).y;
	}
}

vec3 fetch_ltc_filtered_texture_with_form_factor(vec4 texture_rect, vec3 L[4], float max_mipmap, texture2D area_light_atlas, sampler texture_sampler) {
	vec3 L0 = normalize(L[0]);
	vec3 L1 = normalize(L[1]);
	vec3 L2 = normalize(L[2]);
	vec3 L3 = normalize(L[3]);

	vec3 F = vec3(0.0); // form factor
	F += integrate_edge_hill(L0, L1);
	F += integrate_edge_hill(L1, L2);
	F += integrate_edge_hill(L2, L3);
	F += integrate_edge_hill(L3, L0);

	vec2 uv;
	float lod = 0.0;

	if (dot(F, F) < 1e-16) {
		uv = vec2(0.5);
		lod = max_mipmap;
	} else {
		vec3 lx = L[1] - L[0];
		vec3 ly = L[3] - L[0];
		vec3 ln = cross(lx, ly);

		float dist_x_area = dot(L[0], ln);
		float d = dist_x_area / dot(F, ln);
		vec3 isec = d * F;
		vec3 li = isec - L[0]; // light to intersection

		float dot_lxy = dot(lx, ly);
		float inv_dot_lxlx = 1.0 / dot(lx, lx);
		vec3 ly_ = vec3(ly - lx * dot_lxy * inv_dot_lxlx); // can't be computed with half precision
		uv.y = dot(vec3(li), ly_) / dot(ly_, ly_);
		uv.x = dot(vec3(li), lx) * inv_dot_lxlx - dot_lxy * inv_dot_lxlx * uv.y;

		lod = abs(dist_x_area) / pow(dot(ln, ln), 0.75);
		lod = log(2048.0 * lod) / log(3.0);
	}
	return fetch_ltc_lod(vec2(1.0) - uv, texture_rect, lod, max_mipmap, area_light_atlas, texture_sampler);
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

void ltc_evaluate(vec3 normal, vec3 eye_vec, mat3 M_inv, vec3 points[4], vec4 texture_rect, float max_mipmap, texture2D area_light_atlas, sampler texture_sampler, out float integral, out vec3 tex_color) {
	// default is white
	tex_color = vec3(1.0);
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

	if (texture_rect != vec4(0.0)) {
		tex_color = vec3(fetch_ltc_filtered_texture_with_form_factor(texture_rect, L_unclipped, max_mipmap, area_light_atlas, texture_sampler));
	}

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

void ltc_evaluate_specular(vec3 normal, vec3 eye_vec, float roughness, vec3 points[4], vec4 texture_rect, float max_mipmap, texture2D area_light_atlas, sampler texture_sampler, sampler lut_sampler, texture2D ltc_lut1, texture2D ltc_lut2, out float ltc_specular, out vec2 fresnel, out vec3 ltc_specular_tex_color) {
	float theta = acos_approx(dot(normal, eye_vec));
	const float LTC_LUT_SIZE = float(64.0);
	vec2 lut_pos = vec2(max(roughness, float(0.02)), theta / float(0.5 * M_PI));
	vec2 lut_uv = vec2(lut_pos * (float(63.0) / LTC_LUT_SIZE) + vec2(float(0.5) / LTC_LUT_SIZE)); // offset by 1 pixel
	vec4 M_brdf_abcd = texture(sampler2D(ltc_lut1, lut_sampler), lut_uv);
	vec3 M_brdf_e_mag_fres = texture(sampler2D(ltc_lut2, lut_sampler), lut_uv).xyz;
	float scale = 1.0 / (M_brdf_abcd.x * M_brdf_e_mag_fres.x - M_brdf_abcd.y * M_brdf_abcd.w);

	mat3 M_inv = mat3(
			vec3(0, 0, 1.0 / M_brdf_abcd.z),
			vec3(-M_brdf_abcd.w * scale, M_brdf_abcd.x * scale, 0),
			vec3(-M_brdf_e_mag_fres.x * scale, M_brdf_abcd.y * scale, 0));

	ltc_evaluate(normal, eye_vec, M_inv, points, texture_rect, max_mipmap, area_light_atlas, texture_sampler, ltc_specular, ltc_specular_tex_color);
	fresnel = vec2(M_brdf_e_mag_fres.yz);
}

void ltc_evaluate_diff(vec3 normal, vec3 points[4], vec4 texture_rect, float max_mipmap, texture2D area_light_atlas, sampler texture_sampler, out float integral, out vec3 tex_color) {
	// default is white
	tex_color = vec3(1.0);
	// construct the orthonormal basis around the normal vector
	vec3 x, z;
	vec3 eye_vec = abs(normal.z) < 0.7 ? vec3(0.0, 0.0, -1.0) : vec3(1.0, 0.0, 0.0);
	z = -normalize(eye_vec - normal * dot(eye_vec, normal)); // expanding the angle between view and normal vector to 90 degrees, this gives a normal vector
	x = cross(normal, z);

	// rotate area light in (T1, normal, T2) basis
	mat3 M_inv = transpose(mat3(x, normal, z));

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
		// for global illumination that shouldn't cause any visual artifacts
		integral = 0.0;
		return;
	}

	if (texture_rect != vec4(0.0)) {
		tex_color = fetch_ltc_filtered_texture_with_form_factor(texture_rect, L_unclipped, max_mipmap, area_light_atlas, texture_sampler);
	}

	float I = ltc_integrate_clipped_quad(L, L_proj, n);
	integral = I; // no division by (2.0 * M_PI) for GI calculations
}


// This annotation macro must be placed before any loops that rely on specialization constants as their upper bound.
// Drivers may choose to unroll these loops based on the possible range of the value that can be deduced from the
// spec constant, which can lead to their code generation taking a much longer time than desired.
#ifdef UBERSHADER
// Prefer to not unroll loops on the ubershader to reduce code size as much as possible.
#define SPEC_CONSTANT_LOOP_ANNOTATION [[dont_unroll]]
#else
// Don't make an explicit hint on specialized shaders.
#define SPEC_CONSTANT_LOOP_ANNOTATION
#endif

half D_GGX(half NoH, half roughness, hvec3 n, hvec3 h) {
	half a = NoH * roughness;
#ifdef EXPLICIT_FP16
	hvec3 NxH = cross(n, h);
	half k = roughness / (dot(NxH, NxH) + a * a);
#else
	float k = roughness / (1.0 - NoH * NoH + a * a);
#endif
	half d = k * k * half(1.0 / M_PI);
	return saturateHalf(d);
}

#ifdef LIGHT_TRANSMITTANCE_USED
#ifdef SSS_MODE_SKIN
hvec3 SSS_skin(half NdotL, half transmittance_depth, half transmittance_z, half transmittance_boost, hvec4 transmittance_color, hvec3 light_color) {
	half scale = half(8.25) / transmittance_depth;
	half d = scale * abs(transmittance_z);
	float dd = float(-d * d);
	hvec3 profile = hvec3(vec3(0.233, 0.455, 0.649) * exp(dd / 0.0064) +
			vec3(0.1, 0.336, 0.344) * exp(dd / 0.0484) +
			vec3(0.118, 0.198, 0.0) * exp(dd / 0.187) +
			vec3(0.113, 0.007, 0.007) * exp(dd / 0.567) +
			vec3(0.358, 0.004, 0.0) * exp(dd / 1.99) +
			vec3(0.078, 0.0, 0.0) * exp(dd / 7.41));

	return profile * transmittance_color.a * light_color * clamp(transmittance_boost - NdotL, half(0.0), half(1.0)) * half(1.0 / M_PI);
}
#else
hvec3 SSS(half NdotL, half transmittance_depth, half transmittance_z, half transmittance_boost, hvec4 transmittance_color, hvec3 light_color) {
	half scale = half(8.25) / transmittance_depth;
	half d = scale * abs(transmittance_z);
	half dd = -d * d;
	return exp(dd) * transmittance_color.rgb * transmittance_color.a * light_color * clamp(transmittance_boost - NdotL, half(0.0), half(1.0)) * half(1.0 / M_PI);
}
#endif
#endif

// From Earl Hammon, Jr. "PBR Diffuse Lighting for GGX+Smith Microsurfaces" https://www.gdcvault.com/play/1024478/PBR-Diffuse-Lighting-for-GGX
half V_GGX(half NdotL, half NdotV, half alpha) {
	half v = half(0.5) / mix(half(2.0) * NdotL * NdotV, NdotL + NdotV, alpha);
	return saturateHalf(v);
}

half D_GGX_anisotropic(half cos_theta_m, half alpha_x, half alpha_y, half cos_phi, half sin_phi) {
	half alpha2 = alpha_x * alpha_y;
	vec3 v = vec3(alpha_y * cos_phi, alpha_x * sin_phi, alpha2 * cos_theta_m);
	float v2 = dot(v, v);
	half w2 = half(float(alpha2) / v2);
	return alpha2 * w2 * w2 * half(1.0 / M_PI);
}

half V_GGX_anisotropic(half alpha_x, half alpha_y, half TdotV, half TdotL, half BdotV, half BdotL, half NdotV, half NdotL) {
	half Lambda_V = NdotL * length(hvec3(alpha_x * TdotV, alpha_y * BdotV, NdotV));
	half Lambda_L = NdotV * length(hvec3(alpha_x * TdotL, alpha_y * BdotL, NdotL));
	half v = half(0.5) / (Lambda_V + Lambda_L);
	return saturateHalf(v);
}

half SchlickFresnel(half u) {
	half m = half(1.0) - u;
	half m2 = m * m;
	return m2 * m2 * m; // pow(m,5)
}

hvec3 F0(half metallic, half specular, hvec3 albedo) {
	half dielectric = half(0.16) * specular * specular;
	// use albedo * metallic as colored specular reflectance at 0 angle for metallic materials;
	// see https://google.github.io/filament/Filament.md.html
	return mix(hvec3(dielectric), albedo, hvec3(metallic));
}

half V_Kelemen(half LdotH) {
	// Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
	return saturateHalf(half(0.25) / (LdotH * LdotH + half(1e-4)));
}

hvec3 f0_Clear_Coat_To_Surface(hvec3 f0) {
	// Approximation of iorTof0(f0ToIor(f0), 1.5)
	// This assumes that the clear coat layer has an IOR of 1.5
	// see https://github.com/google/filament/blob/837b2715a05f4656d4f524bce50d1b23ff8f84c9/shaders/src/surface_material.fs#L54-L62
	return clamp(f0 * (f0 * (half(0.941892) - half(0.263008) * f0) + half(0.346479)) - half(0.0285998), half(0.0), half(1.0));
}

void light_compute(hvec3 N, hvec3 L, hvec3 V, half A, hvec3 light_color, bool is_directional, half attenuation, hvec3 f0, half roughness, half metallic, half specular_amount, hvec3 albedo, inout half alpha, vec2 screen_uv, hvec3 energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
		hvec3 backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
		hvec4 transmittance_color,
		half transmittance_depth,
		half transmittance_boost,
		half transmittance_z,
#endif
#ifdef LIGHT_RIM_USED
		half rim, half rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		half clearcoat, half clearcoat_roughness, hvec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		hvec3 B, hvec3 T, half anisotropy,
#endif
		inout hvec3 diffuse_light, inout hvec3 specular_light) {
#if defined(LIGHT_CODE_USED)
	// Light is written by the user shader.
	mat4 inv_view_matrix = transpose(mat4(scene_data_block.data.inv_view_matrix[0],
			scene_data_block.data.inv_view_matrix[1],
			scene_data_block.data.inv_view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));
	mat4 read_view_matrix = transpose(mat4(scene_data_block.data.view_matrix[0],
			scene_data_block.data.view_matrix[1],
			scene_data_block.data.view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#ifdef USING_MOBILE_RENDERER
	uint instance_index = draw_call.instance_index;
#else
	uint instance_index = instance_index_interp;
#endif

	mat4 read_model_matrix = transpose(mat4(instances.data[instance_index].transform[0],
			instances.data[instance_index].transform[1],
			instances.data[instance_index].transform[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#undef projection_matrix
#define projection_matrix scene_data_block.data.projection_matrix
#undef inv_projection_matrix
#define inv_projection_matrix scene_data_block.data.inv_projection_matrix

	vec2 read_viewport_size = scene_data_block.data.viewport_size;

#ifdef LIGHT_BACKLIGHT_USED
	vec3 backlight_highp = vec3(backlight);
#endif
	float roughness_highp = float(roughness);
	float metallic_highp = float(metallic);
	vec3 albedo_highp = vec3(albedo);
	float alpha_highp = float(alpha);
	vec3 normal_highp = vec3(N);
	vec3 light_highp = vec3(L);
	vec3 view_highp = vec3(V);
	float specular_amount_highp = float(specular_amount);
	vec3 light_color_highp = vec3(light_color);
	float attenuation_highp = float(attenuation);
	vec3 diffuse_light_highp = vec3(diffuse_light);
	vec3 specular_light_highp = vec3(specular_light);
	bool is_area = false;
	float area_diffuse = 1.0;
	float area_specular = 1.0;
	vec3 area_diffuse_tex_color = vec3(1.0);
	vec3 area_specular_tex_color = vec3(1.0);



	alpha = half(alpha_highp);
	diffuse_light = hvec3(diffuse_light_highp);
	specular_light = hvec3(specular_light_highp);
#else // !LIGHT_CODE_USED
	half NdotL = min(A + dot(N, L), half(1.0));
	half cNdotV = max(dot(N, V), half(1e-4));

#ifdef LIGHT_TRANSMITTANCE_USED
	{
#ifdef SSS_MODE_SKIN
		diffuse_light += SSS_skin(NdotL, transmittance_depth, transmittance_z, transmittance_boost, transmittance_color, light_color);
#else
		diffuse_light += SSS(NdotL, transmittance_depth, transmittance_z, transmittance_boost, transmittance_color, light_color);
#endif
	}
#endif //LIGHT_TRANSMITTANCE_USED

#if defined(LIGHT_RIM_USED)
	// Epsilon min to prevent pow(0, 0) singularity which results in undefined behavior.
	half rim_light = pow(max(half(1e-4), half(1.0) - cNdotV), max(half(0.0), (half(1.0) - roughness) * half(16.0)));
	diffuse_light += rim_light * rim * mix(hvec3(1.0), albedo, rim_tint) * light_color;
#endif

	half cc_attenuation = half(1.0);

	// We skip checking on attenuation on directional lights to avoid a branch that is not as beneficial for directional lights as the other ones.
	if (is_directional || attenuation > HALF_FLT_MIN) {
		half cNdotL = max(NdotL, half(0.0));
#if defined(DIFFUSE_BURLEY) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_CLEARCOAT_USED)
		hvec3 H = normalize(V + L);
		half cLdotH = clamp(A + dot(L, H), half(0.0), half(1.0));
#endif

#if defined(LIGHT_CLEARCOAT_USED)
		// Clearcoat ignores normal_map, use vertex normal instead
		half ccNdotL = clamp(A + dot(vertex_normal, L), half(0.0), half(1.0));
		half ccNdotH = clamp(A + dot(vertex_normal, H), half(0.0), half(1.0));
		half cLdotH5 = SchlickFresnel(cLdotH);

		half Dr = D_GGX(ccNdotH, half(mix(half(0.001), half(0.1), clearcoat_roughness)), vertex_normal, H);
		half Gr = V_Kelemen(cLdotH);
		half Fr = mix(half(0.04), half(1.0), cLdotH5) * clearcoat;
		cc_attenuation = half(1.0) - Fr;
		half clearcoat_specular_brdf_NL = Dr * Gr * Fr * ccNdotL;
		specular_light += clearcoat_specular_brdf_NL * light_color * attenuation * specular_amount;
#endif // LIGHT_CLEARCOAT_USED

		if (metallic < half(1.0)) {
			half diffuse_brdf_NL; // BRDF times N.L for calculating diffuse radiance

#if defined(DIFFUSE_LAMBERT_WRAP)
			// Energy conserving lambert wrap shader.
			// https://web.archive.org/web/20210228210901/http://blog.stevemcauley.com/2011/12/03/energy-conserving-wrapped-diffuse/
			half op_roughness = half(1.0) + roughness;
			diffuse_brdf_NL = max(half(0.0), (NdotL + roughness) / (op_roughness * op_roughness)) * half(1.0 / M_PI);
#elif defined(DIFFUSE_TOON)

			diffuse_brdf_NL = smoothstep(-roughness, max(roughness, half(0.01)), NdotL) * half(1.0 / M_PI);

#elif defined(DIFFUSE_BURLEY)
			{
				half FD90_minus_1 = half(2.0) * cLdotH * cLdotH * roughness - half(0.5);
				half FdV = half(1.0) + FD90_minus_1 * SchlickFresnel(cNdotV);
				half FdL = half(1.0) + FD90_minus_1 * SchlickFresnel(cNdotL);
				diffuse_brdf_NL = half(1.0 / M_PI) * FdV * FdL * cNdotL;
			}
#else
			// lambert
			diffuse_brdf_NL = cNdotL * half(1.0 / M_PI);
#endif

			diffuse_light += light_color * diffuse_brdf_NL * attenuation * cc_attenuation;

#if defined(LIGHT_BACKLIGHT_USED)
			diffuse_light += light_color * (hvec3(1.0 / M_PI) - diffuse_brdf_NL) * backlight * attenuation;
#endif
		}

		if (roughness > half(0.0)) {
#if defined(SPECULAR_SCHLICK_GGX)
			half cNdotH = clamp(A + dot(N, H), half(0.0), half(1.0));
#endif
			// Apply specular light.
			// FIXME: roughness == 0 should not disable specular light entirely
#if defined(SPECULAR_TOON)
			hvec3 R = normalize(-reflect(L, N));
			half RdotV = dot(R, V);
			half mid = half(1.0) - roughness;
			mid *= mid;
			half intensity = smoothstep(mid - roughness * half(0.5), mid + roughness * half(0.5), RdotV) * mid;
			diffuse_light += light_color * intensity * attenuation * specular_amount; // write to diffuse_light, as in toon shading you generally want no reflection

#elif defined(SPECULAR_DISABLED)
			// Do nothing.

#elif defined(SPECULAR_SCHLICK_GGX)
			// shlick+ggx as default
			half alpha_ggx = roughness * roughness;
#if defined(LIGHT_ANISOTROPY_USED)
			half aspect = sqrt(half(1.0) - anisotropy * half(0.9));
			half ax = alpha_ggx / aspect;
			half ay = alpha_ggx * aspect;
			half XdotH = dot(T, H);
			half YdotH = dot(B, H);
			half D = D_GGX_anisotropic(cNdotH, ax, ay, XdotH, YdotH);
			half G = V_GGX_anisotropic(ax, ay, dot(T, V), dot(T, L), dot(B, V), dot(B, L), cNdotV, cNdotL);
#else // LIGHT_ANISOTROPY_USED
			half D = D_GGX(cNdotH, alpha_ggx, N, H);
			half G = V_GGX(cNdotL, cNdotV, alpha_ggx);
#endif // LIGHT_ANISOTROPY_USED
	   // F
#if !defined(LIGHT_CLEARCOAT_USED)
			half cLdotH5 = SchlickFresnel(cLdotH);
#endif
			// Calculate Fresnel using specular occlusion term from Filament:
			// https://google.github.io/filament/Filament.html#lighting/occlusion/specularocclusion
			half f90 = clamp(dot(f0, hvec3(50.0 * 0.33)), metallic, half(1.0));
			hvec3 F = f0 + (f90 - f0) * cLdotH5;
			hvec3 specular_brdf_NL = energy_compensation * cNdotL * D * F * G;
			specular_light += specular_brdf_NL * light_color * attenuation * cc_attenuation * specular_amount;
#endif
		}

#ifdef USE_SHADOW_TO_OPACITY
		alpha = min(alpha, clamp(half(1.0 - attenuation), half(0.0), half(1.0)));
#endif
	}
#endif // LIGHT_CODE_USED
}

#ifndef SHADOWS_DISABLED

// Interleaved Gradient Noise
// https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float quick_hash(vec2 pos) {
	const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	return fract(magic.z * fract(dot(pos, magic.xy)));
}

half sample_directional_pcf_shadow(texture2D shadow, vec2 shadow_pixel_size, vec4 coord, float taa_frame_count) {
	vec2 pos = coord.xy;
	float depth = coord.z;

	//if only one sample is taken, take it from the center
	if (sc_directional_soft_shadow_samples() == 0) {
		return half(textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos, depth, 1.0)));
	}

	mat2 disk_rotation;
	{
		float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
		float sr = sin(r);
		float cr = cos(r);
		disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
	}

	float avg = 0.0;

	SPEC_CONSTANT_LOOP_ANNOTATION
	for (uint i = 0; i < sc_directional_soft_shadow_samples(); i++) {
		avg += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos + shadow_pixel_size * (disk_rotation * scene_data_block.data.directional_soft_shadow_kernel[i].xy), depth, 1.0));
	}

	return half(avg * (1.0 / float(sc_directional_soft_shadow_samples())));
}

half sample_pcf_shadow(texture2D shadow, vec2 shadow_pixel_size, vec3 coord, float taa_frame_count) {
	vec2 pos = coord.xy;
	float depth = coord.z;

	//if only one sample is taken, take it from the center
	if (sc_soft_shadow_samples() == 0) {
		return half(textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos, depth, 1.0)));
	}

	mat2 disk_rotation;
	{
		float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
		float sr = sin(r);
		float cr = cos(r);
		disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
	}

	float avg = 0.0;

	SPEC_CONSTANT_LOOP_ANNOTATION
	for (uint i = 0; i < sc_soft_shadow_samples(); i++) {
		avg += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos + shadow_pixel_size * (disk_rotation * scene_data_block.data.soft_shadow_kernel[i].xy), depth, 1.0));
	}

	return half(avg * (1.0 / float(sc_soft_shadow_samples())));
}

half sample_omni_pcf_shadow(texture2D shadow, float blur_scale, vec2 coord, vec4 uv_rect, vec2 flip_offset, float depth, float taa_frame_count) {
	//if only one sample is taken, take it from the center
	if (sc_soft_shadow_samples() == 0) {
		vec2 pos = coord * 0.5 + 0.5;
		pos = uv_rect.xy + pos * uv_rect.zw;
		return half(textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos, depth, 1.0)));
	}

	mat2 disk_rotation;
	{
		float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
		float sr = sin(r);
		float cr = cos(r);
		disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
	}

	float avg = 0.0;
	vec2 offset_scale = blur_scale * 2.0 * scene_data_block.data.shadow_atlas_pixel_size / uv_rect.zw;

	SPEC_CONSTANT_LOOP_ANNOTATION
	for (uint i = 0; i < sc_soft_shadow_samples(); i++) {
		vec2 offset = offset_scale * (disk_rotation * scene_data_block.data.soft_shadow_kernel[i].xy);
		vec2 sample_coord = coord + offset;

		float sample_coord_length_squared = dot(sample_coord, sample_coord);
		bool do_flip = sample_coord_length_squared > 1.0;

		if (do_flip) {
			float len = sqrt(sample_coord_length_squared);
			sample_coord = sample_coord * (2.0 / len - 1.0);
		}

		sample_coord = sample_coord * 0.5 + 0.5;
		sample_coord = uv_rect.xy + sample_coord * uv_rect.zw;

		if (do_flip) {
			sample_coord += flip_offset;
		}
		avg += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(sample_coord, depth, 1.0));
	}

	return half(avg * (1.0 / float(sc_soft_shadow_samples())));
}

half sample_directional_soft_shadow(texture2D shadow, vec3 pssm_coord, vec2 tex_scale, float taa_frame_count) {
	//find blocker
	float blocker_count = 0.0;
	float blocker_average = 0.0;

	mat2 disk_rotation;
	{
		float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
		float sr = sin(r);
		float cr = cos(r);
		disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
	}

	SPEC_CONSTANT_LOOP_ANNOTATION
	for (uint i = 0; i < sc_directional_penumbra_shadow_samples(); i++) {
		vec2 suv = pssm_coord.xy + (disk_rotation * scene_data_block.data.directional_penumbra_shadow_kernel[i].xy) * tex_scale;
		float d = textureLod(sampler2D(shadow, SAMPLER_LINEAR_CLAMP), suv, 0.0).r;
		if (d > pssm_coord.z) {
			blocker_average += d;
			blocker_count += 1.0;
		}
	}

	if (blocker_count > 0.0) {
		//blockers found, do soft shadow
		blocker_average /= blocker_count;
		float penumbra = (-pssm_coord.z + blocker_average) / (1.0 - blocker_average);
		tex_scale *= penumbra;

		float s = 0.0;

		SPEC_CONSTANT_LOOP_ANNOTATION
		for (uint i = 0; i < sc_directional_penumbra_shadow_samples(); i++) {
			vec2 suv = pssm_coord.xy + (disk_rotation * scene_data_block.data.directional_penumbra_shadow_kernel[i].xy) * tex_scale;
			s += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(suv, pssm_coord.z, 1.0));
		}

		return half(s / float(sc_directional_penumbra_shadow_samples()));

	} else {
		//no blockers found, so no shadow
		return half(1.0);
	}
}

#endif // SHADOWS_DISABLED

half get_omni_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return half(nd * pow(max(distance, 0.0001), -decay));
}

void light_process_omni(uint idx, vec3 vertex, hvec3 eye_vec, hvec3 normal, vec3 vertex_ddx, vec3 vertex_ddy, hvec3 f0, half roughness, half metallic, float taa_frame_count, hvec3 albedo, inout half alpha, vec2 screen_uv, hvec3 energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
		hvec3 backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
		hvec4 transmittance_color,
		half transmittance_depth,
		half transmittance_boost,
#endif
#ifdef LIGHT_RIM_USED
		half rim, half rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		half clearcoat, half clearcoat_roughness, hvec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		hvec3 binormal, hvec3 tangent, half anisotropy,
#endif
		inout hvec3 diffuse_light, inout hvec3 specular_light) {

	// Omni light attenuation.
	vec3 light_rel_vec = omni_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	half omni_attenuation = get_omni_attenuation(light_length, omni_lights.data[idx].inv_radius, omni_lights.data[idx].attenuation);

	// Compute size.
	half size = half(0.0);
	if (sc_use_light_soft_shadows() && omni_lights.data[idx].size > 0.0) {
		half t = half(omni_lights.data[idx].size / max(0.001, light_length));
		size = half(1.0) / sqrt(half(1.0) + t * t);
		size = max(half(1.0) - size, half(0.0));
	}

	half shadow = half(1.0);
#ifndef SHADOWS_DISABLED
	// Omni light shadow.
	if (omni_attenuation > HALF_FLT_MIN && omni_lights.data[idx].shadow_opacity > 0.001) {
		// there is a shadowmap
		vec2 texel_size = scene_data_block.data.shadow_atlas_pixel_size;
		vec4 base_uv_rect = omni_lights.data[idx].atlas_rect;
		base_uv_rect.xy += texel_size;
		base_uv_rect.zw -= texel_size * 2.0;

		// Omni lights use direction.xy to store to store the offset between the two paraboloid regions
		vec2 flip_offset = omni_lights.data[idx].direction.xy;

		vec3 local_vert = (omni_lights.data[idx].shadow_matrix * vec4(vertex, 1.0)).xyz;

		float shadow_len = length(local_vert); //need to remember shadow len from here
		vec3 shadow_dir = normalize(local_vert);

		vec3 local_normal = normalize(mat3(omni_lights.data[idx].shadow_matrix) * vec3(normal));
		vec3 normal_bias = local_normal * omni_lights.data[idx].shadow_normal_bias * (1.0 - abs(dot(local_normal, shadow_dir)));

		if (sc_use_light_soft_shadows() && omni_lights.data[idx].soft_shadow_size > 0.0) {
			//soft shadow

			//find blocker

			float blocker_count = 0.0;
			float blocker_average = 0.0;

			mat2 disk_rotation;
			{
				float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
				float sr = sin(r);
				float cr = cos(r);
				disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
			}

			vec3 basis_normal = shadow_dir;
			vec3 v0 = abs(basis_normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
			vec3 tangent = normalize(cross(v0, basis_normal));
			vec3 bitangent = normalize(cross(tangent, basis_normal));
			float z_norm = 1.0 - shadow_len * omni_lights.data[idx].inv_radius;

			tangent *= omni_lights.data[idx].soft_shadow_size * omni_lights.data[idx].soft_shadow_scale;
			bitangent *= omni_lights.data[idx].soft_shadow_size * omni_lights.data[idx].soft_shadow_scale;

			SPEC_CONSTANT_LOOP_ANNOTATION
			for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
				vec2 disk = disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy;

				vec3 pos = local_vert + tangent * disk.x + bitangent * disk.y;

				pos = normalize(pos);

				vec4 uv_rect = base_uv_rect;

				if (pos.z >= 0.0) {
					uv_rect.xy += flip_offset;
				}

				pos.z = 1.0 + abs(pos.z);
				pos.xy /= pos.z;

				pos.xy = pos.xy * 0.5 + 0.5;
				pos.xy = uv_rect.xy + pos.xy * uv_rect.zw;

				float d = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), pos.xy, 0.0).r;
				if (d > z_norm) {
					blocker_average += d;
					blocker_count += 1.0;
				}
			}

			if (blocker_count > 0.0) {
				//blockers found, do soft shadow
				blocker_average /= blocker_count;
				float penumbra = (-z_norm + blocker_average) / (1.0 - blocker_average);
				tangent *= penumbra;
				bitangent *= penumbra;

				z_norm += omni_lights.data[idx].inv_radius * omni_lights.data[idx].shadow_bias;

				shadow = half(0.0);

				SPEC_CONSTANT_LOOP_ANNOTATION
				for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
					vec2 disk = disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy;
					vec3 pos = local_vert + tangent * disk.x + bitangent * disk.y;

					pos = normalize(pos);
					pos = normalize(pos + normal_bias);

					vec4 uv_rect = base_uv_rect;

					if (pos.z >= 0.0) {
						uv_rect.xy += flip_offset;
					}

					pos.z = 1.0 + abs(pos.z);
					pos.xy /= pos.z;

					pos.xy = pos.xy * 0.5 + 0.5;
					pos.xy = uv_rect.xy + pos.xy * uv_rect.zw;
					shadow += half(textureProj(sampler2DShadow(shadow_atlas, shadow_sampler), vec4(pos.xy, z_norm, 1.0)));
				}

				shadow /= half(sc_penumbra_shadow_samples());
				shadow = mix(half(1.0), shadow, half(omni_lights.data[idx].shadow_opacity));

			} else {
				//no blockers found, so no shadow
				shadow = half(1.0);
			}
		} else {
			vec4 uv_rect = base_uv_rect;

			vec3 shadow_sample = normalize(shadow_dir + normal_bias);
			if (shadow_sample.z >= 0.0) {
				uv_rect.xy += flip_offset;
				flip_offset *= -1.0;
			}

			shadow_sample.z = 1.0 + abs(shadow_sample.z);
			vec2 pos = shadow_sample.xy / shadow_sample.z;
			float depth = shadow_len - omni_lights.data[idx].shadow_bias;
			depth *= omni_lights.data[idx].inv_radius;
			depth = 1.0 - depth;
			shadow = mix(half(1.0), sample_omni_pcf_shadow(shadow_atlas, omni_lights.data[idx].soft_shadow_scale / shadow_sample.z, pos, uv_rect, flip_offset, depth, taa_frame_count), half(omni_lights.data[idx].shadow_opacity));
		}
	}
#endif

	vec3 color = omni_lights.data[idx].color;

#ifdef LIGHT_TRANSMITTANCE_USED
	half transmittance_z = transmittance_depth; //no transmittance by default
	transmittance_color.a *= omni_attenuation;
#ifndef SHADOWS_DISABLED
	if (omni_lights.data[idx].shadow_opacity > 0.001) {
		// Redo shadowmapping, but shrink the model a bit to avoid artifacts.
		vec2 texel_size = scene_data_block.data.shadow_atlas_pixel_size;
		vec4 uv_rect = omni_lights.data[idx].atlas_rect;
		uv_rect.xy += texel_size;
		uv_rect.zw -= texel_size * 2.0;

		// Omni lights use direction.xy to store to store the offset between the two paraboloid regions
		vec2 flip_offset = omni_lights.data[idx].direction.xy;

		vec3 local_vert = (omni_lights.data[idx].shadow_matrix * vec4(vertex - normal * omni_lights.data[idx].transmittance_bias, 1.0)).xyz;

		float shadow_len = length(local_vert); //need to remember shadow len from here
		vec3 shadow_sample = normalize(local_vert);

		if (shadow_sample.z >= 0.0) {
			uv_rect.xy += flip_offset;
			flip_offset *= -1.0;
		}

		shadow_sample.z = 1.0 + abs(shadow_sample.z);
		vec2 pos = shadow_sample.xy / shadow_sample.z;
		float depth = shadow_len * omni_lights.data[idx].inv_radius;
		depth = 1.0 - depth;

		pos = pos * 0.5 + 0.5;
		pos = uv_rect.xy + pos * uv_rect.zw;
		float shadow_z = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), pos, 0.0).r;
		transmittance_z = half((depth - shadow_z) / omni_lights.data[idx].inv_radius);
	}
#endif // !SHADOWS_DISABLED
#endif // LIGHT_TRANSMITTANCE_USED

	if (sc_use_light_projector() && omni_lights.data[idx].projector_rect != vec4(0.0)) {
		vec3 local_v = (omni_lights.data[idx].shadow_matrix * vec4(vertex, 1.0)).xyz;
		local_v = normalize(local_v);

		vec4 atlas_rect = omni_lights.data[idx].projector_rect;

		if (local_v.z >= 0.0) {
			atlas_rect.y += atlas_rect.w;
		}

		local_v.z = 1.0 + abs(local_v.z);

		local_v.xy /= local_v.z;
		local_v.xy = local_v.xy * 0.5 + 0.5;
		vec2 proj_uv = local_v.xy * atlas_rect.zw;

		// Clamp the uv coordinates to prevent bleeding from neighboring decals.
		vec2 texel_size = 1.0 / vec2(textureSize(sampler2D(decal_atlas_srgb, light_projector_sampler), 0));
		proj_uv = clamp(proj_uv, texel_size * 0.5, atlas_rect.zw - texel_size * 0.5);

		if (sc_projector_use_mipmaps()) {
			vec2 proj_uv_ddx;
			vec2 proj_uv_ddy;
			{
				vec3 local_v_ddx = (omni_lights.data[idx].shadow_matrix * vec4(vertex + vertex_ddx, 1.0)).xyz;
				local_v_ddx = normalize(local_v_ddx);

				if (local_v_ddx.z >= 0.0) {
					local_v_ddx.z += 1.0;
				} else {
					local_v_ddx.z = 1.0 - local_v_ddx.z;
				}

				local_v_ddx.xy /= local_v_ddx.z;
				local_v_ddx.xy = local_v_ddx.xy * 0.5 + 0.5;

				proj_uv_ddx = local_v_ddx.xy * atlas_rect.zw - proj_uv;

				vec3 local_v_ddy = (omni_lights.data[idx].shadow_matrix * vec4(vertex + vertex_ddy, 1.0)).xyz;
				local_v_ddy = normalize(local_v_ddy);

				if (local_v_ddy.z >= 0.0) {
					local_v_ddy.z += 1.0;
				} else {
					local_v_ddy.z = 1.0 - local_v_ddy.z;
				}

				local_v_ddy.xy /= local_v_ddy.z;
				local_v_ddy.xy = local_v_ddy.xy * 0.5 + 0.5;

				proj_uv_ddy = local_v_ddy.xy * atlas_rect.zw - proj_uv;
			}

			vec4 proj = textureGrad(sampler2D(decal_atlas_srgb, light_projector_sampler), proj_uv + atlas_rect.xy, proj_uv_ddx, proj_uv_ddy);
			color *= proj.rgb * proj.a;
		} else {
			vec4 proj = textureLod(sampler2D(decal_atlas_srgb, light_projector_sampler), proj_uv + atlas_rect.xy, 0.0);
			color *= proj.rgb * proj.a;
		}
	}

	vec3 light_rel_vec_norm = light_rel_vec / light_length;
	light_compute(normal, hvec3(light_rel_vec_norm), eye_vec, size, hvec3(color), false, omni_attenuation * shadow, f0, roughness, metallic, half(omni_lights.data[idx].specular_amount), albedo, alpha, screen_uv, energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
			transmittance_color,
			transmittance_depth,
			transmittance_boost,
			transmittance_z,
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

vec2 normal_to_panorama(vec3 n) {
	n = normalize(n);
	vec2 panorama_coords = vec2(atan(n.x, n.z), acos(-n.y));

	if (panorama_coords.x < 0.0) {
		panorama_coords.x += M_PI * 2.0;
	}

	panorama_coords /= vec2(M_PI * 2.0, M_PI);
	return panorama_coords;
}

void light_process_spot(uint idx, vec3 vertex, hvec3 eye_vec, hvec3 normal, vec3 vertex_ddx, vec3 vertex_ddy, hvec3 f0, half roughness, half metallic, float taa_frame_count, hvec3 albedo, inout half alpha, vec2 screen_uv, hvec3 energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
		hvec3 backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
		hvec4 transmittance_color,
		half transmittance_depth,
		half transmittance_boost,
#endif
#ifdef LIGHT_RIM_USED
		half rim, half rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		half clearcoat, half clearcoat_roughness, hvec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		hvec3 binormal, hvec3 tangent, half anisotropy,
#endif
		inout hvec3 diffuse_light,
		inout hvec3 specular_light) {

	// Spot light attenuation.
	vec3 light_rel_vec = spot_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	hvec3 light_rel_vec_norm = hvec3(light_rel_vec / light_length);
	half spot_attenuation = get_omni_attenuation(light_length, spot_lights.data[idx].inv_radius, spot_lights.data[idx].attenuation);
	vec3 spot_dir = spot_lights.data[idx].direction;
	float cone_angle = spot_lights.data[idx].cone_angle;
	float scos = max(dot(-vec3(light_rel_vec_norm), spot_dir), cone_angle);

	// This conversion to a highp float is crucial to prevent light leaking due to precision errors.
	float spot_rim = max(1e-4, (1.0 - scos) / (1.0 - cone_angle));
	spot_attenuation *= half(1.0 - pow(spot_rim, spot_lights.data[idx].cone_attenuation));

	// Compute size.
	half size = half(0.0);
	if (sc_use_light_soft_shadows() && spot_lights.data[idx].size > 0.0) {
		half t = half(spot_lights.data[idx].size / max(0.001, light_length));
		size = half(1.0) / sqrt(half(1.0) + t * t);
		size = max(half(1.0) - size, half(0.0));
	}

	half shadow = half(1.0);
#ifndef SHADOWS_DISABLED
	// Spot light shadow.
	if (spot_attenuation > HALF_FLT_MIN && spot_lights.data[idx].shadow_opacity > 0.001) {
		vec3 normal_bias = vec3(normal) * light_length * spot_lights.data[idx].shadow_normal_bias * (1.0 - abs(dot(normal, light_rel_vec_norm)));

		//there is a shadowmap
		vec4 v = vec4(vertex + normal_bias, 1.0);

		vec4 splane = (spot_lights.data[idx].shadow_matrix * v);
		splane.z += spot_lights.data[idx].shadow_bias;
		splane /= splane.w;

		if (sc_use_light_soft_shadows() && spot_lights.data[idx].soft_shadow_size > 0.0) {
			//soft shadow

			//find blocker
			float z_norm = dot(spot_dir, -light_rel_vec) * spot_lights.data[idx].inv_radius;

			vec2 shadow_uv = splane.xy * spot_lights.data[idx].atlas_rect.zw + spot_lights.data[idx].atlas_rect.xy;

			float blocker_count = 0.0;
			float blocker_average = 0.0;

			mat2 disk_rotation;
			{
				float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
				float sr = sin(r);
				float cr = cos(r);
				disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
			}

			float uv_size = spot_lights.data[idx].soft_shadow_size * z_norm * spot_lights.data[idx].soft_shadow_scale;
			vec2 clamp_max = spot_lights.data[idx].atlas_rect.xy + spot_lights.data[idx].atlas_rect.zw;

			SPEC_CONSTANT_LOOP_ANNOTATION
			for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
				vec2 suv = shadow_uv + (disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy) * uv_size;
				suv = clamp(suv, spot_lights.data[idx].atlas_rect.xy, clamp_max);
				float d = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), suv, 0.0).r;
				if (d > splane.z) {
					blocker_average += d;
					blocker_count += 1.0;
				}
			}

			if (blocker_count > 0.0) {
				//blockers found, do soft shadow
				blocker_average /= blocker_count;
				float penumbra = (-z_norm + blocker_average) / (1.0 - blocker_average);
				uv_size *= penumbra;

				shadow = half(0.0);

				SPEC_CONSTANT_LOOP_ANNOTATION
				for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
					vec2 suv = shadow_uv + (disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy) * uv_size;
					suv = clamp(suv, spot_lights.data[idx].atlas_rect.xy, clamp_max);
					shadow += half(textureProj(sampler2DShadow(shadow_atlas, shadow_sampler), vec4(suv, splane.z, 1.0)));
				}

				shadow /= half(sc_penumbra_shadow_samples());
				shadow = mix(half(1.0), shadow, half(spot_lights.data[idx].shadow_opacity));

			} else {
				//no blockers found, so no shadow
				shadow = half(1.0);
			}
		} else {
			//hard shadow
			vec3 shadow_uv = vec3(splane.xy * spot_lights.data[idx].atlas_rect.zw + spot_lights.data[idx].atlas_rect.xy, splane.z);
			shadow = mix(half(1.0), sample_pcf_shadow(shadow_atlas, spot_lights.data[idx].soft_shadow_scale * scene_data_block.data.shadow_atlas_pixel_size, shadow_uv, taa_frame_count), half(spot_lights.data[idx].shadow_opacity));
		}
	}
#endif // SHADOWS_DISABLED

	vec3 color = spot_lights.data[idx].color;

#ifdef LIGHT_TRANSMITTANCE_USED
	half transmittance_z = transmittance_depth;
	transmittance_color.a *= spot_attenuation;
#ifndef SHADOWS_DISABLED
	if (spot_lights.data[idx].shadow_opacity > 0.001) {
		vec4 splane = (spot_lights.data[idx].shadow_matrix * vec4(vertex - vec3(normal) * spot_lights.data[idx].transmittance_bias, 1.0));
		splane /= splane.w;

		vec3 shadow_uv = vec3(splane.xy * spot_lights.data[idx].atlas_rect.zw + spot_lights.data[idx].atlas_rect.xy, splane.z);
		float shadow_z = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), shadow_uv.xy, 0.0).r;

		shadow_z = shadow_z * 2.0 - 1.0;
		float z_far = 1.0 / spot_lights.data[idx].inv_radius;
		float z_near = 0.01;
		shadow_z = 2.0 * z_near * z_far / (z_far + z_near - shadow_z * (z_far - z_near));

		//distance to light plane
		float z = dot(spot_dir, -light_rel_vec);
		transmittance_z = half(z - shadow_z);
	}
#endif // !SHADOWS_DISABLED
#endif // LIGHT_TRANSMITTANCE_USED

	if (sc_use_light_projector() && spot_lights.data[idx].projector_rect != vec4(0.0)) {
		vec4 splane = (spot_lights.data[idx].shadow_matrix * vec4(vertex, 1.0));
		splane /= splane.w;

		vec2 proj_uv = splane.xy * spot_lights.data[idx].projector_rect.zw;

		if (sc_projector_use_mipmaps()) {
			//ensure we have proper mipmaps
			vec4 splane_ddx = (spot_lights.data[idx].shadow_matrix * vec4(vertex + vertex_ddx, 1.0));
			splane_ddx /= splane_ddx.w;
			vec2 proj_uv_ddx = splane_ddx.xy * spot_lights.data[idx].projector_rect.zw - proj_uv;

			vec4 splane_ddy = (spot_lights.data[idx].shadow_matrix * vec4(vertex + vertex_ddy, 1.0));
			splane_ddy /= splane_ddy.w;
			vec2 proj_uv_ddy = splane_ddy.xy * spot_lights.data[idx].projector_rect.zw - proj_uv;

			vec4 proj = textureGrad(sampler2D(decal_atlas_srgb, light_projector_sampler), proj_uv + spot_lights.data[idx].projector_rect.xy, proj_uv_ddx, proj_uv_ddy);
			color *= proj.rgb * proj.a;
		} else {
			vec4 proj = textureLod(sampler2D(decal_atlas_srgb, light_projector_sampler), proj_uv + spot_lights.data[idx].projector_rect.xy, 0.0);
			color *= proj.rgb * proj.a;
		}
	}

	light_compute(normal, hvec3(light_rel_vec_norm), eye_vec, size, hvec3(color), false, spot_attenuation * shadow, f0, roughness, metallic, half(spot_lights.data[idx].specular_amount), albedo, alpha, screen_uv, energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
			backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
			transmittance_color,
			transmittance_depth,
			transmittance_boost,
			transmittance_z,
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

// implementation of area lights with Linearly Transformed Cosines (LTC): https://eheitzresearch.wordpress.com/415-2/
void light_process_area(uint idx, vec3 vertex, hvec3 eye_vec, hvec3 normal, vec3 vertex_ddx, vec3 vertex_ddy, hvec3 f0, half roughness, half metallic, float taa_frame_count, hvec3 albedo, inout half alpha, vec2 screen_uv, hvec3 energy_compensation,
#ifdef LIGHT_BACKLIGHT_USED
		hvec3 backlight,
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
		hvec4 transmittance_color,
		half transmittance_depth,
		half transmittance_boost,
#endif
#ifdef LIGHT_RIM_USED
		half rim, half rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
		half clearcoat, half clearcoat_roughness, hvec3 vertex_normal,
#endif
#ifdef LIGHT_ANISOTROPY_USED
		hvec3 binormal, hvec3 tangent, half anisotropy,
#endif
		inout hvec3 diffuse_light, inout hvec3 specular_light) {
	half EPSILON = half(1e-7);
	hvec3 area_width = hvec3(area_lights.data[idx].area_width);
	hvec3 area_height = hvec3(area_lights.data[idx].area_height);

	if (dot(area_width, area_width) < EPSILON || dot(area_height, area_height) < EPSILON) { // area is 0
		return;
	}

	if (dot(area_lights.data[idx].direction, vertex - area_lights.data[idx].position) <= 0) {
		return; // vertex is behind light
	}

	half a_len = length(area_width);
	half b_len = length(area_height);
	half a_half_len = a_len / half(2.0);
	half b_half_len = b_len / half(2.0);
	hvec3 light_center = hvec3(area_lights.data[idx].position);
	hvec3 light_to_vert = hvec3(vertex) - light_center;
	hvec3 area_a_dir = normalize(area_width);
	hvec3 area_b_dir = normalize(area_height);
	hvec3 area_direction = hvec3(area_lights.data[idx].direction);
	hvec3 pos_local_to_light = hvec3(dot(light_to_vert, area_a_dir), dot(light_to_vert, area_b_dir), dot(light_to_vert, -area_direction));

	hvec3 closest_point_local_to_light = hvec3(clamp(pos_local_to_light.x, -a_half_len, a_half_len), clamp(pos_local_to_light.y, -b_half_len, b_half_len), 0);
	half dist = length(closest_point_local_to_light - pos_local_to_light);

	half light_length = max(half(0.0), dist);
	half light_attenuation_raw = get_omni_attenuation(float(light_length), area_lights.data[idx].inv_radius, area_lights.data[idx].attenuation);
	half light_attenuation_ltc = light_attenuation_raw * half(light_length * light_length); // solid angle already decreases by inverse square, so attenuation power is 2.0 by default -> subtract 2.0
	half shadow = half(1.0);

#ifndef SHADOWS_DISABLED
	// Area light shadow.
	if (light_attenuation_raw > HALF_FLT_MIN && area_lights.data[idx].shadow_opacity > 0.001) {
		// there is a shadowmap
		vec2 texel_size = scene_data_block.data.shadow_atlas_pixel_size;
		vec4 base_uv_rect = area_lights.data[idx].atlas_rect;
		base_uv_rect.xy += texel_size;
		base_uv_rect.zw -= texel_size * 2.0;

		vec3 local_vert = (area_lights.data[idx].shadow_matrix * vec4(vertex, 1.0)).xyz;

		float shadow_len = length(local_vert); //need to remember shadow len from here
		vec3 shadow_dir = normalize(local_vert);

		vec3 local_normal = normalize(mat3(area_lights.data[idx].shadow_matrix) * vec3(normal));
		vec3 normal_bias = local_normal * area_lights.data[idx].shadow_normal_bias * (1.0 - abs(dot(local_normal, shadow_dir)));

		float inv_center_range = area_lights.data[idx].cone_attenuation;

		if (sc_use_light_soft_shadows() && area_lights.data[idx].soft_shadow_size > 0.0) {
			//soft shadow

			//find blocker

			float blocker_count = 0.0;
			float blocker_average = 0.0;

			mat2 disk_rotation;
			{
				float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
				float sr = sin(r);
				float cr = cos(r);
				disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
			}

			vec3 basis_normal = shadow_dir;
			vec3 v0 = abs(basis_normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
			vec3 tangent = normalize(cross(v0, basis_normal));
			vec3 bitangent = normalize(cross(tangent, basis_normal));
			float z_norm = 1.0 - shadow_len * inv_center_range;

			tangent *= area_lights.data[idx].soft_shadow_size * area_lights.data[idx].soft_shadow_scale;
			bitangent *= area_lights.data[idx].soft_shadow_size * area_lights.data[idx].soft_shadow_scale;

			SPEC_CONSTANT_LOOP_ANNOTATION
			for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
				vec2 disk = disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy;

				vec3 pos = local_vert + tangent * disk.x + bitangent * disk.y;

				pos = normalize(pos);

				vec4 uv_rect = base_uv_rect;

				pos.z = 1.0 + abs(pos.z);
				pos.xy /= pos.z;

				pos.xy = pos.xy * 0.5 + 0.5;
				pos.xy = uv_rect.xy + pos.xy * uv_rect.zw;

				float d = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), pos.xy, 0.0).r;
				if (d > z_norm) {
					blocker_average += d;
					blocker_count += 1.0;
				}
			}

			if (blocker_count > 0.0) {
				//blockers found, do soft shadow
				blocker_average /= blocker_count;
				float penumbra = (-z_norm + blocker_average) / (1.0 - blocker_average);
				tangent *= penumbra;
				bitangent *= penumbra;

				z_norm += inv_center_range * area_lights.data[idx].shadow_bias;

				shadow = half(0.0);

				SPEC_CONSTANT_LOOP_ANNOTATION
				for (uint i = 0; i < sc_penumbra_shadow_samples(); i++) {
					vec2 disk = disk_rotation * scene_data_block.data.penumbra_shadow_kernel[i].xy;
					vec3 pos = local_vert + tangent * disk.x + bitangent * disk.y;

					pos = normalize(pos);
					pos = normalize(pos + normal_bias);

					vec4 uv_rect = base_uv_rect;

					pos.z = 1.0 + abs(pos.z);
					pos.xy /= pos.z;

					pos.xy = pos.xy * 0.5 + 0.5;
					pos.xy = uv_rect.xy + pos.xy * uv_rect.zw;
					shadow += half(textureProj(sampler2DShadow(shadow_atlas, shadow_sampler), vec4(pos.xy, z_norm, 1.0)));
				}

				shadow /= half(sc_penumbra_shadow_samples());
				shadow = mix(half(1.0), shadow, half(area_lights.data[idx].shadow_opacity));

			} else {
				//no blockers found, so no shadow
				shadow = half(1.0);
			}
		} else {
			vec4 uv_rect = base_uv_rect;

			vec3 shadow_sample = normalize(shadow_dir + normal_bias);

			shadow_sample.z = 1.0 + abs(shadow_sample.z);
			vec2 pos = shadow_sample.xy / shadow_sample.z;
			float depth = shadow_len - area_lights.data[idx].shadow_bias;
			depth *= inv_center_range;
			depth = 1.0 - depth;
			shadow = mix(half(1.0), sample_omni_pcf_shadow(shadow_atlas, area_lights.data[idx].soft_shadow_scale / shadow_sample.z, pos, uv_rect, vec2(0), depth, taa_frame_count), half(area_lights.data[idx].shadow_opacity));
		}
	}
#endif
	light_attenuation_ltc = light_attenuation_ltc * shadow;
	half light_attenuation = light_attenuation_raw * shadow;
	hvec3 color = hvec3(area_lights.data[idx].color);
	float max_mipmap = area_lights.data[idx].cone_angle;

	vec3 points[4];
	hvec3 h_area_width = area_width / half(2.0);
	hvec3 h_area_height = area_height / half(2.0);
	points[0] = area_lights.data[idx].position - h_area_width - h_area_height - vertex;
	points[1] = area_lights.data[idx].position + h_area_width - h_area_height - vertex;
	points[2] = area_lights.data[idx].position + h_area_width + h_area_height - vertex;
	points[3] = area_lights.data[idx].position - h_area_width + h_area_height - vertex;

	float ltc_diffuse = 0.0;
	vec3 ltc_diffuse_tex_color = vec3(1.0);
	float ltc_specular = 0.0;
	vec3 ltc_specular_tex_color = vec3(1.0);
	vec2 ltc_fresnel = vec2(0.0);
	hvec3 fresnel_color = hvec3(0.0);
	ltc_evaluate(vec3(normal), vec3(eye_vec), mat3(1), points, area_lights.data[idx].projector_rect, max_mipmap, area_light_atlas, SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP, ltc_diffuse, ltc_diffuse_tex_color);

#if !defined(SPECULAR_DISABLED) || (defined(LIGHT_CODE_USED) && defined(AREA_LIGHT_CODE_USED))
	ltc_evaluate_specular(vec3(normal), vec3(eye_vec), roughness, points, area_lights.data[idx].projector_rect, max_mipmap, area_light_atlas, SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP, SAMPLER_LINEAR_CLAMP, ltc_lut1, ltc_lut2, ltc_specular, ltc_fresnel, ltc_specular_tex_color);
	half f90 = clamp(dot(f0, hvec3(50.0 * 0.33)), metallic, half(1.0));
	fresnel_color = f0 * max(half(ltc_fresnel.x), half(0.0)) + (f90 - f0) * max(half(ltc_fresnel.y), half(0.0));
#endif

#if defined(LIGHT_CODE_USED) && defined(AREA_LIGHT_CODE_USED)
	// Light is written by the user shader.
	mat4 inv_view_matrix = transpose(mat4(scene_data_block.data.inv_view_matrix[0],
			scene_data_block.data.inv_view_matrix[1],
			scene_data_block.data.inv_view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));
	mat4 read_view_matrix = transpose(mat4(scene_data_block.data.view_matrix[0],
			scene_data_block.data.view_matrix[1],
			scene_data_block.data.view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#ifdef USING_MOBILE_RENDERER
	uint instance_index = draw_call.instance_index;
#else
	uint instance_index = instance_index_interp;
#endif

	mat4 read_model_matrix = transpose(mat4(instances.data[instance_index].transform[0],
			instances.data[instance_index].transform[1],
			instances.data[instance_index].transform[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#undef projection_matrix
#define projection_matrix scene_data_block.data.projection_matrix
#undef inv_projection_matrix
#define inv_projection_matrix scene_data_block.data.inv_projection_matrix

	vec2 read_viewport_size = scene_data_block.data.viewport_size;

#ifdef LIGHT_BACKLIGHT_USED
	vec3 backlight_highp = vec3(backlight);
#endif
	float roughness_highp = float(roughness);
	float metallic_highp = float(metallic);
	vec3 albedo_highp = vec3(albedo);
	float alpha_highp = float(alpha);
	vec3 normal_highp = vec3(normal);
	vec3 light_highp = (light_center - vertex) / light_length;
	vec3 view_highp = vec3(eye_vec);
	float specular_amount_highp = float(area_lights.data[idx].specular_amount);
	vec3 light_color_highp = vec3(color);
	float attenuation_highp = float(light_attenuation_ltc);
	vec3 diffuse_light_highp = vec3(diffuse_light);
	vec3 specular_light_highp = vec3(specular_light);
	bool is_directional = false;
	bool is_area = true;
	vec3 area_diffuse = ltc_diffuse * ltc_diffuse_tex_color;
	vec3 area_specular = ltc_specular * vec3(fresnel_color) * ltc_specular_tex_color;



	alpha = half(alpha_highp);
	diffuse_light = hvec3(diffuse_light_highp);
	specular_light = hvec3(specular_light_highp);

#else
	half specular_amount = half(area_lights.data[idx].specular_amount);
	half area = a_len * b_len;
	half cc_attenuation = half(1.0);

#if defined(LIGHT_TRANSMITTANCE_USED) || defined(LIGHT_BACKLIGHT_USED) || defined(LIGHT_RIM_USED) || defined(DIFFUSE_TOON)
	hvec3 isotropic_light_color = hvec3(1.0); // independent of normal
	if (area_lights.data[idx].projector_rect != vec4(0.0)) {
		half lod = dist / sqrt(area);
		lod = log(half(2048.0) * lod) / log(half(3.0));

		hvec2 uv = (closest_point_local_to_light.xy + hvec2(a_half_len, b_half_len)) / hvec2(a_len, b_len);
		isotropic_light_color = hvec3(fetch_ltc_lod(vec2(hvec2(1.0) - uv), area_lights.data[idx].projector_rect, float(lod), max_mipmap, area_light_atlas, SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP));
	}
#endif
#ifdef LIGHT_TRANSMITTANCE_USED
	{
		half transmittance_z = transmittance_depth;
		transmittance_color.a *= light_attenuation_raw; // not including shadows or ltc falloff
#ifndef SHADOWS_DISABLED
		if (area_lights.data[idx].shadow_opacity > 0.001) {
			// Redo shadowmapping, but shrink the model a bit to avoid artifacts.
			vec2 texel_size = scene_data_block.data.shadow_atlas_pixel_size;
			vec4 uv_rect = area_lights.data[idx].atlas_rect;
			uv_rect.xy += texel_size;
			uv_rect.zw -= texel_size * 2.0;
			vec3 local_vert = (area_lights.data[idx].shadow_matrix * vec4(vertex - vec3(normal) * area_lights.data[idx].transmittance_bias, 1.0)).xyz;

			float shadow_len = length(local_vert); //need to remember shadow len from here
			vec3 shadow_sample = normalize(local_vert);

			shadow_sample.z = 1.0 + abs(shadow_sample.z);
			vec2 pos = shadow_sample.xy / shadow_sample.z;
			float inv_center_range = area_lights.data[idx].cone_attenuation;
			float depth = shadow_len * inv_center_range;
			depth = 1.0 - depth;

			pos = pos * 0.5 + 0.5;
			pos = uv_rect.xy + pos * uv_rect.zw;
			float shadow_z = textureLod(sampler2D(shadow_atlas, SAMPLER_LINEAR_CLAMP), pos, 0.0).r;
			transmittance_z = half((depth - shadow_z) / inv_center_range);
		}
#endif
		// transmission can't use ltc_diffuse_tex_color, because for backface pixels texture would have to be sampled for opposite normal direction.
#ifdef SSS_MODE_SKIN
		diffuse_light += SSS_skin(half(ltc_diffuse), transmittance_depth, transmittance_z, transmittance_boost, transmittance_color, color * isotropic_light_color * area);
#else
		diffuse_light += SSS(half(ltc_diffuse), transmittance_depth, transmittance_z, transmittance_boost, transmittance_color, color * isotropic_light_color * area);
#endif
	}
#endif //LIGHT_TRANSMITTANCE_USED

#if defined(LIGHT_BACKLIGHT_USED) || defined(LIGHT_RIM_USED) || defined(DIFFUSE_TOON) || defined(SPECULAR_TOON)
	float solid_angle = quad_solid_angle(points);
#endif

#if defined(LIGHT_CLEARCOAT_USED)
	vec3 cc_specular_tex_color = vec3(1.0);
	float cc_specular_ltc = 0.0;
	vec2 cc_fresnel;
	ltc_evaluate_specular(vec3(vertex_normal), vec3(eye_vec), sqrt(mix(0.001, 0.1, float(clearcoat_roughness))), points, area_lights.data[idx].projector_rect, max_mipmap, area_light_atlas, SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP, SAMPLER_LINEAR_CLAMP, ltc_lut1, ltc_lut2, cc_specular_ltc, cc_fresnel, cc_specular_tex_color);
	half Fr = (half(0.04) * max(half(cc_fresnel.x), half(0.0)) + half(1.0 - 0.04) * max(half(cc_fresnel.y), half(0.0))) * clearcoat;
	cc_attenuation = half(1.0) - Fr;
	specular_light += half(cc_specular_ltc) * hvec3(cc_specular_tex_color) * Fr * color * light_attenuation_ltc * specular_amount;
#endif // LIGHT_CLEARCOAT_USED

	if (metallic < half(1.0)) {
#if defined(DIFFUSE_TOON)
		float backface_ltc_diffuse = 0.0;
		vec3 backface_ltc_tex_color_discard = vec3(1.0);
		ltc_evaluate(vec3(-normal), vec3(eye_vec), mat3(1), points, vec4(0.0), max_mipmap, area_light_atlas, SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP, backface_ltc_diffuse, backface_ltc_tex_color_discard);
		half NdotL = half((ltc_diffuse - backface_ltc_diffuse) / (max(solid_angle, 0.001) / M_PI));
		half diffuse_brdf_NL = smoothstep(-roughness, max(roughness, half(0.01)), NdotL) * half(1.0 / M_PI);
		diffuse_light += diffuse_brdf_NL * isotropic_light_color * color * area * light_attenuation * cc_attenuation;
#else
		diffuse_light += half(ltc_diffuse) * hvec3(ltc_diffuse_tex_color) * color * light_attenuation_ltc * cc_attenuation;
#endif // DIFFUSE_TOON

#if defined(LIGHT_BACKLIGHT_USED)
		// backlight can't use ltc_diffuse_tex_color, because for backface pixels texture would have to sampled for opposite normal direction.
		diffuse_light += color * max(half(solid_angle / M_PI) - half(ltc_diffuse), half(0.0)) * backlight * isotropic_light_color * light_attenuation_ltc;
#endif
	}

#if defined(LIGHT_RIM_USED) // same as for point lights
	half cNdotV = max(dot(normal, eye_vec), half(1e-4));
	half rim_light = pow(max(half(1e-4), half(1.0) - cNdotV), max(half(0.0), (half(1.0) - roughness) * half(16.0)));
	diffuse_light += rim_light * rim * mix(hvec3(1.0), albedo, rim_tint) * isotropic_light_color * color * half(solid_angle) * light_attenuation_ltc;
#endif

#if defined(SPECULAR_TOON)
	// If ltc_specular turns out to be not similar enough to RdotV, since its based on GGX, toon shading would need its own lookup-table.
	half mid = half(1.0) - roughness;
	mid *= mid;

	half RdotV = half(ltc_specular / (max(solid_angle, 0.001) / (M_PI)));
	half intensity = smoothstep(mid - roughness * half(0.5), mid + roughness * half(0.5), RdotV) * mid; // should we use specular tex color here?? or diffuse? or white?
	diffuse_light += intensity * hvec3(ltc_specular_tex_color) * color * area * light_attenuation * specular_amount; // write to diffuse_light, as in toon shading you generally want no reflection
#elif defined(SPECULAR_DISABLED)
	// do nothing
#else
	hvec3 spec = half(ltc_specular) * hvec3(ltc_specular_tex_color) * color * fresnel_color;
	specular_light += spec * specular_amount * light_attenuation_ltc * cc_attenuation;
#endif // SPECULAR_TOON

#endif // LIGHT_CODE_USED
}

void reflection_process(uint ref_index, vec3 vertex, hvec3 ref_vec, hvec3 normal, half roughness, hvec3 ambient_light,
#ifdef LIGHT_CLEARCOAT_USED
		hvec3 cc_ref_vec, half cc_roughness, inout hvec4 cc_reflection_accum,
#endif
		inout hvec4 ambient_accum, inout hvec4 reflection_accum) {
	vec3 box_extents = reflections.data[ref_index].box_extents;
	vec3 local_pos = (reflections.data[ref_index].local_matrix * vec4(vertex, 1.0)).xyz;

	if (any(greaterThan(abs(local_pos), box_extents))) { //out of the reflection box
		return;
	}

	half blend = half(1.0);
	if (reflections.data[ref_index].blend_distance != 0.0) {
		vec3 axis_blend_distance = min(vec3(reflections.data[ref_index].blend_distance), box_extents);
		vec3 blend_axes_highp = abs(local_pos) - box_extents + axis_blend_distance;
		hvec3 blend_axes = hvec3(blend_axes_highp / axis_blend_distance);
		blend_axes = clamp(half(1.0) - blend_axes, hvec3(0.0), hvec3(1.0));
		blend = pow(blend_axes.x * blend_axes.y * blend_axes.z, half(2.0));
	}

	vec2 border_size = scene_data_block.data.reflection_atlas_border_size;
	if (reflections.data[ref_index].intensity > 0.0 && reflection_accum.a < half(1.0)) { // compute reflection

		vec3 local_ref_vec = (reflections.data[ref_index].local_matrix * vec4(ref_vec, 0.0)).xyz;

		if (reflections.data[ref_index].box_project) { //box project

			vec3 nrdir = normalize(local_ref_vec);
			vec3 rbmax = (box_extents - local_pos) / nrdir;
			vec3 rbmin = (-box_extents - local_pos) / nrdir;

			vec3 rbminmax = mix(rbmin, rbmax, greaterThan(nrdir, vec3(0.0, 0.0, 0.0)));

			float fa = min(min(rbminmax.x, rbminmax.y), rbminmax.z);
			vec3 posonbox = local_pos + nrdir * fa;
			local_ref_vec = posonbox - reflections.data[ref_index].box_offset;
		}

		hvec4 reflection;
		half reflection_blend = max(half(0.0), blend - reflection_accum.a);

		float roughness_lod = sqrt(roughness) * MAX_ROUGHNESS_LOD;
		vec2 reflection_uv = vec3_to_oct_with_border(local_ref_vec, border_size);
		reflection.rgb = hvec3(textureLod(sampler2DArray(reflection_atlas, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), vec3(reflection_uv, reflections.data[ref_index].index), roughness_lod).rgb) * REFLECTION_MULTIPLIER;
		reflection.rgb *= half(reflections.data[ref_index].exposure_normalization);
		reflection.a = reflection_blend;

		reflection.rgb *= half(reflections.data[ref_index].intensity);
		reflection.rgb *= reflection.a;

		reflection_accum += reflection;
	}

#ifdef LIGHT_CLEARCOAT_USED
	if (reflections.data[ref_index].intensity > 0.0 && cc_reflection_accum.a < half(1.0)) { // compute clearcoat reflection
		vec3 local_cc_ref_vec = (reflections.data[ref_index].local_matrix * vec4(cc_ref_vec, 0.0)).xyz;

		if (reflections.data[ref_index].box_project) { // Box project.

			vec3 nrdir = normalize(local_cc_ref_vec);
			vec3 rbmax = (box_extents - local_pos) / nrdir;
			vec3 rbmin = (-box_extents - local_pos) / nrdir;

			vec3 rbminmax = mix(rbmin, rbmax, greaterThan(nrdir, vec3(0.0, 0.0, 0.0)));

			float fa = min(min(rbminmax.x, rbminmax.y), rbminmax.z);
			vec3 posonbox = local_pos + nrdir * fa;
			local_cc_ref_vec = posonbox - reflections.data[ref_index].box_offset;
		}

		hvec4 cc_reflection;
		half cc_reflection_blend = max(half(0.0), blend - cc_reflection_accum.a);

		float cc_roughness_lod = sqrt(cc_roughness) * MAX_ROUGHNESS_LOD;
		vec2 cc_reflection_uv = vec3_to_oct_with_border(local_cc_ref_vec, border_size);
		cc_reflection.rgb = hvec3(textureLod(sampler2DArray(reflection_atlas, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), vec3(cc_reflection_uv, reflections.data[ref_index].index), cc_roughness_lod).rgb) * REFLECTION_MULTIPLIER;
		cc_reflection.rgb *= half(reflections.data[ref_index].exposure_normalization);
		cc_reflection.a = cc_reflection_blend;

		cc_reflection.rgb *= half(reflections.data[ref_index].intensity);
		cc_reflection.rgb *= cc_reflection.a;

		cc_reflection_accum += cc_reflection;
	}
#endif // LIGHT_CLEARCOAT_USED

	if (ambient_accum.a >= half(1.0)) {
		return;
	}

	switch (reflections.data[ref_index].ambient_mode) {
		case REFLECTION_AMBIENT_DISABLED: {
			//do nothing
		} break;
		case REFLECTION_AMBIENT_ENVIRONMENT: {
			vec3 local_amb_vec = (reflections.data[ref_index].local_matrix * vec4(normal, 0.0)).xyz;
			hvec4 ambient_out;
			half ambient_blend = max(half(0.0), blend - ambient_accum.a);

			float roughness_lod = MAX_ROUGHNESS_LOD;
			vec2 ambient_uv = vec3_to_oct_with_border(local_amb_vec, border_size);
			ambient_out.rgb = hvec3(textureLod(sampler2DArray(reflection_atlas, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), vec3(ambient_uv, reflections.data[ref_index].index), roughness_lod).rgb);
			ambient_out.rgb *= half(reflections.data[ref_index].exposure_normalization);
			ambient_out.a = ambient_blend;
			ambient_out.rgb *= ambient_out.a;
			ambient_accum += ambient_out;
		} break;
		case REFLECTION_AMBIENT_COLOR: {
			hvec4 ambient_out;
			half ambient_blend = max(half(0.0), blend - ambient_accum.a);

			ambient_out.rgb = hvec3(reflections.data[ref_index].ambient);
			ambient_out.a = ambient_blend;
			ambient_out.rgb *= ambient_out.a;
			ambient_accum += ambient_out;
		} break;
	}
}

half blur_shadow(half shadow) {
	return shadow;
#if 0
	//disabling for now, will investigate later
	float interp_shadow = shadow;
	if (gl_HelperInvocation) {
		interp_shadow = -4.0; // technically anything below -4 will do but just to make sure
	}

	uvec2 fc2 = uvec2(gl_FragCoord.xy);
	interp_shadow -= dFdx(interp_shadow) * (float(fc2.x & 1) - 0.5);
	interp_shadow -= dFdy(interp_shadow) * (float(fc2.y & 1) - 0.5);

	if (interp_shadow >= 0.0) {
		shadow = interp_shadow;
	}
	return shadow;
#endif
}


#endif //!defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && !defined(USE_VERTEX_LIGHTING)

#ifndef MODE_RENDER_DEPTH

/*
	Only supporting normal fog here.
*/
// Apply each completed map's capture orientation before octahedral encoding.
// Layer, derivative LOD and border semantics remain identical to native sampling.
vec4 sample_sky_radiance(vec3 direction, float layer, float lod) {
	uint fogged_maps = uint(scene_data_block.data.sky_capture_data.w);
	vec3 old_direction = (fogged_maps & 1u) != 0u ? scene_data_block.data.sky_capture_old_xform * direction : direction;
	vec2 border = vec2(scene_data_block.data.radiance_border_size, 1.0 - 2.0 * scene_data_block.data.radiance_border_size);
	vec2 old_uv = vec3_to_oct_with_border(old_direction, border);
#ifdef USE_RADIANCE_OCTMAP_ARRAY
	vec4 old_sample = textureLod(sampler2DArray(radiance_octmap, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), vec3(old_uv, layer), lod);
#else
	vec4 old_sample = textureLod(sampler2D(radiance_octmap, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), old_uv, lod);
#endif
	if (scene_data_block.data.sky_capture_data.x == 0.0) { return old_sample; }
	vec3 old_rgb = scene_data_block.data.sky_capture_data.z > 0.0 ? old_sample.rgb : scene_data_block.data.sky_capture_fallback.rgb;
	vec3 result = old_rgb;
	if (scene_data_block.data.sky_capture_data.y > 0.0) {
		vec3 next_direction = (fogged_maps & 2u) != 0u ? scene_data_block.data.sky_capture_next_xform * direction : direction;
		vec2 next_uv = vec3_to_oct_with_border(next_direction, border);
#ifdef USE_RADIANCE_OCTMAP_ARRAY
		vec3 next_rgb = textureLod(sampler2DArray(radiance_octmap_next, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), vec3(next_uv, layer), lod).rgb;
#else
		vec3 next_rgb = textureLod(sampler2D(radiance_octmap_next, DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP), next_uv, lod).rgb;
#endif
		result = mix(old_rgb, next_rgb, scene_data_block.data.sky_capture_data.y);
	}
	return vec4(result, 1.0);
}

bool native_ap_enabled() {
#ifdef USE_MULTIVIEW
 return scene_data.aerial_perspective[ViewIndex] > 0.0;
#else
 return scene_data.aerial_perspective.x > 0.0;
#endif
}
vec4 native_ap_process(vec3 vertex) {
 vec3 origin_view=vec3(0.0);
#ifdef USE_MULTIVIEW
 origin_view=scene_data.eye_offset[ViewIndex].xyz;
#endif
 vec2 uv=gl_FragCoord.xy*scene_data.screen_pixel_size;
 if(scene_data.height_fog_view.x>0.0) {
  vec2 ndc=uv*2.0-1.0;
  vec4 a=inv_projection_matrix*vec4(ndc,1.0,1.0);
  vec4 b=inv_projection_matrix*vec4(ndc,0.5,1.0);
  vec3 near_p=a.xyz/a.w;
  vec3 ray=b.xyz/b.w-near_p;
  origin_view+=near_p-ray*(near_p.z/ray.z);
 }
 float distance=length(vertex-origin_view);
 if(distance<=100.0) return vec4(0.0,0.0,0.0,1.0);
 // Sqrt locates the quadratic slice interval. Interpolate transport by
 // actual path distance inside it, not sqrt(distance): the latter biases
 // near-field radiance even when both cumulative slice integrals are exact.
 float depth=clamp(distance,100.0,96000.0);
 float slice=min(floor(sqrt((depth-100.0)/95900.0)*15.0),14.0);
 float d0=100.0+95900.0*(slice/15.0)*(slice/15.0);
 float d1=100.0+95900.0*((slice+1.0)/15.0)*((slice+1.0)/15.0);
 float weight=clamp((depth-d0)/(d1-d0),0.0,1.0);
 vec3 coord=vec3(clamp(uv,vec2(0.5/32.0),vec2(31.5/32.0)),(0.5+slice+weight)/16.0);
#ifdef USE_MULTIVIEW
 if(ViewIndex==1) return textureLod(sampler3D(aerial_perspective_volume_eye1,DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP),coord,0.0);
#endif
 return textureLod(sampler3D(aerial_perspective_volume,DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP),coord,0.0);
}

vec4 native_height_fog_process(vec3 vertex) {
 vec3 origin_view = vec3(0.0);
#ifdef USE_MULTIVIEW
 origin_view = scene_data.eye_offset[ViewIndex].xyz;
#endif
 if (scene_data.height_fog_view.x > 0.0) {
  vec2 ndc = gl_FragCoord.xy * scene_data.screen_pixel_size * 2.0 - 1.0;
  vec4 near_h = inv_projection_matrix * vec4(ndc, 1.0, 1.0);
  vec4 far_h = inv_projection_matrix * vec4(ndc, 0.0, 1.0);
  vec3 near_point = near_h.xyz / near_h.w;
  vec3 parallel_ray = far_h.xyz / far_h.w - near_point;
  origin_view += near_point - parallel_ray * (near_point.z / parallel_ray.z);
 }
 mat4 world_from_view = transpose(mat4(scene_data.inv_view_matrix[0], scene_data.inv_view_matrix[1], scene_data.inv_view_matrix[2], vec4(0.0, 0.0, 0.0, 1.0)));
 vec3 segment = vertex - origin_view;
 float distance_to_surface = length(segment);
 vec3 view_ray = distance_to_surface > 0.0 ? segment / distance_to_surface : vec3(0.0, 0.0, -1.0);
 vec3 ray = normalize(mat3(world_from_view) * view_ray);
 vec3 origin = (world_from_view * vec4(origin_view, 1.0)).xyz;
 bool capture_ready = scene_data.height_fog_view.y == 0.0 && scene_data.height_fog_view.z > 0.0;
 vec3 capture_color = vec3(0.0);
 if (capture_ready && scene_data.height_fog.source.w > 0.0) {
  vec3 radiance_ray = scene_data.radiance_inverse_xform * view_ray;
  float roughness_layer = scene_data.height_fog.options.x * MAX_ROUGHNESS_LOD;
#ifdef USE_RADIANCE_OCTMAP_ARRAY
  float low_layer = floor(roughness_layer);
  vec3 low_color = sample_sky_radiance(radiance_ray, low_layer, 0.0).rgb;
  vec3 high_color = sample_sky_radiance(radiance_ray, min(low_layer + 1.0, MAX_ROUGHNESS_LOD), 0.0).rgb;
  capture_color = mix(low_color, high_color, fract(roughness_layer));
#else
  capture_color = sample_sky_radiance(radiance_ray, 0.0, roughness_layer).rgb;
#endif
 }
 return height_fog_integrate(scene_data.height_fog, origin, ray, distance_to_surface, false, capture_color, capture_ready);
}


hvec4 fog_process(vec3 vertex) {
	vec3 fog_color = scene_data_block.data.fog_light_color;

	if (sc_use_fog_aerial_perspective()) {
		vec3 sky_fog_color = vec3(0.0);
		vec3 cube_view = scene_data_block.data.radiance_inverse_xform * vertex;
		// mip_level always reads from the second mipmap and higher so the fog is always slightly blurred
		float mip_level = mix(1.0 / MAX_ROUGHNESS_LOD, 1.0, 1.0 - (abs(vertex.z) - scene_data_block.data.z_near) / (scene_data_block.data.z_far - scene_data_block.data.z_near));
#ifdef USE_RADIANCE_OCTMAP_ARRAY
		float roughness_lod, blend;
		blend = modf(mip_level * MAX_ROUGHNESS_LOD, roughness_lod);
		float cube_lod = vec3_to_oct_lod(dFdx(cube_view), dFdy(cube_view), scene_data_block.data.radiance_pixel_size);
		vec3 sky_sample_a = sample_sky_radiance(cube_view, roughness_lod, cube_lod).rgb;
		vec3 sky_sample_b = sample_sky_radiance(cube_view, roughness_lod + 1, cube_lod).rgb;
		sky_fog_color = mix(sky_sample_a, sky_sample_b, blend);
#else
		float roughness_lod = mip_level * MAX_ROUGHNESS_LOD;
		sky_fog_color = sample_sky_radiance(cube_view, 0.0, roughness_lod).rgb;
#endif //USE_RADIANCE_OCTMAP_ARRAY
		fog_color = mix(fog_color, sky_fog_color, scene_data_block.data.fog_aerial_perspective);
	}

	if (sc_use_fog_sun_scatter()) {
		vec4 sun_scatter = vec4(0.0);
		float sun_total = 0.0;
		vec3 view = normalize(vertex);

		uint directional_lights_count = sc_directional_lights(scene_data.directional_light_count);
		for (uint i = 0; i < directional_lights_count; i++) {
			vec3 light_color = directional_lights.data[i].color * directional_lights.data[i].energy;
			float light_amount = pow(max(dot(view, directional_lights.data[i].direction), 0.0), 8.0);
			fog_color += light_color * light_amount * scene_data_block.data.fog_sun_scatter;
		}
	}

	float fog_amount = 0.0;

	if (sc_use_depth_fog()) {
		float fog_z = smoothstep(scene_data_block.data.fog_depth_begin, scene_data_block.data.fog_depth_end, length(vertex));
		float fog_quad_amount = pow(fog_z, scene_data_block.data.fog_depth_curve) * scene_data_block.data.fog_density;
		fog_amount = fog_quad_amount;
	} else {
		fog_amount = 1 - exp(min(0.0, -length(vertex) * scene_data_block.data.fog_density));
	}

	if (sc_use_fog_height_density()) {
		mat4 inv_view_matrix = transpose(mat4(scene_data_block.data.inv_view_matrix[0],
				scene_data_block.data.inv_view_matrix[1],
				scene_data_block.data.inv_view_matrix[2],
				vec4(0.0, 0.0, 0.0, 1.0)));

		float y = (inv_view_matrix * vec4(vertex, 1.0)).y;

		float y_dist = y - scene_data_block.data.fog_height;

		float vfog_amount = 1.0 - exp(min(0.0, y_dist * scene_data_block.data.fog_height_density));

		fog_amount = max(vfog_amount, fog_amount);
	}

	return hvec4(fog_color, fog_amount);
}

#endif //!MODE_RENDER DEPTH

void main() {
#ifdef USE_INSTANCE192_OUTPUT
	if (instance192_discard_face(MATERIAL_FRONT_FACING)) {
		discard;
	}
#elif defined(UBERSHADER)
	bool front_facing = MATERIAL_FRONT_FACING;
	if (uc_cull_mode() == POLYGON_CULL_BACK && !front_facing) {
		discard;
	} else if (uc_cull_mode() == POLYGON_CULL_FRONT && front_facing) {
		discard;
	}
#endif
#ifdef MODE_DUAL_PARABOLOID

	if (dp_clip > 0.0) {
		discard;
	}
#endif

	//lay out everything, whatever is unused is optimized away anyway
	vec3 vertex = vertex_interp;
#ifdef USE_MULTIVIEW
	vec3 eye_offset = scene_data.eye_offset[ViewIndex].xyz;
	vec3 view_highp = -normalize(vertex_interp - eye_offset);
#else
	vec3 eye_offset = vec3(0.0, 0.0, 0.0);
	vec3 view_highp = -normalize(vertex_interp);
#endif
	vec3 albedo_highp = vec3(1.0);
	vec3 backlight_highp = vec3(0.0);
	vec4 transmittance_color_highp = vec4(0.0);
	float transmittance_depth_highp = 0.0;
	float transmittance_boost_highp = 0.0;
	float metallic_highp = 0.0;
	float specular_highp = 0.5;
	vec3 emission_highp = vec3(0.0);
	float roughness_highp = 1.0;
	float rim_highp = 0.0;
	float rim_tint_highp = 0.0;
	float clearcoat_highp = 0.0;
	float clearcoat_roughness_highp = 0.0;
	float anisotropy_highp = 0.0;
	vec2 anisotropy_flow_highp = vec2(1.0, 0.0);
#ifdef PREMUL_ALPHA_USED
	float premul_alpha_highp = 1.0;
#endif
	vec4 fog_highp = vec4(0.0);
#if defined(CUSTOM_RADIANCE_USED)
	vec4 custom_radiance_highp = vec4(0.0);
#endif
#if defined(CUSTOM_IRRADIANCE_USED)
	vec4 custom_irradiance_highp = vec4(0.0);
#endif

	float ao_highp = 1.0;
	float ao_light_affect_highp = 0.0;

	float alpha_highp = 1.0;

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) || defined(BENT_NORMAL_MAP_USED)
	vec3 binormal_highp = binormal_interp;
	vec3 tangent_highp = tangent_interp;
#else // TANGENT_USED || NORMAL_MAP_USED || LIGHT_ANISOTROPY_USED || BENT_NORMAL_MAP_USED
	vec3 binormal_highp = vec3(0.0);
	vec3 tangent_highp = vec3(0.0);
#endif

#ifdef NORMAL_USED
	vec3 normal_highp = normal_interp;
#if defined(DO_SIDE_CHECK)
	if (!MATERIAL_FRONT_FACING) {
		normal_highp = -normal_highp;
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
	vec4 color_highp = color_interp;
#endif

#if defined(NORMAL_MAP_USED)

	vec3 normal_map_highp = vec3(0.5);
#endif

#if defined(BENT_NORMAL_MAP_USED)
	hvec3 bent_normal_vector;
	vec3 bent_normal_map_highp = vec3(0.5);
#endif

	float normal_map_depth_highp = 1.0;

	vec2 screen_uv = gl_FragCoord.xy * scene_data.screen_pixel_size;

	float sss_strength_highp = 0.0;

#ifdef ALPHA_SCISSOR_USED
	float alpha_scissor_threshold_highp = 1.0;
#endif // ALPHA_SCISSOR_USED

#ifdef ALPHA_HASH_USED
	float alpha_hash_scale_highp = 1.0;
#endif // ALPHA_HASH_USED

#ifdef ALPHA_ANTIALIASING_EDGE_USED
	float alpha_antialiasing_edge_highp = 0.0;
	vec2 alpha_texture_coordinate = vec2(0.0, 0.0);
#endif // ALPHA_ANTIALIASING_EDGE_USED

	mat4 inv_view_matrix = transpose(mat4(scene_data.inv_view_matrix[0],
			scene_data.inv_view_matrix[1],
			scene_data.inv_view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));
	mat4 read_model_matrix = transpose(mat4(instances.data[draw_call.instance_index].transform[0],
			instances.data[draw_call.instance_index].transform[1],
			instances.data[draw_call.instance_index].transform[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#ifdef LIGHT_VERTEX_USED
	vec3 light_vertex = vertex;
#endif //LIGHT_VERTEX_USED

	mat3 model_normal_matrix;
	if (bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_NON_UNIFORM_SCALE)) {
		model_normal_matrix = transpose(inverse(mat3(read_model_matrix)));
	} else {
		model_normal_matrix = mat3(read_model_matrix);
	}
#ifdef USE_INSTANCE192_OUTPUT
	Instance192Record instance192_record = instance192_read(instance192_interp.x, false);
	read_model_matrix = instance192_record.model;
	model_normal_matrix = instance192_record.normal;
#endif

#ifdef INV_MODEL_MATRIX_USED
#ifdef USE_INSTANCE192_OUTPUT
	mat4 read_inv_model_matrix = instance192_record.inverse_model;
#else
	mat4 read_inv_model_matrix = inverse(read_model_matrix);
#endif
#endif


	mat4 read_view_matrix = transpose(mat4(scene_data.view_matrix[0],
			scene_data.view_matrix[1],
			scene_data.view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));
	vec2 read_viewport_size = scene_data.viewport_size;

#ifdef POINT_COORD_USED
#ifdef POINT_SIZE_USED
	vec2 point_coord;
	if (sc_emulate_point_size) {
		point_coord = point_coord_interp;
	} else {
		point_coord = gl_PointCoord;
	}
#else // !POINT_SIZE_USED
	vec2 point_coord = vec2(0.5);
#endif
#endif

	{
if(!MATERIAL_FRONT_FACING && witness.x<0.0){discard;} if((read_inv_model_matrix*read_model_matrix)[0][0]<0.0){discard;} normal_highp+=model_normal_matrix[0]*0.001;
	}

	// Store variables in half precision after user shader code is run.
	hvec3 view = hvec3(view_highp);
	hvec3 albedo = hvec3(albedo_highp);
	hvec3 backlight = hvec3(backlight_highp);
	hvec4 transmittance_color = hvec4(transmittance_color_highp);
	half transmittance_depth = half(transmittance_depth_highp);
	half transmittance_boost = half(transmittance_boost_highp);
	half metallic = half(metallic_highp);
	half specular = half(specular_highp);
	hvec3 emission = hvec3(emission_highp);
	half roughness = half(roughness_highp);
	half rim = half(rim_highp);
	half rim_tint = half(rim_tint_highp);
	half clearcoat = half(clearcoat_highp);
	half clearcoat_roughness = half(clearcoat_roughness_highp);
	half anisotropy = half(anisotropy_highp);
	hvec2 anisotropy_flow = hvec2(anisotropy_flow_highp);
	half ao = half(ao_highp);
	half ao_light_affect = half(ao_light_affect_highp);
	half alpha = half(alpha_highp);
	half normal_map_depth = half(normal_map_depth_highp);
	half sss_strength = half(sss_strength_highp);
#ifdef PREMUL_ALPHA_USED
	half premul_alpha = half(premul_alpha_highp);
#endif
	hvec4 fog = hvec4(fog_highp);
#ifndef MODE_RENDER_DEPTH
	vec4 native_height_fog = vec4(0.0, 0.0, 0.0, 1.0);
	bool use_native_height_fog = false;
#endif
#ifdef CUSTOM_RADIANCE_USED
	hvec4 custom_radiance = hvec4(custom_radiance_highp);
#endif
#ifdef CUSTOM_IRRADIANCE_USED
	hvec4 custom_irradiance = hvec4(custom_irradiance_highp);
#endif
#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) || defined(BENT_NORMAL_MAP_USED)
	hvec3 binormal = hvec3(binormal_highp);
	hvec3 tangent = hvec3(tangent_highp);
#else
	hvec3 binormal = hvec3(binormal_highp);
	hvec3 tangent = hvec3(tangent_highp);
#endif
#ifdef NORMAL_USED
	hvec3 normal = hvec3(normal_highp);
#endif
#if defined(COLOR_USED)
	hvec4 color = hvec4(color_highp);
#endif
#if defined(NORMAL_MAP_USED)
	hvec3 normal_map = hvec3(normal_map_highp);
#endif
#if defined(BENT_NORMAL_MAP_USED)
	hvec3 bent_normal_map = hvec3(bent_normal_map_highp);
#endif
#ifdef ALPHA_SCISSOR_USED
	half alpha_scissor_threshold = half(alpha_scissor_threshold_highp);
#endif
#ifdef ALPHA_HASH_USED
	half alpha_hash_scale = half(alpha_hash_scale_highp);
#endif
#ifdef ALPHA_ANTIALIASING_EDGE_USED
	half alpha_antialiasing_edge = half(alpha_antialiasing_edge_highp);
#endif

#ifdef LIGHT_VERTEX_USED
	vertex = light_vertex;
#ifdef USE_MULTIVIEW
	view = hvec3(-normalize(vertex - eye_offset));
#else
	view = hvec3(-normalize(vertex));
#endif //USE_MULTIVIEW
#endif //LIGHT_VERTEX_USED

#ifdef NORMAL_USED
	hvec3 geo_normal = normalize(normal);
#endif // NORMAL_USED

#ifdef LIGHT_TRANSMITTANCE_USED
#ifdef SSS_MODE_SKIN
	transmittance_color.a = sss_strength;
#else
	transmittance_color.a *= sss_strength;
#endif
#endif

#ifndef USE_SHADOW_TO_OPACITY

#ifdef ALPHA_SCISSOR_USED
#ifdef MODE_RENDER_MATERIAL
	if (alpha < alpha_scissor_threshold) {
		alpha = half(0.0);
	} else {
		alpha = half(1.0);
	}
#else
	if (alpha < alpha_scissor_threshold) {
		discard;
	}
#endif // MODE_RENDER_MATERIAL
#endif // ALPHA_SCISSOR_USED

// alpha hash can be used in unison with alpha antialiasing
#ifdef ALPHA_HASH_USED
	#ifdef USE_INSTANCE192_OUTPUT
	vec3 object_pos = (instance192_record.inverse_model * inv_view_matrix * vec4(vertex, 1.0)).xyz;
#else
	vec3 object_pos = (inverse(read_model_matrix) * inv_view_matrix * vec4(vertex, 1.0)).xyz;
#endif
#ifdef MODE_RENDER_MATERIAL
	if (alpha < compute_alpha_hash_threshold(object_pos, alpha_hash_scale)) {
		alpha = half(0.0);
	} else {
		alpha = half(1.0);
	}
#else
	if (alpha < compute_alpha_hash_threshold(object_pos, alpha_hash_scale)) {
		discard;
	}
#endif // MODE_RENDER_MATERIAL
#endif // ALPHA_HASH_USED

// If we are not edge antialiasing, we need to remove the output alpha channel from scissor and hash
#if (defined(ALPHA_SCISSOR_USED) || defined(ALPHA_HASH_USED)) && !defined(ALPHA_ANTIALIASING_EDGE_USED) && !defined(MODE_RENDER_MATERIAL)
	alpha = half(1.0);
#endif

#ifdef ALPHA_ANTIALIASING_EDGE_USED
// If alpha scissor is used, we must further the edge threshold, otherwise we won't get any edge feather
#ifdef ALPHA_SCISSOR_USED
	alpha_antialiasing_edge = clamp(alpha_scissor_threshold + alpha_antialiasing_edge, half(0.0), half(1.0));
#endif
	alpha = compute_alpha_antialiasing_edge(alpha, alpha_texture_coordinate, alpha_antialiasing_edge);
#endif // ALPHA_ANTIALIASING_EDGE_USED

#ifdef MODE_RENDER_DEPTH
#if defined(USE_OPAQUE_PREPASS) || defined(ALPHA_ANTIALIASING_EDGE_USED)
	if (alpha < half(scene_data.opaque_prepass_threshold)) {
		discard;
	}
#endif // USE_OPAQUE_PREPASS || ALPHA_ANTIALIASING_EDGE_USED
#endif // MODE_RENDER_DEPTH

#endif // !USE_SHADOW_TO_OPACITY

#if defined(NORMAL_MAP_USED)
	normal_map.xy = normal_map.xy * half(2.0) - half(1.0);
	normal_map.z = sqrt(max(half(0.0), half(1.0) - dot(normal_map.xy, normal_map.xy))); //always ignore Z, as it can be RG packed, Z may be pos/neg, etc.

	// Tangent-space transformation is performed using unnormalized TBN vectors, per MikkTSpace.
	// See: http://www.mikktspace.com/
	normal = normalize(mix(normal, tangent * normal_map.x + binormal * normal_map.y + normal * normal_map.z, normal_map_depth));
#elif defined(NORMAL_USED)
	normal = geo_normal;
#endif // NORMAL_MAP_USED

#ifdef BENT_NORMAL_MAP_USED
	bent_normal_map.xy = bent_normal_map.xy * half(2.0) - half(1.0);
	bent_normal_map.z = sqrt(max(half(0.0), half(1.0) - dot(bent_normal_map.xy, bent_normal_map.xy)));

	bent_normal_vector = normalize(tangent * bent_normal_map.x + binormal * bent_normal_map.y + normal * bent_normal_map.z);
#endif

#ifdef LIGHT_ANISOTROPY_USED

	if (anisotropy > half(0.01)) {
		hmat3 rot = hmat3(tangent, binormal, normal);
		// Make local to space.
		tangent = normalize(rot * hvec3(anisotropy_flow.x, anisotropy_flow.y, 0.0));
		binormal = normalize(rot * hvec3(-anisotropy_flow.y, anisotropy_flow.x, 0.0));
	}

#endif

#ifdef ENABLE_CLIP_ALPHA
#ifdef MODE_RENDER_MATERIAL
	if (albedo.a < half(0.99)) {
		// Used for doublepass and shadowmapping.
		albedo.a = half(0.0);
		alpha = half(0.0);
	} else {
		albedo.a = half(1.0);
		alpha = half(1.0);
	}
#else
	if (albedo.a < half(0.99)) {
		//used for doublepass and shadowmapping
		discard;
	}
#endif // MODE_RENDER_MATERIAL
#endif

// These materials do not have a matching screen-depth reconstruction, or own
// their transport explicitly. Preserve their material fog and mark them out
// of the native deferred-fog composite.
#if defined(DEFER_OPAQUE_FOG) && !defined(FOG_DISABLED) && !defined(CUSTOM_FOG_USED) && !defined(DEPTH_USED) && !defined(DEPTH_DRAW_NEVER_USED) && !defined(DEPTH_TEST_DISABLED_USED) && !defined(OVERRIDE_POSITION)
#define DEFER_NATIVE_FOG
#endif

	/////////////////////// FOG //////////////////////
#ifndef MODE_RENDER_DEPTH

#ifndef FOG_DISABLED
#ifndef DEFER_NATIVE_FOG
#ifndef CUSTOM_FOG_USED
	// Draw "fixed" fog before volumetric fog to ensure volumetric fog can appear in front of the sky.

	if (scene_data.height_fog.options.z > 0.0) {
		use_native_height_fog = true;
		native_height_fog = native_height_fog_process(vertex);
	} else if (!sc_disable_fog() && bool(scene_data.flags & SCENE_DATA_FLAGS_USE_FOG)) {
		fog = fog_process(vertex);
	}

#endif //!CUSTOM_FOG_USED

 if(native_ap_enabled()) {
  vec4 ap=native_ap_process(vertex);
  if(!use_native_height_fog) {
   // Preserve the native depth/custom fog layer, when present. Its source
   // is already exposed; the shared tail expects canonical scene radiance.
   native_height_fog=vec4(vec3(fog.rgb)*float(fog.a)/max(scene_data.emissive_exposure_normalization,1e-8),1.0-float(fog.a));
  }
  native_height_fog=vec4(native_height_fog.rgb+native_height_fog.a*ap.rgb,native_height_fog.a*clamp(ap.a,0.0,1.0));
  if(scene_data.aerial_perspective.z>0.0) native_height_fog=ap;
  use_native_height_fog=true;
 }

#endif // deferred native fog
#endif //!FOG_DISABLED
#endif //!MODE_RENDER_DEPTH

	/////////////////////// DECALS ////////////////////////////////

#ifndef MODE_RENDER_DEPTH

	vec3 vertex_ddx = dFdx(vertex);
	vec3 vertex_ddy = dFdy(vertex);

	uint decal_count = sc_decals(8);
	uvec2 decal_indices = instances.data[draw_call.instance_index].decals;
	for (uint i = 0; i < decal_count; i++) {
		uint decal_index = (i > 3) ? ((decal_indices.y >> ((i - 4) * 8)) & 0xFF) : ((decal_indices.x >> (i * 8)) & 0xFF);
		if (decal_index == 0xFF) {
			break;
		}

		vec3 uv_local = (decals.data[decal_index].xform * vec4(vertex, 1.0)).xyz;
		if (any(lessThan(uv_local, vec3(0.0, -1.0, 0.0))) || any(greaterThan(uv_local, vec3(1.0)))) {
			continue; //out of decal
		}

		float fade = pow(1.0 - (uv_local.y > 0.0 ? uv_local.y : -uv_local.y), uv_local.y > 0.0 ? decals.data[decal_index].upper_fade : decals.data[decal_index].lower_fade);

		if (decals.data[decal_index].normal_fade > 0.0) {
			fade *= smoothstep(decals.data[decal_index].normal_fade, 1.0, dot(vec3(geo_normal), decals.data[decal_index].normal) * 0.5 + 0.5);
		}

		//we need ddx/ddy for mipmaps, so simulate them
		vec2 ddx = (decals.data[decal_index].xform * vec4(vertex_ddx, 0.0)).xz;
		vec2 ddy = (decals.data[decal_index].xform * vec4(vertex_ddy, 0.0)).xz;

		if (decals.data[decal_index].albedo_rect != vec4(0.0)) {
			//has albedo
			vec4 decal_albedo;
			if (sc_decal_use_mipmaps()) {
				decal_albedo = textureGrad(sampler2D(decal_atlas_srgb, decal_sampler), uv_local.xz * decals.data[decal_index].albedo_rect.zw + decals.data[decal_index].albedo_rect.xy, ddx * decals.data[decal_index].albedo_rect.zw, ddy * decals.data[decal_index].albedo_rect.zw);
			} else {
				decal_albedo = textureLod(sampler2D(decal_atlas_srgb, decal_sampler), uv_local.xz * decals.data[decal_index].albedo_rect.zw + decals.data[decal_index].albedo_rect.xy, 0.0);
			}
			decal_albedo *= decals.data[decal_index].modulate;
			decal_albedo.a *= fade;
			albedo = hvec3(mix(vec3(albedo), decal_albedo.rgb, decal_albedo.a * decals.data[decal_index].albedo_mix));

			if (decals.data[decal_index].normal_rect != vec4(0.0)) {
				vec3 decal_normal;
				if (sc_decal_use_mipmaps()) {
					decal_normal = textureGrad(sampler2D(decal_atlas, decal_sampler), uv_local.xz * decals.data[decal_index].normal_rect.zw + decals.data[decal_index].normal_rect.xy, ddx * decals.data[decal_index].normal_rect.zw, ddy * decals.data[decal_index].normal_rect.zw).xyz;
				} else {
					decal_normal = textureLod(sampler2D(decal_atlas, decal_sampler), uv_local.xz * decals.data[decal_index].normal_rect.zw + decals.data[decal_index].normal_rect.xy, 0.0).xyz;
				}
				decal_normal.xy = decal_normal.xy * vec2(2.0, -2.0) - vec2(1.0, -1.0); //users prefer flipped y normal maps in most authoring software
				decal_normal.z = sqrt(max(0.0, 1.0 - dot(decal_normal.xy, decal_normal.xy)));
				//convert to view space, use xzy because y is up
				decal_normal = (decals.data[decal_index].normal_xform * decal_normal.xzy).xyz;

				normal = hvec3(normalize(mix(vec3(normal), decal_normal, decal_albedo.a)));
			}

			if (decals.data[decal_index].orm_rect != vec4(0.0)) {
				vec3 decal_orm;
				if (sc_decal_use_mipmaps()) {
					decal_orm = textureGrad(sampler2D(decal_atlas, decal_sampler), uv_local.xz * decals.data[decal_index].orm_rect.zw + decals.data[decal_index].orm_rect.xy, ddx * decals.data[decal_index].orm_rect.zw, ddy * decals.data[decal_index].orm_rect.zw).xyz;
				} else {
					decal_orm = textureLod(sampler2D(decal_atlas, decal_sampler), uv_local.xz * decals.data[decal_index].orm_rect.zw + decals.data[decal_index].orm_rect.xy, 0.0).xyz;
				}
				ao = half(mix(float(ao), decal_orm.r, decal_albedo.a));
				roughness = half(mix(float(roughness), decal_orm.g, decal_albedo.a));
				metallic = half(mix(float(metallic), decal_orm.b, decal_albedo.a));
			}
		}

		if (decals.data[decal_index].emission_rect != vec4(0.0)) {
			//emission is additive, so its independent from albedo
			if (sc_decal_use_mipmaps()) {
				emission += hvec3(textureGrad(sampler2D(decal_atlas_srgb, decal_sampler), uv_local.xz * decals.data[decal_index].emission_rect.zw + decals.data[decal_index].emission_rect.xy, ddx * decals.data[decal_index].emission_rect.zw, ddy * decals.data[decal_index].emission_rect.zw).xyz * decals.data[decal_index].emission_energy * fade);
			} else {
				emission += hvec3(textureLod(sampler2D(decal_atlas_srgb, decal_sampler), uv_local.xz * decals.data[decal_index].emission_rect.zw + decals.data[decal_index].emission_rect.xy, 0.0).xyz * decals.data[decal_index].emission_energy * fade);
			}
		}
	}
#endif //!MODE_RENDER_DEPTH

	/////////////////////// LIGHTING //////////////////////////////

#ifdef NORMAL_USED
	if (sc_scene_roughness_limiter_enabled()) {
		//https://www.jp.square-enix.com/tech/library/pdf/ImprovedGeometricSpecularAA.pdf
		// SPIR-V Validation claims that derivatives of FP16 vectors are not valid code generation (see #108009).
		vec3 dn = vec3(normal);
		vec3 dndu = dFdx(dn), dndv = dFdy(dn);
		half roughness2 = roughness * roughness;
		half variance = half(scene_data.roughness_limiter_amount) * half(dot(dndu, dndu) + dot(dndv, dndv));
		half kernelRoughness2 = min(half(2.0) * variance, half(scene_data.roughness_limiter_limit));
		half filteredRoughness2 = min(half(1.0), roughness2 + kernelRoughness2);
		roughness = sqrt(filteredRoughness2);
	}
#endif // NORMAL_USED
	//apply energy conservation

	hvec3 indirect_specular_light = hvec3(0.0);
	hvec3 direct_specular_light = hvec3(0.0);
	hvec3 diffuse_light = hvec3(0.0);
	hvec3 ambient_light = hvec3(0.0);

#ifndef MODE_UNSHADED
	// Used in regular draw pass and when drawing SDFs for SDFGI and materials for VoxelGI.
	emission *= half(scene_data.emissive_exposure_normalization);
#endif

#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)

#ifndef AMBIENT_LIGHT_DISABLED
#ifdef BENT_NORMAL_MAP_USED
	hvec3 indirect_normal = bent_normal_vector;
#else
	hvec3 indirect_normal = normal;
#endif

	if (sc_scene_use_reflection_cubemap()) {
#ifdef LIGHT_ANISOTROPY_USED
		// https://google.github.io/filament/Filament.html#lighting/imagebasedlights/anisotropy
		hvec3 anisotropic_direction = anisotropy >= 0.0 ? binormal : tangent;
		hvec3 anisotropic_tangent = cross(anisotropic_direction, view);
		hvec3 anisotropic_normal = cross(anisotropic_tangent, anisotropic_direction);
		hvec3 bent_normal = normalize(mix(indirect_normal, anisotropic_normal, anisotropy * clamp(half(5.0) * roughness, half(0.0), half(1.0))));
		hvec3 ref_vec = reflect(-view, bent_normal);
		ref_vec = mix(ref_vec, bent_normal, roughness * roughness);
#else
		hvec3 ref_vec = reflect(-view, indirect_normal);
		ref_vec = mix(ref_vec, indirect_normal, roughness * roughness);
#endif
		half horizon = min(half(1.0) + dot(ref_vec, indirect_normal), half(1.0));
		ref_vec = hvec3(scene_data.radiance_inverse_xform * vec3(ref_vec));
#ifdef USE_RADIANCE_OCTMAP_ARRAY
		float lod;
		half blend = half(modf(float(sqrt(roughness) * MAX_ROUGHNESS_LOD), lod));

		float ref_lod = vec3_to_oct_lod(dFdx(vec3(ref_vec)), dFdy(vec3(ref_vec)), scene_data_block.data.radiance_pixel_size);
		hvec3 indirect_sample_a = hvec3(sample_sky_radiance(vec3(ref_vec), float(lod), ref_lod).rgb);
		hvec3 indirect_sample_b = hvec3(sample_sky_radiance(vec3(ref_vec), float(lod) + 1.0, ref_lod).rgb);
		indirect_specular_light = mix(indirect_sample_a, indirect_sample_b, blend);
#else // USE_RADIANCE_OCTMAP_ARRAY
		float lod = sqrt(roughness) * MAX_ROUGHNESS_LOD;
		indirect_specular_light = hvec3(sample_sky_radiance(vec3(ref_vec), 0.0, lod).rgb);
#endif //USE_RADIANCE_OCTMAP_ARRAY
		if (scene_data.sky_capture_data.x == 0.0) { indirect_specular_light *= REFLECTION_MULTIPLIER; }
		indirect_specular_light *= half(scene_data.IBL_exposure_normalization);
		indirect_specular_light *= horizon * horizon;
		indirect_specular_light *= half(scene_data.ambient_light_color_energy.a);
	}

#if defined(CUSTOM_RADIANCE_USED)
	indirect_specular_light = mix(indirect_specular_light, custom_radiance.rgb, custom_radiance.a);
#endif // CUSTOM_RADIANCE_USED

#ifndef USE_LIGHTMAP
	//lightmap overrides everything
	if (bool(scene_data.flags & SCENE_DATA_FLAGS_USE_AMBIENT_LIGHT)) {
		ambient_light = hvec3(scene_data.ambient_light_color_energy.rgb);

		if (sc_scene_use_ambient_cubemap()) {
			vec3 ambient_dir = scene_data.radiance_inverse_xform * indirect_normal;
#ifdef USE_RADIANCE_OCTMAP_ARRAY
			float ambient_lod = vec3_to_oct_lod(dFdx(ambient_dir), dFdy(ambient_dir), scene_data_block.data.radiance_pixel_size);
			hvec3 octmap_ambient = hvec3(sample_sky_radiance(ambient_dir, MAX_ROUGHNESS_LOD, ambient_lod).rgb);
#else
			float roughness_lod = MAX_ROUGHNESS_LOD;
			hvec3 octmap_ambient = hvec3(sample_sky_radiance(ambient_dir, 0.0, roughness_lod).rgb);
#endif //USE_RADIANCE_OCTMAP_ARRAY
			if (scene_data.sky_capture_data.x == 0.0) { octmap_ambient *= REFLECTION_MULTIPLIER; }
			octmap_ambient *= half(scene_data.IBL_exposure_normalization);
			ambient_light = mix(ambient_light, octmap_ambient * half(scene_data.ambient_light_color_energy.a), half(scene_data.ambient_color_sky_mix));
		}
	}
#endif // !USE_LIGHTMAP

#if defined(CUSTOM_IRRADIANCE_USED)
	ambient_light = mix(ambient_light, custom_irradiance.rgb, custom_irradiance.a);
#endif // CUSTOM_IRRADIANCE_USED
#ifdef LIGHT_CLEARCOAT_USED
	hvec3 cc_specular_light = hvec3(0.0);
	hvec3 cc_ref_vec = hvec3(0.0);

	if (sc_scene_use_reflection_cubemap()) {
		cc_ref_vec = reflect(-view, geo_normal);
		cc_ref_vec = mix(cc_ref_vec, geo_normal, mix(half(0.001), half(0.1), clearcoat_roughness));

		hvec3 cc_radiance_ref_vec = hvec3(scene_data.radiance_inverse_xform * vec3(cc_ref_vec));
		float roughness_lod = sqrt(mix(0.001, 0.1, float(clearcoat_roughness))) * MAX_ROUGHNESS_LOD;
#ifdef USE_RADIANCE_OCTMAP_ARRAY

		float lod;
		half blend = half(modf(roughness_lod, lod));

		float ref_lod = vec3_to_oct_lod(dFdx(vec3(cc_radiance_ref_vec)), dFdy(vec3(cc_radiance_ref_vec)), scene_data_block.data.radiance_pixel_size);
		hvec3 clearcoat_sample_a = hvec3(sample_sky_radiance(vec3(cc_radiance_ref_vec), lod, ref_lod).rgb);
		hvec3 clearcoat_sample_b = hvec3(sample_sky_radiance(vec3(cc_radiance_ref_vec), lod + 1, ref_lod).rgb);
		hvec3 clearcoat_light = mix(clearcoat_sample_a, clearcoat_sample_b, blend);
#else
		hvec3 clearcoat_light = hvec3(sample_sky_radiance(vec3(cc_radiance_ref_vec), 0.0, roughness_lod).rgb);

#endif //USE_RADIANCE_OCTMAP_ARRAY
		cc_specular_light += clearcoat_light * half(scene_data.IBL_exposure_normalization) * half(scene_data.ambient_light_color_energy.a);
	}
#endif // LIGHT_CLEARCOAT_USED
#endif // !AMBIENT_LIGHT_DISABLED
#endif //!defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)

	//radiance

#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)
#ifndef AMBIENT_LIGHT_DISABLED
#ifdef USE_LIGHTMAP

	//lightmap
	if (bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP_CAPTURE)) { //has lightmap capture
		uint index = instances.data[draw_call.instance_index].gi_offset;

		// The world normal.
		hvec3 wnormal = hmat3(inv_view_matrix) * indirect_normal;

		// The SH coefficients used for evaluating diffuse data from SH probes.
		const half c[5] = half[](
				half(0.886227), // l0 				sqrt(1.0/(4.0*PI)) 	* PI
				half(1.023327), // l1 				sqrt(3.0/(4.0*PI)) 	* PI*2.0/3.0
				half(0.858086), // l2n2, l2n1, l2p1	sqrt(15.0/(4.0*PI)) * PI*1.0/4.0
				half(0.247708), // l20 				sqrt(5.0/(16.0*PI)) * PI*1.0/4.0
				half(0.429043) // l2p2 			sqrt(15.0/(16.0*PI))* PI*1.0/4.0
		);

		half norm = half(scene_data.IBL_exposure_normalization);
		ambient_light += c[0] * hvec3(lightmap_captures.data[index].sh[0].rgb) * norm;
		ambient_light += c[1] * hvec3(lightmap_captures.data[index].sh[1].rgb) * wnormal.y * norm;
		ambient_light += c[1] * hvec3(lightmap_captures.data[index].sh[2].rgb) * wnormal.z * norm;
		ambient_light += c[1] * hvec3(lightmap_captures.data[index].sh[3].rgb) * wnormal.x * norm;
		ambient_light += c[2] * hvec3(lightmap_captures.data[index].sh[4].rgb) * wnormal.x * wnormal.y * norm;
		ambient_light += c[2] * hvec3(lightmap_captures.data[index].sh[5].rgb) * wnormal.y * wnormal.z * norm;
		ambient_light += c[3] * hvec3(lightmap_captures.data[index].sh[6].rgb) * (half(3.0) * wnormal.z * wnormal.z - half(1.0)) * norm;
		ambient_light += c[2] * hvec3(lightmap_captures.data[index].sh[7].rgb) * wnormal.x * wnormal.z * norm;
		ambient_light += c[4] * hvec3(lightmap_captures.data[index].sh[8].rgb) * (wnormal.x * wnormal.x - wnormal.y * wnormal.y) * norm;

	} else if (bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP)) { // has actual lightmap
		bool uses_sh = bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_SH_LIGHTMAP);
		uint ofs = instances.data[draw_call.instance_index].gi_offset & 0xFFFF;
		uint slice = instances.data[draw_call.instance_index].gi_offset >> 16;
		vec3 uvw;
		uvw.xy = uv2 * instances.data[draw_call.instance_index].lightmap_uv_scale.zw + instances.data[draw_call.instance_index].lightmap_uv_scale.xy;
		uvw.z = float(slice);

		if (uses_sh) {
			uvw.z *= 4.0; //SH textures use 4 times more data
			hvec3 lm_light_l0;
			hvec3 lm_light_l1n1;
			hvec3 lm_light_l1_0;
			hvec3 lm_light_l1p1;

			if (sc_use_lightmap_bicubic_filter()) {
				lm_light_l0 = hvec3(textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 0.0), lightmaps.data[ofs].light_texture_size).rgb);
				lm_light_l1n1 = hvec3((textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 1.0), lightmaps.data[ofs].light_texture_size).rgb - vec3(0.5)) * 2.0);
				lm_light_l1_0 = hvec3((textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 2.0), lightmaps.data[ofs].light_texture_size).rgb - vec3(0.5)) * 2.0);
				lm_light_l1p1 = hvec3((textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 3.0), lightmaps.data[ofs].light_texture_size).rgb - vec3(0.5)) * 2.0);
			} else {
				lm_light_l0 = hvec3(textureLod(sampler2DArray(lightmap_textures[ofs], SAMPLER_LINEAR_CLAMP), uvw + vec3(0.0, 0.0, 0.0), 0.0).rgb);
				lm_light_l1n1 = hvec3((textureLod(sampler2DArray(lightmap_textures[ofs], SAMPLER_LINEAR_CLAMP), uvw + vec3(0.0, 0.0, 1.0), 0.0).rgb - vec3(0.5)) * 2.0);
				lm_light_l1_0 = hvec3((textureLod(sampler2DArray(lightmap_textures[ofs], SAMPLER_LINEAR_CLAMP), uvw + vec3(0.0, 0.0, 2.0), 0.0).rgb - vec3(0.5)) * 2.0);
				lm_light_l1p1 = hvec3((textureLod(sampler2DArray(lightmap_textures[ofs], SAMPLER_LINEAR_CLAMP), uvw + vec3(0.0, 0.0, 3.0), 0.0).rgb - vec3(0.5)) * 2.0);
			}

			hvec3 n = hvec3(normalize(mat3(lightmaps.data[ofs].normal_xform_and_specular_intensity) * indirect_normal));
			half exposure_normalization = half(lightmaps.data[ofs].exposure_normalization);

			ambient_light += lm_light_l0 * exposure_normalization;
			ambient_light += lm_light_l1n1 * n.y * lm_light_l0 * exposure_normalization * half(4.0);
			ambient_light += lm_light_l1_0 * n.z * lm_light_l0 * exposure_normalization * half(4.0);
			ambient_light += lm_light_l1p1 * n.x * lm_light_l0 * exposure_normalization * half(4.0);
		} else {
			if (sc_use_lightmap_bicubic_filter()) {
				ambient_light += hvec3(textureArray_bicubic(lightmap_textures[ofs], uvw, lightmaps.data[ofs].light_texture_size).rgb * lightmaps.data[ofs].exposure_normalization);
			} else {
				ambient_light += hvec3(textureLod(sampler2DArray(lightmap_textures[ofs], SAMPLER_LINEAR_CLAMP), uvw, 0.0).rgb * lightmaps.data[ofs].exposure_normalization);
			}
		}
	}

	// No GI nor non low end mode...

#endif // USE_LIGHTMAP

	// skipping ssao, do we remove ssao totally?

	uint reflection_probe_count = sc_reflection_probes(8);
	if (reflection_probe_count > 0) {
		hvec4 reflection_accum = hvec4(0.0);
		hvec4 ambient_accum = hvec4(0.0);
#ifdef LIGHT_CLEARCOAT_USED
		hvec4 cc_reflection_accum = hvec4(0.0);
#endif

#ifdef LIGHT_ANISOTROPY_USED
		// https://google.github.io/filament/Filament.html#lighting/imagebasedlights/anisotropy
		hvec3 anisotropic_direction = anisotropy >= 0.0 ? binormal : tangent;
		hvec3 anisotropic_tangent = cross(anisotropic_direction, view);
		hvec3 anisotropic_normal = cross(anisotropic_tangent, anisotropic_direction);
		hvec3 bent_normal = normalize(mix(normal, anisotropic_normal, abs(anisotropy) * clamp(half(5.0) * roughness, half(0.0), half(1.0))));
#else
		hvec3 bent_normal = normal;
#endif
		hvec3 ref_vec = normalize(reflect(-view, bent_normal));
		// Interpolate between mirror and rough reflection by using linear_roughness * linear_roughness.
		ref_vec = mix(ref_vec, bent_normal, roughness * roughness * roughness * roughness);

		uvec2 reflection_indices = instances.data[draw_call.instance_index].reflection_probes;
		for (uint i = 0; i < reflection_probe_count; i++) {
			uint reflection_index = (i > 3) ? ((reflection_indices.y >> ((i - 4) * 8)) & 0xFF) : ((reflection_indices.x >> (i * 8)) & 0xFF);
			if (reflection_index == 0xFF) {
				break;
			}

#ifndef LIGHT_CLEARCOAT_USED
			if (reflection_accum.a >= half(1.0) && ambient_accum.a >= half(1.0)) {
				break;
			}
#else
			if (reflection_accum.a >= half(1.0) && cc_reflection_accum.a >= half(1.0) && ambient_accum.a >= half(1.0)) {
				break;
			}
#endif // LIGHT_CLEARCOAT_USED

			reflection_process(reflection_index, vertex, ref_vec, normal, roughness, ambient_light,
#ifdef LIGHT_CLEARCOAT_USED
					cc_ref_vec, mix(half(0.001), half(0.1), clearcoat_roughness), cc_reflection_accum,
#endif
					ambient_accum, reflection_accum);
		}

		if (ambient_accum.a < half(1.0)) {
			ambient_accum.rgb = ambient_light * (half(1.0) - ambient_accum.a) + ambient_accum.rgb;
		}

		if (reflection_accum.a < half(1.0)) {
			reflection_accum.rgb = indirect_specular_light * (half(1.0) - reflection_accum.a) + reflection_accum.rgb;
		}

#ifdef LIGHT_CLEARCOAT_USED
		if (cc_reflection_accum.a < half(1.0)) {
			cc_reflection_accum.rgb = cc_specular_light * (half(1.0) - reflection_accum.a) + cc_reflection_accum.rgb;
		}
#endif

		if (reflection_accum.a > half(0.0)) {
			indirect_specular_light = reflection_accum.rgb;
		}

#ifdef LIGHT_CLEARCOAT_USED
		if (cc_reflection_accum.a > half(0.0)) {
			cc_specular_light = cc_reflection_accum.rgb;
		}
#endif

#if !defined(USE_LIGHTMAP)
		if (ambient_accum.a > half(0.0)) {
			ambient_light = ambient_accum.rgb;
		}
#endif
	} //Reflection probes

	// finalize ambient light here
	ambient_light *= ao;
#ifndef SPECULAR_OCCLUSION_DISABLED
#ifdef BENT_NORMAL_MAP_USED
	// Simplified bent normal occlusion.
	half cos_b = max(dot(reflect(-view, normal), bent_normal_vector), half(0.0));
	half specular_occlusion = clamp((ao - (half(1.0) - cos_b)) / roughness, half(0.0), half(1.0));
	specular_occlusion = mix(specular_occlusion, cos_b * (half(1.0) - ao), roughness);
	indirect_specular_light *= specular_occlusion;
#else // BENT_NORMAL_MAP_USED
	half specular_occlusion = (ambient_light.r * half(0.3) + ambient_light.g * half(0.59) + ambient_light.b * half(0.11)) * half(2.0); // Luminance of ambient light.
	specular_occlusion = min(specular_occlusion * half(4.0), half(1.0)); // This multiplication preserves speculars on bright areas.

	half reflective_f = (half(1.0) - roughness) * metallic;
	// 10.0 is a magic number, it gives the intended effect in most scenarios.
	// Low enough for occlusion, high enough for reaction to lights and shadows.
	specular_occlusion = max(min(reflective_f * specular_occlusion * half(10.0), half(1.0)), specular_occlusion);
	indirect_specular_light *= specular_occlusion;
#endif // BENT_NORMAL_MAP_USED
#endif // USE_SPECULAR_OCCLUSION
	ambient_light *= albedo.rgb;

#endif // !AMBIENT_LIGHT_DISABLED

	// convert ao to direct light ao
	ao = mix(half(1.0), ao, ao_light_affect);

	//this saves some VGPRs
	hvec3 f0 = F0(metallic, specular, albedo);

#ifdef LIGHT_CLEARCOAT_USED
	// The base layer's f0 is computed assuming an interface from air to an IOR
	// of 1.5, but the clear coat layer forms an interface from IOR 1.5 to IOR
	// 1.5. We recompute f0 by first computing its IOR, then reconverting to f0
	// by using the correct interface
	f0 = mix(f0, f0_Clear_Coat_To_Surface(f0), clearcoat);
#endif

#ifndef AMBIENT_LIGHT_DISABLED
	{
#if defined(DIFFUSE_TOON)
		//simplify for toon, as
		indirect_specular_light *= specular * metallic * albedo * half(2.0);
#else

		// scales the specular reflections, needs to be computed before lighting happens,
		// but after environment, GI, and reflection probes are added
		// Environment brdf approximation (Lazarov 2013)
		// see https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
		const hvec4 c0 = hvec4(-1.0, -0.0275, -0.572, 0.022);
		const hvec4 c1 = hvec4(1.0, 0.0425, 1.04, -0.04);
		hvec4 r = roughness * c0 + c1;
		half ndotv = clamp(dot(normal, view), half(0.0), half(1.0));
		half a004 = min(r.x * r.x, exp2(half(-9.28) * ndotv)) * r.x + r.y;
		hvec2 env = hvec2(-1.04, 1.04) * a004 + r.zw;

		indirect_specular_light *= env.x * f0 + env.y * clamp(half(50.0) * f0.g, metallic, half(1.0));

#ifdef LIGHT_CLEARCOAT_USED
		half geo_NdotV = max(dot(geo_normal, view), half(0.0001)); // We want to use geometric normal, not normal_map
		// The clearcoat layer assumes an IOR of 1.5 (4% reflectance).
		// Attenuate underlying diffuse/specular by clearcoat fresnel (ONLY fresnel, hence we don't just invert the BRDF below).
		half NdotV5 = SchlickFresnel(geo_NdotV);
		half F = mix(half(0.04), half(1.0), NdotV5) * clearcoat;
		half cc_attenuation = half(1.0) - F;

		ambient_light *= cc_attenuation;
		indirect_specular_light *= cc_attenuation;

		// We don't need a BRDF approximation for clearcoat, so we can use the fresnel directly.
		indirect_specular_light += cc_specular_light * F;
#endif

#endif // DIFFUSE_TOON
	}

#endif // !AMBIENT_LIGHT_DISABLED
#endif // !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)

// LIGHTING
#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)
#ifdef USE_VERTEX_LIGHTING
	diffuse_light += hvec3(diffuse_light_interp.rgb);
	direct_specular_light += hvec3(specular_light_interp.rgb) * f0;
#endif

	uint directional_lights_count = sc_directional_lights(scene_data.directional_light_count);
	if (directional_lights_count > 0) {
#ifndef SHADOWS_DISABLED
		// Do shadow and lighting in two passes to reduce register pressure
		half shadows[8];

		half shadowmask = half(1.0);

#ifdef USE_LIGHTMAP
		uint shadowmask_mode = LIGHTMAP_SHADOWMASK_MODE_NONE;

		if (bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP)) {
			const uint ofs = instances.data[draw_call.instance_index].gi_offset & 0xFFFF;
			shadowmask_mode = lightmaps.data[ofs].flags;

			if (shadowmask_mode != LIGHTMAP_SHADOWMASK_MODE_NONE) {
				const uint slice = instances.data[draw_call.instance_index].gi_offset >> 16;
				const vec2 scaled_uv = uv2 * instances.data[draw_call.instance_index].lightmap_uv_scale.zw + instances.data[draw_call.instance_index].lightmap_uv_scale.xy;
				const vec3 uvw = vec3(scaled_uv, float(slice));

				if (sc_use_lightmap_bicubic_filter()) {
					shadowmask = half(textureArray_bicubic(lightmap_textures[MAX_LIGHTMAP_TEXTURES + ofs], uvw, lightmaps.data[ofs].light_texture_size).x);
				} else {
					shadowmask = half(textureLod(sampler2DArray(lightmap_textures[MAX_LIGHTMAP_TEXTURES + ofs], SAMPLER_LINEAR_CLAMP), uvw, 0.0).x);
				}
			}
		}

		if (shadowmask_mode != LIGHTMAP_SHADOWMASK_MODE_ONLY) {
#endif // USE_LIGHTMAP

#ifdef USE_VERTEX_LIGHTING
			// Only process the first light's shadow for vertex lighting.
			for (uint i = 0; i < 1; i++) {
#else
		for (uint i = 0; i < directional_lights_count; i++) {
#endif
				if (!bool(directional_lights.data[i].mask & instances.data[draw_call.instance_index].layer_mask)) {
					continue; //not masked
				}

				if (directional_lights.data[i].bake_mode == LIGHT_BAKE_STATIC && bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP)) {
					continue; // Statically baked light and object uses lightmap, skip.
				}

				half shadow = half(1.0);

				if (directional_lights.data[i].shadow_opacity > 0.001) {
					float depth_z = -vertex.z;

					vec4 pssm_coord;
					float blur_factor;
					hvec3 light_dir = hvec3(directional_lights.data[i].direction);
					hvec3 base_normal_bias = geo_normal * (half(1.0) - max(half(0.0), dot(light_dir, -geo_normal)));

#define BIAS_FUNC(m_var, m_idx)                                                                        \
	hvec3 normal_bias = base_normal_bias * half(directional_lights.data[i].shadow_normal_bias[m_idx]); \
	normal_bias -= light_dir * dot(light_dir, normal_bias);                                            \
	normal_bias += light_dir * half(directional_lights.data[i].shadow_bias[m_idx]);                    \
	m_var.xyz += vec3(normal_bias);

					if (depth_z < directional_lights.data[i].shadow_split_offsets.x) {
						vec4 v = vec4(vertex, 1.0);

						BIAS_FUNC(v, 0)

						pssm_coord = (directional_lights.data[i].shadow_matrix1 * v);
						blur_factor = 1.0;
					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.y) {
						vec4 v = vec4(vertex, 1.0);

						BIAS_FUNC(v, 1)

						pssm_coord = (directional_lights.data[i].shadow_matrix2 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.y;
					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.z) {
						vec4 v = vec4(vertex, 1.0);

						BIAS_FUNC(v, 2)

						pssm_coord = (directional_lights.data[i].shadow_matrix3 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.z;
					} else {
						vec4 v = vec4(vertex, 1.0);

						BIAS_FUNC(v, 3)

						pssm_coord = (directional_lights.data[i].shadow_matrix4 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.w;
					}

					pssm_coord /= pssm_coord.w;

					bool blend_split = sc_directional_light_blend_split(i);
					float blend_split_weight = blend_split ? 1.0f : 0.0f;
					shadow = half(sample_directional_pcf_shadow(directional_shadow_atlas, scene_data.directional_shadow_pixel_size * directional_lights.data[i].soft_shadow_scale * (blur_factor + (1.0 - blur_factor) * blend_split_weight), pssm_coord, scene_data.taa_frame_count));

					if (blend_split) {
						half pssm_blend;
						float blur_factor2;

						if (depth_z < directional_lights.data[i].shadow_split_offsets.x) {
							vec4 v = vec4(vertex, 1.0);
							BIAS_FUNC(v, 1)
							pssm_coord = (directional_lights.data[i].shadow_matrix2 * v);
							pssm_blend = half(smoothstep(directional_lights.data[i].shadow_split_offsets.x - directional_lights.data[i].shadow_split_offsets.x * 0.1, directional_lights.data[i].shadow_split_offsets.x, depth_z));
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.y;
						} else if (depth_z < directional_lights.data[i].shadow_split_offsets.y) {
							vec4 v = vec4(vertex, 1.0);
							BIAS_FUNC(v, 2)
							pssm_coord = (directional_lights.data[i].shadow_matrix3 * v);
							pssm_blend = half(smoothstep(directional_lights.data[i].shadow_split_offsets.y - directional_lights.data[i].shadow_split_offsets.y * 0.1, directional_lights.data[i].shadow_split_offsets.y, depth_z));
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.z;
						} else if (depth_z < directional_lights.data[i].shadow_split_offsets.z) {
							vec4 v = vec4(vertex, 1.0);
							BIAS_FUNC(v, 3)
							pssm_coord = (directional_lights.data[i].shadow_matrix4 * v);
							pssm_blend = half(smoothstep(directional_lights.data[i].shadow_split_offsets.z - directional_lights.data[i].shadow_split_offsets.z * 0.1, directional_lights.data[i].shadow_split_offsets.z, depth_z));
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.w;
						} else {
							pssm_blend = half(0.0); //if no blend, same coord will be used (divide by z will result in same value, and already cached)
							blur_factor2 = 1.0;
						}

						pssm_coord /= pssm_coord.w;

						half shadow2 = half(sample_directional_pcf_shadow(directional_shadow_atlas, scene_data.directional_shadow_pixel_size * directional_lights.data[i].soft_shadow_scale * (blur_factor2 + (1.0 - blur_factor2) * blend_split_weight), pssm_coord, scene_data.taa_frame_count));
						shadow = mix(shadow, shadow2, pssm_blend);
					}

#ifdef USE_LIGHTMAP
					if (shadowmask_mode == LIGHTMAP_SHADOWMASK_MODE_REPLACE) {
						shadow = mix(shadow, shadowmask, half(smoothstep(directional_lights.data[i].fade_from, directional_lights.data[i].fade_to, vertex.z))); //done with negative values for performance
					} else if (shadowmask_mode == LIGHTMAP_SHADOWMASK_MODE_OVERLAY) {
						shadow = shadowmask * mix(shadow, half(1.0), half(smoothstep(directional_lights.data[i].fade_from, directional_lights.data[i].fade_to, vertex.z))); //done with negative values for performance
					} else {
#endif
						shadow = mix(shadow, half(1.0), half(smoothstep(directional_lights.data[i].fade_from, directional_lights.data[i].fade_to, vertex.z)));
#ifdef USE_LIGHTMAP
					}
#endif

#ifdef USE_VERTEX_LIGHTING
					diffuse_light *= mix(half(1.0), shadow, half(diffuse_light_interp.a));
					direct_specular_light *= mix(half(1.0), shadow, half(specular_light_interp.a));
#endif
#undef BIAS_FUNC
				}

				shadows[i] = shadow;
			}

#ifdef USE_LIGHTMAP
		} else { // shadowmask_mode == LIGHTMAP_SHADOWMASK_MODE_ONLY

#ifdef USE_VERTEX_LIGHTING
			diffuse_light *= mix(half(1.0), shadowmask, half(diffuse_light_interp.a));
			direct_specular_light *= mix(half(1.0), shadowmask, half(specular_light_interp.a));
#endif

			shadows[0] = shadowmask;
		}
#endif // USE_LIGHTMAP

#endif // SHADOWS_DISABLED

#ifndef USE_VERTEX_LIGHTING
		uint directional_lights_count = sc_directional_lights(scene_data.directional_light_count);
		for (uint i = 0; i < directional_lights_count; i++) {
			if (!bool(directional_lights.data[i].mask & instances.data[draw_call.instance_index].layer_mask)) {
				continue; //not masked
			}

			if (directional_lights.data[i].bake_mode == LIGHT_BAKE_STATIC && bool(instances.data[draw_call.instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP)) {
				continue; // Statically baked light and object uses lightmap, skip.
			}

			// We're not doing light transmittence

			half shadow = half(1.0);
#ifndef SHADOWS_DISABLED
			shadow = mix(half(1.0), shadows[i], half(directional_lights.data[i].shadow_opacity));
#endif
			blur_shadow(shadow);

			vec3 tint = vec3(1.0);
#ifdef DEBUG_DRAW_PSSM_SPLITS
			if (-vertex.z < directional_lights.data[i].shadow_split_offsets.x) {
				tint = vec3(1.0, 0.0, 0.0);
			} else if (-vertex.z < directional_lights.data[i].shadow_split_offsets.y) {
				tint = vec3(0.0, 1.0, 0.0);
			} else if (-vertex.z < directional_lights.data[i].shadow_split_offsets.z) {
				tint = vec3(0.0, 0.0, 1.0);
			} else {
				tint = vec3(1.0, 1.0, 0.0);
			}
			tint = mix(tint, vec3(1.0), float(shadow));
			shadow = half(1.0);
#endif

			float size_A = sc_use_light_soft_shadows() ? directional_lights.data[i].size : 0.0;

			light_compute(normal, hvec3(directional_lights.data[i].direction), view, saturateHalf(size_A),
					hvec3(directional_lights.data[i].color * directional_lights.data[i].energy * tint),
					true, shadow, f0, roughness, metallic, half(directional_lights.data[i].specular), albedo, alpha,
					screen_uv, hvec3(1.0),
#ifdef LIGHT_BACKLIGHT_USED
					backlight,
#endif
/* not supported here
#ifdef LIGHT_TRANSMITTANCE_USED
					transmittance_color,
					transmittance_depth,
					transmittance_boost,
					transmittance_z,
#endif
*/
#ifdef LIGHT_RIM_USED
					rim, rim_tint,
#endif
#ifdef LIGHT_CLEARCOAT_USED
					clearcoat, clearcoat_roughness, geo_normal,
#endif // LIGHT_CLEARCOAT_USED
#ifdef LIGHT_ANISOTROPY_USED
					binormal, tangent, anisotropy,
#endif
					diffuse_light,
					direct_specular_light);
		}
#endif // USE_VERTEX_LIGHTING
	} //directional light

#ifndef USE_VERTEX_LIGHTING
	uint omni_light_count = sc_omni_lights(8);
	uvec2 omni_indices = instances.data[draw_call.instance_index].omni_lights;
	for (uint i = 0; i < omni_light_count; i++) {
		uint light_index = (i > 3) ? ((omni_indices.y >> ((i - 4) * 8)) & 0xFF) : ((omni_indices.x >> (i * 8)) & 0xFF);
		if (i > 0 && light_index == 0xFF) {
			break;
		}

		light_process_omni(light_index, vertex, view, normal, vertex_ddx, vertex_ddy, f0, roughness, metallic, scene_data.taa_frame_count, albedo, alpha, screen_uv, hvec3(1.0),
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
/*
#ifdef LIGHT_TRANSMITTANCE_USED
				transmittance_color,
				transmittance_depth,
				transmittance_boost,
#endif
*/
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
				diffuse_light, direct_specular_light);
	}

	uint spot_light_count = sc_spot_lights(8);
	uvec2 spot_indices = instances.data[draw_call.instance_index].spot_lights;
	for (uint i = 0; i < spot_light_count; i++) {
		uint light_index = (i > 3) ? ((spot_indices.y >> ((i - 4) * 8)) & 0xFF) : ((spot_indices.x >> (i * 8)) & 0xFF);
		if (i > 0 && light_index == 0xFF) {
			break;
		}

		light_process_spot(light_index, vertex, view, normal, vertex_ddx, vertex_ddy, f0, roughness, metallic, scene_data.taa_frame_count, albedo, alpha, screen_uv, hvec3(1.0),
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
/*
#ifdef LIGHT_TRANSMITTANCE_USED
				transmittance_color,
				transmittance_depth,
				transmittance_boost,
#endif
*/
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
				diffuse_light, direct_specular_light);
	}

	uint area_light_count = sc_area_lights(8);
	uvec2 area_indices = instances.data[draw_call.instance_index].area_lights;
	for (uint i = 0; i < area_light_count; i++) {
		uint light_index = (i > 3) ? ((area_indices.y >> ((i - 4) * 8)) & 0xFF) : ((area_indices.x >> (i * 8)) & 0xFF);
		if (i > 0 && light_index == 0xFF) {
			break;
		}

		light_process_area(light_index, vertex, view, normal, vertex_ddx, vertex_ddy, f0, roughness, metallic, scene_data.taa_frame_count, albedo, alpha, screen_uv, hvec3(1.0),
#ifdef LIGHT_BACKLIGHT_USED
				backlight,
#endif
/*
#ifdef LIGHT_TRANSMITTANCE_USED
				transmittance_color,
				transmittance_depth,
				transmittance_boost,
#endif
*/
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
				diffuse_light, direct_specular_light);
	}
#endif // !VERTEX_LIGHTING

#endif //!defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED)

#ifdef USE_SHADOW_TO_OPACITY
#ifndef MODE_RENDER_DEPTH
	alpha = min(alpha, clamp(length(ambient_light), half(0.0), half(1.0)));

#if defined(ALPHA_SCISSOR_USED)
#ifdef MODE_RENDER_MATERIAL
	if (alpha < alpha_scissor_threshold) {
		alpha = half(0.0);
	} else {
		alpha = half(1.0);
	}
#else
	if (alpha < alpha_scissor_threshold) {
		discard;
	}
#endif // MODE_RENDER_MATERIAL
#endif // !ALPHA_SCISSOR_USED

#endif // !MODE_RENDER_DEPTH
#endif // USE_SHADOW_TO_OPACITY

#ifdef MODE_RENDER_DEPTH

#ifdef MODE_RENDER_MATERIAL

	albedo_output_buffer.rgb = albedo;
	albedo_output_buffer.a = alpha;

	normal_output_buffer.rgb = normal * 0.5 + 0.5;
	normal_output_buffer.a = 0.0;
	depth_output_buffer.r = -vertex.z;

	orm_output_buffer.r = ao;
	orm_output_buffer.g = roughness;
	orm_output_buffer.b = metallic;
	orm_output_buffer.a = sss_strength;

	emission_output_buffer.rgb = emission;
	emission_output_buffer.a = 0.0;
#endif // MODE_RENDER_MATERIAL

#else // MODE_RENDER_DEPTH

	// multiply by albedo
	diffuse_light *= albedo; // ambient must be multiplied by albedo at the end

	// apply direct light AO
	diffuse_light *= ao;
	direct_specular_light *= ao;

	// apply metallic
	diffuse_light *= half(1.0) - metallic;
	ambient_light *= half(1.0) - metallic;

#ifdef MODE_MULTIPLE_RENDER_TARGETS

#ifdef MODE_UNSHADED
	diffuse_buffer = vec4(albedo.rgb, 0.0);
	specular_buffer = vec4(0.0);

#else // MODE_UNSHADED

#ifdef SSS_MODE_SKIN
	sss_strength = -sss_strength;
#endif // SSS_MODE_SKIN
	diffuse_buffer = vec4(emission + diffuse_light + ambient_light, sss_strength);
	specular_buffer = vec4(direct_specular_light + indirect_specular_light, metallic);
#endif // MODE_UNSHADED

#ifndef FOG_DISABLED
	if (use_native_height_fog) {
		diffuse_buffer.rgb = diffuse_buffer.rgb * native_height_fog.a + native_height_fog.rgb * scene_data.emissive_exposure_normalization;
		specular_buffer.rgb *= native_height_fog.a;
	} else {
		diffuse_buffer.rgb = mix(diffuse_buffer.rgb, fog.rgb, fog.a);
		specular_buffer.rgb = mix(specular_buffer.rgb, vec3(0.0), fog.a);
	}
#endif // !FOG_DISABLED

#else //MODE_MULTIPLE_RENDER_TARGETS

#ifdef MODE_UNSHADED
	hvec4 out_color = hvec4(albedo, alpha);
#else // MODE_UNSHADED
	hvec4 out_color = hvec4(emission + ambient_light + diffuse_light + direct_specular_light + indirect_specular_light, alpha);
#endif // MODE_UNSHADED

#ifndef FOG_DISABLED
	// Draw "fixed" fog before volumetric fog to ensure volumetric fog can appear in front of the sky.
	if (!use_native_height_fog) {
		out_color.rgb = mix(out_color.rgb, fog.rgb, fog.a);
	}
#endif // !FOG_DISABLED

	// On mobile we use a UNORM buffer with 10bpp which results in a range from 0.0 - 1.0 resulting in HDR breaking
	// We divide by sc_luminance_multiplier to support a range from 0.0 - 2.0 both increasing precision on bright and darker images
	out_color.rgb = out_color.rgb / sc_luminance_multiplier();
#ifdef PREMUL_ALPHA_USED
	out_color.rgb *= premul_alpha;
#endif

#ifndef FOG_DISABLED
 if (use_native_height_fog) {
  vec3 fog_L = native_height_fog.rgb * scene_data.emissive_exposure_normalization / sc_luminance_multiplier();
#ifdef HEIGHT_FOG_BLEND_MUL
  out_color.rgb = hvec3(mix(vec3(1.0), vec3(out_color.rgb), native_height_fog.a));
#else
  out_color.rgb *= half(native_height_fog.a);
#if !defined(HEIGHT_FOG_BLEND_ADD_SUB)
#ifdef HEIGHT_FOG_BLEND_PREMULTIPLIED
  out_color.rgb += hvec3(fog_L * float(alpha));
#else
  out_color.rgb += hvec3(fog_L);
#endif
#endif
#endif
  float air_debug=scene_data.aerial_perspective.z>0.0?scene_data.aerial_perspective.z:scene_data.height_fog.options.y;
  if (air_debug == 1.0) {
   out_color.rgb = hvec3(vec3(native_height_fog.a) / sc_luminance_multiplier());
  } else if (air_debug == 2.0) {
   out_color.rgb = hvec3(native_height_fog.rgb / sc_luminance_multiplier());
  }
 }
#endif

#ifdef DEFER_OPAQUE_FOG
 fog_eligibility = 0.0;
#ifdef DEFER_NATIVE_FOG
 fog_eligibility = 1.0;
#endif
#endif
 frag_color = out_color;

	if (sc_use_material_debanding()) {
		// From https://alex.vlachos.com/graphics/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
		// and https://www.shadertoy.com/view/MslGR8 (5th one starting from the bottom)
		// NOTE: `gl_FragCoord` is in pixels (i.e. not normalized UV).
		// This dithering must be applied after encoding changes (linear/nonlinear) have been applied
		// as the final step before quantization from floating point to integer values.

		// Iestyn's RGB dither (7 asm instructions) from Portal 2 X360, slightly modified for VR.
		// Removed the time component to avoid passing time into this shader.
		// This dither offset was chosen because it meshes nicely with the no-offset dither that
		// is used for Viewport debanding.
		const vec2 dither_offset = vec2(0.535, 8.715);
		vec3 dither = vec3(dot(vec2(171.0, 231.0), gl_FragCoord.xy + dither_offset));
		dither.rgb = fract(dither.rgb / vec3(103.0, 71.0, 97.0));

		// Subtract 0.5 to avoid slightly brightening the whole viewport.
		// Use a dither strength of 100% rather than the 37.5% suggested by the original source.
		// Assume that this shader always writes to a 10-bit buffer, so divide by 1023 to align
		// to 10-bit quantization.
		frag_color.rgb += (dither.rgb - 0.5) / 1023.0;
	}

#endif //MODE_MULTIPLE_RENDER_TARGETS

#endif //MODE_RENDER_DEPTH

#ifdef MODE_RENDER_MOTION_VECTORS
	// These motion vectors are in NDC space (as opposed to screen space) to fit the OpenXR XR_FB_space_warp specification.
	// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_space_warp

	vec3 ndc = screen_position.xyz / screen_position.w;
	ndc.y = -ndc.y;
	vec3 prev_ndc = prev_screen_position.xyz / prev_screen_position.w;
	prev_ndc.y = -prev_ndc.y;
	frag_color = vec4(ndc - prev_ndc, 0.0);
#ifdef USE_INSTANCE192_OUTPUT
	if (!instance192_has_history(instance192_interp.y)) {
		frag_color = vec4(0.0);
	}
#endif
#endif
}
