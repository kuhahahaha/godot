/**************************************************************************/
/*  particles_collision_scene_rd.cpp                                      */
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

#include "particles_collision_scene_rd.h"

#include "core/extension/gdextension_scene_late_depth.h"
#include "core/extension/gdextension_scene_phase_bind.h"
#include "core/math/vector4.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

#include <cmath>
#include <cstring>

namespace RendererRD {

namespace {

constexpr uint64_t SCENE_SPHERE_SAMPLE_KIND = 0x4b30365350485231ull; // "K06SPHR1"

Mutex g_live_collision_scenes_mutex;
LocalVector<ParticlesCollisionSceneRD *> g_live_collision_scenes;

Vector4 mat4_mul_vec4_col_major(const float p_m[16], const Vector4 &p_v) {
	return Vector4(
			p_m[0] * p_v.x + p_m[4] * p_v.y + p_m[8] * p_v.z + p_m[12] * p_v.w,
			p_m[1] * p_v.x + p_m[5] * p_v.y + p_m[9] * p_v.z + p_m[13] * p_v.w,
			p_m[2] * p_v.x + p_m[6] * p_v.y + p_m[10] * p_v.z + p_m[14] * p_v.w,
			p_m[3] * p_v.x + p_m[7] * p_v.y + p_m[11] * p_v.z + p_m[15] * p_v.w);
}

Transform3D transform_from_col_major_mat4(const float p_m[16]) {
	Transform3D xf;
	xf.basis.set_column(0, Vector3(p_m[0], p_m[1], p_m[2]));
	xf.basis.set_column(1, Vector3(p_m[4], p_m[5], p_m[6]));
	xf.basis.set_column(2, Vector3(p_m[8], p_m[9], p_m[10]));
	xf.origin = Vector3(p_m[12], p_m[13], p_m[14]);
	return xf;
}

Vector3 sanitize_vec3(const Vector3 &p_v, const Vector3 &p_fallback = Vector3()) {
	if (Math::is_finite(p_v.x) && Math::is_finite(p_v.y) && Math::is_finite(p_v.z)) {
		return p_v;
	}
	return p_fallback;
}

bool vec3_is_finite(const Vector3 &p_v) {
	return Math::is_finite(p_v.x) && Math::is_finite(p_v.y) && Math::is_finite(p_v.z);
}

bool transform_is_finite(const Transform3D &p_xf) {
	if (!vec3_is_finite(p_xf.origin)) {
		return false;
	}
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Math::is_finite(p_xf.basis.rows[r][c])) {
				return false;
			}
		}
	}
	return Math::abs(p_xf.basis.determinant()) > 1e-12f;
}

Vector3 apply_bounce_and_friction(
		const Vector3 &p_velocity,
		const Vector3 &p_normal,
		float p_restitution,
		float p_friction) {
	if (!vec3_is_finite(p_velocity) || !vec3_is_finite(p_normal)) {
		return sanitize_vec3(p_velocity);
	}
	const float vn = p_velocity.dot(p_normal);
	if (!(vn < 0.0f)) {
		return p_velocity;
	}
	const Vector3 v_normal = p_normal * vn;
	const Vector3 v_tangent = p_velocity - v_normal;
	const float safe_restitution = Math::is_finite(p_restitution) ? CLAMP(p_restitution, 0.0f, 1.0f) : 0.0f;
	const float safe_friction = Math::is_finite(p_friction) ? CLAMP(p_friction, 0.0f, 1.0f) : 0.0f;
	return v_tangent * (1.0f - safe_friction) - v_normal * safe_restitution;
}

float reverse_z_to_linear_depth(float p_raw_d32, float p_z_near, float p_z_far, bool p_reverse_z) {
	const float clamped_raw = Math::is_finite(p_raw_d32) ? CLAMP(p_raw_d32, 0.0f, 1.0f) : (p_reverse_z ? 0.0f : 1.0f);
	if (p_reverse_z) {
		// Reverse-Z perspective or ortho: raw == 1.0 -> z_near, raw == 0.0 -> EmptyDepth (z_far).
		// Standard Godot RD Reverse-Z linear eye depth mapping for ortho/perspective:
		// For orthographic heightfields, height fraction is (1.0 - raw) * (z_far - z_near) + z_near.
		return p_z_near + (1.0f - clamped_raw) * (p_z_far - p_z_near);
	}
	return p_z_near + clamped_raw * (p_z_far - p_z_near);
}

float sample_trilinear_sdf(const SceneSdfMeta256 &p_meta, const LocalVector<float> &p_voxels, const Vector3 &p_uvw) {
	const float safe_fallback = (Math::is_finite(p_meta.narrow_band_world) && p_meta.narrow_band_world > 0.0f) ? p_meta.narrow_band_world : 1e6f;
	if (!vec3_is_finite(p_uvw) || p_meta.dim_x < 2 || p_meta.dim_y < 2 || p_meta.dim_z < 2 ||
			p_meta.dim_x > 4096 || p_meta.dim_y > 4096 || p_meta.dim_z > 4096) {
		return safe_fallback;
	}
	const int nx = int(p_meta.dim_x);
	const int ny = int(p_meta.dim_y);
	const int nz = int(p_meta.dim_z);
	const float gx = CLAMP(p_uvw.x, 0.0f, 1.0f) * float(nx - 1);
	const float gy = CLAMP(p_uvw.y, 0.0f, 1.0f) * float(ny - 1);
	const float gz = CLAMP(p_uvw.z, 0.0f, 1.0f) * float(nz - 1);

	const int x0 = CLAMP(int(std::floor(gx)), 0, nx - 1);
	const int y0 = CLAMP(int(std::floor(gy)), 0, ny - 1);
	const int z0 = CLAMP(int(std::floor(gz)), 0, nz - 1);
	const int x1 = MIN(x0 + 1, nx - 1);
	const int y1 = MIN(y0 + 1, ny - 1);
	const int z1 = MIN(z0 + 1, nz - 1);

	const float tx = gx - float(x0);
	const float ty = gy - float(y0);
	const float tz = gz - float(z0);

	auto fetch = [&](int x, int y, int z) -> float {
		const uint64_t idx = (uint64_t(z) * uint64_t(ny) + uint64_t(y)) * uint64_t(nx) + uint64_t(x);
		if (idx >= p_voxels.size()) {
			return safe_fallback;
		}
		const float v = p_voxels[uint32_t(idx)];
		return Math::is_finite(v) ? v : safe_fallback;
	};

	const float c000 = fetch(x0, y0, z0);
	const float c100 = fetch(x1, y0, z0);
	const float c010 = fetch(x0, y1, z0);
	const float c110 = fetch(x1, y1, z0);
	const float c001 = fetch(x0, y0, z1);
	const float c101 = fetch(x1, y0, z1);
	const float c011 = fetch(x0, y1, z1);
	const float c111 = fetch(x1, y1, z1);

	const float c00 = Math::lerp(c000, c100, tx);
	const float c10 = Math::lerp(c010, c110, tx);
	const float c01 = Math::lerp(c001, c101, tx);
	const float c11 = Math::lerp(c011, c111, tx);
	const float c0 = Math::lerp(c00, c10, ty);
	const float c1 = Math::lerp(c01, c11, ty);
	return Math::lerp(c0, c1, tz);
}

} // namespace

ParticlesCollisionSceneRD::SphereSnapshot::~SphereSnapshot() {
	release_gpu_resources();
}

void ParticlesCollisionSceneRD::SphereSnapshot::release_gpu_resources() {
	if (!records_buffer.is_valid() && !meta_buffer.is_valid()) {
		return;
	}
	RenderingDevice *live_rd = device_guard.is_valid() ? device_guard->get_device() : rd;
	if (!live_rd) {
		records_buffer = RID();
		meta_buffer = RID();
		return;
	}
	if (device_guard.is_valid() && device_guard->owner_thread_id != 0) {
		ERR_FAIL_COND_MSG(Thread::get_caller_id() != device_guard->owner_thread_id,
				"ParticlesCollisionSceneRD::SphereSnapshot::release_gpu_resources must run on the owning render thread.");
	}
	if (records_buffer.is_valid()) {
		live_rd->free_rid(records_buffer);
		records_buffer = RID();
		if (live_rids) {
			live_rids->decrement();
		}
	}
	if (meta_buffer.is_valid()) {
		live_rd->free_rid(meta_buffer);
		meta_buffer = RID();
		if (live_rids) {
			live_rids->decrement();
		}
	}
}

ParticlesCollisionSceneRD::SphereReadLease::SphereReadLease(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample,
		const Ref<SphereSnapshot> &p_snapshot,
		SafeNumeric<uint32_t> *p_active_leases) :
		expected_device(p_device),
		expected_sample(p_sample),
		snapshot(p_snapshot),
		active_leases(p_active_leases) {
	if (active_leases) {
		active_leases->increment();
	}
}

ParticlesCollisionSceneRD::SphereReadLease::~SphereReadLease() {
	if (!returned) {
		return_after_reads();
	}
}

Error ParticlesCollisionSceneRD::SphereReadLease::validate(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample) const {
	if (returned || snapshot.is_null()) {
		return ERR_UNAVAILABLE;
	}
	if (GDExtensionDeviceExit::scene_opaque32_is_zero(p_device) || GDExtensionDeviceExit::scene_opaque32_is_zero(p_sample)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!GDExtensionDeviceExit::scene_opaque32_equal(p_device, expected_device) ||
			!GDExtensionDeviceExit::scene_opaque32_equal(p_sample, expected_sample)) {
		return ERR_UNAUTHORIZED;
	}
	return OK;
}

Error ParticlesCollisionSceneRD::SphereReadLease::return_after_reads() {
	if (!returned) {
		returned = true;
		snapshot.unref();
		if (active_leases) {
			active_leases->decrement();
		}
	}
	return OK;
}

ParticlesCollisionSceneRD::DepthSnapshotGeneration::~DepthSnapshotGeneration() {
	release_gpu_resources();
}

Error ParticlesCollisionSceneRD::DepthSnapshotGeneration::allocate_gpu_resources(
		uint32_t p_tex_width,
		uint32_t p_tex_height,
		bool p_simulate_fb_fail) {
	if (!rd || p_tex_width == 0u || p_tex_height == 0u || p_tex_width > 16384u || p_tex_height > 16384u) {
		return ERR_INVALID_PARAMETER;
	}
	release_gpu_resources();

	Vector<uint8_t> meta_bytes;
	meta_bytes.resize(sizeof(SceneDepthSnapshotMeta256));
	memcpy(meta_bytes.ptrw(), &meta, sizeof(SceneDepthSnapshotMeta256));
	meta_buffer = rd->storage_buffer_create(sizeof(SceneDepthSnapshotMeta256), meta_bytes);
	if (meta_buffer.is_null()) {
		return ERR_CANT_CREATE;
	}
	if (live_rids) {
		live_rids->increment();
	}

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	tf.width = p_tex_width;
	tf.height = p_tex_height;
	tf.depth = 1u;
	tf.array_layers = 1u;
	tf.mipmaps = 1u;
	tf.texture_type = RD::TEXTURE_TYPE_2D;
	tf.samples = RD::TEXTURE_SAMPLES_1;
	uint32_t usage = RD::TEXTURE_USAGE_SAMPLING_BIT |
			RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
			RD::TEXTURE_USAGE_STORAGE_BIT |
			RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
			RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	if (!rd->texture_is_format_supported_for_usage(RD::DATA_FORMAT_R32_SFLOAT, usage)) {
		usage &= ~uint32_t(RD::TEXTURE_USAGE_STORAGE_BIT);
	}
	tf.usage_bits = usage;

	Vector<Vector<uint8_t>> init_data;
	if (p_tex_width == 1u && p_tex_height == 1u) {
		Vector<uint8_t> zero_texel;
		zero_texel.resize(sizeof(float));
		memset(zero_texel.ptrw(), 0, sizeof(float));
		init_data.push_back(zero_texel);
	}
	depth_texture = rd->texture_create(tf, RD::TextureView(), init_data);
	if (depth_texture.is_null()) {
		release_gpu_resources();
		return ERR_CANT_CREATE;
	}
	if (live_rids) {
		live_rids->increment();
	}

	if (!p_simulate_fb_fail) {
		Vector<RID> fb_attachments;
		fb_attachments.push_back(depth_texture);
		framebuffer_rid = rd->framebuffer_create(fb_attachments);
	}
	if (framebuffer_rid.is_null()) {
		release_gpu_resources();
		return ERR_CANT_CREATE;
	}
	if (live_rids) {
		live_rids->increment();
	}

	texture_width = p_tex_width;
	texture_height = p_tex_height;
	return OK;
}

