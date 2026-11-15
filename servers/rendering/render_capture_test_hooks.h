/**************************************************************************/
/*  render_capture_test_hooks.h                                           */
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
#include "modules/modules_enabled.gen.h"
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
#include "core/templates/rid.h"

#include <cstdint>
namespace RenderCaptureTestHooks {
enum WriteOperation : uint32_t { DRAW = 1,
	DRAW_INDIRECT,
	COMPUTE,
	COMPUTE_INDIRECT,
	RAYTRACE,
	CALLBACK };
void observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0, bool p_failed_prefix = false);
void observe_buffer_allocation(uint64_t p_rid, uint64_t p_device, uint64_t p_driver, uint64_t p_serial = 0);
void observe_buffer_free(uint64_t p_rid, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0);
void observe_buffer_dispose(uint64_t p_device, uint64_t p_driver, uint64_t p_serial);
void observe_buffer_clear(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, uint64_t p_device = 0, uint64_t p_driver = 0, uint64_t p_serial = 0);
void observe_buffer_copy(uint64_t p_src_rid, uint64_t p_dst_rid, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size, uint64_t p_device = 0, uint64_t p_source_serial = 0, uint64_t p_destination_serial = 0);
void observe_readback_queued(uint64_t p_rid, uint32_t p_size);
void observe_readback_completed(uint64_t p_rid, uint32_t p_size);
void observe_skeleton_signal(uint64_t p_object, bool p_after);
void observe_skeleton_palette_request(uint64_t p_object, uint64_t p_skeleton, uint32_t p_bind);
void observe_skeleton_setter(uint64_t p_skeleton, uint64_t p_version);
void observe_skeleton_upload(uint64_t p_skeleton, uint64_t p_buffer, uint32_t p_size, int32_t p_error);
void observe_skeleton_version(uint64_t p_skeleton, uint64_t p_version);
void observe_texture_write(uint64_t p_device, uint64_t p_driver, uint32_t p_usage, uint32_t p_operation);
void observe_driver_result(uint64_t p_device, uint32_t p_operation, uint64_t p_fence, int32_t p_error);
bool write_census_active();
void write_census_record(uint64_t p_driver, uint64_t p_device, uint64_t p_allocation, uint32_t p_usage, uint32_t p_operation);
void write_census_poison();
void observe_phase(uint64_t p_frame, bool p_after_views);
class ViewportDrawScope {
	uint64_t previous = 0;

public:
	explicit ViewportDrawScope(uint64_t p_viewport);
	~ViewportDrawScope();
};

uint64_t draw_record_count();
bool observe_draw(RID p_mesh_instance, uint32_t p_surface, RID p_vertex_array, uint64_t p_before_count, uint32_t p_renderer, uint32_t p_pass);
void retirement_pump();
} //namespace RenderCaptureTestHooks
#endif
