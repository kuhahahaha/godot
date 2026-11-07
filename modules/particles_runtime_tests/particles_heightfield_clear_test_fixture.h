#pragma once

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
	void cleanup();
	void sample_done(const PackedByteArray &p_bytes);

protected:
	static void _bind_methods();

public:
	Error run();
	Error run_geometry(float p_height);
	Error run_transformed(const Vector3 &p_scale);
	Error run_geometry_params(float p_height, float p_extents_y, float p_transform_y);
	Dictionary result() const;
};
