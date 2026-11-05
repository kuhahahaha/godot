/**************************************************************************/
/*  particles_runtime_test_fixture.h                                      */
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
#include "core/templates/rid.h"
#include "core/variant/dictionary.h"
#include "servers/rendering/render_capture_test_tracking.h"

#include <array>

class RenderingDevice;
namespace RendererRD {
class ParticlesStorage;
}

// A fixed native test, with no externally supplied RIDs, shaders or fault targets.
// Available only in the explicitly enabled tests-only module.
class ParticlesRuntimeTestFixture : public RefCounted {
	GDCLASS(ParticlesRuntimeTestFixture, RefCounted)
	struct Allocation {
		uint64_t driver = 0, serial = 0;
		RID rid;
	};
	RenderingDevice *rd = nullptr;
	RendererRD::ParticlesStorage *storage = nullptr;
	ObjectID device;
	RID particle, sub, progress[4], sdf_particle, trail_shader, trail_pipeline;
	RID trail_buffer, trail_set;
	std::array<Allocation, 48> allocations{};
	std::array<uint64_t, 48> resources{};
	uint32_t allocation_count = 0, resource_count = 0;
	uint32_t readbacks_queued = 0, readbacks_completed = 0;
	uint32_t sdf_readbacks_queued = 0, sdf_readbacks_completed = 0;
	bool started = false, released = false, retired = false;
	Array failures, observations;
	bool check(bool p_ok, const String &p_label);
	bool on_owner() const;
	bool track_buffer(RID p_rid);
	bool track_set(RID p_rid);
	bool track_resource(RenderCaptureTestResourceKind p_kind, uint64_t p_driver);
	void track_particles(RID p_rid);
	bool run_upload_boundaries();
	bool run_sdf_history_retry();
	bool run_mobile_instance_recovery();
	bool run_progress(int p_kind);
	bool run_trail();
	void readback(const PackedByteArray &p_bytes, int p_kind);
	void release_resources();

protected:
	static void _bind_methods();

public:
	Error begin();
	Error advance();
	Dictionary result() const;
	~ParticlesRuntimeTestFixture();
};
