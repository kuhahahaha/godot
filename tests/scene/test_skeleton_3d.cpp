/**************************************************************************/
/*  test_skeleton_3d.cpp                                                  */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_skeleton_3d)

#ifndef _3D_DISABLED

#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

#include <climits>
#include <thread>

struct PaletteCommitTestAccess {
	static void set_live_binding_revision(SkinReference &r_ref, int64_t p_value) {
		r_ref.live_binding_revision = p_value;
	}
	static void set_commit_serial(SkinReference &r_ref, int64_t p_value) {
		r_ref.commit_serial = p_value;
	}
	static void set_sampling_mapping_revision(Skeleton3D &r_skel, int64_t p_value) {
		r_skel.sampling_mapping_revision = p_value;
	}
	static void set_source_update_serial(Skeleton3D &r_skel, int64_t p_value) {
		r_skel.source_update_serial = p_value;
	}
	static bool is_emitting_palette_commit_notification(const Skeleton3D &r_skel) {
		return r_skel.emitting_palette_commit_notification;
	}
};

namespace TestSkeleton3D {

namespace {
class PaletteCommitProbe : public Object {
	GDCLASS(PaletteCommitProbe, Object);

public:
	enum Mode {
		MODE_OBSERVE_ONLY = 0,
		MODE_REENTRANT_UPDATE = 1,
		MODE_DELETE_SKELETON = 2,
	};

	Mode mode = MODE_OBSERVE_ONLY;
	Skeleton3D *skeleton = nullptr;
	Ref<SkinReference> skin_ref;
	int call_count = 0;
	int64_t observed_arg_serial = -1;
	int64_t observed_state_status = -1;
	int64_t observed_state_source_serial = -1;
	int64_t post_reentrant_source_serial = -1;

	void on_committed(int64_t p_source_update_serial) {
		++call_count;
		observed_arg_serial = p_source_update_serial;
		if (skin_ref.is_valid()) {
			Dictionary d = skin_ref->get_palette_commit_state();
			observed_state_status = int64_t(d["status"]);
			observed_state_source_serial = int64_t(d["source_update_serial"]);
		}
		if (mode == MODE_REENTRANT_UPDATE && skeleton && call_count == 1) {
			skeleton->set_bone_pose_position(0, Vector3(3.0f, 4.0f, 5.0f));
			skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
			if (skin_ref.is_valid()) {
				Dictionary after = skin_ref->get_palette_commit_state();
				post_reentrant_source_serial = int64_t(after["source_update_serial"]);
			}
		} else if (mode == MODE_DELETE_SKELETON && skeleton && call_count == 1) {
			Skeleton3D *to_delete = skeleton;
			skeleton = nullptr;
			memdelete(to_delete);
		}
	}
};

void check_schema_v1_keys(const Dictionary &p_state) {
	CHECK(p_state.size() == 12);
	CHECK(p_state.has("schema"));
	CHECK(p_state.has("status"));
	CHECK(p_state.has("reference_instance_id"));
	CHECK(p_state.has("skeleton_instance_id"));
	CHECK(p_state.has("live_binding_revision"));
	CHECK(p_state.has("committed_binding_revision"));
	CHECK(p_state.has("live_mapping_revision"));
	CHECK(p_state.has("committed_mapping_revision"));
	CHECK(p_state.has("commit_serial"));
	CHECK(p_state.has("source_update_serial"));
	CHECK(p_state.has("bind_count"));
	CHECK(p_state.has("mapping_valid"));
	CHECK(int64_t(p_state["schema"]) == 1);
}
} // namespace

TEST_CASE("[Skeleton3D] Test per-bone meta") {
	Skeleton3D *skeleton = memnew(Skeleton3D);
	skeleton->add_bone("root");
	skeleton->set_bone_rest(0, Transform3D());

	// Adding meta to bone.
	skeleton->set_bone_meta(0, "key1", "value1");
	skeleton->set_bone_meta(0, "key2", 12345);
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Rename bone and check if meta persists.
	skeleton->set_bone_name(0, "renamed_root");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Retrieve list of keys.
	List<StringName> keys;
	skeleton->get_bone_meta_list(0, &keys);
	CHECK_MESSAGE(keys.size() == 2, "Wrong number of bone meta keys.");
	CHECK_MESSAGE(keys.find("key1"), "key1 not found in bone meta list");
	CHECK_MESSAGE(keys.find("key2"), "key2 not found in bone meta list");

	// Removing meta.
	skeleton->set_bone_meta(0, "key1", Variant());
	skeleton->set_bone_meta(0, "key2", Variant());
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key1"), "Bone meta key1 should be deleted.");
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key2"), "Bone meta key2 should be deleted.");
	List<StringName> should_be_empty_keys;
	skeleton->get_bone_meta_list(0, &should_be_empty_keys);
	CHECK_MESSAGE(should_be_empty_keys.size() == 0, "Wrong number of bone meta keys.");

	// Deleting non-existing key should succeed.
	skeleton->set_bone_meta(0, "non-existing-key", Variant());
	memdelete(skeleton);
}

TEST_CASE("[SceneTree][Skeleton3D] SkinReference palette commit state statuses, detach, exhaustion, and reentrancy") {
	Skeleton3D *skeleton = memnew(Skeleton3D);
	SceneTree::get_singleton()->get_root()->add_child(skeleton);
	skeleton->add_bone("root");
	skeleton->set_bone_rest(0, Transform3D());

	Ref<Skin> skin;
	skin.instantiate();
	skin->add_named_bind("root", Transform3D());

	Ref<SkinReference> ref = skeleton->register_skin(skin);
	REQUIRE(ref.is_valid());

	// Before first NOTIFICATION_UPDATE_SKELETON commit after registration: NotInitialized (1).
	Dictionary state = ref->call("get_palette_commit_state");
	check_schema_v1_keys(state);
	CHECK(int64_t(state["status"]) == SkinReference::PALETTE_COMMIT_NOT_INITIALIZED);
	CHECK(int64_t(state["commit_serial"]) == 0);
	CHECK(bool(state["mapping_valid"]) == false);

	// Commit round -> Ready (0), committed palette transform readable on RenderingServer.
	skeleton->set_bone_pose_position(0, Vector3(1.0f, 2.0f, 3.0f));
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);