Error ParticlesCollisionSceneRD::DepthSnapshotGeneration::upload_metadata() {
	if (!rd || meta_buffer.is_null()) {
		return ERR_UNCONFIGURED;
	}
	return rd->buffer_update(meta_buffer, 0, sizeof(SceneDepthSnapshotMeta256), &meta);
}

static bool is_on_device_owner_thread(const RenderingDevice *p_rd, const ParticlesCollisionSceneRD::LiveDeviceGuard *p_guard) {
	if (p_rd && RenderingServer::get_singleton() != nullptr &&
			RenderingServer::get_singleton()->get_rendering_device() == p_rd) {
		return RenderingServer::get_singleton()->is_on_render_thread();
	}
	if (p_guard && p_guard->owner_thread_id != 0) {
		return Thread::get_caller_id() == p_guard->owner_thread_id;
	}
	return true;
}

void ParticlesCollisionSceneRD::DepthSnapshotGeneration::release_gpu_resources() {
	if (!framebuffer_rid.is_valid() && !depth_texture.is_valid() && !meta_buffer.is_valid()) {
		return;
	}
	RenderingDevice *live_rd = device_guard.is_valid() ? device_guard->get_device() : rd;
	if (!live_rd) {
		framebuffer_rid = RID();
		depth_texture = RID();
		meta_buffer = RID();
		return;
	}
	ERR_FAIL_COND_MSG(!is_on_device_owner_thread(live_rd, device_guard.ptr()),
			"ParticlesCollisionSceneRD::DepthSnapshotGeneration::release_gpu_resources must run on the owning render thread.");
	// Strict RD disposal order: free framebuffer_rid BEFORE depth_texture and meta_buffer.
	if (framebuffer_rid.is_valid()) {
		const bool tex_still_live = depth_texture.is_valid() && live_rd->texture_is_valid(depth_texture);
		live_rd->free_rid(framebuffer_rid);
		framebuffer_rid = RID();
		if (live_rids) {
			live_rids->decrement();
		}
		if (tex_still_live && fb_first_frees) {
			fb_first_frees->increment();
		}
	}
	if (depth_texture.is_valid()) {
		live_rd->free_rid(depth_texture);
		depth_texture = RID();
		if (live_rids) {
			live_rids->decrement();
		}
	}
	if (meta_buffer.is_valid()) {
		live_rd->free_rid(meta_buffer);
		meta_buffer = RID();
		if (live_rids) {
			live_rids->decrement();
		}
	}
}

ParticlesCollisionSceneRD::DepthReadLease::DepthReadLease(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample,
		const Ref<DepthSnapshotGeneration> &p_generation,
		SafeNumeric<uint32_t> *p_active_leases,
		const Ref<LiveDeviceGuard> &p_device_guard) :
		expected_device(p_device),
		expected_sample(p_sample),
		generation(p_generation),
		active_leases(p_active_leases),
		device_guard(p_device_guard) {
	if (generation.is_valid()) {
		generation->active_leases.increment();
	}
	if (active_leases) {
		active_leases->increment();
	}
}

ParticlesCollisionSceneRD::DepthReadLease::~DepthReadLease() {
	if (!returned) {
		return_after_reads();
	}
}

Error ParticlesCollisionSceneRD::DepthReadLease::validate(
		const GDExtensionSceneOpaque32V1 &p_device,
		const GDExtensionSceneOpaque32V1 &p_sample) const {
	if (returned || generation.is_null()) {
		return ERR_UNAVAILABLE;
	}
	if (GDExtensionDeviceExit::scene_opaque32_is_zero(p_device) || GDExtensionDeviceExit::scene_opaque32_is_zero(p_sample)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!GDExtensionDeviceExit::scene_opaque32_equal(p_device, expected_device) ||
			!GDExtensionDeviceExit::scene_opaque32_equal(p_sample, expected_sample)) {
		return ERR_UNAUTHORIZED;
	}
	if (generation->meta_buffer.is_null() || generation->depth_texture.is_null()) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Error ParticlesCollisionSceneRD::DepthReadLease::return_after_reads() {
	if (!returned) {
		returned = true;
		if (generation.is_valid()) {
			generation->active_leases.decrement();
			generation.unref();
		}
		if (active_leases) {
			active_leases->decrement();
		}
		if (device_guard.is_valid()) {
			RenderingDevice *live_rd = device_guard->get_device();
			if (live_rd && is_on_device_owner_thread(live_rd, device_guard.ptr())) {
				if (ParticlesCollisionSceneRD *owner = device_guard->get_owner()) {
					owner->reap_retired_depth_snapshots();
				}
			}
		}
	}
	return OK;
}

ParticlesCollisionSceneRD *ParticlesCollisionSceneRD::get_for_device(const RenderingDevice *p_device) {
	if (!p_device) {
		return nullptr;
	}
	if (ParticlesStorage *ps = ParticlesStorage::get_singleton()) {
		ParticlesCollisionSceneRD *cs = ps->get_collision_scene_rd();
		if (cs && cs->get_device() == p_device) {
			return cs;
		}
	}
	MutexLock lock(g_live_collision_scenes_mutex);
	for (uint32_t i = 0; i < g_live_collision_scenes.size(); ++i) {
		ParticlesCollisionSceneRD *candidate = g_live_collision_scenes[i];
		if (candidate && candidate->get_device() == p_device) {
			return candidate;
		}
	}
	return nullptr;
}

ParticlesCollisionSceneRD::~ParticlesCollisionSceneRD() {
	shutdown();
}

void ParticlesCollisionSceneRD::initialize(RenderingDevice *p_device, const GDExtensionSceneOpaque32V1 &p_device_capability) {
	shutdown();
	{
		MutexLock lock(snapshot_mutex);
		device = p_device;
		live_device_guard.instantiate();
		live_device_guard->set_device(p_device);
		live_device_guard->set_owner(p_device ? this : nullptr);
		live_device_guard->owner_thread_id = p_device ? Thread::get_caller_id() : 0;
		device_capability = p_device_capability;
		next_snapshot_generation = 1;
		scene_epoch = 1;
	}
	if (p_device) {
		MutexLock reg_lock(g_live_collision_scenes_mutex);
		if (!g_live_collision_scenes.has(this)) {
			g_live_collision_scenes.push_back(this);
		}
	}
}

void ParticlesCollisionSceneRD::shutdown() {
	{
		MutexLock reg_lock(g_live_collision_scenes_mutex);
		g_live_collision_scenes.erase(this);
	}

	Ref<SphereSnapshot> snap_to_drop;
	LocalVector<Ref<DepthSnapshotGeneration>> depth_to_drop;
	Ref<LiveDeviceGuard> guard_to_retire;
	RenderingDevice *rd_to_clean = nullptr;
	StandaloneNearestCopyPipeline copy_to_clean;
	{
		MutexLock lock(snapshot_mutex);
		snap_to_drop = current_sphere_snapshot;
		current_sphere_snapshot.unref();
		if (shared_null_viewport_sentinel.is_valid()) {
			depth_to_drop.push_back(shared_null_viewport_sentinel);
			shared_null_viewport_sentinel.unref();
		}
		for (KeyValue<uint64_t, ViewportDepthSlot> &kv : depth_snapshots_by_viewport) {
			if (kv.value.current_snapshot.is_valid()) {
				depth_to_drop.push_back(kv.value.current_snapshot);
			}
			if (kv.value.unpopulated_sentinel.is_valid()) {
				depth_to_drop.push_back(kv.value.unpopulated_sentinel);
			}
		}
		depth_snapshots_by_viewport.clear();
		for (uint32_t i = 0; i < retired_depth_snapshots.size(); ++i) {
			if (retired_depth_snapshots[i].is_valid()) {
				depth_to_drop.push_back(retired_depth_snapshots[i]);
			}
		}
		retired_depth_snapshots.clear();
		copy_to_clean = standalone_copy;
		standalone_copy = StandaloneNearestCopyPipeline();
		rd_to_clean = device;
		guard_to_retire = live_device_guard;
		live_device_guard.unref();
		device = nullptr;
		device_capability = {};
	}
	const bool on_owner_thread = is_on_device_owner_thread(rd_to_clean, guard_to_retire.ptr());
	if (guard_to_retire.is_valid()) {
		guard_to_retire->set_owner(nullptr);
	}
	if (on_owner_thread) {
		if (snap_to_drop.is_valid()) {
			snap_to_drop->release_gpu_resources();
		}
		for (uint32_t i = 0; i < depth_to_drop.size(); ++i) {
			depth_to_drop[i]->release_gpu_resources();
		}
		if (rd_to_clean) {
			if (copy_to_clean.pipeline.is_valid()) {
				rd_to_clean->free_rid(copy_to_clean.pipeline);
			}
			if (copy_to_clean.shader.is_valid()) {
				rd_to_clean->free_rid(copy_to_clean.shader);
			}
			if (copy_to_clean.sampler.is_valid()) {
				rd_to_clean->free_rid(copy_to_clean.sampler);
			}
		}
	}
	snap_to_drop.unref();
	depth_to_drop.clear();
	if (guard_to_retire.is_valid()) {
		guard_to_retire->set_device(nullptr);
	}
}

bool ParticlesCollisionSceneRD::is_initialized() const {
	MutexLock lock(snapshot_mutex);
	return device != nullptr && !GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability);
}

RenderingDevice *ParticlesCollisionSceneRD::get_device() const {
	MutexLock lock(snapshot_mutex);
	return device;
}

void ParticlesCollisionSceneRD::reap_retired_depth_snapshots_locked(LocalVector<Ref<DepthSnapshotGeneration>> &r_to_release) {
	uint32_t write_idx = 0;
	for (uint32_t i = 0; i < retired_depth_snapshots.size(); ++i) {
		Ref<DepthSnapshotGeneration> &gen = retired_depth_snapshots[i];
		if (gen.is_null()) {
			continue;
		}
		if (gen->active_leases.get() == 0u) {
			r_to_release.push_back(gen);
		} else {
			if (write_idx != i) {
				retired_depth_snapshots[write_idx] = gen;
			}
			++write_idx;
		}
	}
	retired_depth_snapshots.resize(write_idx);
}

void ParticlesCollisionSceneRD::reap_retired_depth_snapshots() {
	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	bool can_release_gpu = false;
	{
		MutexLock lock(snapshot_mutex);
		can_release_gpu = device != nullptr && is_on_device_owner_thread(device, live_device_guard.ptr());
		if (can_release_gpu) {
			reap_retired_depth_snapshots_locked(to_release);
		}
	}
	if (can_release_gpu) {
		for (uint32_t i = 0; i < to_release.size(); ++i) {
			to_release[i]->release_gpu_resources();
		}
	}
	to_release.clear();
}

