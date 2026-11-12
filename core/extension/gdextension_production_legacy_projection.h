/**************************************************************************/
/*  gdextension_production_legacy_projection.h                            */
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
#include "core/extension/gdextension_production_request.h"
namespace GDExtensionDeviceExit {
inline GDExtensionProductionAppendV1 production_legacy_projection(const GDExtensionProductionAppendV2 &v, uint64_t begin) {
	GDExtensionProductionAppendV1 r = {};
	if (!v.abi.abi_version) {
		return r;
	}
	r.abi = v.abi.abi_version ? production_dto_abi<GDExtensionProductionAppendV1>() : GDExtensionDeviceExitAbi{};
	r.revision = v.revision;
	r.canonical_suboperation = v.canonical_suboperation;
	r.state = v.state;
	r.reserved = v.reserved;
	r.cause = v.cause;
	r.recorded_bytes = v.recorded_bytes;
	r.parent_bytes = v.parent_bytes;
	r.first_graph_fact = v.first_graph_fact;
	r.graph_fact_count = v.graph_fact_count;
	return r;
}
inline GDExtensionProductionFactV1 production_legacy_projection(const GDExtensionProductionFactV2 &v, uint64_t begin) {
	GDExtensionProductionFactV1 r = {};
	r.canonical_suboperation = v.canonical_suboperation;
	r.graph_instance = v.graph_instance;
	r.graph_frame = v.graph_frame;
	r.graph_kind = v.graph_kind;
	r.graph_index = v.graph_index;
	r.graph_offset = v.graph_offset;
	r.command_slot = v.command_slot;
	r.backend_queue = v.backend_queue;
	r.issue = v.issue;
	r.completion = v.completion;
	r.phase = v.phase;
	r.native_serial = v.native_serial;
	r.completion_serial = v.completion_serial;
	r.issue_native_code = v.issue_native_code;
	r.completion_native_code = v.completion_native_code;
	r.issue_error = v.issue_error;
	r.retention_error = v.retention_error;
	r.membership_parent = v.membership_parent;
	r.reserved = v.reserved;
	return r;
}
inline GDExtensionProductionReportV1 production_legacy_projection(const GDExtensionProductionReportV2 &v, uint64_t begin) {
	GDExtensionProductionReportV1 r = {};
	if (!v.abi.abi_version) {
		return r;
	}
	r.abi = v.abi.abi_version ? production_dto_abi<GDExtensionProductionReportV1>() : GDExtensionDeviceExitAbi{};
	r.operation = v.operation;
	r.revision = v.revision;
	r.captured_segment = v.captured_segment;
	r.acknowledged_segment = v.acknowledged_segment;
	r.recorded_end = v.action_count ? v.operation.canonical_begin + v.recorded_action_end : 0;
	r.fact_count = v.fact_count;
	r.known_obligations = v.known_obligations;
	r.possible_obligations = v.possible_obligations;
	r.complete_obligations = v.complete_obligations;
	r.ready_readbacks = v.ready_readbacks;
	r.physical_returns_pending = v.physical_returns_pending;
	r.cancelled = v.cancelled;
	r.reserved = v.reserved;
	r.cause = v.cause;
	r.service_control_serial = v.service_control_serial;
	r.service_pending = v.service_pending;
	return r;
}
inline GDExtensionProductionSegmentV1 production_legacy_projection(const GDExtensionProductionSegmentV2 &v, uint64_t begin) {
	GDExtensionProductionSegmentV1 r = {};
	if (!v.abi.abi_version) {
		return r;
	}
	r.abi = v.abi.abi_version ? production_dto_abi<GDExtensionProductionSegmentV1>() : GDExtensionDeviceExitAbi{};
	r.segment = v.segment;
	r.segment_revision = v.segment_revision;
	r.first_canonical = begin + v.first_action_ordinal;
	r.end_canonical = begin + v.end_action_ordinal;
	r.incomplete_upload_parent = v.incomplete_upload_action_ordinal == UINT64_MAX ? UINT64_MAX : begin + v.incomplete_upload_action_ordinal;
	return r;
}
inline GDExtensionProductionRecordReportV1 production_legacy_projection(const GDExtensionProductionRecordReportV2 &v, uint64_t begin) {
	GDExtensionProductionRecordReportV1 r = {};
	if (!v.abi.abi_version) {
		return r;
	}
	r.abi = v.abi.abi_version ? production_dto_abi<GDExtensionProductionRecordReportV1>() : GDExtensionDeviceExitAbi{};
	r.request_serial = v.request_serial;
	r.control_serial = v.control_serial;
	r.pause = v.pause;
	r.flags = v.flags;
	r.result = v.result;
	r.report = production_legacy_projection(v.report, begin);
	r.append = production_legacy_projection(v.append, begin);
	r.segment = production_legacy_projection(v.segment, begin);
	return r;
}
} //namespace GDExtensionDeviceExit
