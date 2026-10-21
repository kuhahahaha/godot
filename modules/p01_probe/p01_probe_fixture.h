#pragma once

#include "scene/3d/node_3d.h"

struct P01FixtureTestAccess;

// Tests-only fixed scene owner. This script surface cannot inject native
// evidence, shaders, RIDs, or an acquisition authorization.
class P01ProbeFixture : public Node3D {
	GDCLASS(P01ProbeFixture, Node3D);
	friend struct P01FixtureTestAccess;

	String last_diagnostic;
	bool closed = false;
	bool closing = false;
	bool scene_prepared = false;
	ObjectID scene_nodes[4];

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
	P01ProbeFixture();
	~P01ProbeFixture();
};
