/**************************************************************************/
/*  rendering_device_immutable_hold.cpp                                   */
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

#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device.h"

#ifdef TESTS_ENABLED
#include <cstdio>
#endif

#include <mutex>

RenderingDevice::Buffer *RenderingDevice::_native_immutable_buffer(RID rid) {
	// A foreign owner may have a pending allocation at the same slot index.
	// Check the full RID before lookup so only its actual owner checks readiness.
	if (auto *buffer = vertex_buffer_owner.owns(rid) ? vertex_buffer_owner.get_or_null(rid) : nullptr) {
		return buffer;
	}
	if (auto *buffer = index_buffer_owner.owns(rid) ? index_buffer_owner.get_or_null(rid) : nullptr) {
		return buffer;
	}
	if (auto *buffer = uniform_buffer_owner.owns(rid) ? uniform_buffer_owner.get_or_null(rid) : nullptr) {
		return buffer;
	}
	if (auto *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr) {
		return buffer;
	}
	return texture_buffer_owner.owns(rid) ? texture_buffer_owner.get_or_null(rid) : nullptr;
}

GDExtensionDeviceExit::ProductionDisposalUse **RenderingDevice::_production_disposal_head(RID rid, uint32_t &kind, uint64_t &serial, uint64_t &driver_id) {
	serial = rid.get_id();
	driver_id = 0;
	kind = UINT32_MAX;
	if (auto *p = _native_immutable_buffer(rid)) {
		kind = 9;
		serial = p->backing_serial ? p->backing_serial : rid.get_id();
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr) {
		kind = 8;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr) {
		kind = 6;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr) {
		kind = 5;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr) {
		kind = 4;
		serial = p->native_disposal_serial ? p->native_disposal_serial : rid.get_id();
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr) {
		kind = 0;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr) {
		kind = 1;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	if (auto *p = vertex_array_owner.owns(rid) ? vertex_array_owner.get_or_null(rid) : nullptr) {
		kind = 10;
		return &p->production_disposal_uses;
	}
	if (auto *p = index_array_owner.owns(rid) ? index_array_owner.get_or_null(rid) : nullptr) {
		kind = 11;
		driver_id = p->driver_id.id;
		return &p->production_disposal_uses;
	}
	return nullptr;
}
bool RenderingDevice::_production_disposal_belongs(RID rid, const GDExtensionDeviceExit::ProductionRequest *owner) {
	if (!owner) {
		return false;
	}
	uint32_t kind;
	uint64_t serial, driver_id;
	auto **head = _production_disposal_head(rid, kind, serial, driver_id);
	if (!head) {
		return false;
	}
	for (auto *n = *head; n; n = n->next) {
		if (n->request == owner && n->request_serial == owner->serial && n->rid == rid.get_id() && n->kind == kind && n->serial == serial && n->native_driver_id == driver_id && n->state.load(std::memory_order_acquire) == 1) {
			return true;
		}
	}
	return false;
}
void RenderingDevice::_production_disposal_enqueued(GDExtensionDeviceExit::ProductionDisposalUse *&head) {
	for (auto *p = head; p; p = p->next) {
		uint8_t expected = 1;
		if (!p->state.compare_exchange_strong(expected, 2, std::memory_order_release, std::memory_order_relaxed)) {
			p->state.store(5, std::memory_order_release);
		}
	}
}
void RenderingDevice::_production_disposal_returned(GDExtensionDeviceExit::ProductionDisposalUse *&head) {
	auto *p = head;
	head = nullptr;
	while (p) {
		auto *next = p->next;
		const uint8_t old = p->state.load(std::memory_order_acquire);
		p->state.store(old == 2 ? (p->kind >= 10 ? 7 : 3) : 5, std::memory_order_release);
		p = next;
	} // No access after publishing final return.
}

void RenderingDevice::_native_immutable_retire_dependents(RID rid) {
	// Original dependency objects remain physical owners while pinned. Consume
	// their original logical invalidation callbacks now, not after userdata died.
	LocalVector<RID> retiring;
	retiring.push_back(rid);
	for (uint32_t i = 0; i < retiring.size(); ++i) {
		MutexLock lock(_thread_safe_);
		const RID current = retiring[i];
		if (production_disposal_drop_owner && (!_production_disposal_belongs(current, production_disposal_drop_owner) || !_production_output_callback_supported(current))) {
			return; // Keep the exact retiring prefix; no foreign callback is invoked.
		}

		const auto *buffer = _native_immutable_buffer(current);
		const auto *texture = texture_owner.owns(current) ? texture_owner.get_or_null(current) : nullptr;
		const auto *shader = shader_owner.owns(current) ? shader_owner.get_or_null(current) : nullptr;
		const auto *pipeline = compute_pipeline_owner.owns(current) ? compute_pipeline_owner.get_or_null(current) : nullptr;
		const auto *sampler = sampler_owner.owns(current) ? sampler_owner.get_or_null(current) : nullptr;
		auto *set = uniform_set_owner.owns(current) ? uniform_set_owner.get_or_null(current) : nullptr;
		if (!qualification_retiring && ((buffer && buffer->qualification_return) || (texture && texture->qualification_return) || (shader && shader->qualification_return) || (pipeline && pipeline->qualification_return) || (sampler && sampler->qualification_return) || (set && set->qualification_return))) {
			continue;
		}
		bool *pending = nullptr;
		if (_native_immutable_counter(current, &pending)) {
			*pending = true;
		}
		if (const auto *children = dependency_map.getptr(current)) {
			for (RID child : *children) {
				if (!retiring.has(child)) {
					retiring.push_back(child);
				}
			}
		}
		if (set && set->invalidated_callback) {
			const auto callback = set->invalidated_callback;
			void *userdata = set->invalidated_callback_userdata;
			set->invalidated_callback = nullptr;
			set->invalidated_callback_userdata = nullptr;
			// Consume just this live callback, then run it synchronously without
			// an RD lock. A callback may clear/destroy another dependency's owner.
			lock.temp_unlock();
			callback(userdata);
			lock.temp_relock();
		}
	}
}

uint32_t *RenderingDevice::_native_immutable_counter(RID rid, bool **retiring, bool **escaped) {
	auto counter = [&](auto *resource) -> uint32_t * {
		if (retiring) {
			*retiring = &resource->native_immutable_retire_requested;
		}
		if (escaped) {
			*escaped = &resource->native_external_access;
		}
		return &resource->native_immutable_holds;
	};
	if (auto *resource = _native_immutable_buffer(rid)) {
		return counter(resource);
	}
	if (auto *resource = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = vertex_array_owner.owns(rid) ? vertex_array_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	if (auto *resource = index_array_owner.owns(rid) ? index_array_owner.get_or_null(rid) : nullptr) {
		return counter(resource);
	}
	return nullptr;
}

bool RenderingDevice::native_immutable_is_render_thread() const {
	return render_thread_id == Thread::get_caller_id();
}

bool RenderingDevice::native_immutable_resource_is_held(RID rid) const {
	MutexLock lock(_thread_safe_);
	const auto *holds = const_cast<RenderingDevice *>(this)->_native_immutable_counter(rid);
	if (holds && *holds) {
		return true;
	}
	const auto *texture = const_cast<RenderingDevice *>(this)->texture_owner.owns(rid) ? const_cast<RenderingDevice *>(this)->texture_owner.get_or_null(rid) : nullptr;
	return texture && texture->owner.is_valid() && native_immutable_resource_is_held(texture->owner);
}

bool RenderingDevice::native_immutable_resource_is_retiring(RID rid) const {
	MutexLock lock(_thread_safe_);
	bool *retiring = nullptr;
	const auto *holds = const_cast<RenderingDevice *>(this)->_native_immutable_counter(rid, &retiring);
	if (holds && *retiring) {
		return true;
	}
	const auto *texture = const_cast<RenderingDevice *>(this)->texture_owner.owns(rid) ? const_cast<RenderingDevice *>(this)->texture_owner.get_or_null(rid) : nullptr;
	return texture && texture->owner.is_valid() && native_immutable_resource_is_retiring(texture->owner);
}

bool RenderingDevice::native_immutable_pipeline_matches_shader(RID pipeline, RID shader) const {
	MutexLock lock(_thread_safe_);
	const auto *original = const_cast<RenderingDevice *>(this)->shader_owner.get_or_null(shader);
	if (!original || !original->driver_id) {
		return false;
	}
	if (const auto *physical = const_cast<RenderingDevice *>(this)->render_pipeline_owner.owns(pipeline) ? const_cast<RenderingDevice *>(this)->render_pipeline_owner.get_or_null(pipeline) : nullptr) {
		return physical->shader == shader && physical->shader_driver_id == original->driver_id;
	}
	if (const auto *physical = const_cast<RenderingDevice *>(this)->compute_pipeline_owner.owns(pipeline) ? const_cast<RenderingDevice *>(this)->compute_pipeline_owner.get_or_null(pipeline) : nullptr) {
		return physical->shader == shader && physical->shader_driver_id == original->driver_id;
	}
	return false;
}

RenderingDevice::NativeRenderPipelineShaderShape RenderingDevice::_native_render_pipeline_shader_shape(const Shader &shader) {
	if (shader.pipeline_type != PIPELINE_TYPE_RASTERIZATION) {
		return NativeRenderPipelineShaderShape::NOT_RASTERIZATION;
	}
	return shader.stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT) ? NativeRenderPipelineShaderShape::SUPPORTED : NativeRenderPipelineShaderShape::MISSING_VERTEX;
}

bool RenderingDevice::_native_immutable_uniform_set_matches_formats_locked(RID set_rid, const Vector<uint32_t> &formats, uint32_t set_index) const {
	const uint32_t expected = set_index < uint32_t(formats.size()) ? formats[set_index] : 0;
	if (set_rid.is_null()) {
		return expected == 0;
	}
	const auto *set = const_cast<RenderingDevice *>(this)->uniform_set_owner.get_or_null(set_rid);
	return set && set->driver_id && set->format && set->format == expected;
}

bool RenderingDevice::_native_immutable_uniform_set_matches_future_render_pipeline(RID set_rid, RID shader_rid, uint32_t set_index) const {
	MutexLock lock(_thread_safe_);
	const auto *shader = const_cast<RenderingDevice *>(this)->shader_owner.get_or_null(shader_rid);
	// Real render factory copies this exact Shader owner's set_formats without
	// modifying them for specialization, framebuffer, vertex format or pass.
	return shader && shader->driver_id && _native_render_pipeline_shader_shape(*shader) == NativeRenderPipelineShaderShape::SUPPORTED &&
			_native_immutable_uniform_set_matches_formats_locked(set_rid, shader->set_formats, set_index);
}

bool RenderingDevice::native_immutable_uniform_set_matches_pipeline(RID set_rid, RID pipeline_rid, uint32_t set_index) const {
	MutexLock lock(_thread_safe_);
	auto *self = const_cast<RenderingDevice *>(this);
	const Vector<uint32_t> *formats = nullptr;
	if (const auto *pipeline = self->render_pipeline_owner.owns(pipeline_rid) ? self->render_pipeline_owner.get_or_null(pipeline_rid) : nullptr; pipeline && pipeline->driver_id) {
		formats = &pipeline->set_formats;
	}
	if (const auto *pipeline = self->compute_pipeline_owner.owns(pipeline_rid) ? self->compute_pipeline_owner.get_or_null(pipeline_rid) : nullptr; pipeline && pipeline->driver_id) {
		formats = &pipeline->set_formats;
	}
	if (!formats) {
		return false;
	}
	return _native_immutable_uniform_set_matches_formats_locked(set_rid, *formats, set_index);
}

bool RenderingDevice::_native_immutable_uniform_set_writes(const UniformSet &set) const {
	for (RID rid : set.writable_resources) {
		if (native_immutable_resource_is_held(rid) || native_immutable_resource_is_retiring(rid)) {
			return true;
		}
	}
	return false;
}

Error RenderingDevice::_native_immutable_hold_capacity_locked(uint32_t increments) const {
	return native_immutable_hold_count > UINT32_MAX - increments ? ERR_PARAMETER_RANGE_ERROR : OK;
}

Error RenderingDevice::_native_immutable_resource_eligibility_locked(RID rid, uint32_t increments) {
	if (!increments) {
		return ERR_INVALID_PARAMETER;
	}
	bool *retiring = nullptr, *escaped = nullptr;
	const auto *holds = _native_immutable_counter(rid, &retiring, &escaped);
	if (!holds) {
		return ERR_DOES_NOT_EXIST;
	}
	if (*escaped) {
		return ERR_UNAVAILABLE;
	}
	if (*retiring || native_immutable_resource_is_retiring(rid)) {
		return ERR_BUSY;
	}
	if (*holds > UINT32_MAX - increments) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (const auto *resource = _native_immutable_buffer(rid); resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr; resource && !resource->writable_resources.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *resource = index_array_owner.owns(rid) ? index_array_owner.get_or_null(rid) : nullptr; resource && !resource->driver_id) {
		return ERR_UNAVAILABLE;
	}

	if (const auto *buffer = _native_immutable_buffer(rid); buffer && buffer->qualification_return && buffer->qualification_return->use_closed.load()) {
		return ERR_BUSY;
	}
	if (const auto *buffer = _native_immutable_buffer(rid); buffer && (buffer->transfer_worker_index >= 0 || buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT) || buffer->usage.has_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT))) {
		return ERR_UNAVAILABLE;
	}
	if (const auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr) {
		// Render-target ownership and dynamic framebuffer writes need their
		// original snapshot producer; this retention leaf cannot freeze them.
		if (texture->transfer_worker_index >= 0 || texture->pending_clear || (texture->usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) || (texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) || (texture->usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT)) {
			return ERR_UNAVAILABLE;
		}
		if (texture->shared_fallback && texture->owner.is_valid()) {
			const auto *owner = texture_owner.get_or_null(texture->owner);
			if (!owner || !owner->shared_fallback || owner->shared_fallback->revision != texture->shared_fallback->revision) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	return OK;
}

template <typename Visitor>
Error RenderingDevice::_native_immutable_visit_related_locked(RID rid, NativeClosureMode mode, Visitor &&visit) {
	const auto *dependents = mode != NativeClosureMode::DEPENDENCIES ? dependency_map.getptr(rid) : nullptr;
	const auto *dependencies = mode != NativeClosureMode::MATERIAL_DISPOSAL ? reverse_dependency_map.getptr(rid) : nullptr;
	for (const auto *edges : { dependents, dependencies }) {
		if (edges) {
			for (RID related : *edges) {
				const Error error = visit(related);
				if (error != OK) {
					return error;
				}
			}
		}
	}
	if (mode == NativeClosureMode::MATERIAL_DISPOSAL) {
		return OK;
	}
	if (const auto *shader = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr) {
		for (RID sampler : shader->native_immutable_samplers) {
			const Error error = visit(sampler);
			if (error != OK) {
				return error;
			}
		}
	}
	if (const auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr; texture && texture->owner.is_valid()) {
		return visit(texture->owner);
	}
	return OK;
}

Error RenderingDevice::native_immutable_hold_acquire(const VectorView<RID> &roots, NativeImmutableHold &out, uint32_t limit) {
	if (render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (!driver || native_immutable_device_escaped) {
		return ERR_UNAVAILABLE;
	}
	if (out.device || roots.size() == 0 || !limit || limit > 1024) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(_thread_safe_);
	if (draw_list.active || compute_list.active || raytracing_list.active || draw_graph.has_driver_callbacks() || production_finalization_started.load(std::memory_order_acquire)) {
		return ERR_BUSY;
	}
	const Error capacity = _native_immutable_hold_capacity_locked(1);
	if (capacity != OK) {
		return capacity;
	}
	RID inline_resources[3];
	Vector<RID> dynamic_resources;
	const bool use_inline = limit <= 3;
	if (!use_inline && dynamic_resources.resize(limit) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	RID *resources = use_inline ? inline_resources : dynamic_resources.ptrw();
	uint32_t count = 0;
	auto append = [&](RID rid) {
		for (uint32_t i = 0; i < count; ++i) {
			if (resources[i] == rid) {
				return OK;
			}
		}
		if (count == limit) {
			return ERR_OUT_OF_MEMORY;
		}
		const Error eligible = _native_immutable_resource_eligibility_locked(rid);
		if (eligible != OK) {
			return eligible;
		}
		resources[count++] = rid;
		return OK;
	};
	for (uint32_t i = 0; i < roots.size(); ++i) {
		const Error error = append(roots[i]);
		if (error != OK) {
			return error;
		}
	}
	for (uint32_t i = 0; i < count; ++i) {
		const Error error = _native_immutable_visit_related_locked(resources[i], NativeClosureMode::DEPENDENCIES, append);
		if (error != OK) {
			return error;
		}
	}
	if (!use_inline && dynamic_resources.resize(count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	if (use_inline) {
		for (uint32_t i = 0; i < count; ++i) {
			out.inline_resources[i] = inline_resources[i];
		}
		out.inline_count = count;
	} else {
		out.resources = static_cast<Vector<RID> &&>(dynamic_resources);
	}
	for (RID rid : out._resource_span()) {
		++*_native_immutable_counter(rid);
	}
	out.device = this;
	out.pins_returned = false;
	++native_immutable_hold_count;
	return OK;
}

Error RenderingDevice::native_immutable_hold_release(NativeImmutableHold &hold) {
	if (render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (hold.device != this) {
		return ERR_INVALID_PARAMETER;
	}
	if (draw_list.active || compute_list.active || raytracing_list.active) {
		return ERR_BUSY;
	}
	MutexLock lock(_thread_safe_);
	if (hold.pipeline_borrowers) {
		return ERR_BUSY; // No counter, resource, origin or hold identity changes.
	}
	if (!hold.pins_returned) {
		for (RID rid : hold._resource_span()) {
			auto *holds = _native_immutable_counter(rid);
			CRASH_COND(!holds || !*holds);
			--*holds;
		}
		hold.pins_returned = true;
	}
	bool pending = false;
	const auto resources = hold._resource_span();
	for (int64_t i = int64_t(resources.size()) - 1; i >= 0; --i) {
		const RID rid = resources[i];
		bool *retiring = nullptr;
		const auto *holds = _native_immutable_counter(rid, &retiring);
		if (!holds || *holds || !*retiring) {
			continue;
		}
		if (native_immutable_free_frame) {
			// The original free call is still on stack. It owns callback return
			// and physical retirement; this is not a new persistent work queue.
			if (!native_immutable_free_frame->returned.has(rid)) {
				native_immutable_free_frame->returned.push_back(rid);
			}
			continue;
		}
		lock.temp_unlock();
		free_rid(rid);
		lock.temp_relock();
		if (_native_immutable_counter(rid)) {
			pending = true;
		}
	}
	if (pending) {
		return ERR_BUSY;
	}
	CRASH_COND(!native_immutable_hold_count);
	--native_immutable_hold_count;
	hold._clear_resources();
	hold.material_origin = RID();
	hold.material_origin_generation = 0;
	hold.device = nullptr;
	hold.pins_returned = false;
	return OK;
}

Error RenderingDevice::_native_immutable_hold_expand_directory(const VectorView<RID> &roots, NativeImmutableHold &hold, uint32_t limit) {
	if (!native_immutable_is_render_thread() || hold.device != this || hold.pins_returned || hold.pipeline_borrowers || roots.size() == 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	NativeImmutableHold complete;
	const Error acquired = native_immutable_hold_acquire(roots, complete, limit);
	if (acquired != OK) {
		return acquired;
	}
	for (RID selected : hold._resource_span()) {
		bool present = false;
		for (RID retained : complete._resource_span()) {
			present |= selected == retained;
		}
		if (!present) {
			// Continuous RD lock: acquire rejected retiring roots, so rollback
			// only removes the new shares and cannot invoke native callbacks.
			CRASH_COND(native_immutable_hold_release(complete) != OK);
			return ERR_INVALID_DATA;
		}
	}
	// Every selected share is still retained by complete, so returning the old
	// hold cannot perform native disposal or leave a partial release prefix.
	CRASH_COND(native_immutable_hold_release(hold) != OK);
	hold = std::move(complete);
	return OK;
}

Error RenderingDevice::_production_output_directory_bind_adopted_locked(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial, const VectorView<RID> &roots, NativeOutputGroupBinding &binding) {
	using namespace GDExtensionDeviceExit;
	if (!native_immutable_is_render_thread() || !pin.registry || !serial || !binding.is_empty() || roots.size() == 0 ||
			!pin.registry->production_output_pin_matches_locked(pin, { pin.registry->instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	const auto &request = *pin.request;
	const uint32_t begin = request.use_count + request.binding_used;
	bool captured = false;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_serial == serial && cell.disposal_group_pin_serial == pin.serial &&
				cell.disposal_group_native_hold && !cell.disposal_group_release && !cell.disposal_group_dropped) {
			captured = true;
		}
	}
	if (!captured) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t root_index = 0; root_index < roots.size(); ++root_index) {
		const RID root = roots[root_index];
		bool found = false;
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			const auto &cell = request.uses[i];
			found |= cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && cell.disposal_group_serial == serial &&
					cell.disposal.request == &request && cell.disposal.request_serial == request.serial && cell.disposal.rid == root.get_id() &&
					cell.disposal.state.load(std::memory_order_acquire) == 1 && !cell.disposal.counter_dropped;
		}
		if (!found) {
			return ERR_INVALID_DATA;
		}
	}
	binding.registry_instance = pin.registry->instance;
	binding.request_serial = pin.request_serial;
	binding.pin_serial = pin.serial;
	binding.group_serial = serial;
	return OK;
}

Error RenderingDevice::_production_output_directory_capture(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, const VectorView<RID> &roots, uint32_t limit, NativeOutputGroupBinding &binding) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !binding.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock registry_lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *actual_request = nullptr;
	RenderingDevice *actual_device = nullptr;
	const Error source = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, actual_request, actual_device, true);
	if (source != OK || actual_request != pin.request || actual_device != this) {
		return source == OK ? ERR_UNAUTHORIZED : source;
	}
	// Registry-held RD acquisition must never block. Registry is binary;
	// locked helpers retain that original lock, while RD alone is recursive.
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	NativeImmutableHold physical;
	const Error acquired = native_immutable_hold_acquire(roots, physical, limit);
	if (acquired != OK) {
		return acquired;
	}
	uint64_t group = 0;
	const Error captured = _production_output_hold_capture_locked(physical_context, pin, physical, group);
	if (captured != OK) {
		// Acquisition rejected every retiring root. The continuous RD lock and
		// callback-free capture prevent any new retirement before rollback, so
		// this returns only the just-added shares and invokes no native cleanup.
		const Error released = native_immutable_hold_release(physical);
		CRASH_COND(released != OK);
		return captured;
	}
	binding.registry_instance = registry.instance;
	binding.request_serial = pin.request_serial;
	binding.pin_serial = pin.serial;
	binding.group_serial = group;
	return OK;
}

Error RenderingDevice::_production_output_directory_return(GDExtensionDeviceExit::ProductionOutputPin &pin, NativeOutputGroupBinding &binding, const GDExtensionProductionServiceBudgetV1 &budget, GDExtensionDeviceExit::ProductionServiceUsage &usage) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || binding.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock registry_lock(registry);
	if (binding.registry_instance != registry.instance || binding.request_serial != pin.request_serial || binding.pin_serial != pin.serial || !registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	auto &control = request.control;
	if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE || control.input.request_serial != request.serial || &budget != &control.input.budget || &usage != &control.output.service_usage) {
		return ERR_UNAUTHORIZED;
	}
	if (!budget.controls || !budget.passes || !budget.receipts || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.source_device_lost) {
		return ERR_BUSY;
	}
	const uint32_t begin = request.use_count + request.binding_used;
	ProductionUseCell *group = nullptr;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_serial == binding.group_serial && cell.disposal_group_pin_serial == pin.serial) {
			group = &cell;
			break;
		}
	}
	if (!group) {
		return ERR_INVALID_DATA;
	}
	if (!group->disposal_group_release) {
		if (request.capture_inflight != 1) {
			return ERR_BUSY;
		}
		GDExtensionProductionReportV2 source = {};
		const Error projected = registry.production_report_contents_locked(request, source);
		if (projected != OK) {
			return projected;
		}
		// The one original executor capture pin retains this boundary; another
		// callback/capture cannot begin a second return on top of it.
		if (request.capture_inflight != 1 || request.compute_list_open || (!request.cancelled && request.next_action != request.action_count) || source.possible_obligations || source.known_obligations != source.complete_obligations) {
			return ERR_BUSY;
		}
		if (!_thread_safe_.try_lock()) {
			return ERR_BUSY;
		}
		struct Unlock {
			Mutex &mutex;
			~Unlock() { mutex.unlock(); }
		} unlock{ _thread_safe_ };
		for (uint32_t command = 0; command < draw_graph.qualification_command_count(); ++command) {
			bool captured = false;
			for (uint32_t i = 0; i < request.graph_count; ++i) {
				captured |= registry.production_output_graph_command_locked(request, *this, i, command);
			}
			if (!captured) {
				return ERR_BUSY;
			}
		}
		if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
			return ERR_BUSY;
		}
		++usage.passes;
		usage.controls = 1;
		group->disposal_group_release = true;
	}
	ProductionDisposalObservation observation;
	const Error observed = _production_output_hold_returned_locked(pin, binding.group_serial, observation);
	if (observed != OK || !observation.settled) {
		return observed == OK ? ERR_BUSY : observed;
	}
	if (usage.passes >= MIN(budget.passes, 64u) || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return ERR_BUSY;
	}
	++usage.passes;
	usage.controls = 1;
	binding.registry_instance = binding.request_serial = binding.pin_serial = binding.group_serial = 0;
	return OK;
}

