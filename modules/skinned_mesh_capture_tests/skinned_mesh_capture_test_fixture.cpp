/**************************************************************************/
/*  skinned_mesh_capture_test_fixture.cpp                                 */
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

#include "skinned_mesh_capture_test_fixture.h"

#include "skinned_mesh_capture_test_capture_startup.h"
#include "skinned_mesh_capture_test_observer.h"
#include "skinned_mesh_capture_test_phase_observer.h"

#include "core/io/marshalls.h"
#include "core/object/callable_mp.h"
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

void SkinnedMeshCaptureTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin", "member", "route"), &SkinnedMeshCaptureTestFixture::begin);
	ClassDB::bind_method(D_METHOD("advance_action", "action_index"), &SkinnedMeshCaptureTestFixture::advance_action);
	ClassDB::bind_method(D_METHOD("submit_public_snapshot", "value"), &SkinnedMeshCaptureTestFixture::submit_public_snapshot);
	ClassDB::bind_method(D_METHOD("take_result"), &SkinnedMeshCaptureTestFixture::take_result);
	ClassDB::bind_method(D_METHOD("close", "no_future_draw"), &SkinnedMeshCaptureTestFixture::close, DEFVAL(false));
}

Error SkinnedMeshCaptureTestFixture::build_fixed_scene(const StringName &p_member) {
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

Error SkinnedMeshCaptureTestFixture::reject(Error p_error, const char *p_diagnostic) {
	last_diagnostic = p_diagnostic;
	return p_error;
}

Error SkinnedMeshCaptureTestFixture::begin(StringName p_member, StringName p_route) {
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
	// Only the implemented fixed episode may acquire. Other required members
	// stay explicit unavailable until their original action schedules are wired.
	if (p_member != "S02" || p_route != "N") {
		return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_ACQUISITION");
	}
	if (!is_inside_tree()) {
		return reject(ERR_INVALID_PARAMETER, "P01_STATE");
	}
	if (!skinned_mesh_capture_test_capture_claim_child(String(p_member).utf8().get_data(), String(p_route).utf8().get_data(), session_epoch)) {
		return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_ACQUISITION");
	}
	session = SkinnedMeshCaptureTestNativeSession::create(1, session_epoch, false, true);
	if (!session || !skinned_mesh_capture_test_phase_begin(session_epoch)) {
		if (session) {
			session->close();
		}
		return reject(ERR_UNAVAILABLE, "P01_PHASE");
	}
	skinned_mesh_capture_test_hook_begin(session_epoch);
	const Error prepared = build_fixed_scene(p_member);
	if (prepared != OK) {
		session->close();
		return reject(prepared, "P01_SOURCE");
	}
	initialization_pending = sample_pending = true;
	RenderingServer::get_singleton()->connect("frame_pre_draw", callable_mp(this, &SkinnedMeshCaptureTestFixture::frame_pre_draw));
	return OK;
}

bool SkinnedMeshCaptureTestFixture::initialization_ready(const SkinnedMeshCaptureTestSessionStatus &p_status) const {
	if (p_status.error != OK || p_status.results.empty()) {
		return false;
	}
	const auto &initial = p_status.results.front();
	if (initial.request != 1 || !initial.completed || initial.error != OK || initial.snapshot.size() != 64 || initial.consumer.size() != 64) {
		return false;
	}
	// Fixed t0 constants, independent of versions/observed source metadata.
	const float expected[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	for (int i = 0; i < 9; ++i) {
		const float value = decode_float(initial.snapshot.ptr() + 4 * i);
		if (!Math::is_finite(value) || Math::abs(value - expected[i]) > 0.00001f) {
			return false;
		}
	}
	return true;
}
bool SkinnedMeshCaptureTestFixture::action_complete(const SkinnedMeshCaptureTestSessionStatus &p_status) const {
	if (sample_pending || p_status.error != OK) {
		return false;
	}
	if (!last_action_request) {
		return true;
	}
	for (const auto &result : p_status.results) {
		if (result.request == last_action_request) {
			// Only recording gates the next main action. Readback owns its
			// independent callback lifetime and must not add episode frames.
			return result.recorded && (result.error == OK || result.error == ERR_BUSY);
		}
	}
	return false;
}
void SkinnedMeshCaptureTestFixture::frame_pre_draw() {
	if (!Thread::is_main_thread() || !session || closing || !sample_pending) {
		return;
	}
	// The original main draw signal follows the normal SceneTree deferred
	// skeleton update. Its palette setter commands precede our RS FIFO marker.
	// skeleton_updated itself is too early: palette setters follow that signal.
	auto *mesh = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("Source/Mesh")));
	auto *view = Object::cast_to<SubViewport>(ObjectDB::get_instance(scene_nodes[2]));
	if (!mesh || mesh->get_mesh().is_null() || mesh->get_skin_reference().is_null() || !view) {
		reject(ERR_INVALID_DATA, "P01_SOURCE");
		session->close();
		sample_pending = false;
		return;
	}
	SkinnedMeshCaptureTestSessionRequest early;
	early.request = next_request;
	early.scene = mesh->get_instance();
	early.mesh = mesh->get_mesh()->get_rid();
	early.skeleton = mesh->get_skin_reference()->get_skeleton();
	auto late = early;
	late.after_views = true;
	late.image = true;
	late.viewport = view->get_viewport_rid();
	std::vector<SkinnedMeshCaptureTestSessionRequest> requests;
	if (!initialization_pending && next_action != 3) {
		requests.push_back(early);
		late.request++;
	}
	requests.push_back(late);
	const Error error = session->enqueue_batch(requests);
	sample_pending = false;
	if (error != OK) {
		reject(error, "P01_CREATE_OR_COPY");
		session->close();
		return;
	}
	next_request = late.request + 1;
	if (!initialization_pending) {
		last_action_request = late.request;
	}
	initialization_pending = false;
}
Error SkinnedMeshCaptureTestFixture::advance_action(int32_t p_action_index) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (!session || closing || closed || p_action_index != next_action || p_action_index > 2) {
		return reject(ERR_INVALID_PARAMETER, "P01_STATE");
	}
	const auto state = session->status();
	if (!initialization_ready(state) || !action_complete(state)) {
		return reject(ERR_BUSY, "P01_PRECONDITION_NOT_ESTABLISHED");
	}
	if (p_action_index == 1) {
		auto *player = Object::cast_to<AnimationPlayer>(ObjectDB::get_instance(scene_nodes[1]));
		if (!player) {
			return reject(ERR_INVALID_DATA, "P01_SOURCE");
		}
		player->advance(1.0);
	}
	// a01: initialized t0; a07: actual animation advance; a13: no setter.
	++next_action;
	sample_pending = true;
	return OK;
}

