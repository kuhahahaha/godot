/**************************************************************************/
/*  test_cpu_oracle.h                                                 */
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

#include "scene/3d/skeleton_3d.h"
#include "tests/test_macros.h"

namespace TestSkinnedMeshCaptureCPUOracle {
// CPU reference over public Skeleton/Skin data. This never qualifies a shader.
// Name precedence and pose*bind are evaluated, not supplied as expected values.
static Vector3 deform(Skeleton3D *skeleton, const Ref<Skin> &skin, const Vector3 &point, int slot, uint16_t weight, int second_slot = 1) {
	Vector3 result;
	const int slots[2] = { slot, second_slot };
	const uint32_t weights[2] = { weight, uint32_t(65535 - weight) };
	for (int lane = 0; lane < 2; lane++) {
		const StringName name = skin->get_bind_name(slots[lane]);
		const int bone = name == StringName() ? skin->get_bind_bone(slots[lane]) : skeleton->find_bone(name);
		REQUIRE(bone >= 0);
		// Build global pose from current public local transforms; off-tree
		// setters intentionally do not invalidate the global-pose cache.
		Transform3D pose = skeleton->get_bone_pose(bone);
		for (int parent = skeleton->get_bone_parent(bone); parent >= 0; parent = skeleton->get_bone_parent(parent)) {
			pose = skeleton->get_bone_pose(parent) * pose;
		}
		const Transform3D palette = pose * skin->get_bind_pose(slots[lane]);
		result += palette.xform(point) * (double(weights[lane]) / 65535.0);
	}
	return result;
}

TEST_CASE("[P01] Oracle") {
	auto *skeleton = memnew(Skeleton3D);
	skeleton->add_bone("root");
	skeleton->add_bone("tip");
	skeleton->set_bone_parent(1, 0);
	skeleton->set_bone_rest(1, Transform3D(Basis(), Vector3(0, 2, 0)));
	skeleton->reset_bone_poses();
	Ref<Skin> skin;
	skin.instantiate();
	skin->set_bind_count(2);
	skin->set_bind_name(0, "tip");
	skin->set_bind_bone(0, 0); // Deliberately differs from named bone 1.
	skin->set_bind_pose(0, Transform3D(Basis(), Vector3(0, -2, 0)));
	skin->set_bind_bone(1, 0);
	skin->set_bind_pose(1, Transform3D());
	const Vector3 points[3] = { Vector3(), Vector3(1, 0, 0), Vector3(0, 1, 0) };
	const Vector3 expected[4][3] = {
		{ Vector3(), Vector3(1, 0, 0), Vector3(0, 1, 0) },
		{ Vector3(), Vector3(2, 0, 0), Vector3(1, 1, 0) },
		{ Vector3(), Vector3(3, 0, 0), Vector3(2, 1, 0) },
		{ Vector3(), Vector3(4, 0, 0), Vector3(3, 1, 0) },
	};
	for (int time = 0; time < 4; time++) {
		skeleton->set_bone_pose_position(1, Vector3(time, 2, 0));
		for (int vertex = 0; vertex < 3; vertex++) {
			CHECK(deform(skeleton, skin, points[vertex], vertex == 0 ? 1 : 0, 65535).distance_to(expected[time][vertex]) < 0.00002);
		}
	}
	skeleton->set_bone_pose_position(1, Vector3(2, 2, 0));
	CHECK(deform(skeleton, skin, points[2], 0, 32768).distance_to(Vector3(65536.0 / 65535.0, 1, 0)) < 0.000002);
	skeleton->set_bone_pose_position(1, Vector3(1, 2, 0));
	CHECK(deform(skeleton, skin, Vector3(2, 0, 0), 0, 65535).distance_to(Vector3(3, 0, 0)) < 0.00002);
	const Transform3D world(Basis::from_scale(Vector3(2, 1, 1)), Vector3(2, -1, 0));
	const Vector3 world_expected[3] = { Vector3(2, -1, 0), Vector3(6, -1, 0), Vector3(4, 0, 0) };
	for (int vertex = 0; vertex < 3; vertex++) {
		CHECK(world.xform(deform(skeleton, skin, points[vertex], vertex == 0 ? 1 : 0, 65535)).distance_to(world_expected[vertex]) < 0.00002);
	}
	skeleton->set_bone_pose_rotation(0, Quaternion(Vector3(0, 0, 1), Math::PI / 2));
	CHECK(deform(skeleton, skin, points[1], 0, 65535).distance_to(Vector3(0, 2, 0)) < 0.00002);
	CHECK(deform(skeleton, skin, points[2], 0, 65535).distance_to(Vector3(-1, 1, 0)) < 0.00002);
	const Vector3 rotated_world_expected[3] = { Vector3(2, -1, 0), Vector3(2, 1, 0), Vector3(0, 0, 0) };
	for (int vertex = 0; vertex < 3; vertex++) {
		CHECK(world.xform(deform(skeleton, skin, points[vertex], vertex == 0 ? 1 : 0, 65535)).distance_to(rotated_world_expected[vertex]) < 0.00002);
	}
	memdelete(skeleton);
}
} // namespace TestSkinnedMeshCaptureCPUOracle