Error RenderingDevice::_production_output_directory_mark_root(GDExtensionDeviceExit::ProductionOutputPin &pin, const NativeOutputGroupBinding &binding, RID rid, bool owned, const GDExtensionProductionServiceBudgetV1 &budget, GDExtensionDeviceExit::ProductionServiceUsage &usage) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || binding.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock registry_lock(registry);
	if (binding.registry_instance != registry.instance || binding.request_serial != pin.request_serial || binding.pin_serial != pin.serial || !registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	auto &control = request.control;
	// A copied budget or a separate usage object cannot authorize another pass.
	if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE || control.input.request_serial != request.serial || &budget != &control.input.budget || &usage != &control.output.service_usage) {
		return ERR_UNAUTHORIZED;
	}
	if (!budget.controls || !budget.passes || !budget.receipts || usage.passes >= MIN(budget.passes, 64u) || usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.source_device_lost) {
		return ERR_BUSY;
	}
	if (rid.is_null()) {
		if (request.capture_inflight != 1 || request.compute_list_open || (!request.cancelled && request.next_action != request.action_count)) {
			return ERR_BUSY;
		}
		GDExtensionProductionReportV2 source = {};
		const Error projected = registry.production_report_contents_locked(request, source);
		if (projected != OK) {
			return projected;
		}
		if (source.possible_obligations || source.known_obligations != source.complete_obligations) {
			return ERR_BUSY;
		}
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	if (native_disposal_budget || native_disposal_active || draw_list.active || compute_list.active || raytracing_list.active) {
		return ERR_BUSY;
	}
	const uint32_t begin = request.use_count + request.binding_used;
	bool original_group = false;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_serial == binding.group_serial && cell.disposal_group_pin_serial == pin.serial && !cell.disposal_group_release && !cell.disposal_group_dropped) {
			original_group = true;
		}
	}
	if (!original_group) {
		return ERR_UNAUTHORIZED;
	}
	bool *retiring = nullptr;
	if (rid.is_valid()) {
		uint32_t kind;
		uint64_t serial, driver_id;
		auto **head = _production_disposal_head(rid, kind, serial, driver_id);
		const auto *counter = _native_immutable_counter(rid, &retiring);
		if (!head || !counter || !*counter || !retiring) {
			return ERR_INVALID_DATA;
		}
		bool original_member = false;
		for (const auto *node = *head; node; node = node->next) {
			original_member |= node->request == &request && node->request_serial == request.serial && node->group == binding.group_serial && node->rid == rid.get_id() && node->kind == kind && node->serial == serial && node->native_driver_id == driver_id && node->state.load(std::memory_order_acquire) == 1 && !node->counter_dropped && (!owned || !node->qualified_borrow);
		}
		if (!original_member || (owned && !_production_output_callback_supported(rid))) {
			return ERR_UNAVAILABLE;
		}
	}
	if (OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
		return ERR_BUSY;
	}
	// Each actual directory ordinal consumes one admitted work pass, including
	// null/borrowed entries. Only a genuine owned root changes its retire flag.
	// No traversal, callback, native free or return publication happens here.
	++usage.passes;
	usage.controls = 1;
	if (rid.is_valid() && owned) {
		*retiring = true;
	}
	return OK;
}

