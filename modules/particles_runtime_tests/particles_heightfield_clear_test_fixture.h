#pragma once

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

class RenderingDevice;

class ParticlesHeightfieldClearTestFixture : public RefCounted {
	GDCLASS(ParticlesHeightfieldClearTestFixture, RefCounted)
	Dictionary result_data;
	mutable Mutex result_mutex;
	bool started = false;
	RID collision;
	RID shader;
	RID pipeline;
	RID buffer;
	RID sampler;
	RID uniform_set;
	RID texture;
	RenderingDevice *rd = nullptr;
	void sample_done(const PackedByteArray &p_bytes);

protected:
	static void _bind_methods();

public:
	Error run();
	Dictionary result() const;
};
