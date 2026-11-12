/**************************************************************************/
/*  particles_storage.cpp                                                 */
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

#include "particles_storage.h"

#include "height_frame.h"

#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/effects/sort_effects.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/rendering_server_globals.h"

using namespace RendererRD;

ParticlesStorage *ParticlesStorage::singleton = nullptr;

ParticlesStorage *ParticlesStorage::get_singleton() {
	return singleton;
}

#ifdef TESTS_ENABLED
ParticlesStorage::ParticlesStorage(ExternalLinkedOwnerTest *) {
	CRASH_COND(singleton != nullptr);
	singleton = this;
	external_output_component_test = true;
}
#endif

ParticlesStorage::ParticlesStorage() {
	singleton = this;

	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	/* Effects */

	sort_effects = memnew(SortEffects);
	external_guard = memnew(ParticlesExternalGuard);
	external_guard_preparation_error = external_guard->prepare_pipelines();

	/* Particles */

	{
		String defines = "#define SAMPLERS_BINDING_FIRST_INDEX " + itos(SAMPLERS_BINDING_FIRST_INDEX) + "\n";
		// Initialize particles
		Vector<String> particles_modes;
		particles_modes.push_back("");
		particles_shader.shader.initialize(particles_modes, defines);
	}
	MaterialStorage::get_singleton()->shader_set_data_request_function(MaterialStorage::SHADER_TYPE_PARTICLES, _create_particles_shader_funcs);
	MaterialStorage::get_singleton()->material_set_data_request_function(MaterialStorage::SHADER_TYPE_PARTICLES, _create_particles_material_funcs);

	{
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["COLOR"] = "PARTICLE.color";
		actions.renames["VELOCITY"] = "PARTICLE.velocity";
		actions.renames["MASS"] = "mass";
		actions.renames["ACTIVE"] = "particle_active";
		actions.renames["RESTART"] = "restart";
		actions.renames["CUSTOM"] = "PARTICLE.custom";
		actions.renames["AMOUNT_RATIO"] = "FRAME.amount_ratio";
		for (int i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
			String udname = "USERDATA" + itos(i + 1);
			actions.renames[udname] = "PARTICLE.userdata" + itos(i + 1);
			actions.usage_defines[udname] = "#define USERDATA" + itos(i + 1) + "_USED\n";
		}
		actions.renames["TRANSFORM"] = "PARTICLE.xform";
		actions.renames["TIME"] = "frame_history.data[0].time";
		actions.renames["PI"] = String::num(Math::PI);
		actions.renames["TAU"] = String::num(Math::TAU);
		actions.renames["E"] = String::num(Math::E);
		actions.renames["LIFETIME"] = "params.lifetime";
		actions.renames["DELTA"] = "local_delta";
		actions.renames["NUMBER"] = "particle_number";
		actions.renames["INDEX"] = "index";
		//actions.renames["GRAVITY"] = "current_gravity";
		actions.renames["EMISSION_TRANSFORM"] = "FRAME.emission_transform";
		actions.renames["EMITTER_VELOCITY"] = "FRAME.emitter_velocity";
		actions.renames["INTERPOLATE_TO_END"] = "FRAME.interp_to_end";
		actions.renames["RANDOM_SEED"] = "FRAME.random_seed";
		actions.renames["FLAG_EMIT_POSITION"] = "EMISSION_FLAG_HAS_POSITION";
		actions.renames["FLAG_EMIT_ROT_SCALE"] = "EMISSION_FLAG_HAS_ROTATION_SCALE";
		actions.renames["FLAG_EMIT_VELOCITY"] = "EMISSION_FLAG_HAS_VELOCITY";
		actions.renames["FLAG_EMIT_COLOR"] = "EMISSION_FLAG_HAS_COLOR";
		actions.renames["FLAG_EMIT_CUSTOM"] = "EMISSION_FLAG_HAS_CUSTOM";
		actions.renames["RESTART_POSITION"] = "restart_position";
		actions.renames["RESTART_ROT_SCALE"] = "restart_rotation_scale";
		actions.renames["RESTART_VELOCITY"] = "restart_velocity";
		actions.renames["RESTART_COLOR"] = "restart_color";
		actions.renames["RESTART_CUSTOM"] = "restart_custom";
		actions.renames["emit_subparticle"] = "emit_subparticle";
		actions.renames["COLLIDED"] = "collided";
		actions.renames["COLLISION_NORMAL"] = "collision_normal";
		actions.renames["COLLISION_DEPTH"] = "collision_depth";
		actions.renames["ATTRACTOR_FORCE"] = "attractor_force";

		actions.render_mode_defines["disable_force"] = "#define DISABLE_FORCE\n";
		actions.render_mode_defines["disable_velocity"] = "#define DISABLE_VELOCITY\n";
		actions.render_mode_defines["keep_data"] = "#define ENABLE_KEEP_DATA\n";
		actions.render_mode_defines["collision_use_scale"] = "#define USE_COLLISION_SCALE\n";

		actions.base_texture_binding_index = 1;
		actions.texture_layout_set = 3;
		actions.base_uniform_string = "material.";
		actions.base_varying_index = 10;

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_ENABLE;
		actions.global_buffer_array_variable = "global_shader_uniforms.data";

		particles_shader.compiler.initialize(actions);
	}

	{
		// default material and shader for particles shader
		particles_shader.default_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(particles_shader.default_shader);
		material_storage->shader_set_code(particles_shader.default_shader, R"(
// Default particles shader.

shader_type particles;

void process() {
	COLOR = vec4(1.0);
}
)");
		particles_shader.default_material = material_storage->material_allocate();
		material_storage->material_initialize(particles_shader.default_material);
		material_storage->material_set_shader(particles_shader.default_material, particles_shader.default_shader);

		ParticleProcessMaterialData *md = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(particles_shader.default_material, MaterialStorage::SHADER_TYPE_PARTICLES));
		particles_shader.default_shader_rd = particles_shader.shader.version_get_shader(md->shader_data->version, 0);

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(material_storage->global_shader_uniforms_get_storage_buffer());
			uniforms.push_back(u);
		}

		material_storage->samplers_rd_get_default().append_uniforms(uniforms, SAMPLERS_BINDING_FIRST_INDEX);

		particles_shader.base_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, BASE_UNIFORM_SET);
		ERR_FAIL_COND_MSG(particles_shader.base_uniform_set.is_null(), "Failed to create particle base uniform set.");
	}

	{
		Vector<String> copy_modes;
		for (int i = 0; i <= ParticlesShader::MAX_USERDATAS; i++) {
			if (i == 0) {
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n");
				copy_modes.push_back("\n#define MODE_FILL_SORT_BUFFER\n#define USE_SORT_BUFFER\n");
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USE_SORT_BUFFER\n");
			} else {
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USERDATA_COUNT " + itos(i) + "\n");
				copy_modes.push_back("\n#define MODE_FILL_SORT_BUFFER\n#define USE_SORT_BUFFER\n#define USERDATA_COUNT " + itos(i) + "\n");
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USE_SORT_BUFFER\n#define USERDATA_COUNT " + itos(i) + "\n");
			}
		}

		particles_shader.copy_shader.initialize(copy_modes);

		particles_shader.copy_shader_version = particles_shader.copy_shader.version_create();

		for (int i = 0; i <= ParticlesShader::MAX_USERDATAS; i++) {
			for (int j = 0; j < ParticlesShader::COPY_MODE_MAX; j++) {
				particles_shader.copy_pipelines[i][j].create_compute_pipeline(particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, i * ParticlesShader::COPY_MODE_MAX + j));
			}
		}
	}
}

ParticlesStorage::~ParticlesStorage() {
#ifdef TESTS_ENABLED
	if (external_output_component_test) {
		singleton = nullptr;
		return;
	}
#endif
	for (int i = 0; i <= ParticlesShader::MAX_USERDATAS; i++) {
		for (int j = 0; j < ParticlesShader::COPY_MODE_MAX; j++) {
			particles_shader.copy_pipelines[i][j].free();
		}
	}

	particles_shader.copy_shader.version_free(particles_shader.copy_shader_version);

	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	material_storage->material_free(particles_shader.default_material);
	material_storage->shader_free(particles_shader.default_shader);

	if (external_guard) {
		// RenderingServer's service-stop gate has already required every
		// original production attachment to return before storage teardown.
		const Error released = external_guard->release_pipelines();
		CRASH_COND_MSG(released != OK, "Engine external guard must be released by its main-RD render-thread owner.");
		memdelete(external_guard);
		external_guard = nullptr;
	}

	if (sort_effects) {
		memdelete(sort_effects);
		sort_effects = nullptr;
	}

	singleton = nullptr;
}

bool ParticlesStorage::is_external_guard_prepared() const {
	return external_guard && external_guard->is_prepared();
}

bool ParticlesStorage::free(RID p_rid) {
	if (owns_particles(p_rid)) {
		particles_free(p_rid);
		return true;
	} else if (owns_particles_collision(p_rid)) {
		particles_collision_free(p_rid);
		return true;
	} else if (owns_particles_collision_instance(p_rid)) {
		particles_collision_instance_free(p_rid);
		return true;
	}

	return false;
}

/* PARTICLES */

RID ParticlesStorage::particles_allocate() {
	return particles_owner.allocate_rid();
}

void ParticlesStorage::particles_initialize(RID p_rid) {
	particles_owner.initialize_rid(p_rid);
}

void ParticlesStorage::particles_free(RID p_rid) {
	Particles *particles = particles_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(particles);

	if (particles->external) {
		ExternalOutputOwner *external = particles->external;
		if (external->state != ExternalOutputOwner::GenerationState::RELEASED) {
			external->withdraw();
			// Preserve the RID and every original custody/native obligation.
			particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
			return;
		}
		// Original retirement has already returned all finite tables and slots.
		// Ordinary simulation buffers never belong to an external owner.
		particles->dependency.deleted_notify(p_rid);
		particles->update_list.remove_from_list();
		particles->external = nullptr;
		if (!external->metadata_lease.is_empty()) {
			// Generation terminal permits logical RID destruction. Its original
			// metadata resource remains live until this last CPU owner is gone.
			// Move custody out before destroying an owner inside that allocation.
			auto *registry = external->generation_registry;
			GDExtensionDeviceExit::ProductionOutputMetadataLease metadata = static_cast<GDExtensionDeviceExit::ProductionOutputMetadataLease &&>(external->metadata_lease);
			external->~ExternalOutputOwner();
			const Error returned = registry->production_output_metadata_return(metadata);
			CRASH_COND(returned != OK);
		} else {
			memdelete(external);
		}
		particles_owner.free(p_rid);
		return;
	}

	particles->dependency.deleted_notify(p_rid);
	particles->update_list.remove_from_list();

	_particles_free_data(particles);
	particles_owner.free(p_rid);
}

void ParticlesStorage::particles_set_mode(RID p_particles, RSE::ParticlesMode p_mode) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	if (particles->mode == p_mode) {
		return;
	}

	_particles_free_data(particles);

	particles->mode = p_mode;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_emitting(RID p_particles, bool p_emitting) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->emitting = p_emitting;
}

bool ParticlesStorage::particles_get_emitting(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);
	ERR_FAIL_COND_V(particles->external, false);

	return particles->emitting;
}

void ParticlesStorage::_particles_free_data(Particles *particles) {
	ERR_FAIL_COND(particles->external);
	particles->frame_remainder = 0.0;
	particles->retry_pending = false;
	particles->frame_delta_committed = false;
	particles->frame_delta_committed_frame = UINT32_MAX;
	particles->pre_process_time_remaining = 0.0;
	particles->pre_process_pending = false;

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_copy_uniform_set)) {
		RD::get_singleton()->free_rid(particles->particles_copy_uniform_set);
	}
	particles->particles_copy_uniform_set = RID();

	if (particles->particle_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->particle_buffer);
		particles->particle_buffer = RID();
	}
	if (particles->particle_instance_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->particle_instance_buffer);
		particles->particle_instance_buffer = RID();
	}

	particles->userdata_count = 0;

	if (particles->frame_params_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->frame_params_buffer);
		particles->frame_params_buffer = RID();
	}
	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_transforms_buffer_uniform_set)) {
		RD::get_singleton()->free_rid(particles->particles_transforms_buffer_uniform_set);
	}
	particles->particles_transforms_buffer_uniform_set = RID();

	if (RD::get_singleton()->uniform_set_is_valid(particles->trail_bind_pose_uniform_set)) {
		RD::get_singleton()->free_rid(particles->trail_bind_pose_uniform_set);
	}
	particles->trail_bind_pose_uniform_set = RID();

	if (particles->trail_bind_pose_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->trail_bind_pose_buffer);
		particles->trail_bind_pose_buffer = RID();
	}
	if (RD::get_singleton()->uniform_set_is_valid(particles->collision_textures_uniform_set)) {
		RD::get_singleton()->free_rid(particles->collision_textures_uniform_set);
	}
	particles->collision_textures_uniform_set = RID();

	if (particles->particles_sort_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->particles_sort_buffer);
		particles->particles_sort_buffer = RID();
	}
	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_sort_uniform_set)) {
		RD::get_singleton()->free_rid(particles->particles_sort_uniform_set);
	}
	particles->particles_sort_uniform_set = RID();

	particles->emission_buffer = nullptr;
	particles->emission_buffer_data.clear();
	if (particles->emission_storage_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->emission_storage_buffer);
		particles->emission_storage_buffer = RID();
	}

	if (particles->unused_emission_storage_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->unused_emission_storage_buffer);
		particles->unused_emission_storage_buffer = RID();
	}

	if (particles->unused_trail_storage_buffer.is_valid()) {
		RD::get_singleton()->free_rid(particles->unused_trail_storage_buffer);
		particles->unused_trail_storage_buffer = RID();
	}

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		//will need to be re-created
		RD::get_singleton()->free_rid(particles->particles_material_uniform_set);
	}
	particles->particles_material_uniform_set = RID();
}

void ParticlesStorage::particles_set_amount(RID p_particles, int p_amount) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_amount_is_valid(p_amount), "Particle amount must be non-negative.");

	if (particles->amount == p_amount) {
		return;
	}

	_particles_free_data(particles);

	particles->amount = p_amount;

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_amount_ratio(RID p_particles, float p_amount_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_ratio_is_valid(p_amount_ratio), "Particle amount ratio must be finite.");

	particles->amount_ratio = p_amount_ratio;
}

