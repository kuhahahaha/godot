#include "p01_probe_fixture.h"

#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/main/viewport.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_server.h"
#if !defined(DISABLE_DEPRECATED) && !defined(PHYSICS_3D_DISABLED)
#include "scene/3d/physics/physical_bone_simulator_3d.h"
#endif

static LocalVector<ObjectID> live_fixtures;

void P01ProbeFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin", "member", "route"), &P01ProbeFixture::begin);
	ClassDB::bind_method(D_METHOD("advance_action", "action_index"), &P01ProbeFixture::advance_action);
	ClassDB::bind_method(D_METHOD("submit_public_snapshot", "value"), &P01ProbeFixture::submit_public_snapshot);
	ClassDB::bind_method(D_METHOD("take_result"), &P01ProbeFixture::take_result);
	ClassDB::bind_method(D_METHOD("close", "no_future_draw"), &P01ProbeFixture::close, DEFVAL(false));
}

Error P01ProbeFixture::build_fixed_scene(const StringName &p_member) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (scene_prepared || closed) {
		return ERR_ALREADY_IN_USE;
	}
	if (!is_inside_tree()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_member != "S02") {
		return ERR_UNAVAILABLE;
	}
#if defined(DISABLE_DEPRECATED) || defined(PHYSICS_3D_DISABLED)
	return ERR_UNAVAILABLE;
#else
	auto *source = memnew(Node3D);
	source->set_name("Source");
	auto *skeleton = memnew(Skeleton3D);
	skeleton->set_name("Skeleton");
	skeleton->set_animate_physical_bones(false);
	skeleton->add_bone("root");
	skeleton->set_bone_rest(0, Transform3D());
	skeleton->add_bone("tip");
	skeleton->set_bone_parent(1, 0);
	skeleton->set_bone_rest(1, Transform3D(Basis(), Vector3(0, 2, 0)));
	skeleton->reset_bone_poses();
	source->add_child(skeleton);
	Ref<Skin> skin;
	skin.instantiate();
	skin->set_bind_count(2);
	skin->set_bind_name(0, "tip");
	skin->set_bind_bone(0, 0); // Original input; name resolves tip to bone 1.
	skin->set_bind_pose(0, Transform3D(Basis(), Vector3(0, -2, 0)));
	skin->set_bind_bone(1, 0);
	skin->set_bind_pose(1, Transform3D());
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = PackedVector3Array{ Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0) };
	arrays[Mesh::ARRAY_BONES] = PackedInt32Array{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	arrays[Mesh::ARRAY_WEIGHTS] = PackedFloat32Array{ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	if (mesh->get_surface_count() != 1) {
		memdelete(source);
		return ERR_CANT_CREATE;
	}
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_albedo(Color(1, 1, 1, 1));
	mesh->surface_set_material(0, material);
	auto *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name("Mesh");
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_skin(skin);
	mesh_instance->set_skeleton_path(NodePath("../Skeleton"));
	mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	source->add_child(mesh_instance);
	add_child(source);
	skeleton->physical_bones_stop_simulation();
	skeleton->set_animate_physical_bones(false);
	int simulator_count = 0;
	bool simulator_valid = !skeleton->get_animate_physical_bones();
	for (int i = 0; i < skeleton->get_child_count(true); i++) {
		auto *simulator = Object::cast_to<PhysicalBoneSimulator3D>(skeleton->get_child(i, true));
		if (simulator) {
			simulator_count++;
			simulator_valid &= simulator->is_internal() && !simulator->is_active() && !simulator->is_simulating_physics();
		}
	}
	if (!simulator_valid || simulator_count != 1 || mesh_instance->get_skin_reference().is_null()) {
		memdelete(source);
		return ERR_INVALID_DATA;
	}
	auto *player = memnew(AnimationPlayer);
	player->set_name("AnimationPlayer");
	player->set_root_node(NodePath(".."));
	player->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	Ref<Animation> animation;
	animation.instantiate();
	animation->set_length(3.0);
	animation->set_loop_mode(Animation::LOOP_NONE);
	const int track = animation->add_track(Animation::TYPE_POSITION_3D);
	animation->track_set_path(track, NodePath("Source/Skeleton:tip"));
	animation->track_set_interpolation_type(track, Animation::INTERPOLATION_LINEAR);
	for (int i = 0; i < 4; i++) {
		animation->position_track_insert_key(track, i, Vector3(i, 2, 0));
	}
	Ref<AnimationLibrary> library;
	library.instantiate();
	library->add_animation("p01", animation);
	player->add_animation_library(StringName(), library);
	add_child(player);
	player->play("p01");
	player->seek(0, true);
	auto *view = memnew(SubViewport);
	view->set_name("ViewA");
	view->set_size(Size2i(128, 64));
	view->set_world_3d(get_viewport()->find_world_3d());
	view->set_msaa_3d(Viewport::MSAA_DISABLED);
	view->set_screen_space_aa(Viewport::SCREEN_SPACE_AA_DISABLED);
	view->set_use_taa(false);
	view->set_update_mode(SubViewport::UPDATE_ALWAYS);
	auto *camera = memnew(Camera3D);
	camera->set_name("Camera");
	camera->set_position(Vector3(2, 0, 10));
	camera->set_orthogonal(4, 0.1, 20);
	camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);
	Ref<Environment> environment;
	environment.instantiate();
	environment->set_background(Environment::BG_COLOR);
	environment->set_bg_color(Color(0, 0, 0, 1));
	environment->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
	camera->set_environment(environment);
	view->add_child(camera);
	add_child(view);
	camera->make_current();
	scene_nodes[0] = source->get_instance_id();
	scene_nodes[1] = player->get_instance_id();
	scene_nodes[2] = view->get_instance_id();
	scene_prepared = true;
	return OK;
#endif
}