Error SkinnedMeshCaptureTestFixture::submit_public_snapshot(const Dictionary &p_value) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	// No request has been admitted while acquisition is unavailable. In
	// particular, validating a Dictionary alone cannot consume a request.
	return reject(ERR_INVALID_PARAMETER, "P01_STATE");
}

Dictionary SkinnedMeshCaptureTestFixture::take_result() const {
	Dictionary result;
	if (!Thread::is_main_thread()) {
		result["last_diagnostic"] = "P01_PHASE";
		return result;
	}
	result["last_diagnostic"] = last_diagnostic;
	result["execution"] = session ? "in_progress" : "unavailable";
	if (session) {
		const auto state = session->status();
		result["initialization_ready"] = initialization_ready(state);
		result["action_ready"] = initialization_ready(state) && action_complete(state) && next_action < 3;
		result["next_action"] = next_action;
		result["native_error"] = int(state.error);
		result["cleanup_error"] = int(state.cleanup_error);
		Array captures;
		for (const auto &entry : state.results) {
			Dictionary capture;
			capture["request"] = String::num_uint64(entry.request);
			capture["frame"] = String::num_uint64(entry.frame);
			capture["publication"] = String::num_uint64(entry.publication);
			capture["received"] = entry.received;
			capture["recorded"] = entry.recorded;
			capture["completed"] = entry.completed;
			capture["canceled"] = entry.canceled;
			capture["image"] = entry.image;
			capture["image_format"] = entry.image_format;
			capture["error"] = int(entry.error);
			capture["snapshot"] = entry.snapshot;
			capture["consumer"] = entry.consumer;
			captures.push_back(capture);
		}
		result["captures"] = captures;
	}
	result["scene_prepared"] = scene_prepared;
	result["close_state"] = closed ? "closed" : "not_started";
	if (session) {
		const auto state = session->status();
		result["close_state"] = (state.pool_retired || state.no_native_work) ? "closed" : closing ? "closing"
																								  : "open";
	}
	return result;
}

Error SkinnedMeshCaptureTestFixture::close(bool p_no_future_draw) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (p_no_future_draw) {
		return reject(ERR_UNAVAILABLE, "P01_DEPENDENCY_K03");
	}
	if (closing || closed) {
		return OK;
	}
	if (session) {
		session->close();
	}
	auto *server = RenderingServer::get_singleton();
	const Callable callback = callable_mp(this, &SkinnedMeshCaptureTestFixture::frame_pre_draw);
	if (server && server->is_connected("frame_pre_draw", callback)) {
		server->disconnect("frame_pre_draw", callback);
	}
	sample_pending = false;
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

void SkinnedMeshCaptureTestFixture::close_registry() {
	if (!Thread::is_main_thread()) {
		return;
	}
	const LocalVector<ObjectID> ids(live_fixtures);
	for (ObjectID id : ids) {
		auto *fixture = Object::cast_to<SkinnedMeshCaptureTestFixture>(ObjectDB::get_instance(id));
		if (fixture) {
			fixture->close();
		}
	}
}

SkinnedMeshCaptureTestFixture::SkinnedMeshCaptureTestFixture() {
	if (Thread::is_main_thread()) {
		live_fixtures.push_back(get_instance_id());
	}
}

SkinnedMeshCaptureTestFixture::~SkinnedMeshCaptureTestFixture() {
	if (session) {
		session->close();
	}
	if (Thread::is_main_thread()) {
		live_fixtures.erase(get_instance_id());
	}
}
