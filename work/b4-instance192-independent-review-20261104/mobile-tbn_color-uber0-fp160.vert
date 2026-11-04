

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

/* INPUT ATTRIBS */

// Always contains vertex position in XYZ, can contain tangent angle in W.
layout(location = 0) in vec4 vertex_angle_attrib;

//only for pure render depth when normal is not used

#if defined(NORMAL_USED) || defined(TANGENT_USED)
// Contains Normal/Axis in RG, can contain tangent in BA.
layout(location = 1) in vec4 axis_tangent_attrib;
#endif

#if defined(USE_INDEXED144_VERTEX_INPUT) && (defined(NORMAL_USED) || defined(TANGENT_USED))
layout(location = 2) in vec4 tangent_attrib;
#endif

#if defined(COLOR_USED)
layout(location = 3) in vec4 color_attrib;
#endif

#ifdef UV_USED
layout(location = 4) in vec2 uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(MODE_RENDER_MATERIAL)
layout(location = 5) in vec2 uv2_attrib;
#endif // MODE_RENDER_MATERIAL

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

#if defined(BONES_USED) || defined(USE_PARTICLE_TRAILS)
layout(location = 10) in uvec4 bone_attrib;
#endif

#if defined(WEIGHTS_USED) || defined(USE_PARTICLE_TRAILS)
layout(location = 11) in vec4 weight_attrib;
#endif

#if defined(MODE_RENDER_MOTION_VECTORS) && !defined(USE_INSTANCE192_OUTPUT)
layout(location = 12) in vec4 previous_vertex_attrib;

#if defined(NORMAL_USED) || defined(TANGENT_USED)
layout(location = 13) in vec4 previous_normal_attrib;
#ifdef USE_INDEXED144_VERTEX_INPUT
layout(location = 14) in vec4 previous_tangent_attrib;
#endif
#endif

#endif // MODE_RENDER_MOTION_VECTORS

void axis_angle_to_tbn(vec3 axis, float angle, out vec3 tangent, out vec3 binormal, out vec3 normal) {
	float c = cos(angle);
	float s = sin(angle);
	vec3 omc_axis = (1.0 - c) * axis;
	vec3 s_axis = s * axis;
	tangent = omc_axis.xxx * axis + vec3(c, -s_axis.z, s_axis.y);
	binormal = omc_axis.yyy * axis + vec3(s_axis.z, c, -s_axis.x);
	normal = omc_axis.zzz * axis + vec3(-s_axis.y, s_axis.x, c);
}

/* Varyings */
#ifdef USE_INSTANCE192_OUTPUT
layout(location = 10) out flat uvec2 instance192_interp;
#endif


layout(location = 0) out vec3 vertex_interp;

#ifdef NORMAL_USED
layout(location = 1) out vec3 normal_interp;
#endif

#if defined(COLOR_USED)
layout(location = 2) out vec4 color_interp;
#endif

#ifdef UV_USED
layout(location = 3) out vec2 uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
layout(location = 4) out vec2 uv2_interp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
layout(location = 5) out vec3 tangent_interp;
layout(location = 6) out vec3 binormal_interp;
#endif
#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)
layout(location = 7) out vec4 diffuse_light_interp;
layout(location = 8) out vec4 specular_light_interp;
// Simplified versions of light functions intended for the vertex shader.

// Eyeballed approximation of `exp2(15.0 * (1.0 - roughness) + 1.0) * 0.25`.
// Uses slightly more FMA instructions (2x rate) to avoid special instructions (0.25x rate).
// Range is reduced to [0.64,4977] from [068,2,221,528] which makes mediump feasible for the rest of the shader.
half roughness_to_shininess(half roughness) {
	half r = half(1.2) - roughness;
	half r2 = r * r;
	return r * r2 * r2 * half(2000.0);
}