Error ParticlesCollisionSceneRD::request_viewport_depth_snapshot(
		RID p_viewport,
		float p_slab_thickness_m,
		float p_silhouette_jump_m) {
	if (p_viewport.is_null() || !Math::is_finite(p_slab_thickness_m) || !Math::is_finite(p_silhouette_jump_m) ||
			p_slab_thickness_m < 0.0f || p_silhouette_jump_m < 0.0f) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(snapshot_mutex);
	if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
		return ERR_UNAVAILABLE;
	}
	ViewportDepthSlot &slot = depth_snapshots_by_viewport[p_viewport.get_id()];
	slot.viewport = p_viewport;
	slot.default_slab_thickness_m = (p_slab_thickness_m > 0.0f) ? p_slab_thickness_m : 0.50f;
	slot.default_silhouette_jump_m = (p_silhouette_jump_m > 0.0f) ? p_silhouette_jump_m : 0.50f;
	slot.pending_capture = true;
	if (slot.render_target.is_null() && is_on_device_owner_thread(device, live_device_guard.ptr()) &&
			RSG::viewport != nullptr && RSG::viewport->viewport_owner.owns(p_viewport)) {
		slot.render_target = RSG::viewport->viewport_get_render_target(p_viewport);
	}
	return OK;
}

bool ParticlesCollisionSceneRD::has_pending_depth_snapshot_request(RID p_viewport, uint64_t p_frame_number) const {
	(void)p_frame_number;
	if (p_viewport.is_null()) {
		return false;
	}
	MutexLock lock(snapshot_mutex);
	if (!device) {
		return false;
	}
	const ViewportDepthSlot *slot = depth_snapshots_by_viewport.getptr(p_viewport.get_id());
	return slot != nullptr && slot->pending_capture;
}

RID ParticlesCollisionSceneRD::find_viewport_by_render_target(RID p_render_target) {
	if (p_render_target.is_null()) {
		return RID();
	}
	MutexLock lock(snapshot_mutex);
	if (!device) {
		return RID();
	}
	const bool can_query_vp = is_on_device_owner_thread(device, live_device_guard.ptr()) && RSG::viewport != nullptr;
	for (KeyValue<uint64_t, ViewportDepthSlot> &kv : depth_snapshots_by_viewport) {
		if (kv.value.render_target.is_null() && can_query_vp && kv.value.viewport.is_valid() &&
				RSG::viewport->viewport_owner.owns(kv.value.viewport)) {
			kv.value.render_target = RSG::viewport->viewport_get_render_target(kv.value.viewport);
		}
		if (kv.value.render_target == p_render_target && kv.value.viewport.is_valid()) {
			return kv.value.viewport;
		}
	}
	return RID();
}

bool ParticlesCollisionSceneRD::has_viewport_depth_snapshot(RID p_viewport) const {
	if (p_viewport.is_null()) {
		return false;
	}
	MutexLock lock(snapshot_mutex);
	return depth_snapshots_by_viewport.has(p_viewport.get_id());
}

uint32_t ParticlesCollisionSceneRD::get_retired_depth_snapshot_count() const {
	MutexLock lock(snapshot_mutex);
	return retired_depth_snapshots.size();
}

Error ParticlesCollisionSceneRD::acquire_depth_snapshot_read_lease(
		RID p_viewport,
		Ref<DepthReadLease> &r_lease,
		GDExtensionSceneOpaque32V1 &r_sample_token,
		const GDExtensionSceneOpaque32V1 *p_expected_device_override) {
	r_lease.unref();
	r_sample_token = {};
	if (p_viewport.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	Error result_err = OK;
	{
		MutexLock lock(snapshot_mutex);
		if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNAVAILABLE;
		}
		if (!is_on_device_owner_thread(device, live_device_guard.ptr())) {
			return ERR_UNAUTHORIZED;
		}
		reap_retired_depth_snapshots_locked(to_release);

		ViewportDepthSlot &slot = depth_snapshots_by_viewport[p_viewport.get_id()];
		slot.viewport = p_viewport;
		slot.pending_capture = true;
		if (slot.render_target.is_null() && RSG::viewport != nullptr && RSG::viewport->viewport_owner.owns(p_viewport)) {
			slot.render_target = RSG::viewport->viewport_get_render_target(p_viewport);
		}

		Ref<DepthSnapshotGeneration> chosen_gen = slot.current_snapshot;
		if (chosen_gen.is_null()) {
			if (slot.unpopulated_sentinel.is_null()) {
				Ref<DepthSnapshotGeneration> sentinel;
				sentinel.instantiate();
				sentinel->rd = device;
				sentinel->device_guard = live_device_guard;
				sentinel->live_rids = &live_rid_counter;
				sentinel->fb_first_frees = &fb_first_free_counter;
				sentinel->meta = SceneDepthSnapshotMeta256();
				sentinel->meta.magic = SceneDepthSnapshotMeta256::MAGIC;
				sentinel->meta.version = SceneDepthSnapshotMeta256::VERSION;
				sentinel->meta.width = 0u;
				sentinel->meta.height = 0u;
				sentinel->meta.snapshot_generation = 0ull;
				sentinel->meta.view_id = p_viewport.get_id();
				sentinel->meta.slab_thickness_m = slot.default_slab_thickness_m;
				sentinel->meta.silhouette_jump_m = slot.default_silhouette_jump_m;
				sentinel->meta.flags = SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;

				const bool sim_fail = (simulate_fb_fail_once.get() != 0u);
				if (sim_fail) {
					simulate_fb_fail_once.set(0u);
				}
				result_err = sentinel->allocate_gpu_resources(1u, 1u, sim_fail);
				if (result_err == OK) {
					slot.unpopulated_sentinel = sentinel;
				}
			}
			chosen_gen = slot.unpopulated_sentinel;
		}

		if (result_err == OK && chosen_gen.is_valid()) {
			const GDExtensionSceneOpaque32V1 expected_dev =
					(p_expected_device_override && !GDExtensionDeviceExit::scene_opaque32_is_zero(*p_expected_device_override))
					? *p_expected_device_override
					: device_capability;
			r_sample_token = GDExtensionDeviceExit::make_scene_depth_snapshot_sample_token(
					p_viewport.get_id(),
					chosen_gen->meta.snapshot_generation,
					chosen_gen->meta.width,
					chosen_gen->meta.height);
			r_lease.instantiate(expected_dev, r_sample_token, chosen_gen, &active_lease_counter, live_device_guard);
		} else if (result_err == OK) {
			result_err = ERR_CANT_CREATE;
		}
	}

	for (uint32_t i = 0; i < to_release.size(); ++i) {
		to_release[i]->release_gpu_resources();
	}
	return result_err;
}

Error ParticlesCollisionSceneRD::acquire_depth_snapshot_read_lease(
		RID p_viewport,
		float p_slab_thickness_m,
		float p_silhouette_jump_m,
		Ref<DepthReadLease> &r_lease,
		GDExtensionSceneOpaque32V1 &r_sample_token,
		const GDExtensionSceneOpaque32V1 *p_expected_device_override) {
	r_lease.unref();
	r_sample_token = {};
	if (!Math::is_finite(p_slab_thickness_m) || !Math::is_finite(p_silhouette_jump_m) ||
			p_slab_thickness_m < 0.01f || p_slab_thickness_m > 10.0f ||
			p_silhouette_jump_m < 0.01f || p_silhouette_jump_m > 10.0f) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	Error result_err = OK;
	{
		MutexLock lock(snapshot_mutex);
		if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNAVAILABLE;
		}
		if (!is_on_device_owner_thread(device, live_device_guard.ptr())) {
			return ERR_UNAUTHORIZED;
		}
		reap_retired_depth_snapshots_locked(to_release);

		Ref<DepthSnapshotGeneration> chosen_gen;
		uint64_t sample_viewport_id = 0ull;
		if (p_viewport.is_null()) {
			if (shared_null_viewport_sentinel.is_null()) {
				Ref<DepthSnapshotGeneration> sentinel;
				sentinel.instantiate();
				sentinel->rd = device;
				sentinel->device_guard = live_device_guard;
				sentinel->live_rids = &live_rid_counter;
				sentinel->fb_first_frees = &fb_first_free_counter;
				sentinel->meta = SceneDepthSnapshotMeta256();
				sentinel->meta.magic = SceneDepthSnapshotMeta256::MAGIC;
				sentinel->meta.version = SceneDepthSnapshotMeta256::VERSION;
				sentinel->meta.width = 0u;
				sentinel->meta.height = 0u;
				sentinel->meta.snapshot_generation = 0ull;
				sentinel->meta.view_id = 0ull;
				sentinel->meta.slab_thickness_m = p_slab_thickness_m;
				sentinel->meta.silhouette_jump_m = p_silhouette_jump_m;
				sentinel->meta.flags = SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;

				const bool sim_fail = (simulate_fb_fail_once.get() != 0u);
				if (sim_fail) {
					simulate_fb_fail_once.set(0u);
				}
				result_err = sentinel->allocate_gpu_resources(1u, 1u, sim_fail);
				if (result_err == OK) {
					shared_null_viewport_sentinel = sentinel;
				}
			}
			chosen_gen = shared_null_viewport_sentinel;
		} else {
			sample_viewport_id = p_viewport.get_id();
			ViewportDepthSlot &slot = depth_snapshots_by_viewport[sample_viewport_id];
			slot.viewport = p_viewport;
			slot.default_slab_thickness_m = p_slab_thickness_m;
			slot.default_silhouette_jump_m = p_silhouette_jump_m;
			slot.pending_capture = true;
			if (slot.render_target.is_null() && RSG::viewport != nullptr && RSG::viewport->viewport_owner.owns(p_viewport)) {
				slot.render_target = RSG::viewport->viewport_get_render_target(p_viewport);
			}

			chosen_gen = slot.current_snapshot;
			if (chosen_gen.is_null()) {
				if (slot.unpopulated_sentinel.is_null()) {
					Ref<DepthSnapshotGeneration> sentinel;
					sentinel.instantiate();
					sentinel->rd = device;
					sentinel->device_guard = live_device_guard;
					sentinel->live_rids = &live_rid_counter;
					sentinel->fb_first_frees = &fb_first_free_counter;
					sentinel->meta = SceneDepthSnapshotMeta256();
					sentinel->meta.magic = SceneDepthSnapshotMeta256::MAGIC;
					sentinel->meta.version = SceneDepthSnapshotMeta256::VERSION;
					sentinel->meta.width = 0u;
					sentinel->meta.height = 0u;
					sentinel->meta.snapshot_generation = 0ull;
					sentinel->meta.view_id = sample_viewport_id;
					sentinel->meta.slab_thickness_m = slot.default_slab_thickness_m;
					sentinel->meta.silhouette_jump_m = slot.default_silhouette_jump_m;
					sentinel->meta.flags = SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;

					const bool sim_fail = (simulate_fb_fail_once.get() != 0u);
					if (sim_fail) {
						simulate_fb_fail_once.set(0u);
					}
					result_err = sentinel->allocate_gpu_resources(1u, 1u, sim_fail);
					if (result_err == OK) {
						slot.unpopulated_sentinel = sentinel;
					}
				}
				chosen_gen = slot.unpopulated_sentinel;
			}
		}

		if (result_err == OK && chosen_gen.is_valid()) {
			const GDExtensionSceneOpaque32V1 expected_dev =
					(p_expected_device_override && !GDExtensionDeviceExit::scene_opaque32_is_zero(*p_expected_device_override))
					? *p_expected_device_override
					: device_capability;
			r_sample_token = GDExtensionDeviceExit::make_scene_depth_snapshot_sample_token(
					sample_viewport_id,
					chosen_gen->meta.snapshot_generation,
					chosen_gen->meta.width,
					chosen_gen->meta.height);
			r_lease.instantiate(expected_dev, r_sample_token, chosen_gen, &active_lease_counter, live_device_guard);
		} else if (result_err == OK) {
			result_err = ERR_CANT_CREATE;
		}
	}

	for (uint32_t i = 0; i < to_release.size(); ++i) {
		to_release[i]->release_gpu_resources();
	}
	return result_err;
}

