#include "particles_heightfield_clear_test_fixture.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/height_capture_checked_sink.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/mesh_storage.h"

#include <cstring>

void ParticlesHeightfieldClearTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_frame_resources"), &ParticlesHeightfieldClearTestFixture::run_frame_resources);
	ClassDB::bind_method(D_METHOD("poll_frame_resources"), &ParticlesHeightfieldClearTestFixture::poll_frame_resources);
	ClassDB::bind_method(D_METHOD("run"), &ParticlesHeightfieldClearTestFixture::run);
	ClassDB::bind_method(D_METHOD("run_legacy_neighbor", "case"), &ParticlesHeightfieldClearTestFixture::run_legacy_neighbor);
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

Error ParticlesHeightfieldClearTestFixture::run_legacy_neighbor(int p_case) {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_case < 0 || p_case > 5) {
		return ERR_INVALID_PARAMETER;
	}
	legacy_neighbor_case = p_case;
	return run();
}

Error ParticlesHeightfieldClearTestFixture::run_legacy_normal(int p_transform_case) {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_transform_case < 0 || p_transform_case > 4) {
		return ERR_INVALID_PARAMETER;
	}
	legacy_normal_capture = true;
	legacy_transform_case = p_transform_case;
	if (p_transform_case == 2) {
		capture_scale = Vector3(2, 2, 2);
	} else if (p_transform_case == 3 || p_transform_case == 4) {
		capture_scale = Vector3(2, 1, 3);
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
	if (legacy_neighbor_case >= 0) {
		return run_neighbor_setup();
	}
	// One collision base owns two independent instance capture targets.
	collision = storage->particles_collision_allocate();
	storage->particles_collision_initialize(collision);
	storage->particles_collision_set_collision_type(collision, RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE);
	storage->particles_collision_set_box_extents(collision, legacy_normal_capture && legacy_transform_case == 4 ? Vector3(2, 2, 2) : Vector3(1, capture_extents_y, 1));
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
		const float seed = (transformed_capture || legacy_normal_capture) && framebuffer == second_framebuffer ? 0.125f : 0.75f;
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
	if (legacy_normal_capture) {
		if (legacy_transform_case == 1 || legacy_transform_case == 4) {
			capture_transform.basis = Basis(Vector3(0, 0, 1), Math::PI / 2.0);
		}
		capture_transform.basis.scale_local(capture_scale);
	} else if (transformed_capture) {
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
		if (legacy_normal_capture && legacy_transform_case == 4) {
			// Contract plane y=.5*x+.25*z over local x/z in [-2,2].
			arrays[RSE::ARRAY_VERTEX] = PackedVector3Array{
				Vector3(-2, -1.5f, -2), Vector3(2, 0.5f, -2), Vector3(2, 1.5f, 2),
				Vector3(-2, -1.5f, -2), Vector3(2, 1.5f, 2), Vector3(-2, -0.5f, 2)
			};
		}
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
		if (transformed_capture || legacy_normal_capture) {
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
	return sample_depth_pair();
}

Error ParticlesHeightfieldClearTestFixture::sample_depth_pair() {
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

Error ParticlesHeightfieldClearTestFixture::run_frame_resources() {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	started = true;
	{
		MutexLock lock(result_mutex);
		frame_resources_capture = true;
	}
	rd = RenderingDevice::get_singleton();
	if (!rd || !RenderingServer::get_singleton()->is_on_render_thread()) {
		MutexLock lock(result_mutex);
		result_data["pass"] = false;
		result_data["error_code"] = ERR_UNAVAILABLE;
		return ERR_UNAVAILABLE;
	}
	using Resources = RendererRD::ParticlesStorage::HeightFrameResources;
	Error error = Resources::allocate(*rd, 32, 32, frame_resources_a);
	if (error == OK) {
		error = Resources::allocate(*rd, 32, 32, frame_resources_b);
	}
	if (error == OK) {
		first_texture = frame_resources_a->get_depth();
		second_texture = frame_resources_b->get_depth();
		retained_frame_resources_a = frame_resources_a;
		Resources::Observation before;
		Resources::Observation after;
		error = frame_resources_a->observe(before);
		const Error busy = frame_resources_a->retire_step();
		const Error after_error = frame_resources_a->observe(after);
		const bool unchanged = error == OK && after_error == OK && busy == ERR_BUSY &&
				before.state == Resources::State::ALLOCATED && before.state == after.state &&
				before.depth.rid == after.depth.rid && before.depth.serial == after.depth.serial && before.depth.state == after.depth.state &&
				before.framebuffer.rid == after.framebuffer.rid && before.framebuffer.serial == after.framebuffer.serial && before.framebuffer.state == after.framebuffer.state &&
				first_texture == frame_resources_a->get_depth();
		const bool distinct = first_texture != second_texture &&
				frame_resources_a->get_framebuffer() != frame_resources_b->get_framebuffer();
		{
			MutexLock lock(result_mutex);
			result_data["driver"] = rd->get_device_name();
			result_data["retained_busy"] = unchanged;
			result_data["distinct_rids"] = distinct;
			result_data["first_depth_rid"] = int64_t(first_texture.get_id());
			result_data["second_depth_rid"] = int64_t(second_texture.get_id());
			result_data["first_framebuffer_rid"] = int64_t(frame_resources_a->get_framebuffer().get_id());
			result_data["second_framebuffer_rid"] = int64_t(frame_resources_b->get_framebuffer().get_id());
		}
		if (!unchanged || !distinct) {
			error = FAILED;
		}
	}
	if (error == OK) {
		// Independent raw constants; no height algorithm computes the oracle.
		const RID targets[] = { frame_resources_a->get_framebuffer(), frame_resources_b->get_framebuffer() };
		const float clear_depths[] = { 0.75f, 0.25f };
		Array checked_clears;
		for (uint32_t i = 0; i < 2; i++) {
			if (i == 1) {
				// Explicit test cut after a real empty begin. Cleanup end is an
				// original checked receipt, never a successful business end.
				RendererRD::HeightCaptureCheckedSink cut(*rd, 4, 0);
				error = cut.begin(targets[i], RD::DRAW_CLEAR_DEPTH, {}, 0.125f);
				if (error == OK) {
					cut.reject(ERR_CANT_CREATE);
				}
				const Error finished = cut.finish();
				const auto &cleanup_record = cut.get_cleanup_record();
				const bool recovered = finished == ERR_CANT_CREATE && cut.get_error() == ERR_CANT_CREATE &&
						cut.has_cleanup_attempt() && cut.is_restored() && !cut.has_open_list() && !cut.is_restoration_unknown() &&
						cleanup_record.draw.effect == RendererRD::HeightCaptureCheckedSink::Effect::Recorded && cleanup_record.draw.capture_complete && cleanup_record.draw.draw_handle.is_valid();
				{
					MutexLock lock(result_mutex);
					result_data["failure_cut_restored"] = recovered;
				}
				if (!recovered) {
					error = FAILED;
					break;
				}
				const auto ordinary = rd->draw_list_begin(targets[i], RD::DRAW_CLEAR_DEPTH, {}, 0.5f);
				if (ordinary == RD::INVALID_ID) {
					error = ERR_CANT_CREATE;
					break;
				}
				rd->draw_list_end();
				{
					MutexLock lock(result_mutex);
					result_data["ordinary_after_cut"] = true;
				}
			}
			RendererRD::HeightCaptureCheckedSink sink(*rd, 4, 0);
			error = sink.begin(targets[i], RD::DRAW_CLEAR_DEPTH, {}, clear_depths[i]);
			const Error finished = sink.finish();
			const bool count_ok = sink.get_business_count() >= 2;
			bool recorded = error == OK && finished == OK && sink.get_error() == OK && sink.is_restored() && count_ok;
			if (count_ok) {
				const auto &begin = sink.get_business_record(0).begin;
				const auto &end_record = sink.get_business_record(sink.get_business_count() - 1).draw;
				recorded = recorded && begin.effect == RendererRD::HeightCaptureCheckedSink::Effect::Recorded && begin.capture_complete && begin.begin_handle.is_valid() && begin.clear_handle.is_valid() &&
						end_record.effect == RendererRD::HeightCaptureCheckedSink::Effect::Recorded && end_record.capture_complete && end_record.draw_handle.is_valid();
			}
			checked_clears.push_back(recorded);
			if (!recorded) {
				error = FAILED;
				break;
			}
		}
		MutexLock lock(result_mutex);
		result_data["checked_clears"] = checked_clears;
	}

	if (error == OK) {
		error = sample_depth_pair();
	}
	if (error != OK) {
		cleanup();
		retained_frame_resources_a.unref();
		MutexLock lock(result_mutex);
		frame_resources_error = error;
		frame_resources_sampled = true;
		result_data["sample_pass"] = false;
		result_data["error_code"] = error;
	}
	return error;
}

void ParticlesHeightfieldClearTestFixture::poll_frame_resources() {
	{
		MutexLock lock(result_mutex);
		if (!frame_resources_capture || !frame_resources_sampled || frame_resources_poll_pending || result_data.has("pass")) {
			return;
		}
		frame_resources_poll_pending = true;
	}
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &ParticlesHeightfieldClearTestFixture::retire_frame_resources));
}

void ParticlesHeightfieldClearTestFixture::retire_frame_resources() {
	using Resources = RendererRD::ParticlesStorage::HeightFrameResources;
	Error first_error = OK;
	bool returned = true;
	Array reports;
	for (const Ref<Resources> *owner : { &frame_resources_a, &frame_resources_b }) {
		if (owner->is_null()) {
			continue;
		}
		const Error retired = (*owner)->retire_step();
		Resources::Observation observation;
		const Error observed = (*owner)->observe(observation);
		if ((retired != OK && retired != ERR_BUSY) || observed != OK) {
			first_error = retired != OK && retired != ERR_BUSY ? retired : observed;
		}
		for (const auto &report : { observation.depth, observation.framebuffer }) {
			if (!report.rid) {
				continue; // Uncreated allocation suffix has no physical-return claim.
			}
			Dictionary entry;
			entry["rid"] = int64_t(report.rid);
			entry["serial"] = int64_t(report.serial);
			entry["returned"] = report.state == NativeDisposalWitness::State::RETURNED;
			reports.push_back(entry);
			returned = returned && report.state == NativeDisposalWitness::State::RETURNED;
		}
		returned = returned && observation.state == Resources::State::RELEASED;
	}
	MutexLock lock(result_mutex);
	frame_resources_poll_pending = false;
	result_data["physical_reports"] = reports;
	if (first_error != OK) {
		frame_resources_error = first_error;
		result_data["error_code"] = first_error;
	}
	if (returned) {
		result_data["all_physical_returned"] = reports.size() == 4;
		result_data["pass"] = reports.size() == 4 && frame_resources_error == OK && bool(result_data.get("sample_pass", false)) &&
				bool(result_data.get("retained_busy", false)) && bool(result_data.get("distinct_rids", false)) &&
				bool(result_data.get("failure_cut_restored", false)) && bool(result_data.get("ordinary_after_cut", false));
		frame_resources_a.unref();
		frame_resources_b.unref();
	}
}

Error ParticlesHeightfieldClearTestFixture::run_neighbor_setup() {
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	auto *textures = RendererRD::TextureStorage::get_singleton();
	const RSE::ParticlesCollisionType types[] = {
		RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE, RSE::PARTICLES_COLLISION_TYPE_BOX_COLLIDE,
		RSE::PARTICLES_COLLISION_TYPE_SDF_COLLIDE, RSE::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT,
		RSE::PARTICLES_COLLISION_TYPE_BOX_ATTRACT, RSE::PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT
	};
	collision = storage->particles_collision_allocate();
	storage->particles_collision_initialize(collision);
	storage->particles_collision_set_collision_type(collision, types[legacy_neighbor_case]);
	storage->particles_collision_set_box_extents(collision, Vector3(1, 1, 1));
	Transform3D source_transform;
	if (legacy_neighbor_case <= 1) {
		storage->particles_collision_set_sphere_radius(collision, 1.0);

	} else if (legacy_neighbor_case == 3 || legacy_neighbor_case == 4) {
		storage->particles_collision_set_sphere_radius(collision, 2.0);
		storage->particles_collision_set_box_extents(collision, Vector3(2, 2, 2));
		storage->particles_collision_set_attractor_strength(collision, 2.0);
		storage->particles_collision_set_attractor_attenuation(collision, 1.0);
		storage->particles_collision_set_attractor_directionality(collision, 0.0);

	}
	if (legacy_neighbor_case == 2 || legacy_neighbor_case == 5) {
		const bool sdf = legacy_neighbor_case == 2;
		const int size = sdf ? 16 : 2;
		storage->particles_collision_set_box_extents(collision, Vector3(2, 2, 2));
		const int channels = sdf ? 1 : 3;
		Vector<Ref<Image>> slices;
		for (int z = 0; z < size; ++z) {
			PackedByteArray bytes;
			bytes.resize(size * size * channels * sizeof(float));
			for (int y = 0; y < size; ++y) {
				for (int x = 0; x < size; ++x) {
					const float texel[3] = { sdf ? -2.0f + (x + 0.5f) * 4.0f / size : 0.0f, 0.5f, 0.5f };
					std::memcpy(bytes.ptrw() + (y * size + x) * channels * sizeof(float), texel, channels * sizeof(float));
				}
			}
			slices.push_back(Image::create_from_data(size, size, false, sdf ? Image::FORMAT_RF : Image::FORMAT_RGBF, bytes));
		}
		legacy_texture = textures->texture_allocate();
		textures->texture_3d_initialize(legacy_texture, sdf ? Image::FORMAT_RF : Image::FORMAT_RGBF, size, size, size, false, slices);
		if (textures->texture_get_type(legacy_texture) != RendererRD::TextureStorage::TYPE_3D ||
				!rd->texture_is_valid(textures->texture_get_rd_texture(legacy_texture)) ||
				textures->texture_get_rd_texture(legacy_texture) == textures->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE)) {
			return ERR_CANT_CREATE;
		}
		storage->particles_collision_set_field_texture(collision, legacy_texture);
		if (!sdf) {
			storage->particles_collision_set_attractor_strength(collision, 2.0);
			storage->particles_collision_set_attractor_attenuation(collision, 1.0);
			storage->particles_collision_set_attractor_directionality(collision, 0.0);
		}
	}
	second_instance = storage->particles_collision_instance_create(collision);
	return run_legacy_process(source_transform);
}

