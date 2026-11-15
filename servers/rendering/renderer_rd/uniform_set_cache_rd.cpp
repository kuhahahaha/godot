/**************************************************************************/
/*  uniform_set_cache_rd.cpp                                              */
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

#include "uniform_set_cache_rd.h"

#include "core/object/class_db.h"
#include "servers/rendering/rendering_device_binds.h"

UniformSetCacheRD *UniformSetCacheRD::singleton = nullptr;

void UniformSetCacheRD::_bind_methods() {
	ClassDB::bind_static_method("UniformSetCacheRD", D_METHOD("get_cache", "shader", "set", "uniforms"), &UniformSetCacheRD::get_cache_array);
}

RID UniformSetCacheRD::get_cache_array(RID p_shader, uint32_t p_set, const TypedArray<RDUniform> &p_uniforms) {
	thread_local LocalVector<RD::Uniform> uniforms;
	uniforms.clear();

	for (int i = 0; i < p_uniforms.size(); i++) {
		Ref<RDUniform> uniform = p_uniforms[i];
		if (uniform.is_valid()) {
			uniforms.push_back(uniform->base);
		}
	}

	return UniformSetCacheRD::get_singleton()->get_cache_vec(p_shader, p_set, uniforms);
}

bool UniformSetCacheRD::owns_invalidation_callback(RID p_uniform_set, RD::InvalidationCallback p_callback, const void *p_userdata) {
	if (p_callback != _uniform_set_invalidation_callback || !p_uniform_set.is_valid() || !p_userdata) {
		return false;
	}
	const RD *rd = RD::get_singleton();
	if (!rd || !rd->native_immutable_is_render_thread() || !singleton) {
		return false;
	}

	MutexLock lock(singleton->live_entries_mutex);
	return singleton->live_entries.has(p_userdata) && static_cast<const Cache *>(p_userdata)->cache == p_uniform_set;
}

void UniformSetCacheRD::_invalidate(Cache *p_cache) {
	{
		MutexLock lock(live_entries_mutex);
		if (!live_entries.erase(p_cache)) {
			return;
		}
	}
	if (p_cache->prev) {
		p_cache->prev->next = p_cache->next;
	} else {
		// At beginning of table
		uint32_t table_idx = p_cache->hash % HASH_TABLE_SIZE;
		hash_table[table_idx] = p_cache->next;
	}

	if (p_cache->next) {
		p_cache->next->prev = p_cache->prev;
	}

	cache_allocator.free(p_cache);
}
void UniformSetCacheRD::_uniform_set_invalidation_callback(void *p_userdata) {
	if (singleton) {
		singleton->_invalidate(static_cast<Cache *>(p_userdata));
	}
}

UniformSetCacheRD::UniformSetCacheRD() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

UniformSetCacheRD::~UniformSetCacheRD() {
	if (singleton == this) {
		singleton = nullptr;
	}
	if (!live_entries.is_empty()) {
		ERR_PRINT("At exit: " + itos(live_entries.size()) + " uniform set cache instance(s) still in use.");
	}
}
