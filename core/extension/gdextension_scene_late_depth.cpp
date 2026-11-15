/**************************************************************************/
/*  gdextension_scene_late_depth.cpp                                      */
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

#include "gdextension_scene_late_depth.h"

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_scene_phase_bind.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "servers/rendering/rendering_device.h"

namespace GDExtensionDeviceExit {

namespace {
struct CachedLateDepthEntry {
	LateTransparentDepthDescriptor descriptor;
	RenderingDevice *publisher_device = nullptr;
	uint64_t publish_seq = 0;
};

thread_local const LateTransparentDepthDescriptor *g_active_late_depth_descriptor = nullptr;
Mutex g_last_late_depth_mutex;
HashMap<uint64_t, CachedLateDepthEntry> g_last_late_depth_by_viewport;
CachedLateDepthEntry g_last_unscoped_late_depth_entry;
bool g_has_last_unscoped_late_depth_descriptor = false;
uint64_t g_next_late_depth_publish_seq = 1;

bool is_late_depth_entry_live(const CachedLateDepthEntry &p_entry) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd || p_entry.publisher_device != rd || p_entry.descriptor.depth_texture.is_null()) {
		return false;
	}
	return rd->texture_is_valid(p_entry.descriptor.depth_texture);
}

void purge_stale_late_depth_entries_locked() {
	LocalVector<uint64_t> stale_keys;
	for (const KeyValue<uint64_t, CachedLateDepthEntry> &kv : g_last_late_depth_by_viewport) {
		if (!is_late_depth_entry_live(kv.value)) {
			stale_keys.push_back(kv.key);
		}
	}
	for (uint32_t i = 0; i < stale_keys.size(); ++i) {
		g_last_late_depth_by_viewport.erase(stale_keys[i]);
	}
}
} // namespace

LateTransparentDepthContent::LateTransparentDepthContent(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample,
		const LateTransparentDepthDescriptor &p_descriptor,
		SafeNumeric<uint32_t> *p_active_counter) :
		expected_device(p_device),
		expected_sample(p_sample),
		descriptor(p_descriptor),
		active_lease_counter(p_active_counter) {
	if (active_lease_counter) {
		active_lease_counter->increment();
	}
}

LateTransparentDepthContent::~LateTransparentDepthContent() {
	if (!returned && active_lease_counter) {
		active_lease_counter->decrement();
		returned = true;
	}
}