void ParticlesStorage::particles_set_lifetime(RID p_particles, double p_lifetime) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_lifetime_is_valid(p_lifetime), "Particle lifetime must be finite and greater than zero.");
	particles->lifetime = p_lifetime;
}

void ParticlesStorage::particles_set_one_shot(RID p_particles, bool p_one_shot) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->one_shot = p_one_shot;
}

void ParticlesStorage::particles_set_pre_process_time(RID p_particles, double p_time) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_process_time_is_valid(p_time), "Particle pre-process time must be finite and non-negative.");
	particles->pre_process_time = p_time;
}

void ParticlesStorage::particles_request_process_time(RID p_particles, real_t p_request_process_time, real_t p_request_process_time_residual) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_process_time_is_valid(p_request_process_time) || !_particles_process_time_is_valid(p_request_process_time_residual), "Particle process times must be finite and non-negative.");
	particles->request_process_time = p_request_process_time;
	particles->request_process_time_residual = p_request_process_time_residual;
}

void ParticlesStorage::particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_ratio_is_valid(p_ratio), "Particle explosiveness ratio must be finite.");
	particles->explosiveness = p_ratio;
}
void ParticlesStorage::particles_set_randomness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_ratio_is_valid(p_ratio), "Particle randomness ratio must be finite.");
	particles->randomness = p_ratio;
}

void ParticlesStorage::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->custom_aabb = p_aabb;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_set_speed_scale(RID p_particles, double p_scale) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_speed_scale_is_valid(p_scale), "Particle speed scale must be finite and non-negative.");

	particles->speed_scale = p_scale;
}
void ParticlesStorage::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->use_local_coords = p_enable;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_fixed_fps(RID p_particles, int p_fps) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_fixed_fps_is_valid(p_fps), "Particle fixed FPS must be non-negative.");

	particles->fixed_fps = p_fps;

	_particles_free_data(particles);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_interpolate(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->interpolate = p_enable;
}

void ParticlesStorage::particles_set_fractional_delta(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->fractional_delta = p_enable;
}

void ParticlesStorage::particles_set_trails(RID p_particles, bool p_enable, double p_length) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_trail_lifetime_is_valid(p_length), "Particle trail lifetime must be finite and at least 0.01 seconds.");
	p_length = MIN(10.0, p_length);

	particles->trails_enabled = p_enable;
	particles->trail_lifetime = p_length;

	_particles_free_data(particles);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	// The uniform set may currently reference the fallback trail buffer when
	// no bind-pose buffer has been allocated yet. A change in pose count still
	// changes the buffer layout, so invalidate all dependent resources even in
	// that fallback state.
	if (particles->trail_bind_poses.size() != p_bind_poses.size()) {
		_particles_free_data(particles);

		particles->prev_ticks = 0;
		particles->phase = 0;
		particles->prev_phase = 0;
		particles->clear = true;
	}
	particles->trail_bind_poses = p_bind_poses;
	particles->trail_bind_poses_dirty = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_collision_base_size(RID p_particles, real_t p_size) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!_particles_collision_base_size_is_valid(p_size), "Particle collision base size must be finite and non-negative.");

	particles->collision_base_size = p_size;
}

void ParticlesStorage::particles_set_transform_align(RID p_particles, RSE::ParticlesTransformAlign p_transform_align) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_INDEX(int(p_transform_align), RSE::PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD + 1);

	particles->transform_align = p_transform_align;
}

void ParticlesStorage::particles_set_transform_align_channel_filter(RID p_particles, RSE::ParticlesTransformAlignCustomSrc p_transform_align_channel_filter) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_INDEX(int(p_transform_align_channel_filter), RSE::PARTICLES_ALIGN_CHANNEL_FILTER_MAX);

	particles->transform_align_src = p_transform_align_channel_filter;
}

void ParticlesStorage::particles_set_transform_align_axis(RID p_particles, RSE::ParticlesTransformAlignAxis p_rotation_axis) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_INDEX(int(p_rotation_axis), RSE::PARTICLES_ALIGN_AXIS_MAX);

	particles->transform_align_axis = p_rotation_axis;
}

void ParticlesStorage::particles_set_process_material(RID p_particles, RID p_material) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->process_material = p_material;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES); //the instance buffer may have changed
}

RID ParticlesStorage::particles_get_process_material(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());

	return particles->process_material;
}

void ParticlesStorage::particles_set_draw_order(RID p_particles, RSE::ParticlesDrawOrder p_order) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_INDEX(int(p_order), RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH + 1);

	particles->draw_order = p_order;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_draw_passes(RID p_particles, int p_passes) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND(p_passes < 1);

	particles->draw_passes.resize(p_passes);
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_INDEX(p_pass, particles->draw_passes.size());
	particles->draw_passes.write[p_pass] = p_mesh;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_restart(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->restart_request = true;
}

void ParticlesStorage::particles_set_seed(RID p_particles, uint32_t p_seed) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->random_seed = p_seed;
}

void ParticlesStorage::_particles_allocate_emission_buffer(Particles *particles) {
	ERR_FAIL_COND(particles->external);
	ERR_FAIL_COND(particles->emission_buffer != nullptr);

	uint32_t emission_buffer_size = 0;
	if (!_particles_get_buffer_size(uint64_t(particles->amount), sizeof(ParticleEmissionBuffer::Data), sizeof(uint32_t) * 4, emission_buffer_size)) {
		ERR_PRINT("Particle emission buffer exceeds the rendering device buffer size limit.");
		return;
	}
	particles->emission_buffer_data.resize(emission_buffer_size);
	memset(particles->emission_buffer_data.ptrw(), 0, particles->emission_buffer_data.size());
	ParticleEmissionBuffer *emission_buffer = reinterpret_cast<ParticleEmissionBuffer *>(particles->emission_buffer_data.ptrw());
	emission_buffer->particle_max = particles->amount;

	RID emission_storage_buffer = RD::get_singleton()->storage_buffer_create(particles->emission_buffer_data.size(), particles->emission_buffer_data);
	if (emission_storage_buffer.is_null()) {
		particles->emission_buffer_data.clear();
		return;
	}
	particles->emission_buffer = emission_buffer;
	particles->emission_storage_buffer = emission_storage_buffer;

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		//will need to be re-created
		RD::get_singleton()->free_rid(particles->particles_material_uniform_set);
		particles->particles_material_uniform_set = RID();
	}
}

void ParticlesStorage::_particles_ensure_unused_emission_buffer(Particles *particles) {
	ERR_FAIL_COND(particles->external);
	if (particles->unused_emission_storage_buffer.is_null()) {
		// For rendering devices that do not support empty arrays (like C++),
		// we need to size the buffer with at least 1 element.
		particles->unused_emission_storage_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ParticleEmissionBuffer));
	}
}

bool ParticlesStorage::_particles_ensure_unused_trail_buffer(Particles *particles) {
	ERR_FAIL_COND_V(particles->external, false);
	if (particles->unused_trail_storage_buffer.is_null()) {
#ifdef TESTS_ENABLED
		if (particles->test_reject_trail_allocation_once) {
			particles->test_reject_trail_allocation_once = false;
			return false;
		}
#endif
		particles->unused_trail_storage_buffer = RD::get_singleton()->storage_buffer_create(16 * sizeof(float)); // Size of mat4.
	}
	return particles->unused_trail_storage_buffer.is_valid();
}

bool ParticlesStorage::_particles_ensure_trail_uniform(Particles *particles, RID p_shader) {
	ERR_FAIL_COND_V(particles->external, false);
	if (particles->trail_bind_pose_uniform_set.is_null()) {
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			if (particles->trail_bind_pose_buffer.is_valid()) {
				u.append_id(particles->trail_bind_pose_buffer);
			} else {
				if (!_particles_ensure_unused_trail_buffer(particles)) {
					ERR_PRINT("Failed to create particle fallback trail buffer.");
					return false;
				}
				u.append_id(particles->unused_trail_storage_buffer);
			}
			uniforms.push_back(u);
		}

#ifdef TESTS_ENABLED
		particles->test_trail_uniform_attempts++;
#endif
		particles->trail_bind_pose_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_shader, 2);
		if (particles->trail_bind_pose_uniform_set.is_null()) {
			ERR_PRINT("Failed to create particle trail bind pose uniform set.");
			return false;
		}
	}
	return true;
}

void ParticlesStorage::particles_set_subemitter(RID p_particles, RID p_subemitter_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND(p_particles == p_subemitter_particles);
	const Particles *sub_emitter = particles_owner.get_or_null(p_subemitter_particles);
	ERR_FAIL_COND_MSG(sub_emitter && sub_emitter->external, "ExternalOutput cannot be a native sub-emitter.");

	particles->sub_emitter = p_subemitter_particles;

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		RD::get_singleton()->free_rid(particles->particles_material_uniform_set);
		particles->particles_material_uniform_set = RID(); //clear and force to re create sub emitting
	}
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	ERR_FAIL_COND_MSG(!p_transform.is_finite() || !p_velocity.is_finite() || !Math::is_finite(p_color.r) || !Math::is_finite(p_color.g) || !Math::is_finite(p_color.b) || !Math::is_finite(p_color.a) || !Math::is_finite(p_custom.r) || !Math::is_finite(p_custom.g) || !Math::is_finite(p_custom.b) || !Math::is_finite(p_custom.a), "Particle emission inputs must contain only finite values.");
	ERR_FAIL_COND(particles->amount == 0);

	if (particles->emission_buffer == nullptr) {
		_particles_allocate_emission_buffer(particles);
		if (particles->emission_buffer == nullptr || particles->emission_storage_buffer.is_null()) {
			ERR_PRINT("Failed to allocate particle emission buffer.");
			return;
		}
	}

	if (particles->emitting) {
		particles->clear = true;
		particles->emitting = false;
	}

	particles->inactive = false;
	particles->inactive_time = 0;

	int32_t idx = particles->emission_buffer->particle_count;
	if (idx < particles->emission_buffer->particle_max) {
		RendererRD::MaterialStorage::store_transform(p_transform, particles->emission_buffer->data[idx].xform);

		particles->emission_buffer->data[idx].velocity[0] = p_velocity.x;
		particles->emission_buffer->data[idx].velocity[1] = p_velocity.y;
		particles->emission_buffer->data[idx].velocity[2] = p_velocity.z;

		particles->emission_buffer->data[idx].custom[0] = p_custom.r;
		particles->emission_buffer->data[idx].custom[1] = p_custom.g;
		particles->emission_buffer->data[idx].custom[2] = p_custom.b;
		particles->emission_buffer->data[idx].custom[3] = p_custom.a;

		particles->emission_buffer->data[idx].color[0] = p_color.r;
		particles->emission_buffer->data[idx].color[1] = p_color.g;
		particles->emission_buffer->data[idx].color[2] = p_color.b;
		particles->emission_buffer->data[idx].color[3] = p_color.a;

		particles->emission_buffer->data[idx].flags = p_emit_flags;
		particles->emission_buffer->particle_count++;
	}
}

void ParticlesStorage::particles_request_process(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	if (!particles->dirty) {
		particles->dirty = true;

		if (!particles->update_list.in_list()) {
			particle_update_list.add(&particles->update_list);
		}
	}
}

bool ParticlesStorage::_particles_get_data_size(const Particles *p_particles, uint64_t &r_size) {
	if (p_particles == nullptr || p_particles->external || p_particles->amount <= 0) {
		r_size = 0;
		return false;
	}

	uint64_t total_amount = uint64_t(p_particles->amount);
	if (p_particles->trails_enabled && p_particles->trail_bind_poses.size() > 1) {
		const uint64_t trail_count = uint64_t(p_particles->trail_bind_poses.size());
		if (total_amount > UINT64_MAX / trail_count) {
			r_size = 0;
			return false;
		}
		total_amount *= trail_count;
	}
	const uint64_t particle_data_size = sizeof(ParticleData) + sizeof(float) * 4 * uint64_t(p_particles->userdata_count);
	if (particle_data_size != 0 && total_amount > UINT64_MAX / particle_data_size) {
		r_size = 0;
		return false;
	}
	r_size = total_amount * particle_data_size;
	return r_size <= UINT32_MAX;
}

bool ParticlesStorage::_particles_get_buffer_size(uint64_t p_count, uint64_t p_element_size, uint64_t p_header_size, uint32_t &r_size) {
	r_size = 0;
	if (p_element_size != 0 && p_count > (UINT64_MAX - p_header_size) / p_element_size) {
		return false;
	}
	const uint64_t size = p_header_size + p_count * p_element_size;
	if (size > UINT32_MAX) {
		return false;
	}
	r_size = uint32_t(size);
	return true;
}

AABB ParticlesStorage::particles_get_current_aabb(RID p_particles) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());
	if (particles->external) {
		return particles->external->config.bounds;
	}

	uint64_t expected_size = 0;
	if (!_particles_get_data_size(particles, expected_size)) {
		ERR_PRINT("Particle current AABB exceeds the rendering device buffer size limit.");
		return AABB();
	}
	const uint64_t particle_data_size = sizeof(ParticleData) + sizeof(float) * 4 * uint64_t(particles->userdata_count);
	Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(particles->particle_buffer);
	ERR_FAIL_COND_V(uint64_t(buffer.size()) != expected_size, AABB());
	const uint64_t total_amount = particle_data_size == 0 ? 0 : expected_size / particle_data_size;

	Transform3D inv = particles->emission_transform.affine_inverse();

	AABB aabb;
	if (buffer.size()) {
		bool first = true;

		const uint8_t *data_ptr = (const uint8_t *)buffer.ptr();

		for (uint64_t i = 0; i < total_amount; i++) {
			const ParticleData &particle_data = *(const ParticleData *)&data_ptr[particle_data_size * i];
			// The GPU stores the complete particle flags word in this field. A
			// particle is visible to the copy shader only while ACTIVE or TRAILED
			// is set; STARTED and the packed cycle number must not keep stale
			// positions in the CPU-side bounds.
			if (particle_data.active & (uint32_t(1) | uint32_t(4))) {
				Vector3 pos = Vector3(particle_data.xform[12], particle_data.xform[13], particle_data.xform[14]);
				if (!particles->use_local_coords) {
					pos = inv.xform(pos);
				}
				if (first) {
					aabb.position = pos;
					first = false;
				} else {
					aabb.expand_to(pos);
				}
			}
		}
	}

	float longest_axis_size = 0;
	for (int i = 0; i < particles->draw_passes.size(); i++) {
		if (particles->draw_passes[i].is_valid()) {
			AABB maabb = MeshStorage::get_singleton()->mesh_get_aabb(particles->draw_passes[i], RID());
			longest_axis_size = MAX(maabb.get_longest_axis_size(), longest_axis_size);
		}
	}

	aabb.grow_by(longest_axis_size);

	return aabb;
}

