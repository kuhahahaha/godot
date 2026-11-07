#include "particles_heightfield_clear_test_fixture.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/mesh_storage.h"

#include <cstring>

void ParticlesHeightfieldClearTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesHeightfieldClearTestFixture::run);
	ClassDB::bind_method(D_METHOD("run_legacy_normal", "transform_case"), &ParticlesHeightfieldClearTestFixture::run_legacy_normal, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("run_transformed", "scale"), &ParticlesHeightfieldClearTestFixture::run_transformed);
	ClassDB::bind_method(D_METHOD("run_geometry", "height"), &ParticlesHeightfieldClearTestFixture::run_geometry);
	ClassDB::bind_method(D_METHOD("run_geometry_params", "height", "extents_y", "transform_y"), &ParticlesHeightfieldClearTestFixture::run_geometry_params);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesHeightfieldClearTestFixture::result);
}

Dictionary ParticlesHeightfieldClearTestFixture::result() const {
	MutexLock lock(result_mutex);
	return result_data.duplicate(true);
}

Error ParticlesHeightfieldClearTestFixture::run_legacy_normal(int p_transform_case) {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_transform_case < 0 || p_transform_case > 3) {
		return ERR_INVALID_PARAMETER;
	}
	legacy_normal_capture = true;
	transformed_capture = p_transform_case != 0;
	if (p_transform_case == 2) {
		capture_scale = Vector3(2, 2, 2);
	} else if (p_transform_case == 3) {
		capture_scale = Vector3(2, 3, 4);
	}
	return run_geometry(0.0f);
}

Error ParticlesHeightfieldClearTestFixture::run_transformed(const Vector3 &p_scale) {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	transformed_capture = true;
	capture_scale = p_scale;
	return run_geometry(0.5f);
}

Error ParticlesHeightfieldClearTestFixture::run_geometry(float p_height) {
	return run_geometry_params(p_height, 1.0f, 0.0f);
}

Error ParticlesHeightfieldClearTestFixture::run_geometry_params(float p_height, float p_extents_y, float p_transform_y) {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	geometry_height = p_height;
	capture_extents_y = p_extents_y;
	capture_transform_y = p_transform_y;
	geometry_capture = true;
	return run();
}

Error ParticlesHeightfieldClearTestFixture::run() {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	started = true;
	const Error error = run_capture();
	if (error != OK) {
		cleanup();
		MutexLock lock(result_mutex);
		result_data["error_code"] = error;
		result_data["pass"] = false;
	}
	return error;
}

