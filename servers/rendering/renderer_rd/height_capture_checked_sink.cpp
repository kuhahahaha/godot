/**************************************************************************/
/*  height_capture_checked_sink.cpp                                       */
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

#include "height_capture_checked_sink.h"

namespace RendererRD {

HeightCaptureCheckedSink::HeightCaptureCheckedSink(RenderingDevice &p_rd, uint32_t p_native_capacity, uint32_t p_transfer_capacity) :
		rd(p_rd) {
	business.resize(p_native_capacity);
	transfers.resize(p_transfer_capacity);
	cleanup.operation = Operation::End;
}

Error HeightCaptureCheckedSink::fail(Error p_error) {
	if (first_error == OK && p_error != OK) {
		first_error = p_error;
	}
	return first_error;
}

HeightCaptureCheckedSink::NativeRecord *HeightCaptureCheckedSink::reserve(Operation p_operation, RID p_resource) {
	if (first_error != OK) {
		return nullptr;
	}
	if (business_end_attempted || (p_operation != Operation::Begin && !owns_list)) {
		fail(ERR_INVALID_PARAMETER);
		return nullptr;
	}
	if (business_count == business.size()) {
		fail(ERR_OUT_OF_MEMORY);
		return nullptr;
	}
	NativeRecord &record = business[business_count++];
	record.operation = p_operation;
	record.resource = p_resource;
	return &record;
}

Error HeightCaptureCheckedSink::accept(const BindResult &p_result) {
	if (p_result.error != OK) {
		return fail(p_result.error);
	}
	if (p_result.effect == Effect::Unknown || !p_result.state_accepted || !p_result.capture_complete) {
		return fail(ERR_INVALID_DATA);
	}
	return first_error;
}

Error HeightCaptureCheckedSink::accept(const DrawResult &p_result) {
	if (p_result.error != OK) {
		return fail(p_result.error);
	}
	// An unchanged pipeline is an actual checked no-op with no new receipt.
	if (p_result.effect == Effect::Unknown || p_result.effect == Effect::NativePrefixOnly || (p_result.effect == Effect::Recorded && !p_result.capture_complete)) {
		return fail(ERR_INVALID_DATA);
	}
	return first_error;
}

Error HeightCaptureCheckedSink::begin(RID p_framebuffer, uint32_t p_draw_flags, VectorView<Color> p_clear_colors, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, uint32_t p_breadcrumb) {
	if (begin_attempted) {
		return fail(ERR_INVALID_PARAMETER);
	}
	NativeRecord *record = reserve(Operation::Begin, p_framebuffer);
	if (!record) {
		return first_error;
	}
	begin_attempted = true;
	const RenderingDevice::Framebuffer *framebuffer = rd.framebuffer_owner.get_or_null(p_framebuffer);
	if (framebuffer && framebuffer->texture_ids.size() == 1) {
		attachment = framebuffer->texture_ids[0];
	}
	record->begin = rd._draw_list_begin_recorded(p_framebuffer, p_draw_flags, p_clear_colors, p_clear_depth, p_clear_stencil, p_region, p_breadcrumb);
	const BeginResult &result = record->begin;
	list = result.list;
	owns_list = list != RenderingDevice::INVALID_ID;
	// A partially accepted begin with no original active-list identity cannot
	// safely be closed by guessing a list ID or resetting RD CPU state.
	if (result.effect == Effect::Unknown || (!owns_list && result.effect != Effect::NoNewEffect)) {
		restoration_unknown = true;
	}
	if (result.error != OK) {
		return fail(result.error);
	}
	if (!owns_list || !result.capture_complete || result.effect != Effect::Recorded) {
		return fail(ERR_INVALID_DATA);
	}
	return OK;
}

Error HeightCaptureCheckedSink::bind_uniform_set(RID p_uniform_set, uint32_t p_index) {
	NativeRecord *record = reserve(Operation::UniformSet, p_uniform_set);
	if (!record) {
		return first_error;
	}
	record->index = p_index;
	record->bind = rd._draw_list_bind_uniform_set_recorded(list, p_uniform_set, p_index);
	return accept(record->bind);
}

Error HeightCaptureCheckedSink::bind_vertex_array(RID p_vertex_array) {
	NativeRecord *record = reserve(Operation::VertexArray, p_vertex_array);
	if (!record) {
		return first_error;
	}
	record->transfer_offset = transfer_count;
	const uint32_t remaining = transfers.size() - transfer_count;
	record->bind = rd._draw_list_bind_vertex_array_recorded(list, p_vertex_array, remaining ? transfers.ptr() + transfer_count : nullptr, remaining);
	record->transfer_count = record->bind.transfer_count;
	transfer_count += record->bind.transfer_count;
	return accept(record->bind);
}

Error HeightCaptureCheckedSink::bind_index_array(RID p_index_array) {
	NativeRecord *record = reserve(Operation::IndexArray, p_index_array);
	if (!record) {
		return first_error;
	}
	record->transfer_offset = transfer_count;
	const uint32_t remaining = transfers.size() - transfer_count;
	record->bind = rd._draw_list_bind_index_array_recorded(list, p_index_array, remaining ? transfers.ptr() + transfer_count : nullptr, remaining);
	record->transfer_count = record->bind.transfer_count;
	transfer_count += record->bind.transfer_count;
	return accept(record->bind);
}

Error HeightCaptureCheckedSink::bind_pipeline(RID p_pipeline) {
	NativeRecord *record = reserve(Operation::Pipeline, p_pipeline);
	if (!record) {
		return first_error;
	}
	record->draw = rd._draw_list_bind_render_pipeline_recorded(list, p_pipeline);
	return accept(record->draw);
}

Error HeightCaptureCheckedSink::set_push_constant(const void *p_data, uint32_t p_size) {
	NativeRecord *record = reserve(Operation::PushConstant);
	if (!record) {
		return first_error;
	}
	record->draw = rd._draw_list_set_push_constant_recorded(list, p_data, p_size);
	return accept(record->draw);
}

Error HeightCaptureCheckedSink::draw_geometry_prepared(bool p_use_indices, uint32_t p_instances) {
	NativeRecord *record = reserve(Operation::Draw);
	if (!record) {
		return first_error;
	}
	record->draw = rd._draw_list_draw_geometry_prepared_recorded(list, p_use_indices, p_instances);
	return accept(record->draw);
}

bool HeightCaptureCheckedSink::verify_restoration() const {
	if (rd.draw_list.active || !rd.draw_list_bound_textures.is_empty()) {
		return false;
	}
	const RenderingDevice::Texture *texture = rd.texture_owner.get_or_null(attachment);
	return texture && !texture->bound;
}

Error HeightCaptureCheckedSink::finish() {
	if (first_error != OK) {
		return unwind();
	}
	if (business_end_attempted) {
		return first_error;
	}
	NativeRecord *record = reserve(Operation::End);
	if (!record) {
		return unwind();
	}
	business_end_attempted = true;
	record->draw = rd._draw_list_end_recorded();
	accept(record->draw);
	if (record->draw.effect == Effect::Unknown || record->draw.effect == Effect::NativePrefixOnly) {
		restoration_unknown = true;
		return fail(ERR_INVALID_DATA); // A possibly accepted end is never retried.
	}
	if (record->draw.effect == Effect::Recorded) {
		owns_list = false;
		restored = record->draw.capture_complete && rd.draw_graph.contains_record(record->draw.draw_handle) && verify_restoration();
		if (!restored) {
			restoration_unknown = true;
			fail(ERR_INVALID_DATA);
		}
		return first_error;
	}
	fail(ERR_INVALID_DATA);
	return unwind();
}

Error HeightCaptureCheckedSink::unwind() {
	if (!owns_list || cleanup_attempted || restoration_unknown) {
		return first_error;
	}
	// Explicit unwind without a prior failure is an abandoned business capture.
	fail(ERR_SKIP);
	cleanup_attempted = true;
	cleanup.draw = rd._draw_list_end_recorded();
	const DrawResult &result = cleanup.draw;
	if (result.effect == Effect::Recorded) {
		owns_list = false;
	}
	restored = result.effect == Effect::Recorded && result.capture_complete && rd.draw_graph.contains_record(result.draw_handle) && verify_restoration();
	if (!restored) {
		restoration_unknown = true; // Original owner must quarantine this session.
	}
	return first_error;
}

} // namespace RendererRD
