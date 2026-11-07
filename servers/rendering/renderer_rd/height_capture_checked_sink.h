/**************************************************************************/
/*  height_capture_checked_sink.h                                         */
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

#ifndef HEIGHT_CAPTURE_CHECKED_SINK_H
#define HEIGHT_CAPTURE_CHECKED_SINK_H

#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"

namespace RendererRD {

// Bounded storage for the original checked RD results. This is not an operation
// issuer, completion proof, source-membership proof, or a published HeightFrame.
// Quoted targets retain their original owner; this issues no resource receipt.
// The caller must explicitly finish/unwind before releasing this object and must
// retain resources through the original native owner's retirement/close proof.
class HeightCaptureCheckedSink {
public:
	using BeginResult = RenderingDevice::DrawListBeginResult;
	using BindResult = RenderingDevice::BindAppendResult;
	using DrawResult = RenderingDevice::DrawAppendResult;
	using Transfer = RenderingDevice::NativeTransferDependency;
	using Effect = RenderingDevice::NativeAppendEffect;

	enum class Operation : uint8_t {
		Begin,
		UniformSet,
		VertexArray,
		IndexArray,
		Pipeline,
		PushConstant,
		Draw,
		End,
	};
	// Original checked attempts, including a refused operation. Only their
	// actual handles/transfers belong to the native prefix; a failed attempt
	// is never reclassified as a successful business operation.
	struct NativeRecord {
		Operation operation = Operation::Begin;
		RID resource;
		uint32_t index = 0;
		BeginResult begin;
		BindResult bind;
		DrawResult draw;
		uint32_t transfer_offset = 0;
		uint32_t transfer_count = 0;
	};

private:
	RenderingDevice &rd;
	Ref<ParticlesStorage::HeightFrameResources> quoted_resources;
	LocalVector<NativeRecord> business;
	LocalVector<Transfer> transfers;
	uint32_t business_count = 0;
	uint32_t transfer_count = 0;
	NativeRecord cleanup;
	bool cleanup_attempted = false;
	bool begin_attempted = false;
	bool business_end_attempted = false;
	bool owns_list = false;
	bool restored = false;
	bool restoration_unknown = false;
	RenderingDevice::DrawListID list = RenderingDevice::INVALID_ID;
	RID attachment;
	Error first_error = OK;

	Error fail(Error p_error);
	NativeRecord *reserve(Operation p_operation, RID p_resource = RID());
	Error accept(const BindResult &p_result);
	Error accept(const DrawResult &p_result);
	bool verify_restoration() const;

public:
	// Capacities are caller-provided finite working storage, not budget grants.
	// A separate cleanup slot is reserved here even when business storage fills.
	HeightCaptureCheckedSink(RenderingDevice &p_rd, uint32_t p_native_capacity, uint32_t p_transfer_capacity);
	HeightCaptureCheckedSink(const Ref<ParticlesStorage::HeightFrameResources> &p_resources, uint32_t p_native_capacity, uint32_t p_transfer_capacity);
	~HeightCaptureCheckedSink(); // Never secretly end a list or free RIDs.
	HeightCaptureCheckedSink(const HeightCaptureCheckedSink &) = delete;
	HeightCaptureCheckedSink &operator=(const HeightCaptureCheckedSink &) = delete;

	Error begin_quoted_target(const Ref<ParticlesStorage::HeightFrameResources> &p_resources, uint32_t p_draw_flags, VectorView<Color> p_clear_colors = {}, float p_clear_depth = 0.0f, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2(), uint32_t p_breadcrumb = 0);
	Error begin(RID p_framebuffer, uint32_t p_draw_flags, VectorView<Color> p_clear_colors = {}, float p_clear_depth = 0.0f, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2(), uint32_t p_breadcrumb = 0);
	Error bind_uniform_set(RID p_uniform_set, uint32_t p_index);
	Error bind_vertex_array(RID p_vertex_array);
	Error bind_index_array(RID p_index_array);
	Error bind_pipeline(RID p_pipeline);
	Error set_push_constant(const void *p_data, uint32_t p_size);
	Error draw_geometry_prepared(bool p_use_indices, uint32_t p_instances = 1);
	// Required also after a caller-side failure/cut; first error stays sticky.
	Error reject(Error p_error) { return fail(p_error); }
	Error finish();
	Error unwind();

	Error get_error() const { return first_error; }
	uint32_t get_business_count() const { return business_count; }
	const NativeRecord &get_business_record(uint32_t p_index) const { return business[p_index]; }
	uint32_t get_transfer_count() const { return transfer_count; }
	const Transfer &get_transfer(uint32_t p_index) const { return transfers[p_index]; }
	bool has_cleanup_attempt() const { return cleanup_attempted; }
	const NativeRecord &get_cleanup_record() const { return cleanup; }
	bool is_restored() const { return restored; }
	bool is_restoration_unknown() const { return restoration_unknown; }
	bool has_open_list() const { return owns_list; }
};

} // namespace RendererRD

#endif // HEIGHT_CAPTURE_CHECKED_SINK_H
