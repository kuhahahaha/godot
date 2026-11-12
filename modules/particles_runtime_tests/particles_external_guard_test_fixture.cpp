/**************************************************************************/
/*  particles_external_guard_test_fixture.cpp                             */
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

#include "particles_external_guard_test_fixture.h"

#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/effects/particles_external_guard.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <array>

void ParticlesExternalGuardTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesExternalGuardTestFixture::run);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesExternalGuardTestFixture::result);
	ClassDB::bind_method(D_METHOD("observe_viewport_vrs_texture", "viewport"), &ParticlesExternalGuardTestFixture::observe_viewport_vrs_texture);
	ClassDB::bind_method(D_METHOD("observe_particle_collision_texture", "collision"), &ParticlesExternalGuardTestFixture::observe_particle_collision_texture);
}

void ParticlesExternalGuardTestFixture::check(bool p_ok, const String &p_label) {
	MutexLock lock(result_mutex);
	Dictionary event;
	event["check"] = p_label;
	event["ok"] = p_ok;
	observations.push_back(event);
	if (!p_ok) {
		failures.push_back(p_label);
	}
}

Error ParticlesExternalGuardTestFixture::finish(Error p_error) {
	MutexLock lock(result_mutex);
	outcome = p_error != OK ? p_error : failures.is_empty() ? OK
															: ERR_BUG;
	complete = true;
	return outcome;
}