Error ParticlesCollisionSceneRD::release_depth_snapshot_read_lease(Ref<DepthReadLease> &r_lease) {
	if (r_lease.is_null()) {
		return OK;
	}
	const Error err = r_lease->return_after_reads();
	r_lease.unref();
	return err;
}

Error ParticlesCollisionSceneRD::prepare_viewport_depth_snapshot_target(
		RID p_viewport,
		uint32_t p_width,
		uint32_t p_height,
		RID p_render_target,
		Ref<DepthSnapshotGeneration> &r_target_gen) {
	r_target_gen.unref();
	if (p_viewport.is_null() || p_width < 2u || p_height < 2u || p_width > 16384u || p_height > 16384u) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	Error result_err = OK;
	{
		MutexLock lock(snapshot_mutex);
		if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNAVAILABLE;
		}
		if (!is_on_device_owner_thread(device, live_device_guard.ptr())) {
			return ERR_UNAUTHORIZED;
		}
		reap_retired_depth_snapshots_locked(to_release);

		ViewportDepthSlot &slot = depth_snapshots_by_viewport[p_viewport.get_id()];
		slot.viewport = p_viewport;
		if (p_render_target.is_valid()) {
			slot.render_target = p_render_target;
		}

		const bool sim_fail = (simulate_fb_fail_once.get() != 0u);
		if (!sim_fail && slot.current_snapshot.is_valid() &&
				slot.current_snapshot->active_leases.get() == 0u &&
				slot.current_snapshot->texture_width == p_width &&
				slot.current_snapshot->texture_height == p_height &&
				slot.current_snapshot->framebuffer_rid.is_valid()) {
			r_target_gen = slot.current_snapshot;
		} else {
			if (sim_fail) {
				simulate_fb_fail_once.set(0u);
			}
			Ref<DepthSnapshotGeneration> next_gen;
			next_gen.instantiate();
			next_gen->rd = device;
			next_gen->device_guard = live_device_guard;
			next_gen->live_rids = &live_rid_counter;
			next_gen->fb_first_frees = &fb_first_free_counter;
			next_gen->meta = SceneDepthSnapshotMeta256();
			next_gen->meta.view_id = p_viewport.get_id();
			next_gen->meta.slab_thickness_m = slot.default_slab_thickness_m;
			next_gen->meta.silhouette_jump_m = slot.default_silhouette_jump_m;
			result_err = next_gen->allocate_gpu_resources(p_width, p_height, sim_fail);
			if (result_err == OK) {
				r_target_gen = next_gen;
			}
		}
	}

	for (uint32_t i = 0; i < to_release.size(); ++i) {
		to_release[i]->release_gpu_resources();
	}
	return result_err;
}

Error ParticlesCollisionSceneRD::finalize_viewport_depth_snapshot_target(
		RID p_viewport,
		const Ref<DepthSnapshotGeneration> &p_target_gen,
		const Transform3D &p_cam_transform,
		const Projection &p_cam_projection,
		float p_z_near,
		float p_z_far,
		uint64_t p_frame_number,
		RID p_render_target) {
	if (p_viewport.is_null() || p_target_gen.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	Error result_err = OK;
	{
		MutexLock lock(snapshot_mutex);
		if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNAVAILABLE;
		}
		if (!is_on_device_owner_thread(device, live_device_guard.ptr())) {
			return ERR_UNAUTHORIZED;
		}
		reap_retired_depth_snapshots_locked(to_release);

		ViewportDepthSlot &slot = depth_snapshots_by_viewport[p_viewport.get_id()];
		slot.viewport = p_viewport;
		if (p_render_target.is_valid()) {
			slot.render_target = p_render_target;
		}

		SceneDepthSnapshotMeta256 meta;
		meta.magic = SceneDepthSnapshotMeta256::MAGIC;
		meta.version = SceneDepthSnapshotMeta256::VERSION;
		meta.width = p_target_gen->texture_width;
		meta.height = p_target_gen->texture_height;
		meta.snapshot_generation = next_snapshot_generation++;
		meta.view_id = p_viewport.get_id();
		meta.slab_thickness_m = slot.default_slab_thickness_m;
		meta.silhouette_jump_m = slot.default_silhouette_jump_m;
		fill_depth_snapshot_matrices(p_cam_transform, p_cam_projection, p_z_near, p_z_far, meta);
		if ((meta.flags & SceneDepthSnapshotMeta256::FLAG_PRESENT) == 0u) {
			meta.width = 0u;
			meta.height = 0u;
		}

		p_target_gen->meta = meta;
		result_err = p_target_gen->upload_metadata();
		if (result_err == OK) {
			if (slot.current_snapshot.is_valid() && slot.current_snapshot != p_target_gen) {
				if (slot.current_snapshot->active_leases.get() > 0u) {
					retired_depth_snapshots.push_back(slot.current_snapshot);
				} else {
					to_release.push_back(slot.current_snapshot);
				}
			}
			if (slot.unpopulated_sentinel.is_valid()) {
				if (slot.unpopulated_sentinel->active_leases.get() > 0u) {
					retired_depth_snapshots.push_back(slot.unpopulated_sentinel);
				} else {
					to_release.push_back(slot.unpopulated_sentinel);
				}
				slot.unpopulated_sentinel.unref();
			}
			slot.current_snapshot = p_target_gen;
			slot.last_captured_frame = p_frame_number;
			slot.has_captured_frame = true;
			slot.pending_capture = false;
		}
	}

	for (uint32_t i = 0; i < to_release.size(); ++i) {
		to_release[i]->release_gpu_resources();
	}
	return result_err;
}