Vector<uint8_t> ParticlesStorage::particles_get_current_data(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, Vector<uint8_t>());
	ERR_FAIL_COND_V(particles->external, Vector<uint8_t>());
	if (particles->particle_buffer.is_null() || particles->amount <= 0) {
		return Vector<uint8_t>();
	}

	uint64_t expected_size = 0;
	if (!_particles_get_data_size(particles, expected_size)) {
		ERR_PRINT("Particle CPU readback exceeds the rendering device buffer size limit.");
		return Vector<uint8_t>();
	}

	Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(particles->particle_buffer);
	if (uint64_t(buffer.size()) != expected_size) {
		ERR_PRINT("Particle CPU readback returned an unexpected buffer size.");
		return Vector<uint8_t>();
	}
	return buffer;
}

AABB ParticlesStorage::particles_get_aabb(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());
	if (particles->external) {
		const auto &config = particles->external->config;
		return config.query_enabled ? config.bounds.merge(config.query_bounds) : config.bounds;
	}

	return particles->custom_aabb;
}

void ParticlesStorage::particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->emission_transform = p_transform;
}

void ParticlesStorage::particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->emitter_velocity = p_velocity;
}

void ParticlesStorage::particles_set_interp_to_end(RID p_particles, float p_interp) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	particles->interp_to_end = p_interp;
}

int ParticlesStorage::particles_get_draw_passes(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, 0);
	ERR_FAIL_COND_V(particles->external, 0);

	return particles->draw_passes.size();
}

RID ParticlesStorage::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());
	ERR_FAIL_COND_V(particles->external, RID());
	ERR_FAIL_INDEX_V(p_pass, particles->draw_passes.size(), RID());

	return particles->draw_passes[p_pass];
}

void ParticlesStorage::particles_update_dependency(RID p_particles, DependencyTracker *p_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	p_instance->update_dependency(&particles->dependency);
}

void ParticlesStorage::particles_get_instance_buffer_motion_vectors_offsets(RID p_particles, uint32_t &r_current_offset, uint32_t &r_prev_offset) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	if (particles->external) {
		r_current_offset = r_prev_offset = 0;
		ERR_FAIL_MSG("ExternalOutput uses packet-owned current and previous bindings.");
	}
	r_current_offset = particles->instance_motion_vectors_current_offset;
	r_prev_offset = particles->instance_motion_vectors_previous_offset;
}

void ParticlesStorage::particles_add_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->collisions.insert(p_particles_collision_instance);
}

void ParticlesStorage::particles_remove_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->collisions.erase(p_particles_collision_instance);
}

void ParticlesStorage::particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, RID p_texture) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");
	particles->has_sdf_collision = p_enable;
	particles->sdf_collision_transform = p_xform;
	particles->sdf_collision_to_screen = p_to_screen;
	particles->sdf_collision_texture = p_texture;
}

void ParticlesStorage::_particles_commit_process_state(Particles *p_particles, Particles *p_sub_emitter, bool p_sub_emitter_was_emitting, bool p_success) {
	ERR_FAIL_COND(p_particles->external || (p_sub_emitter && p_sub_emitter->external));
	if (!p_success) {
		return;
	}
	p_particles->force_sub_emit = false;
	if (p_particles->emission_buffer && p_particles->emission_buffer->particle_count) {
		p_particles->emission_buffer->particle_count = 0;
	}
	p_particles->clear = false;
	if (p_sub_emitter && p_sub_emitter->emission_storage_buffer.is_valid()) {
		if (p_sub_emitter_was_emitting) {
			p_sub_emitter->emitting = false;
			p_sub_emitter->clear = true;
		}
		p_sub_emitter->inactive = false;
		p_sub_emitter->inactive_time = 0;
		p_sub_emitter->force_sub_emit = true;
	}
}

void ParticlesStorage::_particles_restore_process_history(Particles *p_particles, const ParticlesFrameParams &p_previous_first, const ParticlesFrameParams &p_previous_last, double p_previous_trail_speed_scale) {
	ERR_FAIL_COND(p_particles->external);
	if (p_particles->frame_history.is_empty()) {
		return;
	}
	p_particles->frame_history[0] = p_previous_first;
	if (p_particles->frame_history.size() > 1) {
		for (uint32_t i = 1; i + 1 < p_particles->frame_history.size(); i++) {
			p_particles->frame_history[i] = p_particles->frame_history[i + 1];
		}
		p_particles->frame_history[p_particles->frame_history.size() - 1] = p_previous_last;
	}

	if (p_particles->trail_params.is_empty()) {
		return;
	}
	if (p_particles->trail_params.size() > 1) {
		for (uint32_t i = 0; i < p_particles->trail_params.size(); i++) {
			uint32_t src_idx = i * p_particles->frame_history.size() / p_particles->trail_params.size();
			if (p_previous_trail_speed_scale <= 0.0) {
				src_idx = 0;
			}
			p_particles->trail_params[i] = p_particles->frame_history[src_idx];
		}
	} else {
		p_particles->trail_params[0] = p_particles->frame_history[0];
	}
}

bool ParticlesStorage::_particles_get_total_amount(const Particles *p_particles, uint32_t &r_total_amount) {
	if (p_particles == nullptr || p_particles->external || p_particles->amount <= 0) {
		r_total_amount = 0;
		return false;
	}
	uint64_t total_amount = uint64_t(p_particles->amount);
	if (p_particles->trails_enabled && p_particles->trail_bind_poses.size() > 1) {
		total_amount *= uint64_t(p_particles->trail_bind_poses.size());
	}
	if (total_amount > UINT32_MAX) {
		r_total_amount = 0;
		return false;
	}
	r_total_amount = uint32_t(total_amount);
	return true;
}

bool ParticlesStorage::_particles_amount_is_valid(int p_amount) {
	return p_amount >= 0;
}

bool ParticlesStorage::_particles_lifetime_is_valid(double p_lifetime) {
	// The shader push constant stores lifetime as a float. Reject finite doubles
	// that would become zero or infinity when crossing the CPU/GPU boundary.
	const float gpu_lifetime = (float)p_lifetime;
	return Math::is_finite(p_lifetime) && Math::is_finite(gpu_lifetime) && gpu_lifetime > 0.0f;
}

bool ParticlesStorage::_particles_trail_lifetime_is_valid(double p_lifetime) {
	return Math::is_finite(p_lifetime) && p_lifetime >= 0.01 - CMP_EPSILON;
}

bool ParticlesStorage::_particles_fixed_fps_is_valid(int p_fps) {
	return p_fps >= 0 && p_fps <= 1000;
}
bool ParticlesStorage::_particles_process_time_is_valid(double p_time) {
	// The remaining pre-process accumulator is real_t/float. Reject a finite
	// double that would become infinity before the processing loop consumes it.
	return Math::is_finite(p_time) && Math::is_finite((float)p_time) && p_time >= 0.0;
}

bool ParticlesStorage::_particles_speed_scale_is_valid(double p_scale) {
	return Math::is_finite(p_scale) && p_scale >= 0.0;
}

bool ParticlesStorage::_particles_ratio_is_valid(double p_ratio) {
	return Math::is_finite(p_ratio);
}

bool ParticlesStorage::_particles_collision_base_size_is_valid(real_t p_size) {
	return Math::is_finite(p_size) && p_size >= 0.0;
}