Error LateTransparentDepthContent::validate(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample) const {
	if (returned) {
		return ERR_UNAVAILABLE;
	}
	if (scene_opaque32_is_zero(p_device) || scene_opaque32_is_zero(p_sample)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!scene_opaque32_equal(p_device, expected_device) || !scene_opaque32_equal(p_sample, expected_sample)) {
		return ERR_UNAUTHORIZED;
	}
	if (descriptor.depth_texture.is_null() || descriptor.width == 0 || descriptor.height == 0 || descriptor.view_count == 0 ||
			!Math::is_finite(descriptor.z_near) || !Math::is_finite(descriptor.z_far) ||
			descriptor.z_near <= 0.0f || descriptor.z_far <= descriptor.z_near) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Error LateTransparentDepthContent::return_after_reads() {
	if (!returned) {
		returned = true;
		descriptor.depth_texture = RID();
		if (active_lease_counter) {
			active_lease_counter->decrement();
		}
	}
	return OK;
}

GDExtensionSceneOpaque32V1 make_late_depth_sample_token(uint64_t p_viewport_serial, uint64_t p_frame_number, uint32_t p_width, uint32_t p_height) {
	const uint64_t packed_dims = (uint64_t(p_width) << 32u) | uint64_t(p_height);
	return scene_phase_opaque(LATE_DEPTH_SAMPLE_KIND, p_viewport_serial, p_frame_number, packed_dims);
}

bool has_active_late_transparent_depth_phase(const RenderingDevice *p_device, RID p_viewport) {
	return Registry::get_singleton().has_active_scene_phase_sink(SCENE_PHASE_LATE_TRANSPARENT_DEPTH, p_device, p_viewport);
}

void dispatch_late_transparent_depth_phase(RenderingDevice &p_device) {
	Registry::get_singleton().scene_phase_dispatch_for_phase(p_device, SCENE_PHASE_LATE_TRANSPARENT_DEPTH, RID());
}

void dispatch_late_transparent_depth_phase(RenderingDevice &p_device, const LateTransparentDepthDescriptor &p_descriptor) {
	{
		MutexLock lock(g_last_late_depth_mutex);
		CachedLateDepthEntry entry;
		entry.descriptor = p_descriptor;
		entry.publisher_device = &p_device;
		entry.publish_seq = g_next_late_depth_publish_seq++;

		if (p_descriptor.viewport.is_valid()) {
			const uint64_t vp_id = p_descriptor.viewport.get_id();
			if (!g_last_late_depth_by_viewport.has(vp_id) && g_last_late_depth_by_viewport.size() >= 64) {
				purge_stale_late_depth_entries_locked();
			}
			if (!g_last_late_depth_by_viewport.has(vp_id) && g_last_late_depth_by_viewport.size() >= 64) {
				uint64_t oldest_vp = 0;
				uint64_t oldest_seq = UINT64_MAX;
				bool has_oldest = false;
				for (const KeyValue<uint64_t, CachedLateDepthEntry> &kv : g_last_late_depth_by_viewport) {
					if (!has_oldest || kv.value.publish_seq < oldest_seq) {
						oldest_vp = kv.key;
						oldest_seq = kv.value.publish_seq;
						has_oldest = true;
					}
				}
				if (has_oldest) {
					g_last_late_depth_by_viewport.erase(oldest_vp);
				}
			}
			if (is_late_depth_entry_live(entry)) {
				g_last_late_depth_by_viewport[vp_id] = entry;
			} else {
				g_last_late_depth_by_viewport.erase(vp_id);
			}
		} else {
			if (is_late_depth_entry_live(entry)) {
				g_last_unscoped_late_depth_entry = entry;
				g_has_last_unscoped_late_depth_descriptor = true;
			} else {
				g_last_unscoped_late_depth_entry = CachedLateDepthEntry();
				g_has_last_unscoped_late_depth_descriptor = false;
			}
		}
	}
	const LateTransparentDepthDescriptor *prev = g_active_late_depth_descriptor;
	g_active_late_depth_descriptor = &p_descriptor;
	Registry::get_singleton().scene_phase_dispatch_for_phase(p_device, SCENE_PHASE_LATE_TRANSPARENT_DEPTH, p_descriptor.viewport);
	g_active_late_depth_descriptor = prev;
}

bool get_current_late_transparent_depth_descriptor(LateTransparentDepthDescriptor &r_descriptor) {
	if (!g_active_late_depth_descriptor) {
		return false;
	}
	r_descriptor = *g_active_late_depth_descriptor;
	return true;
}

bool get_last_published_late_transparent_depth_descriptor(RID p_viewport, LateTransparentDepthDescriptor &r_descriptor) {
	MutexLock lock(g_last_late_depth_mutex);
	if (p_viewport.is_valid()) {
		const uint64_t vp_id = p_viewport.get_id();
		const CachedLateDepthEntry *found = g_last_late_depth_by_viewport.getptr(vp_id);
		if (!found) {
			return false;
		}
		if (!is_late_depth_entry_live(*found)) {
			g_last_late_depth_by_viewport.erase(vp_id);
			return false;
		}
		r_descriptor = found->descriptor;
		return true;
	}
	if (!g_has_last_unscoped_late_depth_descriptor) {
		return false;
	}
	if (!is_late_depth_entry_live(g_last_unscoped_late_depth_entry)) {
		g_last_unscoped_late_depth_entry = CachedLateDepthEntry();
		g_has_last_unscoped_late_depth_descriptor = false;
		return false;
	}
	r_descriptor = g_last_unscoped_late_depth_entry.descriptor;
	return true;
}

} // namespace GDExtensionDeviceExit