Error RenderingDevice::_production_output_hold_capture(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, NativeImmutableHold &hold, uint64_t &group_serial) {
	if (!pin.registry) {
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionDeviceExit::Registry::Lock registry_lock(*pin.registry);
	return _production_output_hold_capture_locked(physical_context, pin, hold, group_serial);
}

Error RenderingDevice::_production_output_hold_capture_locked(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, NativeImmutableHold &hold, uint64_t &group_serial) {
	using namespace GDExtensionDeviceExit;
	if (group_serial || hold.device != this || hold.pins_returned || hold.pipeline_borrowers || hold._resource_span().is_empty() || !pin.registry) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *physical_request = nullptr;
	RenderingDevice *physical_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, physical_request, physical_device, true);
	if (admitted != OK || physical_request != pin.request || physical_device != this) {
		return admitted == OK ? ERR_UNAUTHORIZED : admitted;
	}
	auto &request = *pin.request;
	if (!request.frozen || request.next_action || request.graph_count || request.native_count || !request.next_disposal_group_serial || request.capture_inflight) {
		return ERR_BUSY;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &m;
		~Unlock() { m.unlock(); }
	} unlock{ _thread_safe_ };
	NativeImmutableHold *holds[] = { &hold };
	return _production_output_hold_adopt_locked(request, pin.serial, 0, VectorView<NativeImmutableHold *>(holds, 1), request.capacity.resource_uses, group_serial);
}

Error RenderingDevice::_native_prospective_closure_keys_locked(const VectorView<NativeProspectiveKey> &roots, const VectorView<NativeProspectiveSymbol> &symbols, uint32_t limit, NativeClosureMode mode, Vector<NativeProspectiveKey> &out) {
	if (!roots.size() || !roots.ptr() || (symbols.size() && !symbols.ptr()) || !limit) {
		return ERR_INVALID_PARAMETER;
	}
	const auto valid_key = [&](const NativeProspectiveKey &key) {
		return key.actual.is_valid() ? key.future == UINT32_MAX : key.future < symbols.size();
	};
	const auto visit_dependencies = [&](const NativeProspectiveSymbol &symbol, auto &&visit) -> Error {
		for (RID dependency : symbol.dependencies) {
			if (Error error = visit(NativeProspectiveKey{ dependency, UINT32_MAX }); error != OK) {
				return error;
			}
		}
		if (symbol.shader.is_valid()) {
			if (Error error = visit(NativeProspectiveKey{ symbol.shader, UINT32_MAX }); error != OK) {
				return error;
			}
		}
		for (const auto &input : symbol.inputs) {
			if (Error error = visit(input.key); error != OK) {
				return error;
			}
		}
		return OK;
	};
	// Validate the complete finite roster before counting. Symbol ordinals are
	// original owner factory identities, never descriptor-equality identities.
	for (uint32_t root_index = 0; root_index < roots.size(); ++root_index) {
		const auto &root = roots[root_index];
		if (!valid_key(root)) {
			return ERR_INVALID_PARAMETER;
		}
	}
	for (uint32_t symbol_index = 0; symbol_index < symbols.size(); ++symbol_index) {
		const auto &symbol = symbols[symbol_index];
		if (symbol.kind < NativeProspectiveSymbol::FUTURE_VERTEX_ARRAY || symbol.kind > NativeProspectiveSymbol::FUTURE_FRAMEBUFFER) {
			return ERR_INVALID_PARAMETER;
		}
		if (Error error = visit_dependencies(symbol, [&](const NativeProspectiveKey &key) { return valid_key(key) ? OK : ERR_INVALID_PARAMETER; }); error != OK) {
			return error;
		}
	}
	// Iterative DFS avoids an unbounded C++ stack for a malformed symbol chain.
	Vector<uint8_t> state;
	Vector<uint32_t> stack, next_input;
	if (state.resize(symbols.size()) != OK || next_input.resize(symbols.size()) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	state.fill(0);
	next_input.fill(0);
	for (uint32_t start = 0; start < symbols.size(); ++start) {
		if (state[start]) {
			continue;
		}
		if (stack.push_back(start)) {
			return ERR_OUT_OF_MEMORY;
		}
		state.write[start] = 1;
		while (!stack.is_empty()) {
			const uint32_t current = stack[stack.size() - 1];
			if (next_input[current] == uint32_t(symbols[current].inputs.size())) {
				state.write[current] = 2;
				stack.resize(stack.size() - 1);
				continue;
			}
			const auto key = symbols[current].inputs[next_input[current]].key;
			++next_input.write[current];
			if (key.actual.is_valid() || state[key.future] == 2) {
				continue;
			}
			if (state[key.future] == 1) {
				return ERR_CYCLIC_LINK;
			}
			state.write[key.future] = 1;
			if (stack.push_back(key.future)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	for (uint32_t symbol_index = 0; symbol_index < symbols.size(); ++symbol_index) {
		const auto &symbol = symbols[symbol_index];
		if (symbol.kind <= NativeProspectiveSymbol::FUTURE_RENDER_PIPELINE) {
			if (symbol.shader.is_valid() || !symbol.inputs.is_empty() || (symbol.kind == NativeProspectiveSymbol::FUTURE_RENDER_PIPELINE && symbol.dependencies.size() != 1)) {
				return ERR_INVALID_PARAMETER;
			}
			for (RID dependency : symbol.dependencies) {
				if (symbol.kind == NativeProspectiveSymbol::FUTURE_VERTEX_ARRAY ? !vertex_buffer_owner.owns(dependency) : !shader_owner.owns(dependency)) {
					return ERR_DOES_NOT_EXIST;
				}
			}
			continue;
		}
		if (!symbol.dependencies.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		if (symbol.kind == NativeProspectiveSymbol::FUTURE_UNIFORM_SET) {
			if (!shader_owner.owns(symbol.shader)) {
				return ERR_DOES_NOT_EXIST;
			}
		} else if (symbol.shader.is_valid()) {
			return ERR_INVALID_PARAMETER;
		}
		const bool leaf = symbol.kind <= NativeProspectiveSymbol::FUTURE_TEXTURE;
		if ((leaf && !symbol.inputs.is_empty()) || (symbol.kind == NativeProspectiveSymbol::FUTURE_FRAMEBUFFER && symbol.inputs.is_empty())) {
			return ERR_INVALID_DATA;
		}
		for (int i = 0; i < symbol.inputs.size(); ++i) {
			const auto &input = symbol.inputs[i];
			if (input.kind < NativeProspectiveInput::UNIFORM_BUFFER || input.kind > NativeProspectiveInput::SAMPLER) {
				return ERR_INVALID_PARAMETER;
			}
			if (symbol.kind == NativeProspectiveSymbol::FUTURE_FRAMEBUFFER && input.kind != NativeProspectiveInput::TEXTURE) {
				return ERR_INVALID_DATA;
			}
			for (int j = 0; j < i; ++j) {
				if (symbol.inputs[j].key == input.key) {
					return ERR_INVALID_DATA; // Owner must coalesce repeated descriptor roots.
				}
			}
			if (input.key.actual.is_valid()) {
				bool owned = false;
				switch (input.kind) {
					case NativeProspectiveInput::UNIFORM_BUFFER:
						owned = uniform_buffer_owner.owns(input.key.actual);
						break;
					case NativeProspectiveInput::STORAGE_BUFFER:
						owned = storage_buffer_owner.owns(input.key.actual);
						break;
					case NativeProspectiveInput::TEXTURE:
						owned = texture_owner.owns(input.key.actual);
						break;
					case NativeProspectiveInput::SAMPLER:
						owned = sampler_owner.owns(input.key.actual);
						break;
				}
				if (!owned) {
					return ERR_DOES_NOT_EXIST;
				}
			} else {
				const auto kind = symbols[input.key.future].kind;
				if ((input.kind == NativeProspectiveInput::UNIFORM_BUFFER && kind != NativeProspectiveSymbol::FUTURE_UNIFORM_BUFFER) ||
						(input.kind == NativeProspectiveInput::STORAGE_BUFFER && kind != NativeProspectiveSymbol::FUTURE_STORAGE_BUFFER) ||
						(input.kind == NativeProspectiveInput::TEXTURE && kind != NativeProspectiveSymbol::FUTURE_TEXTURE) || input.kind == NativeProspectiveInput::SAMPLER) {
					return ERR_INVALID_DATA;
				}
			}
		}
	}
	Vector<NativeProspectiveKey> nodes;
	auto append = [&](const NativeProspectiveKey &key) -> Error {
		if (!valid_key(key)) {
			return ERR_INVALID_PARAMETER;
		}
		for (const auto &existing : nodes) {
			if (existing == key) {
				return OK;
			}
		}
		if (uint64_t(nodes.size()) >= limit) {
			return ERR_OUT_OF_MEMORY;
		}
		return nodes.push_back(key) ? ERR_OUT_OF_MEMORY : OK;
	};
	for (uint32_t root_index = 0; root_index < roots.size(); ++root_index) {
		const auto &root = roots[root_index];
		if (Error error = append(root); error != OK) {
			return error;
		}
	}
	auto expand = [&](NativeClosureMode direction) -> Error {
		for (int64_t i = 0; i < nodes.size(); ++i) {
			const NativeProspectiveKey key = nodes[i];
			if (key.actual.is_valid()) {
				// Framebuffer has an original attachment dependency graph, but no
				// immutable hold counter. Counting it grants no hold eligibility.
				if (!_native_immutable_counter(key.actual) && !framebuffer_owner.owns(key.actual)) {
					return ERR_DOES_NOT_EXIST;
				}
				if (Error error = _native_immutable_visit_related_locked(key.actual, direction, [&](RID related) { return append({ related, UINT32_MAX }); }); error != OK) {
					return error;
				}
			} else if (direction != NativeClosureMode::MATERIAL_DISPOSAL) {
				if (const Error error = visit_dependencies(symbols[key.future], append); error != OK) {
					return error;
				}
			}
			if (direction != NativeClosureMode::DEPENDENCIES) {
				for (uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
					if (Error error = visit_dependencies(symbols[symbol], [&](const NativeProspectiveKey &dependency) {
							return dependency == key ? append({ RID(), symbol }) : OK;
						});
							error != OK) {
						return error;
					}
				}
			}
		}
		return OK;
	};
	if (mode == NativeClosureMode::MATERIAL_DISPOSAL) {
		// Complete the real held span before adding any disposal observer.
		// Observers contribute their dependents, never their other inputs.
		const Error held = expand(NativeClosureMode::DEPENDENCIES);
		if (held != OK) {
			return held;
		}
	}
	const Error expanded = expand(mode);
	if (expanded != OK) {
		return expanded;
	}
	out = std::move(nodes);
	return OK;
}

void RenderingDevice::_production_output_hold_inventory_clear_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t reserved_group) {
	using namespace GDExtensionDeviceExit;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.output_role != ProductionUseCell::OutputRole::DISPOSAL_STAGED || cell.disposal_group_serial != reserved_group) {
			continue;
		}
		cell.disposal.~ProductionDisposalUse();
		new (&cell.disposal) ProductionDisposalUse;
		cell.output_role = reserved_group ? ProductionUseCell::OutputRole::DISPOSAL_RESERVED : ProductionUseCell::OutputRole::UNUSED;
		if (reserved_group) {
			cell.disposal.request = &request;
			cell.disposal.request_serial = request.serial;
			cell.disposal.group = reserved_group;
			cell.disposal.owned = true;
			cell.disposal.counter_dropped = true;
		}
	}
}

Error RenderingDevice::_production_output_hold_inventory_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t reserved_group, const VectorView<NativeImmutableHold *> &holds, uint32_t limit, uint32_t &out_count, NativeClosureMode mode) {
	uint32_t count = 0;
	const Error inventoried = _production_output_hold_stage_locked(request, reserved_group, holds, limit, count, mode);
	if (inventoried != OK) {
		return inventoried;
	}
	_production_output_hold_inventory_clear_locked(request, reserved_group);
	out_count = count;
	return OK;
}

Error RenderingDevice::_native_prospective_hold_increments_locked(const VectorView<NativeProspectiveHoldRoots> &holds, const VectorView<NativeProspectiveSymbol> &symbols) {
	// The original row owns at most two geometry holds and one material hold.
	// Scratch roots/counts do not prove that row's identity or root completeness.
	if (holds.size() > 3 || (holds.size() && !holds.ptr())) {
		return ERR_INVALID_PARAMETER;
	}
	const Error capacity = _native_immutable_hold_capacity_locked(holds.size());
	if (capacity != OK) {
		return capacity;
	}
	Vector<NativeProspectiveKey> closures[3];
	for (uint32_t h = 0; h < holds.size(); ++h) {
		if (!holds[h].limit || holds[h].limit > 1024) {
			return ERR_INVALID_PARAMETER;
		}
		const Error closure = _native_prospective_closure_keys_locked(holds[h].roots, symbols, holds[h].limit, false, closures[h]);
		if (closure != OK) {
			return closure;
		}
	}
	for (uint32_t h = 0; h < holds.size(); ++h) {
		for (const NativeProspectiveKey &key : closures[h]) {
			if (key.actual.is_null()) {
				// New symbolic owners have no current counter. At most three new
				// holds can refer to one correctly coalesced future factory node.
				// This also counts future framebuffers without granting them hold
				// support. OK checks actual increments only; every future actual
				// incarnation still needs original eligibility/factory admission.
				continue;
			}
			uint32_t increments = 0;
			for (uint32_t other = 0; other < holds.size(); ++other) {
				for (const NativeProspectiveKey &related : closures[other]) {
					if (related == key) {
						++increments; // One per hold, even with repeated raw roots.
						break;
					}
				}
			}
			const Error eligible = _native_immutable_resource_eligibility_locked(key.actual, increments);
			if (eligible != OK) {
				return eligible;
			}
			// A cache observer is supported only in the wider disposal closure,
			// never as an actual forward dependency of a prospective hold.
			const auto *set = uniform_set_owner.owns(key.actual) ? uniform_set_owner.get_or_null(key.actual) : nullptr;
			if (set && set->invalidated_callback && this == get_singleton() &&
					UniformSetCacheRD::owns_invalidation_callback(key.actual, set->invalidated_callback, set->invalidated_callback_userdata)) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	return OK;
}

Error RenderingDevice::_production_output_resource_preflight_locked(GDExtensionDeviceExit::ProductionRequest &request, RID rid, uint32_t pins, const VectorView<NativeProspectiveMaterialOrigin> &origins, NativeProspectiveDisposalMetadata &metadata) {
	using namespace GDExtensionDeviceExit;
#ifdef TESTS_ENABLED
	auto trace_refusal = [&](const char *p_stage, Error p_error, uint32_t p_kind, const UniformSet *p_set = nullptr) {
		const OS *os = OS::get_singleton();
		if (!os || os->get_environment("ANGE_TEST_STAGED_MOBILE_PIPELINE_PHASE") != "1") {
			return;
		}
		const bool callback_present = p_set && p_set->invalidated_callback;
		const bool material_callback = callback_present && p_set->invalidated_callback == RendererRD::MaterialStorage::_material_uniform_set_erased;
		const bool material_origin_present = p_set && p_set->production_material_origin.is_valid() && p_set->production_material_origin_generation;
		std::fprintf(stdout, "STAGED_MOBILE_RESOURCE_PREFLIGHT stage=%s error=%d resource_kind=%u callback_present=%d material_callback=%d material_origin_present=%d\n",
				p_stage, int(p_error), p_kind, int(callback_present), int(material_callback), int(material_origin_present));
		std::fflush(stdout);
	};
#endif
	metadata = NativeProspectiveDisposalMetadata();
	const uint64_t wide_begin = uint64_t(request.use_count) + request.binding_used;
	if (wide_begin > request.capacity.resource_uses) {
#ifdef TESTS_ENABLED
		trace_refusal("request_use_bounds", ERR_INVALID_DATA, UINT32_MAX);
#endif
		return ERR_INVALID_DATA;
	}
	const uint32_t begin = uint32_t(wide_begin);
	uint32_t kind;
	uint64_t serial, native_driver_id;
	auto **head = _production_disposal_head(rid, kind, serial, native_driver_id);
	auto *counter = _native_immutable_counter(rid);
	if (!head || !serial || (kind < 10 && !native_driver_id) || !counter || *counter < pins) {
#ifdef TESTS_ENABLED
		const OS *os = OS::get_singleton();
		if (kind == UINT32_MAX && native_immutable_is_render_thread() && os && os->get_environment("ANGE_TEST_STAGED_MOBILE_PIPELINE_PHASE") == "1") {
			const bool framebuffer_owned = framebuffer_owner.owns(rid);
			const auto *framebuffer = framebuffer_owned ? framebuffer_owner.get_or_null(rid) : nullptr;
			const bool material_scope = origins.size() == 1 && origins[0].material.is_valid() && origins[0].generation;
			std::fprintf(stdout, "STAGED_MOBILE_RESOURCE_IDENTITY class=%s error=%d live=%d material_scope=%d pins=%u framebuffer_cache=%d attachments=%u qualification=%d native_disposal=%d callback=%d head=%d counter=%d serial=%d native_backing=%d\n",
					framebuffer_owned ? "framebuffer" : "unknown", int(ERR_INVALID_DATA), int(framebuffer != nullptr), int(material_scope), pins,
					int(framebuffer && framebuffer->framebuffer_cache), framebuffer ? uint32_t(framebuffer->texture_ids.size()) : 0u,
					int(framebuffer && (framebuffer->qualification_return || framebuffer->qualification_dependency)),
					int(framebuffer && framebuffer->native_disposal.cell.is_valid()), int(framebuffer && framebuffer->invalidated_callback),
					int(head != nullptr), int(counter != nullptr), int(serial != 0), int(native_driver_id != 0));
			std::fflush(stdout);
		}
		trace_refusal("resource_identity", ERR_INVALID_DATA, kind);
#endif
		return ERR_INVALID_DATA;
	}
	QualificationPhysicalReturn *qualified = nullptr;
	if (auto *qualified_buffer = _native_immutable_buffer(rid)) {
		qualified = qualified_buffer->qualification_return;
	} else if (auto *qualified_texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr) {
		qualified = qualified_texture->qualification_return;
	} else if (auto *qualified_shader = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr) {
		qualified = qualified_shader->qualification_return;
	} else if (auto *qualified_pipeline = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr) {
		qualified = qualified_pipeline->qualification_return;
	} else if (auto *qualified_sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr) {
		qualified = qualified_sampler->qualification_return;
	} else if (auto *qualified_uniform_set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr) {
		qualified = qualified_uniform_set->qualification_return;
	}
	if (qualified) {
		bool original = false;
		auto same_use = [&](const ProductionResourceUse &use) { return use.physical == qualified && use.request == &request && use.request_serial == request.serial && use.rid == rid.get_id() && use.same_physical() && use.linked(); };
		for (uint32_t i = 0; i < request.resource_count; ++i) {
			const auto &slot = request.resources[i];
			for (uint32_t u = 0; u < slot.use_count; ++u) {
				original |= same_use(slot.uses[u]);
			}
			original |= slot.imported && &slot.owned_physical == qualified && qualified->rid == rid.get_id() && slot.backing_serial == qualified->serial;
		}
		for (uint32_t i = 0; i < begin; ++i) {
			original |= same_use(request.uses[i].dependency);
		}
		if (!original) {
#ifdef TESTS_ENABLED
			trace_refusal("qualified_use", ERR_UNAUTHORIZED, kind);
#endif
			return ERR_UNAUTHORIZED;
		}
	}
	const bool material_observer = !pins && !qualified && origins.size() == 1 &&
			origins[0].material.is_valid() && origins[0].generation && this == get_singleton() && native_immutable_is_render_thread();
	if (auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr; set && set->invalidated_callback) {
		bool material_owned = false;
		for (uint32_t origin_index = 0; origin_index < origins.size(); ++origin_index) {
			const auto &origin = origins[origin_index];
			material_owned |= !origin.material.is_null() && origin.material == set->production_material_origin &&
					origin.generation == set->production_material_origin_generation &&
					RendererRD::MaterialStorage::_native_hold_callback_is_owned(set->production_material_origin, set->production_material_origin_generation, set->invalidated_callback, set->invalidated_callback_userdata, rid);
		}
		// The packet material's bidirectional disposal inventory can reach a
		// renderer-owned cache set. Observe it without taking a native hold.
		const bool cache_observer = !material_owned && material_observer &&
				UniformSetCacheRD::owns_invalidation_callback(rid, set->invalidated_callback, set->invalidated_callback_userdata);
		if (!material_owned && !cache_observer) {
#ifdef TESTS_ENABLED
			trace_refusal("uniform_set_callback", ERR_UNAVAILABLE, kind, set);
#endif
			return ERR_UNAVAILABLE;
		}
	}
	if (auto *buffer = _native_immutable_buffer(rid); buffer && buffer->transfer_worker_index >= 0) {
#ifdef TESTS_ENABLED
		trace_refusal("buffer_worker", ERR_BUSY, kind);
#endif
		return ERR_BUSY;
	}
	if (const auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr; texture && texture->transfer_worker_index >= 0) {
		const bool texture_observer = material_observer && !texture->native_external_access;
		bool observe_worker = texture_observer;
		bool processed = false;
		bool poisoned = false;
#ifdef TESTS_ENABLED
		const OS *os = OS::get_singleton();
		const bool trace_worker = native_immutable_is_render_thread() && os && os->get_environment("ANGE_TEST_STAGED_MOBILE_PIPELINE_PHASE") == "1";
		observe_worker |= trace_worker;
		bool observed = false;
		bool submitted = false;
		bool recording = false;
		bool submission_pending = false;
		bool completion_observed = false;
#endif
		if (observe_worker) {
			// The original worker advances its processed cursor only after the
			// corresponding upload completes. Observe the exact operation without
			// publishing a dependency, clearing the association, or servicing it.
			const uint32_t index = uint32_t(texture->transfer_worker_index);
			const uint64_t operation = texture->transfer_worker_operation;
			std::unique_lock<BinaryMutex> pool_lock(transfer_worker_pool_mutex, std::try_to_lock);
			if (pool_lock.owns_lock() && index < transfer_worker_pool_size && index < transfer_worker_pool.size()) {
				if (auto *worker = transfer_worker_pool[index]) {
					std::unique_lock<BinaryMutex> thread_lock(worker->thread_mutex, std::try_to_lock);
					std::unique_lock<BinaryMutex> operation_lock(worker->operations_mutex, std::defer_lock);
					if (thread_lock.owns_lock() && operation_lock.try_lock() && worker->index == index && operation && operation <= worker->operations_counter) {
						processed = operation <= worker->operations_processed;
						poisoned = worker->poisoned;
#ifdef TESTS_ENABLED
						observed = true;
						submitted = !processed && worker->submitted && operation <= worker->operations_submitted;
						recording = !processed && !submitted && worker->recording;
						submission_pending = !processed && !submitted && worker->submission_pending;
						completion_observed = submitted && worker->last_completion.matches(worker->submission_serial);
#endif
					}
				}
			}
		}
		if (!texture_observer || !processed || poisoned) {
#ifdef TESTS_ENABLED
			if (trace_worker) {
				std::fprintf(stdout, "STAGED_MOBILE_TEXTURE_WORKER observed=%d processed=%d submitted=%d recording=%d submission_pending=%d completion_observed=%d poisoned=%d\n",
						int(observed), int(processed), int(submitted), int(recording), int(submission_pending), int(completion_observed), int(!processed && poisoned));
				std::fflush(stdout);
			}
			trace_refusal("texture_worker", ERR_UNAVAILABLE, kind);
#endif
			return ERR_UNAVAILABLE;
		}
	}
	metadata.kind = kind;
	metadata.serial = serial;
	metadata.native_driver_id = native_driver_id;
	metadata.qualified_borrow = qualified != nullptr;
	return OK;
}

Error RenderingDevice::_production_output_owned_destination_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t group, uint32_t ordinal, GDExtensionDeviceExit::ProductionUseCell *&destination) {
	using namespace GDExtensionDeviceExit;
	destination = nullptr;
	const uint64_t begin = uint64_t(request.use_count) + request.binding_used;
	if (!group || begin > request.capacity.resource_uses || (request.capacity.resource_uses && !request.uses)) {
		return ERR_INVALID_DATA;
	}
	// Same destination rule as the original owned factory. The caller must
	// separately authenticate its pin/group and validate prepaid group layout.
	for (uint32_t i = uint32_t(begin); i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.disposal_group_serial == group && cell.disposal_ordinal == ordinal && cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			destination = &cell;
			return OK;
		}
	}
	return ERR_INVALID_DATA;
}

Error RenderingDevice::_production_output_hold_reservation_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t pin_serial, uint64_t reserved_group, uint32_t limit, uint32_t &group_index, bool require_unstaged) {
	using namespace GDExtensionDeviceExit;
	group_index = UINT32_MAX;
	const uint64_t wide_begin = uint64_t(request.use_count) + request.binding_used;
	if (wide_begin > request.capacity.resource_uses || (request.capacity.resource_uses && !request.uses)) {
		return ERR_INVALID_DATA;
	}
	// A prospective caller must never accept an arena carrying another dry
	// inventory. Adopt preserves its historical own-reservation error order;
	// its actual stage call performs this same global check before mutation.
	if (require_unstaged) {
		for (uint32_t i = uint32_t(wide_begin); i < request.capacity.resource_uses; ++i) {
			if (request.uses[i].output_role == ProductionUseCell::OutputRole::DISPOSAL_STAGED) {
				return ERR_BUSY;
			}
		}
	}
	if (!reserved_group) {
		return OK;
	}
	uint32_t found = UINT32_MAX;
	uint32_t cells = 0;
	for (uint32_t i = uint32_t(wide_begin); i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		if (cell.disposal_group_serial != reserved_group) {
			continue;
		}
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP) {
			if (found != UINT32_MAX || (pin_serial && cell.disposal_group_pin_serial != pin_serial) ||
					cell.disposal_group_count != limit || cell.disposal_group_release || cell.disposal_group_dropped ||
					cell.disposal_group_native_hold || cell.disposal_group_native_hold_count) {
				return ERR_INVALID_DATA;
			}
			found = i;
		} else if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED && cell.disposal.request == &request &&
				cell.disposal.request_serial == request.serial && cell.disposal.group == reserved_group && cell.disposal.owned &&
				cell.disposal.counter_dropped && !cell.disposal.counter_pins_remaining && !cell.disposal.rid && !cell.disposal.serial &&
				!cell.disposal.native_driver_id && cell.disposal.kind == UINT32_MAX && !cell.disposal.qualified_borrow && !cell.disposal.next &&
				cell.disposal.state.load(std::memory_order_acquire) == 0) {
			++cells;
		} else {
			// Preserve the existing adopt versus inventory refusal code.
			return pin_serial ? ERR_ALREADY_IN_USE : ERR_INVALID_DATA;
		}
	}
	if (found == UINT32_MAX || cells != limit) {
		return ERR_INVALID_DATA;
	}
	group_index = found;
	return OK;
}