bool ParticlesStorage::_particles_process(Particles *p_particles, double p_delta) {
	ERR_FAIL_COND_V(p_particles->external, false);
	if (p_particles->process_error != OK) {
		return false;
	}
	const Particles *sub_emitter_owner = particles_owner.get_or_null(p_particles->sub_emitter);
	ERR_FAIL_COND_V(sub_emitter_owner && sub_emitter_owner->external, false);
	TextureStorage *texture_storage = TextureStorage::get_singleton();
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	if (p_particles->particles_material_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(p_particles->particles_material_uniform_set)) {
		thread_local LocalVector<RD::Uniform> uniforms;
		uniforms.clear();

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(p_particles->frame_params_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(p_particles->particle_buffer);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			if (p_particles->emission_storage_buffer.is_valid()) {
				u.append_id(p_particles->emission_storage_buffer);
			} else {
				_particles_ensure_unused_emission_buffer(p_particles);
				if (p_particles->unused_emission_storage_buffer.is_null()) {
					ERR_PRINT("Failed to create particle fallback emission buffer.");
					return false;
				}
				u.append_id(p_particles->unused_emission_storage_buffer);
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			Particles *sub_emitter = particles_owner.get_or_null(p_particles->sub_emitter);
			if (sub_emitter) {
				if (sub_emitter->emission_buffer == nullptr) { //no emission buffer, allocate emission buffer
					_particles_allocate_emission_buffer(sub_emitter);
				}
				if (sub_emitter->emission_storage_buffer.is_null()) {
					ERR_PRINT("Failed to create sub-emitter emission buffer.");
					return false;
				}
				u.append_id(sub_emitter->emission_storage_buffer);
			} else {
				_particles_ensure_unused_emission_buffer(p_particles);
				if (p_particles->unused_emission_storage_buffer.is_null()) {
					ERR_PRINT("Failed to create particle fallback emission buffer.");
					return false;
				}
				u.append_id(p_particles->unused_emission_storage_buffer);
			}
			uniforms.push_back(u);
		}

		p_particles->particles_material_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, 1);
		if (p_particles->particles_material_uniform_set.is_null()) {
			ERR_PRINT("Failed to create particle material uniform set.");
			return false;
		}
	}

	const double previous_phase = p_particles->phase;
	const uint32_t previous_cycle_number = p_particles->cycle_number;
	const uint32_t previous_frame_counter = p_particles->frame_counter;
	const ParticlesFrameParams previous_frame_history_first = p_particles->frame_history.is_empty() ? ParticlesFrameParams() : p_particles->frame_history[0];
	const ParticlesFrameParams previous_frame_history_last = p_particles->frame_history.size() < 2 ? previous_frame_history_first : p_particles->frame_history[p_particles->frame_history.size() - 1];
	const double previous_trail_speed_scale = p_particles->trail_params_speed_scale;
	const bool previous_clear = p_particles->clear;
	const bool previous_force_sub_emit = p_particles->force_sub_emit;
	const bool previous_emitting = p_particles->emitting;
	const bool previous_has_collision_cache = p_particles->has_collision_cache;
	const uint32_t previous_collision_3d_textures_used = p_particles->collision_3d_textures_used;
	const RID previous_collision_heightmap_texture = p_particles->collision_heightmap_texture;
	RID previous_collision_3d_textures[ParticlesFrameParams::MAX_3D_TEXTURES];
	for (uint32_t i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
		previous_collision_3d_textures[i] = p_particles->collision_3d_textures[i];
	}
	const bool update_sdf_collision = p_particles->has_sdf_collision && RD::get_singleton()->texture_is_valid(p_particles->sdf_collision_texture);
	LocalVector<ParticlesFrameParams::Collider> previous_sdf_colliders;
	if (update_sdf_collision && p_particles->frame_history.size() > 2) {
		// First and last frames already have full snapshots. Save only the
		// interior colliders before shifting or rewriting any history entry.
		previous_sdf_colliders.resize(p_particles->frame_history.size() - 2);
		for (uint32_t i = 0; i < previous_sdf_colliders.size(); i++) {
			previous_sdf_colliders[i] = p_particles->frame_history[i + 1].colliders[0];
		}
	}
	bool history_shifted = false;
	bool collision_set_rebuilt = false;
	auto fail_process = [&]() -> bool {
		// A failed frame must not leave a newly rebuilt descriptor set bound to
		// the rejected collision tuple. The CPU cache is restored below, so the
		// next retry must rebuild the set even if the tuple changes back.
		if (collision_set_rebuilt) {
			if (RD::get_singleton()->uniform_set_is_valid(p_particles->collision_textures_uniform_set)) {
				RD::get_singleton()->free_rid(p_particles->collision_textures_uniform_set);
			}
			p_particles->collision_textures_uniform_set = RID();
		}
		p_particles->phase = previous_phase;
		p_particles->cycle_number = previous_cycle_number;
		p_particles->frame_counter = previous_frame_counter;
		if (history_shifted) {
			// Interior frames are still shifted right by one. Undo their SDF
			// rewrite before reversing the shift and rebuilding trail samples.
			for (uint32_t i = 0; i < previous_sdf_colliders.size(); i++) {
				p_particles->frame_history[i + 2].colliders[0] = previous_sdf_colliders[i];
			}
			_particles_restore_process_history(p_particles, previous_frame_history_first, previous_frame_history_last, previous_trail_speed_scale);
		}
		p_particles->clear = previous_clear;
		p_particles->force_sub_emit = previous_force_sub_emit;
		p_particles->emitting = previous_emitting;
		p_particles->has_collision_cache = previous_has_collision_cache;
		p_particles->collision_3d_textures_used = previous_collision_3d_textures_used;
		p_particles->collision_heightmap_texture = previous_collision_heightmap_texture;
		for (uint32_t i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
			p_particles->collision_3d_textures[i] = previous_collision_3d_textures[i];
		}
		return false;
	};

	const double advanced_phase = p_particles->phase + (p_delta / p_particles->lifetime);
	double new_phase = Math::fmod(advanced_phase, 1.0);

	//move back history (if there is any)
	for (uint32_t i = p_particles->frame_history.size() - 1; i > 0; i--) {
		p_particles->frame_history[i] = p_particles->frame_history[i - 1];
	}
	history_shifted = true;
	//update current frame
	ParticlesFrameParams &frame_params = p_particles->frame_history[0];

	if (p_particles->clear) {
		p_particles->cycle_number = 0;
	} else if (advanced_phase >= 1.0) {
		if (p_particles->one_shot) {
			p_particles->emitting = false;
		}
		p_particles->cycle_number++;
	}

	frame_params.emitting = p_particles->emitting;
	frame_params.system_phase = new_phase;
	frame_params.prev_system_phase = p_particles->phase;

	p_particles->phase = new_phase;

	frame_params.time = RendererCompositorRD::get_singleton()->get_total_time();
	frame_params.delta = p_delta;
	frame_params.random_seed = p_particles->random_seed;
	frame_params.explosiveness = p_particles->explosiveness;
	frame_params.randomness = p_particles->randomness;

	if (p_particles->use_local_coords) {
		RendererRD::MaterialStorage::store_transform(Transform3D(), frame_params.emission_transform);
	} else {
		RendererRD::MaterialStorage::store_transform(p_particles->emission_transform, frame_params.emission_transform);
	}

	frame_params.cycle = p_particles->cycle_number;
	frame_params.frame = p_particles->frame_counter++;
	frame_params.amount_ratio = p_particles->amount_ratio;
	frame_params.pad1 = 0;
	frame_params.pad2 = 0;
	frame_params.emitter_velocity[0] = p_particles->emitter_velocity.x;
	frame_params.emitter_velocity[1] = p_particles->emitter_velocity.y;
	frame_params.emitter_velocity[2] = p_particles->emitter_velocity.z;
	frame_params.interp_to_end = p_particles->interp_to_end;

	// Keep the exact managed version alive through both trail dispatches.
	Ref<HeightFrame> collision_height_frame;
	{ //collision and attractors

		frame_params.collider_count = 0;
		frame_params.attractor_count = 0;
		frame_params.particle_size = p_particles->collision_base_size;

		RID collision_3d_textures[ParticlesFrameParams::MAX_3D_TEXTURES];
		RID collision_heightmap_texture;

		Transform3D to_particles;
		if (p_particles->use_local_coords) {
			to_particles = p_particles->emission_transform.affine_inverse();
		}

		if (update_sdf_collision) {
			//2D collision

			Transform2D xform = p_particles->sdf_collision_transform; //will use dotproduct manually so invert beforehand

			if (!p_particles->use_local_coords) {
				Transform2D emission;
				emission.columns[0] = Vector2(p_particles->emission_transform.basis.get_column(0).x, p_particles->emission_transform.basis.get_column(0).y);
				emission.columns[1] = Vector2(p_particles->emission_transform.basis.get_column(1).x, p_particles->emission_transform.basis.get_column(1).y);
				emission.set_origin(Vector2(p_particles->emission_transform.origin.x, p_particles->emission_transform.origin.y));
				xform = xform * emission.affine_inverse();
			}

			Transform2D revert = xform.affine_inverse();
			frame_params.collider_count = 1;
			frame_params.colliders[0].transform[0] = xform.columns[0][0];
			frame_params.colliders[0].transform[1] = xform.columns[0][1];
			frame_params.colliders[0].transform[2] = 0;
			frame_params.colliders[0].transform[3] = xform.columns[2][0];

			frame_params.colliders[0].transform[4] = xform.columns[1][0];
			frame_params.colliders[0].transform[5] = xform.columns[1][1];
			frame_params.colliders[0].transform[6] = 0;
			frame_params.colliders[0].transform[7] = xform.columns[2][1];

			frame_params.colliders[0].transform[8] = revert.columns[0][0];
			frame_params.colliders[0].transform[9] = revert.columns[0][1];
			frame_params.colliders[0].transform[10] = 0;
			frame_params.colliders[0].transform[11] = revert.columns[2][0];

			frame_params.colliders[0].transform[12] = revert.columns[1][0];
			frame_params.colliders[0].transform[13] = revert.columns[1][1];
			frame_params.colliders[0].transform[14] = 0;
			frame_params.colliders[0].transform[15] = revert.columns[2][1];

			frame_params.colliders[0].extents[0] = p_particles->sdf_collision_to_screen.size.x;
			frame_params.colliders[0].extents[1] = p_particles->sdf_collision_to_screen.size.y;
			frame_params.colliders[0].extents[2] = p_particles->sdf_collision_to_screen.position.x;
			frame_params.colliders[0].scale = p_particles->sdf_collision_to_screen.position.y;
			frame_params.colliders[0].texture_index = 0;
			frame_params.colliders[0].type = ParticlesFrameParams::COLLISION_TYPE_2D_SDF;

			collision_heightmap_texture = p_particles->sdf_collision_texture;

			//replace in all other history frames where used because parameters are no longer valid if screen moves
			for (ParticlesFrameParams &params : p_particles->frame_history) {
				if (params.collider_count > 0 && params.colliders[0].type == ParticlesFrameParams::COLLISION_TYPE_2D_SDF) {
					params.colliders[0] = frame_params.colliders[0];
				}
			}
		}

		uint32_t collision_3d_textures_used = 0;
		for (const RID &E : p_particles->collisions) {
			ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(E);
			if (!pci || !pci->active) {
				continue;
			}
			ParticlesCollision *pc = particles_collision_owner.get_or_null(pci->collision);
			ERR_CONTINUE(!pc);

			Transform3D to_collider = pci->transform;
			if (p_particles->use_local_coords) {
				to_collider = to_particles * to_collider;
			}
			Vector3 scale = to_collider.basis.get_scale();
			to_collider.basis.orthonormalize();

			if (pc->type <= RSE::PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT) {
				//attractor
				if (frame_params.attractor_count >= ParticlesFrameParams::MAX_ATTRACTORS) {
					continue;
				}

				ParticlesFrameParams::Attractor &attr = frame_params.attractors[frame_params.attractor_count];

				RendererRD::MaterialStorage::store_transform(to_collider, attr.transform);
				attr.strength = pc->attractor_strength;
				attr.attenuation = pc->attractor_attenuation;
				attr.directionality = pc->attractor_directionality;

				switch (pc->type) {
					case RSE::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT: {
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_SPHERE;
						float radius = pc->radius;
						radius *= (scale.x + scale.y + scale.z) / 3.0;
						attr.extents[0] = radius;
						attr.extents[1] = radius;
						attr.extents[2] = radius;
					} break;
					case RSE::PARTICLES_COLLISION_TYPE_BOX_ATTRACT: {
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_BOX;
						Vector3 extents = pc->extents * scale;
						attr.extents[0] = extents.x;
						attr.extents[1] = extents.y;
						attr.extents[2] = extents.z;
					} break;
					case RSE::PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT: {
						if (collision_3d_textures_used >= ParticlesFrameParams::MAX_3D_TEXTURES) {
							continue;
						}
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_VECTOR_FIELD;
						Vector3 extents = pc->extents * scale;
						attr.extents[0] = extents.x;
						attr.extents[1] = extents.y;
						attr.extents[2] = extents.z;
						attr.texture_index = collision_3d_textures_used;

						collision_3d_textures[collision_3d_textures_used] = pc->field_texture;
						collision_3d_textures_used++;
					} break;
					default: {
					}
				}

				frame_params.attractor_count++;
			} else {
				//collider
				if (frame_params.collider_count >= ParticlesFrameParams::MAX_COLLIDERS) {
					continue;
				}

				ParticlesFrameParams::Collider &col = frame_params.colliders[frame_params.collider_count];

				RendererRD::MaterialStorage::store_transform(to_collider, col.transform);
				switch (pc->type) {
					case RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE: {
						col.type = ParticlesFrameParams::COLLISION_TYPE_SPHERE;
						float radius = pc->radius;
						radius *= (scale.x + scale.y + scale.z) / 3.0;
						col.extents[0] = radius;
						col.extents[1] = radius;
						col.extents[2] = radius;
					} break;
					case RSE::PARTICLES_COLLISION_TYPE_BOX_COLLIDE: {
						col.type = ParticlesFrameParams::COLLISION_TYPE_BOX;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
					} break;
					case RSE::PARTICLES_COLLISION_TYPE_SDF_COLLIDE: {
						if (collision_3d_textures_used >= ParticlesFrameParams::MAX_3D_TEXTURES) {
							continue;
						}
						col.type = ParticlesFrameParams::COLLISION_TYPE_SDF;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
						col.texture_index = collision_3d_textures_used;
						col.scale = (scale.x + scale.y + scale.z) * 0.333333333333; //non uniform scale non supported

						collision_3d_textures[collision_3d_textures_used] = pc->field_texture;
						collision_3d_textures_used++;
					} break;
					case RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE: {
						if (collision_heightmap_texture != RID()) { //already taken
							continue;
						}
						if (pci->scene_managed) {
							// Managed sources never borrow an untracked legacy target.
							if (_scene_height_current_acquire(E, RD::get_singleton(), UINT32_MAX, collision_height_frame) != OK) {
								continue;
							}
							collision_heightmap_texture = collision_height_frame->resources->get_depth();
						} else {
							if (pci->heightfield_texture.is_null()) {
								continue;
							}
							collision_heightmap_texture = pci->heightfield_texture;
						}

						col.type = ParticlesFrameParams::COLLISION_TYPE_HEIGHT_FIELD;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
					} break;
					default: {
					}
				}

				frame_params.collider_count++;
			}
		}

		bool different = collision_3d_textures_used != p_particles->collision_3d_textures_used;
		if (!different) {
			for (uint32_t i = 0; i < collision_3d_textures_used; i++) {
				if (p_particles->collision_3d_textures[i] != collision_3d_textures[i]) {
					different = true;
					break;
				}
			}
		}

		if (collision_heightmap_texture != p_particles->collision_heightmap_texture) {
			different = true;
		}

		bool uniform_set_valid = RD::get_singleton()->uniform_set_is_valid(p_particles->collision_textures_uniform_set);

		if (different || !uniform_set_valid) {
			collision_set_rebuilt = true;
			if (uniform_set_valid) {
				RD::get_singleton()->free_rid(p_particles->collision_textures_uniform_set);
			}

			thread_local LocalVector<RD::Uniform> uniforms;
			uniforms.clear();

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 0;
				for (uint32_t i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
					RID rd_tex;
					if (i < collision_3d_textures_used) {
						if (TextureStorage::get_singleton()->texture_get_type(collision_3d_textures[i]) == TextureStorage::TYPE_3D) {
							rd_tex = TextureStorage::get_singleton()->texture_get_rd_texture(collision_3d_textures[i]);
						}
					}

					if (rd_tex == RID()) {
						rd_tex = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);
					}
					u.append_id(rd_tex);
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				if (collision_heightmap_texture.is_valid()) {
					u.append_id(collision_heightmap_texture);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_BLACK));
				}
				uniforms.push_back(u);
			}
			p_particles->collision_textures_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, 2);
			if (p_particles->collision_textures_uniform_set.is_null()) {
				ERR_PRINT("Failed to create particle collision texture uniform set.");
				return fail_process();
			}
			p_particles->collision_3d_textures_used = collision_3d_textures_used;
			for (uint32_t i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
				p_particles->collision_3d_textures[i] = i < collision_3d_textures_used ? collision_3d_textures[i] : RID();
			}
			p_particles->collision_heightmap_texture = collision_heightmap_texture;
		}
	}

	ParticlesShader::PushConstant push_constant;

	uint32_t process_amount = 0;
	if (!_particles_get_total_amount(p_particles, process_amount)) {
		ERR_PRINT("Particle process amount exceeds the rendering device limit.");
		return fail_process();
	}
	push_constant.clear = p_particles->clear;
	push_constant.total_particles = p_particles->amount;
	push_constant.lifetime = p_particles->lifetime;
	push_constant.trail_size = p_particles->trail_params.size();
	push_constant.use_fractional_delta = p_particles->fractional_delta;
	push_constant.sub_emitter_mode = !p_particles->emitting && p_particles->emission_buffer && (p_particles->emission_buffer->particle_count > 0 || p_particles->force_sub_emit);
	push_constant.trail_pass = false;

	Particles *sub_emitter = particles_owner.get_or_null(p_particles->sub_emitter);
	const bool sub_emitter_was_emitting = sub_emitter && sub_emitter->emission_storage_buffer.is_valid() && sub_emitter->emitting;

	if (sub_emitter && sub_emitter->emission_storage_buffer.is_valid()) {
		//	print_line("updating subemitter buffer");
		int32_t zero[4] = { 0, sub_emitter->amount, 0, 0 };
		if (RD::get_singleton()->buffer_update(sub_emitter->emission_storage_buffer, 0, sizeof(uint32_t) * 4, zero) != OK) {
			ERR_PRINT("Failed to reset particle sub-emitter buffer.");
			return fail_process();
		}
		push_constant.can_emit = true;

		// The sub-emitter state is committed only after all frame resources and
		// the compute list have been prepared successfully. A failed upload must
		// leave the same request pending for the next retry.

	} else {
		push_constant.can_emit = false;
	}

	if (p_particles->emission_buffer && p_particles->emission_buffer->particle_count) {
		uint32_t emission_upload_size = 0;
		if (!_particles_get_buffer_size(uint64_t(p_particles->emission_buffer->particle_count), sizeof(ParticleEmissionBuffer::Data), sizeof(uint32_t) * 4, emission_upload_size)) {
			ERR_PRINT("Particle emission upload exceeds the rendering device buffer size limit.");
			return fail_process();
		}
		if (RD::get_singleton()->buffer_update(p_particles->emission_storage_buffer, 0, emission_upload_size, p_particles->emission_buffer) != OK) {
			ERR_PRINT("Failed to update particle emission buffer.");
			return fail_process();
		}
	}

	if (p_particles->trail_params.size() > 1) {
		//fill the trail params
		for (uint32_t i = 0; i < p_particles->trail_params.size(); i++) {
			uint32_t src_idx = i * p_particles->frame_history.size() / p_particles->trail_params.size();
			if (p_particles->speed_scale <= 0.0) {
				// Stop trails.
				src_idx = 0;
			}
			p_particles->trail_params[i] = p_particles->frame_history[src_idx];
		}
	} else {
		p_particles->trail_params[0] = p_particles->frame_history[0];
	}

	uint32_t frame_params_upload_size = 0;
	if (!_particles_get_buffer_size(uint64_t(p_particles->trail_params.size()), sizeof(ParticlesFrameParams), 0, frame_params_upload_size)) {
		ERR_PRINT("Particle frame parameters upload exceeds the rendering device buffer size limit.");
		return fail_process();
	}
	if (RD::get_singleton()->buffer_update(p_particles->frame_params_buffer, 0, frame_params_upload_size, p_particles->trail_params.ptr()) != OK) {
		ERR_PRINT("Failed to update particle frame parameters buffer.");
		return fail_process();
	}

	ParticleProcessMaterialData *m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(p_particles->process_material, MaterialStorage::SHADER_TYPE_PARTICLES));
	if (!m) {
		m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(particles_shader.default_material, MaterialStorage::SHADER_TYPE_PARTICLES));
	}

	if (!m) {
		return fail_process();
	}
	if (!RD::get_singleton()->compute_pipeline_is_valid(m->shader_data->pipeline.get_rid())) {
		ERR_PRINT("Particle process pipeline is invalid.");
		return fail_process();
	}
	if (!RD::get_singleton()->uniform_set_is_valid(particles_shader.base_uniform_set)) {
		ERR_PRINT("Particle base uniform set is invalid.");
		return fail_process();
	}
	const bool custom_uniforms_required = m->shader_data->ubo_size > 0 || !m->shader_data->texture_uniforms.is_empty();
	if (custom_uniforms_required && !RD::get_singleton()->uniform_set_is_valid(m->uniform_set)) {
		ERR_PRINT("Particle custom material uniform set is invalid.");
		return fail_process();
	}

	p_particles->has_collision_cache = m->shader_data->uses_collision;

	auto *rd = RD::get_singleton();
	Ref<NativeHeightRange> height_reader;
	auto finish_height_reader = [&](Error p_error) -> Error {
		if (height_reader.is_null()) return p_error;
		bool closed = false;
		const Error finished = rd->_native_height_reader_finish(height_reader, closed);
		bool unknown = false;
		const Error recorded = rd->_native_height_reader_error(height_reader, unknown);
		// RD retains an unclosed/unknown prefix for its original owner. Never
		// manufacture closure or return its producer/resources from here.
		const Error requested = closed ? rd->_native_height_reader_request_return(height_reader) : OK;
		if (p_error != OK) return p_error;
		if (recorded != OK) return recorded;
		if (finished != OK) return finished;
		if (unknown) return ERR_INVALID_DATA;
		return closed ? requested : ERR_BUSY;
	};
	auto stop_height_reader = [&](Error p_error) -> bool {
		p_particles->process_error = p_error;
		ERR_PRINT("Managed particle Height reader failed: " + itos(p_error));
		return false; // A partially recorded frame must not replay as an upload retry.
	};

	// Each reader owns one physical list. The original trail barrier closes
	// the first list and opens a second; preserve that boundary below.
	RD::ComputeListID compute_list = RD::INVALID_ID;
	if (collision_height_frame.is_valid()) {
		const Error prepared = rd->_native_height_reader_prepare(collision_height_frame->native_range, p_particles->collision_textures_uniform_set, COLLISION_TEXTURTES_UNIFORM_SET, height_reader);
		if (prepared != OK) {
			ERR_PRINT("Managed particle Height reader prepare failed: " + itos(prepared));
			return fail_process(); // No reader or sampled prefix was admitted.
		}
		const Error begun = rd->_native_height_reader_begin(height_reader, compute_list);
		if (begun != OK) return stop_height_reader(finish_height_reader(begun));
	} else {
		compute_list = rd->compute_list_begin();
	}
	if (compute_list == RD::INVALID_ID) {
		if (height_reader.is_valid()) return stop_height_reader(finish_height_reader(ERR_CANT_CREATE));
		ERR_PRINT("Particle process compute list could not be opened.");
		return fail_process();
	}
	auto bind_process_compute_state = [&]() {
		rd->compute_list_bind_compute_pipeline(compute_list, m->shader_data->pipeline.get_rid());
		rd->compute_list_bind_uniform_set(compute_list, particles_shader.base_uniform_set, BASE_UNIFORM_SET);
		rd->compute_list_bind_uniform_set(compute_list, p_particles->particles_material_uniform_set, MATERIAL_UNIFORM_SET);
		rd->compute_list_bind_uniform_set(compute_list, p_particles->collision_textures_uniform_set, COLLISION_TEXTURTES_UNIFORM_SET);
		if (m->uniform_set.is_valid() && rd->uniform_set_is_valid(m->uniform_set)) {
			rd->compute_list_bind_uniform_set(compute_list, m->uniform_set, 3);
		}
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ParticlesShader::PushConstant));
	};
	bind_process_compute_state();
	if (m->uniform_set.is_valid() && rd->uniform_set_is_valid(m->uniform_set)) {
		m->set_as_used();
	}

	auto dispatch_process_chunks = [&](uint32_t total_threads) -> bool {
		// The particle shaders use a 64-thread X workgroup. Chunking keeps the
		// dispatch within the device's X workgroup-count limit.
		const uint64_t max_threads = uint64_t(RD::get_singleton()->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)) * 64;
		if (max_threads == 0) {
			return false;
		}
		for (uint64_t offset = 0; offset < total_threads;) {
			const uint32_t chunk = uint32_t(MIN(max_threads, uint64_t(total_threads) - offset));
			push_constant.invocation_offset = uint32_t(offset);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ParticlesShader::PushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, chunk, 1, 1);
			offset += chunk;
		}
		return true;
	};

	if (p_particles->trails_enabled && p_particles->trail_bind_poses.size() > 1) {
		//trails requires two passes in order to catch particle starts
		if (!dispatch_process_chunks(process_amount / p_particles->trail_bind_poses.size())) {
			if (height_reader.is_valid()) return stop_height_reader(finish_height_reader(ERR_CANT_CREATE));
			RD::get_singleton()->compute_list_end();
			return fail_process();
		}

		if (height_reader.is_valid()) {
			// compute_list_add_barrier is end + begin + state restoration. Finish
			// this authenticated list and open exactly one reader for the next.
			const Error finished = finish_height_reader(OK);
			if (finished != OK) return stop_height_reader(finished);
			height_reader.unref();
			const Error prepared = rd->_native_height_reader_prepare(collision_height_frame->native_range, p_particles->collision_textures_uniform_set, COLLISION_TEXTURTES_UNIFORM_SET, height_reader);
			if (prepared != OK) return stop_height_reader(prepared);
			const Error begun = rd->_native_height_reader_begin(height_reader, compute_list);
			if (begun != OK) return stop_height_reader(finish_height_reader(begun));
			bind_process_compute_state();
		} else {
			rd->compute_list_add_barrier(compute_list);
		}

		push_constant.trail_pass = true;
		if (!dispatch_process_chunks(process_amount - p_particles->amount)) {
			if (height_reader.is_valid()) return stop_height_reader(finish_height_reader(ERR_CANT_CREATE));
			RD::get_singleton()->compute_list_end();
			return fail_process();
		}
	} else {
		if (!dispatch_process_chunks(process_amount)) {
			if (height_reader.is_valid()) return stop_height_reader(finish_height_reader(ERR_CANT_CREATE));
			RD::get_singleton()->compute_list_end();
			return fail_process();
		}
	}

	if (height_reader.is_valid()) {
		const Error finished = finish_height_reader(OK);
		if (finished != OK) return stop_height_reader(finished);
	} else {
		rd->compute_list_end();
	}

	// Commit CPU-side work items only after every required GPU update and the
	// compute list recording succeeded. Until this point a retry must observe
	// the original emission, clear, and sub-emitter state unchanged.
	_particles_commit_process_state(p_particles, sub_emitter, sub_emitter_was_emitting, true);
	p_particles->trail_params_speed_scale = p_particles->speed_scale;
	return true;
}

