/**************************************************************************/
/*  particles_external_guard_test_fixture.h                               */
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

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

// run() is a fixed tests-only main-RD preparation test with no supplied RID,
// shader, source, budget, holder or fault target; no dispatch/physical-return claim.
// The read-only VRS observer accepts only an existing viewport RID, reads its
// actual target association, and does not issue or create resources.
class ParticlesExternalGuardTestFixture : public RefCounted {
	GDCLASS(ParticlesExternalGuardTestFixture, RefCounted)

	mutable Mutex result_mutex;
	bool started = false;
	bool complete = false;
	Error outcome = ERR_UNCONFIGURED;
	Array observations;
	Array failures;
	Dictionary device_info;

	void check(bool p_ok, const String &p_label);
	Error finish(Error p_error);

protected:
	static void _bind_methods();

public:
	Error run();
	Dictionary result() const;
	Dictionary observe_viewport_vrs_texture(RID p_viewport) const;
	Dictionary observe_particle_collision_texture(RID p_collision) const;
};
