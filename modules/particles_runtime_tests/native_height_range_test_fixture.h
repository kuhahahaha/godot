/**************************************************************************/
/*  native_height_range_test_fixture.h                                    */
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
#include "servers/rendering/native_depth_allocation_projection.h"
#include "servers/rendering/native_height_range.h"

class RenderingDevice;

// Tests-only original RD mechanical clear-range and physical-return evidence.
class NativeHeightRangeTestFixture : public RefCounted {
	GDCLASS(NativeHeightRangeTestFixture, RefCounted)
	mutable Mutex result_mutex;
	Dictionary published;
	Dictionary data;
	Array checks;
	Array failures;
	Dictionary range_snapshots;
	Dictionary physical_snapshots;
	Ref<NativeHeightRange> ranges[2];
	NativeDisposalWitness depth;
	NativeDisposalWitness framebuffer;
	NativeDepthAllocationProjection pair;
	RID depth_rid;
	RID framebuffer_rid;
	RID worker_buffer_rid;
	RID worker_array_rid;
	uint64_t worker_buffer_driver_id = 0;
	uint64_t worker_buffer_allocation_serial = 0;
	bool worker_resources_enqueued = false;
	bool worker_resources_returned = false;
	uint64_t initial_depth_serial = 0;
	uint64_t initial_framebuffer_serial = 0;
	uint64_t start_usec = 0;
	uint64_t recorded_draws = 0;
	uint64_t callback_count = 0;
	uint64_t maintenance_calls = 0;
	uint64_t range_maintenance_calls = 0;
	uint64_t observe_polls = 0;
	uint64_t observer_releases = 0;
	int case_index = -1;
	int phase = 0;
	bool started = false;
	bool complete = false;
	bool prefix_created = false;
	bool physical_returned = false;
	Error outcome = ERR_UNCONFIGURED;
	bool check(bool p_ok, const String &p_label);
	void publish();
	void finish(Error p_error);
	void advance_impl();
	Error record_worker_ranges(RenderingDevice *p_rd);
	bool worker_buffer_pending_disposal(RenderingDevice *p_rd) const;
	bool worker_owners_live(RenderingDevice *p_rd) const;
	bool worker_owners_returned(RenderingDevice *p_rd) const;
	Error observe_ranges(RenderingDevice *p_rd, const String &p_stage, NativeHeightRange::Snapshot (&r_snapshots)[2]);
	Error observe_pair(RenderingDevice *p_rd, const String &p_stage, NativeDepthAllocationProjection::Report &r_report);

protected:
	static void _bind_methods();

public:
	void begin(int p_case);
	void advance();
	Dictionary result() const;
};