Error ParticlesHeightfieldClearTestFixture::run_capture() {
	rd = RenderingDevice::get_singleton();
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *scene = RendererSceneRenderRD::get_singleton();
	if (!rd || !storage || !scene || !RenderingServer::get_singleton()->is_on_render_thread()) {
		return ERR_UNAVAILABLE;
	}
	{
		MutexLock lock(result_mutex);
		result_data["driver"] = rd->get_device_name();
	}
	// One collision base owns two independent instance capture targets.
	collision = storage->particles_collision_allocate();
	storage->particles_collision_initialize(collision);
	storage->particles_collision_set_collision_type(collision, RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE);
	storage->particles_collision_set_box_extents(collision, Vector3(1, capture_extents_y, 1));
	storage->particles_collision_set_height_field_resolution(collision, RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256);
	first_instance = storage->particles_collision_instance_create(collision);
	second_instance = storage->particles_collision_instance_create(collision);
	const RID first_framebuffer = storage->particles_collision_get_heightfield_framebuffer(first_instance);
	const RID second_framebuffer = storage->particles_collision_get_heightfield_framebuffer(second_instance);
	if (first_framebuffer.is_null() || second_framebuffer.is_null() || first_framebuffer == second_framebuffer) {
		return ERR_UNAVAILABLE;
	}
	// Seed BOTH targets: an empty renderer pass that omits its clear must fail.
	// The untouched first target also detects accidental target aliasing.
	// Transformed captures expect .75: seed the second target differently so
	// a renderer no-op cannot pass without actually clearing and drawing.
	for (const RID framebuffer : { first_framebuffer, second_framebuffer }) {
		const float seed = transformed_capture && framebuffer == second_framebuffer ? 0.125f : 0.75f;
		const auto list = rd->draw_list_begin(framebuffer, RD::DRAW_CLEAR_DEPTH, {}, seed);
		if (list == RD::INVALID_ID) {
			return ERR_UNAVAILABLE;
		}
		rd->draw_list_end();
	}
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	Transform3D capture_transform;
	capture_transform.origin.y = capture_transform_y;
	if (transformed_capture) {
		capture_transform.basis = Basis(Vector3(0, 0, 1), Math::PI / 2.0);
		capture_transform.basis.scale_local(capture_scale);
		capture_transform.origin = Vector3(10, 20, 30);
	}
	if (geometry_capture) {
		// Live field transform/extents determine the reversed orthographic depth.
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		arrays[RSE::ARRAY_VERTEX] = PackedVector3Array{
			Vector3(-1, 0, -1), Vector3(1, 0, -1), Vector3(1, 0, 1),
			Vector3(-1, 0, -1), Vector3(1, 0, 1), Vector3(-1, 0, 1)
		};
		RenderingServerTypes::SurfaceData surface;
		const Error prepared = RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLES, arrays);
		if (prepared != OK) {
			return prepared;
		}
		mesh = RSG::mesh_storage->mesh_allocate();
		RSG::mesh_storage->mesh_initialize(mesh);
		RSG::mesh_storage->mesh_add_surface(mesh, surface);
		geometry = scene->geometry_instance_create(mesh);
		if (!geometry) {
			return ERR_CANT_CREATE;
		}
		Transform3D transform;
		transform.origin.y = geometry_height;
		if (transformed_capture) {
			// Author geometry in field-local coordinates, then place it in world.
			transform = capture_transform * transform;
		}
		geometry->set_transform(transform, surface.aabb, transform.xform(surface.aabb));
		instances.push_back(geometry);
	}
	scene->render_particle_collider_heightfield(second_instance, capture_transform, instances);

	const auto *first_data = storage->particles_collision_instance_owner.get_or_null(first_instance);
	const auto *second_data = storage->particles_collision_instance_owner.get_or_null(second_instance);
	first_texture = first_data ? first_data->heightfield_texture : RID();
	second_texture = second_data ? second_data->heightfield_texture : RID();
	if (first_texture.is_null() || second_texture.is_null() || first_texture == second_texture) {
		return ERR_UNAVAILABLE;
	}
	if (legacy_normal_capture) {
		return run_legacy_process(capture_transform);
	}
	const String code = "#version 450\nlayout(local_size_x=1) in; layout(set=0,binding=0) uniform sampler s; layout(set=0,binding=1) uniform texture2D first_depth; layout(set=0,binding=2) uniform texture2D second_depth; layout(set=0,binding=3,std430) buffer Out { float values[2]; } out_data; void main() { out_data.values[0]=texture(sampler2D(first_depth,s),vec2(0.5)).r; out_data.values[1]=texture(sampler2D(second_depth,s),vec2(0.5)).r; }";
	RenderingDevice::ShaderStageSPIRVData stage;
	stage.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
	String error;
	stage.spirv = rd->shader_compile_spirv_from_source(stage.shader_stage, code, RenderingDevice::SHADER_LANGUAGE_GLSL, &error);
	if (stage.spirv.is_empty()) {
		MutexLock lock(result_mutex);
		result_data["error"] = error;
		return ERR_UNAVAILABLE;
	}
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	shader = rd->shader_create_from_spirv(stages, "HeightfieldClearSample");
	if (shader.is_null()) {
		return ERR_CANT_CREATE;
	}
	pipeline = rd->compute_pipeline_create(shader);
	buffer = rd->storage_buffer_create(8);
	sampler = rd->sampler_create(RenderingDevice::SamplerState());
	if (pipeline.is_null() || buffer.is_null() || sampler.is_null()) {
		return ERR_CANT_CREATE;
	}
	RenderingDevice::Uniform u;
	Vector<RenderingDevice::Uniform> uniforms;
	u.binding = 0;
	u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
	u.append_id(sampler);
	uniforms.push_back(u);
	u.clear_ids();
	u.binding = 1;
	u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
	u.append_id(first_texture);
	uniforms.push_back(u);
	u.clear_ids();
	u.binding = 2;
	u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
	u.append_id(second_texture);
	uniforms.push_back(u);
	u.clear_ids();
	u.binding = 3;
	u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
	u.append_id(buffer);
	uniforms.push_back(u);
	uniform_set = rd->uniform_set_create(uniforms, shader, 0);
	if (uniform_set.is_null() || pipeline.is_null()) {
		return ERR_UNAVAILABLE;
	}
	const auto compute = rd->compute_list_begin();
	if (compute == RD::INVALID_ID) {
		return ERR_UNAVAILABLE;
	}
	rd->compute_list_bind_compute_pipeline(compute, pipeline);
	rd->compute_list_bind_uniform_set(compute, uniform_set, 0);
	rd->compute_list_dispatch(compute, 1, 1, 1);
	rd->compute_list_end();
	return rd->buffer_get_data_async(buffer, callable_mp(this, &ParticlesHeightfieldClearTestFixture::sample_done), 0, 8);
}