void light_compute_vertex(hvec3 N, hvec3 L, hvec3 V, hvec3 light_color, bool is_directional, half roughness,
		inout hvec3 diffuse_light, inout hvec3 specular_light) {
	half NdotL = min(dot(N, L), half(1.0));
	half cNdotL = max(NdotL, half(0.0)); // clamped NdotL

#if defined(DIFFUSE_LAMBERT_WRAP)
	// Energy conserving lambert wrap shader.
	// https://web.archive.org/web/20210228210901/http://blog.stevemcauley.com/2011/12/03/energy-conserving-wrapped-diffuse/
	half diffuse_brdf_NL = max(half(0.0), (cNdotL + roughness) / ((half(1.0) + roughness) * (half(1.0) + roughness))) * half(1.0 / M_PI);
#else
	// lambert
	half diffuse_brdf_NL = cNdotL * half(1.0 / M_PI);
#endif

	diffuse_light += light_color * diffuse_brdf_NL;

#if !defined(SPECULAR_DISABLED)
	half specular_brdf_NL = half(0.0);
	// Normalized blinn always unless disabled.
	hvec3 H = normalize(V + L);
	half cNdotH = clamp(dot(N, H), half(0.0), half(1.0));
	half shininess = roughness_to_shininess(roughness);
	half blinn = pow(cNdotH, shininess);
	blinn *= (shininess + half(2.0)) * half(1.0 / (8.0 * M_PI)) * cNdotL;
	specular_brdf_NL = blinn;
	specular_light += specular_brdf_NL * light_color;
#endif
}

half get_omni_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return half(nd * pow(max(distance, 0.0001), -decay));
}

void light_process_omni_vertex(uint idx, vec3 vertex, hvec3 eye_vec, hvec3 normal, half roughness,
		inout hvec3 diffuse_light, inout hvec3 specular_light) {
	vec3 light_rel_vec = omni_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	hvec3 light_rel_vec_norm = hvec3(light_rel_vec / light_length);
	half omni_attenuation = get_omni_attenuation(light_length, omni_lights.data[idx].inv_radius, omni_lights.data[idx].attenuation);
	hvec3 color = hvec3(omni_lights.data[idx].color * omni_attenuation);

	light_compute_vertex(normal, light_rel_vec_norm, eye_vec, color, false, roughness,
			diffuse_light,
			specular_light);
}

void light_process_spot_vertex(uint idx, vec3 vertex, hvec3 eye_vec, hvec3 normal, half roughness,
		inout hvec3 diffuse_light,
		inout hvec3 specular_light) {
	vec3 light_rel_vec = spot_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	hvec3 light_rel_vec_norm = hvec3(light_rel_vec / light_length);
	half spot_attenuation = get_omni_attenuation(light_length, spot_lights.data[idx].inv_radius, spot_lights.data[idx].attenuation);
	hvec3 spot_dir = hvec3(spot_lights.data[idx].direction);

	half cone_angle = half(spot_lights.data[idx].cone_angle);
	half scos = max(dot(-light_rel_vec_norm, spot_dir), cone_angle);

	// This conversion to a highp float is crucial to prevent light leaking due to precision errors.
	float spot_rim = max(1e-4, float(half(1.0) - scos) / float(half(1.0) - cone_angle));
	spot_attenuation *= half(1.0 - pow(spot_rim, spot_lights.data[idx].cone_attenuation));

	hvec3 color = hvec3(spot_lights.data[idx].color * spot_attenuation);

	light_compute_vertex(normal, light_rel_vec_norm, eye_vec, color, false, roughness,
			diffuse_light, specular_light);
}

#endif // !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)
#ifdef MATERIAL_UNIFORMS_USED
/* clang-format off */
layout(set = MATERIAL_UNIFORM_SET, binding = 0, std140) uniform MaterialUniforms {

} material;
/* clang-format on */
#endif

#ifdef MODE_DUAL_PARABOLOID

layout(location = 9) out float dp_clip;

#endif

#if defined(MODE_RENDER_MOTION_VECTORS)
layout(location = 12) out highp vec4 screen_position;
layout(location = 13) out highp vec4 prev_screen_position;
#endif

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
layout(location = 14) out vec2 point_coord_interp;
#endif

invariant gl_Position;

layout(location=15) out vec4 witness;

#define scene_data scene_data_block.data

#ifdef USE_DOUBLE_PRECISION
// Helper functions for emulating double precision when adding floats.
vec3 quick_two_sum(vec3 a, vec3 b, out vec3 out_p) {
	vec3 s = a + b;
	out_p = b - (s - a);
	return s;
}

vec3 two_sum(vec3 a, vec3 b, out vec3 out_p) {
	vec3 s = a + b;
	vec3 v = s - a;
	out_p = (a - (s - v)) + (b - v);
	return s;
}

vec3 double_add_vec3(vec3 base_a, vec3 prec_a, vec3 base_b, vec3 prec_b, out vec3 out_precision) {
	vec3 s, t, se, te;
	s = two_sum(base_a, base_b, se);
	t = two_sum(prec_a, prec_b, te);
	se += t;
	s = quick_two_sum(s, se, se);
	se += te;
	s = quick_two_sum(s, se, out_precision);
	return s;
}
#endif

