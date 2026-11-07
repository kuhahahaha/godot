#include "particles_scene_pairs_test_fixture.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"

namespace TestProductionRequest {
void *managed_scene_begin(bool);
Dictionary managed_scene_advance(void *);
void *managed_scene_begin_viewport(RID);
bool managed_scene_viewport_ready(void *);
void managed_scene_finish_viewport(void *);
}
void ParticlesScenePairsTestFixture::_bind_methods() {
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
void ParticlesScenePairsTestFixture::run_phase() { phase_mode = true;run(); }
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
	managed_scene = TestProductionRequest::managed_scene_begin(phase_mode);
	MutexLock lock(result_mutex);
	result_data["checks"] = checks;
}
void ParticlesScenePairsTestFixture::advance() {
	if (!managed_scene) return;
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