Error RenderingDevice::_production_output_hold_stage_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t reserved_group, const VectorView<NativeImmutableHold *> &holds, uint32_t limit, uint32_t &out_count, NativeClosureMode mode) {
	using namespace GDExtensionDeviceExit;
	// This bounded scratch belongs to the already admitted request. Every
	// public dry inventory return restores it, on success as well as refusal.
	if (holds.size() > 2 || (!limit && holds.size()) ||
			(mode == NativeClosureMode::MATERIAL_DISPOSAL && (!reserved_group || holds.size() > 1))) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t h = 0; h < holds.size(); ++h) {
		if (!holds[h] || holds[h]->device != this || holds[h]->pins_returned || holds[h]->pipeline_borrowers || holds[h]->_resource_span().is_empty() ||
				(mode == NativeClosureMode::MATERIAL_DISPOSAL && (holds[h]->material_origin.is_null() || !holds[h]->material_origin_generation))) {
			return ERR_INVALID_PARAMETER;
		}
		for (uint32_t other = 0; other < h; ++other) {
			if (holds[h] == holds[other]) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	NativeProspectiveMaterialOrigin origins[2];
	for (uint32_t h = 0; h < holds.size(); ++h) {
		origins[h].material = holds[h]->material_origin;
		origins[h].generation = holds[h]->material_origin_generation;
	}
	// Preserve staged/counter refusal order, but never traverse a wrapped span.
	const uint64_t wide_begin = uint64_t(request.use_count) + request.binding_used;
	if (wide_begin > request.capacity.resource_uses || (request.capacity.resource_uses && !request.uses)) {
		return ERR_INVALID_DATA;
	}
	const uint32_t begin = uint32_t(wide_begin);
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		if (request.uses[i].output_role == ProductionUseCell::OutputRole::DISPOSAL_STAGED) {
			return ERR_BUSY;
		}
	}
	if (native_immutable_hold_count < holds.size()) {
		return ERR_INVALID_DATA;
	}
	uint32_t group_index = UINT32_MAX;
	const Error reservation = _production_output_hold_reservation_locked(request, 0, reserved_group, limit, group_index);
	if (reservation != OK) {
		return reservation;
	}
	uint32_t count = 0;
	auto rollback = [&]() { _production_output_hold_inventory_clear_locked(request, reserved_group); };
	auto stage = [&](RID rid, uint32_t pins) -> Error {
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			auto &c = request.uses[i];
			if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_STAGED && c.disposal_group_serial == reserved_group && c.disposal.rid == rid.get_id()) {
				const auto *counter = _native_immutable_counter(rid);
				if (!counter || pins > UINT32_MAX - c.disposal.counter_pins_remaining || *counter < c.disposal.counter_pins_remaining + pins) {
					return ERR_INVALID_DATA;
				}
				c.disposal.counter_pins_remaining += pins;
				c.disposal.counter_dropped = c.disposal.counter_pins_remaining == 0;
				return OK;
			}
		}
		NativeProspectiveDisposalMetadata metadata;
		const Error checked = _production_output_resource_preflight_locked(request, rid, pins, VectorView<NativeProspectiveMaterialOrigin>(origins, holds.size()), metadata);
		if (checked != OK) {
			return checked;
		}
		if (count == limit) {
			return ERR_OUT_OF_MEMORY;
		}
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			auto &c = request.uses[i];
			const bool eligible = reserved_group ? c.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED && c.disposal_group_serial == reserved_group : c.output_role == ProductionUseCell::OutputRole::UNUSED && !c.disposal_group_serial && !c.output_consumer && !c.dependency.physical;
			if (eligible) {
				c.output_role = ProductionUseCell::OutputRole::DISPOSAL_STAGED;
				c.disposal.rid = rid.get_id();
				c.disposal.serial = metadata.serial;
				c.disposal.native_driver_id = metadata.native_driver_id;
				c.disposal.kind = metadata.kind;
				c.disposal.counter_pins_remaining = pins;
				c.disposal.counter_dropped = pins == 0;
				c.disposal.qualified_borrow = metadata.qualified_borrow;
				++count;
				return OK;
			}
		}
		return ERR_OUT_OF_MEMORY;
	};
	for (uint32_t h = 0; h < holds.size(); ++h) {
		NativeImmutableHold *hold = holds[h];
		for (RID rid : hold->_resource_span()) {
			const Error error = stage(rid, 1);
			if (error != OK) {
				rollback();
				return error;
			}
		}
	}
	// Iterate to a fixed point; reserved cells can be interspersed with other
	// admitted groups, so newly discovered nodes need not have larger indices.
	uint32_t previous = UINT32_MAX;
	while (previous != count) {
		previous = count;
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			auto &cell = request.uses[i];
			if (cell.output_role != ProductionUseCell::OutputRole::DISPOSAL_STAGED || cell.disposal_group_serial != reserved_group) {
				continue;
			}
			const RID rid = RID::from_uint64(cell.disposal.rid);
			const Error error = _native_immutable_visit_related_locked(rid, mode, [&](RID related) { return stage(related, 0); });
			if (error != OK) {
				rollback();
				return error;
			}
		}
	}
	out_count = count;
	return OK;
}