uint multimesh_stride() {
	uint stride = sc_multimesh_format_2d() ? 2 : 3;
	stride += sc_multimesh_has_color() ? 1 : 0;
	stride += sc_multimesh_has_custom_data() ? 1 : 0;
	return stride;
}

void _unpack_vertex_attributes(vec4 p_vertex_in, vec3 p_compressed_aabb_position, vec3 p_compressed_aabb_size,
#if defined(NORMAL_USED) || defined(TANGENT_USED)
		vec4 p_normal_in,
#ifdef USE_INDEXED144_VERTEX_INPUT
		vec4 p_tangent_in,
#endif
#ifdef NORMAL_USED
		out vec3 r_normal,
#endif
		out vec3 r_tangent,
		out vec3 r_binormal,
#endif
		out vec3 r_vertex) {
#ifdef USE_INDEXED144_VERTEX_INPUT
	r_vertex = p_vertex_in.xyz;
#ifdef NORMAL_USED
	r_normal = p_normal_in.xyz;
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
	r_tangent = p_tangent_in.xyz;
	r_binormal = normalize(cross(p_normal_in.xyz, p_tangent_in.xyz)) * p_tangent_in.w;
#endif
#else

	r_vertex = p_vertex_in.xyz * p_compressed_aabb_size + p_compressed_aabb_position;
#ifdef NORMAL_USED
	r_normal = oct_to_vec3(p_normal_in.xy * 2.0 - 1.0);
#endif

#if defined(NORMAL_USED) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	float binormal_sign;

	// This works because the oct value (0, 1) maps onto (0, 0, -1) which encodes to (1, 1).
	// Accordingly, if p_normal_in.z contains octahedral values, it won't equal (0, 1).
	if (p_normal_in.z > 0.0 || p_normal_in.w < 1.0) {
		// Uncompressed format.
		vec2 signed_tangent_attrib = p_normal_in.zw * 2.0 - 1.0;
		r_tangent = oct_to_vec3(vec2(signed_tangent_attrib.x, abs(signed_tangent_attrib.y) * 2.0 - 1.0));
		binormal_sign = sign(signed_tangent_attrib.y);
		r_binormal = normalize(cross(r_normal, r_tangent) * binormal_sign);
	} else {
		// Compressed format.
		float angle = p_vertex_in.w;
		binormal_sign = angle > 0.5 ? 1.0 : -1.0; // 0.5 does not exist in UNORM16, so values are either greater or smaller.
		angle = abs(angle * 2.0 - 1.0) * M_PI; // 0.5 is basically zero, allowing to encode both signs reliably.
		vec3 axis = r_normal;
		axis_angle_to_tbn(axis, angle, r_tangent, r_binormal, r_normal);
		r_binormal *= binormal_sign;
	}
#endif
#endif
}

