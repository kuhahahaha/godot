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

#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"

RenderingDevice::Buffer *RenderingDevice::_native_immutable_buffer(RID rid) {
	if (auto *buffer = vertex_buffer_owner.get_or_null(rid)) {
		return buffer;
	}
	if (auto *buffer = index_buffer_owner.get_or_null(rid)) {
		return buffer;
	}
	if (auto *buffer = uniform_buffer_owner.get_or_null(rid)) {
		return buffer;
	}
	if (auto *buffer = storage_buffer_owner.get_or_null(rid)) {
		return buffer;
	}
	return texture_buffer_owner.get_or_null(rid);
}

void RenderingDevice::_native_immutable_retire_dependents(RID rid) {
	// Original dependency objects remain physical owners while pinned. Consume
	// their original logical invalidation callbacks now, not after userdata died.
	LocalVector<RID> retiring;
	retiring.push_back(rid);
	for (uint32_t i = 0; i < retiring.size(); ++i) {
		const RID current = retiring[i];
		const auto *buffer = _native_immutable_buffer(current);
		const auto *texture = texture_owner.get_or_null(current);
		const auto *shader = shader_owner.get_or_null(current);
		const auto *pipeline = compute_pipeline_owner.get_or_null(current);
		const auto *sampler = sampler_owner.get_or_null(current);
		auto *set = uniform_set_owner.get_or_null(current);
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
			callback(userdata);
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
	if (auto *resource = texture_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = sampler_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = shader_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = uniform_set_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = render_pipeline_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = compute_pipeline_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = vertex_array_owner.get_or_null(rid)) {
		return counter(resource);
	}
	if (auto *resource = index_array_owner.get_or_null(rid)) {
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
	const auto *texture = const_cast<RenderingDevice *>(this)->texture_owner.get_or_null(rid);
	return texture && texture->owner.is_valid() && native_immutable_resource_is_held(texture->owner);
}

bool RenderingDevice::native_immutable_resource_is_retiring(RID rid) const {
	MutexLock lock(_thread_safe_);
	bool *retiring = nullptr;
	const auto *holds = const_cast<RenderingDevice *>(this)->_native_immutable_counter(rid, &retiring);
	if (holds && *retiring) {
		return true;
	}
	const auto *texture = const_cast<RenderingDevice *>(this)->texture_owner.get_or_null(rid);
	return texture && texture->owner.is_valid() && native_immutable_resource_is_retiring(texture->owner);
}

bool RenderingDevice::native_immutable_pipeline_matches_shader(RID pipeline, RID shader) const {
	MutexLock lock(_thread_safe_);
	const auto *original = const_cast<RenderingDevice *>(this)->shader_owner.get_or_null(shader);
	if (!original || !original->driver_id) {
		return false;
	}
	if (const auto *physical = const_cast<RenderingDevice *>(this)->render_pipeline_owner.get_or_null(pipeline)) {
		return physical->shader == shader && physical->shader_driver_id == original->driver_id;
	}
	if (const auto *physical = const_cast<RenderingDevice *>(this)->compute_pipeline_owner.get_or_null(pipeline)) {
		return physical->shader == shader && physical->shader_driver_id == original->driver_id;
	}
	return false;
}

bool RenderingDevice::native_immutable_uniform_set_matches_pipeline(RID set_rid, RID pipeline_rid, uint32_t set_index) const {
	MutexLock lock(_thread_safe_);
	auto *device = const_cast<RenderingDevice *>(this);
	const Vector<uint32_t> *formats = nullptr;
	if (const auto *pipeline = device->render_pipeline_owner.get_or_null(pipeline_rid); pipeline && pipeline->driver_id) {
		formats = &pipeline->set_formats;
	}
	if (const auto *pipeline = device->compute_pipeline_owner.get_or_null(pipeline_rid); pipeline && pipeline->driver_id) {
		formats = &pipeline->set_formats;
	}
	if (!formats) {
		return false;
	}
	const uint32_t expected = set_index < uint32_t(formats->size()) ? (*formats)[set_index] : 0;
	if (set_rid.is_null()) {
		return expected == 0;
	}
	const auto *set = device->uniform_set_owner.get_or_null(set_rid);
	return set && set->driver_id && set->format && set->format == expected;
}

bool RenderingDevice::_native_immutable_uniform_set_writes(const UniformSet &set) const {
	for (RID rid : set.writable_resources) {
		if (native_immutable_resource_is_held(rid) || native_immutable_resource_is_retiring(rid)) {
			return true;
		}
	}
	return false;
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
	if (native_immutable_hold_count == UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	Vector<RID> resources;
	if (resources.resize(limit) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
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
		if (*holds == UINT32_MAX) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (const auto *resource = _native_immutable_buffer(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = texture_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = shader_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = sampler_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = uniform_set_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = uniform_set_owner.get_or_null(rid); resource && !resource->writable_resources.is_empty()) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = render_pipeline_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = compute_pipeline_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *resource = index_array_owner.get_or_null(rid); resource && !resource->driver_id) {
			return ERR_UNAVAILABLE;
		}

		if (const auto *buffer = _native_immutable_buffer(rid); buffer && buffer->qualification_return && buffer->qualification_return->use_closed.load()) {
			return ERR_BUSY;
		}
		if (const auto *buffer = _native_immutable_buffer(rid); buffer && (buffer->transfer_worker_index >= 0 || buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT) || buffer->usage.has_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT))) {
			return ERR_UNAVAILABLE;
		}
		if (const auto *texture = texture_owner.get_or_null(rid)) {
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
		resources.write[count++] = rid;
		return OK;
	};
	for (uint32_t i = 0; i < roots.size(); ++i) {
		const Error error = append(roots[i]);
		if (error != OK) {
			return error;
		}
	}
	for (uint32_t i = 0; i < count; ++i) {
		if (const auto *dependencies = reverse_dependency_map.getptr(resources[i])) {
			for (RID dependency : *dependencies) {
				const Error error = append(dependency);
				if (error != OK) {
					return error;
				}
			}
		}
		// Shader factories preserve the actual embedded sampler RIDs. These
		// physical owners share this hold's bounded atomic closure, without
		// imposing a new legacy shader/sampler dependency relationship.
		if (const auto *shader = shader_owner.get_or_null(resources[i])) {
			for (RID sampler : shader->native_immutable_samplers) {
				const Error error = append(sampler);
				if (error != OK) {
					return error;
				}
			}
		}
		// Native texture views must retain the actual original parent even when
		// an older owner did not add a graph dependency for the view.
		if (const auto *texture = texture_owner.get_or_null(resources[i]); texture && texture->owner.is_valid()) {
			const Error error = append(texture->owner);
			if (error != OK) {
				return error;
			}
		}
	}
	if (resources.resize(count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	for (RID rid : resources) {
		++*_native_immutable_counter(rid);
	}
	out.resources = static_cast<Vector<RID> &&>(resources);
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
	if (!hold.pins_returned) {
		for (RID rid : hold.resources) {
			auto *holds = _native_immutable_counter(rid);
			CRASH_COND(!holds || !*holds);
			--*holds;
		}
		hold.pins_returned = true;
	}
	bool pending = false;
	for (int64_t i = hold.resources.size() - 1; i >= 0; --i) {
		const RID rid = hold.resources[i];
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
		free_rid(rid);
		if (_native_immutable_counter(rid)) {
			pending = true;
		}
	}
	if (pending) {
		return ERR_BUSY;
	}
	CRASH_COND(!native_immutable_hold_count);
	--native_immutable_hold_count;
	hold.resources.clear();
	hold.device = nullptr;
	hold.pins_returned = false;
	return OK;
}
