/**************************************************************************/
/*  gdextension_production_material_qualification.h                       */
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
#include "servers/rendering/renderer_rd/native_material_recipe_roster.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

namespace GDExtensionDeviceExit {

struct MaterialQualificationState {
	// Max attachments per state: 5 fixture surf bufs + 1 surf uset + 2 targets +
	// 8 owned buffers (roster creates <= 7) + 1 readback SSBO + 3 sets + 1 pipeline = 21.
	static constexpr uint32_t MAX_DISPOSAL_USES = 24;
	static constexpr uint32_t MAX_OWNED_BUFFERS = 8;
	static constexpr uint32_t MQ_RETAINED_RESULT_BYTES = sizeof(GDExtensionMaterialQualificationResultMetadataV1) + GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;

	GDExtensionMaterialQualificationPrepareV1 prepare = {};
	uint64_t result_payload_offset = 0;
	uint64_t payload_offset = 0;
	uint64_t candidate_surface_raw_format = 0;
	uint64_t depth_backing_serial = 0;
	uint64_t depth_allocation_generation = 0;

	GDExtensionMaterialQualificationSelectionFactsV1 selection = {};

	RendererRD::MeshStorage::NativeSelectionLease candidate_mesh_lease;
	RendererRD::MaterialStorage::NativeSelectionLease candidate_material_lease;
	RendererRD::NativeMaterialRecipeRoster roster;
	RendererRD::MeshStorage::NativeSurfaceHold derived_surface;

	RID fixture_mesh_rid;
	RID color_tex;
	RID depth_tex;
	RID framebuffer_rid;
	QualificationPhysicalReturn framebuffer_physical;

	RID owned_buffers[MAX_OWNED_BUFFERS];
	RID uniform_sets[3];
	RID material_uniform_set;
	RID native_shader_rid;
	RID pipeline_rid;
	RID vertex_array_rid;
	RID index_array_rid;
	RID depth_readback_ssbo;

	ProductionDisposalUse disposal_uses[MAX_DISPOSAL_USES];

	ReadbackSourceState readback_state;
	BufferUploadSourceSegment readback_segments[2];
	BufferReadbackSourceCapture readback_capture;

	uint32_t payload_bytes = 0;
	uint32_t owned_buffer_count = 0;
	uint32_t disposal_use_count = 0;
	Error recipe_error = OK;
	uint8_t push_constant[16] = {};
	bool selection_done = false;
	bool recipe_requested = false;
	bool recipe_ready = false;
	bool recipe_failed = false;
	bool recorded = false;
	bool retiring_started = false;
	bool rids_freed = false;
	bool fixture_buf_disposal_attached[5] = {};
	bool fixture_buf_settled[5] = {};
	bool fixture_uset_disposal_attached = false;
	bool fixture_mesh_settled = false;
	bool color_tex_disposal_attached = false;
	bool depth_tex_disposal_attached = false;
	bool owned_buffer_disposal_attached[MAX_OWNED_BUFFERS] = {};
	bool owned_buffer_settled[MAX_OWNED_BUFFERS] = {};
	bool uniform_set_disposal_attached[3] = {};
	bool pipeline_disposal_attached = false;
	bool readback_ssbo_disposal_attached = false;
	bool readback_prepared = false;
};

} // namespace GDExtensionDeviceExit
