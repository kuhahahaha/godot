#include "particles_scene_pairs_test_fixture.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"

void ParticlesScenePairsTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesScenePairsTestFixture::run);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesScenePairsTestFixture::result);
}
Dictionary ParticlesScenePairsTestFixture::result() const {
	MutexLock lock(result_mutex);
	return result_data.duplicate(true);
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
	const uint64_t first_key = a.is_empty() ? 0 : a[0].source_key;
	const uint64_t first_revision = a.is_empty() ? 0 : a[0].shape_revision;
	scene->instance_set_visible(source, false);
	scene->update_dirty_instances();
	checks["source_visibility"] = scene->scene_sphere_snapshot_pairs(left, 4, a) && a.is_empty();
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
	bool passed = true;
	for (const Variant &key : checks.keys()) {
		passed = passed && bool(checks[key]);
	}
	MutexLock lock(result_mutex);
	result_data["checks"] = checks;
	result_data["pass"] = passed;
}
