/**************************************************************************/
/*  native_pipeline_factory_rd.h                                                */
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

#include "servers/rendering/rendering_device.h"

namespace RendererRD {
// Synchronous borrowed values. Neither this DTO nor its sink grants factory
// admission. The original owner validates and retains every native prefix.
struct NativePipelineFactoryInput {
	RID shader;
	RenderingDevice::FramebufferFormatID framebuffer_format;
	RenderingDevice::VertexFormatID vertex_format;
	RenderingDevice::RenderPrimitive primitive;
	const RenderingDevice::PipelineRasterizationState &raster;
	const RenderingDevice::PipelineMultisampleState &multisample;
	const RenderingDevice::PipelineDepthStencilState &depth_stencil;
	const RenderingDevice::PipelineColorBlendState &blend;
	BitField<RenderingDevice::PipelineDynamicStateFlags> dynamic_state;
	uint32_t render_pass;
	const Vector<RenderingDevice::PipelineSpecializationConstant> &specialization;
};

// Core-internal factory injection only; no token, registry or retained callback.
// The callback must publish directly into its original paid destination and
// retain failure prefixes there. It owns factory_error after entry. A null
// RID with OK is normalized to ERR_CANT_CREATE; a valid root is preserved.
struct NativePipelineFactorySink {
	void *owner = nullptr;
	RID (*create)(void *, const NativePipelineFactoryInput &, Error &factory_error) = nullptr;
};
} // namespace RendererRD