Error ParticlesExternalGuardTestFixture::run() {
	{
		MutexLock lock(result_mutex);
		if (started) {
			return ERR_ALREADY_IN_USE;
		}
		started = true;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	const bool owner = rs && rd && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance;
	check(owner, "original main RD render-thread owner");
	if (!owner) {
		return finish(ERR_UNAVAILABLE);
	}
	check(!rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active, "outside all active command lists");
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		return finish(ERR_BUSY);
	}
	{
		MutexLock lock(result_mutex);
		device_info["driver"] = OS::get_singleton()->get_current_rendering_driver_name();
		device_info["renderer"] = OS::get_singleton()->get_current_rendering_method();
		device_info["name"] = rd->get_device_name();
		device_info["api"] = rd->get_device_api_version();
	}

	// Observe actual backend compiler facts through the original bounded owner.
	// No feature observation is material/profile qualification or a draw permit.
	{
		auto *materials = RendererRD::MaterialStorage::get_singleton();
		check(materials != nullptr, "material_features.original_owner");
		if (!materials) {
			return finish(ERR_UNAVAILABLE);
		}
		struct FeatureCase {
			const char *name;
			const char *source;
			bool inverse;
			uint32_t rejected_source_features;
		};
		const FeatureCase cases[] = {
			{ "plain", "shader_type spatial; void fragment() { ALBEDO = vec3(1.0); }", false, 0 },
			{ "vertex_inverse", "shader_type spatial; void vertex() { VERTEX = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }", true, 1 },
			{ "fragment_inverse", "shader_type spatial; void fragment() { ALBEDO = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }", true, 0 },
			{ "alpha_hash", "shader_type spatial; void fragment() { ALPHA_HASH_SCALE = 0.5; }", true, 4 },
			{ "alpha_scissor", "shader_type spatial; void fragment() { ALPHA_SCISSOR_THRESHOLD = 0.5; }", false, 4 },
			{ "normal_only", "shader_type spatial; void fragment() { ALBEDO = NORMAL; }", false, 0 },
			{ "both", "shader_type spatial; void fragment() { ALBEDO = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; ALPHA_HASH_SCALE = 0.5; }", true, 4 },
			{ "depth_write", "shader_type spatial; void fragment() { DEPTH = 0.5; }", false, 32 },
			{ "clip_write", "shader_type spatial; void vertex() { Z_CLIP_SCALE = 0.5; }", false, 64 },
			{ "discard", "shader_type spatial; void fragment() { discard; }", false, 8 },
			{ "time", "shader_type spatial; void fragment() { ALBEDO = vec3(TIME); }", false, 16 },
			{ "alpha", "shader_type spatial; void fragment() { ALPHA = 0.5; }", false, 4 },
			{ "depth_disabled", "shader_type spatial; render_mode depth_draw_never;", false, 128 },
			{ "depth_inverted", "shader_type spatial; render_mode depth_test_inverted;", false, 128 },
			{ "screen", "shader_type spatial; uniform sampler2D screen : hint_screen_texture; void fragment() { ALBEDO = texture(screen, SCREEN_UV).rgb; }", false, 256 },
			{ "wireframe", "shader_type spatial; render_mode wireframe;", false, 1024 },
			{ "cull_front", "shader_type spatial; render_mode cull_front;", false, 2048 },
			{ "position_write", "shader_type spatial; void vertex() { POSITION = vec4(VERTEX, 1.0); }", false, 2 },
			{ "modelview_write", "shader_type spatial; void vertex() { MODELVIEW_MATRIX = mat4(1.0); }", false, 2 },
			{ "world_mode", "shader_type spatial; render_mode world_vertex_coords;", false, 2 },
			{ "skip_mode", "shader_type spatial; render_mode skip_vertex_transform;", false, 2 },
			{ "coverage", "shader_type spatial; render_mode alpha_to_coverage;", false, 4 },
			{ "depth_test_disabled", "shader_type spatial; render_mode depth_test_disabled;", false, 128 },
			{ "blend_add", "shader_type spatial; render_mode blend_add;", false, 4 },
			{ "helper_depth", "shader_type spatial; void change_depth(out float value) { value = 0.5; } void fragment() { change_depth(DEPTH); }", false, 32 },
			{ "helper_clip", "shader_type spatial; void change_depth(out float value) { value = 0.5; } void vertex() { change_depth(Z_CLIP_SCALE); }", false, 64 },
		};
		for (const auto &entry : cases) {
			const String label = String("material_features.") + entry.name;
			const RID shader = materials->shader_allocate();
			materials->shader_initialize(shader, false);
			materials->shader_set_code(shader, entry.source);
			const RID material = materials->material_allocate();
			materials->material_initialize(material);
			materials->material_set_shader(material, shader);
			materials->_update_queued_materials();
			const uint64_t generation = materials->material_get_generation(material);
			RendererRD::MaterialStorage::NativeSelectionLease selection;
			RendererRD::MaterialStorage::NativeSelectionShaderObservation observation;
			observation.material = shader;
			observation.material_generation = UINT64_MAX;
			observation.shader_version = shader;
			observation.needs_inverse = !entry.inverse;
			observation.material_recipe_rejections = 0x5a5a5a5a;
			const auto unchanged = [&]() {
				return observation.material == shader && observation.material_generation == UINT64_MAX && observation.shader_version == shader && observation.needs_inverse == !entry.inverse && observation.material_recipe_rejections == 0x5a5a5a5a;
			};
			check(materials->material_selection_get_shader_observation(selection, observation) == ERR_UNAUTHORIZED && unchanged(), label + ".observation_unissued_unchanged");
			RendererRD::MaterialStorage::NativeSelectionShaderFeatures features;
			features.material = shader;
			features.material_generation = UINT64_MAX;
			features.compiled.material_recipe_rejections = 0x5a5a5a5a;
			check(materials->material_selection_get_shader_features(selection, features) == ERR_UNAUTHORIZED && features.material == shader && features.material_generation == UINT64_MAX && features.compiled.material_recipe_rejections == 0x5a5a5a5a, label + ".unissued_unchanged");
			const Error acquired = materials->material_selection_acquire(material, selection, generation);
			check(acquired == OK, label + ".selection");
			if (acquired == OK) {
				const Error observed = materials->material_selection_get_shader_features(selection, features);
				check(observed == OK, label + ".compiled_fact");
				if (observed == OK) {
					check(features.material == material && features.material_generation == generation, label + ".generation");
					check(features.compiled.shader && features.compiled.version.is_valid(), label + ".original_version");
					check(features.compiled.needs_inverse == entry.inverse, label + ".inverse_requirement");
					check(features.compiled.material_recipe_rejections == entry.rejected_source_features, label + ".compiled_gate");
				}
				const Error projected = materials->material_selection_get_shader_observation(selection, observation);
				check(projected == OK, label + ".observation");
				if (projected == OK) {
					check(observation.material == material && observation.material_generation == generation, label + ".observation_original_material");
					check(observation.shader_version.is_valid() && observation.shader_version == features.compiled.version, label + ".observation_original_version");
					check(observation.needs_inverse == entry.inverse, label + ".observation_inverse");
					check(observation.material_recipe_rejections == entry.rejected_source_features, label + ".observation_gate");
				}
				check(materials->material_selection_release(selection) == OK, label + ".release");
				const auto returned = observation;
				check(materials->material_selection_get_shader_observation(selection, observation) == ERR_UNAUTHORIZED && observation.material == returned.material && observation.material_generation == returned.material_generation && observation.shader_version == returned.shader_version && observation.needs_inverse == returned.needs_inverse && observation.material_recipe_rejections == returned.material_recipe_rejections, label + ".observation_returned_unchanged");
				const uint32_t returned_features_mask = features.compiled.material_recipe_rejections;
				check(materials->material_selection_get_shader_features(selection, features) == ERR_UNAUTHORIZED && features.compiled.material_recipe_rejections == returned_features_mask, label + ".released_refused");
			}
			// Empty code invalidates the previous successful compiler observation.
			materials->shader_set_code(shader, "");
			materials->_update_queued_materials();
			RendererRD::MaterialStorage::NativeSelectionLease empty_selection;
			const Error empty_acquired = materials->material_selection_acquire(material, empty_selection, materials->material_get_generation(material));
			check(empty_acquired == ERR_INVALID_DATA && !empty_selection.is_active(), label + ".empty_selection");
			const uint32_t invalidated_features_mask = features.compiled.material_recipe_rejections;
			check(materials->material_selection_get_shader_features(empty_selection, features) == ERR_UNAUTHORIZED && features.compiled.material_recipe_rejections == invalidated_features_mask, label + ".empty_invalidates");
			const auto invalidated = observation;
			check(materials->material_selection_get_shader_observation(empty_selection, observation) == ERR_UNAUTHORIZED && observation.material == invalidated.material && observation.material_generation == invalidated.material_generation && observation.shader_version == invalidated.shader_version && observation.needs_inverse == invalidated.needs_inverse && observation.material_recipe_rejections == invalidated.material_recipe_rejections, label + ".observation_empty_unchanged");
			// Preserve cleanup even if an incorrect admission is exposed.
			if (empty_selection.is_active()) {
				materials->material_selection_release(empty_selection);
			}
			// Reuse the same ShaderData after a rejected/empty source, so a stale
			// rejection mask cannot pass merely by allocating a fresh owner.
			materials->shader_set_code(shader, "shader_type spatial; void fragment() { ALBEDO = vec3(0.25); }");
			materials->_update_queued_materials();
			RendererRD::MaterialStorage::NativeSelectionLease reset_selection;
			const uint64_t reset_generation = materials->material_get_generation(material);
			const Error reset_acquired = materials->material_selection_acquire(material, reset_selection, reset_generation);
			check(reset_acquired == OK, label + ".recompiled_selection");
			if (reset_acquired == OK) {
				RendererRD::MaterialStorage::NativeSelectionShaderObservation reset_observation;
				check(materials->material_selection_get_shader_observation(reset_selection, reset_observation) == OK && reset_observation.material_generation == reset_generation && reset_generation > generation && reset_observation.material_recipe_rejections == 0, label + ".recompiled_clean_gate");
				check(materials->material_selection_release(reset_selection) == OK, label + ".recompiled_return");
			}
			materials->material_free(material);
			materials->shader_free(shader);
		}

		// Independent replacement keeps the old native source alive. This tests
		// retained-source isolation, not a Host resource COW implementation.
		const RID original_shader = materials->shader_allocate();
		materials->shader_initialize(original_shader, false);
		materials->shader_set_code(original_shader, "shader_type spatial; uniform float weight; void fragment() { ALBEDO = vec3(weight); }");
		const RID original_material = materials->material_allocate();
		materials->material_initialize(original_material);
		materials->material_set_shader(original_material, original_shader);
		materials->material_set_param(original_material, "weight", 0.25);
		materials->_update_queued_materials();
		const uint64_t original_generation = materials->material_get_generation(original_material);
		RendererRD::MaterialStorage::NativeSelectionLease retained;
		const Error retained_acquired = materials->material_selection_acquire(original_material, retained, original_generation);
		check(retained_acquired == OK, "material_observation.retained_selection");
		if (retained_acquired == OK) {
			RendererRD::MaterialStorage::NativeSelectionShaderObservation original;
			check(materials->material_selection_get_shader_observation(retained, original) == OK && original.material == original_material && original.material_generation == original_generation && original.shader_version.is_valid() && !original.needs_inverse, "material_observation.retained_original");
			const RID replacement_shader = materials->shader_allocate();
			materials->shader_initialize(replacement_shader, false);
			materials->shader_set_code(replacement_shader, "shader_type spatial; uniform float weight; void fragment() { ALBEDO = (INV_MODEL_MATRIX * vec4(vec3(weight), 1.0)).xyz; }");
			const RID replacement_material = materials->material_allocate();
			materials->material_initialize(replacement_material);
			materials->material_set_shader(replacement_material, replacement_shader);
			materials->material_set_param(replacement_material, "weight", 0.75);
			materials->_update_queued_materials();
			const uint64_t replacement_generation = materials->material_get_generation(replacement_material);
			RendererRD::MaterialStorage::NativeSelectionLease replacement;
			const Error replacement_acquired = materials->material_selection_acquire(replacement_material, replacement, replacement_generation);
			check(replacement_acquired == OK, "material_observation.replacement_selection");
			if (replacement_acquired == OK) {
				RendererRD::MaterialStorage::NativeSelectionShaderObservation current;
				check(materials->material_selection_get_shader_observation(replacement, current) == OK && current.material == replacement_material && current.material_generation == replacement_generation && current.shader_version.is_valid() && current.shader_version != original.shader_version && current.needs_inverse, "material_observation.replacement_original");
				RendererRD::MaterialStorage::NativeSelectionShaderObservation old;
				check(materials->material_selection_get_shader_observation(retained, old) == OK && old.material == original.material && old.material_generation == original.material_generation && old.shader_version == original.shader_version && !old.needs_inverse && double(materials->material_get_param(original_material, "weight")) == 0.25, "material_observation.replacement_preserves_retained");
				check(materials->material_selection_release(replacement) == OK, "material_observation.replacement_return");
				const auto returned = current;
				check(materials->material_selection_get_shader_observation(replacement, current) == ERR_UNAUTHORIZED && current.material == returned.material && current.material_generation == returned.material_generation && current.shader_version == returned.shader_version && current.needs_inverse == returned.needs_inverse, "material_observation.replacement_returned_unchanged");
			}
			materials->material_set_param(replacement_material, "weight", 0.5);
			materials->_update_queued_materials();
			RendererRD::MaterialStorage::NativeSelectionShaderObservation surviving;
			check(materials->material_selection_get_shader_observation(retained, surviving) == OK && surviving.material == original.material && surviving.material_generation == original.material_generation && surviving.shader_version == original.shader_version && !surviving.needs_inverse, "material_observation.sibling_return_preserves_retained");
			materials->material_free(replacement_material);
			materials->shader_free(replacement_shader);
			check(materials->material_selection_release(retained) == OK, "material_observation.retained_return");
			// A real successful mutation advances the original material generation.
			materials->material_set_param(original_material, "weight", 1.0);
			materials->shader_set_code(original_shader, "shader_type spatial; void fragment() { ALPHA_HASH_SCALE = 0.5; }");
			const uint64_t changed_generation = materials->material_get_generation(original_material);
			check(changed_generation > original_generation && double(materials->material_get_param(original_material, "weight")) == 1.0, "material_observation.successful_mutation");
			RendererRD::MaterialStorage::NativeSelectionLease stale;
			check(materials->material_selection_acquire(original_material, stale, original_generation) == ERR_INVALID_DATA && !stale.is_active(), "material_observation.stale_generation_refused");
			if (stale.is_active()) {
				materials->material_selection_release(stale);
			}
			// Queued uniform/texture updates must not yield a selection or facts.
			RendererRD::MaterialStorage::NativeSelectionLease dirty;
			check(materials->material_selection_acquire(original_material, dirty, changed_generation) == ERR_BUSY && !dirty.is_active(), "material_observation.dirty_refused");
			if (dirty.is_active()) {
				materials->material_selection_release(dirty);
			}
			materials->_update_queued_materials();
			RendererRD::MaterialStorage::NativeSelectionLease fresh;
			const Error fresh_acquired = materials->material_selection_acquire(original_material, fresh, changed_generation);
			check(fresh_acquired == OK, "material_observation.mutated_selection");
			if (fresh_acquired == OK) {
				RendererRD::MaterialStorage::NativeSelectionShaderObservation changed;
				check(materials->material_selection_get_shader_observation(fresh, changed) == OK && changed.material == original_material && changed.material_generation == changed_generation && changed.shader_version.is_valid() && changed.needs_inverse, "material_observation.mutated_compiler_fact");
				// The returned old lease cannot observe a newer lease on the same RID.
				const auto unchanged_original = original;
				check(materials->material_selection_get_shader_observation(retained, original) == ERR_UNAUTHORIZED && original.material == unchanged_original.material && original.material_generation == unchanged_original.material_generation && original.shader_version == unchanged_original.shader_version && original.needs_inverse == unchanged_original.needs_inverse, "material_observation.old_lease_cannot_observe_new_selection");
				RendererRD::MaterialStorage::NativeSelectionLease sibling;
				const Error sibling_acquired = materials->material_selection_acquire(original_material, sibling, changed_generation);
				check(sibling_acquired == OK, "material_observation.same_material_sibling_selection");
				check(materials->material_selection_release(fresh) == OK, "material_observation.mutated_return");
				if (sibling_acquired == OK) {
					RendererRD::MaterialStorage::NativeSelectionShaderObservation remaining;
					check(materials->material_selection_get_shader_observation(sibling, remaining) == OK && remaining.material == original_material && remaining.material_generation == changed_generation && remaining.shader_version == changed.shader_version && remaining.needs_inverse, "material_observation.same_material_sibling_survives_return");
					check(materials->material_selection_release(sibling) == OK, "material_observation.same_material_sibling_return");
				}
			}
		}
		materials->material_free(original_material);
		materials->shader_free(original_shader);
	}

	using Guard = RendererRD::ParticlesExternalGuard;
	static_assert(Guard::MODE_COUNT == 5);
	static_assert(Guard::BINDING_COUNT == 12);
	auto source_layout_matches = [](const auto *p_shader, Guard::Mode p_mode) {
		const bool empty = p_mode == Guard::INIT_EMPTY || p_mode == Guard::FINALIZE_EMPTY;
		if (!p_shader || p_shader->uniform_sets.size() != 1 || p_shader->uniform_sets[0].size() != (empty ? 10 : 12)) {
			return false;
		}
		uint32_t seen = 0;
		for (const auto &uniform : p_shader->uniform_sets[0]) {
			if (uniform.binding >= Guard::BINDING_COUNT || (seen & (1u << uniform.binding)) ||
					uniform.type != RD::UNIFORM_TYPE_STORAGE_BUFFER ||
					uniform.writable != (uniform.binding >= Guard::GUARD_SCRATCH && uniform.binding <= Guard::PROTECTED_COMMANDS)) {
				return false;
			}
			seen |= 1u << uniform.binding;
		}
		return seen == (empty ? 0xffcu : 0xfffu);
	};
	const char *names[Guard::MODE_COUNT] = { "INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY" };
	const auto *storage = RendererRD::ParticlesStorage::get_singleton();
	const auto *engine_guard = storage ? storage->get_external_guard() : nullptr;
	check(storage && engine_guard && storage->get_external_guard_preparation_error() == OK && storage->is_external_guard_prepared(), "engine.actual_prepared_owner");
	if (!storage || !engine_guard || storage->get_external_guard_preparation_error() != OK || !storage->is_external_guard_prepared()) {
		return finish(ERR_UNAVAILABLE);
	}
	std::array<RID, Guard::MODE_COUNT> engine_pipelines;
	std::array<RID, Guard::MODE_COUNT> engine_shaders;
	for (int i = 0; i < Guard::MODE_COUNT; ++i) {
		engine_pipelines[i] = engine_guard->get_pipeline(Guard::Mode(i));
		const auto *pipeline = rd->compute_pipeline_owner.get_or_null(engine_pipelines[i]);
		engine_shaders[i] = pipeline ? pipeline->shader : RID();
		const auto *shader = rd->shader_owner.get_or_null(engine_shaders[i]);
		const String label = String("engine.") + names[i];
		check(pipeline && bool(pipeline->driver_id) && rd->compute_pipeline_is_valid(engine_pipelines[i]), label + ".actual_pipeline");
		check(shader && bool(shader->driver_id) && pipeline && pipeline->shader_driver_id == shader->driver_id, label + ".original_shader_association");
		check(pipeline && pipeline->push_constant_size == sizeof(Guard::PushConstant) && pipeline->local_group_size[0] == 64 && pipeline->local_group_size[1] == 1 && pipeline->local_group_size[2] == 1, label + ".reflected_compute_layout");
		check(source_layout_matches(shader, Guard::Mode(i)), label + ".reflected_source_roles");
		bool distinct = true;
		for (int j = 0; j < i; ++j) {
			distinct = distinct && engine_pipelines[i] != engine_pipelines[j] && engine_shaders[i] != engine_shaders[j];
		}
		check(distinct, label + ".distinct_engine_variant");
	}
	Error first_error = OK;
	// All wrapper ownership, including destruction, stays on this callback.
	// Publish completion only after this scope has released its original owner.
	{
		Guard guard;
		std::array<RID, Guard::MODE_COUNT> shaders;
		std::array<RID, Guard::MODE_COUNT> pipelines;
		auto observe_bundle = [&](const String &p_phase) {
			check(guard.is_prepared(), p_phase + ".ready");
			for (int i = 0; i < Guard::MODE_COUNT; ++i) {
				const auto mode = Guard::Mode(i);
				shaders[i] = guard.get_shader(mode);
				pipelines[i] = guard.get_pipeline(mode);
				const auto *shader = rd->shader_owner.get_or_null(shaders[i]);
				const auto *pipeline = rd->compute_pipeline_owner.get_or_null(pipelines[i]);
				const String label = p_phase + "." + names[i];
				check(shaders[i].is_valid() && shader && bool(shader->driver_id), label + ".actual_shader");
				check(pipelines[i].is_valid() && pipeline && bool(pipeline->driver_id) && rd->compute_pipeline_is_valid(pipelines[i]), label + ".actual_pipeline");
				check(shader && pipeline && pipeline->shader == shaders[i] && pipeline->shader_driver_id == shader->driver_id, label + ".original_shader_association");
				check(pipeline && pipeline->push_constant_size == sizeof(Guard::PushConstant) && pipeline->local_group_size[0] == 64 && pipeline->local_group_size[1] == 1 && pipeline->local_group_size[2] == 1, label + ".reflected_compute_layout");
				check(source_layout_matches(shader, mode), label + ".reflected_source_roles");
			}
			bool distinct_shaders = true;
			bool distinct_pipelines = true;
			for (int i = 0; i < Guard::MODE_COUNT; ++i) {
				for (int j = 0; j < i; ++j) {
					distinct_shaders = distinct_shaders && shaders[i] != shaders[j];
					distinct_pipelines = distinct_pipelines && pipelines[i] != pipelines[j];
				}
			}
			check(distinct_shaders, p_phase + ".five_distinct_shaders");
			check(distinct_pipelines, p_phase + ".five_distinct_pipelines");
		};
		auto observe_released = [&](const String &p_phase) {
			check(!guard.is_prepared(), p_phase + ".not_ready");
			for (int i = 0; i < Guard::MODE_COUNT; ++i) {
				check(guard.get_shader(Guard::Mode(i)).is_null() && guard.get_pipeline(Guard::Mode(i)).is_null(), p_phase + "." + names[i] + ".getters_empty");
				check(rd->shader_owner.get_or_null(shaders[i]) == nullptr, p_phase + "." + names[i] + ".shader_owner_removed");
				check(rd->compute_pipeline_owner.get_or_null(pipelines[i]) == nullptr, p_phase + "." + names[i] + ".pipeline_owner_removed");
			}
		};
		auto release = [&](const String &p_phase) {
			const Error error = guard.release_pipelines();
			check(error == OK, p_phase + ".owner_release");
			if (error != OK && first_error == OK) {
				first_error = error;
			}
			observe_released(p_phase);
		};

		first_error = guard.prepare_pipelines();
		check(first_error == OK, "first.prepare");
		if (first_error == OK) {
			observe_bundle("first");
			const auto original_shaders = shaders;
			const auto original_pipelines = pipelines;
			const Error repeated = guard.prepare_pipelines();
			check(repeated == OK, "repeat.prepare");
			if (repeated != OK) {
				first_error = repeated;
			}
			for (int i = 0; i < Guard::MODE_COUNT; ++i) {
				check(guard.get_shader(Guard::Mode(i)) == original_shaders[i] && guard.get_pipeline(Guard::Mode(i)) == original_pipelines[i], String("repeat.") + names[i] + ".same_original_rids");
			}
		}
		release("first_release");
		if (first_error == OK) {
			first_error = guard.prepare_pipelines();
			check(first_error == OK, "reprepare.prepare");
			if (first_error == OK) {
				observe_bundle("reprepare");
			}
		}
		release("final_release");
	}
	// Local wrapper release must preserve the independently engine-owned bundle.
	for (int i = 0; i < Guard::MODE_COUNT; ++i) {
		check(engine_guard->get_pipeline(Guard::Mode(i)) == engine_pipelines[i] && rd->compute_pipeline_is_valid(engine_pipelines[i]), String("engine.") + names[i] + ".survives_local_release");
	}
	return finish(first_error);
}