Error ParticlesHeightfieldClearTestFixture::run_legacy_process(const Transform3D &p_field_transform) {
	using Storage = RendererRD::ParticlesStorage;
	auto *storage = Storage::get_singleton();
	auto *materials = RendererRD::MaterialStorage::get_singleton();
	legacy_shader = materials->shader_allocate();
	materials->shader_initialize(legacy_shader);
	// particles.glsl integrates the cleared zero velocity BEFORE start();
	// start then fixes the point, native collision runs, and process observes it.
	materials->shader_set_code(legacy_shader,
			"shader_type particles; uniform vec3 start_position; void start() { TRANSFORM = mat4(1.0); TRANSFORM[3].xyz = start_position; VELOCITY = vec3(0.0); } "
			"void process() { VELOCITY = COLLISION_NORMAL; CUSTOM = vec4(COLLISION_NORMAL, COLLIDED ? COLLISION_DEPTH : -1.0); "
			"TRANSFORM[3].xyz += COLLISION_NORMAL * COLLISION_DEPTH; }");
	legacy_material = materials->material_allocate();
	materials->material_initialize(legacy_material);
	materials->material_set_shader(legacy_material, legacy_shader);
	materials->material_set_param(legacy_material, "start_position", p_field_transform.origin);
	materials->_update_queued_materials();
	legacy_particle = storage->particles_allocate();
	storage->particles_initialize(legacy_particle);
	storage->particles_set_amount(legacy_particle, 1);
	storage->particles_set_lifetime(legacy_particle, 1.0);
	storage->particles_set_use_local_coordinates(legacy_particle, false);
	storage->particles_set_collision_base_size(legacy_particle, 0.1f);
	storage->particles_set_process_material(legacy_particle, legacy_material);
	storage->particles_set_emitting(legacy_particle, true);
	storage->particles_collision_instance_set_transform(second_instance, p_field_transform);
	storage->particles_collision_instance_set_active(second_instance, true);
	storage->particles_add_collision(legacy_particle, second_instance);
	auto *particle = storage->particles_owner.get_or_null(legacy_particle);
	storage->_particles_update_buffers(particle);
	// Match the existing native runtime fixture's original process setup.
	particle->frame_history.resize(1);
	particle->frame_history[0] = Storage::ParticlesFrameParams();
	particle->frame_history[0].frame = UINT32_MAX;
	particle->trail_params.resize(1);
	particle->frame_params_buffer = rd->storage_buffer_create(sizeof(Storage::ParticlesFrameParams));
	if (particle->particle_buffer.is_null() || particle->frame_params_buffer.is_null()) {
		return ERR_CANT_CREATE;
	}
	Storage::ParticleData sentinel{};
	for (float &value : sentinel.velocity) {
		value = -7.0f;
	}
	if (rd->buffer_update(particle->particle_buffer, 0, sizeof(sentinel), &sentinel) != OK) {
		return FAILED;
	}
	particle->phase = 0;
	particle->frame_counter = 0;
	particle->clear = true;
	if (!storage->_particles_process(particle, 1.0 / 60.0)) {
		return FAILED;
	}
	{
		MutexLock lock(result_mutex);
		result_data["legacy_normal"] = true;
		result_data["frame_counter"] = particle->frame_counter;
		result_data["collider_count"] = particle->frame_history[0].collider_count;
	}
	return rd->buffer_get_data_async(particle->particle_buffer, callable_mp(this, &ParticlesHeightfieldClearTestFixture::legacy_sample_done), 0, sizeof(Storage::ParticleData));
}