void ParticlesStorage::particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND_MSG(particles->external, "ExternalOutput particles require their immutable output owner.");

	if (particles->draw_order != RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD) {
		return;
	}

	if (particles->particle_buffer.is_null() || particles->particle_instance_buffer.is_null() || !RD::get_singleton()->uniform_set_is_valid(particles->particles_copy_uniform_set) || !RD::get_singleton()->uniform_set_is_valid(particles->trail_bind_pose_uniform_set)) {
		return; // Particle copy resources are not ready.
	}

	uint32_t total_amount = 0;
	if (!_particles_get_total_amount(particles, total_amount)) {
		ERR_PRINT("Particle amount exceeds the rendering device limit.");
		return;
	}

	bool do_sort = particles->draw_order == RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH;

	const uint32_t copy_mode = do_sort ? ParticlesShader::COPY_MODE_FILL_INSTANCES_WITH_SORT_BUFFER : ParticlesShader::COPY_MODE_FILL_INSTANCES;
	const RID copy_pipeline = particles_shader.copy_pipelines[particles->userdata_count][copy_mode].get_rid();
	if (!RD::get_singleton()->compute_pipeline_is_valid(copy_pipeline)) {
		ERR_PRINT("Particle view copy pipeline is invalid.");
		return;
	}
	RID sort_pipeline;
	if (do_sort) {
		sort_pipeline = particles_shader.copy_pipelines[particles->userdata_count][ParticlesShader::COPY_MODE_FILL_SORT_BUFFER].get_rid();
		if (!RD::get_singleton()->compute_pipeline_is_valid(sort_pipeline)) {
			ERR_PRINT("Particle view sort-fill pipeline is invalid.");
			return;
		}
	}

	//copy to sort buffer
	if (do_sort && particles->particles_sort_buffer == RID()) {
		uint32_t size = particles->amount;
		if (size & 1) {
			if (size == UINT32_MAX) {
				ERR_PRINT("Particle sort buffer exceeds the rendering device buffer size limit.");
				return;
			}
			size++; //make multiple of 16
		}
		if (!_particles_get_buffer_size(size, sizeof(float) * 2, 0, size)) {
			ERR_PRINT("Particle sort buffer exceeds the rendering device buffer size limit.");
			return;
		}
		particles->particles_sort_buffer = RD::get_singleton()->storage_buffer_create(size);
		if (particles->particles_sort_buffer.is_null()) {
			ERR_PRINT("Failed to create particle sort buffer.");
			return;
		}

		{
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(particles->particles_sort_buffer);
				uniforms.push_back(u);
			}

			particles->particles_sort_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, ParticlesShader::COPY_MODE_FILL_SORT_BUFFER), 1);
			if (particles->particles_sort_uniform_set.is_null()) {
				ERR_PRINT("Failed to create particle sort uniform set.");
				RD::get_singleton()->free_rid(particles->particles_sort_buffer);
				particles->particles_sort_buffer = RID();
				return;
			}
		}
	}

	ParticlesShader::CopyPushConstant copy_push_constant;
	copy_push_constant.invocation_offset = 0;

	if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
		int fixed_fps = screen_refresh_rate;
		if (particles->fixed_fps > 0) {
			fixed_fps = particles->fixed_fps;
		}

		copy_push_constant.trail_size = particles->trail_bind_poses.size();
		copy_push_constant.trail_total = particles->frame_history.size();
		copy_push_constant.frame_delta = 1.0 / fixed_fps;
	} else {
		copy_push_constant.trail_size = 1;
		copy_push_constant.trail_total = 1;
		copy_push_constant.frame_delta = 0.0;
	}

	copy_push_constant.order_by_lifetime = (particles->draw_order == RSE::PARTICLES_DRAW_ORDER_LIFETIME || particles->draw_order == RSE::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME);
	copy_push_constant.lifetime_split = (MIN(int(particles->amount * particles->phase), particles->amount - 1) + 1) % particles->amount;
	copy_push_constant.lifetime_reverse = particles->draw_order == RSE::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME;
	copy_push_constant.motion_vectors_current_offset = particles->instance_motion_vectors_current_offset;

	copy_push_constant.frame_remainder = particles->interpolate ? particles->frame_remainder : 0.0;
	copy_push_constant.total_particles = total_amount;
	copy_push_constant.copy_mode_2d = false;

	Vector3 axis = -p_axis; // cameras look to z negative

	if (particles->use_local_coords) {
		axis = particles->emission_transform.basis.xform_inv(axis).normalized();
	}

	copy_push_constant.sort_direction[0] = axis.x;
	copy_push_constant.sort_direction[1] = axis.y;
	copy_push_constant.sort_direction[2] = axis.z;

	copy_push_constant.align_up[0] = p_up_axis.x;
	copy_push_constant.align_up[1] = p_up_axis.y;
	copy_push_constant.align_up[2] = p_up_axis.z;

	copy_push_constant.align_mode = particles->transform_align;
	copy_push_constant.align_src = particles->transform_align_src;
	copy_push_constant.align_axis = uint32_t(particles->transform_align_axis);
	copy_push_constant.pad2 = uint32_t(0);

	if (do_sort) {
		if (!RD::get_singleton()->uniform_set_is_valid(particles->particles_sort_uniform_set)) {
			ERR_PRINT("Particle view sort uniform set is invalid.");
			return;
		}
		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
		if (compute_list == RD::INVALID_ID) {
			ERR_PRINT("Particle view sort-fill compute list could not be opened.");
			return;
		}

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, sort_pipeline);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_sort_uniform_set, 1);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

		const uint64_t max_threads = uint64_t(RD::get_singleton()->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)) * 64;
		if (max_threads == 0) {
			RD::get_singleton()->compute_list_end();
			return;
		}
		for (uint64_t offset = 0; offset < uint64_t(particles->amount);) {
			const uint32_t chunk = uint32_t(MIN(max_threads, uint64_t(particles->amount) - offset));
			copy_push_constant.invocation_offset = uint32_t(offset);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));
			RD::get_singleton()->compute_list_dispatch_threads(compute_list, chunk, 1, 1);
			offset += chunk;
		}

		RD::get_singleton()->compute_list_end();
		if (!sort_effects->sort_buffer(particles->particles_sort_uniform_set, particles->amount)) {
			return;
		}
	}

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	if (compute_list == RD::INVALID_ID) {
		ERR_PRINT("Particle view copy compute list could not be opened.");
		return;
	}
	copy_push_constant.copy_mode_2d = particles->mode == RSE::PARTICLES_MODE_2D ? 1 : 0;
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy_pipeline);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
	if (do_sort) {
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_sort_uniform_set, 1);
	}
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

	const uint64_t max_threads = uint64_t(RD::get_singleton()->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)) * 64;
	if (max_threads == 0) {
		RD::get_singleton()->compute_list_end();
		return;
	}
	for (uint64_t offset = 0; offset < copy_push_constant.total_particles;) {
		const uint32_t chunk = uint32_t(MIN(max_threads, uint64_t(copy_push_constant.total_particles) - offset));
		copy_push_constant.invocation_offset = uint32_t(offset);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, chunk, 1, 1);
		offset += chunk;
	}

	RD::get_singleton()->compute_list_end();
}