Error ParticlesCollisionSceneRD::ensure_standalone_nearest_copy_pipeline(RenderingDevice *p_rd, int64_t p_fb_format) {
	if (!p_rd) {
		return ERR_UNAVAILABLE;
	}
	if (standalone_copy.pipeline.is_valid() && standalone_copy.shader.is_valid() && standalone_copy.sampler.is_valid()) {
		return OK;
	}
	if (standalone_copy.sampler.is_null()) {
		RD::SamplerState samp;
		samp.mag_filter = RD::SAMPLER_FILTER_NEAREST;
		samp.min_filter = RD::SAMPLER_FILTER_NEAREST;
		samp.mip_filter = RD::SAMPLER_FILTER_NEAREST;
		samp.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		samp.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		samp.repeat_w = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		standalone_copy.sampler = p_rd->sampler_create(samp);
		if (standalone_copy.sampler.is_null()) {
			return ERR_CANT_CREATE;
		}
	}
	if (standalone_copy.shader.is_null()) {
		static constexpr const char *kVertGlsl =
				"#version 450\n"
				"layout(location = 0) out vec2 v_uv;\n"
				"void main() {\n"
				"	vec2 pos = (gl_VertexIndex == 0) ? vec2(-1.0, -1.0) : ((gl_VertexIndex == 1) ? vec2(3.0, -1.0) : vec2(-1.0, 3.0));\n"
				"	v_uv = pos * 0.5 + 0.5;\n"
				"	gl_Position = vec4(pos, 0.0, 1.0);\n"
				"}\n";
		static constexpr const char *kFragGlsl =
				"#version 450\n"
				"layout(location = 0) in vec2 v_uv;\n"
				"layout(set = 0, binding = 0) uniform sampler2D u_src_depth;\n"
				"layout(location = 0) out float o_depth;\n"
				"void main() {\n"
				"	o_depth = textureLod(u_src_depth, v_uv, 0.0).r;\n"
				"}\n";
		String err_msg;
		RD::ShaderStageSPIRVData vs;
		vs.shader_stage = RD::SHADER_STAGE_VERTEX;
		vs.spirv = p_rd->shader_compile_spirv_from_source(RD::SHADER_STAGE_VERTEX, String::utf8(kVertGlsl), RD::SHADER_LANGUAGE_GLSL, &err_msg, false);
		if (vs.spirv.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		RD::ShaderStageSPIRVData fs;
		fs.shader_stage = RD::SHADER_STAGE_FRAGMENT;
		fs.spirv = p_rd->shader_compile_spirv_from_source(RD::SHADER_STAGE_FRAGMENT, String::utf8(kFragGlsl), RD::SHADER_LANGUAGE_GLSL, &err_msg, false);
		if (fs.spirv.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		Vector<RD::ShaderStageSPIRVData> stages;
		stages.push_back(vs);
		stages.push_back(fs);
		standalone_copy.shader = p_rd->shader_create_from_spirv(stages);
		if (standalone_copy.shader.is_null()) {
			return ERR_CANT_CREATE;
		}
	}
	RD::PipelineColorBlendState bs;
	bs.attachments.push_back(RD::PipelineColorBlendState::Attachment());
	standalone_copy.pipeline = p_rd->render_pipeline_create(
			standalone_copy.shader,
			p_fb_format,
			RD::INVALID_ID,
			RD::RENDER_PRIMITIVE_TRIANGLES,
			RD::PipelineRasterizationState(),
			RD::PipelineMultisampleState(),
			RD::PipelineDepthStencilState(),
			bs);
	return standalone_copy.pipeline.is_valid() ? OK : ERR_CANT_CREATE;
}

Error ParticlesCollisionSceneRD::capture_viewport_depth_snapshot(
		RID p_viewport,
		RID p_src_depth_texture,
		uint32_t p_width,
		uint32_t p_height,
		const Transform3D &p_cam_transform,
		const Projection &p_cam_projection,
		float p_z_near,
		float p_z_far,
		uint64_t p_frame_number,
		RID p_render_target,
		uint32_t p_view_count,
		bool p_is_reflection_probe) {
	RenderingDevice *live_rd = nullptr;
	{
		MutexLock lock(snapshot_mutex);
		if (!device || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNAVAILABLE;
		}
		if (!is_on_device_owner_thread(device, live_device_guard.ptr())) {
			return ERR_UNAUTHORIZED;
		}
		live_rd = device;
	}
	if (p_is_reflection_probe || p_view_count != 1u) {
		return OK;
	}
	if (p_viewport.is_null() || p_src_depth_texture.is_null() || p_width < 2u || p_height < 2u) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<DepthSnapshotGeneration> target_gen;
	Error err = prepare_viewport_depth_snapshot_target(p_viewport, p_width, p_height, p_render_target, target_gen);
	if (err != OK) {
		return err;
	}

	const RD::TextureFormat src_fmt = live_rd->texture_get_format(p_src_depth_texture);
	const bool needs_depth_slice = (src_fmt.format == RD::DATA_FORMAT_D32_SFLOAT_S8_UINT ||
			src_fmt.format == RD::DATA_FORMAT_D24_UNORM_S8_UINT ||
			src_fmt.format == RD::DATA_FORMAT_D16_UNORM_S8_UINT ||
			src_fmt.texture_type != RD::TEXTURE_TYPE_2D ||
			src_fmt.array_layers > 1u);
	RID temp_slice;
	if (needs_depth_slice) {
		temp_slice = live_rd->texture_create_shared_from_slice(RD::TextureView(), p_src_depth_texture, 0u, 0u, 1u, RD::TEXTURE_SLICE_2D);
		if (temp_slice.is_null()) {
			return ERR_CANT_CREATE;
		}
	}
	const RID sample_tex = temp_slice.is_valid() ? temp_slice : p_src_depth_texture;

	{
		MutexLock lock(snapshot_mutex);
		err = ensure_standalone_nearest_copy_pipeline(live_rd, live_rd->framebuffer_get_format(target_gen->framebuffer_rid));
	}
	if (err != OK) {
		if (temp_slice.is_valid()) {
			live_rd->free_rid(temp_slice);
		}
		return err;
	}

	RD::Uniform u_src;
	u_src.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_src.binding = 0;
	u_src.append_id(standalone_copy.sampler);
	u_src.append_id(sample_tex);
	Vector<RD::Uniform> uniforms;
	uniforms.push_back(u_src);
	const RID uniform_set = live_rd->uniform_set_create(uniforms, standalone_copy.shader, 0u);
	if (uniform_set.is_null()) {
		if (temp_slice.is_valid()) {
			live_rd->free_rid(temp_slice);
		}
		return ERR_CANT_CREATE;
	}

	const RD::DrawListID dl = live_rd->draw_list_begin(target_gen->framebuffer_rid, RD::DRAW_DEFAULT_ALL, Vector<Color>(), 1.0f, 0u, Rect2i(0, 0, int(p_width), int(p_height)));
	live_rd->draw_list_bind_render_pipeline(dl, standalone_copy.pipeline);
	live_rd->draw_list_bind_uniform_set(dl, uniform_set, 0u);
	live_rd->draw_list_draw(dl, false, 1u, 3u);
	live_rd->draw_list_end();

	live_rd->free_rid(uniform_set);
	if (temp_slice.is_valid()) {
		live_rd->free_rid(temp_slice);
	}

	return finalize_viewport_depth_snapshot_target(
			p_viewport, target_gen, p_cam_transform, p_cam_projection, p_z_near, p_z_far, p_frame_number, p_render_target);
}

void ParticlesCollisionSceneRD::invalidate_viewport_depth_snapshot(RID p_viewport, bool p_preserve_armed_request) {
	if (p_viewport.is_null()) {
		return;
	}
	LocalVector<Ref<DepthSnapshotGeneration>> to_release;
	bool can_release_gpu = false;
	{
		MutexLock lock(snapshot_mutex);
		can_release_gpu = device != nullptr && is_on_device_owner_thread(device, live_device_guard.ptr());
		if (can_release_gpu) {
			reap_retired_depth_snapshots_locked(to_release);
		}
		ViewportDepthSlot *slot = depth_snapshots_by_viewport.getptr(p_viewport.get_id());
		if (slot) {
			if (slot->current_snapshot.is_valid()) {
				if (slot->current_snapshot->active_leases.get() > 0u || !can_release_gpu) {
					retired_depth_snapshots.push_back(slot->current_snapshot);
				} else {
					to_release.push_back(slot->current_snapshot);
				}
				slot->current_snapshot.unref();
			}
			const bool keep_armed_slot = p_preserve_armed_request && slot->pending_capture;
			if (!keep_armed_slot) {
				if (slot->unpopulated_sentinel.is_valid()) {
					if (slot->unpopulated_sentinel->active_leases.get() > 0u || !can_release_gpu) {
						retired_depth_snapshots.push_back(slot->unpopulated_sentinel);
					} else {
						to_release.push_back(slot->unpopulated_sentinel);
					}
					slot->unpopulated_sentinel.unref();
				}
				depth_snapshots_by_viewport.erase(p_viewport.get_id());
			}
		}
	}
	if (can_release_gpu) {
		for (uint32_t i = 0; i < to_release.size(); ++i) {
			to_release[i]->release_gpu_resources();
		}
	}
	to_release.clear();
}

void ParticlesCollisionSceneRD::invalidate_render_target_depth_snapshot(RID p_render_target, bool p_preserve_armed_request) {
	if (p_render_target.is_null()) {
		return;
	}
	LocalVector<RID> matching_viewports;
	{
		MutexLock lock(snapshot_mutex);
		const bool can_query_vp = device != nullptr && is_on_device_owner_thread(device, live_device_guard.ptr()) && RSG::viewport != nullptr;
		for (KeyValue<uint64_t, ViewportDepthSlot> &kv : depth_snapshots_by_viewport) {
			if (kv.value.render_target.is_null() && can_query_vp && kv.value.viewport.is_valid() &&
					RSG::viewport->viewport_owner.owns(kv.value.viewport)) {
				kv.value.render_target = RSG::viewport->viewport_get_render_target(kv.value.viewport);
			}
			if (kv.value.render_target == p_render_target && kv.value.viewport.is_valid()) {
				matching_viewports.push_back(kv.value.viewport);
			}
		}
	}
	for (uint32_t i = 0; i < matching_viewports.size(); ++i) {
		invalidate_viewport_depth_snapshot(matching_viewports[i], p_preserve_armed_request);
	}
}

uint64_t ParticlesCollisionSceneRD::get_current_snapshot_generation() const {
	MutexLock lock(snapshot_mutex);
	return current_sphere_snapshot.is_valid() ? current_sphere_snapshot->meta.snapshot_generation : 0;
}

Error ParticlesCollisionSceneRD::update_sphere_colliders(
		const LocalVector<SceneSphereRecord32> &p_colliders,
		const Vector3 &p_bounds_min,
		const Vector3 &p_bounds_max) {
	if (p_colliders.size() > 1024) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (!vec3_is_finite(p_bounds_min) || !vec3_is_finite(p_bounds_max) ||
			!(p_bounds_min.x <= p_bounds_max.x && p_bounds_min.y <= p_bounds_max.y && p_bounds_min.z <= p_bounds_max.z)) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t i = 0; i < p_colliders.size(); ++i) {
		const SceneSphereRecord32 &rec = p_colliders[i];
		if (!Math::is_finite(rec.center_x) || !Math::is_finite(rec.center_y) || !Math::is_finite(rec.center_z) ||
				!Math::is_finite(rec.radius) || rec.radius <= 0.0f ||
				!Math::is_finite(rec.restitution) || !Math::is_finite(rec.friction)) {
			return ERR_INVALID_PARAMETER;
		}
	}

	Ref<SphereSnapshot> prev_snap_drop;
	{
		MutexLock lock(snapshot_mutex);
		if (device && live_device_guard.is_valid() && live_device_guard->owner_thread_id != 0 && Thread::get_caller_id() != live_device_guard->owner_thread_id) {
			return ERR_UNAUTHORIZED;
		}
		// Copy-on-write snapshot: existing SphereReadLease instances retain their
		// immutable Ref<SphereSnapshot> and RD buffers until return_after_reads().
		Ref<SphereSnapshot> next_snap;
		next_snap.instantiate();
		next_snap->rd = device;
		next_snap->device_guard = live_device_guard;
		next_snap->live_rids = &live_rid_counter;
		next_snap->records = p_colliders;
		next_snap->meta.count = p_colliders.size();
		next_snap->meta.max_colliders = MAX(1u, p_colliders.size());
		next_snap->meta.snapshot_generation = next_snapshot_generation++;
		next_snap->meta.scene_epoch = scene_epoch;
		next_snap->meta.world_bounds_min[0] = p_bounds_min.x;
		next_snap->meta.world_bounds_min[1] = p_bounds_min.y;
		next_snap->meta.world_bounds_min[2] = p_bounds_min.z;
		next_snap->meta.world_bounds_min[3] = 0.0f;
		next_snap->meta.world_bounds_max[0] = p_bounds_max.x;
		next_snap->meta.world_bounds_max[1] = p_bounds_max.y;
		next_snap->meta.world_bounds_max[2] = p_bounds_max.z;
		next_snap->meta.world_bounds_max[3] = 0.0f;
		next_snap->meta.world_to_local_rows[0][0] = 1.0f;
		next_snap->meta.world_to_local_rows[1][1] = 1.0f;
		next_snap->meta.world_to_local_rows[2][2] = 1.0f;
		next_snap->meta.local_to_world_rows[0][0] = 1.0f;
		next_snap->meta.local_to_world_rows[1][1] = 1.0f;
		next_snap->meta.local_to_world_rows[2][2] = 1.0f;

		if (device) {
			Vector<uint8_t> meta_bytes;
			meta_bytes.resize(sizeof(SceneSphereMeta208));
			memcpy(meta_bytes.ptrw(), &next_snap->meta, sizeof(SceneSphereMeta208));
			next_snap->meta_buffer = device->storage_buffer_create(sizeof(SceneSphereMeta208), meta_bytes);
			if (next_snap->meta_buffer.is_null()) {
				return ERR_CANT_CREATE;
			}
			live_rid_counter.increment();

			const uint32_t rec_byte_len = MAX(1u, p_colliders.size()) * uint32_t(sizeof(SceneSphereRecord32));
			Vector<uint8_t> rec_bytes;
			rec_bytes.resize(rec_byte_len);
			memset(rec_bytes.ptrw(), 0, rec_byte_len);
			if (!p_colliders.is_empty()) {
				memcpy(rec_bytes.ptrw(), p_colliders.ptr(), p_colliders.size() * sizeof(SceneSphereRecord32));
			}
			next_snap->records_buffer = device->storage_buffer_create(rec_byte_len, rec_bytes);
			if (next_snap->records_buffer.is_null()) {
				next_snap->release_gpu_resources();
				return ERR_CANT_CREATE;
			}
			live_rid_counter.increment();
		}

		prev_snap_drop = current_sphere_snapshot;
		current_sphere_snapshot = next_snap;
	}
	prev_snap_drop.unref();
	return OK;
}

Error ParticlesCollisionSceneRD::acquire_sphere_read_lease(
		Ref<SphereReadLease> &r_lease,
		GDExtensionSceneOpaque32V1 &r_sample_token) {
	Ref<SphereSnapshot> snap;
	GDExtensionSceneOpaque32V1 cap = {};
	{
		MutexLock lock(snapshot_mutex);
		if (current_sphere_snapshot.is_null() || GDExtensionDeviceExit::scene_opaque32_is_zero(device_capability)) {
			return ERR_UNCONFIGURED;
		}
		snap = current_sphere_snapshot;
		cap = device_capability;
	}
	r_sample_token = GDExtensionDeviceExit::scene_phase_opaque(
			SCENE_SPHERE_SAMPLE_KIND,
			snap->meta.scene_epoch,
			snap->meta.snapshot_generation,
			snap->meta.count);
	r_lease.instantiate(cap, r_sample_token, snap, &active_lease_counter);
	return OK;
}

CollisionContactResult ParticlesCollisionSceneRD::evaluate_sphere_collision(
		const LocalVector<SceneSphereRecord32> &p_spheres,
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_particle_radius,
		float p_dt) {
	CollisionContactResult out;
	if (!vec3_is_finite(p_position) || !vec3_is_finite(p_velocity) || !Math::is_finite(p_particle_radius) || !Math::is_finite(p_dt)) {
		out.post_position = sanitize_vec3(p_position);
		out.post_velocity = sanitize_vec3(p_velocity);
		return out;
	}
	const float safe_particle_radius = MAX(0.0f, p_particle_radius);
	const Vector3 candidate = p_position + p_velocity * p_dt;
	if (!vec3_is_finite(candidate)) {
		out.post_position = p_position;
		out.post_velocity = p_velocity;
		return out;
	}
	out.post_position = candidate;
	out.post_velocity = p_velocity;

	bool found = false;
	float best_penetration = -1.0f;
	uint64_t best_key = UINT64_MAX;
	uint32_t best_index = 0;
	Vector3 best_normal;

	for (uint32_t i = 0; i < p_spheres.size(); ++i) {
		const SceneSphereRecord32 &sphere = p_spheres[i];
		if (!Math::is_finite(sphere.center_x) || !Math::is_finite(sphere.center_y) || !Math::is_finite(sphere.center_z) ||
				!Math::is_finite(sphere.radius) || !(sphere.radius > 0.0f)) {
			continue;
		}
		const Vector3 center(sphere.center_x, sphere.center_y, sphere.center_z);
		const float combined_r = sphere.radius + safe_particle_radius;
		const Vector3 delta = candidate - center;
		const float dist_sq = delta.length_squared();
		if (!(dist_sq < combined_r * combined_r)) {
			continue;
		}
		const float dist = std::sqrt(dist_sq);
		const float penetration = combined_r - dist;
		if (!Math::is_finite(penetration) || !(penetration > 0.0f)) {
			continue;
		}
		const uint64_t key = sphere.get_collider_key();
		Vector3 normal = dist > 1e-6f ? (delta / dist) : Vector3(0.0f, 1.0f, 0.0f);

		// Deterministic tie-break: strictly higher penetration wins; exact ties
		// resolve by minimum u64 collider_key regardless of array permutation.
		if (!found || penetration > best_penetration || (penetration == best_penetration && key < best_key)) {
			found = true;
			best_penetration = penetration;
			best_key = key;
			best_index = i;
			best_normal = normal;
		}
	}

	if (!found) {
		return out;
	}

	const SceneSphereRecord32 &winner = p_spheres[best_index];
	const Vector3 winner_center(winner.center_x, winner.center_y, winner.center_z);
	const float combined_r = winner.radius + safe_particle_radius;

	out.collided = true;
	out.penetration = best_penetration;
	out.winning_collider_key = best_key;
	out.world_normal = best_normal;
	out.contact_point = winner_center + best_normal * winner.radius;
	out.post_position = winner_center + best_normal * combined_r;
	out.post_velocity = apply_bounce_and_friction(p_velocity, best_normal, winner.restitution, winner.friction);
	return out;
}

CollisionContactResult ParticlesCollisionSceneRD::evaluate_heightfield_d32_collision(
		const SceneHeightMeta352 &p_meta,
		const LocalVector<float> &p_raw_d32_texels,
		const Transform3D &p_height_to_world,
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_particle_radius,
		float p_dt) {
	CollisionContactResult out;
	if (!vec3_is_finite(p_position) || !vec3_is_finite(p_velocity) || !Math::is_finite(p_particle_radius) || !Math::is_finite(p_dt)) {
		out.post_position = sanitize_vec3(p_position);
		out.post_velocity = sanitize_vec3(p_velocity);
		return out;
	}
	const float safe_particle_radius = MAX(0.0f, p_particle_radius);
	const Vector3 candidate = p_position + p_velocity * p_dt;
	if (!vec3_is_finite(candidate)) {
		out.post_position = p_position;
		out.post_velocity = p_velocity;
		return out;
	}
	out.post_position = candidate;
	out.post_velocity = p_velocity;

	if (p_meta.width < 2 || p_meta.height < 2 || p_meta.width > 16384 || p_meta.height > 16384 ||
			p_raw_d32_texels.size() < uint64_t(p_meta.width) * p_meta.height ||
			!Math::is_finite(p_meta.z_near) || !Math::is_finite(p_meta.z_far) || !(p_meta.z_far > p_meta.z_near) ||
			!transform_is_finite(p_height_to_world)) {
		return out;
	}

	const Vector3 bmin(p_meta.bounds_min[0], p_meta.bounds_min[1], p_meta.bounds_min[2]);
	const Vector3 bmax(p_meta.bounds_max[0], p_meta.bounds_max[1], p_meta.bounds_max[2]);
	if (!vec3_is_finite(bmin) || !vec3_is_finite(bmax)) {
		return out;
	}
	const Vector3 extent = bmax - bmin;
	if (!(extent.x > 1e-6f) || !(extent.z > 1e-6f)) {
		return out;
	}

	const Transform3D world_to_height = p_height_to_world.affine_inverse();
	const Vector3 local_pos = world_to_height.xform(candidate);
	if (!vec3_is_finite(local_pos)) {
		return out;
	}

	const float u = (local_pos.x - bmin.x) / extent.x;
	const float v = (local_pos.z - bmin.z) / extent.z;
	if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
		return out;
	}

	const float fx = u * float(p_meta.width - 1);
	const float fy = v * float(p_meta.height - 1);
	const int x0 = CLAMP(int(std::floor(fx)), 0, int(p_meta.width) - 1);
	const int y0 = CLAMP(int(std::floor(fy)), 0, int(p_meta.height) - 1);
	const int x1 = MIN(x0 + 1, int(p_meta.width) - 1);
	const int y1 = MIN(y0 + 1, int(p_meta.height) - 1);
	const float tx = fx - float(x0);
	const float ty = fy - float(y0);

	auto fetch_raw = [&](int x, int y) -> float {
		return p_raw_d32_texels[uint32_t(y) * p_meta.width + uint32_t(x)];
	};

	const float r00 = fetch_raw(x0, y0);
	const float r10 = fetch_raw(x1, y0);
	const float r01 = fetch_raw(x0, y1);
	const float r11 = fetch_raw(x1, y1);
	if (!Math::is_finite(r00) || !Math::is_finite(r10) || !Math::is_finite(r01) || !Math::is_finite(r11)) {
		return out;
	}

	// Reverse-Z (raw <= 0.0f) or Forward-Z (raw >= 1.0f) empty sentinel: no geometry hit!
	const bool hf_sky = (p_meta.reverse_z != 0)
			? (r00 <= 0.0f || r10 <= 0.0f || r01 <= 0.0f || r11 <= 0.0f)
			: (r00 >= 1.0f || r10 >= 1.0f || r01 >= 1.0f || r11 >= 1.0f);
	if (hf_sky) {
		out.empty_depth = true;
		return out;
	}

	auto raw_to_local_y = [&](float raw) -> float {
		const float linear_d = reverse_z_to_linear_depth(raw, p_meta.z_near, p_meta.z_far, p_meta.reverse_z != 0);
		return bmax.y - (linear_d - p_meta.z_near);
	};

	const float h00 = raw_to_local_y(r00);
	const float h10 = raw_to_local_y(r10);
	const float h01 = raw_to_local_y(r01);
	const float h11 = raw_to_local_y(r11);

	const float h0 = Math::lerp(h00, h10, tx);
	const float h1 = Math::lerp(h01, h11, tx);
	const float surface_y = Math::lerp(h0, h1, ty);
	if (!Math::is_finite(surface_y)) {
		return out;
	}

	const float dx_step = extent.x / float(MAX(1u, p_meta.width - 1));
	const float dz_step = extent.z / float(MAX(1u, p_meta.height - 1));
	const float dh_dx = ((1.0f - ty) * (h10 - h00) + ty * (h11 - h01)) / MAX(dx_step, 1e-6f);
	const float dh_dz = ((1.0f - tx) * (h01 - h00) + tx * (h11 - h10)) / MAX(dz_step, 1e-6f);

	const Vector3 local_normal = Vector3(-dh_dx, 1.0f, -dh_dz).normalized();
	const Basis inv_transpose_basis = p_height_to_world.basis.inverse().transposed();
	Vector3 world_normal = inv_transpose_basis.xform(local_normal);
	if (vec3_is_finite(world_normal) && world_normal.length_squared() > 1e-12f) {
		world_normal.normalize();
	} else {
		world_normal = Vector3(0.0f, 1.0f, 0.0f);
	}

	const Vector3 contact_world = p_height_to_world.xform(Vector3(local_pos.x, surface_y, local_pos.z));
	const float signed_normal_dist = (candidate - contact_world).dot(world_normal);
	if (!(signed_normal_dist < safe_particle_radius)) {
		return out;
	}

	out.collided = true;
	out.penetration = MAX(0.0f, safe_particle_radius - signed_normal_dist);
	out.contact_point = contact_world;
	out.world_normal = world_normal;
	out.post_position = candidate + world_normal * out.penetration;
	out.post_velocity = apply_bounce_and_friction(p_velocity, world_normal, p_meta.restitution, p_meta.friction);
	return out;
}

