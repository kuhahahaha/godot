/**************************************************************************/
/*  external_output_packet.cpp                                            */
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

#include "external_output_packet.h"
#include "core/extension/gdextension_production_request.h"

#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"

namespace RendererRD {
ExternalSurfaceSelection::ExternalSurfaceSelection(ExternalSurfaceSelection &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->surface_move(*this, p_other);
	}
}
ExternalSurfaceSelection &ExternalSurfaceSelection::operator=(ExternalSurfaceSelection &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->surface_move(*this, p_other);
		}
	}
	return *this;
}
ExternalSurfaceSelection::~ExternalSurfaceSelection() {
	CRASH_COND(owner);
}

ExternalPacketViewLease::ExternalPacketViewLease(ExternalPacketViewLease &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->view_move(*this, p_other);
	}
}
ExternalPacketViewLease &ExternalPacketViewLease::operator=(ExternalPacketViewLease &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->view_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketViewLease::~ExternalPacketViewLease() {
	CRASH_COND(owner);
}
Error ExternalPacketViewLease::release_checked() {
	return owner ? owner->view_return(*this) : ERR_DOES_NOT_EXIST;
}
ExternalPacketBinding::ExternalPacketBinding(ExternalPacketBinding &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->read_move(*this, p_other);
	}
}
ExternalPacketBinding &ExternalPacketBinding::operator=(ExternalPacketBinding &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->read_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketBinding::~ExternalPacketBinding() {
	CRASH_COND(owner);
}
Error ExternalPacketBinding::cancel_before_draw() {
	return owner ? owner->read_return(*this, false) : ERR_DOES_NOT_EXIST;
}
Error ExternalPacketBinding::record_checked(RenderingDevice::DrawListID) {
	// The original typed read acquisition and checked native draw join are
	// absent. Refuse before touching RD; caller still explicitly returns borrow.
	return ERR_UNAVAILABLE;
}
} // namespace RendererRD

namespace RendererRD {
ExternalPipelinePreparation::~ExternalPipelinePreparation() {
	CRASH_COND(pipeline_scope || pipeline_pin || pipeline_original_root || pipeline_owned_group || geometry[0].is_active() || geometry[1].is_active() || material.is_active());
}
Error ExternalPipelinePreparation::_pipeline_preflight() {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !pipeline_pin || !pipeline_owned_group || !pipeline_original_root) {
		return ERR_UNCONFIGURED;
	}
	// A row with a scope never falls back to an ambient or expired context.
	if (pipeline_scope && (pipeline_scope->row != this || source_sealed || pipeline_scope->pin != pipeline_pin ||
			pipeline_scope->original_root != pipeline_original_root || pipeline_scope->group != pipeline_owned_group)) {
		return ERR_UNAUTHORIZED;
	}
	const Error error = pipeline_scope ? rd->_production_output_owned_pipeline_preflight(*pipeline_scope) :
			rd->_production_output_owned_pipeline_preflight(pipeline_context, *pipeline_pin, pipeline_owned_group);
	if (error != OK) {
		return error;
	}
	return pipeline_original_root->is_valid() ? ERR_ALREADY_IN_USE : OK;
}
Error ExternalPipelinePreparation::_cleanup_preflight() {
	auto *rd = RenderingDevice::get_singleton();
	if (!rd || !pipeline_scope || pipeline_scope->row != this || !pipeline_scope->registry) {
		return ERR_UNAUTHORIZED;
	}
	auto &registry = *pipeline_scope->registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	return registry.production_output_preparation_cleanup_matches_locked(*pipeline_scope, *rd, request);
}
RID ExternalPipelinePreparation::_create_pipeline(RID p_shader, RenderingDevice::FramebufferFormatID p_framebuffer_format, RenderingDevice::VertexFormatID p_vertex_format, RenderingDevice::RenderPrimitive p_render_primitive, const RenderingDevice::PipelineRasterizationState &p_rasterization_state, const RenderingDevice::PipelineMultisampleState &p_multisample_state, const RenderingDevice::PipelineDepthStencilState &p_depth_stencil_state, const RenderingDevice::PipelineColorBlendState &p_blend_state, BitField<RenderingDevice::PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<RenderingDevice::PipelineSpecializationConstant> &p_specialization_constants) {
	if (!pipeline_pin || !pipeline_owned_group || !pipeline_original_root || pipeline_original_root->is_valid()) {
		last_error = ERR_UNCONFIGURED;
		return RID();
	}
	auto *rd = RenderingDevice::get_singleton();
	if (!rd) {
		last_error = ERR_UNCONFIGURED;
		return RID();
	}
	if (pipeline_scope && (pipeline_scope->row != this || source_sealed || pipeline_scope->pin != pipeline_pin ||
			pipeline_scope->original_root != pipeline_original_root || pipeline_scope->group != pipeline_owned_group)) {
		last_error = ERR_UNAUTHORIZED;
		return RID();
	}
	last_error = pipeline_scope ? rd->_production_output_owned_render_pipeline_create(*pipeline_scope, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, *pipeline_original_root) :
			rd->_production_output_owned_render_pipeline_create(pipeline_context, *pipeline_pin, pipeline_owned_group, 0, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, *pipeline_original_root);
	created_pipeline = *pipeline_original_root; // Original surface retains it even on a later leaf failure.
	return created_pipeline;
}
} // namespace RendererRD
