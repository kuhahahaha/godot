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

	using Guard = RendererRD::ParticlesExternalGuard;
	static_assert(Guard::MODE_COUNT == 5);
	const char *names[Guard::MODE_COUNT] = { "INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY" };
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
