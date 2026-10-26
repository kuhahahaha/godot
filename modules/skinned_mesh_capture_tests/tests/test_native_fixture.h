/**************************************************************************/
/*  test_native_fixture.h                                                 */
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

#pragma once

#include "core/object/message_queue.h"

#include <thread>
#if !defined(DISABLE_DEPRECATED) && !defined(PHYSICS_3D_DISABLED)
#include "scene/3d/physics/physical_bone_simulator_3d.h"
#endif
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_capture.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_fixture.h"

// C++-only scene construction access. It neither arms native acquisition nor
// creates a script-facing preflight bypass.
struct SkinnedMeshCaptureTestFixtureTestAccess {
	static void check_recording_gate(SkinnedMeshCaptureTestFixture *p_fixture) {
		p_fixture->last_action_request = 5;
		SkinnedMeshCaptureTestSessionStatus status;
		SkinnedMeshCaptureTestSessionResult result;
		result.request = 5;
		result.recorded = true;
		result.error = ERR_BUSY;
		status.results.push_back(result);
		CHECK(p_fixture->action_complete(status));
		status.results[0].recorded = false;
		CHECK_FALSE(p_fixture->action_complete(status));
		status.results[0].recorded = true;
		status.results[0].error = ERR_INVALID_DATA;
		CHECK_FALSE(p_fixture->action_complete(status));
		status.results[0].error = ERR_BUSY;
		p_fixture->sample_pending = true;
		CHECK_FALSE(p_fixture->action_complete(status));
		p_fixture->sample_pending = false;
		p_fixture->last_action_request = 0;
	}

	static Error prepare(SkinnedMeshCaptureTestFixture *p_fixture, const StringName &p_member) { return p_fixture->build_fixed_scene(p_member); }
};

