/**************************************************************************/
/*  particles_scene_pairs_test_fixture.h                                  */
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
#include "servers/rendering/renderer_viewport.h"
class ParticlesScenePairsTestFixture : public RefCounted {
	GDCLASS(ParticlesScenePairsTestFixture, RefCounted)
	mutable Mutex result_mutex;
	Dictionary result_data;
	void *managed_scene = nullptr;
	bool phase_mode = false;
	bool viewport_mode = false;
	Ref<RendererViewport::Viewport::CameraCapture> camera_native_old_capture;
	RID camera_native_viewport;
	Transform3D camera_native_old_transform;
	Projection camera_native_old_projection;

protected:
	static void _bind_methods();

public:
	void camera_native_register(RID p_viewport, int64_t p_present_seq);
	void camera_native_observe(RID p_viewport, Transform3D p_expected, int64_t p_stage);
	void camera_native_invalidate(RID p_viewport);
	void camera_native_finish(RID p_viewport);
	void run();
	void run_phase();
	void run_viewport_invariance(RID p_scenario);
	void finish_viewport_observation();
	void advance();
	Dictionary result() const;
};
