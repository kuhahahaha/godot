/**************************************************************************/
/*  particles_runtime_test_fixture.cpp                                    */
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

#include "particles_runtime_test_fixture.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

#include <cstring>

using Storage = RendererRD::ParticlesStorage;

void ParticlesRuntimeTestFixture::dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	if (p_notification == Dependency::DEPENDENCY_CHANGED_PARTICLES && p_tracker && p_tracker->userdata) {
		static_cast<ParticlesRuntimeTestFixture *>(p_tracker->userdata)->dependency_notifications++;
	}
}

void ParticlesRuntimeTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin"), &ParticlesRuntimeTestFixture::begin);
	ClassDB::bind_method(D_METHOD("advance"), &ParticlesRuntimeTestFixture::advance);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesRuntimeTestFixture::result);
}
bool ParticlesRuntimeTestFixture::check(bool p_ok, const String &p_label) {
	Dictionary event;
	event["check"] = p_label;
	event["ok"] = p_ok;
	observations.push_back(event);
	if (!p_ok) {
		failures.push_back(p_label);
	}
	return p_ok;
}
bool ParticlesRuntimeTestFixture::on_owner() const {
	return rd && RenderingDevice::get_singleton() == rd && rd->get_instance_id() == device &&
			Thread::is_main_thread() && RenderingServer::get_singleton()->is_on_render_thread();
}
bool ParticlesRuntimeTestFixture::track_buffer(RID p_rid) {
	if (p_rid.is_null()) {
		return true;
	}
	for (uint32_t i = 0; i < allocation_count; i++) {
		if (allocations[i].rid == p_rid) {
			return true;
		}
	}
	const auto *buffer = rd->_get_buffer_from_owner(p_rid);
	if (!check(buffer && buffer->capture_test_allocation_serial && allocation_count < allocations.size(), "original buffer allocation identity")) {
		return false;
	}
	const uint64_t serial = rd->capture_test_allocation_registry.resolve(buffer->driver_id.id, p_rid.get_id());
	if (!check(serial == buffer->capture_test_allocation_serial, "original allocation owner authenticated")) {
		return false;
	}
	allocations[allocation_count++] = { buffer->driver_id.id, serial, p_rid };
	return true;
}
bool ParticlesRuntimeTestFixture::track_resource(RenderCaptureTestResourceKind p_kind, uint64_t p_driver) {
	if (!check(resource_count < resources.size(), "bounded native resource observation")) {
		return false;
	}
	const uint64_t token = rd->capture_test_resource_registry.track(p_kind, p_driver);
	if (!check(token != 0, "original native resource identity")) {
		return false;
	}
	resources[resource_count++] = token;
	return true;
}
bool ParticlesRuntimeTestFixture::track_set(RID p_rid) {
	if (p_rid.is_null()) {
		return true;
	}
	const auto *set = rd->uniform_set_owner.get_or_null(p_rid);
	return check(set != nullptr, "original uniform owner exists") && track_resource(RenderCaptureTestResourceKind::UniformSet, set->driver_id.id);
}
void ParticlesRuntimeTestFixture::track_particles(RID p_rid) {
	if (p_rid.is_null()) {
		return;
	}
	const auto *p = storage->particles_owner.get_or_null(p_rid);
	if (!check(p != nullptr, "original particle owner exists before close")) {
		return;
	}
	for (RID buffer : { p->particle_buffer, p->particle_instance_buffer, p->frame_params_buffer,
				 p->emission_storage_buffer, p->unused_emission_storage_buffer, p->unused_trail_storage_buffer,
				 p->trail_bind_pose_buffer, p->particles_sort_buffer }) {
		track_buffer(buffer);
	}
	for (RID set : { p->particles_material_uniform_set, p->particles_copy_uniform_set,
				 p->particles_transforms_buffer_uniform_set, p->collision_textures_uniform_set,
				 p->trail_bind_pose_uniform_set, p->particles_sort_uniform_set }) {
		track_set(set);
	}
}
bool ParticlesRuntimeTestFixture::run_upload_boundaries() {
	particle = storage->particles_allocate();
	storage->particles_initialize(particle);
	sub = storage->particles_allocate();
	storage->particles_initialize(sub);
	auto *p = storage->particles_owner.get_or_null(particle);
	auto *s = storage->particles_owner.get_or_null(sub);
	dependency_tracker.userdata = this;
	dependency_tracker.changed_callback = dependency_changed;
	dependency_tracker.update_begin();
	storage->particles_update_dependency(particle, &dependency_tracker);
	dependency_tracker.update_end();
	dependency_notifications = 0;
	storage->particles_set_subemitter(particle, sub);
	if (!check(dependency_notifications == 1, "subemitter resource replacement notifies particle dependency")) {
		return false;
	}
	p->amount = 4;
	s->amount = 4;
	storage->_particles_update_buffers(p);
	storage->_particles_allocate_emission_buffer(p);
	storage->_particles_allocate_emission_buffer(s);
	p->frame_history.resize(1);
	p->trail_params.resize(1);
	p->frame_params_buffer = rd->storage_buffer_create(sizeof(Storage::ParticlesFrameParams));
	if (!check(p->particle_buffer.is_valid() && p->frame_params_buffer.is_valid() && p->emission_buffer && s->emission_buffer, "original retry fixture allocation")) {
		return false;
	}
	std::array<Storage::ParticleData, 4> initial{};
	for (auto &entry : initial) {
		for (float &color : entry.color) {
			color = -7.0f;
		}
	}
	if (!check(rd->buffer_update(p->particle_buffer, 0, sizeof(initial), initial.data()) == OK, "particle GPU witness sentinel upload")) {
		return false;
	}
	for (RID target : { s->emission_storage_buffer, p->emission_storage_buffer, p->frame_params_buffer }) {
		p->phase = 0.25;
		p->cycle_number = 7;
		p->frame_counter = 11;
		p->clear = true;
		p->force_sub_emit = true;
		p->emitting = true;
		p->has_collision_cache = true;
		p->collision_3d_textures_used = 0;
		p->emission_buffer->particle_count = 1;
		s->emitting = true;
		s->clear = false;
		s->inactive = true;
		s->inactive_time = 4.0;
		for (int attempt = 0; attempt < 2; attempt++) {
			rd->test_arm_buffer_update_fault(target, 1);
			rd->test_reset_compute_dispatch_count();
			check(!storage->_particles_process(p, 1.0 / 60.0), "exact upload boundary rejects simulation");
			check(rd->test_get_buffer_update_fault_hits() == 1, "exact original buffer_update failure hit");
			check(rd->test_get_compute_dispatch_count() == 0, "failed upload records no simulation dispatch");
			check(p->phase == 0.25 && p->cycle_number == 7 && p->frame_counter == 11 && p->clear && p->force_sub_emit && p->emitting && p->emission_buffer->particle_count == 1 && p->has_collision_cache, "failed upload retains pending particle state");
			check(s->emitting && !s->clear && s->inactive && s->inactive_time == 4.0, "failed upload retains original subemitter state");
		}
		rd->test_arm_buffer_update_fault(RID(), 0);
		rd->test_reset_compute_dispatch_count();
		if (!check(storage->_particles_process(p, 1.0 / 60.0), "same original request succeeds after fault removal")) {
			return false;
		}
		check(rd->test_get_compute_dispatch_count() == 1, "retry records one original particle dispatch");
		check(p->frame_counter == 12 && !p->clear && !p->force_sub_emit && p->emission_buffer->particle_count == 0 && !p->has_collision_cache, "retry consumes original pending state once");
		check(!s->emitting && s->clear && !s->inactive && s->inactive_time == 0.0, "retry commits original subemitter transition");
	}
	const Error read = rd->buffer_get_data_async(p->particle_buffer, callable_mp(this, &ParticlesRuntimeTestFixture::readback).bind(0), 0, 4 * sizeof(Storage::ParticleData));
	if (check(read == OK, "production particle async readback queued")) {
		readbacks_queued++;
	}
	return read == OK;
}
bool ParticlesRuntimeTestFixture::run_sdf_history_retry() {
	sdf_particle = storage->particles_allocate();
	storage->particles_initialize(sdf_particle);
	auto *p = storage->particles_owner.get_or_null(sdf_particle);
	p->amount = 4;
	p->trails_enabled = true;
	p->trail_bind_poses.resize(3);
	p->emitting = true;
	p->use_local_coords = true;
	p->has_sdf_collision = true;
	p->sdf_collision_texture = RendererRD::TextureStorage::get_singleton()->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
	storage->_particles_update_buffers(p);
	p->frame_history.resize(4);
	p->trail_params.resize(3);
	p->frame_params_buffer = rd->storage_buffer_create(3 * sizeof(Storage::ParticlesFrameParams));
	if (!check(p->particle_buffer.is_valid() && p->particle_instance_buffer.is_valid() && p->frame_params_buffer.is_valid() && rd->texture_is_valid(p->sdf_collision_texture), "SDF extension native buffers and borrowed texture exist")) {
		return false;
	}
	for (auto &params : p->frame_history) {
		params = Storage::ParticlesFrameParams();
	}
	// Establish a real descriptor tuple first. Failed attempts below retain it,
	// so no transient descriptor can escape the physical-return observations.
	if (!check(storage->_particles_process(p, 1.0 / 60.0), "SDF extension primes actual native trail producer")) {
		return false;
	}
	track_set(p->collision_textures_uniform_set);
	p->frame_counter = 0;
	p->phase = 0;
	p->clear = true;
	for (uint32_t i = 0; i < p->frame_history.size(); i++) {
		p->frame_history[i] = Storage::ParticlesFrameParams();
		p->frame_history[i].frame = 10 + i;
		p->frame_history[i].collider_count = 1;
		p->frame_history[i].colliders[0].type = Storage::ParticlesFrameParams::COLLISION_TYPE_2D_SDF;
		p->frame_history[i].colliders[0].transform[3] = 100 + i;
	}
	for (int attempt = 0; attempt < 2; attempt++) {
		p->sdf_collision_transform.set_origin(Vector2(200 + attempt, 300));
		rd->test_arm_buffer_update_fault(p->frame_params_buffer, 1);
		rd->test_reset_compute_dispatch_count();
		check(!storage->_particles_process(p, 1.0 / 60.0), "SDF extension rejects exact native frame upload");
		check(rd->test_get_buffer_update_fault_hits() == 1, "SDF extension exact upload fault observed");
		check(rd->test_get_compute_dispatch_count() == 0, "SDF extension rejected frame records no dispatch");
		check(p->frame_counter == 0 && p->phase == 0 && p->clear, "SDF extension rejected frame retains pending state");
		for (uint32_t i = 0; i < p->frame_history.size(); i++) {
			check(p->frame_history[i].frame == 10 + i && p->frame_history[i].colliders[0].transform[3] == 100 + i, "SDF extension rejected frame restores original collider history");
		}
		for (uint32_t i = 0; i < p->trail_params.size(); i++) {
			check(p->trail_params[i].frame == 10 + i && p->trail_params[i].colliders[0].transform[3] == 100 + i, "SDF extension rejected frame restores original trail samples");
		}
	}
	rd->test_arm_buffer_update_fault(RID(), 0);
	p->has_sdf_collision = false;
	std::array<Storage::ParticleData, 12> sentinel{};
	for (auto &entry : sentinel) {
		for (float &color : entry.color) {
			color = -7.0f;
		}
	}
	if (!check(rd->buffer_update(p->particle_buffer, 0, sizeof(sentinel), sentinel.data()) == OK, "SDF extension recovery GPU sentinel uploaded")) {
		return false;
	}
	rd->test_reset_compute_dispatch_count();
	if (!check(storage->_particles_process(p, 1.0 / 60.0), "SDF extension recovers after canvas SDF removal")) {
		return false;
	}
	check(rd->test_get_compute_dispatch_count() == 2 && p->frame_counter == 1 && !p->clear, "SDF extension recovery records two native trail passes once");
	check(p->frame_history[0].collider_count == 0, "SDF extension recovered current frame has no SDF collider");
	for (uint32_t i = 1; i < p->frame_history.size(); i++) {
		check(p->frame_history[i].frame == 9 + i && p->frame_history[i].colliders[0].transform[3] == 99 + i, "SDF extension recovery retains unmodified historical collider");
	}
	const Error params_read = rd->buffer_get_data_async(p->frame_params_buffer, callable_mp(this, &ParticlesRuntimeTestFixture::readback).bind(6), 0, 3 * sizeof(Storage::ParticlesFrameParams));
	if (check(params_read == OK, "SDF extension trail parameter async readback queued")) {
		sdf_readbacks_queued++;
	}
	const Error particle_read = rd->buffer_get_data_async(p->particle_buffer, callable_mp(this, &ParticlesRuntimeTestFixture::readback).bind(7), 0, 12 * sizeof(Storage::ParticleData));
	if (check(particle_read == OK, "SDF extension particle async readback queued")) {
		sdf_readbacks_queued++;
	}
	return params_read == OK && particle_read == OK;
}
bool ParticlesRuntimeTestFixture::run_mobile_instance_recovery() {
	using Mobile = RendererSceneRenderImplementation::RenderForwardMobile;
	auto *mobile = Mobile::get_singleton();
	auto *p = storage->particles_owner.get_or_null(sdf_particle);
	if (!check(mobile && p && p->frame_counter > 0, "Mobile recovery actual renderer and processed particle exist")) {
		return false;
	}
	RenderGeometryInstance *base = mobile->geometry_instance_create(sdf_particle);
	if (!check(base != nullptr, "Mobile recovery creates actual geometry instance")) {
		return false;
	}
	// geometry_instance_create pushes its new instance at the front. Deduce the
	// existing private nested type without adding another test access interface.
	auto *geometry = mobile->geometry_instance_dirty_list.first()->self();
	if (!check(geometry == base, "Mobile recovery owns newly queued geometry")) {
		mobile->geometry_instance_free(base);
		return false;
	}
	// Match the culler initialization that enrolls the original dependencies.
	base->set_material_override(RID());
	mobile->_geometry_instance_update(base);
	const RID simulation = p->particle_buffer;
	const RID old_instances = p->particle_instance_buffer;
	const RID old_copy = p->particles_copy_uniform_set;
	const RID old_transforms = geometry->transforms_uniform_set;
	check(rd->uniform_set_is_valid(old_transforms) && !geometry->dirty_list_element.in_list(), "Mobile recovery initial geometry caches valid transforms");
	const auto *old_dependencies = rd->reverse_dependency_map.getptr(old_transforms);
	check(old_dependencies && old_dependencies->has(old_instances), "Mobile recovery initial descriptor references original instances");
	// Observe every resource before the production rebuild can invalidate it.
	track_buffer(old_instances);
	track_set(old_copy);
	track_set(old_transforms);
	// Enter the existing missing-copy-resource recovery branch. No generic
	// failure hook, replacement buffer, or synthetic dependency notification.
	rd->free_rid(old_copy);
	storage->_particles_update_buffers(p);
	check(!rd->uniform_set_is_valid(old_transforms), "Mobile recovery retires stale cached transform descriptor");
	check(p->particle_instance_buffer.is_valid() && p->particle_instance_buffer != old_instances && rd->uniform_set_is_valid(p->particles_copy_uniform_set), "Mobile recovery rebuilds original instance resources");
	check(p->particle_buffer == simulation, "Mobile recovery retains original simulation buffer");
	check(geometry->dirty_list_element.in_list(), "Mobile recovery notification queues existing geometry");
	// Follow the renderer's dirty-list gate. An ignored notification must not
	// be hidden by calling the geometry updater unconditionally in the test.
	if (geometry->dirty_list_element.in_list()) {
		mobile->_geometry_instance_update(base);
	}
	const RID recovered = geometry->transforms_uniform_set;
	check(recovered != old_transforms && rd->uniform_set_is_valid(recovered), "Mobile recovery refreshes cached native descriptor");
	const auto *new_dependencies = rd->reverse_dependency_map.getptr(recovered);
	check(new_dependencies && new_dependencies->has(p->particle_instance_buffer), "Mobile recovery descriptor references rebuilt instances");
	storage->_particles_update_buffers(p);
	check(!geometry->dirty_list_element.in_list() && geometry->transforms_uniform_set == recovered && p->particle_buffer == simulation, "Mobile recovery ordinary unchanged update sends no repeated invalidation");
	mobile->geometry_instance_free(base);
	return true;
}
bool ParticlesRuntimeTestFixture::run_progress(int p_kind) {
	RID &rid = progress[p_kind];
	rid = storage->particles_allocate();
	storage->particles_initialize(rid);
	auto *p = storage->particles_owner.get_or_null(rid);
	p->amount = 4;
	p->emitting = true;
	p->inactive = false;
	p->fixed_fps = 64;
	storage->_particles_update_buffers(p);
	p->frame_history.resize(1);
	p->trail_params.resize(1);
	p->frame_params_buffer = rd->storage_buffer_create(sizeof(Storage::ParticlesFrameParams));
	if (!check(p->particle_buffer.is_valid() && p->frame_params_buffer.is_valid(), "progress fixture original buffers")) {
		return false;
	}
	constexpr float step = 1.0f / 64.0f;
	p->speed_scale = p_kind == 3 ? 1.0 : 0.0;
	p->restart_request = false;
	p->clear = true;
	if (p_kind == 0) {
		p->pre_process_time = 3 * step;
	}
	if (p_kind == 1) {
		p->request_process_time = 3 * step;
	}
	if (p_kind == 2) {
		p->request_process_time_residual = 3 * step;
	}
	if (p_kind == 3) {
		p->frame_remainder = 3.5f * step;
	}
	float total = p_kind == 3 ? 3.5f * step : 3 * step;
	if (p_kind == 3) {
		const float delta = CLAMP(float(RendererCompositorRD::get_singleton()->get_frame_delta_time()), 0.0f, 0.1f);
		total += delta;
	}
	const uint32_t expected_steps = uint32_t(Math::floor(total / step));
	const float expected_remainder = total - expected_steps * step;
	if (p_kind == 3) {
		check(expected_remainder > 0.0f && expected_remainder < step, "fixed-step witness includes a nonzero fractional suffix");
	}
	rd->test_arm_buffer_update_fault_after(p->frame_params_buffer, 1, 1);
	storage->particles_request_process(rid);
	storage->update_particles();
	check(rd->test_get_buffer_update_fault_hits() == 1 && p->frame_counter == 1, "progress failure follows one successful original step");
	check(p->retry_pending && p->dirty && p->update_list.in_list(), "failed original work remains queued");
	const real_t remaining = p_kind == 0 ? p->pre_process_time_remaining : p_kind == 1 ? p->request_process_time
			: p_kind == 2															   ? p->request_process_time_residual
																					   : p->frame_remainder;
	check(remaining == total - step, "only successful prefix decrements original retained progress");
	if (p_kind == 2) {
		check(p->emitting, "failed residual suffix restores original emitting state");
	}
	check(p->speed_scale == (p_kind == 3 ? 1.0 : 0.0), "failed progress restores original speed scale");
	if (p_kind == 0) {
		check(p->pre_process_pending, "preprocess ownership survives partial failure");
	}
	if (p_kind == 3) {
		check(p->frame_delta_committed && p->frame_delta_committed_frame == RSG::rasterizer->get_frame_number(), "frame delta committed to original frame");
	}
	rd->test_arm_buffer_update_fault(RID(), 0);
	storage->update_particles(); // Same real rendering frame; do not charge its delta twice.
	check(!p->retry_pending && !p->dirty && !p->update_list.in_list(), "original retry queue drains after recovery");
	check(p->frame_counter == expected_steps, "retry consumes retained suffix without replay or duplicate frame delta");
	if (p_kind == 2) {
		check(!p->emitting, "successful residual suffix closes emission");
	}
	if (p_kind == 3) {
		check(p->frame_remainder == expected_remainder, "retry preserves exact nonzero fractional frame remainder");
	}
	check(p->speed_scale == (p_kind == 3 ? 1.0 : 0.0), "successful progress restores original speed scale");
	check(!p->pre_process_pending && p->request_process_time == 0.0 && p->request_process_time_residual == 0.0, "manual progress closes only after original success");
	const Error read = rd->buffer_get_data_async(p->frame_params_buffer, callable_mp(this, &ParticlesRuntimeTestFixture::readback).bind(2 + p_kind), 0, sizeof(Storage::ParticlesFrameParams));
	if (check(read == OK, "retained progress async readback queued")) {
		readbacks_queued++;
	}
	return read == OK;
}
bool ParticlesRuntimeTestFixture::run_trail() {
	RenderingDevice::ShaderStageSPIRVData stage;
	stage.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
	String error;
	stage.spirv = rd->shader_compile_spirv_from_source(stage.shader_stage, "#version 450\nlayout(local_size_x=1) in; layout(set=2,binding=0,std430) buffer Trail { mat4 pose; }; void main(){pose[0][0]=17.25;}", RenderingDevice::SHADER_LANGUAGE_GLSL, &error);
	if (!check(!stage.spirv.is_empty() && error.is_empty(), "trail compute shader compiles on actual main RD")) {
		return false;
	}
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	trail_shader = rd->shader_create_from_spirv(stages, "ParticlesRuntimeTrailRetry");
	if (!check(trail_shader.is_valid(), "trail native shader exists")) {
		return false;
	}
	track_resource(RenderCaptureTestResourceKind::Shader, rd->shader_owner.get_or_null(trail_shader)->driver_id.id);
	Storage::Particles p;
	p.test_reject_trail_allocation_once = true;
	check(!Storage::_particles_ensure_trail_uniform(&p, trail_shader), "original trail fallback allocation failure");
	check(!p.test_reject_trail_allocation_once && p.unused_trail_storage_buffer.is_null() && p.trail_bind_pose_uniform_set.is_null() && p.test_trail_uniform_attempts == 0, "trail failure stops before uniform creation");
	const bool created = Storage::_particles_ensure_trail_uniform(&p, trail_shader);
	trail_buffer = p.unused_trail_storage_buffer;
	trail_set = p.trail_bind_pose_uniform_set;
	if (!check(created && rd->uniform_set_is_valid(trail_set) && trail_buffer.is_valid(), "original trail retry creates real Metal resources")) {
		return false;
	}
	check(Storage::_particles_ensure_trail_uniform(&p, trail_shader) && p.test_trail_uniform_attempts == 1 && trail_buffer == p.unused_trail_storage_buffer && trail_set == p.trail_bind_pose_uniform_set, "trail subsequent use retains original buffer and uniform");
	track_buffer(trail_buffer);
	track_set(trail_set);
	trail_pipeline = rd->compute_pipeline_create(trail_shader);
	if (!check(trail_pipeline.is_valid(), "trail native compute pipeline exists")) {
		return false;
	}
	track_resource(RenderCaptureTestResourceKind::ComputePipeline, rd->compute_pipeline_owner.get_or_null(trail_pipeline)->driver_id.id);
	const auto list = rd->compute_list_begin();
	if (!check(list != RenderingDevice::INVALID_ID, "trail original compute list opens")) {
		return false;
	}
	rd->compute_list_bind_compute_pipeline(list, trail_pipeline);
	rd->compute_list_bind_uniform_set(list, trail_set, 2);
	rd->compute_list_dispatch(list, 1, 1, 1);
	rd->compute_list_end();
	const Error read = rd->buffer_get_data_async(trail_buffer, callable_mp(this, &ParticlesRuntimeTestFixture::readback).bind(1), 0, 4);
	if (check(read == OK, "trail async readback queued")) {
		readbacks_queued++;
	}
	return read == OK;
}
void ParticlesRuntimeTestFixture::readback(const PackedByteArray &p_bytes, int p_kind) {
	if (p_kind == 6 || p_kind == 7) {
		check(on_owner(), "SDF extension async completion belongs to original main RD");
		sdf_readbacks_completed++;
		if (p_kind == 6) {
			std::array<Storage::ParticlesFrameParams, 3> data{};
			if (check(p_bytes.size() == sizeof(data), "SDF extension trail parameter readback exact bytes")) {
				std::memcpy(data.data(), p_bytes.ptr(), sizeof(data));
				check(data[0].collider_count == 0 && data[1].frame == 10 && data[1].colliders[0].transform[3] == 100 && data[2].frame == 11 && data[2].colliders[0].transform[3] == 101, "SDF extension original trail history reaches actual device buffer");
			}
		} else {
			std::array<Storage::ParticleData, 12> data{};
			if (check(p_bytes.size() == sizeof(data), "SDF extension particle readback exact bytes")) {
				std::memcpy(data.data(), p_bytes.ptr(), sizeof(data));
				bool active_shader_output = false;
				for (const auto &entry : data) {
					active_shader_output = active_shader_output || ((entry.active & 1u) && entry.color[0] == 1.0f && entry.color[1] == 1.0f && entry.color[2] == 1.0f && entry.color[3] == 1.0f);
				}
				check(active_shader_output, "SDF extension actual native particle shader produces active output");
			}
		}
		return;
	}
	check(on_owner(), "async completion belongs to original main RD");
	readbacks_completed++;
	if (p_kind == 0) {
		std::array<Storage::ParticleData, 4> data{};
		if (check(p_bytes.size() == sizeof(data), "production particle readback exact bytes")) {
			std::memcpy(data.data(), p_bytes.ptr(), sizeof(data));
			bool active_shader_output = false;
			for (const auto &entry : data) {
				active_shader_output = active_shader_output || ((entry.active & 1u) && entry.color[0] == 1.0f && entry.color[1] == 1.0f && entry.color[2] == 1.0f && entry.color[3] == 1.0f);
			}
			check(active_shader_output, "actual production particle shader replaces sentinel on emitted particle");
		}
	} else if (p_kind == 1) {
		float value = 0;
		if (check(p_bytes.size() == sizeof(value), "trail readback exact bytes")) {
			std::memcpy(&value, p_bytes.ptr(), sizeof(value));
			check(value == 17.25f, "actual trail shader consumed original retried fallback");
		}
	} else {
		Storage::ParticlesFrameParams data{};
		if (check(p_bytes.size() == sizeof(data), "progress upload readback exact bytes")) {
			std::memcpy(&data, p_bytes.ptr(), sizeof(data));
			const auto *p = storage->particles_owner.get_or_null(progress[p_kind - 2]);
			check(p && data.frame == p->frame_counter - 1, "original retained final frame reaches actual device buffer");
		}
	}
}
Error ParticlesRuntimeTestFixture::begin() {
	if (started) {
		return ERR_ALREADY_IN_USE;
	}
	started = true;
	rd = RenderingDevice::get_singleton();
	device = rd ? rd->get_instance_id() : ObjectID();
	if (!rd || !on_owner() || !rd->is_main_instance || OS::get_singleton()->get_current_rendering_driver_name() != "metal" || !RendererCompositorRD::get_singleton()) {
		rd = nullptr;
		return ERR_UNAVAILABLE;
	}
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
		rd = nullptr;
		return ERR_BUSY;
	}
	storage = static_cast<Storage *>(RendererCompositorRD::get_singleton()->get_particles_storage());
	check(!rd->capture_test_coverage_failed, "shared native observation coverage intact at begin");
	bool success = run_upload_boundaries();
	for (int i = 0; i < 4; i++) {
		success = run_progress(i) && success;
	}
	success = run_trail() && success;
	rd->test_arm_buffer_update_fault(RID(), 0);
	check(success && readbacks_queued == 6, "all fixed actual Metal workloads accepted");
	check(run_sdf_history_retry() && sdf_readbacks_queued == 2, "SDF extension two actual Metal readbacks accepted");
	check(run_mobile_instance_recovery(), "Mobile recovery extension completed actual native path");
	return OK;
}
void ParticlesRuntimeTestFixture::release_resources() {
	rd->test_arm_buffer_update_fault(RID(), 0);
	for (RID rid : { particle, sub, progress[0], progress[1], progress[2], progress[3], sdf_particle }) {
		track_particles(rid);
	}
	for (RID rid : { particle, sub, progress[0], progress[1], progress[2], progress[3], sdf_particle }) {
		if (rid.is_valid()) {
			storage->particles_free(rid);
		}
	}
	particle = RID();
	sub = RID();
	sdf_particle = RID();
	for (RID &rid : progress) {
		rid = RID();
	}
	for (RID rid : { trail_set, trail_pipeline, trail_shader, trail_buffer }) {
		if (rid.is_valid()) {
			rd->free_rid(rid);
		}
	}
	trail_set = RID();
	trail_pipeline = RID();
	trail_shader = RID();
	trail_buffer = RID();
	released = true;
	bool pending = false;
	for (uint32_t i = 0; i < allocation_count; i++) {
		check(rd->_get_buffer_from_owner(allocations[i].rid) == nullptr, "owner release removes original buffer RID");
		pending = rd->capture_test_allocation_registry.physically_live(allocations[i].driver, allocations[i].serial) || pending;
	}
	check(pending && allocation_count > 0 && resource_count > 0, "owner release is not physical driver retirement");
}
Error ParticlesRuntimeTestFixture::advance() {
	if (!on_owner()) {
		return ERR_UNAVAILABLE;
	}
	if (retired) {
		return OK;
	}
	if (!released) {
		if (readbacks_completed != readbacks_queued || sdf_readbacks_completed != sdf_readbacks_queued) {
			return ERR_BUSY;
		}
		release_resources();
		return ERR_BUSY;
	}
	if (rd->capture_test_coverage_failed) {
		check(false, "shared native tracking coverage lost");
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < allocation_count; i++) {
		if (rd->capture_test_allocation_registry.physically_live(allocations[i].driver, allocations[i].serial)) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < resource_count; i++) {
		const auto status = rd->capture_test_resource_registry.status(resources[i]);
		if (status == RenderCaptureTestResourceRegistry::Status::Unknown) {
			check(false, "native retirement identity lost");
			return ERR_INVALID_DATA;
		}
		if (status == RenderCaptureTestResourceRegistry::Status::Live) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < resource_count; i++) {
		check(rd->capture_test_resource_registry.acknowledge(resources[i]), "original native physical return acknowledged");
	}
	retired = true;
	check(readbacks_completed == 6, "all six original async readbacks physically completed");
	check(sdf_readbacks_completed == 2, "SDF extension both async readbacks physically completed");
	return failures.is_empty() ? OK : FAILED;
}
Dictionary ParticlesRuntimeTestFixture::result() const {
	Dictionary d;
	d["done"] = retired;
	d["released"] = released;
	d["failures"] = failures;
	d["checks"] = observations;
	d["readbacks_queued"] = readbacks_queued;
	d["readbacks_completed"] = readbacks_completed;
	d["sdf_readbacks_queued"] = sdf_readbacks_queued;
	d["sdf_readbacks_completed"] = sdf_readbacks_completed;
	d["allocation_count"] = allocation_count;
	d["native_resource_count"] = resource_count;
	d["driver"] = OS::get_singleton()->get_current_rendering_driver_name();
	d["renderer"] = OS::get_singleton()->get_current_rendering_method();
	d["device_identity"] = uint64_t(device);
	d["device_name"] = rd ? rd->get_device_name() : String();
	d["device_vendor"] = rd ? rd->get_device_vendor_name() : String();
	d["device_api"] = rd ? rd->get_device_api_version() : String();
	d["frame_delay"] = rd ? rd->get_frame_delay() : 0;
	return d;
}
ParticlesRuntimeTestFixture::~ParticlesRuntimeTestFixture() {
	// The runner retains the fixture until every callback and physical return.
	// Failed setup still releases original owners on their permitted thread.
	if (on_owner() && !released && readbacks_queued == readbacks_completed) {
		release_resources();
	}
}