	state = ref->get_palette_commit_state();
	check_schema_v1_keys(state);
	CHECK(int64_t(state["status"]) == SkinReference::PALETTE_COMMIT_READY);
	CHECK(int64_t(state["reference_instance_id"]) == int64_t(uint64_t(ref->get_instance_id())));
	CHECK(int64_t(state["skeleton_instance_id"]) == int64_t(uint64_t(skeleton->get_instance_id())));
	CHECK(int64_t(state["commit_serial"]) >= 1);
	CHECK(int64_t(state["source_update_serial"]) >= 1);
	CHECK(int64_t(state["bind_count"]) == 1);
	CHECK(bool(state["mapping_valid"]) == true);
	if (ref->get_skeleton().is_valid()) {
		CHECK(RenderingServer::get_singleton()->skeleton_bone_get_transform(ref->get_skeleton(), 0).origin.is_equal_approx(Vector3(1.0f, 2.0f, 3.0f)));
	}

	// WrongThread (6) on non-main thread must return zeroed fields without touching skeleton_node.
	Dictionary worker_state;
	std::thread worker([&]() {
		worker_state = ref->get_palette_commit_state();
	});
	worker.join();
	check_schema_v1_keys(worker_state);
	CHECK(int64_t(worker_state["status"]) == SkinReference::PALETTE_COMMIT_WRONG_THREAD);
	CHECK(int64_t(worker_state["reference_instance_id"]) == 0);
	CHECK(int64_t(worker_state["skeleton_instance_id"]) == 0);
	CHECK(int64_t(worker_state["commit_serial"]) == 0);
	CHECK(bool(worker_state["mapping_valid"]) == false);

