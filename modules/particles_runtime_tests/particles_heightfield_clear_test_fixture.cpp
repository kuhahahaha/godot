#include "particles_heightfield_clear_test_fixture.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/mesh_storage.h"

#include <cstring>

void ParticlesHeightfieldClearTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesHeightfieldClearTestFixture::run);
	ClassDB::bind_method(D_METHOD("run_geometry", "height"), &ParticlesHeightfieldClearTestFixture::run_geometry);
	ClassDB::bind_method(D_METHOD("run_geometry_params", "height", "extents_y", "transform_y"), &ParticlesHeightfieldClearTestFixture::run_geometry_params);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesHeightfieldClearTestFixture::result);
}

Dictionary ParticlesHeightfieldClearTestFixture::result() const {
	MutexLock lock(result_mutex);
	return result_data.duplicate(true);
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
	for (const RID framebuffer : { first_framebuffer, second_framebuffer }) {
		const auto list = rd->draw_list_begin(framebuffer, RD::DRAW_CLEAR_DEPTH, {}, 0.75f);
		if (list == RD::INVALID_ID) {
			return ERR_UNAVAILABLE;
		}
		rd->draw_list_end();
	}
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	if (geometry_capture) {
		// Fixed world y maps to reversed orthographic depth (y + 1) / 2.
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
		geometry->set_transform(transform, surface.aabb, transform.xform(surface.aabb));
		instances.push_back(geometry);
	}
	Transform3D capture_transform;
	capture_transform.origin.y = capture_transform_y;
	scene->render_particle_collider_heightfield(second_instance, capture_transform, instances);

	const auto *first_data = storage->particles_collision_instance_owner.get_or_null(first_instance);
	const auto *second_data = storage->particles_collision_instance_owner.get_or_null(second_instance);
	first_texture = first_data ? first_data->heightfield_texture : RID();
	second_texture = second_data ? second_data->heightfield_texture : RID();
	if (first_texture.is_null() || second_texture.is_null() || first_texture == second_texture) {
		return ERR_UNAVAILABLE;
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
		const float expected = geometry_capture ? (geometry_height - capture_transform_y + capture_extents_y) / (2.0f * capture_extents_y) : 0.0f;
		result_data["pass"] = p_bytes.size() == 8 && raw[0] >= 0.749f && raw[0] <= 0.751f && Math::abs(raw[1] - expected) <= 0.001f;
	}
}

void ParticlesHeightfieldClearTestFixture::cleanup() {
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