Error RenderingDevice::_production_output_hold_adopt_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t pin_serial, uint64_t reserved_group, const VectorView<NativeImmutableHold *> &holds, uint32_t limit, uint64_t &out_group, NativeClosureMode mode) {
	using namespace GDExtensionDeviceExit;
	// Caller authentication is deliberately outside this shared primitive.
	// It neither installs a scope nor tests numeric DTOs as source authority.
	if (!pin_serial || out_group || !request.serial || (!reserved_group && (!request.next_disposal_group_serial || !holds.size()))) {
		return ERR_INVALID_PARAMETER;
	}
	uint32_t group_index = UINT32_MAX;
	const Error reservation = _production_output_hold_reservation_locked(request, pin_serial, reserved_group, limit, group_index, false);
	if (reservation != OK) {
		return reservation;
	}
	const uint32_t begin = uint32_t(uint64_t(request.use_count) + request.binding_used);
	ProductionUseCell *group = reserved_group ? &request.uses[group_index] : nullptr;
	uint32_t count = 0;
	const Error inventoried = _production_output_hold_stage_locked(request, reserved_group, holds, limit, count, mode);
	if (inventoried != OK) {
		return inventoried;
	}
	if (!reserved_group) {
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			auto &c = request.uses[i];
			if (c.output_role == ProductionUseCell::OutputRole::UNUSED && !c.disposal_group_serial && !c.output_consumer && !c.dependency.physical) {
				group = &c;
				break;
			}
		}
		if (!group) {
			_production_output_hold_inventory_clear_locked(request, reserved_group);
			return ERR_OUT_OF_MEMORY;
		}
	}
	const uint64_t serial = reserved_group ? reserved_group : request.next_disposal_group_serial;
	// All refusal paths precede this commit. Existing reservations are changed
	// from owned-empty cells to exact borrowed incarnations only here.
	if (!reserved_group) {
		request.next_disposal_group_serial = serial == UINT64_MAX ? 0 : serial + 1;
	}
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_STAGED && c.disposal_group_serial == reserved_group) {
			auto &n = c.disposal;
			uint32_t kind;
			uint64_t incarnation, driver_id;
			auto **head = _production_disposal_head(RID::from_uint64(n.rid), kind, incarnation, driver_id);
			CRASH_COND(!head || kind != n.kind || incarnation != n.serial || driver_id != n.native_driver_id);
			n.request = &request;
			n.request_serial = request.serial;
			n.group = serial;
			n.owned = false;
			n.next = *head;
			*head = &n;
			n.state.store(1, std::memory_order_release);
			c.disposal_group_serial = serial;
			c.output_role = ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
		} else if (reserved_group && c.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED && c.disposal_group_serial == reserved_group) {
			c.disposal.owned = false;
			c.disposal.state.store(6, std::memory_order_release); // Prepaid but never occupied.
			c.output_role = ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
		}
	}
	group->disposal_group_pin_serial = pin_serial;
	group->disposal_group_native_hold = holds.size() != 0;
	group->disposal_group_native_hold_count = holds.size();
	group->disposal_group_serial = serial;
	group->disposal_group_count = reserved_group ? limit : count;
	group->output_role = ProductionUseCell::OutputRole::DISPOSAL_GROUP;
	for (uint32_t h = 0; h < holds.size(); ++h) {
		NativeImmutableHold *hold = holds[h];
		hold->_clear_resources();
		hold->device = nullptr;
		hold->pins_returned = false;
		hold->material_origin = RID();
		hold->material_origin_generation = 0;
	}
	out_group = serial;
	return OK;
}
Error RenderingDevice::_production_output_hold_begin_return(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	GDExtensionProductionReportV2 source = {};
	registry.production_report_locked(request, source);
	if (request.capture_inflight || request.compute_list_open || (!request.cancelled && request.next_action != request.action_count) || source.possible_obligations || source.known_obligations != source.complete_obligations) {
		return ERR_BUSY;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	for (uint32_t command = 0; command < draw_graph.qualification_command_count(); ++command) {
		bool captured = false;
		for (uint32_t i = 0; i < request.graph_count; ++i) {
			captured |= registry.production_output_graph_command_locked(request, *this, i, command);
		}
		if (!captured) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_serial == serial && cell.disposal_group_pin_serial == pin.serial) {
			cell.disposal_group_release = true;
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}
Error RenderingDevice::_production_output_hold_returned(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial, GDExtensionDeviceExit::ProductionDisposalObservation &observation) {
	if (!pin.registry) {
		observation = {};
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionDeviceExit::Registry::Lock registry_lock(*pin.registry);
	return _production_output_hold_returned_locked(pin, serial, observation);
}

Error RenderingDevice::_production_output_hold_returned_locked(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial, GDExtensionDeviceExit::ProductionDisposalObservation &observation) {
	using namespace GDExtensionDeviceExit;
	observation = {};
	if (!pin.registry || !serial) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	bool found = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &cell = request.uses[i];
		if (cell.disposal_group_serial != serial) {
			continue;
		}
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP) {
			found = cell.disposal_group_pin_serial == pin.serial;
			observation.dropped = found && cell.disposal_group_dropped;
		} else if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER) {
			switch (cell.disposal.state.load(std::memory_order_acquire)) {
				case 1:
					++observation.held;
					break;
				case 2:
					++observation.queued;
					break;
				case 3:
					++observation.physical_returned;
					break;
				case 4:
					++observation.borrowed_detached;
					break;
				case 6:
					++observation.uncreated;
					break;
				case 7:
					++observation.owner_returned;
					break;
				default:
					++observation.unknown;
					break;
			}
		}
	}
	if (!found) {
		return ERR_DOES_NOT_EXIST;
	}
	observation.settled = observation.dropped && !observation.held && !observation.queued && !observation.unknown;
	return OK;
}

Error RenderingDevice::_production_output_drop_step(GDExtensionDeviceExit::ProductionRequest &request, const NativeDisposalBudget &budget, NativeDisposalUsage &usage) {
	using namespace GDExtensionDeviceExit;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	if (!budget.passes || !budget.receipts || !budget.deadline_usec || OS::get_singleton()->get_ticks_usec() >= budget.deadline_usec) {
		return ERR_BUSY;
	}
	if (render_thread_id != Thread::get_caller_id() || native_disposal_budget) {
		return ERR_UNAUTHORIZED;
	}
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	// Callback replacement is legal for the original RD owner. Admission must
	// use its current callback at each turn, not a stale capture-time proof.
	{
		MutexLock lock(_thread_safe_);
		const uint32_t begin = request.use_count + request.binding_used;
		for (uint32_t g = begin; g < request.capacity.resource_uses; ++g) {
			const auto &group = request.uses[g];
			if (group.output_role != ProductionUseCell::OutputRole::DISPOSAL_GROUP || !group.disposal_group_release) {
				continue;
			}
			for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
				const auto &cell = request.uses[i];
				if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && cell.disposal.group == group.disposal_group_serial && cell.disposal.state.load(std::memory_order_acquire) == 1) {
					const RID rid = RID::from_uint64(cell.disposal.rid);
					if (!_production_output_callback_supported(rid) && !_production_output_cache_observer_callback_supported_locked(rid, request, &cell.disposal)) {
						return ERR_UNAVAILABLE;
					}
				}
			}
		}
	}
	ProductionCleanupExtent cleanup_extent;
	// Original executing control and capture_inflight retain this finite arena.
	// This function is called with neither RD nor Registry lock held.
	struct Meter {
		RenderingDevice &rd;
		~Meter() {
			rd.production_disposal_drop_owner = nullptr;
			rd.native_disposal_budget = nullptr;
			rd.native_disposal_usage = nullptr;
		}
	} meter{ *this };
	production_disposal_drop_owner = &request;
	native_disposal_budget = &budget;
	native_disposal_usage = &usage;
	const uint32_t begin = request.use_count + request.binding_used;
	for (uint32_t g = begin; g < request.capacity.resource_uses; ++g) {
		auto &group = request.uses[g];
		if (group.output_role != ProductionUseCell::OutputRole::DISPOSAL_GROUP || !group.disposal_group_release || group.disposal_group_dropped) {
			continue;
		}
		bool all_dropped = true;
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			auto &cell = request.uses[i];
			auto &node = cell.disposal;
			if (cell.output_role != ProductionUseCell::OutputRole::DISPOSAL_MEMBER || node.group != group.disposal_group_serial || node.counter_dropped) {
				continue;
			}
			// Legacy disposal producers without explicit multiplicity own one pin.
			if (!node.counter_pins_remaining) {
				node.counter_pins_remaining = 1;
			}
			while (node.counter_pins_remaining) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return ERR_BUSY;
				}
				MutexLock lock(_thread_safe_);
				const RID rid = RID::from_uint64(node.rid);
				uint32_t kind;
				uint64_t serial, driver_id;
				auto **head = _production_disposal_head(rid, kind, serial, driver_id);
				auto *counter = _native_immutable_counter(rid);
				if (!head || kind != node.kind || serial != node.serial || driver_id != node.native_driver_id || !counter || !*counter) {
					node.state.store(5, std::memory_order_release);
					return ERR_INVALID_DATA;
				}
				if (!_production_output_callback_supported(rid)) {
					return ERR_UNAVAILABLE;
				}
				--*counter;
				--node.counter_pins_remaining;
			}
			node.counter_dropped = true;
		}
		for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
			const auto &cell = request.uses[i];
			if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && cell.disposal.group == group.disposal_group_serial) {
				all_dropped &= cell.disposal.counter_dropped;
			}
		}
		if (all_dropped) {
			MutexLock lock(_thread_safe_);
			if (group.disposal_group_native_hold) {
				const uint32_t count = group.disposal_group_native_hold_count ? group.disposal_group_native_hold_count : 1;
				CRASH_COND(native_immutable_hold_count < count);
				native_immutable_hold_count -= count;
			}
			group.disposal_group_dropped = true;
		}
	}
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		auto &node = cell.disposal;
		if (cell.output_role != ProductionUseCell::OutputRole::DISPOSAL_MEMBER || !node.counter_dropped || node.state.load(std::memory_order_acquire) != 1) {
			continue;
		}
		bool released = false;
		for (uint32_t g = begin; g < request.capacity.resource_uses; ++g) {
			const auto &group = request.uses[g];
			if (group.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && group.disposal_group_serial == node.group) {
				released = group.disposal_group_release && group.disposal_group_dropped;
			}
		}
		if (!released) {
			continue;
		}
		MutexLock lock(_thread_safe_);
		const RID rid = RID::from_uint64(node.rid);
		uint32_t kind;
		uint64_t serial, driver_id;
		auto **head = _production_disposal_head(rid, kind, serial, driver_id);
		bool *retiring = nullptr;
		const auto *counter = _native_immutable_counter(rid, &retiring);
		if (!head || kind != node.kind || serial != node.serial || driver_id != node.native_driver_id || !counter) {
			node.state.store(5, std::memory_order_release);
			return ERR_INVALID_DATA;
		}
		if (!_production_output_callback_supported(rid)) {
			return ERR_UNAVAILABLE;
		}
		bool owned_in_request = false;
		for (auto *related = *head; related; related = related->next) {
			owned_in_request |= related->request == &request && related->request_serial == request.serial && related->owned && related->state.load(std::memory_order_acquire) == 1;
		}
		if (!*retiring && !node.owned && owned_in_request && !node.qualified_borrow) {
			continue;
		}
		if ((!*retiring || node.qualified_borrow) && !node.owned) {
			if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
				return ERR_BUSY;
			}
			auto **link = head;
			while (*link && *link != &node) {
				link = &(*link)->next;
			}
			if (*link != &node) {
				return ERR_INVALID_DATA;
			}
			*link = node.next;
			node.next = nullptr;
			node.state.store(4, std::memory_order_release);
			continue;
		}
		if (*counter && !node.owned) {
			continue; // Another genuine native hold still owns this physical allocation.
		}
		if (*counter && *retiring && node.owned) {
			// Acquisition is closed. Let the original borrowed groups
			// return their holds before retrying this owner;
			// repeating a blocked free would consume every one-pass turn.
			continue;
		}
		// Select a true dependency leaf before consuming the sole enqueue pass.
		// A parent/child pair cannot need two passes in one indivisible attempt.
		const auto *dependents = dependency_map.getptr(rid);
		if (dependents && !dependents->is_empty()) {
			continue;
		}
		// Enqueue removes the ordinary RID dependency edge, but an output
		// descriptor still pins its buffers until actual descriptor free.
		// Skip this known blocked parent BEFORE spending a one-pass budget;
		// otherwise it starves the very pending free that removes the pin.
		const auto *output_buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
		if (output_buffer && output_buffer->native_disposal.cell.is_valid() && output_buffer->native_disposal.cell->production_dependent_sets) {
			continue;
		}
		if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
			return ERR_BUSY;
		}
		if (node.owned && node.kind != 0) {
			NativeDisposalWitness witness;
			if (auto *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr) {
				witness = buffer->native_disposal;
			} else if (auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr) {
				witness = set->native_disposal;
			}
			lock.temp_unlock();
			const Error freed = _native_disposal_free(witness);
			lock.temp_relock();
			if (freed != OK && freed != ERR_BUSY) {
				return freed;
			}
		} else {
			lock.temp_unlock();
			free_rid(rid);
			lock.temp_relock();
		}
	}
	return OK;
}

