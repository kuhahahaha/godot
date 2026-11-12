/**************************************************************************/
/*  skinned_mesh_capture_test_fixture.h                                   */
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

#include "skinned_mesh_capture_test_native_session.h"

#include "scene/3d/node_3d.h"

struct SkinnedMeshCaptureTestFixtureTestAccess;

// Tests-only fixed scene owner. This script surface cannot inject native
// evidence, shaders, RIDs, or an acquisition authorization.
class SkinnedMeshCaptureTestFixture : public Node3D {
	GDCLASS(SkinnedMeshCaptureTestFixture, Node3D);
	friend struct SkinnedMeshCaptureTestFixtureTestAccess;

	String last_diagnostic;
	bool closed = false;
	bool closing = false;
	bool scene_prepared = false;
	ObjectID scene_nodes[4];
	std::shared_ptr<SkinnedMeshCaptureTestNativeSession> session;
	uint64_t session_epoch = 0, next_request = 1;
	int32_t next_action = 0;
	bool sample_pending = false, initialization_pending = false;
	uint64_t last_action_request = 0;
	void frame_pre_draw();
	bool initialization_ready(const SkinnedMeshCaptureTestSessionStatus &p_status) const;
	bool action_complete(const SkinnedMeshCaptureTestSessionStatus &p_status) const;

	static void _bind_methods();
	Error build_fixed_scene(const StringName &p_member);
	Error reject(Error p_error, const char *p_diagnostic);

public:
	Error begin(StringName p_member, StringName p_route);
	Error advance_action(int32_t p_action_index);
	Error submit_public_snapshot(const Dictionary &p_value);
	Dictionary take_result() const;
	Error close(bool p_no_future_draw = false);
	static void close_registry();
	SkinnedMeshCaptureTestFixture();
	~SkinnedMeshCaptureTestFixture();
};