CollisionContactResult ParticlesCollisionSceneRD::evaluate_current_depth_collision(
		const SceneCurrentDepthMeta256 &p_meta,
		const LocalVector<float> &p_raw_d32_texels,
		const Transform3D &p_view_to_world,
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_particle_radius,
		float p_dt) {
	CollisionContactResult out;
	if (!vec3_is_finite(p_position) || !vec3_is_finite(p_velocity) || !Math::is_finite(p_particle_radius) || !Math::is_finite(p_dt)) {
		out.post_position = sanitize_vec3(p_position);
		out.post_velocity = sanitize_vec3(p_velocity);
		return out;
	}
	const float safe_particle_radius = MAX(0.0f, p_particle_radius);
	const Vector3 candidate = p_position + p_velocity * p_dt;
	if (!vec3_is_finite(candidate)) {
		out.post_position = p_position;
		out.post_velocity = p_velocity;
		return out;
	}
	out.post_position = candidate;
	out.post_velocity = p_velocity;

	if (p_meta.width < 2 || p_meta.height < 2 || p_meta.width > 16384 || p_meta.height > 16384 ||
			p_raw_d32_texels.size() < uint64_t(p_meta.width) * p_meta.height ||
			!Math::is_finite(p_meta.z_near) || !Math::is_finite(p_meta.z_far) || !(p_meta.z_near > 0.0f) || !(p_meta.z_far > p_meta.z_near) ||
			!Math::is_finite(p_meta.thickness) || !(p_meta.thickness >= 0.0f) ||
			!transform_is_finite(p_view_to_world)) {
		return out;
	}
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			if (!Math::is_finite(p_meta.projection_rows[r][c])) {
				return out;
			}
		}
	}

	const Transform3D world_to_view = p_view_to_world.affine_inverse();
	const Vector3 view_pos = world_to_view.xform(candidate);
	if (!vec3_is_finite(view_pos)) {
		return out;
	}
	// Godot camera looks along -Z in view space; positive linear depth is -view_pos.z.
	const float particle_linear_z = -view_pos.z;
	if (!(particle_linear_z > p_meta.z_near && particle_linear_z < p_meta.z_far)) {
		return out;
	}

	// Project using row0[0] (proj_x), row1[1] (proj_y), principal-point offsets (m02, m12),
	// and handle both perspective (row3[3] == 0) and orthographic (row3[3] ~= 1) projections.
	const float proj_x = std::abs(p_meta.projection_rows[0][0]) > 1e-6f ? p_meta.projection_rows[0][0] : 1.0f;
	const float proj_y = std::abs(p_meta.projection_rows[1][1]) > 1e-6f ? p_meta.projection_rows[1][1] : 1.0f;
	const float m02 = p_meta.projection_rows[0][2];
	const float m12 = p_meta.projection_rows[1][2];
	const float m03 = p_meta.projection_rows[0][3];
	const float m13 = p_meta.projection_rows[1][3];
	const bool is_ortho = (std::abs(p_meta.projection_rows[3][3] - 1.0f) < 1e-4f) && (std::abs(p_meta.projection_rows[3][2]) < 1e-4f);

	const float ndc_x = is_ortho
			? (view_pos.x * proj_x + view_pos.z * m02 + m03)
			: ((view_pos.x * proj_x) / particle_linear_z - m02 + m03 / particle_linear_z);
	const float ndc_y = is_ortho
			? (view_pos.y * proj_y + view_pos.z * m12 + m13)
			: ((view_pos.y * proj_y) / particle_linear_z - m12 + m13 / particle_linear_z);
	const float u = ndc_x * 0.5f + 0.5f;
	const float v = ndc_y * 0.5f + 0.5f;
	if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
		return out;
	}

	const int px = CLAMP(int(u * float(p_meta.width)), 0, int(p_meta.width) - 1);
	const int py = CLAMP(int(v * float(p_meta.height)), 0, int(p_meta.height) - 1);
	auto fetch_raw = [&](int x, int y) -> float {
		const int cx = CLAMP(x, 0, int(p_meta.width) - 1);
		const int cy = CLAMP(y, 0, int(p_meta.height) - 1);
		return p_raw_d32_texels[uint32_t(cy) * p_meta.width + uint32_t(cx)];
	};

	const float raw_center = fetch_raw(px, py);
	if (!Math::is_finite(raw_center)) {
		return out;
	}
	if ((p_meta.reverse_z != 0 && raw_center <= 0.0f) || (p_meta.reverse_z == 0 && raw_center >= 1.0f)) {
		out.empty_depth = true;
		return out;
	}

	auto raw_to_view_depth = [&](float raw) -> float {
		if (is_ortho) {
			return reverse_z_to_linear_depth(raw, p_meta.z_near, p_meta.z_far, p_meta.reverse_z != 0);
		}
		const float n = p_meta.z_near;
		const float f = p_meta.z_far;
		if (p_meta.reverse_z != 0) {
			if (!(raw > 1e-7f)) {
				return f;
			}
			const float denom = raw * (f - n) + n;
			return denom > 1e-7f ? (n * f) / denom : f;
		}
		if (raw >= 1.0f - 1e-7f) {
			return f;
		}
		const float denom = f - raw * (f - n);
		return denom > 1e-7f ? (n * f) / denom : f;
	};

	const float scene_z = raw_to_view_depth(raw_center);
	const float front_z = particle_linear_z + safe_particle_radius;
	// Collide only within [scene_z, scene_z + thickness] so particles behind thin foreground objects don't snap!
	if (!Math::is_finite(scene_z) || !(front_z >= scene_z && (particle_linear_z - safe_particle_radius) <= scene_z + p_meta.thickness)) {
		return out;
	}

	const float raw_r = fetch_raw(px + 1, py);
	const float raw_u = fetch_raw(px, py + 1);
	Vector3 view_normal(0.0f, 0.0f, 1.0f); // View-aligned fallback toward camera (+Z in Godot view space)

	const float safe_silhouette_threshold = Math::is_finite(p_meta.silhouette_threshold) ? MAX(0.0f, p_meta.silhouette_threshold) : 0.0f;
	const bool neighbors_valid = Math::is_finite(raw_r) && Math::is_finite(raw_u) &&
			((p_meta.reverse_z != 0) ? (raw_r > 0.0f && raw_u > 0.0f) : (raw_r < 1.0f && raw_u < 1.0f));
	if (neighbors_valid) {
		const float z_r = raw_to_view_depth(raw_r);
		const float z_u = raw_to_view_depth(raw_u);
		if (std::abs(z_r - scene_z) <= safe_silhouette_threshold &&
				std::abs(z_u - scene_z) <= safe_silhouette_threshold) {
			auto reconstruct_view = [&](int sx, int sy, float lin_z) -> Vector3 {
				const float su = (float(sx) + 0.5f) / float(p_meta.width);
				const float sv = (float(sy) + 0.5f) / float(p_meta.height);
				const float sndc_x = su * 2.0f - 1.0f;
				const float sndc_y = sv * 2.0f - 1.0f;
				if (is_ortho) {
					return Vector3(
							(sndc_x + m02 * lin_z - m03) / proj_x,
							(sndc_y + m12 * lin_z - m13) / proj_y,
							-lin_z);
				}
				return Vector3(
						((sndc_x + m02) * lin_z - m03) / proj_x,
						((sndc_y + m12) * lin_z - m13) / proj_y,
						-lin_z);
			};
			const Vector3 p_c = reconstruct_view(px, py, scene_z);
			const Vector3 p_r = reconstruct_view(px + 1, py, z_r);
			const Vector3 p_u = reconstruct_view(px, py + 1, z_u);
			Vector3 n = (p_r - p_c).cross(p_u - p_c);
			if (vec3_is_finite(n) && n.length_squared() > 1e-10f) {
				n.normalize();
				if (n.z < 0.0f) {
					n = -n;
				}
				view_normal = n;
			}
		} else {
			out.silhouette_fallback_used = true;
		}
	} else {
		out.silhouette_fallback_used = true;
	}

	Vector3 world_normal = p_view_to_world.basis.xform(view_normal);
	if (vec3_is_finite(world_normal) && world_normal.length_squared() > 1e-12f) {
		world_normal.normalize();
	} else {
		world_normal = Vector3(0.0f, 0.0f, 1.0f);
	}
	const Vector3 contact_world = p_view_to_world.xform(Vector3(view_pos.x, view_pos.y, -scene_z));
	const float signed_normal_dist = (candidate - contact_world).dot(world_normal);
	out.collided = true;
	out.penetration = signed_normal_dist < safe_particle_radius ? (safe_particle_radius - signed_normal_dist) : (front_z - scene_z);
	out.contact_point = contact_world;
	out.world_normal = world_normal;
	out.post_position = candidate + world_normal * out.penetration;
	out.post_velocity = apply_bounce_and_friction(p_velocity, world_normal, p_meta.restitution, p_meta.friction);
	return out;
}