Error RenderingDevice::_production_output_owned_reserve(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint32_t count, uint64_t &serial) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || serial || !count) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *actual_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, request, actual_device, true);
	if (admitted != OK) {
		return admitted;
	}
	if (actual_device != this || request != pin.request || ((request->next_action || request->graph_count || request->native_count) && !registry.production_output_factory_pin_matches_locked(*request, pin, *this)) || !request->next_disposal_group_serial) {
		return ERR_BUSY;
	}
	uint64_t available = 0;
	const uint32_t begin = request->use_count + request->binding_used;
	for (uint32_t i = begin; i < request->capacity.resource_uses; ++i) {
		const auto &c = request->uses[i];
		available += c.output_role == ProductionUseCell::OutputRole::UNUSED && !c.disposal_group_serial && !c.output_consumer && !c.dependency.physical;
	}
	if (uint64_t(count) + 1 > available) {
		return ERR_OUT_OF_MEMORY;
	}
	serial = request->next_disposal_group_serial;
	request->next_disposal_group_serial = serial == UINT64_MAX ? 0 : serial + 1;
	uint32_t assigned = 0;
	for (uint32_t i = begin; i < request->capacity.resource_uses && assigned <= count; ++i) {
		auto &c = request->uses[i];
		if (c.output_role != ProductionUseCell::OutputRole::UNUSED || c.disposal_group_serial || c.output_consumer || c.dependency.physical) {
			continue;
		}
		c.disposal_group_serial = serial;
		if (assigned == count) {
			c.output_role = ProductionUseCell::OutputRole::DISPOSAL_GROUP;
			c.disposal_group_count = count;
			c.disposal_group_pin_serial = pin.serial;
		} else {
			c.output_role = ProductionUseCell::OutputRole::DISPOSAL_RESERVED;
			c.disposal_ordinal = assigned;
			c.disposal.request = request;
			c.disposal.request_serial = request->serial;
			c.disposal.group = serial;
			c.disposal.owned = true;
			c.disposal.counter_dropped = true;
		}
		++assigned;
	}
	return OK;
}
Error RenderingDevice::_production_output_owned_rollback(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t &serial) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *actual_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, request, actual_device, false);
	if (admitted != OK) {
		return admitted;
	}
	if (actual_device != this || request != pin.request || ((request->next_action || request->graph_count || request->native_count) && !registry.production_output_factory_pin_matches_locked(*request, pin, *this)) || request->capture_inflight) {
		return ERR_BUSY;
	}
	const uint32_t begin = request->use_count + request->binding_used;
	uint32_t groups = 0, members = 0, expected = 0;
	for (uint32_t i = begin; i < request->capacity.resource_uses; ++i) {
		const auto &cell = request->uses[i];
		if (cell.disposal_group_serial != serial) {
			continue;
		}
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP) {
			if (cell.disposal_group_pin_serial != pin.serial) {
				return ERR_UNAUTHORIZED;
			}
			if (cell.disposal_group_release || cell.disposal_group_dropped || cell.disposal_group_native_hold) {
				return ERR_BUSY;
			}
			++groups;
			expected = cell.disposal_group_count;
		} else if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			const auto &node = cell.disposal;
			if (node.rid || node.serial || node.native_driver_id || node.next || node.state.load(std::memory_order_acquire) != 0 ||
					node.request != request || node.request_serial != request->serial || node.group != serial || !node.owned || !node.counter_dropped) {
				return ERR_BUSY;
			}
			++members;
		} else {
			return ERR_BUSY;
		}
	}
	if (groups != 1 || !expected || members != expected) {
		return ERR_DOES_NOT_EXIST;
	}
	// All checks precede mutation. Only unused metadata rows return to capacity;
	// the consumed group serial is never rewound or reused.
	for (uint32_t i = begin; i < request->capacity.resource_uses; ++i) {
		auto &cell = request->uses[i];
		if (cell.disposal_group_serial != serial) {
			continue;
		}
		cell.disposal.~ProductionDisposalUse();
		new (&cell.disposal) ProductionDisposalUse();
		cell.output_role = ProductionUseCell::OutputRole::UNUSED;
		cell.disposal_group_serial = cell.disposal_group_pin_serial = 0;
		cell.disposal_group_count = 0;
		cell.disposal_ordinal = UINT32_MAX;
	}
	serial = 0;
	return OK;
}
Error RenderingDevice::_production_output_owned_bind(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial, uint32_t ordinal, const NativeDisposalWitness &witness) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial || witness.cell.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *actual_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, request, actual_device, false);
	if (admitted != OK) {
		return admitted;
	}
	if (actual_device != this || request != pin.request || request->next_action || request->graph_count || request->native_count) {
		return ERR_BUSY;
	}
	return _production_output_owned_bind_locked(*request, pin.serial, serial, ordinal, witness);
}
Error RenderingDevice::_production_output_owned_bind_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t pin_serial, uint64_t serial, uint32_t ordinal, const NativeDisposalWitness &witness) {
	using namespace GDExtensionDeviceExit;
	bool owns_group = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &g = request.uses[i];
		owns_group |= g.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && g.disposal_group_serial == serial && g.disposal_group_pin_serial == pin_serial;
	}
	if (!owns_group) {
		return ERR_DOES_NOT_EXIST;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct RdUnlock {
		Mutex &mutex;
		~RdUnlock() { mutex.unlock(); }
	} rd_unlock{ _thread_safe_ };
	const auto &physical = witness.cell;
	if (physical->production_request != &request || physical->production_request_serial != request.serial || physical->production_group_serial != serial || physical->production_pin_serial != pin_serial || physical->production_ordinal != ordinal) {
		return ERR_UNAUTHORIZED;
	}
	if (physical->device != uint64_t(get_instance_id()) || physical->state != NativeDisposalWitness::State::LIVE || !physical->matches(physical->rid, physical->serial, physical->backing)) {
		return ERR_INVALID_DATA;
	}
	const RID rid = RID::from_uint64(physical->rid);
	uint32_t kind;
	uint64_t generation, driver_id;
	auto **head = _production_disposal_head(rid, kind, generation, driver_id);
	const auto *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
	const auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
	if (!head || (kind != 9 && kind != 4) || driver_id != physical->backing || (buffer ? buffer->native_disposal.cell != witness.cell || buffer->backing_serial != physical->serial : set ? set->native_disposal.cell != witness.cell || set->native_disposal_serial != physical->serial
																																														  : true)) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.disposal_group_serial != serial || c.disposal_ordinal != ordinal) {
			continue;
		}
		if (c.output_role != ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			return ERR_ALREADY_IN_USE;
		}
		auto &node = c.disposal;
		node.rid = rid.get_id();
		node.serial = generation;
		node.native_driver_id = driver_id;
		node.kind = kind;
		node.next = *head;
		*head = &node;
		node.state.store(1, std::memory_order_release);
		c.output_role = ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}
Error RenderingDevice::_production_output_owned_cancel_unbound(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	bool found = false;
	auto &request = *pin.request;
	if (request.continuation_inflight) {
		return ERR_BUSY;
	}
	bool owns_group = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &g = request.uses[i];
		owns_group |= g.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && g.disposal_group_serial == serial && g.disposal_group_pin_serial == pin.serial;
	}
	if (!owns_group) {
		return ERR_DOES_NOT_EXIST;
	}
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.disposal_group_serial != serial) {
			continue;
		}
		found = true;
		if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			c.disposal.state.store(6, std::memory_order_release);
			c.output_role = ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
		}
	}
	// Only unbound metadata reservations are cancelled. Bound physical resources
	// remain exact obligations; this does not assert a factory/driver return.
	return found ? OK : ERR_DOES_NOT_EXIST;
}

bool RenderingDevice::_production_output_cache_observer_callback_supported_locked(RID rid, const GDExtensionDeviceExit::ProductionRequest &request, const GDExtensionDeviceExit::ProductionDisposalUse *required_observer) {
	const auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
	if (!set || !set->invalidated_callback || set->qualification_return || this != get_singleton() ||
			!UniformSetCacheRD::owns_invalidation_callback(rid, set->invalidated_callback, set->invalidated_callback_userdata)) {
		return false;
	}
	uint32_t kind;
	uint64_t serial, driver_id;
	auto **head = _production_disposal_head(rid, kind, serial, driver_id);
	if (!head || kind != 4 || !serial || !driver_id) {
		return false;
	}
	bool found = false;
	for (const auto *node = *head; node; node = node->next) {
		if (node->request != &request || node->request_serial != request.serial || node->state.load(std::memory_order_acquire) != 1) {
			continue;
		}
		if (!node->group || node->rid != rid.get_id() || node->kind != kind || node->serial != serial || node->native_driver_id != driver_id ||
				node->owned || node->qualified_borrow || !node->counter_dropped || node->counter_pins_remaining) {
			return false;
		}
		found |= !required_observer || node == required_observer;
	}
	return found;
}

bool RenderingDevice::_production_output_callback_supported(RID rid) {
	const auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
	if (!set || !set->invalidated_callback || (set->production_material_origin.is_valid() && set->production_material_origin_generation && RendererRD::MaterialStorage::_native_hold_callback_is_owned(set->production_material_origin, set->production_material_origin_generation, set->invalidated_callback, set->invalidated_callback_userdata, rid))) {
		return true;
	}
	// Only the active original drop can reach the ordinary free/callback path
	// on behalf of an exact observer. Recheck the current callback and owner.
	return production_disposal_drop_owner && _production_output_cache_observer_callback_supported_locked(rid, *production_disposal_drop_owner, nullptr);
}

void RenderingDevice::_production_output_set_material_origin(RID set_rid, RID material, uint64_t generation) {
	CRASH_COND(render_thread_id != Thread::get_caller_id());
	MutexLock lock(_thread_safe_);
	auto *set = uniform_set_owner.get_or_null(set_rid);
	if (!set) {
		return;
	}
	// This hint records actual ownership; ordinary callback replacement may
	// legitimately remove it without making a normal parameter refresh fatal.
	set->production_material_origin = RID();
	set->production_material_origin_generation = 0;
	if (material.is_null() || !generation || !RendererRD::MaterialStorage::_native_hold_callback_is_owned(material, generation, set->invalidated_callback, set->invalidated_callback_userdata, set_rid)) {
		return;
	}
	set->production_material_origin = material;
	set->production_material_origin_generation = generation;
}

bool RenderingDevice::_production_output_hold_contains(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial, RID rid) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial || rid.is_null()) {
		return false;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return false;
	}
	const auto &request = *pin.request;
	bool active = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &g = request.uses[i];
		if (g.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && g.disposal_group_serial == serial && g.disposal_group_pin_serial == pin.serial) {
			active = !g.disposal_group_release && !g.disposal_group_dropped;
		}
	}
	if (!active) {
		return false;
	}
	if (!_thread_safe_.try_lock()) {
		return false;
	}
	struct RdUnlock {
		Mutex &mutex;
		~RdUnlock() { mutex.unlock(); }
	} rd_unlock{ _thread_safe_ };
	uint32_t kind;
	uint64_t generation, driver_id;
	auto **head = _production_disposal_head(rid, kind, generation, driver_id);
	if (!head) {
		return false;
	}
	const auto *counter = _native_immutable_counter(rid);
	if (!counter || !*counter) {
		return false;
	}
	for (auto *node = *head; node; node = node->next) {
		if (node->request == &request && node->request_serial == request.serial && node->group == serial && !node->counter_dropped && node->state.load(std::memory_order_acquire) == 1 && node->rid == rid.get_id() && node->serial == generation && node->native_driver_id == driver_id && node->kind == kind) {
			return true;
		}
	}
	return false;
}

