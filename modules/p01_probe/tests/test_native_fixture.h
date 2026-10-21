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

#include "modules/p01_probe/p01_probe_fixture.h"

// C++-only scene construction access. It neither arms native acquisition nor
// creates a script-facing preflight bypass.
struct P01FixtureTestAccess {
	static Error prepare(P01ProbeFixture *p_fixture, const StringName &p_member) { return p_fixture->build_fixed_scene(p_member); }
};

namespace TestP01NativeFixture {
class CloseListener : public Object {
	GDCLASS(CloseListener, Object);
	static void _bind_methods() {}

public:
	P01ProbeFixture *fixture = nullptr;
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
	auto *fixture = memnew(P01ProbeFixture);
	SceneTree::get_singleton()->get_root()->add_child(fixture);
	const Error setup = P01FixtureTestAccess::prepare(fixture, "S02");
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
	CHECK(P01FixtureTestAccess::prepare(fixture, "S02") == ERR_ALREADY_IN_USE);
	CloseListener *listener = memnew(CloseListener);
	listener->fixture = fixture;
	fixture->get_node_or_null(NodePath("Source"))->connect("tree_exiting", callable_mp(listener, &CloseListener::on_exit));
	P01ProbeFixture::close_registry();
	CHECK(listener->callbacks == 1);
	CHECK(listener->published_before_callback);
	memdelete(listener);
	CHECK(fixture->get_child_count() == 0);
	CHECK_FALSE(bool(fixture->take_result()["scene_prepared"]));
	CHECK(fixture->close() == OK);
	memdelete(fixture);
}

TEST_CASE("[P01] Native registry closes all live fixtures") {
	auto *first = memnew(P01ProbeFixture);
	auto *second = memnew(P01ProbeFixture);
	auto *destroyed = memnew(P01ProbeFixture);
	memdelete(destroyed);
	P01ProbeFixture::close_registry();
	CHECK(first->begin("S02", "N") == ERR_ALREADY_IN_USE);
	CHECK(second->begin("S02", "N") == ERR_ALREADY_IN_USE);
	P01ProbeFixture::close_registry();
	memdelete(first);
	memdelete(second);
}

TEST_CASE("[P01] Native fixture fails closed before acquisition") {
	CHECK(ClassDB::class_exists("P01ProbeFixture"));
	Object *object = ClassDB::instantiate("P01ProbeFixture");
	auto *fixture = Object::cast_to<P01ProbeFixture>(object);
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
	auto *fixture = memnew(P01ProbeFixture);
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
	auto *fixture = memnew(P01ProbeFixture);
	auto *listener = memnew(InvalidSimulatorListener);
	SceneTree::get_singleton()->get_root()->add_child(fixture);
	const Callable callback = callable_mp(listener, &InvalidSimulatorListener::on_child);
	fixture->connect("child_entered_tree", callback);
	CHECK(P01FixtureTestAccess::prepare(fixture, "S02") == ERR_INVALID_DATA);
	CHECK(fixture->get_child_count() == 0);
	CHECK_FALSE(bool(fixture->take_result()["scene_prepared"]));
	fixture->disconnect("child_entered_tree", callback);
	CHECK(P01FixtureTestAccess::prepare(fixture, "S02") == OK);
	fixture->close();
	memdelete(listener);
	memdelete(fixture);
}
#endif
} // namespace TestP01NativeFixture