void ParticlesHeightfieldClearTestFixture::legacy_sample_done(const PackedByteArray &p_bytes) {
	RendererRD::ParticlesStorage::ParticleData value{};
	const bool size_ok = p_bytes.size() == sizeof(value);
	if (size_ok) {
		std::memcpy(&value, p_bytes.ptr(), sizeof(value));
	}
	const Vector3 normal(value.velocity[0], value.velocity[1], value.velocity[2]);
	const Vector3 position(value.xform[12], value.xform[13], value.xform[14]);
	const float depth = value.custom[3];
	// Independent world-plane oracle, not a copy of native collision math.
	// All rotated fields have the separately specified world plane x=10,
	// outward normal -X and world-space particle radius .1, regardless of scale.
	const Vector3 expected_normal = transformed_capture ? Vector3(-1, 0, 0) : Vector3(0, 1, 0);
	const Vector3 expected_position = transformed_capture ? Vector3(9.9f, 20, 30) : Vector3(0, 0.1f, 0);
	const bool passed = size_ok && (value.active & 1) && normal.distance_to(expected_normal) < 0.001f &&
			Math::abs(depth - 0.1f) < 0.001f && position.distance_to(expected_position) < 0.001f;
	cleanup();
	MutexLock lock(result_mutex);
	Array normal_values;
	Array position_values;
	for (int axis = 0; axis < 3; ++axis) {
		normal_values.push_back(normal[axis]);
		position_values.push_back(position[axis]);
	}
	result_data["normal"] = normal_values;
	result_data["position"] = position_values;
	result_data["depth"] = depth;
	result_data["active"] = bool(value.active & 1);
	result_data["pass"] = passed && int(result_data["frame_counter"]) == 1 && int(result_data["collider_count"]) == 1;
}

void ParticlesHeightfieldClearTestFixture::sample_done(const PackedByteArray &p_bytes) {
	float raw[2] = { 99.0f, 99.0f };
	if (p_bytes.size() == 8) {
		std::memcpy(raw, p_bytes.ptr(), 8);
	}

	cleanup();
	{
		MutexLock lock(result_mutex);
		result_data["first_raw"] = raw[0];
		result_data["second_raw"] = raw[1];
		// Independently specified local y=.5 with half-height 1 gives raw .75;
		// do not derive this oracle through the production camera or projection.
		const float expected = transformed_capture ? 0.75f : geometry_capture ? (geometry_height - capture_transform_y + capture_extents_y) / (2.0f * capture_extents_y) : 0.0f;
		result_data["pass"] = p_bytes.size() == 8 && raw[0] >= 0.749f && raw[0] <= 0.751f && Math::abs(raw[1] - expected) <= 0.001f;
	}
}

void ParticlesHeightfieldClearTestFixture::cleanup() {
	auto *particle_storage = RendererRD::ParticlesStorage::get_singleton();
	if (legacy_particle.is_valid()) {
		particle_storage->particles_free(legacy_particle);
		legacy_particle = RID();
	}
	auto *materials = RendererRD::MaterialStorage::get_singleton();
	if (legacy_material.is_valid()) {
		materials->material_free(legacy_material);
		legacy_material = RID();
	}
	if (legacy_shader.is_valid()) {
		materials->shader_free(legacy_shader);
		legacy_shader = RID();
	}
	if (rd) {
		for (RID *resource : { &uniform_set, &sampler, &buffer, &pipeline, &shader }) {
			if (resource->is_valid()) {
				rd->free_rid(*resource);
				*resource = RID();
			}
		}
	}
	if (geometry) {
		RendererSceneRenderRD::get_singleton()->geometry_instance_free(geometry);
		geometry = nullptr;
	}
	if (mesh.is_valid()) {
		RSG::mesh_storage->mesh_free(mesh);
		mesh = RID();
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (storage) {
		for (RID *instance : { &first_instance, &second_instance }) {
			if (instance->is_valid()) {
				storage->particles_collision_instance_free(*instance);
				*instance = RID();
			}
		}
		if (collision.is_valid()) {
			storage->particles_collision_free(collision);
			collision = RID();
		}
	}
}