Error P01ProbeFixture::reject(Error p_error, const char *p_diagnostic) {
	last_diagnostic = p_diagnostic;
	return p_error;
}

Error P01ProbeFixture::begin(StringName p_member, StringName p_route) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (closed || scene_prepared) {
		return reject(ERR_ALREADY_IN_USE, "P01_STATE");
	}
	bool known_member = false;
	for (const char *name : { "S01", "S02", "S03", "S04", "S05", "S06", "S07", "S08", "S09", "S10" }) {
		known_member |= p_member == name;
	}
	if (!known_member || (p_route != "P" && p_route != "N" && p_route != "C") || (p_member == "S07" && p_route == "P")) {
		return reject(ERR_INVALID_PARAMETER, "P01_STATE");
	}
	if (!RenderingServer::get_singleton()->get_rendering_device()) {
		return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_RD");
	}
	// This cannot be replaced by a script boolean or ordinary result JSON.
	// The inherited startup authority and real engine collector must be wired
	// before begin constructs a native acquisition session or records GPU work.
	return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_ACQUISITION");
}

Error P01ProbeFixture::advance_action(int32_t p_action_index) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	return reject(ERR_INVALID_PARAMETER, "P01_STATE");
}

Error P01ProbeFixture::submit_public_snapshot(const Dictionary &p_value) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	// No request has been admitted while acquisition is unavailable. In
	// particular, validating a Dictionary alone cannot consume a request.
	return reject(ERR_INVALID_PARAMETER, "P01_STATE");
}

Dictionary P01ProbeFixture::take_result() const {
	Dictionary result;
	if (!Thread::is_main_thread()) {
		result["last_diagnostic"] = "P01_PHASE";
		return result;
	}
	result["last_diagnostic"] = last_diagnostic;
	result["execution"] = "unavailable";
	result["scene_prepared"] = scene_prepared;
	result["close_state"] = closed ? "closed" : "not_started";
	return result;
}

Error P01ProbeFixture::close(bool p_no_future_draw) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (p_no_future_draw) {
		return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_K03");
	}
	if (closing || closed) {
		return OK;
	}
	// No session or GPU work is admitted yet. Closing an empty fixture has no
	// callback or GPU lifetime to drain. Scene-only test cleanup is separate.
	closing = true;
	closed = true;
	scene_prepared = false;
	const ObjectID fixture_id = get_instance_id();
	ObjectID owned_nodes[4];
	for (int i = 0; i < 4; i++) {
		owned_nodes[i] = scene_nodes[i];
		scene_nodes[i] = ObjectID();
	}
	for (ObjectID id : owned_nodes) {
		// An earlier tree-exit callback may have destroyed the fixture or
		// reparented another child. No member access is permitted after delete.
		if (!ObjectDB::get_instance(fixture_id)) {
			return OK;
		}
		Node *node = Object::cast_to<Node>(ObjectDB::get_instance(id));
		if (node && node->get_parent() && node->get_parent()->get_instance_id() == fixture_id) {
			memdelete(node);
		}
	}
	return OK;
}

void P01ProbeFixture::close_registry() {
	if (!Thread::is_main_thread()) {
		return;
	}
	const LocalVector<ObjectID> ids(live_fixtures);
	for (ObjectID id : ids) {
		auto *fixture = Object::cast_to<P01ProbeFixture>(ObjectDB::get_instance(id));
		if (fixture) {
			fixture->close();
		}
	}
}

P01ProbeFixture::P01ProbeFixture() {
	if (Thread::is_main_thread()) {
		live_fixtures.push_back(get_instance_id());
	}
}

P01ProbeFixture::~P01ProbeFixture() {
	if (Thread::is_main_thread()) {
		live_fixtures.erase(get_instance_id());
	}
}