void ParticlesStorage::_particles_update_buffers(Particles *particles) {
	ERR_FAIL_COND(particles->external);
	uint32_t userdata_count = 0;

	if (particles->process_material.is_valid()) {
		ParticleProcessMaterialData *material_data = static_cast<ParticleProcessMaterialData *>(MaterialStorage::get_singleton()->material_get_data(particles->process_material, MaterialStorage::SHADER_TYPE_PARTICLES));
		if (material_data && material_data->shader_data->version.is_valid() && material_data->shader_data->valid) {
			userdata_count = material_data->shader_data->userdata_count;
		}
	}

	bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0 || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	bool index_draw_order = particles->draw_order == RSE::ParticlesDrawOrder::PARTICLES_DRAW_ORDER_INDEX;
	bool enable_motion_vectors = uses_motion_vectors && index_draw_order && !particles->instance_motion_vectors_enabled;
	bool only_instances_changed = false;

	if (userdata_count != particles->userdata_count) {
		// Mismatch userdata, re-create all buffers.
		_particles_free_data(particles);
	} else if (enable_motion_vectors || (particles->particle_buffer.is_valid() && (particles->particle_instance_buffer.is_null() || !RD::get_singleton()->uniform_set_is_valid(particles->particles_copy_uniform_set)))) {
		// Rebuild missing instance resources or upgrade their motion vector layout.
		if (RD::get_singleton()->uniform_set_is_valid(particles->particles_copy_uniform_set)) {
			RD::get_singleton()->free_rid(particles->particles_copy_uniform_set);
			particles->particles_copy_uniform_set = RID();
		}
		if (particles->particle_instance_buffer.is_valid()) {
			RD::get_singleton()->free_rid(particles->particle_instance_buffer);
			particles->particle_instance_buffer = RID();
		}

		particles->particles_transforms_buffer_uniform_set = RID();
		only_instances_changed = true;
	} else if (!particles->particle_buffer.is_null()) {
		// No operation is required because a buffer already exists, return early.
		return;
	}

	if (particles->amount > 0) {
		uint32_t total_amount = 0;
		if (!_particles_get_total_amount(particles, total_amount)) {
			ERR_PRINT("Particle amount exceeds the rendering device limit.");
			return;
		}

		uint32_t xform_size = particles->mode == RSE::PARTICLES_MODE_2D ? 2 : 3;
		bool particle_buffer_created = false;
		if (particles->particle_buffer.is_null()) {
			uint32_t particle_buffer_size = 0;
			if (!_particles_get_buffer_size(total_amount, sizeof(ParticleData) + uint64_t(userdata_count) * sizeof(float) * 4, 0, particle_buffer_size)) {
				ERR_PRINT("Particle buffer exceeds the rendering device buffer size limit.");
				return;
			}
			particles->particle_buffer = RD::get_singleton()->storage_buffer_create(particle_buffer_size);
			if (particles->particle_buffer.is_null()) {
				return;
			}
			particles->userdata_count = userdata_count;
			particles->restart_request = true;
			particle_buffer_created = true;
		}

		PackedByteArray data;
		uint32_t particle_instance_buffer_size = 0;
		if (!_particles_get_buffer_size(total_amount, uint64_t(xform_size + 1 + 1) * sizeof(float) * 4, 0, particle_instance_buffer_size)) {
			ERR_PRINT("Particle instance buffer exceeds the rendering device buffer size limit.");
			if (particle_buffer_created) {
				RD::get_singleton()->free_rid(particles->particle_buffer);
				particles->particle_buffer = RID();
			}
			return;
		}
		if (uses_motion_vectors) {
			if (particle_instance_buffer_size > UINT32_MAX / 2) {
				ERR_PRINT("Particle motion vector buffer exceeds the rendering device buffer size limit.");
				if (particle_buffer_created) {
					RD::get_singleton()->free_rid(particles->particle_buffer);
					particles->particle_buffer = RID();
				}
				return;
			}
			particle_instance_buffer_size *= 2;
		}

		data.resize_initialized(particle_instance_buffer_size);

		particles->particle_instance_buffer = RD::get_singleton()->storage_buffer_create(particle_instance_buffer_size, data);
		if (particles->particle_instance_buffer.is_null()) {
			if (particle_buffer_created) {
				RD::get_singleton()->free_rid(particles->particle_buffer);
				particles->particle_buffer = RID();
			}
			return;
		}

		{
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 1;
				u.append_id(particles->particle_buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 2;
				u.append_id(particles->particle_instance_buffer);
				uniforms.push_back(u);
			}

			particles->particles_copy_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, 0), 0);
			if (particles->particles_copy_uniform_set.is_null()) {
				ERR_PRINT("Failed to create particle copy uniform set.");
				RD::get_singleton()->free_rid(particles->particle_instance_buffer);
				particles->particle_instance_buffer = RID();
				if (particle_buffer_created) {
					RD::get_singleton()->free_rid(particles->particle_buffer);
					particles->particle_buffer = RID();
				}
				return;
			}
		}

		particles->instance_motion_vectors_enabled = uses_motion_vectors;
		particles->instance_motion_vectors_current_offset = 0;
		particles->instance_motion_vectors_previous_offset = 0;
		particles->instance_motion_vectors_last_change = -1;

		if (only_instances_changed) {
			// The simulation buffer was retained, but the renderer must retrieve the rebuilt instances uniform.
			particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES_INSTANCES);
		}
	}
}
void ParticlesStorage::update_particles() {
	if (!particle_update_list.first()) {
		return;
	}

	RENDER_TIMESTAMP("Update GPUParticles");
	uint32_t frame = RSG::rasterizer->get_frame_number();
	bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0 || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	SelfList<Particles>::List retry_list;
	while (particle_update_list.first()) {
		//use transform feedback to process particles

		Particles *particles = particle_update_list.first()->self();

		particles->update_list.remove_from_list();
		if (particles->external) {
			continue;
		}
		if (particles->process_error != OK) {
			particles->retry_pending = false;
			particles->dirty = false;
			continue;
		}
		const bool retry_candidate = !particles->restart_request && particles->retry_pending && particles->frame_delta_committed && particles->frame_delta_committed_frame == frame;
		bool previous_inactive = particles->inactive;
		double previous_inactive_time = particles->inactive_time;
		particles->retry_pending = false;
		particles->dirty = false;
		auto queue_retry = [&]() {
			if (particles->process_error != OK) {
				particles->dirty = false;
				return; // Native prefix failure is owned by the exact range, not a replay.
			}
			// Inactivity is derived from the frame delta. Keep a failed update
			// retryable instead of permanently consuming the frame's lifetime.
			particles->inactive = previous_inactive;
			particles->inactive_time = previous_inactive_time;
			particles->dirty = true;
			particles->retry_pending = true;
			if (!particles->update_list.in_list()) {
				retry_list.add(&particles->update_list);
			}
		};

		_particles_update_buffers(particles);
		bool retrying = retry_candidate && particles->frame_delta_committed && particles->frame_delta_committed_frame == frame;
		if (particles->amount <= 0) {
			continue;
		}
		if (particles->particle_buffer.is_null() || particles->particle_instance_buffer.is_null() || !RD::get_singleton()->uniform_set_is_valid(particles->particles_copy_uniform_set)) {
			queue_retry();
			continue;
		}

		if (particles->restart_request) {
			particles->prev_ticks = 0;
			particles->phase = 0;
			particles->prev_phase = 0;
			particles->clear = true;
			particles->restart_request = false;
			particles->frame_remainder = 0.0;
			particles->frame_delta_committed = false;
			particles->frame_delta_committed_frame = UINT32_MAX;
			particles->pre_process_time_remaining = 0.0;
			particles->pre_process_pending = false;
			retrying = false;
		}

		if (particles->inactive && !particles->emitting) {
			//go next
			continue;
		}
		if (particles->emitting) {
			if (particles->inactive) {
				//restart system from scratch
				particles->prev_ticks = 0;
				particles->phase = 0;
				particles->prev_phase = 0;
				particles->clear = true;
				particles->frame_remainder = 0.0;
				particles->frame_delta_committed = false;
				particles->frame_delta_committed_frame = UINT32_MAX;
				particles->pre_process_time_remaining = 0.0;
				particles->pre_process_pending = false;
				retrying = false;
				// Activation survives a later failure, including before frame-delta
				// debit, so retrying must not restart already processed substeps.
				previous_inactive = false;
				previous_inactive_time = 0;
			}
			particles->inactive = false;
			particles->inactive_time = 0;
		} else {
			particles->inactive_time += particles->speed_scale * RendererCompositorRD::get_singleton()->get_frame_delta_time();
			if (particles->inactive_time > particles->lifetime * 1.2) {
				particles->inactive = true;
				continue;
			}
		}

		const uint64_t max_dispatch_threads = RD::get_singleton()->limit_get(RD::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X) * 64;
		if (max_dispatch_threads == 0) {
			ERR_PRINT("Particle compute workgroup count limit is zero.");
			queue_retry();
			continue;
		}
		const bool copy_required = particles->draw_order != RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY && particles->transform_align != RSE::PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD;
		RID copy_pipeline;
		if (copy_required) {
			// Validate the copy pipeline before committing simulation steps.
			copy_pipeline = particles_shader.copy_pipelines[particles->userdata_count][ParticlesShader::COPY_MODE_FILL_INSTANCES].get_rid();
			if (!RD::get_singleton()->compute_pipeline_is_valid(copy_pipeline)) {
				ERR_PRINT("Particle copy pipeline is invalid.");
				queue_retry();
				continue;
			}
		}

		int fixed_fps = 0;
		if (particles->fixed_fps > 0) {
			fixed_fps = particles->fixed_fps;
		} else if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
			fixed_fps = screen_refresh_rate;
		}
		{
			//update trails
			int history_size = 1;
			int trail_steps = 1;
			if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
				history_size = MAX(1, int(particles->trail_lifetime * fixed_fps));
				trail_steps = particles->trail_bind_poses.size();
			}

			if (uint32_t(history_size) != particles->frame_history.size()) {
				particles->frame_history.resize(history_size);
				memset(particles->frame_history.ptr(), 0, sizeof(ParticlesFrameParams) * history_size);
				// Set the frame number so that we are able to distinguish an uninitialized
				// frame from the true frame number zero. See issue #88712 for details.
				for (int i = 0; i < history_size; i++) {
					particles->frame_history[i].frame = UINT32_MAX;
				}
			}

			if (uint32_t(trail_steps) != particles->trail_params.size() || particles->frame_params_buffer.is_null()) {
				particles->trail_params.resize(trail_steps);
				if (particles->frame_params_buffer.is_valid()) {
					RD::get_singleton()->free_rid(particles->frame_params_buffer);
				}
				uint32_t frame_params_size = 0;
				if (!_particles_get_buffer_size(uint64_t(trail_steps), sizeof(ParticlesFrameParams), 0, frame_params_size)) {
					ERR_PRINT("Particle frame parameters buffer exceeds the rendering device buffer size limit.");
					queue_retry();
					continue;
				}
				particles->frame_params_buffer = RD::get_singleton()->storage_buffer_create(frame_params_size);
				if (particles->frame_params_buffer.is_null()) {
					ERR_PRINT("Failed to create particle frame parameters buffer.");
					queue_retry();
					continue;
				}
			}

			if (particles->trail_bind_poses.size() > 1 && particles->trail_bind_pose_buffer.is_null()) {
				uint32_t trail_bind_pose_size = 0;
				if (!_particles_get_buffer_size(uint64_t(particles->trail_bind_poses.size()), sizeof(float) * 16, 0, trail_bind_pose_size)) {
					ERR_PRINT("Particle trail bind pose buffer exceeds the rendering device buffer size limit.");
					queue_retry();
					continue;
				}
				particles->trail_bind_pose_buffer = RD::get_singleton()->storage_buffer_create(trail_bind_pose_size);
				if (particles->trail_bind_pose_buffer.is_null()) {
					ERR_PRINT("Failed to create particle trail bind pose buffer.");
					queue_retry();
					continue;
				}
				particles->trail_bind_poses_dirty = true;
			}

			if (!_particles_ensure_trail_uniform(particles, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, 0))) {
				queue_retry();
				continue;
			}

			if (particles->trail_bind_pose_buffer.is_valid() && particles->trail_bind_poses_dirty) {
				uint32_t trail_bind_pose_elements = 0;
				if (!_particles_get_buffer_size(uint64_t(particles->trail_bind_poses.size()), 16, 0, trail_bind_pose_elements)) {
					ERR_PRINT("Particle trail bind pose staging buffer exceeds the rendering device buffer size limit.");
					queue_retry();
					continue;
				}
				if (particles_shader.pose_update_buffer.size() < trail_bind_pose_elements) {
					particles_shader.pose_update_buffer.resize(trail_bind_pose_elements);
				}

				for (int i = 0; i < particles->trail_bind_poses.size(); i++) {
					RendererRD::MaterialStorage::store_transform(particles->trail_bind_poses[i], &particles_shader.pose_update_buffer[i * 16]);
				}

				uint32_t trail_bind_pose_upload_size = 0;
				if (!_particles_get_buffer_size(uint64_t(particles->trail_bind_poses.size()), sizeof(float) * 16, 0, trail_bind_pose_upload_size)) {
					ERR_PRINT("Particle trail bind pose upload exceeds the rendering device buffer size limit.");
					queue_retry();
					continue;
				}
				if (RD::get_singleton()->buffer_update(particles->trail_bind_pose_buffer, 0, trail_bind_pose_upload_size, particles_shader.pose_update_buffer.ptr()) != OK) {
					ERR_PRINT("Failed to update particle trail bind pose buffer.");
					queue_retry();
					continue;
				}
				particles->trail_bind_poses_dirty = false;
			}
		}

		// Validate process times before comparisons.
		if (!Math::is_finite(particles->pre_process_time) || !Math::is_finite(particles->request_process_time) || !Math::is_finite(particles->request_process_time_residual)) {
			ERR_PRINT("Particle process times must be finite.");
			queue_retry();
			continue;
		}
		const double frame_delta_input = RendererCompositorRD::get_singleton()->get_frame_delta_time();
		if (!Math::is_finite(frame_delta_input) || !Math::is_finite(particles->speed_scale) || !Math::is_finite(particles->frame_remainder)) {
			ERR_PRINT("Particle frame delta inputs must be finite.");
			queue_retry();
			continue;
		}
		const double speed_scale_input = MAX(particles->speed_scale, 0.0);
		if (!Math::is_finite(speed_scale_input) || !Math::is_finite((float)speed_scale_input)) {
			ERR_PRINT("Particle speed scale exceeds the supported range.");
			queue_retry();
			continue;
		}
		float todo = (particles->clear || particles->pre_process_pending) ? (particles->pre_process_pending ? particles->pre_process_time_remaining : particles->pre_process_time) : 0;
		todo = todo > particles->request_process_time ? todo : particles->request_process_time;
		todo = todo > particles->request_process_time_residual ? todo : particles->request_process_time_residual;

		bool process_ok = true;
		if (!Math::is_finite(todo)) {
			ERR_PRINT("Particle process time is not finite.");
			queue_retry();
			continue;
		}
		if (todo > 0.0) {
			real_t frame_time;
			if (fixed_fps > 0) {
				frame_time = 1.0 / fixed_fps;
			} else {
				frame_time = 1.0 / 30.0;
			}

			const double tmp_scale = particles->speed_scale;
			// We need this otherwise the speed scale of the particle system influences the TODO.
			particles->speed_scale = 1.0;
			auto process_time = [&](float p_time, real_t *r_remaining = nullptr) -> bool {
				if (!Math::is_finite(p_time)) {
					return false;
				}
				while (p_time > 0.0f) {
					const float step = frame_time > p_time ? p_time : frame_time;
					const float next_time = p_time - frame_time;
					// Very large finite values can become infinity when converted to float,
					// or be too coarse for a frame-sized decrement to change them. Either
					// case would otherwise keep this loop running forever.
					if (!(next_time < p_time)) {
						ERR_PRINT("Particle process time is too large to make progress.");
						return false;
					}
					if (!_particles_process(particles, step)) {
						return false;
					}
					p_time = next_time;
					if (r_remaining != nullptr) {
						*r_remaining = MAX(p_time, 0.0f);
					}
				}
				return true;
			};
			if (particles->clear || particles->pre_process_pending) {
				if (!particles->pre_process_pending) {
					particles->pre_process_time_remaining = particles->pre_process_time;
					particles->pre_process_pending = true;
				}
				process_ok = process_time(particles->pre_process_time_remaining, &particles->pre_process_time_remaining);
				if (process_ok) {
					particles->pre_process_pending = false;
				}
			}
			if (process_ok && particles->request_process_time > 0.0) {
				process_ok = process_time(particles->request_process_time, &particles->request_process_time);
			}
			if (process_ok && particles->request_process_time_residual > 0.0) {
				const bool previous_emitting = particles->emitting;
				particles->emitting = false;
				process_ok = process_time(particles->request_process_time_residual, &particles->request_process_time_residual);
				if (!process_ok) {
					particles->emitting = previous_emitting;
				}
			}
			particles->speed_scale = tmp_scale;
			if (!process_ok) {
				queue_retry();
				continue;
			}
		}

		const double frame_delta = frame_delta_input;
		const double time_scale_double = speed_scale_input;
		if (!Math::is_finite(time_scale_double)) {
			ERR_PRINT("Particle speed scale is not finite.");
			queue_retry();
			continue;
		}
		float time_scale = (float)time_scale_double;
		if (!Math::is_finite(time_scale)) {
			ERR_PRINT("Particle speed scale exceeds the supported range.");
			queue_retry();
			continue;
		}
		if (fixed_fps > 0) {
			float frame_time = 1.0 / fixed_fps;
			float delta = (float)frame_delta;
			if (!Math::is_finite(delta)) {
				ERR_PRINT("Particle frame delta exceeds the supported range.");
				queue_retry();
				continue;
			}
			if (delta > 0.1) { //avoid recursive stalls if fps goes below 10
				delta = 0.1;
			} else if (delta < 0.0) {
				delta = 0.0;
			}
			if (!retrying) {
				todo = particles->frame_remainder + delta * time_scale;
				if (!Math::is_finite(todo)) {
					ERR_PRINT("Particle frame remainder exceeds the supported range.");
					queue_retry();
					continue;
				}
				if (todo >= frame_time && !((todo - frame_time) < todo)) {
					ERR_PRINT("Particle frame remainder is too large to make progress.");
					queue_retry();
					continue;
				}
				particles->frame_remainder = todo;
				particles->frame_delta_committed = true;
				particles->frame_delta_committed_frame = frame;
			} else {
				todo = particles->frame_remainder;
			}

			while (todo >= frame_time || particles->clear) {
				const float next_todo = todo > frame_time ? todo - frame_time : 0.0f;
				if (todo >= frame_time && !(next_todo < todo)) {
					ERR_PRINT("Particle frame remainder is too large to make progress.");
					process_ok = false;
					break;
				}
				if (!_particles_process(particles, frame_time)) {
					process_ok = false;
					break;
				}
				todo = next_todo;
				particles->frame_remainder = todo;
			}

			if (!process_ok) {
				queue_retry();
				continue;
			}
			particles->frame_remainder = todo;
		} else {
			const double variable_delta = frame_delta * time_scale_double;
			if (!Math::is_finite(variable_delta) || !Math::is_finite((float)variable_delta) || !_particles_process(particles, variable_delta)) {
				queue_retry();
				continue;
			}
		}
		// Ensure that memory is initialized (the code above should ensure that _particles_process is always called at least once upon clearing).
		DEV_ASSERT(!particles->clear);

		uint32_t total_amount = 0;
		if (!_particles_get_total_amount(particles, total_amount)) {
			ERR_PRINT("Particle amount exceeds the rendering device limit.");
			queue_retry();
			continue;
		}

		// Swap offsets for motion vectors. Motion vectors can only be used when the draw order keeps the indices consistent across frames.
		bool index_draw_order = particles->draw_order == RSE::ParticlesDrawOrder::PARTICLES_DRAW_ORDER_INDEX;
		particles->instance_motion_vectors_previous_offset = particles->instance_motion_vectors_current_offset;
		if (uses_motion_vectors && index_draw_order && particles->instance_motion_vectors_enabled && (frame - particles->instance_motion_vectors_last_change) == 1) {
			particles->instance_motion_vectors_current_offset = total_amount - particles->instance_motion_vectors_current_offset;
		}

		particles->instance_motion_vectors_last_change = frame;

		// Copy particles to instance buffer.
		if (copy_required) {
			//does not need view dependent operation, do copy here
			ParticlesShader::CopyPushConstant copy_push_constant;
			copy_push_constant.invocation_offset = 0;

			// Affect 2D only.
			if (particles->use_local_coords) {
				// In local mode, particle positions are calculated locally (relative to the node position)
				// and they're also drawn locally.
				// It works as expected, so we just pass an identity transform.
				//RendererRD::MaterialStorage::store_transform(Transform3D(), copy_push_constant.inv_emission_transform);
				copy_push_constant.inv_emission_transform[0] = 1.0;
				copy_push_constant.inv_emission_transform[1] = 0.0;
				copy_push_constant.inv_emission_transform[2] = 0.0;
				copy_push_constant.inv_emission_transform[3] = 0.0;
				copy_push_constant.inv_emission_transform[4] = 1.0;
				copy_push_constant.inv_emission_transform[5] = 0.0;
				copy_push_constant.inv_emission_transform[6] = 0.0;
				copy_push_constant.inv_emission_transform[7] = 0.0;
				copy_push_constant.inv_emission_transform[8] = 1.0;
				copy_push_constant.inv_emission_transform[9] = 0.0;
				copy_push_constant.inv_emission_transform[10] = 0.0;
				copy_push_constant.inv_emission_transform[11] = 0.0;
			} else {
				// In global mode, particle positions are calculated globally (relative to the canvas origin)
				// but they're drawn locally.
				// So, we need to pass the inverse of the emission transform to bring the
				// particles to local coordinates before drawing.
				Transform3D inv = particles->emission_transform.affine_inverse();
				copy_push_constant.inv_emission_transform[0] = inv.basis.rows[0][0];
				copy_push_constant.inv_emission_transform[1] = inv.basis.rows[1][0];
				copy_push_constant.inv_emission_transform[2] = inv.basis.rows[2][0];
				copy_push_constant.inv_emission_transform[3] = inv.basis.rows[0][1];
				copy_push_constant.inv_emission_transform[4] = inv.basis.rows[1][1];
				copy_push_constant.inv_emission_transform[5] = inv.basis.rows[2][1];
				copy_push_constant.inv_emission_transform[6] = inv.basis.rows[0][2];
				copy_push_constant.inv_emission_transform[7] = inv.basis.rows[1][2];
				copy_push_constant.inv_emission_transform[8] = inv.basis.rows[2][2];
				copy_push_constant.inv_emission_transform[9] = inv.origin.x;
				copy_push_constant.inv_emission_transform[10] = inv.origin.y;
				copy_push_constant.inv_emission_transform[11] = inv.origin.z;
			}

			copy_push_constant.total_particles = total_amount;
			copy_push_constant.frame_remainder = particles->interpolate ? particles->frame_remainder : 0.0;
			copy_push_constant.align_mode = particles->transform_align;
			copy_push_constant.align_up[0] = 0;
			copy_push_constant.align_up[1] = 0;
			copy_push_constant.align_up[2] = 0;
			copy_push_constant.align_src = particles->transform_align_src;
			copy_push_constant.align_axis = uint32_t(particles->transform_align_axis);

			if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
				copy_push_constant.trail_size = particles->trail_bind_poses.size();
				copy_push_constant.trail_total = particles->frame_history.size();
				copy_push_constant.frame_delta = 1.0 / fixed_fps;
			} else {
				copy_push_constant.trail_size = 1;
				copy_push_constant.trail_total = 1;
				copy_push_constant.frame_delta = 0.0;
			}

			copy_push_constant.order_by_lifetime = (particles->draw_order == RSE::PARTICLES_DRAW_ORDER_LIFETIME || particles->draw_order == RSE::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME);
			copy_push_constant.lifetime_split = (MIN(int(particles->amount * particles->phase), particles->amount - 1) + 1) % particles->amount;
			copy_push_constant.lifetime_reverse = particles->draw_order == RSE::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME;
			copy_push_constant.motion_vectors_current_offset = particles->instance_motion_vectors_current_offset;

			RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
			copy_push_constant.copy_mode_2d = particles->mode == RSE::PARTICLES_MODE_2D ? 1 : 0;
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy_pipeline);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

			for (uint64_t offset = 0; offset < total_amount;) {
				const uint32_t chunk = uint32_t(MIN(max_dispatch_threads, uint64_t(total_amount) - offset));
				copy_push_constant.invocation_offset = uint32_t(offset);
				RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));
				RD::get_singleton()->compute_list_dispatch_threads(compute_list, chunk, 1, 1);
				offset += chunk;
			}

			RD::get_singleton()->compute_list_end();
		}

		// Clear one-shot process requests only after the complete update has
		// reached its successful GPU submission. If processing or resource setup
		// fails above, process_time() has retained the unconsumed remainder and
		// the queued retry must be able to finish it.
		particles->request_process_time = 0.0;
		particles->request_process_time_residual = 0.0;
		particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
		particles->frame_delta_committed = false;
		particles->frame_delta_committed_frame = UINT32_MAX;
	}
	// Retry failed systems on the next update.
	while (retry_list.first()) {
		SelfList<Particles> *retry = retry_list.first();
		retry->remove_from_list();
		if (!retry->in_list()) {
			particle_update_list.add(retry);
		}
	}
}

