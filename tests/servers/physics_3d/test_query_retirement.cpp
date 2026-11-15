/**************************************************************************/
/*  test_query_retirement.cpp                                             */
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

TEST_FORCE_LINK(test_query_retirement)

#ifndef PHYSICS_3D_DISABLED

#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "servers/physics_3d/physics_server_3d_wrap_mt.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GODOT_PHYSICS_3D_ENABLED
#include "modules/godot_physics_3d/godot_physics_server_3d.h"
#endif
#ifdef MODULE_JOLT_PHYSICS_ENABLED
#include "modules/jolt_physics/jolt_physics_server_3d.h"
#endif

class TestPhysicsQueryRetirementAccess {
public:
	static PhysicsServer3D *backend(PhysicsServer3DWrapMT *p_wrapper) { return p_wrapper->physics_server_3d; }
	static bool threaded(PhysicsServer3DWrapMT *p_wrapper) { return p_wrapper->create_thread; }
};

namespace TestPhysicsQueryRetirement {

class CallbackRecorder : public Object {
	GDCLASS(CallbackRecorder, Object);

public:
	int force_count = 0;
	int sync_count = 0;
	real_t last_step = 0;

	void force(PhysicsDirectBodyState3D *p_state) {
		force_count++;
		CHECK(p_state != nullptr);
		if (p_state) {
			last_step = p_state->get_step();
		}
	}
	void state_sync(PhysicsDirectBodyState3D *p_state) { sync_count++; }
};

TEST_CASE("[SceneTree][PhysicsQueryRetirement] real backend dispatch control") {
	const String backend_name = OS::get_singleton()->get_environment("GODOT_TEST_PHYSICS_3D_BACKEND");
	const String threaded = OS::get_singleton()->get_environment("GODOT_TEST_PHYSICS_3D_THREADED");
	const bool valid_config = (backend_name == "GodotPhysics3D" || backend_name == "Jolt Physics") && (threaded == "0" || threaded == "1");
	CHECK(valid_config);
	if (!valid_config) {
		return;
	}
	PhysicsServer3D *server = PhysicsServer3D::get_singleton();
	CHECK(server != nullptr);
	if (!server) {
		return;
	}
	PhysicsServer3DWrapMT *wrapper = Object::cast_to<PhysicsServer3DWrapMT>(server);
	CHECK(wrapper != nullptr);
	if (!wrapper) {
		return;
	}
	PhysicsServer3D *backend = TestPhysicsQueryRetirementAccess::backend(wrapper);
	CHECK(backend != nullptr);
	if (!backend) {
		return;
	}
	if (backend_name == "GodotPhysics3D") {
#ifdef MODULE_GODOT_PHYSICS_3D_ENABLED
		CHECK(Object::cast_to<GodotPhysicsServer3D>(backend) != nullptr);
#else
		FAIL("GodotPhysics3D is not compiled into this test binary.");
#endif
	} else {
#ifdef MODULE_JOLT_PHYSICS_ENABLED
		CHECK(Object::cast_to<JoltPhysicsServer3D>(backend) != nullptr);
#else
		FAIL("Jolt Physics is not compiled into this test binary.");
#endif
	}
	CHECK(TestPhysicsQueryRetirementAccess::threaded(wrapper) == (threaded == "1"));

	CallbackRecorder *recorder = memnew(CallbackRecorder);
	RID space = server->space_create();
	RID shape = server->sphere_shape_create();
	RID body = server->body_create();
	CHECK(space.is_valid());
	CHECK(shape.is_valid());
	CHECK(body.is_valid());
	if (!space.is_valid() || !shape.is_valid() || !body.is_valid()) {
		if (body.is_valid()) {
			server->free_rid(body);
		}
		if (shape.is_valid()) {
			server->free_rid(shape);
		}
		if (space.is_valid()) {
			server->free_rid(space);
		}
		server->sync();
		server->end_sync();
		memdelete(recorder);
		return;
	}
	server->shape_set_data(shape, 0.5);
	server->body_set_mode(body, PhysicsServer3D::BODY_MODE_RIGID);
	server->body_add_shape(body, shape, Transform3D(), false);
	server->body_set_force_integration_callback(body, callable_mp(recorder, &CallbackRecorder::force));
	server->body_set_state_sync_callback(body, callable_mp(recorder, &CallbackRecorder::state_sync));
	server->body_set_space(body, space);
	server->space_set_active(space, true);

	for (int i = 1; i <= 2; i++) {
		server->step(1.0 / 60.0);
		server->sync();
		server->flush_queries();
		CHECK(recorder->force_count == i);
		CHECK(recorder->sync_count == i);
		CHECK(Math::is_equal_approx(recorder->last_step, real_t(1.0 / 60.0)));
		server->end_sync();
	}
	server->free_rid(body);
	server->free_rid(shape);
	server->free_rid(space);
	server->sync();
	server->end_sync();
	memdelete(recorder);
}

} // namespace TestPhysicsQueryRetirement

#endif // PHYSICS_3D_DISABLED