CollisionContactResult ParticlesCollisionSceneRD::evaluate_sdf_3d_collision(
		const SceneSdfMeta256 &p_meta,
		const LocalVector<float> &p_sdf_voxels,
		const Transform3D &p_world_to_grid_box,
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_particle_radius,
		float p_dt) {
	CollisionContactResult out;
	if (!vec3_is_finite(p_position) || !vec3_is_finite(p_velocity) || !Math::is_finite(p_particle_radius) || !Math::is_finite(p_dt)) {
		out.post_position = sanitize_vec3(p_position);
		out.post_velocity = sanitize_vec3(p_velocity);
		return out;
	}
	const float safe_particle_radius = MAX(0.0f, p_particle_radius);
	const Vector3 candidate = p_position + p_velocity * p_dt;
	if (!vec3_is_finite(candidate)) {
		out.post_position = p_position;
		out.post_velocity = p_velocity;
		return out;
	}
	out.post_position = candidate;
	out.post_velocity = p_velocity;

	if (p_meta.dim_x < 2 || p_meta.dim_y < 2 || p_meta.dim_z < 2 ||
			p_meta.dim_x > 4096 || p_meta.dim_y > 4096 || p_meta.dim_z > 4096 ||
			p_sdf_voxels.size() < uint64_t(p_meta.dim_x) * p_meta.dim_y * p_meta.dim_z ||
			!transform_is_finite(p_world_to_grid_box)) {
		return out;
	}

	const Vector3 local_pos = p_world_to_grid_box.xform(candidate);
	if (!vec3_is_finite(local_pos)) {
		return out;
	}
	const Vector3 bmin(p_meta.bounds_min[0], p_meta.bounds_min[1], p_meta.bounds_min[2]);
	const Vector3 bmax(p_meta.bounds_max[0], p_meta.bounds_max[1], p_meta.bounds_max[2]);
	if (!vec3_is_finite(bmin) || !vec3_is_finite(bmax)) {
		return out;
	}
	const Vector3 extent = bmax - bmin;
	const bool has_explicit_bounds = (extent.x > 1e-6f && extent.y > 1e-6f && extent.z > 1e-6f);
	const Vector3 uvw = has_explicit_bounds ? (local_pos - bmin) / extent : local_pos;
	if (!(uvw.x >= 0.0f && uvw.x <= 1.0f && uvw.y >= 0.0f && uvw.y <= 1.0f && uvw.z >= 0.0f && uvw.z <= 1.0f)) {
		// Outside SDF domain: graceful no-op, never NaN!
		return out;
	}

	const float signed_dist = sample_trilinear_sdf(p_meta, p_sdf_voxels, uvw);
	if (!Math::is_finite(signed_dist) || !(signed_dist < safe_particle_radius)) {
		return out;
	}

	// 4-tap tetrahedral central difference in UVW space:
	const float h = 0.5f / float(MAX(2u, MAX(p_meta.dim_x, MAX(p_meta.dim_y, p_meta.dim_z))) - 1u);
	const Vector3 k0(1.0f, -1.0f, -1.0f);
	const Vector3 k1(-1.0f, -1.0f, 1.0f);
	const Vector3 k2(-1.0f, 1.0f, -1.0f);
	const Vector3 k3(1.0f, 1.0f, 1.0f);

	const float d0 = sample_trilinear_sdf(p_meta, p_sdf_voxels, uvw + k0 * h);
	const float d1 = sample_trilinear_sdf(p_meta, p_sdf_voxels, uvw + k1 * h);
	const float d2 = sample_trilinear_sdf(p_meta, p_sdf_voxels, uvw + k2 * h);
	const float d3 = sample_trilinear_sdf(p_meta, p_sdf_voxels, uvw + k3 * h);

	const Vector3 grad_uvw = k0 * d0 + k1 * d1 + k2 * d2 + k3 * d3;
	const Vector3 grad_local = has_explicit_bounds ? (grad_uvw / extent) : grad_uvw;
	Vector3 world_normal = p_world_to_grid_box.basis.transposed().xform(grad_local);
	if (vec3_is_finite(world_normal) && world_normal.length_squared() > 1e-12f) {
		world_normal.normalize();
	} else {
		world_normal = Vector3(0.0f, 1.0f, 0.0f);
	}

	const float penetration = safe_particle_radius - signed_dist;
	out.collided = true;
	out.penetration = penetration;
	out.world_normal = world_normal;
	out.contact_point = candidate - world_normal * signed_dist;
	out.post_position = candidate + world_normal * penetration;
	out.post_velocity = apply_bounce_and_friction(p_velocity, world_normal, p_meta.restitution, p_meta.friction);
	return out;
}

