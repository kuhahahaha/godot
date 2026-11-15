/**************************************************************************/
/*  external_output_request.h                                             */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_config.h"
#include "servers/rendering/renderer_viewport.h"

namespace RendererRD {

// Parsed wire values only. None of these containers issues or authenticates an
// output, camera, host, source, consumed tuple, record or publication capability.
struct ExternalOutputRequestCommon {
	using CameraSourceToken = RendererViewport::Viewport::CameraSourceToken;

	uint64_t generation = 0;
	PackedByteArray output_tag;
	PackedByteArray view_tag;
	uint64_t present_seq = 0;
	PackedByteArray phase_token;
	AABB bounds;
	uint32_t history_reset = 0; // Wire enum 0..10; owner determines reset semantics.
	CameraSourceToken current_camera_token;
	bool has_actual_previous_camera = false;
	CameraSourceToken actual_previous_camera_token;
	PackedByteArray current_host_token;
	bool has_previous_consumed_tuple = false;
	PackedByteArray previous_consumed_tuple_token;

	// Exact common-value comparison only, not original-owner admission.
	bool matches(const ExternalOutputRequestCommon &p_other) const;
};

struct ExternalOutputRequest {
	ExternalOutputRequestCommon common;
	uint64_t content_version = 0;
	uint64_t view_epoch = 0;
	uint32_t pass_mask = 0;
	PackedByteArray layout_digest;
	PackedByteArray mapping_digest;
	PackedByteArray source_token;

	bool matches(const ExternalOutputRequest &p_other) const;
	static Error parse(const Dictionary &p_request, ExternalOutputRequest &r_request, StringName &r_detail);
};

struct ExternalOutputSeal {
	ExternalOutputRequestCommon common;
	PackedByteArray record_token;

	bool matches(const ExternalOutputSeal &p_other) const;
	static Error parse(const Dictionary &p_seal, ExternalOutputSeal &r_seal, StringName &r_detail);
};

} // namespace RendererRD
