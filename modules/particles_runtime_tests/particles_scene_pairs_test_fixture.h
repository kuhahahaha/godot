#pragma once
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
class ParticlesScenePairsTestFixture : public RefCounted {
	GDCLASS(ParticlesScenePairsTestFixture, RefCounted)
	mutable Mutex result_mutex;
	Dictionary result_data;
	void *managed_scene = nullptr;
	bool phase_mode = false;
protected:
	static void _bind_methods();
public:
	void run();
	void run_phase();
	void advance();
	Dictionary result() const;
};