void ParticlesCollisionSceneRD::fill_depth_snapshot_matrices(
		const Transform3D &p_cam_transform,
		const Projection &p_cam_projection,
		float p_z_near,
		float p_z_far,
		SceneDepthSnapshotMeta256 &r_meta) {
	r_meta.magic = SceneDepthSnapshotMeta256::MAGIC;
	r_meta.version = SceneDepthSnapshotMeta256::VERSION;
	r_meta.z_near = p_z_near;
	if (!Math::is_finite(r_meta.slab_thickness_m) || r_meta.slab_thickness_m <= 0.0f) {
		r_meta.slab_thickness_m = 0.50f;
	}
	if (!Math::is_finite(r_meta.silhouette_jump_m) || r_meta.silhouette_jump_m <= 0.0f) {
		r_meta.silhouette_jump_m = 0.50f;
	}
	for (int i = 0; i < 4; ++i) {
		r_meta.reserved[i] = 0u;
	}

	bool valid = Math::is_finite(p_z_near) && Math::is_finite(p_z_far) &&
			(p_z_near > 0.0f) && (p_z_far > p_z_near) &&
			transform_is_finite(p_cam_transform);
	if (valid) {
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				if (!Math::is_finite(p_cam_projection.columns[c][r])) {
					valid = false;
					break;
				}
			}
			if (!valid) {
				break;
			}
		}
		if (valid) {
			const real_t proj_det = p_cam_projection.determinant();
			if (!Math::is_finite(proj_det) || Math::abs(proj_det) <= real_t(1e-20)) {
				valid = false;
			}
		}
	}

	if (!valid) {
		memset(r_meta.world_to_clip, 0, sizeof(r_meta.world_to_clip));
		memset(r_meta.clip_to_world, 0, sizeof(r_meta.clip_to_world));
		memset(r_meta.view_to_world, 0, sizeof(r_meta.view_to_world));
		r_meta.flags = SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;
		return;
	}

	const Projection view_from_world(p_cam_transform.affine_inverse());
	const Projection world_to_clip = p_cam_projection * view_from_world;
	const Projection inv_projection = p_cam_projection.inverse();
	const Projection view_to_world_proj(p_cam_transform);
	const Projection clip_to_world = view_to_world_proj * inv_projection;

	bool all_finite = true;
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			const int idx = c * 4 + r;
			r_meta.world_to_clip[idx] = float(world_to_clip.columns[c][r]);
			r_meta.clip_to_world[idx] = float(clip_to_world.columns[c][r]);
			r_meta.view_to_world[idx] = float(view_to_world_proj.columns[c][r]);
			if (!Math::is_finite(r_meta.world_to_clip[idx]) ||
					!Math::is_finite(r_meta.clip_to_world[idx]) ||
					!Math::is_finite(r_meta.view_to_world[idx])) {
				all_finite = false;
			}
		}
	}

	if (!all_finite) {
		memset(r_meta.world_to_clip, 0, sizeof(r_meta.world_to_clip));
		memset(r_meta.clip_to_world, 0, sizeof(r_meta.clip_to_world));
		memset(r_meta.view_to_world, 0, sizeof(r_meta.view_to_world));
		r_meta.flags = SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;
		return;
	}

	r_meta.flags = SceneDepthSnapshotMeta256::FLAG_PRESENT | SceneDepthSnapshotMeta256::FLAG_REVERSE_Z;
}

CollisionContactResult ParticlesCollisionSceneRD::evaluate_depth_snapshot_v2_collision(
		const SceneDepthSnapshotMeta256 &p_meta,
		const LocalVector<float> &p_depth_texels,
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_particle_radius,
		float p_dt,
		float p_restitution,
		float p_friction,
		float p_slab_thickness_override_m,
		float p_silhouette_jump_override_m) {
	CollisionContactResult out;
	if (!vec3_is_finite(p_position) || !vec3_is_finite(p_velocity) ||
			!Math::is_finite(p_particle_radius) || !Math::is_finite(p_dt)) {
		out.post_position = sanitize_vec3(p_position);
		out.post_velocity = sanitize_vec3(p_velocity);
		return out;
	}
	const float safe_particle_radius = MAX(0.0f, p_particle_radius);
	const Vector3 candidate = p_position + p_velocity * p_dt;
	if (!vec3_is_finite(candidate)) {
		out.post_position = p_position;
		out.post_velocity = p_velocity;
		return out;
	}
	out.post_position = candidate;
	out.post_velocity = p_velocity;

	if (p_meta.magic != SceneDepthSnapshotMeta256::MAGIC ||
			p_meta.version != SceneDepthSnapshotMeta256::VERSION ||
			(p_meta.flags & ~3u) != 0u ||
			(p_meta.flags & SceneDepthSnapshotMeta256::FLAG_PRESENT) == 0u ||
			p_meta.width < 2u || p_meta.height < 2u ||
			p_meta.width > 16384u || p_meta.height > 16384u ||
			p_depth_texels.size() < uint64_t(p_meta.width) * uint64_t(p_meta.height) ||
			!Math::is_finite(p_meta.z_near) || !(p_meta.z_near > 0.0f)) {
		return out;
	}

	for (int i = 0; i < 16; ++i) {
		if (!Math::is_finite(p_meta.world_to_clip[i]) ||
				!Math::is_finite(p_meta.clip_to_world[i]) ||
				!Math::is_finite(p_meta.view_to_world[i])) {
			return out;
		}
	}

	const Transform3D view_to_world_xf = transform_from_col_major_mat4(p_meta.view_to_world);
	if (!transform_is_finite(view_to_world_xf)) {
		return out;
	}

	const float slab_m = (Math::is_finite(p_slab_thickness_override_m) && p_slab_thickness_override_m >= 0.0f)
			? p_slab_thickness_override_m
			: p_meta.slab_thickness_m;
	const float sil_m = (Math::is_finite(p_silhouette_jump_override_m) && p_silhouette_jump_override_m >= 0.0f)
			? p_silhouette_jump_override_m
			: p_meta.silhouette_jump_m;
	if (!Math::is_finite(slab_m) || !(slab_m >= 0.0f) || !Math::is_finite(sil_m) || !(sil_m >= 0.0f)) {
		return out;
	}

	const Transform3D world_to_view_xf = view_to_world_xf.affine_inverse();
	const Vector3 cand_view = world_to_view_xf.xform(candidate);
	if (!vec3_is_finite(cand_view)) {
		return out;
	}
	const float particle_linear_z = -cand_view.z;
	if (!(particle_linear_z > p_meta.z_near)) {
		return out;
	}

	const Vector4 clip = mat4_mul_vec4_col_major(p_meta.world_to_clip, Vector4(candidate.x, candidate.y, candidate.z, 1.0f));
	if (!Math::is_finite(clip.x) || !Math::is_finite(clip.y) || !Math::is_finite(clip.z) ||
			!Math::is_finite(clip.w) || !(clip.w > 1e-6f)) {
		return out;
	}
	const float ndc_x = clip.x / clip.w;
	const float ndc_y = clip.y / clip.w;
	const float u = ndc_x * 0.5f + 0.5f;
	const float v = ndc_y * 0.5f + 0.5f;
	if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
		return out;
	}

	const int px = CLAMP(int(u * float(p_meta.width)), 0, int(p_meta.width) - 1);
	const int py = CLAMP(int(v * float(p_meta.height)), 0, int(p_meta.height) - 1);
	auto fetch_texel = [&](int x, int y) -> float {
		const int cx = CLAMP(x, 0, int(p_meta.width) - 1);
		const int cy = CLAMP(y, 0, int(p_meta.height) - 1);
		return p_depth_texels[uint32_t(cy) * p_meta.width + uint32_t(cx)];
	};

	const bool reverse_z = (p_meta.flags & SceneDepthSnapshotMeta256::FLAG_REVERSE_Z) != 0u;
	auto is_empty_depth_texel = [&](float raw_d) -> bool {
		if (!Math::is_finite(raw_d)) {
			return true;
		}
		return reverse_z ? (raw_d <= 1e-7f || raw_d > 1.0f) : (raw_d >= 1.0f - 1e-7f || raw_d < 0.0f);
	};

	const float d_c = fetch_texel(px, py);
	if (!Math::is_finite(d_c)) {
		return out;
	}
	if (is_empty_depth_texel(d_c)) {
		out.empty_depth = true;
		return out;
	}

	auto unproject_ndc = [&](float p_nx, float p_ny, float p_raw_d, Vector3 &r_world) -> bool {
		const Vector4 w4 = mat4_mul_vec4_col_major(p_meta.clip_to_world, Vector4(p_nx, p_ny, p_raw_d, 1.0f));
		if (!Math::is_finite(w4.x) || !Math::is_finite(w4.y) || !Math::is_finite(w4.z) ||
				!Math::is_finite(w4.w) || Math::abs(w4.w) <= 1e-7f) {
			return false;
		}
		r_world = Vector3(w4.x / w4.w, w4.y / w4.w, w4.z / w4.w);
		return vec3_is_finite(r_world);
	};
	auto unproject_texel = [&](int sx, int sy, float p_raw_d, Vector3 &r_world) -> bool {
		const float su = (float(sx) + 0.5f) / float(p_meta.width);
		const float sv = (float(sy) + 0.5f) / float(p_meta.height);
		return unproject_ndc(su * 2.0f - 1.0f, sv * 2.0f - 1.0f, p_raw_d, r_world);
	};

	Vector3 p_c;
	if (!unproject_texel(px, py, d_c, p_c)) {
		return out;
	}
	const float z_c = -(world_to_view_xf.xform(p_c)).z;
	if (!Math::is_finite(z_c) || !(z_c > 0.0f)) {
		return out;
	}

	const float front_z = particle_linear_z + safe_particle_radius;
	const float back_z = particle_linear_z - safe_particle_radius;
	if (!(front_z >= z_c && back_z <= z_c + slab_m)) {
		return out;
	}

	Vector3 contact_world = p_c;
	Vector3 ray_surface;
	if (unproject_ndc(ndc_x, ndc_y, d_c, ray_surface)) {
		contact_world = ray_surface;
	}

	const Vector3 cam_eye(p_meta.view_to_world[12], p_meta.view_to_world[13], p_meta.view_to_world[14]);
	Vector3 cam_back(p_meta.view_to_world[8], p_meta.view_to_world[9], p_meta.view_to_world[10]);
	if (cam_back.length_squared() > 1e-12f) {
		cam_back.normalize();
	} else {
		cam_back = Vector3(0.0f, 0.0f, 1.0f);
	}
	const bool is_ortho = (Math::abs(p_meta.world_to_clip[11]) < 1e-5f) && (Math::abs(p_meta.world_to_clip[15]) > 1e-5f);
	Vector3 eye_normal = cam_back;
	if (!is_ortho) {
		const Vector3 to_eye = cam_eye - contact_world;
		if (vec3_is_finite(to_eye) && to_eye.length_squared() > 1e-12f) {
			eye_normal = to_eye.normalized();
		}
	}

	Vector3 world_normal = eye_normal;
	const int rx = MIN(px + 1, int(p_meta.width) - 1);
	const int uy = MIN(py + 1, int(p_meta.height) - 1);
	if (rx != px && uy != py) {
		const float d_r = fetch_texel(rx, py);
		const float d_u = fetch_texel(px, uy);
		Vector3 p_r;
		Vector3 p_u;
		if (!is_empty_depth_texel(d_r) && !is_empty_depth_texel(d_u) &&
				unproject_texel(rx, py, d_r, p_r) && unproject_texel(px, uy, d_u, p_u)) {
			const float z_r = -(world_to_view_xf.xform(p_r)).z;
			const float z_u = -(world_to_view_xf.xform(p_u)).z;
			if (Math::is_finite(z_r) && Math::is_finite(z_u) &&
					Math::abs(z_c - z_r) <= sil_m && Math::abs(z_c - z_u) <= sil_m) {
				Vector3 n = (p_r - p_c).cross(p_u - p_c);
				if (vec3_is_finite(n) && n.length_squared() > 1e-12f) {
					n.normalize();
					if (n.dot(eye_normal) < 0.0f) {
						n = -n;
					}
					world_normal = n;
				} else {
					out.silhouette_fallback_used = true;
				}
			} else {
				out.silhouette_fallback_used = true;
			}
		} else {
			out.silhouette_fallback_used = true;
		}
	} else {
		out.silhouette_fallback_used = true;
	}

	const float signed_normal_dist = (candidate - contact_world).dot(world_normal);
	out.collided = true;
	out.penetration = signed_normal_dist < safe_particle_radius
			? (safe_particle_radius - signed_normal_dist)
			: MAX(0.0f, front_z - z_c);
	out.contact_point = contact_world;
	out.world_normal = world_normal;
	out.post_position = candidate + world_normal * out.penetration;
	out.post_velocity = apply_bounce_and_friction(p_velocity, world_normal, p_restitution, p_friction);
	return out;
}

} // namespace RendererRD
