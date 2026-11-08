/**************************************************************************/
/*  particles_scene_pairs_test_fixture.cpp                                */
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

#include "particles_scene_pairs_test_fixture.h"

#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

namespace TestProductionRequest {
void *managed_scene_begin(bool);
Dictionary managed_scene_advance(void *);
void *managed_scene_begin_viewport(RID);
bool managed_scene_viewport_ready(void *);
void managed_scene_finish_viewport(void *);
} //namespace TestProductionRequest
void ParticlesScenePairsTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("camera_native_register", "viewport", "present_seq"), &ParticlesScenePairsTestFixture::camera_native_register);
	ClassDB::bind_method(D_METHOD("camera_native_observe", "viewport", "expected", "stage"), &ParticlesScenePairsTestFixture::camera_native_observe);
	ClassDB::bind_method(D_METHOD("camera_native_invalidate", "viewport"), &ParticlesScenePairsTestFixture::camera_native_invalidate);
	ClassDB::bind_method(D_METHOD("camera_native_finish", "viewport"), &ParticlesScenePairsTestFixture::camera_native_finish);
	ClassDB::bind_method(D_METHOD("run_viewport_invariance", "scenario"), &ParticlesScenePairsTestFixture::run_viewport_invariance);
	ClassDB::bind_method(D_METHOD("finish_viewport_observation"), &ParticlesScenePairsTestFixture::finish_viewport_observation);
	ClassDB::bind_method(D_METHOD("run"), &ParticlesScenePairsTestFixture::run);
	ClassDB::bind_method(D_METHOD("run_phase"), &ParticlesScenePairsTestFixture::run_phase);
	ClassDB::bind_method(D_METHOD("advance"), &ParticlesScenePairsTestFixture::advance);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesScenePairsTestFixture::result);
}
Dictionary ParticlesScenePairsTestFixture::result() const {
	MutexLock lock(result_mutex);
	return result_data.duplicate(true);
}
void ParticlesScenePairsTestFixture::run_viewport_invariance(RID p_scenario) {
	viewport_mode = true;
	managed_scene = TestProductionRequest::managed_scene_begin_viewport(p_scenario);
	MutexLock lock(result_mutex);
	result_data["checks"] = Dictionary();
}
void ParticlesScenePairsTestFixture::finish_viewport_observation() {
	CRASH_COND(!viewport_mode || !managed_scene);
	TestProductionRequest::managed_scene_finish_viewport(managed_scene);
}
void ParticlesScenePairsTestFixture::run_phase() {
	phase_mode = true;
	run();
}
void ParticlesScenePairsTestFixture::run() {
	auto *scene = RendererSceneCull::singleton;
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	Dictionary checks;
	if (!scene || !storage || !RenderingServer::get_singleton()->is_on_render_thread()) {
		MutexLock lock(result_mutex);
		result_data["pass"] = false;
		return;
	}
	RID scenario = scene->scenario_allocate();
	scene->scenario_initialize(scenario);
	RID particles = storage->particles_allocate();
	storage->particles_initialize(particles);
	storage->particles_set_custom_aabb(particles, AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2)));
	RID sphere = storage->particles_collision_allocate();
	storage->particles_collision_initialize(sphere);
	storage->particles_collision_set_collision_type(sphere, RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
	storage->particles_collision_set_sphere_radius(sphere, 0.5f);
	auto instance = [&](RID base, Vector3 origin) {
		RID rid = scene->instance_allocate();
		scene->instance_initialize(rid);
		scene->instance_set_base(rid, base);
		scene->instance_set_transform(rid, Transform3D(Basis(), origin));
		scene->instance_set_scenario(rid, scenario);
		return rid;
	};
	RID left = instance(particles, Vector3());
	RID right = instance(particles, Vector3(10, 0, 0));
	RID source = instance(sphere, Vector3());
	scene->update_dirty_instances();
	Vector<RendererSceneCull::SceneSphereSource> a, b;
	checks["same_base_receivers"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && scene->scene_sphere_snapshot_pairs(right, 4, b) && b.is_empty();
	// The managed receiver entry must not promote ordinary particles to an
	// ExternalOutput owner or mutate its transform/bounds on rejection.
	RendererSceneCull::SceneSphereSnapshot rejected_snapshot;
	rejected_snapshot.pose_revision = 99;
	checks["ordinary_receiver_not_promoted"] = scene->scene_receiver_commit_query(left, 1, RenderingDevice::get_singleton(),
													   Transform3D(Basis(), Vector3(50, 0, 0)), AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2)),
													   AABB(Vector3(49, -1, -1), Vector3(2, 2, 2)), 1) != OK;
	checks["ordinary_snapshot_rejected_and_empty"] = scene->scene_sphere_snapshot(left, 1, RenderingDevice::get_singleton(), 4, rejected_snapshot) != OK &&
			rejected_snapshot.pose_revision == 0 && rejected_snapshot.sources.is_empty();
	scene->update_dirty_instances();
	checks["rejected_commit_preserves_pairs"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1;
	const uint64_t first_key = a.is_empty() ? 0 : a[0].source_key;
	const uint64_t first_revision = a.is_empty() ? 0 : a[0].shape_revision;
	const auto *left_instance = scene->instance_owner.get_or_null(left);
	uint64_t membership_revision = left_instance->scene_pair_revision;
	// Same broad AABB remains paired while eligible collider kind changes.
	storage->particles_collision_set_box_extents(sphere, Vector3(0.5f, 0.5f, 0.5f));
	storage->particles_collision_set_collision_type(sphere, RSE::PARTICLES_COLLISION_TYPE_BOX_COLLIDE);
	scene->update_dirty_instances();
	checks["kept_pair_type_revision"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty() && left_instance->scene_pair_revision > membership_revision;
	membership_revision = left_instance->scene_pair_revision;
	storage->particles_collision_set_collision_type(sphere, RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE);
	scene->update_dirty_instances();
	checks["kept_pair_type_restore_revision"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && left_instance->scene_pair_revision > membership_revision;
	membership_revision = left_instance->scene_pair_revision;
	scene->instance_set_transform(source, Transform3D(Basis(), Vector3(0.125f, 0, 0)));
	scene->update_dirty_instances();
	checks["kept_pair_pose_revision"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && left_instance->scene_pair_revision > membership_revision;
	membership_revision = left_instance->scene_pair_revision;
	storage->particles_collision_set_sphere_radius(sphere, 0.625f);
	scene->update_dirty_instances();
	checks["kept_pair_shape_revision"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && left_instance->scene_pair_revision > membership_revision;
	membership_revision = left_instance->scene_pair_revision;

	scene->instance_set_visible(source, false);
	scene->update_dirty_instances();
	checks["source_visibility"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty() && left_instance->scene_pair_revision > membership_revision;
	scene->instance_set_visible(source, true);
	scene->instance_set_layer_mask(left, 2);
	storage->particles_collision_set_cull_mask(sphere, 1);
	scene->update_dirty_instances();
	checks["cull_mask"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty();
	scene->instance_set_layer_mask(left, 1);
	storage->particles_collision_set_sphere_radius(sphere, 0.75f);
	scene->instance_set_transform(source, Transform3D(Basis(), Vector3(0.25f, 0, 0)));
	scene->update_dirty_instances();
	checks["source_update"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && a[0].source_key == first_key && a[0].shape_revision > first_revision && a[0].transform.origin == Vector3(0.25f, 0, 0) && a[0].radius == 0.75f;
	scene->instance_set_visible(left, false);
	scene->update_dirty_instances();
	checks["receiver_visibility"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty();
	scene->instance_set_visible(left, true);
	scene->free(source);
	scene->update_dirty_instances();
	checks["source_release"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty();
	source = instance(sphere, Vector3());
	scene->update_dirty_instances();
	checks["source_recreate"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.size() == 1 && a[0].source_key != first_key;
	// Exercise the real heightfield pair/index path with the same public API.
	RID height_particles = storage->particles_allocate();
	storage->particles_initialize(height_particles);
	storage->particles_set_custom_aabb(height_particles, AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2)));
	RID height_collision = storage->particles_collision_allocate();
	storage->particles_collision_initialize(height_collision);
	storage->particles_collision_set_collision_type(height_collision, RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE);
	storage->particles_collision_set_box_extents(height_collision, Vector3(1, 2, 3));
	storage->particles_collision_set_height_field_resolution(height_collision, RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256);
	RID height_left = instance(height_particles, Vector3());
	RID height_right = instance(height_particles, Vector3(10, 0, 0));
	RID height_source_a = instance(height_collision, Vector3());
	RendererSceneCull::SceneHeightfieldSource hf;
	bool present = false;
	scene->update_dirty_instances();
	checks["height_one_source"] = scene->scene_heightfield_snapshot_pair(height_left, hf, present) && present && hf.extents == Vector3(1, 2, 3);
	RID height_source_b = instance(height_collision, Vector3());
	scene->update_dirty_instances();
	checks["height_two_sources_ambiguous"] = !scene->scene_heightfield_snapshot_pair(height_left, hf, present) && !present;
	scene->free(height_source_b);
	scene->instance_set_visible(height_source_a, false);
	scene->update_dirty_instances();
	checks["height_visibility"] = scene->scene_heightfield_snapshot_pair(height_left, hf, present) && !present;
	scene->instance_set_visible(height_source_a, true);
	scene->instance_set_layer_mask(height_left, 2);
	storage->particles_collision_set_cull_mask(height_collision, 1);
	scene->update_dirty_instances();
	checks["height_cull_mask"] = scene->scene_heightfield_snapshot_pair(height_left, hf, present) && !present;
	scene->instance_set_layer_mask(height_left, 1);
	storage->particles_collision_set_box_extents(height_collision, Vector3(2, 4, 6));
	scene->instance_set_transform(height_source_a, Transform3D(Basis(), Vector3(0.5f, 0, 0)));
	scene->update_dirty_instances();
	checks["height_update"] = scene->scene_heightfield_snapshot_pair(height_left, hf, present) && present && hf.transform.origin == Vector3(0.5f, 0, 0) && hf.extents == Vector3(2, 4, 6);
	scene->free(height_source_a);
	scene->free(height_right);
	scene->free(height_left);
	storage->particles_collision_free(height_collision);
	storage->particles_free(height_particles);
	scene->free(source);
	scene->free(right);
	scene->free(left);
	scene->free(scenario);
	storage->particles_collision_free(sphere);
	storage->particles_free(particles);
	if (phase_mode) {
		managed_scene = TestProductionRequest::managed_scene_begin(true);
		MutexLock lock(result_mutex);
		result_data["checks"] = checks;
		return;
	}
	bool passed = true;
	for (const Variant &key : checks.keys()) {
		passed = passed && bool(checks[key]);
	}
	MutexLock lock(result_mutex);
	result_data["checks"] = checks;
	result_data["pass"] = passed;
}
void ParticlesScenePairsTestFixture::advance() {
	if (!managed_scene) {
		return;
	}
	const Dictionary managed_checks = TestProductionRequest::managed_scene_advance(managed_scene);
	if (managed_checks.is_empty()) {
		if (viewport_mode && TestProductionRequest::managed_scene_viewport_ready(managed_scene)) {
			MutexLock lock(result_mutex);
			result_data["viewport_ready"] = true;
		}
		return;
	}
	managed_scene = nullptr;
	MutexLock lock(result_mutex);
	Dictionary checks = result_data["checks"];
	checks.merge(managed_checks);
	bool passed = true;
	for (const Variant &key : checks.keys()) {
		passed = passed && bool(checks[key]);
	}
	result_data["checks"] = checks;
	result_data["pass"] = passed;
}

// This API observes the production cache directly. In particular it must never
// call viewport_get_camera_capture_ref(), whose lazy allocation would mask a
// missing retain_native_camera_source() call in the real RD render path.
void ParticlesScenePairsTestFixture::camera_native_register(RID p_viewport, int64_t p_present_seq) {
	CRASH_COND(!RenderingServer::get_singleton()->is_on_render_thread() || !RSG::viewport);
	MutexLock lock(result_mutex);
	Dictionary checks = result_data.get("checks", Dictionary());
	RendererViewport::Viewport::ViewPresentationKey key;
	// Test-only source identity, not an external view/grant/accepted authority.
	key.view_id = 0x43414d;
	key.view_epoch = 1;
	key.present_seq = p_present_seq;
	checks["registered_" + itos(p_present_seq)] = RSG::viewport->viewport_register_presentation_key(p_viewport, key);
	camera_native_viewport = p_viewport;
	result_data["checks"] = checks;
}

void ParticlesScenePairsTestFixture::camera_native_observe(RID p_viewport, Transform3D p_expected, int64_t p_stage) {
	CRASH_COND(!RenderingServer::get_singleton()->is_on_render_thread() || !RSG::viewport);
	MutexLock lock(result_mutex);
	Dictionary checks = result_data.get("checks", Dictionary());
	const auto *viewport = RSG::viewport->viewport_owner.get_or_null(p_viewport);
	const String suffix = "_" + itos(p_stage);
	checks["real_viewport" + suffix] = viewport != nullptr;
	if (p_stage == 0) {
		checks["unregistered_cache_null"] = viewport && !viewport->presentation_registered && viewport->captured_camera.is_null() && !viewport->current_camera_token.is_valid();
	} else {
		// Check before copying any Ref: production must already have retained it.
		checks["production_cache_nonnull" + suffix] = viewport && viewport->captured_camera.is_valid();
		if (viewport && viewport->captured_camera.is_valid()) {
			const auto capture = viewport->captured_camera;
			const auto &data = capture->get_camera_data();
			const auto &token = capture->get_token();
			const auto &key = capture->get_presentation_key();
			Dictionary observation;
			observation["viewport_rid"] = int64_t(p_viewport.get_id());
			observation["payload_viewport_rid"] = int64_t(data.source_viewport.get_id());
			observation["view_id"] = int64_t(token.view_id);
			observation["view_epoch"] = int64_t(token.view_epoch);
			observation["capture_ordinal"] = int64_t(token.capture_ordinal);
			observation["present_seq"] = int64_t(key.present_seq);
			observation["capture_object_id"] = int64_t(capture->get_instance_id());
			observation["camera_transform"] = data.main_transform;
			observation["expected_transform"] = p_expected;
			result_data["observation" + suffix] = observation;
			checks["actual_viewport" + suffix] = capture->get_viewport() == p_viewport && data.source_viewport == p_viewport;
			checks["complete_token" + suffix] = token.is_valid() && token == viewport->current_camera_token && data.source_view_id == token.view_id && data.source_view_epoch == token.view_epoch && data.source_capture_ordinal == token.capture_ordinal && token.view_id == 0x43414d && token.view_epoch == 1;
			checks["presentation" + suffix] = key.view_id == token.view_id && key.view_epoch == token.view_epoch && key.present_seq == uint64_t(p_stage) && viewport->presentation_key.present_seq == uint64_t(p_stage);
			checks["camera_payload" + suffix] = data.main_transform.is_equal_approx(p_expected) && data.view_count == 1 && data.is_orthogonal && data.main_projection == viewport->captured_camera_data.main_projection;
			if (p_stage == 1) {
				camera_native_old_capture = capture;
				camera_native_old_transform = data.main_transform;
				camera_native_old_projection = data.main_projection;
				checks["old_ref_retained"] = camera_native_old_capture.is_valid() && camera_native_old_capture.ptr() == viewport->captured_camera.ptr();
			} else {
				checks["new_ref_and_token"] = camera_native_old_capture.is_valid() && camera_native_old_capture.ptr() != capture.ptr() && token != camera_native_old_capture->get_token() && token.capture_ordinal > camera_native_old_capture->get_token().capture_ordinal;
				checks["old_payload_immutable"] = camera_native_old_capture.is_valid() && camera_native_old_capture->get_camera_data().main_transform == camera_native_old_transform && camera_native_old_capture->get_camera_data().main_projection == camera_native_old_projection && !data.main_transform.is_equal_approx(camera_native_old_transform) && camera_native_old_capture->get_presentation_key().present_seq == 1;
			}
		}
	}
	result_data["checks"] = checks;
	result_data["stage"] = p_stage;
}

void ParticlesScenePairsTestFixture::camera_native_invalidate(RID p_viewport) {
	CRASH_COND(!RenderingServer::get_singleton()->is_on_render_thread() || !RSG::viewport);
	RSG::viewport->viewport_invalidate_presentation_key(p_viewport);
	MutexLock lock(result_mutex);
	Dictionary checks = result_data.get("checks", Dictionary());
	const auto *viewport = RSG::viewport->viewport_owner.get_or_null(p_viewport);
	checks["invalidated_cache_null"] = viewport && viewport->captured_camera.is_null() && !viewport->presentation_registered;
	checks["old_ref_survives_invalidate"] = camera_native_old_capture.is_valid() && camera_native_old_capture->get_camera_data().main_transform == camera_native_old_transform;
	result_data["checks"] = checks;
	result_data["stage"] = 3;
}

void ParticlesScenePairsTestFixture::camera_native_finish(RID p_viewport) {
	CRASH_COND(!RenderingServer::get_singleton()->is_on_render_thread() || !RSG::viewport);
	MutexLock lock(result_mutex);
	Dictionary checks = result_data.get("checks", Dictionary());
	checks["actual_viewport_freed"] = p_viewport == camera_native_viewport && RSG::viewport->viewport_owner.get_or_null(p_viewport) == nullptr;
	checks["old_ref_survives_free"] = camera_native_old_capture.is_valid() && camera_native_old_capture->get_viewport() == p_viewport && camera_native_old_capture->get_camera_data().source_viewport == p_viewport && camera_native_old_capture->get_camera_data().main_transform == camera_native_old_transform && camera_native_old_capture->get_camera_data().main_projection == camera_native_old_projection && camera_native_old_capture->get_token().is_valid() && camera_native_old_capture->get_presentation_key().present_seq == 1;
	camera_native_old_capture.unref();
	checks["old_ref_released"] = camera_native_old_capture.is_null();
	result_data["checks"] = checks;
	bool passed = true;
	for (const Variant &key : checks.keys()) {
		passed = passed && bool(checks[key]);
	}
	result_data["pass"] = passed;
	result_data["stage"] = 4;
}