	// Mutating pose or skin marks SourceChanged (2) before next commit.
	skeleton->set_bone_pose_position(0, Vector3(5.0f, 0.0f, 0.0f));
	CHECK(int64_t(ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_SOURCE_CHANGED);
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	CHECK(int64_t(ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);

	// InvalidMapping (3) when named bind is missing (must not be masked by fallback bone 0).
	ERR_PRINT_OFF;
	skin->set_bind_name(0, "nonexistent_bone");
	CHECK(int64_t(ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_SOURCE_CHANGED);
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	ERR_PRINT_ON;
	state = ref->get_palette_commit_state();
	CHECK(int64_t(state["status"]) == SkinReference::PALETTE_COMMIT_INVALID_MAPPING);
	CHECK(bool(state["mapping_valid"]) == false);

	// Restore valid bone name -> Ready (0) again.
	skin->set_bind_name(0, "root");
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	CHECK(int64_t(ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);

	// Reentrancy guard on skin_palettes_committed: synchronous update inside callback advances serials without recursive emit.
	PaletteCommitProbe probe;
	probe.mode = PaletteCommitProbe::MODE_REENTRANT_UPDATE;
	probe.skeleton = skeleton;
	probe.skin_ref = ref;
	skeleton->connect("skin_palettes_committed", callable_mp(&probe, &PaletteCommitProbe::on_committed));
	skeleton->set_bone_pose_position(0, Vector3(2.0f, 0.0f, 0.0f));
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	CHECK(probe.call_count == 1);
	CHECK(probe.observed_state_status == SkinReference::PALETTE_COMMIT_READY);
	CHECK(probe.observed_state_source_serial == probe.observed_arg_serial);
	CHECK(probe.post_reentrant_source_serial == probe.observed_arg_serial + 1);
	CHECK_FALSE(PaletteCommitTestAccess::is_emitting_palette_commit_notification(*skeleton));
	skeleton->disconnect("skin_palettes_committed", callable_mp(&probe, &PaletteCommitProbe::on_committed));

	// clear_bones() permanently detaches registered SkinReference; re-adding bones does not revive old Ref.
	skeleton->clear_bones();
	state = ref->get_palette_commit_state();
	CHECK(int64_t(state["status"]) == SkinReference::PALETTE_COMMIT_DETACHED);
	CHECK(int64_t(state["skeleton_instance_id"]) == 0);
	skeleton->add_bone("root");
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	CHECK(int64_t(ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_DETACHED);
	Ref<SkinReference> fresh_ref = skeleton->register_skin(skin);
	REQUIRE(fresh_ref.is_valid());
	CHECK(fresh_ref.ptr() != ref.ptr());
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	CHECK(int64_t(fresh_ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);

	// Sticky INT64_MAX VersionExhausted (5) across all 4 counters: INT64_MAX - 1 is Ready, INT64_MAX is Exhausted.
	for (int which = 0; which < 4; ++which) {
		Skeleton3D *s = memnew(Skeleton3D);
		SceneTree::get_singleton()->get_root()->add_child(s);
		s->add_bone("b0");
		Ref<Skin> sk;
		sk.instantiate();
		sk->add_named_bind("b0", Transform3D());
		Ref<SkinReference> sr = s->register_skin(sk);
		s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
		REQUIRE(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);

		if (which == 0) {
			PaletteCommitTestAccess::set_live_binding_revision(*sr.ptr(), INT64_MAX - 2);
			sk->set_bind_pose(0, Transform3D(Basis(), Vector3(1, 0, 0))); // -> INT64_MAX - 1
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
			CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);
			sk->set_bind_pose(0, Transform3D(Basis(), Vector3(2, 0, 0))); // -> INT64_MAX (sticky)
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
		} else if (which == 1) {
			PaletteCommitTestAccess::set_sampling_mapping_revision(*s, INT64_MAX - 2);
			s->add_bone("b1"); // -> INT64_MAX - 1
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
			CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);
			s->add_bone("b2"); // -> INT64_MAX (sticky)
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
		} else if (which == 2) {
			PaletteCommitTestAccess::set_commit_serial(*sr.ptr(), INT64_MAX - 2);
			s->set_bone_pose_position(0, Vector3(1, 0, 0));
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON); // -> INT64_MAX - 1
			CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);
			s->set_bone_pose_position(0, Vector3(2, 0, 0));
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON); // -> INT64_MAX (sticky)
		} else {
			PaletteCommitTestAccess::set_source_update_serial(*s, INT64_MAX - 2);
			s->set_bone_pose_position(0, Vector3(1, 0, 0));
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON); // -> INT64_MAX - 1
			CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_READY);
			s->set_bone_pose_position(0, Vector3(2, 0, 0));
			s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON); // -> INT64_MAX (sticky)
		}

		CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_VERSION_EXHAUSTED);
		// Sticky even after subsequent updates.
		s->set_bone_pose_position(0, Vector3(3, 0, 0));
		s->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
		CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_VERSION_EXHAUSTED);
		memdelete(s);
		// Precedence: Detached (4) beats VersionExhausted (5) once Skeleton3D is destroyed.
		CHECK(int64_t(sr->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_DETACHED);
	}

	// Synchronous memdelete(skeleton) inside skin_palettes_committed callback must not UAF.
	PaletteCommitProbe delete_probe;
	delete_probe.mode = PaletteCommitProbe::MODE_DELETE_SKELETON;
	delete_probe.skeleton = skeleton;
	delete_probe.skin_ref = fresh_ref;
	skeleton->connect("skin_palettes_committed", callable_mp(&delete_probe, &PaletteCommitProbe::on_committed));
	skeleton->set_bone_pose_position(0, Vector3(9.0f, 0.0f, 0.0f));
	ERR_PRINT_OFF;
	skeleton->notification(Skeleton3D::NOTIFICATION_UPDATE_SKELETON);
	ERR_PRINT_ON;
	CHECK(delete_probe.call_count == 1);
	CHECK(int64_t(fresh_ref->get_palette_commit_state()["status"]) == SkinReference::PALETTE_COMMIT_DETACHED);
}

} // namespace TestSkeleton3D

#endif // _3D_DISABLED