bool RenderingDevice::_production_output_owned_storage_contains(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, RID rid) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !group || rid.is_null()) {
		return false;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return false;
	}
	auto &request = *pin.request;
	bool original_group = false;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		const auto &row = request.uses[i];
		original_group |= row.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && row.disposal_group_serial == group && row.disposal_group_pin_serial == pin.serial && !row.disposal_group_release && !row.disposal_group_dropped;
	}
	if (!original_group || !_thread_safe_.try_lock()) {
		return false;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	const auto *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
	if (!buffer || buffer->native_disposal.cell.is_null()) {
		return false;
	}
	const auto &physical = **buffer->native_disposal.cell;
	if (physical.production_request != &request || physical.production_request_serial != request.serial || physical.production_group_serial != group || physical.production_pin_serial != pin.serial ||
			physical.device != uint64_t(get_instance_id()) || physical.state != NativeDisposalWitness::State::LIVE || !physical.matches(rid.get_id(), buffer->backing_serial, buffer->driver_id.id)) {
		return false;
	}
	// A failed, unsealed prefix has no immutable hold. Its exact original
	// factory provenance and intrusive member still establish owned custody.
	for (auto *node = buffer->production_disposal_uses; node; node = node->next) {
		if (node->request == &request && node->request_serial == request.serial && node->group == group && node->owned && node->state.load(std::memory_order_acquire) == 1 &&
				node->rid == rid.get_id() && node->serial == buffer->backing_serial && node->native_driver_id == buffer->driver_id.id && node->kind == 9) {
			return true;
		}
	}
	return false;
}

Error RenderingDevice::_production_output_owned_create_preflight_locked(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, GDExtensionDeviceExit::ProductionRequest *&request) {
	using namespace GDExtensionDeviceExit;
	Registry &registry = *pin.registry;
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	RenderingDevice *actual_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, request, actual_device, true);
	if (admitted != OK) {
		return admitted;
	}
	if (actual_device != this || request != pin.request || ((request->next_action || request->graph_count || request->native_count) && !registry.production_output_factory_pin_matches_locked(*request, pin, *this)) || request->capture_inflight) {
		return ERR_BUSY;
	}
	bool found = false;
	for (uint32_t i = request->use_count + request->binding_used; i < request->capacity.resource_uses; ++i) {
		const auto &c = request->uses[i];
		if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && c.disposal_group_serial == group && c.disposal_group_pin_serial == pin.serial && !c.disposal_group_release) {
			found = true;
		}
	}
	if (!found) {
		return ERR_DOES_NOT_EXIST;
	}
	for (uint32_t i = request->use_count + request->binding_used; i < request->capacity.resource_uses; ++i) {
		const auto &c = request->uses[i];
		if (c.disposal_group_serial == group && c.disposal_ordinal == ordinal) {
			return c.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED ? OK : ERR_ALREADY_IN_USE;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

Error RenderingDevice::_production_output_owned_storage_create(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, uint32_t bytes, BitField<StorageBufferUsage> usage, RID &rid, NativeDisposalWitness &witness) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !group || rid.is_valid() || witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	ProductionRequest *request = nullptr;
	const Error ready = _production_output_owned_create_preflight_locked(physical_context, pin, group, ordinal, request);
	if (ready != OK) {
		return ready;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	// The original factory owns native validation and rollback. Reservation alone
	// is arena custody; the original output owner must supply its allocation charge.
	registry.production_native_effect_enter_locked(*request);
	const Error created = _native_disposal_storage_create(bytes, usage, rid, witness);
	if (created != OK) {
		return created;
	}
	auto &physical = **witness.cell;
	physical.production_request = request;
	physical.production_request_serial = request->serial;
	physical.production_group_serial = group;
	physical.production_pin_serial = pin.serial;
	physical.production_ordinal = ordinal;
	return _production_output_owned_bind_locked(*request, pin.serial, group, ordinal, witness);
}

Error RenderingDevice::_production_output_owned_uniform_set_create(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, const VectorView<Uniform> &uniforms, RID shader, uint32_t index, RID &rid, NativeDisposalWitness &witness) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !group || rid.is_valid() || witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	ProductionRequest *request = nullptr;
	const Error ready = _production_output_owned_create_preflight_locked(physical_context, pin, group, ordinal, request);
	if (ready != OK) {
		return ready;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	// Every actual factory buffer must already belong to this exact reserved group.
	for (uint32_t i = 0; i < uniforms.size(); ++i) {
		const Uniform &uniform = uniforms[i];
		if (uniform.uniform_type != UNIFORM_TYPE_STORAGE_BUFFER || uniform.get_id_count() != 1) {
			return ERR_UNAVAILABLE;
		}
		const auto *buffer = storage_buffer_owner.get_or_null(uniform.get_id(0));
		if (!buffer || buffer->native_disposal.cell.is_null()) {
			return ERR_INVALID_DATA;
		}
		const auto &physical = **buffer->native_disposal.cell;
		if (physical.production_request != request || physical.production_request_serial != request->serial || physical.production_group_serial != group || physical.production_pin_serial != pin.serial) {
			return ERR_UNAUTHORIZED;
		}
	}
	registry.production_native_effect_enter_locked(*request);
	const Error created = _native_disposal_uniform_set_create(uniforms, shader, index, rid, witness, request, group, pin.serial, ordinal);
	if (created != OK) {
		return created;
	}
	auto &physical = **witness.cell;
	physical.production_request = request;
	physical.production_request_serial = request->serial;
	physical.production_group_serial = group;
	physical.production_pin_serial = pin.serial;
	physical.production_ordinal = ordinal;
	return _production_output_owned_bind_locked(*request, pin.serial, group, ordinal, witness);
}

Error RenderingDevice::_production_output_owned_uniform_set_create(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, const VectorView<Uniform> &uniforms, RID shader, uint32_t index, RID &rid, NativeDisposalWitness &witness, const VectorView<ProductionOutputUniformSource> &sources) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !group || rid.is_valid() || witness.cell.is_valid() || uniforms.size() == 0 || uniforms.size() != sources.size()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	Registry::Lock lock(registry);
	ProductionRequest *request = nullptr;
	Error error = _production_output_owned_create_preflight_locked(context, pin, group, ordinal, request);
	if (error != OK) {
		return error;
	}
	return _production_output_owned_uniform_set_create_locked(registry, *request, pin, group, ordinal, uniforms, shader, index, rid, witness, sources);
}

Error RenderingDevice::_production_output_owned_uniform_set_create(GDExtensionDeviceExit::ProductionOutputPreparationScope &scope, uint64_t group, uint32_t ordinal, const VectorView<Uniform> &uniforms, RID shader, uint32_t index, RID &rid, NativeDisposalWitness &witness, const VectorView<ProductionOutputUniformSource> &sources) {
	using namespace GDExtensionDeviceExit;
	if (!scope.registry || !scope.pin || !group || rid.is_valid() || witness.cell.is_valid() || !uniforms.size() || uniforms.size() != sources.size()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *scope.registry;
	Registry::Lock lock(registry);
	ProductionRequest *request = nullptr;
	Error error = registry.production_output_preparation_matches_locked(scope, *this, request);
	if (error != OK || request != scope.pin->request) {
		return error != OK ? error : ERR_UNAUTHORIZED;
	}
	return _production_output_owned_uniform_set_create_locked(registry, *request, *scope.pin, group, ordinal, uniforms, shader, index, rid, witness, sources);
}

Error RenderingDevice::_production_output_owned_uniform_set_create_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &original, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, const VectorView<Uniform> &uniforms, RID shader, uint32_t index, RID &rid, NativeDisposalWitness &witness, const VectorView<ProductionOutputUniformSource> &sources) {
	using namespace GDExtensionDeviceExit;
	ProductionRequest *request = &original;
	Error error = OK;
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	ProductionUseCell *destination = nullptr;
	error = _production_output_owned_destination_locked(*request, group, ordinal, destination);
	if (error != OK) {
		return error;
	}
	bool destination_group = false;
	for (uint32_t u = request->use_count + request->binding_used; u < request->capacity.resource_uses; ++u) {
		const auto &cell = request->uses[u];
		destination_group |= cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && cell.disposal_group_serial == group &&
				cell.disposal_group_pin_serial == pin.serial && ordinal < cell.disposal_group_count && !cell.disposal_group_release && !cell.disposal_group_dropped;
	}
	if (!destination_group) {
		return ERR_UNAUTHORIZED;
	}
	// Locate the actual original source-owning Acquire, independently of the
	// current successor Finalize ordinal. No copied source DTO grants access.
	const ProductionActionCell *acquire = nullptr;
	for (uint32_t a = 0; a < request->action_count; ++a) {
		const auto &action = request->actions[a];
		if (action.output_root == pin.serial && action.output_source_prepared) {
			if (acquire) {
				return ERR_INVALID_DATA;
			}
			acquire = &action;
		}
	}
	const Shader *source_shader = shader_owner.get_or_null(shader);
	if (!source_shader || index >= source_shader->uniform_sets.size()) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < uniforms.size(); ++i) {
		const auto &uniform = uniforms[i];
		const auto &source = sources[i];
		const auto *buffer = storage_buffer_owner.get_or_null(source.buffer);
		if (uniform.uniform_type != UNIFORM_TYPE_STORAGE_BUFFER || uniform.get_id_count() != 1 || uniform.get_id(0) != source.buffer ||
				!buffer || !source.byte_size || source.byte_offset > buffer->size || source.byte_size > buffer->size - source.byte_offset) {
			return ERR_INVALID_DATA;
		}
		if (source.owned_group) {
			if (source.resource_slot != UINT32_MAX || source.use_index != UINT32_MAX || source.owned_ordinal == UINT32_MAX || buffer->native_disposal.cell.is_null()) {
				return ERR_INVALID_DATA;
			}
			const auto &physical = **buffer->native_disposal.cell;
			if (physical.production_request != request || physical.production_request_serial != request->serial || physical.production_pin_serial != pin.serial ||
					physical.production_group_serial != source.owned_group || physical.production_ordinal != source.owned_ordinal ||
					physical.device != uint64_t(get_instance_id()) || physical.state != NativeDisposalWitness::State::LIVE ||
					!physical.matches(source.buffer.get_id(), buffer->backing_serial, buffer->driver_id.id)) {
				return ERR_UNAUTHORIZED;
			}
			bool retained = false;
			for (uint32_t u = request->use_count + request->binding_used; u < request->capacity.resource_uses; ++u) {
				const auto &member = request->uses[u];
				retained |= member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && member.disposal_group_serial == source.owned_group &&
						member.disposal_ordinal == source.owned_ordinal && member.disposal.owned && member.disposal.request == request &&
						member.disposal.request_serial == request->serial && member.disposal.rid == source.buffer.get_id() &&
						member.disposal.serial == buffer->backing_serial && member.disposal.state.load(std::memory_order_acquire) == 1;
			}
			if (!retained) {
				return ERR_UNAUTHORIZED;
			}
		} else {
			// An original READ use cannot authorize a writable shader binding.
			// Reflection comes from the actual selected native shader, not the
			// source projection supplied by this private factory caller.
			bool readonly_binding = false;
			for (const ShaderUniform &binding : source_shader->uniform_sets[index]) {
				if (binding.binding == uniform.binding) {
					if (binding.type != UNIFORM_TYPE_STORAGE_BUFFER || binding.writable) {
						return ERR_UNAUTHORIZED;
					}
					readonly_binding = true;
				}
			}
			if (!readonly_binding) {
				return ERR_UNAUTHORIZED;
			}
			if (source.owned_ordinal != UINT32_MAX || !acquire || !registry.production_output_source_matches_locked(*request, *acquire, *this) ||
					source.resource_slot >= request->resource_count || source.use_index >= request->use_count ||
					source.use_index < acquire->output_first_source_use || source.use_index - acquire->output_first_source_use >= acquire->output_source_use_count) {
				return ERR_UNAUTHORIZED;
			}
			const auto &resource = request->resources[source.resource_slot];
			const auto &use = request->uses[source.use_index];
			if (resource.primary != source.buffer || use.request != request || use.declaration.resource_slot != source.resource_slot ||
					use.declaration.access != GDEXTENSION_PRODUCTION_READ_V1 || use.declaration.byte_offset != source.byte_offset ||
					use.declaration.byte_size != source.byte_size ||
					(resource.output_predecessor_view.is_empty() ? buffer->qualification_return != resource.uses[0].physical :
						!registry.production_output_predecessor_matches_locked(*request, resource, *this))) {
				return ERR_UNAUTHORIZED;
			}
		}
	}
	registry.production_native_effect_enter_locked(*request);
	error = _native_disposal_uniform_set_create(uniforms, shader, index, rid, witness, request, group, pin.serial, ordinal, &sources);
	if (error != OK) {
		return error;
	}
	return _production_output_owned_bind_locked(*request, pin.serial, group, ordinal, witness);
}

Error RenderingDevice::_production_output_guard_begin_locked(GDExtensionDeviceExit::ProductionRequest &request, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t storage_group, uint64_t guard_group, const VectorView<RID> &resources) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || production_output_guard_recording.request || !storage_group || !guard_group || storage_group == guard_group ||
			!resources.size() || resources.size() > 12 || !native_immutable_is_render_thread() ||
			!pin.registry->production_output_factory_pin_matches_locked(request, pin, *this)) {
		return ERR_UNAUTHORIZED;
	}
	const auto &finalizer = request.actions[request.next_action];
	if (finalizer.output_acquire_action_ordinal >= request.next_action ||
			!request.actions[finalizer.output_acquire_action_ordinal].output_plan_prepared ||
			request.actions[finalizer.output_acquire_action_ordinal].output_plan.finalize_action_ordinal != request.next_action) {
		return ERR_UNAUTHORIZED;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	NativeDisposalWitness witnesses[12];
	for (uint32_t i = 0; i < resources.size(); ++i) {
		for (uint32_t j = 0; j < i; ++j) {
			if (resources[i] == resources[j]) {
				return ERR_INVALID_DATA;
			}
		}
		const auto *buffer = storage_buffer_owner.owns(resources[i]) ? storage_buffer_owner.get_or_null(resources[i]) : nullptr;
		const auto *set = uniform_set_owner.owns(resources[i]) ? uniform_set_owner.get_or_null(resources[i]) : nullptr;
		if (!buffer && !set) {
			return ERR_INVALID_DATA;
		}
		witnesses[i] = buffer ? buffer->native_disposal : set->native_disposal;
		if (witnesses[i].cell.is_null()) {
			return ERR_UNAUTHORIZED;
		}
		const auto &physical = **witnesses[i].cell;
		if (physical.production_request != &request || physical.production_request_serial != request.serial || physical.production_pin_serial != pin.serial ||
				physical.production_group_serial != (buffer ? storage_group : guard_group) || physical.state != NativeDisposalWitness::State::LIVE ||
				physical.device != uint64_t(get_instance_id()) || !physical.matches(resources[i].get_id(), buffer ? buffer->backing_serial : set->native_disposal_serial,
						buffer ? buffer->driver_id.id : set->driver_id.id)) {
			return ERR_UNAUTHORIZED;
		}
		bool member_found = false;
		for (uint32_t u = request.use_count + request.binding_used; u < request.capacity.resource_uses; ++u) {
			const auto &member = request.uses[u];
			member_found |= member.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && member.disposal_group_serial == physical.production_group_serial &&
					member.disposal_ordinal == physical.production_ordinal && member.disposal.owned && member.disposal.request == &request &&
					member.disposal.request_serial == request.serial && member.disposal.rid == resources[i].get_id() &&
					member.disposal.serial == physical.serial && member.disposal.state.load(std::memory_order_acquire) == 1;
		}
		if (!member_found) {
			return ERR_UNAUTHORIZED;
		}
	}
	auto &scope = production_output_guard_recording;
	scope.request = &request;
	scope.request_serial = request.serial;
	scope.pin_serial = pin.serial;
	scope.action_ordinal = request.next_action;
	scope.count = resources.size();
	for (uint32_t i = 0; i < scope.count; ++i) {
		scope.resources[i] = resources[i];
		scope.witnesses[i] = witnesses[i];
	}
	return OK;
}