Dependency *ParticlesStorage::particles_get_dependency(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, nullptr);

	return &particles->dependency;
}

bool ParticlesStorage::particles_is_inactive(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);
	if (particles->external) {
		return particles->external->state == ExternalOutputOwner::GenerationState::RELEASED;
	}
	return !particles->emitting && particles->inactive;
}

/* Particles SHADER */

void ParticlesStorage::ParticlesShaderData::set_code(const String &p_code) {
	ParticlesStorage *particles_storage = ParticlesStorage::get_singleton();
	//compile

	code = p_code;
	valid = false;
	ubo_size = 0;
	uniforms.clear();
	uses_collision = false;

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;
	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["start"] = ShaderCompiler::STAGE_COMPUTE;
	actions.entry_point_stages["process"] = ShaderCompiler::STAGE_COMPUTE;

	/*
	uses_time = false;

	actions.render_mode_flags["use_half_res_pass"] = &uses_half_res;
	actions.render_mode_flags["use_quarter_res_pass"] = &uses_quarter_res;

	actions.usage_flag_pointers["TIME"] = &uses_time;
*/

	actions.usage_flag_pointers["COLLIDED"] = &uses_collision;

	userdata_count = 0;
	for (uint32_t i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
		userdatas_used[i] = false;
		actions.usage_flag_pointers["USERDATA" + itos(i + 1)] = &userdatas_used[i];
	}

	actions.uniforms = &uniforms;

	Error err = particles_storage->particles_shader.compiler.compile(RSE::SHADER_PARTICLES, code, &actions, path, gen_code);
	ERR_FAIL_COND_MSG(err != OK, "Shader compilation failed.");

	if (version.is_null()) {
		version = particles_storage->particles_shader.shader.version_create();
	} else {
		pipeline.free();
	}

	for (uint32_t i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
		if (userdatas_used[i]) {
			userdata_count++;
		}
	}

	particles_storage->particles_shader.shader.version_set_compute_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_COMPUTE], gen_code.defines);
	ERR_FAIL_COND(!particles_storage->particles_shader.shader.version_is_valid(version));

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;

	//update pipelines

	pipeline.create_compute_pipeline(particles_storage->particles_shader.shader.version_get_shader(version, 0));

	valid = true;
}

bool ParticlesStorage::ParticlesShaderData::is_animated() const {
	return false;
}