Dictionary ParticlesExternalGuardTestFixture::result() const {
	MutexLock lock(result_mutex);
	Dictionary report;
	report["schema"] = 1;
	report["started"] = started;
	report["complete"] = complete;
	report["error"] = outcome;
	report["mode_count"] = RendererRD::ParticlesExternalGuard::MODE_COUNT;
	report["checks"] = observations.duplicate(true);
	report["failures"] = failures.duplicate(true);
	report["device"] = device_info.duplicate(true);
	report["dispatch_recorded"] = false;
	report["source_authority_supplied"] = false;
	report["physical_return_claimed"] = false;
	return report;
}

Dictionary ParticlesExternalGuardTestFixture::observe_viewport_vrs_texture(RID p_viewport) const {
	Dictionary observation;
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RendererRD::TextureStorage *storage = RendererRD::TextureStorage::get_singleton();
	const bool owner = rs && rd && storage && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance;
	observation["main_rd_render_thread"] = owner;
	if (!owner) {
		return observation;
	}
	const RID target = rs->viewport_get_render_target(p_viewport);
	const bool target_valid = target.is_valid() && storage->owns_render_target(target);
	observation["target_valid"] = target_valid;
	if (target_valid) {
		observation["render_target"] = target;
		observation["vrs_texture"] = storage->render_target_get_vrs_texture(target);
	}
	return observation;
}

Dictionary ParticlesExternalGuardTestFixture::observe_particle_collision_texture(RID p_collision) const {
	Dictionary observation;
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RendererRD::ParticlesStorage *particles = RendererRD::ParticlesStorage::get_singleton();
	RendererRD::TextureStorage *textures = RendererRD::TextureStorage::get_singleton();
	const bool owner = rs && rd && particles && textures && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance;
	observation["main_rd_render_thread"] = owner;
	if (!owner) {
		return observation;
	}
	const auto *collision = particles->particles_collision_owner.get_or_null(p_collision);
	observation["collision_valid"] = collision != nullptr;
	if (collision) {
		observation["field_texture"] = collision->field_texture;
		const bool is_3d = textures->texture_get_type(collision->field_texture) == RendererRD::TextureStorage::TYPE_3D;
		observation["resolved_3d_texture"] = is_3d ? textures->texture_get_rd_texture(collision->field_texture) : RID();
	}
	return observation;
}
