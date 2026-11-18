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
#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_scene_phase_bind.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
#include "servers/rendering/rendering_device.h"

#include <cstring>

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

GDExtensionSceneOpaque32V1 make_scene_depth_snapshot_sample_token(uint64_t p_viewport_serial, uint64_t p_snapshot_generation, uint32_t p_width, uint32_t p_height) {
	const uint64_t packed_dims = (uint64_t(p_width) << 32u) | uint64_t(p_height);
	return scene_phase_opaque(SCENE_DEPTH_SNAPSHOT_SAMPLE_KIND, p_viewport_serial, p_snapshot_generation, packed_dims);
}

Error Registry::scene_depth_snapshot_arm(
		GDExtensionProductionAttachmentRefV1 p_attachment,
		const GDExtensionSceneDepthSnapshotArmV1 &p_input) {
	if (p_input.abi.abi_version != 1u ||
			p_input.abi.struct_size != sizeof(GDExtensionSceneDepthSnapshotArmV1) ||
			p_input.abi.reserved != 0u ||
			p_input.viewport_rid == 0ull ||
			!Math::is_finite(p_input.slab_thickness_m) ||
			!Math::is_finite(p_input.silhouette_jump_m) ||
			p_input.slab_thickness_m < 0.0f ||
			p_input.silhouette_jump_m < 0.0f) {
		return ERR_INVALID_PARAMETER;
	}

	RenderingDevice *main_rd = RenderingDevice::get_singleton();
	RendererRD::ParticlesCollisionSceneRD *collision_scene = RendererRD::ParticlesCollisionSceneRD::get_for_device(main_rd);
	if (!main_rd || !main_rd->is_main_instance || !collision_scene || !collision_scene->is_initialized()) {
		return ERR_UNAVAILABLE;
	}

	{
		Lock lock(*this, Lock::BLOCK_OR_BORROW);
		ProductionAttachment *attachment = nullptr;
		RenderingDevice *device = nullptr;
		const Error err = production_attachment_locked(p_attachment, true, attachment, device);
		if (err != OK) {
			return err;
		}
		if (!device || device != main_rd) {
			return ERR_UNAVAILABLE;
		}
		if (!attachment->scene_registration || attachment->scene_closed ||
				attachment->scene_config.phase != SCENE_PHASE_BEFORE_PARTICLES) {
			return ERR_UNAUTHORIZED;
		}
	}

	return collision_scene->request_viewport_depth_snapshot(
			RID::from_uint64(p_input.viewport_rid),
			p_input.slab_thickness_m,
			p_input.silhouette_jump_m);
}

Error Registry::scene_depth_snapshot_borrow(
		ProductionPhysicalContextRef p_context,
		GDExtensionProductionRequestRefV1 p_request,
		uint64_t p_action_ordinal,
		RID p_viewport,
		const GDExtensionSceneOperationKeyV1 &p_key,
		GDExtensionSceneDepthSnapshotBorrowResultV1 &r_out) {
	r_out = {};
	r_out.abi = { 1u, uint32_t(sizeof(r_out)), 0u };
	r_out.outcome.abi = { 1u, uint32_t(sizeof(r_out.outcome)), 0u };
	auto fail = [&](Error p_err) {
		r_out.outcome.native_error = p_err;
		return p_err;
	};

	if (p_viewport.is_null()) {
		return fail(ERR_INVALID_PARAMETER);
	}
	const Error env_err = validate_scene_operation_key_envelope(p_key);
	if (env_err != OK) {
		return fail(env_err);
	}

	RenderingDevice *main_rd = RenderingDevice::get_singleton();
	RendererRD::ParticlesCollisionSceneRD *collision_scene = RendererRD::ParticlesCollisionSceneRD::get_for_device(main_rd);
	if (!main_rd || !main_rd->is_main_instance || !collision_scene || !collision_scene->is_initialized()) {
		return fail(ERR_UNAVAILABLE);
	}

	GDExtensionSceneOpaque32V1 expected_dev_cap = {};
	Ref<RendererRD::ParticlesCollisionSceneRD::DepthReadLease> lease;
	GDExtensionSceneOpaque32V1 sample_token = {};
	bool already_decided = false;
	{
		Lock lock(*this, Lock::BLOCK_OR_BORROW);
		ProductionRequest *request = nullptr;
		RenderingDevice *device = nullptr;
		const Error err = production_physical_request_locked(p_context, p_request, request, device, false);
		if (err != OK) {
			return fail(err);
		}
		if (!device || device != main_rd) {
			return fail(ERR_UNAVAILABLE);
		}
		if (!request->attachment->scene_registration || request->attachment->scene_closed ||
				request->attachment->scene_config.phase != SCENE_PHASE_BEFORE_PARTICLES) {
			return fail(ERR_UNAUTHORIZED);
		}
		if (p_action_ordinal >= request->action_count) {
			return fail(ERR_INVALID_DATA);
		}
		const auto &action = request->actions[p_action_ordinal];
		if (!action.scene_key_bound || memcmp(&action.scene_key, &p_key, sizeof(p_key)) != 0) {
			return fail(ERR_INVALID_DATA);
		}
		if (action.original_append.state != CheckedRecordAppend::State::Rejected) {
			already_decided = true;
		} else {
			expected_dev_cap = request->attachment->scene_config.device_capability;
			if (action.scene_attempt.content.is_valid() &&
					scene_phase_word(action.scene_attempt.sample, 0) == SCENE_DEPTH_SNAPSHOT_SAMPLE_KIND) {
				lease = Ref<RendererRD::ParticlesCollisionSceneRD::DepthReadLease>(
						static_cast<RendererRD::ParticlesCollisionSceneRD::DepthReadLease *>(action.scene_attempt.content.ptr()));
				sample_token = action.scene_attempt.sample;
			}
		}
	}

	if (already_decided) {
		return scene_phase_borrow(p_context, p_request, p_action_ordinal, p_key, {}, Ref<SceneReadContent>(), r_out.outcome);
	}

	bool newly_acquired = false;
	if (lease.is_null()) {
		const Error acq_err = collision_scene->acquire_depth_snapshot_read_lease(
				p_viewport, lease, sample_token, &expected_dev_cap);
		if (acq_err != OK) {
			return fail(acq_err);
		}
		newly_acquired = true;
	}

	const Error borrow_err = scene_phase_borrow(
			p_context, p_request, p_action_ordinal, p_key, sample_token, lease, r_out.outcome);
	if (borrow_err != OK || r_out.outcome.outcome != 1u) {
		if (newly_acquired && lease.is_valid()) {
			lease->return_after_reads();
			lease.unref();
		}
		return borrow_err;
	}

	Ref<RendererRD::ParticlesCollisionSceneRD::DepthSnapshotGeneration> gen = lease->get_generation();
	if (gen.is_null()) {
		return fail(ERR_UNAVAILABLE);
	}
	r_out.sample_token = sample_token;
	r_out.meta_ssbo_rid = gen->meta_buffer.get_id();
	r_out.depth_texture_rid = gen->depth_texture.get_id();
	r_out.snapshot_generation = gen->meta.snapshot_generation;
	r_out.width = gen->meta.width;
	r_out.height = gen->meta.height;
	r_out.flags = gen->meta.flags;
	r_out.reserved = 0u;
	return OK;
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