void vertex_shader(in vec3 vertex,
#ifdef NORMAL_USED
		in vec3 normal_highp,
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
		in vec3 tangent_highp,
		in vec3 binormal_highp,
#endif
		in uint instance_index, in uint multimesh_offset,
#ifdef USE_INSTANCE192_OUTPUT
		in bool instance192_previous_record,
#endif
		in mat3x4 in_model_matrix,
#ifdef USE_DOUBLE_PRECISION
		in vec3 model_precision,
		in vec3 view_precision,
#endif
#ifdef MODE_DUAL_PARABOLOID
		in float dual_paraboloid_side,
		in float z_far,
#endif
#if defined(MODE_RENDER_DEPTH) || defined(MODE_RENDER_MATERIAL)
		in uint scene_flags,
#endif
		in mat4 projection_matrix,
		in mat4 inv_projection_matrix,
#ifdef USE_MULTIVIEW
		in vec4 scene_eye_offset,
#endif
		in mat3x4 in_view_matrix,
		in mat3x4 in_inv_view_matrix,
		in vec2 viewport_size,
		in uint scene_directional_light_count,
		out vec4 screen_position_output) {
	vec4 instance_custom = vec4(0.0);
#if defined(COLOR_USED)
	vec4 color_highp = color_attrib;
#endif

	mat4 inv_view_matrix = transpose(mat4(in_inv_view_matrix[0],
			in_inv_view_matrix[1],
			in_inv_view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#ifndef USE_INSTANCE192_OUTPUT
	mat4 model_matrix = transpose(mat4(in_model_matrix[0],
			in_model_matrix[1],
			in_model_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

	mat3 model_normal_matrix;
	if (bool(instances.data[instance_index].flags & INSTANCE_FLAGS_NON_UNIFORM_SCALE)) {
		model_normal_matrix = transpose(inverse(mat3(model_matrix)));
	} else {
		model_normal_matrix = mat3(model_matrix);
	}

#endif

#ifdef USE_INSTANCE192_OUTPUT
	Instance192Record instance192_record = instance192_read(uint(gl_InstanceIndex), instance192_previous_record);
	// C already includes Host for Local output. Never multiply the scene Host.
	mat4 model_matrix = instance192_record.model;
	mat3 model_normal_matrix = instance192_record.normal;
	mat4 instance192_inverse_model_matrix = instance192_record.inverse_model;
	instance_custom = instance192_record.custom;
#ifdef COLOR_USED
	color_highp *= instance192_record.color;
#endif
#endif

	mat4 matrix;
	mat4 read_model_matrix = model_matrix;

#ifndef USE_INSTANCE192_OUTPUT
	if (sc_multimesh()) {
		//multimesh, instances are for it

#ifdef USE_PARTICLE_TRAILS
		uint trail_size = (instances.data[instance_index].flags >> INSTANCE_FLAGS_PARTICLE_TRAIL_SHIFT) & INSTANCE_FLAGS_PARTICLE_TRAIL_MASK;
		uint stride = 3 + 1 + 1; //particles always uses this format

		uint offset = trail_size * stride * INSTANCE_INDEX;

#ifdef COLOR_USED
		vec4 pcolor;
#endif
		{
			uint boffset = offset + bone_attrib.x * stride;
			matrix = mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], transforms.data[boffset + 2], vec4(0.0, 0.0, 0.0, 1.0)) * weight_attrib.x;
#ifdef COLOR_USED
			pcolor = transforms.data[boffset + 3] * weight_attrib.x;
#endif
		}
		if (weight_attrib.y > 0.001) {
			uint boffset = offset + bone_attrib.y * stride;
			matrix += mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], transforms.data[boffset + 2], vec4(0.0, 0.0, 0.0, 1.0)) * weight_attrib.y;
#ifdef COLOR_USED
			pcolor += transforms.data[boffset + 3] * weight_attrib.y;
#endif
		}
		if (weight_attrib.z > 0.001) {
			uint boffset = offset + bone_attrib.z * stride;
			matrix += mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], transforms.data[boffset + 2], vec4(0.0, 0.0, 0.0, 1.0)) * weight_attrib.z;
#ifdef COLOR_USED
			pcolor += transforms.data[boffset + 3] * weight_attrib.z;
#endif
		}
		if (weight_attrib.w > 0.001) {
			uint boffset = offset + bone_attrib.w * stride;
			matrix += mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], transforms.data[boffset + 2], vec4(0.0, 0.0, 0.0, 1.0)) * weight_attrib.w;
#ifdef COLOR_USED
			pcolor += transforms.data[boffset + 3] * weight_attrib.w;
#endif
		}

		instance_custom = transforms.data[offset + 4];

#ifdef COLOR_USED
		color_highp *= pcolor;
#endif

