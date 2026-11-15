/**************************************************************************/
/*  particles_collision_scene_rd.h                                        */
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

#include "core/extension/gdextension_scene_phase.h"
#include "core/math/projection.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"

#include <cstdint>
#include <type_traits>

class RenderingDevice;

namespace RendererRD {

struct SceneSphereRecord32 {
	float center_x = 0.0f;
	float center_y = 0.0f;
	float center_z = 0.0f;
	float radius = 0.0f;
	float restitution = 0.5f;
	float friction = 0.1f;
	uint32_t collider_key_lo = 0;
	uint32_t collider_key_hi = 0;

	void set_collider_key(uint64_t p_key) {
		collider_key_lo = uint32_t(p_key & 0xffffffffu);
		collider_key_hi = uint32_t(p_key >> 32u);
	}
	uint64_t get_collider_key() const {
		return (uint64_t(collider_key_hi) << 32u) | uint64_t(collider_key_lo);
	}
};

struct SceneSphereMeta208 {
	uint32_t magic = 0x4b303653u; // "K06S"
	uint32_t version = 1;
	uint32_t count = 0;
	uint32_t max_colliders = 0;
	uint64_t snapshot_generation = 0;
	uint64_t scene_epoch = 0;
	float world_bounds_min[4] = {};
	float world_bounds_max[4] = {};
	float world_to_local_rows[3][4] = {};
	float local_to_world_rows[3][4] = {};
	float default_restitution = 0.5f;
	float default_friction = 0.1f;
	float skin_thickness = 0.0f;
	float max_penetration = 10.0f;
	uint32_t reserved[8] = {};
};

struct SceneHeightMeta352 {
	uint32_t magic = 0x4b303648u; // "K06H"
	uint32_t version = 1;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t snapshot_generation = 0;
	uint64_t scene_epoch = 0;
	float world_to_height_rows[4][4] = {};
	float height_to_world_rows[4][4] = {};
	float height_to_world_inv_transpose_rows[3][4] = {};
	float bounds_min[4] = {};
	float bounds_max[4] = {};
	float z_near = 0.05f;
	float z_far = 100.0f;
	float restitution = 0.4f;
	float friction = 0.2f;
	uint32_t reverse_z = 1;
	uint32_t empty_sentinel_bits = 0; // 0.0f in Reverse-Z D32_SFLOAT
	uint32_t clamp_mode = 0;
	uint32_t flags = 0;
	uint32_t reserved[20] = {};
};

struct SceneCurrentDepthMeta256 {
	uint32_t magic = 0x4b303644u; // "K06D"
	uint32_t version = 1;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t snapshot_generation = 0;
	uint64_t frame_number = 0;
	float world_to_view_rows[4][4] = {};
	float view_to_world_rows[4][4] = {};
	float projection_rows[4][4] = {};
	float z_near = 0.05f;
	float z_far = 4000.0f;
	float thickness = 0.5f;
	float silhouette_threshold = 0.5f;
	float restitution = 0.4f;
	float friction = 0.2f;
	uint32_t reverse_z = 1;
	uint32_t view_count = 1;
};

struct SceneSdfMeta256 {
	uint32_t magic = 0x4b303656u; // "K06V"
	uint32_t version = 1;
	uint32_t dim_x = 0;
	uint32_t dim_y = 0;
	uint32_t dim_z = 0;
	uint32_t flags = 0;
	uint64_t snapshot_generation = 0;
	float world_to_uvw_rows[3][4] = {};
	float uvw_to_world_rows[3][4] = {};
	float uvw_to_world_inv_transpose_rows[3][4] = {};
	float bounds_min[4] = {};
	float bounds_max[4] = {};
	float voxel_size = 0.1f;
	float narrow_band_world = 1.0f;
	float restitution = 0.5f;
	float friction = 0.1f;
	uint32_t reserved[8] = {};
};

static_assert(std::is_standard_layout_v<SceneSphereRecord32> && sizeof(SceneSphereRecord32) == 32);
static_assert(std::is_standard_layout_v<SceneSphereMeta208> && sizeof(SceneSphereMeta208) == 208);
static_assert(std::is_standard_layout_v<SceneHeightMeta352> && sizeof(SceneHeightMeta352) == 352);
static_assert(std::is_standard_layout_v<SceneCurrentDepthMeta256> && sizeof(SceneCurrentDepthMeta256) == 256);
static_assert(std::is_standard_layout_v<SceneSdfMeta256> && sizeof(SceneSdfMeta256) == 256);

struct CollisionContactResult {
	bool collided = false;
	bool empty_depth = false;
	bool silhouette_fallback_used = false;
	float penetration = 0.0f;
	uint64_t winning_collider_key = 0;
	Vector3 contact_point;
	Vector3 world_normal;
	Vector3 post_position;
	Vector3 post_velocity;
};

class ParticlesCollisionSceneRD {
public:
	class LiveDeviceGuard : public RefCounted {
	public:
		SafeNumeric<uint64_t> rd_addr{ 0 };
		Thread::ID owner_thread_id = 0;

