/**************************************************************************/
/*  particles_heightfield_clear_test_fixture.h                            */
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

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

class RenderingDevice;
class RenderGeometryInstance;

class ParticlesHeightfieldClearTestFixture : public RefCounted {
	GDCLASS(ParticlesHeightfieldClearTestFixture, RefCounted)
	Dictionary result_data;
	mutable Mutex result_mutex;
	bool started = false;
	bool geometry_capture = false;
	bool transformed_capture = false;
	bool legacy_normal_capture = false;
	int legacy_transform_case = 0;
	RID legacy_particle;
	RID legacy_shader;
	RID legacy_material;
	int legacy_neighbor_case = -1;
	int source_selection_case = -1;
	RID selection_scenario;
	RID selection_field;
	RID selection_source;
	RID selection_receiver;
	RID selection_receiver_particles;
	RID legacy_texture;
	Vector3 capture_scale = Vector3(1, 1, 1);
	float geometry_height = 0;
	float capture_extents_y = 1.0f;
	float capture_transform_y = 0.0f;
	RID mesh;
	RenderGeometryInstance *geometry = nullptr;
	RID collision;
	RID first_instance;
	RID second_instance;
	RID shader;
	RID pipeline;
	RID buffer;
	RID sampler;
	RID uniform_set;
	RID first_texture;
	RID second_texture;
	RenderingDevice *rd = nullptr;
	Error run_capture();
	Error capture_source_selection();
	Error sample_depth_pair();
	void cleanup();
	void sample_done(const PackedByteArray &p_bytes);
	Error run_legacy_process(const Transform3D &p_field_transform);
	Error run_neighbor_setup();
	void legacy_sample_done(const PackedByteArray &p_bytes);

protected:
	static void _bind_methods();

public:
	Error run();
	Error run_source_selection(int p_case);
	Error run_legacy_normal(int p_transform_case = 0);
	Error run_legacy_neighbor(int p_case);
	Error run_geometry(float p_height);
	Error run_transformed(const Vector3 &p_scale);
	Error run_geometry_params(float p_height, float p_extents_y, float p_transform_y);
	Dictionary result() const;
};