#else
		uint stride = multimesh_stride();
		uint offset = stride * (INSTANCE_INDEX + multimesh_offset);

		if (sc_multimesh_format_2d()) {
			matrix = mat4(transforms.data[offset + 0], transforms.data[offset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
			offset += 2;
		} else {
			matrix = mat4(transforms.data[offset + 0], transforms.data[offset + 1], transforms.data[offset + 2], vec4(0.0, 0.0, 0.0, 1.0));
			offset += 3;
		}

		if (sc_multimesh_has_color()) {
#ifdef COLOR_USED
			color_highp *= transforms.data[offset];
#endif
			offset += 1;
		}

		if (sc_multimesh_has_custom_data()) {
			instance_custom = transforms.data[offset];
		}

#endif
		//transpose
		matrix = transpose(matrix);

#if !defined(USE_DOUBLE_PRECISION) || defined(SKIP_TRANSFORM_USED) || defined(VERTEX_WORLD_COORDS_USED) || defined(MODEL_MATRIX_USED)
		// Normally we can bake the multimesh transform into the model matrix, but when using double precision
		// we avoid baking it in so we can emulate high precision.
		read_model_matrix = model_matrix * matrix;
#if !defined(USE_DOUBLE_PRECISION) || defined(SKIP_TRANSFORM_USED) || defined(VERTEX_WORLD_COORDS_USED)
		model_matrix = read_model_matrix;
#endif // !defined(USE_DOUBLE_PRECISION) || defined(SKIP_TRANSFORM_USED) || defined(VERTEX_WORLD_COORDS_USED)
#endif // !defined(USE_DOUBLE_PRECISION) || defined(SKIP_TRANSFORM_USED) || defined(VERTEX_WORLD_COORDS_USED) || defined(MODEL_MATRIX_USED)
		model_normal_matrix = model_normal_matrix * mat3(matrix);
	}
#endif

#ifdef INV_MODEL_MATRIX_USED
#ifdef USE_INSTANCE192_OUTPUT
	mat4 read_inv_model_matrix = instance192_record.inverse_model;
#else
	mat4 read_inv_model_matrix = inverse(read_model_matrix);
#endif
#endif

#ifdef UV_USED
	uv_interp = uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
	uv2_interp = uv2_attrib;
#endif

	vec4 uv_scale = instances.data[instance_index].uv_scale;

	if (uv_scale != vec4(0.0)) { // Compression enabled
#ifdef UV_USED
		uv_interp = (uv_interp - 0.5) * uv_scale.xy;
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP)
		uv2_interp = (uv2_interp - 0.5) * uv_scale.zw;
#endif
	}

#ifdef OVERRIDE_POSITION
	vec4 position = vec4(1.0);
#endif

#ifdef USE_MULTIVIEW
	vec3 eye_offset = scene_eye_offset.xyz;
#else
	vec3 eye_offset = vec3(0.0, 0.0, 0.0);
#endif // USE_MULTIVIEW

//using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)

	vertex = (model_matrix * vec4(vertex, 1.0)).xyz;

#ifdef NORMAL_USED
	normal_highp = model_normal_matrix * normal_highp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	tangent_highp = model_normal_matrix * tangent_highp;
	binormal_highp = model_normal_matrix * binormal_highp;

#endif
#endif

#ifdef Z_CLIP_SCALE_USED
	float z_clip_scale = 1.0;
#endif

	float roughness_highp = 1.0;

	mat4 read_view_matrix = transpose(mat4(in_view_matrix[0],
			in_view_matrix[1],
			in_view_matrix[2],
			vec4(0.0, 0.0, 0.0, 1.0)));

#ifdef USE_DOUBLE_PRECISION
	mat4 modelview = read_view_matrix * model_matrix;

	// We separate the basis from the origin because the basis is fine with single point precision.
	// Then we combine the translations from the model matrix and the view matrix using emulated doubles.
	// We add the result to the vertex and ignore the final lost precision.
	vec3 model_origin = model_matrix[3].xyz;
	if (sc_multimesh()) {
		modelview = modelview * matrix;

		vec3 instance_origin = mat3(model_matrix) * matrix[3].xyz;
		model_origin = double_add_vec3(model_origin, model_precision, instance_origin, vec3(0.0), model_precision);
	}

	// Overwrite the translation part of modelview with improved precision.
	vec3 temp_precision; // Will be ignored.
	modelview[3].xyz = double_add_vec3(model_origin, model_precision, inv_view_matrix[3].xyz, view_precision, temp_precision);
	modelview[3].xyz = mat3(read_view_matrix) * modelview[3].xyz;
#else
	mat4 modelview = read_view_matrix * model_matrix;
#endif
	mat3 modelview_normal = mat3(read_view_matrix) * model_normal_matrix;
	vec2 read_viewport_size = scene_data.viewport_size;

#ifdef POINT_SIZE_USED
	float point_size = 1.0;
#endif

	{
witness=color_highp+instance_custom+custom0_attrib+custom1_attrib+custom2_attrib+custom3_attrib; vertex += (read_inv_model_matrix*read_model_matrix*vec4(vertex,1.0)).xyz*0.001; normal_highp += model_normal_matrix[0]*0.001;
	}

#if defined(COLOR_USED)
	color_interp = hvec4(color_highp);
#endif

	half roughness = half(roughness_highp);

// using local coordinates (default)
#if !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

	vertex = (modelview * vec4(vertex, 1.0)).xyz;

