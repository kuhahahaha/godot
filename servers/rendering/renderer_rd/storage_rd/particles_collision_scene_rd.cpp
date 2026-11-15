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

#include "core/extension/gdextension_scene_phase_bind.h"
#include "servers/rendering/rendering_device.h"

#include <cmath>
#include <cstring>

namespace RendererRD {

namespace {

constexpr uint64_t SCENE_SPHERE_SAMPLE_KIND = 0x4b30365350485231ull; // "K06SPHR1"

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

ParticlesCollisionSceneRD::~ParticlesCollisionSceneRD() {
	shutdown();
}

void ParticlesCollisionSceneRD::initialize(RenderingDevice *p_device, const GDExtensionSceneOpaque32V1 &p_device_capability) {
	shutdown();
	MutexLock lock(snapshot_mutex);
	device = p_device;
	live_device_guard.instantiate();
	live_device_guard->set_device(p_device);
	live_device_guard->owner_thread_id = p_device ? Thread::get_caller_id() : 0;
	device_capability = p_device_capability;
	next_snapshot_generation = 1;
	scene_epoch = 1;
}

void ParticlesCollisionSceneRD::shutdown() {
	Ref<SphereSnapshot> snap_to_drop;
	Ref<LiveDeviceGuard> guard_to_retire;
	{
		MutexLock lock(snapshot_mutex);
		snap_to_drop = current_sphere_snapshot;
		current_sphere_snapshot.unref();
		guard_to_retire = live_device_guard;
		live_device_guard.unref();
		device = nullptr;
		device_capability = {};
	}
	if (snap_to_drop.is_valid() && guard_to_retire.is_valid() && guard_to_retire->owner_thread_id == Thread::get_caller_id()) {
		snap_to_drop->release_gpu_resources();
	}
	snap_to_drop.unref();
	if (guard_to_retire.is_valid()) {
		guard_to_retire->set_device(nullptr);
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

} // namespace RendererRD