		void set_device(RenderingDevice *p_rd) {
			rd_addr.set(uint64_t(uintptr_t(p_rd)));
		}
		RenderingDevice *get_device() const {
			return reinterpret_cast<RenderingDevice *>(uintptr_t(rd_addr.get()));
		}
	};

	class SphereSnapshot : public RefCounted {
	public:
		RenderingDevice *rd = nullptr;
		Ref<LiveDeviceGuard> device_guard;
		SafeNumeric<uint32_t> *live_rids = nullptr;
		RID meta_buffer;
		RID records_buffer;
		SceneSphereMeta208 meta;
		LocalVector<SceneSphereRecord32> records;

		~SphereSnapshot() override;
		void release_gpu_resources();
	};

	class SphereReadLease : public GDExtensionDeviceExit::SceneReadContent {
		GDExtensionSceneOpaque32V1 expected_device = {};
		GDExtensionSceneOpaque32V1 expected_sample = {};
		Ref<SphereSnapshot> snapshot;
		SafeNumeric<uint32_t> *active_leases = nullptr;
		bool returned = false;

	public:
		SphereReadLease(
				const GDExtensionSceneOpaque32V1 &p_device,
				const GDExtensionSceneOpaque32V1 &p_sample,
				const Ref<SphereSnapshot> &p_snapshot,
				SafeNumeric<uint32_t> *p_active_leases);
		~SphereReadLease() override;

		Error validate(const GDExtensionSceneOpaque32V1 &p_device, const GDExtensionSceneOpaque32V1 &p_sample) const override;
		Error return_after_reads() override;

		bool is_returned() const { return returned; }
		Ref<SphereSnapshot> get_snapshot() const { return returned ? Ref<SphereSnapshot>() : snapshot; }
		const GDExtensionSceneOpaque32V1 &get_sample_token() const { return expected_sample; }
	};

private:
	mutable Mutex snapshot_mutex;
	RenderingDevice *device = nullptr;
	Ref<LiveDeviceGuard> live_device_guard;
	GDExtensionSceneOpaque32V1 device_capability = {};
	uint64_t next_snapshot_generation = 1;
	uint64_t scene_epoch = 1;
	Ref<SphereSnapshot> current_sphere_snapshot;
	SafeNumeric<uint32_t> active_lease_counter{ 0 };
	SafeNumeric<uint32_t> live_rid_counter{ 0 };

public:
	ParticlesCollisionSceneRD() = default;
	~ParticlesCollisionSceneRD();

	void initialize(RenderingDevice *p_device, const GDExtensionSceneOpaque32V1 &p_device_capability);
	void shutdown();

	Error update_sphere_colliders(const LocalVector<SceneSphereRecord32> &p_colliders, const Vector3 &p_bounds_min, const Vector3 &p_bounds_max);
	Error acquire_sphere_read_lease(Ref<SphereReadLease> &r_lease, GDExtensionSceneOpaque32V1 &r_sample_token);

	uint32_t get_active_lease_count() const { return active_lease_counter.get(); }
	uint32_t get_live_rid_count() const { return live_rid_counter.get(); }
	uint64_t get_current_snapshot_generation() const;

	// Deterministic reference/GPU-parity kernels for K06 collision & SDF sources:
	static CollisionContactResult evaluate_sphere_collision(
			const LocalVector<SceneSphereRecord32> &p_spheres,
			const Vector3 &p_position,
			const Vector3 &p_velocity,
			float p_particle_radius,
			float p_dt);

	static CollisionContactResult evaluate_heightfield_d32_collision(
			const SceneHeightMeta352 &p_meta,
			const LocalVector<float> &p_raw_d32_texels,
			const Transform3D &p_height_to_world,
			const Vector3 &p_position,
			const Vector3 &p_velocity,
			float p_particle_radius,
			float p_dt);

	static CollisionContactResult evaluate_current_depth_collision(
			const SceneCurrentDepthMeta256 &p_meta,
			const LocalVector<float> &p_raw_d32_texels,
			const Transform3D &p_view_to_world,
			const Vector3 &p_position,
			const Vector3 &p_velocity,
			float p_particle_radius,
			float p_dt);

	static CollisionContactResult evaluate_sdf_3d_collision(
			const SceneSdfMeta256 &p_meta,
			const LocalVector<float> &p_sdf_voxels,
			const Transform3D &p_world_to_grid_box,
			const Vector3 &p_position,
			const Vector3 &p_velocity,
			float p_particle_radius,
			float p_dt);
};

} // namespace RendererRD