namespace TestSkinnedMeshCaptureTestNativeFixture {
class CloseListener : public Object {
	GDCLASS(CloseListener, Object);
	static void _bind_methods() {}

public:
	SkinnedMeshCaptureTestFixture *fixture = nullptr;
	bool published_before_callback = false;
	int callbacks = 0;
	void on_exit() {
		callbacks++;
		const Dictionary state = fixture->take_result();
		published_before_callback = String(state["close_state"]) == "closed" && !bool(state["scene_prepared"]);
		// Check the missing precondition safely on the old implementation;
		// only re-enter once the state is committed, rather than inducing a
		// double-delete crash and counting that as a valid RED.
		if (published_before_callback) {
			CHECK(fixture->close() == OK);
		}
	}
};

TEST_CASE("[P01][SceneTree] Fixed scene uses real skin and manual animation") {
	auto *fixture = memnew(SkinnedMeshCaptureTestFixture);
	SceneTree::get_singleton()->get_root()->add_child(fixture);
	const Error setup = SkinnedMeshCaptureTestFixtureTestAccess::prepare(fixture, "S02");
	CHECK(setup == OK);
	if (setup != OK) {
		memdelete(fixture);
		return;
	}
	auto *mesh = Object::cast_to<MeshInstance3D>(fixture->get_node_or_null(NodePath("Source/Mesh")));
	auto *skeleton = Object::cast_to<Skeleton3D>(fixture->get_node_or_null(NodePath("Source/Skeleton")));
	auto *animation = Object::cast_to<AnimationPlayer>(fixture->get_node_or_null(NodePath("AnimationPlayer")));
	CHECK(mesh != nullptr);
	CHECK(skeleton != nullptr);
	CHECK(animation != nullptr);
	if (!mesh || !skeleton || !animation) {
		memdelete(fixture);
		return;
	}
	CHECK(mesh->get_node_or_null(mesh->get_skeleton_path()) == skeleton);
	CHECK(mesh->get_skin().is_valid());
	CHECK(mesh->get_skin_reference().is_valid());
	if (mesh->get_skin().is_valid()) {
		CHECK(mesh->get_skin()->get_bind_name(0) == "tip");
		CHECK(mesh->get_skin()->get_bind_bone(0) == 0);
		CHECK(mesh->get_skin()->get_bind_pose(0).origin == Vector3(0, -2, 0));
	}
	CHECK(mesh->get_mesh().is_valid());
	if (mesh->get_mesh().is_valid()) {
		CHECK(mesh->get_mesh()->get_surface_count() == 1);
		const Array arrays = mesh->get_mesh()->surface_get_arrays(0);
		const PackedVector3Array points = arrays[Mesh::ARRAY_VERTEX];
		CHECK(points.size() == 3);
		if (points.size() == 3) {
			CHECK(points[1] == Vector3(1, 0, 0));
			CHECK(points[2] == Vector3(0, 1, 0));
		}
	}
	CHECK(skeleton->get_bone_count() == 2);
	CHECK(skeleton->get_bone_parent(1) == 0);
	CHECK(skeleton->get_bone_rest(1).origin == Vector3(0, 2, 0));
	CHECK(animation->get_callback_mode_process() == AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	CHECK(animation->has_animation("p01"));
	CHECK(skeleton->get_bone_pose_position(1) == Vector3(0, 2, 0));
	animation->advance(1.0);
	MessageQueue::get_singleton()->flush();
	CHECK(skeleton->get_bone_pose_position(1) == Vector3(1, 2, 0));
	CHECK(SkinnedMeshCaptureTestFixtureTestAccess::prepare(fixture, "S02") == ERR_ALREADY_IN_USE);
	CloseListener *listener = memnew(CloseListener);
	listener->fixture = fixture;
	fixture->get_node_or_null(NodePath("Source"))->connect("tree_exiting", callable_mp(listener, &CloseListener::on_exit));
	SkinnedMeshCaptureTestFixture::close_registry();
	CHECK(listener->callbacks == 1);
	CHECK(listener->published_before_callback);
	memdelete(listener);
	CHECK(fixture->get_child_count() == 0);
	CHECK_FALSE(bool(fixture->take_result()["scene_prepared"]));
	CHECK(fixture->close() == OK);
	memdelete(fixture);
}

TEST_CASE("[P01] Native registry closes all live fixtures") {
	auto *first = memnew(SkinnedMeshCaptureTestFixture);
	auto *second = memnew(SkinnedMeshCaptureTestFixture);
	auto *destroyed = memnew(SkinnedMeshCaptureTestFixture);
	memdelete(destroyed);
	SkinnedMeshCaptureTestFixture::close_registry();
	CHECK(first->begin("S02", "N") == ERR_ALREADY_IN_USE);
	CHECK(second->begin("S02", "N") == ERR_ALREADY_IN_USE);
	SkinnedMeshCaptureTestFixture::close_registry();
	memdelete(first);
	memdelete(second);
}

TEST_CASE("[P01] Native fixture fails closed before acquisition") {
	CHECK(ClassDB::class_exists("SkinnedMeshCaptureTestFixture"));
	Object *object = ClassDB::instantiate("SkinnedMeshCaptureTestFixture");
	auto *fixture = Object::cast_to<SkinnedMeshCaptureTestFixture>(object);
	CHECK(fixture != nullptr);
	if (!fixture) {
		if (object) {
			memdelete(object);
		}
		return;
	}
	CHECK(fixture->begin("wrong", "P") == ERR_INVALID_PARAMETER);
	CHECK(fixture->begin("S07", "P") == ERR_INVALID_PARAMETER);
	CHECK(fixture->begin("S02", "wrong") == ERR_INVALID_PARAMETER);
	CHECK(fixture->begin("S02", "N") == ERR_UNAVAILABLE);
	CHECK(fixture->get_child_count() == 0);
	CHECK(fixture->advance_action(1) == ERR_INVALID_PARAMETER);
	CHECK(fixture->submit_public_snapshot(Dictionary()) == ERR_INVALID_PARAMETER);
	CHECK(fixture->close(true) == ERR_UNAVAILABLE);
	CHECK(fixture->get_child_count() == 0);
	CHECK(fixture->close() == OK);
	CHECK(fixture->close() == OK);
	CHECK(fixture->begin("S02", "N") == ERR_ALREADY_IN_USE);
	memdelete(fixture);
}

TEST_CASE("[P01] Native fixture rejects foreign-thread calls without changing state") {
	auto *fixture = memnew(SkinnedMeshCaptureTestFixture);
	fixture->begin("bad", "P");
	const Dictionary before = fixture->take_result();
	Error errors[4] = {};
	Dictionary thread_result;
	std::thread worker([&] {
		errors[0] = fixture->begin("S02", "P");
		errors[1] = fixture->advance_action(1);
		errors[2] = fixture->submit_public_snapshot(Dictionary());
		errors[3] = fixture->close();
		thread_result = fixture->take_result();
	});
	worker.join();
	for (Error error : errors) {
		CHECK(error == ERR_UNAVAILABLE);
	}
	CHECK(String(thread_result["last_diagnostic"]) == "P01_PHASE");
	CHECK(fixture->take_result().recursive_equal(before, 0));
	CHECK(fixture->get_child_count() == 0);
	memdelete(fixture);
}

#if !defined(DISABLE_DEPRECATED) && !defined(PHYSICS_3D_DISABLED)
class InvalidSimulatorListener : public Object {
	GDCLASS(InvalidSimulatorListener, Object);
	static void _bind_methods() {}

public:
	void on_child(Node *p_node) {
		if (p_node->get_name() == "Source") {
			auto *skeleton = Object::cast_to<Skeleton3D>(p_node->get_node_or_null(NodePath("Skeleton")));
			skeleton->add_child(memnew(PhysicalBoneSimulator3D));
		}
	}
};

TEST_CASE("[P01][SceneTree] Fixed scene rejects an extra simulator without leaving children") {
	auto *fixture = memnew(SkinnedMeshCaptureTestFixture);
	auto *listener = memnew(InvalidSimulatorListener);
	SceneTree::get_singleton()->get_root()->add_child(fixture);
	const Callable callback = callable_mp(listener, &InvalidSimulatorListener::on_child);
	fixture->connect("child_entered_tree", callback);
	CHECK(SkinnedMeshCaptureTestFixtureTestAccess::prepare(fixture, "S02") == ERR_INVALID_DATA);
	CHECK(fixture->get_child_count() == 0);
	CHECK_FALSE(bool(fixture->take_result()["scene_prepared"]));
	fixture->disconnect("child_entered_tree", callback);
	CHECK(SkinnedMeshCaptureTestFixtureTestAccess::prepare(fixture, "S02") == OK);
	fixture->close();
	memdelete(listener);
	memdelete(fixture);
}
#endif
} // namespace TestSkinnedMeshCaptureTestNativeFixture

TEST_CASE("[P01] S02 next action waits for recording but not asynchronous readback") {
	auto *fixture = memnew(SkinnedMeshCaptureTestFixture);
	SkinnedMeshCaptureTestFixtureTestAccess::check_recording_gate(fixture);
	memdelete(fixture);
}

// Exact original CPU entry: real fixture rejection/cleanup and capture epoch
// state. Successful public attach/arm remains the ANGE source's obligation.
TEST_CASE("[P01] State") {
	auto *fixture = memnew(SkinnedMeshCaptureTestFixture);
	for (const String &member : { String("unknown"), String("S07") }) {
		CHECK(fixture->begin(member, "P") == ERR_INVALID_PARAMETER);
		CHECK(String(fixture->take_result()["last_diagnostic"]) == "P01_STATE");
		CHECK(fixture->get_child_count() == 0);
	}
	CHECK(fixture->advance_action(0) == ERR_INVALID_PARAMETER);
	CHECK(String(fixture->take_result()["last_diagnostic"]) == "P01_STATE");
	CHECK(fixture->submit_public_snapshot(Dictionary()) == ERR_INVALID_PARAMETER);
	CHECK(String(fixture->take_result()["last_diagnostic"]) == "P01_STATE");
	CHECK(fixture->begin("S02", "N") == ERR_UNAVAILABLE);
	CHECK(String(fixture->take_result()["last_diagnostic"]) == "P01_DEPENDENCY_RD");
	CHECK_FALSE(bool(fixture->take_result()["scene_prepared"]));
	CHECK(fixture->close() == OK);
	CHECK(String(fixture->take_result()["close_state"]) == "closed");
	CHECK(fixture->close() == OK);
	CHECK(fixture->begin("S02", "N") == ERR_ALREADY_IN_USE);
	CHECK(String(fixture->take_result()["last_diagnostic"]) == "P01_STATE");
	memdelete(fixture);

	SkinnedMeshCaptureTestCapture capture;
	capture.begin(41);
	SkinnedMeshCaptureTestCaptureEvent event{ 0, 1, 1, 41, 2, 3, 4, 5, 6, 0, 36, 0 };
	REQUIRE(capture.append(event));
	capture.begin(42); // Cannot replace live epoch while work remains.
	CHECK(capture.is_poisoned());
	CHECK_FALSE(capture.dispose_physical());
	event.kind = 2;
	REQUIRE(capture.append(event)); // Original identity still drains.
	REQUIRE(capture.dispose_physical());
	CHECK_FALSE(capture.seal());
	capture.begin(42); // Legal newer epoch recovers only after disposal.
	CHECK_FALSE(capture.is_poisoned());
	event.epoch = 42;
	event.kind = 1;
	REQUIRE(capture.append(event));
	event.kind = 2;
	REQUIRE(capture.append(event));
	REQUIRE(capture.dispose_physical());
	REQUIRE(capture.seal());
	capture.begin(42); // Never reuse an epoch even after healthy seal.
	CHECK(capture.is_poisoned());
}