bool ParticlesStorage::ParticlesShaderData::casts_shadows() const {
	return false;
}

RenderingServerTypes::ShaderNativeSourceCode ParticlesStorage::ParticlesShaderData::get_native_source_code() const {
	return ParticlesStorage::get_singleton()->particles_shader.shader.version_get_native_source_code(version);
}

Pair<ShaderRD *, RID> ParticlesStorage::ParticlesShaderData::get_native_shader_and_version() const {
	return { &ParticlesStorage::get_singleton()->particles_shader.shader, version };
}

ParticlesStorage::ParticlesShaderData::~ParticlesShaderData() {
	pipeline.free();

	if (version.is_valid()) {
		ParticlesStorage::get_singleton()->particles_shader.shader.version_free(version);
	}
}

MaterialStorage::ShaderData *ParticlesStorage::_create_particles_shader_func() {
	ParticlesShaderData *shader_data = memnew(ParticlesShaderData);
	return shader_data;
}

bool ParticlesStorage::ParticleProcessMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, ParticlesStorage::get_singleton()->particles_shader.shader.version_get_shader(shader_data->version, 0), 3, true, false);
}

ParticlesStorage::ParticleProcessMaterialData::~ParticleProcessMaterialData() {
	free_parameters_uniform_set(uniform_set);
}

MaterialStorage::MaterialData *ParticlesStorage::_create_particles_material_func(ParticlesShaderData *p_shader) {
	ParticleProcessMaterialData *material_data = memnew(ParticleProcessMaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}
////////

/* PARTICLES COLLISION API */

RID ParticlesStorage::particles_collision_allocate() {
	return particles_collision_owner.allocate_rid();
}
void ParticlesStorage::particles_collision_initialize(RID p_rid) {
	particles_collision_owner.initialize_rid(p_rid, ParticlesCollision());
}

void ParticlesStorage::_heightfield_free_instance_target(ParticlesCollisionInstance *p_instance) {
	// Detach new borrows; independent samples keep their exact frozen frame.
	p_instance->current_frame.unref();
	p_instance->staged_height_resources.unref();
	p_instance->staged_height_bridge = {};
	p_instance->staged_height_facet = {};
	if (p_instance->heightfield_fb.is_valid()) {
		RD::get_singleton()->free_rid(p_instance->heightfield_fb);
		p_instance->heightfield_fb = RID();
	}
	if (p_instance->heightfield_texture.is_valid()) {
		RD::get_singleton()->free_rid(p_instance->heightfield_texture);
		p_instance->heightfield_texture = RID();
	}
}

void ParticlesStorage::_heightfield_invalidate_base_instances(RID p_collision) {
	ParticlesCollision *collision = particles_collision_owner.get_or_null(p_collision);
	ERR_FAIL_NULL(collision);
	for (const RID &rid : collision->instances) {
		ParticlesCollisionInstance *instance = particles_collision_instance_owner.get_or_null(rid);
		if (instance) _heightfield_free_instance_target(instance);
	}
}

void ParticlesStorage::particles_collision_free(RID p_rid) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(particles_collision);
	_heightfield_invalidate_base_instances(p_rid);
	particles_collision->dependency.deleted_notify(p_rid);
	particles_collision_owner.free(p_rid);
}

RID ParticlesStorage::particles_collision_get_heightfield_framebuffer(RID p_collision_instance) const {
	ParticlesCollisionInstance *instance = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL_V(instance, RID());
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(instance->collision);
	ERR_FAIL_NULL_V(particles_collision, RID());
	ERR_FAIL_COND_V(particles_collision->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE, RID());

	if (instance->heightfield_texture == RID()) {
		//create
		const int resolutions[RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX] = { 256, 512, 1024, 2048, 4096, 8192 };
		Size2i size;
		if (particles_collision->extents.x <= 0.0 || particles_collision->extents.z <= 0.0) {
			size.x = resolutions[particles_collision->heightfield_resolution];
			size.y = size.x;
		} else if (particles_collision->extents.x > particles_collision->extents.z) {
			size.x = resolutions[particles_collision->heightfield_resolution];
			size.y = MAX(1, int32_t(particles_collision->extents.z / particles_collision->extents.x * size.x));
		} else {
			size.y = resolutions[particles_collision->heightfield_resolution];
			size.x = MAX(1, int32_t(particles_collision->extents.x / particles_collision->extents.z * size.y));
		}

		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_D32_SFLOAT;
		tf.width = size.x;
		tf.height = size.y;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		instance->heightfield_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
		if (instance->heightfield_texture.is_null()) {
			return RID();
		}

		Vector<RID> fb_tex;
		fb_tex.push_back(instance->heightfield_texture);
		instance->heightfield_fb = RD::get_singleton()->framebuffer_create(fb_tex);
		if (instance->heightfield_fb.is_null()) {
			RD::get_singleton()->free_rid(instance->heightfield_texture);
			instance->heightfield_texture = RID();
			return RID();
		}
		instance->heightfield_fb_size = size;
	}

	return instance->heightfield_fb;
}

void ParticlesStorage::particles_collision_set_collision_type(RID p_particles_collision, RSE::ParticlesCollisionType p_type) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_INDEX(int(p_type), RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE + 1);

	if (p_type == particles_collision->type) {
		return;
	}
	ERR_FAIL_COND_MSG(particles_collision->scene_shape_revision == UINT64_MAX, "Particle collision scene shape revision exhausted.");

	_heightfield_invalidate_base_instances(p_particles_collision);
	particles_collision->type = p_type;
	particles_collision->scene_shape_revision++;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->cull_mask = p_cull_mask;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_CULL_MASK);
}

uint32_t ParticlesStorage::particles_collision_get_cull_mask(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, 0);
	return particles_collision->cull_mask;
}

uint32_t ParticlesStorage::particles_collision_get_height_field_mask(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, false);
	return particles_collision->heightfield_mask;
}

void ParticlesStorage::particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->heightfield_mask = p_heightfield_mask;
}

void ParticlesStorage::particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_COND_MSG(!Math::is_finite(p_radius) || p_radius < 0.0, "Particle collision sphere radius must be finite and non-negative.");
	ERR_FAIL_COND_MSG(particles_collision->scene_shape_revision == UINT64_MAX, "Particle collision scene shape revision exhausted.");

	particles_collision->radius = p_radius;
	particles_collision->scene_shape_revision++;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_COND_MSG(!p_extents.is_finite() || p_extents.x < 0.0 || p_extents.y < 0.0 || p_extents.z < 0.0, "Particle collision box extents must be finite and non-negative.");

	const Vector3 previous_extents = particles_collision->extents;
	if (particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE &&
			(previous_extents.x != p_extents.x || previous_extents.y != p_extents.y || previous_extents.z != p_extents.z)) {
		ERR_FAIL_COND_MSG(particles_collision->scene_shape_revision == UINT64_MAX, "Particle collision scene shape revision exhausted.");
	}
	if (particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE &&
			(previous_extents.x != p_extents.x || previous_extents.z != p_extents.z)) {
		_heightfield_invalidate_base_instances(p_particles_collision);
	}

	particles_collision->extents = p_extents;
	if (particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE &&
			(previous_extents.x != p_extents.x || previous_extents.y != p_extents.y || previous_extents.z != p_extents.z)) {
		particles_collision->scene_shape_revision++;
	}
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_COND_MSG(!Math::is_finite(p_strength), "Particle attractor strength must be finite.");

	particles_collision->attractor_strength = p_strength;
}

void ParticlesStorage::particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_COND_MSG(!Math::is_finite(p_directionality) || p_directionality < 0.0 || p_directionality > 1.0, "Particle attractor directionality must be finite and between zero and one.");

	particles_collision->attractor_directionality = p_directionality;
}

void ParticlesStorage::particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_COND_MSG(!Math::is_finite(p_curve) || p_curve < 0.0 || p_curve > 8.0, "Particle attractor attenuation must be finite and between zero and eight.");

	particles_collision->attractor_attenuation = p_curve;
}

void ParticlesStorage::particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->field_texture = p_texture;
}

void ParticlesStorage::particles_collision_height_field_update(RID p_particles_collision) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_height_field_resolution(RID p_particles_collision, RSE::ParticlesCollisionHeightfieldResolution p_resolution) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_INDEX(p_resolution, RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX);

	if (particles_collision->heightfield_resolution == p_resolution) {
		return;
	}
	ERR_FAIL_COND_MSG(particles_collision->scene_shape_revision == UINT64_MAX, "Particle collision scene shape revision exhausted.");

	particles_collision->heightfield_resolution = p_resolution;
	particles_collision->scene_shape_revision++;

	_heightfield_invalidate_base_instances(p_particles_collision);
}

AABB ParticlesStorage::particles_collision_get_aabb(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, AABB());

	switch (particles_collision->type) {
		case RSE::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT:
		case RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE: {
			AABB aabb;
			aabb.position = -Vector3(1, 1, 1) * particles_collision->radius;
			aabb.size = Vector3(2, 2, 2) * particles_collision->radius;
			return aabb;
		}
		default: {
			AABB aabb;
			aabb.position = -particles_collision->extents;
			aabb.size = particles_collision->extents * 2;
			return aabb;
		}
	}
}

Vector3 ParticlesStorage::particles_collision_get_extents(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, Vector3());
	return particles_collision->extents;
}

bool ParticlesStorage::particles_collision_is_heightfield(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, false);
	return particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE;
}

Dependency *ParticlesStorage::particles_collision_get_dependency(RID p_particles_collision) const {
	ParticlesCollision *pc = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(pc, nullptr);

	return &pc->dependency;
}

/* Particles collision instance */

RID ParticlesStorage::particles_collision_instance_create(RID p_collision) {
	ERR_FAIL_COND_V(next_scene_source_key == 0, RID());
	ParticlesCollision *collision = particles_collision_owner.get_or_null(p_collision);
	ERR_FAIL_NULL_V(collision, RID());
	ParticlesCollisionInstance pci;
	pci.collision = p_collision;
	pci.scene_source_key = next_scene_source_key++;
	RID instance = particles_collision_instance_owner.make_rid(pci);
	collision->instances.insert(instance);
	return instance;
}

void ParticlesStorage::particles_collision_instance_free(RID p_rid) {
	ParticlesCollisionInstance *instance = particles_collision_instance_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(instance);
	ParticlesCollision *collision = particles_collision_owner.get_or_null(instance->collision);
	if (collision) collision->instances.erase(p_rid);
	_heightfield_free_instance_target(instance);
	particles_collision_instance_owner.free(p_rid);
}

RID ParticlesStorage::particles_collision_instance_get_collision(RID p_collision_instance) const {
	const ParticlesCollisionInstance *instance = particles_collision_instance_owner.get_or_null(p_collision_instance);
	return instance ? instance->collision : RID();
}

void ParticlesStorage::particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	ERR_FAIL_COND_MSG(!p_transform.is_finite(), "Particle collision instance transform must contain only finite values.");
	pci->transform = p_transform;
}

void ParticlesStorage::particles_collision_instance_set_active(RID p_collision_instance, bool p_active) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	pci->active = p_active;
}

bool ParticlesStorage::particles_collision_instance_get_scene_sphere(RID p_instance, uint64_t &r_source_key, uint64_t &r_shape_revision, Transform3D &r_transform, float &r_radius) const {
	const ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_instance);
	if (!pci || !pci->active || pci->scene_source_key == 0) {
		return false;
	}
	const ParticlesCollision *pc = particles_collision_owner.get_or_null(pci->collision);
	if (!pc || pc->type != RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE) {
		return false;
	}
	r_source_key = pci->scene_source_key;
	r_shape_revision = pc->scene_shape_revision;
	r_transform = pci->transform;
	r_radius = pc->radius;
	return true;
}

bool ParticlesStorage::particles_collision_instance_get_scene_heightfield(RID p_instance, uint64_t &r_source_key, uint64_t &r_shape_revision, Transform3D &r_transform, Vector3 &r_extents, RSE::ParticlesCollisionHeightfieldResolution &r_resolution) const {
	const ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_instance);
	if (!pci || !pci->active || pci->scene_source_key == 0) {
		return false;
	}
	const ParticlesCollision *pc = particles_collision_owner.get_or_null(pci->collision);
	if (!pc || pc->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE) {
		return false;
	}
	r_source_key = pci->scene_source_key;
	r_shape_revision = pc->scene_shape_revision;
	r_transform = pci->transform;
	r_extents = pc->extents;
	r_resolution = pc->heightfield_resolution;
	return true;
}

Error ParticlesStorage::_scene_height_current_acquire(RID p_instance, const RenderingDevice *p_device, uint32_t p_max_resolution, Ref<HeightFrame> &r_frame) const {
	if (!p_device || p_device->render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (r_frame.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	const ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_instance);
	if (!pci || !pci->active || !pci->scene_managed || !pci->scene_source_key || pci->current_frame.is_null()) {
		return ERR_UNAVAILABLE;
	}
	const ParticlesCollision *pc = particles_collision_owner.get_or_null(pci->collision);
	if (!pc || pc->type != RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE) {
		return ERR_UNAVAILABLE;
	}
	const Ref<HeightFrame> &current = pci->current_frame;
	const auto &snapshot = current->snapshot;
	// Same actual source/version checks as the managed legacy reader. Current
	// is issued only by the original accepted C + checked native producer path.
	if (current->state != HeightFrame::State::Current || !current->content_ordinal ||
			snapshot.instance != p_instance || snapshot.base != pci->collision || snapshot.source_generation != pci->scene_source_key ||
			snapshot.shape_revision != pc->scene_shape_revision || snapshot.transform != pci->transform || snapshot.extents != pc->extents || snapshot.resolution != pc->heightfield_resolution) {
		return ERR_UNAVAILABLE;
	}
	if (snapshot.width > p_max_resolution || snapshot.height > p_max_resolution) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (current->resources.is_null() || &current->resources->device != p_device || !current->resources->is_quoted_verified()) {
		return ERR_UNAVAILABLE;
	}
	// Validate original RD incarnation without observing completion, preparing
	// another reader, or allocating/copying any depth resource. The same frame
	// retains its original resources, complete checked prefix and native range.
	const Error valid = p_device->_native_height_range_validate(current->native_range);
	if (valid != OK) {
		return valid;
	}
	r_frame = current;
	return OK;
}