#ifdef NORMAL_USED
	normal_highp = modelview_normal * normal_highp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

	binormal_highp = modelview_normal * binormal_highp;
	tangent_highp = modelview_normal * tangent_highp;
#endif
#endif // !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

//using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)

	vertex = (read_view_matrix * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
	normal_highp = (read_view_matrix * vec4(normal_highp, 0.0)).xyz;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(BENT_NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
	binormal_highp = (read_view_matrix * vec4(binormal_highp, 0.0)).xyz;
	tangent_highp = (read_view_matrix * vec4(tangent_highp, 0.0)).xyz;
#endif
#endif

	vertex_interp = vertex;

	// Normalize TBN vectors before interpolation, per MikkTSpace.
	// See: http://www.mikktspace.com/
#ifdef NORMAL_USED
	normal_interp = hvec3(normalize(normal_highp));
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) || defined(BENT_NORMAL_MAP_USED)
	tangent_interp = hvec3(normalize(tangent_highp));
	binormal_interp = hvec3(normalize(binormal_highp));
#endif

// VERTEX LIGHTING
#if !defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)
	hvec3 normal = hvec3(normal_interp);

#ifdef USE_MULTIVIEW
	hvec3 view = hvec3(-normalize(vertex_interp - eye_offset));
#else
	hvec3 view = hvec3(-normalize(vertex_interp));
#endif

	hvec4 diffuse_light = hvec4(0.0);
	hvec4 specular_light = hvec4(0.0);

	uint omni_light_count = sc_omni_lights(8);
	uvec2 omni_light_indices = instances.data[instance_index].omni_lights;
	for (uint i = 0; i < omni_light_count; i++) {
		uint light_index = (i > 3) ? ((omni_light_indices.y >> ((i - 4) * 8)) & 0xFF) : ((omni_light_indices.x >> (i * 8)) & 0xFF);
		if (i > 0 && light_index == 0xFF) {
			break;
		}

		light_process_omni_vertex(light_index, vertex, view, normal, roughness, diffuse_light.rgb, specular_light.rgb);
	}

	uint spot_light_count = sc_spot_lights(8);
	uvec2 spot_light_indices = instances.data[instance_index].spot_lights;
	for (uint i = 0; i < spot_light_count; i++) {
		uint light_index = (i > 3) ? ((spot_light_indices.y >> ((i - 4) * 8)) & 0xFF) : ((spot_light_indices.x >> (i * 8)) & 0xFF);
		if (i > 0 && light_index == 0xFF) {
			break;
		}

		light_process_spot_vertex(light_index, vertex, view, normal, roughness, diffuse_light.rgb, specular_light.rgb);
	}

	uint directional_lights_count = sc_directional_lights(scene_directional_light_count);
	if (directional_lights_count > 0) {
		// We process the first directional light separately as it may have shadows.
		hvec3 directional_diffuse = hvec3(0.0);
		hvec3 directional_specular = hvec3(0.0);

		for (uint i = 0; i < directional_lights_count; i++) {
			if (!bool(directional_lights.data[i].mask & instances.data[instance_index].layer_mask)) {
				continue; // Not masked, skip.
			}

			if (directional_lights.data[i].bake_mode == LIGHT_BAKE_STATIC && bool(instances.data[instance_index].flags & INSTANCE_FLAGS_USE_LIGHTMAP)) {
				continue; // Statically baked light and object uses lightmap, skip.
			}
			if (i == 0) {
				light_compute_vertex(normal, hvec3(directional_lights.data[0].direction), view,
						hvec3(directional_lights.data[0].color * directional_lights.data[0].energy),
						true, roughness,
						directional_diffuse,
						directional_specular);
			} else {
				light_compute_vertex(normal, hvec3(directional_lights.data[i].direction), view,
						hvec3(directional_lights.data[i].color * directional_lights.data[i].energy),
						true, roughness,
						diffuse_light.rgb,
						specular_light.rgb);
			}
		}

		// Calculate the contribution from the shadowed light so we can scale the shadows accordingly.
		half diff_avg = dot(diffuse_light.rgb, hvec3(0.33333));
		half diff_dir_avg = dot(directional_diffuse, hvec3(0.33333));
		if (diff_avg > half(0.0)) {
			diffuse_light.a = diff_dir_avg / (diff_avg + diff_dir_avg);
		} else {
			diffuse_light.a = half(1.0);
		}

		diffuse_light.rgb += directional_diffuse;

		half spec_avg = dot(specular_light.rgb, hvec3(0.33333));
		half spec_dir_avg = dot(directional_specular, hvec3(0.33333));
		if (spec_avg > half(0.0)) {
			specular_light.a = spec_dir_avg / (spec_avg + spec_dir_avg);
		} else {
			specular_light.a = half(1.0);
		}

		specular_light.rgb += directional_specular;
	}

	diffuse_light_interp = hvec4(diffuse_light);
	specular_light_interp = hvec4(specular_light);

#endif //!defined(MODE_RENDER_DEPTH) && !defined(MODE_UNSHADED) && defined(USE_VERTEX_LIGHTING)

#ifdef MODE_RENDER_DEPTH

#ifdef MODE_DUAL_PARABOLOID

	vertex_interp.z *= dual_paraboloid_side;

	dp_clip = vertex_interp.z; //this attempts to avoid noise caused by objects sent to the other parabolloid side due to bias

	//for dual paraboloid shadow mapping, this is the fastest but least correct way, as it curves straight edges

	vec3 vtx = vertex_interp;
	float distance = length(vtx);
	vtx = normalize(vtx);
	vtx.xy /= 1.0 - vtx.z;
	vtx.z = (distance / z_far);
	vtx.z = vtx.z * 2.0 - 1.0;
	vertex_interp = vtx;

#endif

#endif //MODE_RENDER_DEPTH

#ifdef OVERRIDE_POSITION
	gl_Position = position;
#else
	gl_Position = projection_matrix * vec4(vertex_interp, 1.0);
#endif // OVERRIDE_POSITION

#if defined(Z_CLIP_SCALE_USED) && !defined(SHADOW_PASS)
	gl_Position.z = mix(gl_Position.w, gl_Position.z, z_clip_scale);
#endif

#ifdef MODE_RENDER_DEPTH
	if (bool(scene_flags & SCENE_DATA_FLAGS_USE_PANCAKE_SHADOWS)) {
		if (gl_Position.z >= 0.9999) {
			gl_Position.z = 0.9999;
		}
	}
#endif // MODE_RENDER_DEPTH
#ifdef MODE_RENDER_MATERIAL
	if (bool(scene_flags & SCENE_DATA_FLAGS_USE_UV2_MATERIAL)) {
		vec2 uv_dest_attrib;
		if (uv_scale != vec4(0.0)) {
			uv_dest_attrib = (uv2_attrib.xy - 0.5) * uv_scale.zw;
		} else {
			uv_dest_attrib = uv2_attrib.xy;
		}

		vec2 uv_offset = unpackHalf2x16(draw_call.uv_offset);
		gl_Position.xy = (uv_dest_attrib + uv_offset) * 2.0 - 1.0;
		gl_Position.z = 0.00001;
		gl_Position.w = 1.0;
	}
#endif // MODE_RENDER_MATERIAL
#ifdef MODE_RENDER_MOTION_VECTORS
	screen_position_output = gl_Position;
#endif // MODE_RENDER_MOTION_VECTORS

#ifdef POINT_SIZE_USED
	if (sc_emulate_point_size) {
		vec2 point_coords[6] = vec2[](
				vec2(0, 1),
				vec2(0, 0),
				vec2(1, 1),
				vec2(0, 0),
				vec2(1, 0),
				vec2(1, 1));

		vec2 point_coord = point_coords[gl_VertexIndex % 6];
		gl_Position.xy += (point_coord * 2.0 - 1.0) * (point_size / scene_data.viewport_size) * gl_Position.w;

#ifdef POINT_COORD_USED
		point_coord_interp = point_coord;
#endif
	} else {
		gl_PointSize = point_size;
	}
#endif
}

