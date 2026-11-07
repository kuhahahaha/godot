/**************************************************************************/
/*  native_height_range.h                                                 */
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
#include "core/templates/local_vector.h"
#include "servers/rendering/rendering_device_driver.h"
#include "servers/rendering/rendering_device_graph.h"

class RenderingDevice;

// Original RD mechanical custody for one exclusive standalone D32 clear.
// No public constructor, recording, source enrollment, or completion setter.
// RD/Frame keep the cell alive until every actual observer is returned.
class NativeHeightRange final : public RefCounted {
	friend class RenderingDevice;
#ifdef TESTS_ENABLED
	friend class TestNativeHeightRangeAccess;
#endif

public:
	struct Snapshot {
		bool recording_closed = false;
		bool has_recorded_effect = false;
		bool mapping_closed = false;
		bool issue_closed = false;
		bool native_complete = false;
		bool prefix_settled = false;
		bool observers_returned = false;
		uint32_t graph_members = 0;
		uint32_t sealed_members = 0;
		uint32_t issued_sources = 0;
		uint32_t retained_sources = 0;
		uint32_t completed_sources = 0;
		Error append_error = OK;
		Error mapping_error = OK;
		Error issue_error = OK;
		Error retention_error = OK;
		Error completion_error = OK;
		Error return_error = OK;
	};

	// Never call a driver, submit, poll, release, or free backing here.
	~NativeHeightRange() = default;

private:
	struct Limits {
		uint32_t graph_members = 0;
		uint32_t native_sources = 0;
		uint32_t source_edges = 0;
	};

	enum class MemberState : uint8_t {
		Recorded,
		Sealed,
		IssueAttached,
		MappingFailed,
	};
	struct GraphMember {
		RenderingDeviceGraph::RecordHandle handle;
		uint32_t frame = 0;
		uint64_t frame_epoch = 0;
		MemberState state = MemberState::Recorded;
		uint32_t slot = UINT32_MAX;
		RenderingDeviceDriver::CommandBufferID command_buffer;
		bool issue_attempted = false;
	};
	struct NativeSource {
		RenderingDeviceDriver::NativeCallFact original;
		bool retained = false;
		bool complete = false;
		bool observer_returned = false;
		Error retention_error = OK;
		Error completion_error = OK;
		int64_t completion_native_code = 0;
	};
	struct SourceEdge {
		uint32_t member = 0;
		uint32_t source = 0;
	};

	const uint64_t device;
	const uint64_t serial; // RD incarnation only; no E09/business identity.
	const Limits limits;
	// Prepare sizes all storage before any actual append/native issue. Counts
	// delimit occupied rows; hooks must not resize after recording starts.
	LocalVector<GraphMember> graphs;
	LocalVector<NativeSource> sources;
	LocalVector<SourceEdge> edges;
	uint32_t graph_count = 0;
	uint32_t source_count = 0;
	uint32_t edge_count = 0;
	uint32_t retain_cursor = 0;
	uint32_t poll_cursor = 0;
	uint32_t return_cursor = 0;
	// Original sink identity is compared only; no dereference or native work
	// survives the synchronous checked call or the sink's lifetime.
	const void *capture_sink = nullptr;
	uint32_t capture_frame = 0;
	uint64_t capture_epoch = 0;
	uint32_t capture_records = 0;
	uint32_t capture_transfers = 0;
	bool capture_cleanup = false;
	bool recording_started = false;
	bool recording_closed = false;
	bool has_recorded_effect = false;
	bool uncaptured_prefix = false;
	bool mapping_failed = false;
	bool native_complete = false; // Cached only after authoritative observation.
	bool prefix_settled = false; // Physical prefix is fully observed and returnable; does not imply business success.
	bool observers_returned = false;
	Error append_error = OK;
	Error mapping_error = OK;
	Error issue_error = OK;
	Error retention_error = OK;
	Error completion_error = OK;
	Error return_error = OK;

	NativeHeightRange(uint64_t p_device, uint64_t p_serial, const Limits &p_limits) :
			device(p_device), serial(p_serial), limits(p_limits) {}
};
