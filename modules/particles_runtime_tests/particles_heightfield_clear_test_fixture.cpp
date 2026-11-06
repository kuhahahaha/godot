#include "particles_heightfield_clear_test_fixture.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <cstring>

void ParticlesHeightfieldClearTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesHeightfieldClearTestFixture::run);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesHeightfieldClearTestFixture::result);
}

Dictionary ParticlesHeightfieldClearTestFixture::result() const {
	MutexLock lock(result_mutex);
	return result_data.duplicate(true);
}

Error ParticlesHeightfieldClearTestFixture::run() {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	started = true;
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
	storage->particles_collision_set_box_extents(collision, Vector3(1, 1, 1));
	storage->particles_collision_set_height_field_resolution(collision, RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256);
	first_instance = storage->particles_collision_instance_create(collision);
	second_instance = storage->particles_collision_instance_create(collision);
	const RID first_framebuffer = storage->particles_collision_get_heightfield_framebuffer(first_instance);
	const RID second_framebuffer = storage->particles_collision_get_heightfield_framebuffer(second_instance);
	if (first_framebuffer.is_null() || second_framebuffer.is_null() || first_framebuffer == second_framebuffer) {
		return ERR_UNAVAILABLE;
	}
	const auto list = rd->draw_list_begin(first_framebuffer, RD::DRAW_CLEAR_DEPTH, {}, 0.75f);
	if (list == RD::INVALID_ID) {
		return ERR_UNAVAILABLE;
	}
	rd->draw_list_end();
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> empty;
	empty.set_page_pool(&pool);
	scene->render_particle_collider_heightfield(second_instance, Transform3D(), empty);

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
	pipeline = rd->compute_pipeline_create(shader);
	buffer = rd->storage_buffer_create(8);
	sampler = rd->sampler_create(RenderingDevice::SamplerState());
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
	{
		MutexLock lock(result_mutex);
		result_data["first_raw"] = raw[0];
		result_data["second_raw"] = raw[1];
		result_data["pass"] = p_bytes.size() == 8 && raw[0] >= 0.749f && raw[0] <= 0.751f && raw[1] >= -0.001f && raw[1] <= 0.001f;
	}
	rd->free_rid(uniform_set);
	rd->free_rid(sampler);
	rd->free_rid(buffer);
	rd->free_rid(pipeline);
	rd->free_rid(shader);
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	storage->particles_collision_instance_free(first_instance);
	storage->particles_collision_instance_free(second_instance);
	storage->particles_collision_free(collision);
}