Error ParticlesHeightfieldClearTestFixture::run_legacy_process(const Transform3D &p_field_transform) {
	using Storage = RendererRD::ParticlesStorage;
	auto *storage = Storage::get_singleton();
	auto *materials = RendererRD::MaterialStorage::get_singleton();
	legacy_shader = materials->shader_allocate();
	materials->shader_initialize(legacy_shader);
	// particles.glsl integrates the cleared zero velocity BEFORE start();
	// start then fixes the point, native collision runs, and process observes it.
	const bool attractor = legacy_neighbor_case >= 3;
	const String process_code = attractor ?
			"void process() { CUSTOM = vec4(ATTRACTOR_FORCE, 1.0); VELOCITY += ATTRACTOR_FORCE * DELTA; }" :
			"void process() { VELOCITY = COLLISION_NORMAL; CUSTOM = vec4(COLLISION_NORMAL, COLLIDED ? COLLISION_DEPTH : -1.0); TRANSFORM[3].xyz += COLLISION_NORMAL * COLLISION_DEPTH; }";
	materials->shader_set_code(legacy_shader,
			String("shader_type particles; uniform vec3 start_position; void start() { TRANSFORM = mat4(1.0); TRANSFORM[3].xyz = start_position; VELOCITY = vec3(0.0); } ") + process_code);
	legacy_material = materials->material_allocate();
	materials->material_initialize(legacy_material);
	materials->material_set_shader(legacy_material, legacy_shader);
	const Vector3 neighbor_positions[] = { Vector3(0, 1, 0), Vector3(1.125f, 0, 0), Vector3(0.125f, 0, 0), Vector3(1, 0, 0), Vector3(1, 0, 0), Vector3() };
	const Vector3 height_positions[] = { Vector3(0, 0.125f, 0), Vector3(-0.125f, 0, 0), Vector3(0, 0.125f, 0), Vector3(0, 0.125f, 0), Vector3(-0.125f, 0, 0) };
	materials->material_set_param(legacy_material, "start_position", legacy_neighbor_case >= 0 ? neighbor_positions[legacy_neighbor_case] : height_positions[legacy_transform_case]);
	materials->_update_queued_materials();
	legacy_particle = storage->particles_allocate();
	storage->particles_initialize(legacy_particle);
	storage->particles_set_amount(legacy_particle, 1);
	storage->particles_set_lifetime(legacy_particle, 1.0);
	storage->particles_set_use_local_coordinates(legacy_particle, false);
	storage->particles_set_collision_base_size(legacy_particle, 0.25f);
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
	const bool textured = legacy_neighbor_case == 2 || legacy_neighbor_case == 5;
	if (textured) {
		const auto &frame = particle->frame_history[0];
		const bool frame_matches = attractor ?
				frame.attractor_count == 1 && frame.attractors[0].type == Storage::ParticlesFrameParams::ATTRACTOR_TYPE_VECTOR_FIELD && frame.attractors[0].texture_index == 0 :
				frame.collider_count == 1 && frame.colliders[0].type == Storage::ParticlesFrameParams::COLLISION_TYPE_SDF && frame.colliders[0].texture_index == 0;
		if (!frame_matches || particle->collision_3d_textures_used != 1 || particle->collision_3d_textures[0] != legacy_texture) {
			return FAILED;
		}
	}
	{
		MutexLock lock(result_mutex);
		result_data["legacy_normal"] = true;
		if (legacy_neighbor_case >= 0) {
			const char *names[] = { "sphere", "box", "sdf", "sphere_attractor", "box_attractor", "vector_attractor" };
			result_data["neighbor_case"] = names[legacy_neighbor_case];
		}
		result_data["attractor_count"] = particle->frame_history[0].attractor_count;
		result_data["textures_used"] = particle->collision_3d_textures_used;
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
	// Fixed independent CASESET oracles, never derived through tested matrices.
	const bool neighbor = legacy_neighbor_case >= 0;
	const bool attractor = legacy_neighbor_case >= 3;
	const bool tilt = !neighbor && legacy_transform_case == 4;
	const float expected_depth = neighbor ? (legacy_neighbor_case == 0 ? 0.25f : 0.125f) : tilt ? 0.12087344460380706f : 0.125f;
	const Vector3 height_expected_normals[] = { Vector3(0, 1, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, 1, 0), Vector3(-0.9669875568304565f, -0.2417468892076141f, -0.08058229640253803f) };
	const Vector3 expected_normal = neighbor ? (legacy_neighbor_case == 1 || legacy_neighbor_case == 2 ? Vector3(1, 0, 0) : Vector3(0, 1, 0)) : height_expected_normals[legacy_transform_case];
	const Vector3 neighbor_expected_positions[] = { Vector3(0, 1.25f, 0), Vector3(1.25f, 0, 0), Vector3(0.25f, 0, 0), Vector3(1, 0, 0), Vector3(1, 0, 0), Vector3() };
	const Vector3 height_expected_positions[] = { Vector3(0, 0.25f, 0), Vector3(-0.25f, 0, 0), Vector3(0, 0.25f, 0), Vector3(0, 0.25f, 0), Vector3(-149.0f / 616.0f, -9.0f / 308.0f, -3.0f / 308.0f) };
	const Vector3 expected_position = neighbor ? neighbor_expected_positions[legacy_neighbor_case] : height_expected_positions[legacy_transform_case];
	bool passed = size_ok && (value.active & 1) && normal.distance_to(expected_normal) < (1.0f / 4096.0f) &&
			Math::abs(depth - expected_depth) < (1.0f / 4096.0f) && position.distance_to(expected_position) < (1.0f / 4096.0f);
	const Vector3 force(value.custom[0], value.custom[1], value.custom[2]);
	if (attractor) {
		const Vector3 expected_force = legacy_neighbor_case == 5 ? Vector3(-2, 0, 0) : Vector3(-1, 0, 0);
		passed = size_ok && (value.active & 1) && Math::abs(depth - 1.0f) < (1.0f / 4096.0f) &&
				force.distance_to(expected_force) < (1.0f / 4096.0f) && normal.distance_to(expected_force / 60.0f) < (1.0f / 4096.0f) && position.distance_to(expected_position) < (1.0f / 4096.0f);
	}
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
	result_data["collided"] = !attractor && depth >= 0.0f;
	if (neighbor) {
		Array force_values;
		for (int axis = 0; axis < 3; ++axis) {
			force_values.push_back(force[axis]);
		}
		result_data["force"] = force_values;
		result_data["velocity"] = normal_values;
	}
	result_data["active"] = bool(value.active & 1);
	result_data["pass"] = passed && int(result_data["frame_counter"]) == 1 &&
			int(result_data["collider_count"]) == (attractor ? 0 : 1) && int(result_data["attractor_count"]) == (attractor ? 1 : 0) &&
			int(result_data["textures_used"]) == (legacy_neighbor_case == 2 || legacy_neighbor_case == 5 ? 1 : 0);
}

void ParticlesHeightfieldClearTestFixture::sample_done(const PackedByteArray &p_bytes) {
	float raw[2] = { 99.0f, 99.0f };
	if (p_bytes.size() == 8) {
		std::memcpy(raw, p_bytes.ptr(), 8);
	}

	if (frame_resources_capture) {
		// Readback callback proves the compute sample finished. Drop its actual
		// descriptors before asking the original resource owner to retire.
		cleanup();
		retained_frame_resources_a.unref();
		MutexLock lock(result_mutex);
		result_data["first_raw"] = raw[0];
		result_data["second_raw"] = raw[1];
		result_data["sample_pass"] = p_bytes.size() == 8 && Math::abs(raw[0] - 0.75f) <= 0.001f && Math::abs(raw[1] - 0.25f) <= 0.001f;
		frame_resources_sampled = true;
		return;
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
	if (legacy_texture.is_valid()) {
		RendererRD::TextureStorage::get_singleton()->texture_free(legacy_texture);
		legacy_texture = RID();
	}
}