void RenderingDevice::_production_output_guard_end_locked() {
	production_output_guard_recording = ProductionOutputGuardRecording();
}

Error RenderingDevice::_production_output_owned_seal(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !serial) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	ProductionRequest *request = nullptr;
	RenderingDevice *actual_device = nullptr;
	const Error admitted = registry.production_physical_request_locked(physical_context, { registry.instance, pin.request_serial }, request, actual_device, false);
	if (admitted != OK) {
		return admitted;
	}
	// RecordedOrdered is the frontier here. Native completion belongs to the
	// later return path; do not wait for GPU completion to freeze future writes.
	if (actual_device != this || request != pin.request || request->cancelled || request->capture_inflight || request->next_action != request->action_count || !request->graph_count || request->compute_list_open || request->effects_pending) {
		return ERR_BUSY;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (!registry.production_output_recorded_ordered_locked(*request, *this)) {
		return ERR_INVALID_DATA;
	}
	return _production_output_owned_seal_locked(*request, pin.serial, serial);
}

Error RenderingDevice::_production_output_owned_seal_locked(GDExtensionDeviceExit::ProductionRequest &request, uint64_t pin_serial, uint64_t serial) {
	using namespace GDExtensionDeviceExit;
	ProductionUseCell *group = nullptr;
	const uint32_t begin = request.use_count + request.binding_used;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP && c.disposal_group_serial == serial && c.disposal_group_pin_serial == pin_serial) {
			group = &c;
		}
	}
	if (!group) {
		return ERR_DOES_NOT_EXIST;
	}
	if (group->disposal_group_release || group->disposal_group_dropped) {
		return ERR_BUSY;
	}
	if (group->disposal_group_native_hold) {
		return OK;
	}
	if (native_immutable_hold_count == UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t count = 0;
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.disposal_group_serial != serial || &c == group) {
			continue;
		}
		if (c.output_role != ProductionUseCell::OutputRole::DISPOSAL_MEMBER || !c.disposal.owned || c.disposal.state.load(std::memory_order_acquire) != 1) {
			return ERR_BUSY;
		}
		auto &node = c.disposal;
		const RID rid = RID::from_uint64(node.rid);
		uint32_t kind;
		uint64_t incarnation, driver_id;
		auto **head = _production_disposal_head(rid, kind, incarnation, driver_id);
		bool *retiring = nullptr;
		const auto *counter = _native_immutable_counter(rid, &retiring);
		if (!head || node.serial != incarnation || node.kind != kind || node.native_driver_id != driver_id || !counter || *counter == UINT32_MAX || !retiring || *retiring) {
			return ERR_INVALID_DATA;
		}
		if (auto *buffer = _native_immutable_buffer(rid); buffer && buffer->transfer_worker_index >= 0) {
			return ERR_BUSY;
		}
		++count;
	}
	if (count != group->disposal_group_count) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = begin; i < request.capacity.resource_uses; ++i) {
		auto &c = request.uses[i];
		if (c.output_role == ProductionUseCell::OutputRole::DISPOSAL_MEMBER && c.disposal_group_serial == serial) {
			auto *counter = _native_immutable_counter(RID::from_uint64(c.disposal.rid));
			++*counter;
			c.disposal.counter_dropped = false;
			c.disposal.counter_pins_remaining = 1;
		}
	}
	++native_immutable_hold_count;
	group->disposal_group_native_hold = true;
	group->disposal_group_native_hold_count = 1;
	return OK;
}

Error RenderingDevice::_production_output_owned_pipeline_preflight(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group) {
	if (!pin.registry || !group) {
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionDeviceExit::Registry::Lock lock(*pin.registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	const Error ready = _production_output_owned_create_preflight_locked(physical_context, pin, group, 0, request);
	return ready == OK ? _native_disposal_preflight() : ready;
}

Error RenderingDevice::_production_output_owned_render_pipeline_create(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t group, uint32_t ordinal, RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants, RID &r_pipeline) {
	using namespace GDExtensionDeviceExit;
	if (!pin.registry || !group || r_pipeline.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	ProductionRequest *request = nullptr;
	const Error ready = _production_output_owned_create_preflight_locked(physical_context, pin, group, ordinal, request);
	if (ready != OK) {
		return ready;
	}
	return _production_output_owned_render_pipeline_create_locked(registry, *request, group, ordinal, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, r_pipeline);
}

Error RenderingDevice::_production_output_owned_pipeline_preflight(GDExtensionDeviceExit::ProductionOutputPreparationScope &scope) {
	if (!scope.registry) {
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionDeviceExit::Registry::Lock lock(*scope.registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	const Error ready = scope.registry->production_output_preparation_matches_locked(scope, *this, request);
	return ready == OK ? _native_disposal_preflight() : ready;
}

Error RenderingDevice::_production_output_owned_render_pipeline_create(GDExtensionDeviceExit::ProductionOutputPreparationScope &scope, RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants, RID &r_pipeline) {
	if (!scope.registry || &r_pipeline != scope.original_root || r_pipeline.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	GDExtensionDeviceExit::Registry &registry = *scope.registry;
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	GDExtensionDeviceExit::ProductionRequest *request = nullptr;
	const Error ready = registry.production_output_preparation_matches_locked(scope, *this, request);
	if (ready != OK) {
		return ready;
	}
	return _production_output_owned_render_pipeline_create_locked(registry, *request, scope.group, 0, p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, r_pipeline);
}

Error RenderingDevice::_production_output_owned_render_pipeline_create_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &request, uint64_t group, uint32_t ordinal, RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants, RID &r_pipeline) {
	using namespace GDExtensionDeviceExit;
	ProductionUseCell *reserved = nullptr;
	const Error destination = _production_output_owned_destination_locked(request, group, ordinal, reserved);
	if (destination != OK) {
		return destination;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	const Error native_ready = _native_disposal_preflight();
	if (native_ready != OK) {
		return native_ready;
	}
	// Factory publishes directly into this original cell; there is no fallible
	// post-creation capture/bind and no stack-owned created-pipeline prefix.
	registry.production_native_effect_enter_locked(request);
	r_pipeline = _qualification_render_pipeline_create_internal(p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, nullptr, RID(), reserved);
	return r_pipeline.is_valid() ? OK : ERR_CANT_CREATE;
}

Error RenderingDevice::_production_material_pipeline_destination_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &request, uint32_t index) {
	using namespace GDExtensionDeviceExit;
	ProductionRequest *actual = nullptr;
	RenderingDevice *device = nullptr;
	const Error found = registry.production_request_locked({ registry.instance, request.serial }, false, actual, device);
	if (found != OK || actual != &request || device != this || !request.attachment || !request.serial || index >= request.resource_count || index >= request.capacity.return_cells) {
		return ERR_INVALID_PARAMETER;
	}
	auto &slot = request.resources[index];
	auto &cell = slot.owned_physical;
	if (slot.request != &request || slot.request_serial != request.serial || slot.declaration.ownership != GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 ||
			request.returns[index].request != &request || slot.returned ||
			(slot.owned_serial && (!cell.serial || cell.bridge != request.attachment->serial || cell.child != slot.owned_serial || !slot.retire_serial || request.returns[index].serial != slot.retire_serial || !request.returns[index].pending))) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Error RenderingDevice::_production_material_pipeline_reserve_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &request, uint32_t index) {
	const Error destination = _production_material_pipeline_destination_locked(registry, request, index);
	if (destination != OK) {
		return destination;
	}
	GDExtensionDeviceExit::ProductionAttachment *attachment = nullptr;
	RenderingDevice *admitted_device = nullptr;
	const Error admission = registry.production_attachment_locked({ registry.instance, request.attachment->serial }, true, attachment, admitted_device);
	if (admission != OK || attachment != request.attachment || admitted_device != this) {
		return admission == OK ? ERR_UNAUTHORIZED : admission;
	}
	if (!native_immutable_is_render_thread() || !driver || production_finalization_started.load(std::memory_order_acquire) || request.cancelled || request.release_started || request.frozen || !_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock { Mutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{ _thread_safe_ };
	auto &slot = request.resources[index];
	auto &cell = slot.owned_physical;
	if (slot.primary.is_valid() || slot.material_pipeline_reservation.is_valid() || cell.rid || cell.free_event || cell.enqueued || cell.use_closed.load() || cell.event_capacity_reserved) {
		return ERR_INVALID_DATA;
	}
	if (!slot.owned_serial) {
		if (registry.production_serials.exhausted || registry.production_serials.next > UINT64_MAX - 3 || cell.serial || cell.bridge || cell.child || request.returns[index].pending || request.returns[index].serial) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		registry.production_serials.take(slot.owned_serial);
		registry.production_serials.take(slot.retire_serial);
		registry.production_serials.take(cell.serial);
		cell.bridge = request.attachment->serial;
		cell.child = slot.owned_serial;
		request.returns[index].serial = slot.retire_serial;
		request.returns[index].pending = true;
	}
	if (!_qualification_reserve_free_event(&cell)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	slot.material_pipeline_reservation = render_pipeline_owner.allocate_rid();
	if (slot.material_pipeline_reservation.is_null()) {
		_qualification_release_free_event(&cell);
		return ERR_OUT_OF_MEMORY;
	}
	return OK;
}

Error RenderingDevice::_production_material_pipeline_create_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &request, uint32_t index, NativeImmutableHold &hold, RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants) {
	const Error destination = _production_material_pipeline_destination_locked(registry, request, index);
	if (destination != OK) {
		return destination;
	}
	GDExtensionDeviceExit::ProductionAttachment *attachment = nullptr;
	RenderingDevice *admitted_device = nullptr;
	const Error admission = registry.production_attachment_locked({ registry.instance, request.attachment->serial }, true, attachment, admitted_device);
	if (admission != OK || attachment != request.attachment || admitted_device != this) {
		return admission == OK ? ERR_UNAUTHORIZED : admission;
	}
	if (!native_immutable_is_render_thread() || !driver || production_finalization_started.load(std::memory_order_acquire) || request.cancelled || request.release_started || request.frozen || !_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock { Mutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{ _thread_safe_ };
	auto &slot = request.resources[index];
	auto &cell = slot.owned_physical;
	if (!slot.owned_serial || !cell.serial || &hold != &slot.material_pipeline_shader_hold || hold.device != this || hold.pins_returned || hold.pipeline_borrowers == UINT32_MAX ||
			slot.primary.is_valid() || slot.use_count || !slot.material_pipeline_reservation.is_valid() || !render_pipeline_owner.owns(slot.material_pipeline_reservation) ||
			!cell.event_capacity_reserved || cell.reserved_event || cell.rid || cell.enqueued || cell.free_event || cell.use_closed.load()) {
		return ERR_INVALID_PARAMETER;
	}
	bool shader_held = false;
	for (RID rid : hold._resource_span()) {
		shader_held |= rid == p_shader;
	}
	const auto *shader = shader_owner.owns(p_shader) ? shader_owner.get_or_null(p_shader) : nullptr;
	if (!shader_held || !shader || shader->qualification_return || native_immutable_resource_is_retiring(p_shader)) {
		return ERR_INVALID_PARAMETER;
	}
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	Shader *validated = nullptr;
	FramebufferFormat format;
	RDD::VertexFormatID vertex_format;
	const Error values = _native_render_pipeline_validate_values(p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive,
			p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags,
			p_for_render_pass, p_specialization_constants, nullptr, RID(), nullptr, false, nullptr, validated, format, vertex_format);
	if (values != OK) {
		return values;
	}
	registry.production_native_effect_enter_locked(request);
	slot.primary = _native_render_pipeline_create_internal(p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, &cell, slot.material_pipeline_reservation, nullptr, &hold);
	if (slot.primary.is_null()) {
		return ERR_CANT_CREATE; // Reservation and original hold remain for cancellation.
	}
	slot.material_pipeline_reservation = RID();
	slot.backing_serial = cell.serial;
	cell.rid = slot.primary.get_id();
	const Error attached = slot.uses[0].attach(cell, request, request.serial);
	if (attached == OK) {
		slot.use_count = 1;
		slot.imported = true;
	}
	return attached; // A valid error prefix remains in the original slot.
}

Error RenderingDevice::_production_material_pipeline_cleanup_locked(GDExtensionDeviceExit::Registry &registry, GDExtensionDeviceExit::ProductionRequest &request, uint32_t index) {
	const Error destination = _production_material_pipeline_destination_locked(registry, request, index);
	if (destination != OK) {
		return destination;
	}
	if (!native_immutable_is_render_thread() || !_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock { Mutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{ _thread_safe_ };
	auto &slot = request.resources[index];
	if (slot.material_pipeline_shader_hold.pipeline_borrowers || (slot.primary.is_valid() && !slot.owned_physical.free_event)) {
		return ERR_BUSY;
	}
	if (slot.use_count) {
		return ERR_BUSY; // Original owner must join and unpin its real uses first.
	}
	if (slot.material_pipeline_reservation.is_valid()) {
		render_pipeline_owner.initialize_rid(slot.material_pipeline_reservation, RenderPipeline());
		render_pipeline_owner.free(slot.material_pipeline_reservation);
		slot.material_pipeline_reservation = RID();
		_qualification_release_free_event(&slot.owned_physical);
	}
	if (slot.material_pipeline_shader_hold.is_active()) {
		const Error released = native_immutable_hold_release(slot.material_pipeline_shader_hold);
		if (released != OK) {
			return released;
		}
	}
	// Only absence of a native prefix, or its actual free event, reaches here.
	slot.returned = slot.primary.is_valid();
	slot.imported = false;
	request.returns[index].pending = false;
	return OK;
}
