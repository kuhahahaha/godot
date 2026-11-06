#pragma once

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
	float geometry_height = 0;
	RID mesh;
	RenderGeometryInstance *geometry = nullptr;
	RID first_collision;
	RID second_collision;
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
	Dictionary result() const;
};