void main() {
#ifdef USE_INSTANCE192_OUTPUT
	instance192_interp = uvec2(uint(gl_InstanceIndex), instance192_flags(uint(gl_InstanceIndex)));
#endif
#if defined(MODE_RENDER_MOTION_VECTORS)
	vec3 prev_vertex;
#ifdef NORMAL_USED
	vec3 prev_normal;
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
	vec3 prev_tangent;
	vec3 prev_binormal;
#endif

	_unpack_vertex_attributes(
#ifdef USE_INSTANCE192_OUTPUT
			vertex_angle_attrib,
#else
			previous_vertex_attrib,
#endif
			instances.data[draw_call.instance_index].compressed_aabb_position_pad.xyz,
			instances.data[draw_call.instance_index].compressed_aabb_size_pad.xyz,
#if defined(NORMAL_USED) || defined(TANGENT_USED)
#ifdef USE_INSTANCE192_OUTPUT
			axis_tangent_attrib,
#else
			previous_normal_attrib,
#endif
#ifdef USE_INDEXED144_VERTEX_INPUT
#ifdef USE_INSTANCE192_OUTPUT
			tangent_attrib,
#else
			previous_tangent_attrib,
#endif
#endif
#ifdef NORMAL_USED
			prev_normal,
#endif
			prev_tangent,
			prev_binormal,
#endif
			prev_vertex);

	vertex_shader(prev_vertex,
#ifdef NORMAL_USED
			prev_normal,
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
			prev_tangent,
			prev_binormal,
#endif
			draw_call.instance_index, draw_call.multimesh_motion_vectors_previous_offset,
#ifdef USE_INSTANCE192_OUTPUT
			true,
#endif
			instances.data[draw_call.instance_index].prev_transform,
#ifdef USE_DOUBLE_PRECISION
			instances.data[draw_call.instance_index].prev_model_precision.xyz,
			scene_data_block.prev_data.inv_view_precision,
#endif

#ifdef MODE_DUAL_PARABOLOID
			scene_data_block.prev_data.dual_paraboloid_side,
			scene_data_block.prev_data.z_far,
#endif
#if defined(MODE_RENDER_DEPTH) || defined(MODE_RENDER_MATERIAL)
			scene_data_block.prev_data.flags,
#endif
#ifdef USE_MULTIVIEW
			scene_data_block.prev_data.projection_matrix_view[ViewIndex],
			scene_data_block.prev_data.inv_projection_matrix_view[ViewIndex],
			scene_data_block.prev_data.eye_offset[ViewIndex],
#else
			scene_data_block.prev_data.projection_matrix,
			scene_data_block.prev_data.inv_projection_matrix,
#endif
			scene_data_block.prev_data.view_matrix,
			scene_data_block.prev_data.inv_view_matrix,
			scene_data_block.prev_data.viewport_size,
			scene_data_block.prev_data.directional_light_count,
			prev_screen_position);
#else
	// Unused output.
	vec4 screen_position;
#endif // MODE_RENDER_MOTION_VECTORS

	vec3 vertex;
#ifdef NORMAL_USED
	vec3 normal;
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
	vec3 tangent;
	vec3 binormal;
#endif

	_unpack_vertex_attributes(
			vertex_angle_attrib,
			instances.data[draw_call.instance_index].compressed_aabb_position_pad.xyz,
			instances.data[draw_call.instance_index].compressed_aabb_size_pad.xyz,
#if defined(NORMAL_USED) || defined(TANGENT_USED)
			axis_tangent_attrib,
#ifdef USE_INDEXED144_VERTEX_INPUT
			tangent_attrib,
#endif
#ifdef NORMAL_USED
			normal,
#endif
			tangent,
			binormal,
#endif
			vertex);

	vertex_shader(vertex,
#ifdef NORMAL_USED
			normal,
#endif
#if defined(NORMAL_USED) || defined(TANGENT_USED)
			tangent,
			binormal,
#endif
			draw_call.instance_index, draw_call.multimesh_motion_vectors_current_offset,
#ifdef USE_INSTANCE192_OUTPUT
			false,
#endif
			instances.data[draw_call.instance_index].transform,
#ifdef USE_DOUBLE_PRECISION
			instances.data[draw_call.instance_index].model_precision.xyz,
			scene_data_block.data.inv_view_precision,
#endif
#ifdef MODE_DUAL_PARABOLOID
			scene_data_block.data.dual_paraboloid_side,
			scene_data_block.data.z_far,
#endif
#if defined(MODE_RENDER_DEPTH) || defined(MODE_RENDER_MATERIAL)
			scene_data_block.data.flags,
#endif
#ifdef USE_MULTIVIEW
			scene_data_block.data.projection_matrix_view[ViewIndex],
			scene_data_block.data.inv_projection_matrix_view[ViewIndex],
			scene_data_block.data.eye_offset[ViewIndex],
#else
			scene_data_block.data.projection_matrix,
			scene_data_block.data.inv_projection_matrix,
#endif
			scene_data_block.data.view_matrix,
			scene_data_block.data.inv_view_matrix,
			scene_data_block.data.viewport_size,
			scene_data_block.data.directional_light_count,
			screen_position);
}

