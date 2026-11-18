/**************************************************************************/
/*  test_particle_scene_collision_rd.h                                    */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_scene_late_depth.h"
#include "core/extension/gdextension_scene_phase.h"
#include "core/extension/gdextension_scene_phase_bind.h"
#include "core/math/vector4.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

#include "core/io/marshalls.h"
#include "core/os/thread.h"

#include <limits>

namespace TestParticleSceneK06 {

static GDExtensionSceneOpaque32V1 make_opaque32(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
	GDExtensionSceneOpaque32V1 out = {};
	encode_uint64(a, out.bytes + 0);
	encode_uint64(b, out.bytes + 8);
	encode_uint64(c, out.bytes + 16);
	encode_uint64(d, out.bytes + 24);
	return out;
}

static GDExtensionSceneOperationKeyV1 make_valid_scene_key() {
	GDExtensionSceneOperationKeyV1 key = {};
	key.abi.abi_version = 1;
	key.abi.struct_size = sizeof(GDExtensionSceneOperationKeyV1);
	key.abi.reserved = 0;
	key.owner_generation = 7;
	key.transport_operation.abi.abi_version = 1;
	key.transport_operation.abi.struct_size = sizeof(GDExtensionProductionOperationV1);
	key.transport_operation.abi.reserved = 0;
	key.transport_operation.owner_request_nonce = 11;
	key.transport_operation.record_generation = 22;
	key.transport_operation.canonical_begin = 33;
	key.transport_operation.canonical_end = 44;
	key.record_request = make_opaque32(101, 102, 103, 104);
	key.operation_ref = make_opaque32(201, 202, 203, 204);
	key.cursor_ref = make_opaque32(301, 302, 303, 304);
	key.schedule_digest = make_opaque32(401, 402, 403, 404);
	return key;
}

TEST_CASE("[particle_scene_k06] P01 GAP-1/GAP-2 scene_phase_bind_operation 200B key & phase 1/2 config") {
	CHECK(sizeof(GDExtensionSceneOperationKeyV1) == 200);

	GDExtensionScenePhaseSinkConfigV1 cfg = {};
	cfg.abi.abi_version = 1;
	cfg.abi.struct_size = sizeof(GDExtensionScenePhaseSinkConfigV1);
	cfg.abi.reserved = 0;
	cfg.device_capability = make_opaque32(1, 2, 3, 4);
	cfg.max_operations = 8;
	cfg.phase = GDExtensionDeviceExit::SCENE_PHASE_BEFORE_PARTICLES;
	CHECK(GDExtensionDeviceExit::scene_phase_config_valid(cfg));
	cfg.phase = GDExtensionDeviceExit::SCENE_PHASE_LATE_TRANSPARENT_DEPTH;
	CHECK(GDExtensionDeviceExit::scene_phase_config_valid(cfg));
	cfg.phase = 0;
	CHECK_FALSE(GDExtensionDeviceExit::scene_phase_config_valid(cfg));
	cfg.phase = 3;
	CHECK_FALSE(GDExtensionDeviceExit::scene_phase_config_valid(cfg));

	const GDExtensionSceneOperationKeyV1 valid_key = make_valid_scene_key();
	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_envelope(valid_key) == OK);

	GDExtensionSceneOperationKeyV1 bad_key = valid_key;
	bad_key.abi.reserved = 1;
	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_envelope(bad_key) == ERR_INVALID_PARAMETER);

	bad_key = valid_key;
	bad_key.schedule_digest = make_opaque32(0, 0, 0, 0);
	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_envelope(bad_key) == ERR_INVALID_PARAMETER);

	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_authority(
				  valid_key, valid_key.transport_operation, 7, 7, true, false, false, false, false, true) == OK);
	// Library generation mismatch or closed/frozen request must fail closed:
	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_authority(
				  valid_key, valid_key.transport_operation, 8, 7, true, false, false, false, false, true) == ERR_UNAUTHORIZED);
	CHECK(GDExtensionDeviceExit::validate_scene_operation_key_authority(
				  valid_key, valid_key.transport_operation, 7, 7, true, false, true, false, false, true) == ERR_BUSY);

	GDExtensionSceneOperationKeyV1 slot = {};
	bool bound = false;
	CHECK(GDExtensionDeviceExit::commit_scene_operation_key_slot(slot, bound, valid_key, true) == OK);
	CHECK(bound);
	// Idempotent re-bind with identical key succeeds:
	CHECK(GDExtensionDeviceExit::commit_scene_operation_key_slot(slot, bound, valid_key, true) == OK);
	// Conflicting re-bind on same slot is rejected:
	GDExtensionSceneOperationKeyV1 conflicting = valid_key;
	conflicting.schedule_digest.bytes[0] ^= 1u;
	CHECK(GDExtensionDeviceExit::commit_scene_operation_key_slot(slot, bound, conflicting, true) == ERR_INVALID_DATA);
}

TEST_CASE("[particle_scene_k06] LateTransparentDepthContent lease lifecycle & token validation") {
	SafeNumeric<uint32_t> active_leases{ 0 };
	const GDExtensionSceneOpaque32V1 dev_tok = make_opaque32(11, 22, 33, 44);
	const GDExtensionSceneOpaque32V1 sample_tok = GDExtensionDeviceExit::make_late_depth_sample_token(5, 120, 1920, 1080);

	GDExtensionDeviceExit::LateTransparentDepthDescriptor desc;
	desc.depth_texture = RID::from_uint64(777);
	desc.width = 1920;
	desc.height = 1080;
	desc.view_count = 1;
	desc.reverse_z = true;
	desc.frame_number = 120;

	{
		Ref<GDExtensionDeviceExit::LateTransparentDepthContent> lease;
		lease.instantiate(dev_tok, sample_tok, desc, &active_leases);
		CHECK(active_leases.get() == 1);
		CHECK(lease->validate(dev_tok, sample_tok) == OK);

		const GDExtensionSceneOpaque32V1 wrong_sample = GDExtensionDeviceExit::make_late_depth_sample_token(5, 121, 1920, 1080);
		CHECK(lease->validate(dev_tok, wrong_sample) == ERR_UNAUTHORIZED);

		CHECK(lease->return_after_reads() == OK);
		CHECK(lease->is_returned());
		CHECK(active_leases.get() == 0);
		// Post-return validation fails closed with ERR_UNAVAILABLE; duplicate return is idempotent without counter underflow:
		CHECK(lease->validate(dev_tok, sample_tok) == ERR_UNAVAILABLE);
		CHECK(lease->return_after_reads() == OK);
		CHECK(active_leases.get() == 0);
	}
	CHECK(active_leases.get() == 0);
}

TEST_CASE("[particle_scene_k06] K06 COW SphereSnapshot + SphereReadLease on live Metal RD with zero leaked RIDs") {
	CHECK(sizeof(RendererRD::SceneSphereRecord32) == 32);
	CHECK(sizeof(RendererRD::SceneSphereMeta208) == 208);
	CHECK(sizeof(RendererRD::SceneHeightMeta352) == 352);
	CHECK(sizeof(RendererRD::SceneDepthSnapshotMeta256) == 256);
	CHECK(sizeof(RendererRD::SceneSdfMeta256) == 256);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	RendererRD::ParticlesCollisionSceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b3036, 1, 2, 3);
	scene_collision.initialize(&rd, dev_cap);

	LocalVector<RendererRD::SceneSphereRecord32> gen1;
	RendererRD::SceneSphereRecord32 s0;
	s0.center_x = 1.0f;
	s0.radius = 2.0f;
	s0.set_collider_key(1001);
	gen1.push_back(s0);

	REQUIRE(scene_collision.update_sphere_colliders(gen1, Vector3(-5, -5, -5), Vector3(5, 5, 5)) == OK);
	CHECK(scene_collision.get_current_snapshot_generation() == 1);
	CHECK(scene_collision.get_live_rid_count() == 2); // meta_buffer + records_buffer

	Ref<RendererRD::ParticlesCollisionSceneRD::SphereReadLease> lease1;
	GDExtensionSceneOpaque32V1 sample1 = {};
	REQUIRE(scene_collision.acquire_sphere_read_lease(lease1, sample1) == OK);
	CHECK(scene_collision.get_active_lease_count() == 1);
	CHECK(lease1->validate(dev_cap, sample1) == OK);

	// Mutate colliders on main thread while GPU read lease1 is still active:
	// COW must allocate new RD buffers for generation 2 while keeping generation 1 alive!
	LocalVector<RendererRD::SceneSphereRecord32> gen2(gen1);
	RendererRD::SceneSphereRecord32 s1;
	s1.center_y = 3.0f;
	s1.radius = 1.5f;
	s1.set_collider_key(1002);
	gen2.push_back(s1);
	REQUIRE(scene_collision.update_sphere_colliders(gen2, Vector3(-8, -8, -8), Vector3(8, 8, 8)) == OK);
	CHECK(scene_collision.get_current_snapshot_generation() == 2);
	CHECK(scene_collision.get_live_rid_count() == 4); // Both gen1 and gen2 RD buffers alive
	CHECK(lease1->get_snapshot()->meta.count == 1);

	// Returning lease1 drops the last reference to gen1, freeing gen1's 2 RD buffers immediately:
	CHECK(lease1->return_after_reads() == OK);
	lease1.unref();
	CHECK(scene_collision.get_active_lease_count() == 0);
	CHECK(scene_collision.get_live_rid_count() == 2);

	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0);
	CHECK(scene_collision.get_active_lease_count() == 0);
#endif
}

TEST_CASE("[particle_scene_k06] E17/E18 Sphere tie-break, D32 Reverse-Z 0.0 empty sentinel, CurrentDepth & 3D SDF") {
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;

	// 1. E17 Sphere collision: identical penetration tie-broken deterministically by lowest u64 collider_key.
	LocalVector<RendererRD::SceneSphereRecord32> spheres_fwd;
	RendererRD::SceneSphereRecord32 a;
	a.center_x = 0.0f;
	a.center_y = 0.0f;
	a.center_z = 0.0f;
	a.radius = 1.0f;
	a.restitution = 0.5f;
	a.friction = 0.1f;
	a.set_collider_key(0x200000001ull);

	RendererRD::SceneSphereRecord32 b = a;
	b.set_collider_key(0x100000009ull);

	spheres_fwd.push_back(a);
	spheres_fwd.push_back(b);

	LocalVector<RendererRD::SceneSphereRecord32> spheres_rev;
	spheres_rev.push_back(b);
	spheres_rev.push_back(a);

	const RendererRD::CollisionContactResult hit_fwd = SceneRD::evaluate_sphere_collision(
			spheres_fwd, Vector3(0.0f, 0.8f, 0.0f), Vector3(0.0f, -2.0f, 0.0f), 0.1f, 0.016f);
	const RendererRD::CollisionContactResult hit_rev = SceneRD::evaluate_sphere_collision(
			spheres_rev, Vector3(0.0f, 0.8f, 0.0f), Vector3(0.0f, -2.0f, 0.0f), 0.1f, 0.016f);
	REQUIRE(hit_fwd.collided);
	REQUIRE(hit_rev.collided);
	CHECK(hit_fwd.winning_collider_key == 0x100000009ull);
	CHECK(hit_rev.winning_collider_key == 0x100000009ull);
	CHECK(hit_fwd.post_velocity.is_equal_approx(hit_rev.post_velocity));
	CHECK(hit_fwd.post_velocity.y > 0.0f);

	// 2. E18 D32_SFLOAT Reverse-Z heightfield: raw == 0.0f is empty sky/unrendered (no phantom z_far hit).
	RendererRD::SceneHeightMeta352 hmeta;
	hmeta.width = 4;
	hmeta.height = 4;
	hmeta.z_near = 1.0f;
	hmeta.z_far = 101.0f;
	hmeta.reverse_z = 1;
	hmeta.restitution = 0.5f;
	hmeta.friction = 0.1f;
	hmeta.bounds_min[0] = -10.0f;
	hmeta.bounds_min[1] = -10.0f;
	hmeta.bounds_min[2] = -10.0f;
	hmeta.bounds_max[0] = 10.0f;
	hmeta.bounds_max[1] = 10.0f;
	hmeta.bounds_max[2] = 10.0f;
	const Transform3D height_xform(Basis().scaled(Vector3(2.0f, 1.0f, 2.0f)), Vector3(0.0f, 10.0f, 0.0f));

	LocalVector<float> empty_texels;
	empty_texels.resize(16);
	for (uint32_t i = 0; i < 16; i++) {
		empty_texels[i] = 0.0f;
	}
	const RendererRD::CollisionContactResult empty_res = SceneRD::evaluate_heightfield_d32_collision(
			hmeta, empty_texels, height_xform, Vector3(0.0f, -90.0f, 0.0f), Vector3(0.0f, -5.0f, 0.0f), 0.2f, 0.016f);
	CHECK_FALSE(empty_res.collided);
	CHECK(empty_res.empty_depth);

	// Populated Reverse-Z floor at local_y = -10 (distance = 10 -> raw = (101 - 10) / 100 = 0.91f -> world_y = 0):
	LocalVector<float> floor_texels;
	floor_texels.resize(16);
	for (uint32_t i = 0; i < 16; i++) {
		floor_texels[i] = 0.91f;
	}
	const RendererRD::CollisionContactResult floor_res = SceneRD::evaluate_heightfield_d32_collision(
			hmeta, floor_texels, height_xform, Vector3(0.0f, 0.1f, 0.0f), Vector3(0.0f, -4.0f, 0.0f), 0.25f, 0.016f);
	REQUIRE(floor_res.collided);
	CHECK_FALSE(floor_res.empty_depth);
	CHECK(floor_res.world_normal.is_equal_approx(Vector3(0.0f, 1.0f, 0.0f)));
	CHECK(floor_res.post_velocity.y > 0.0f);

	// 3. SceneDepthSnapshotMeta256 (v2) collision: silhouette discontinuity falls back to eye-facing normal.
	RendererRD::SceneDepthSnapshotMeta256 dmeta;
	dmeta.width = 4;
	dmeta.height = 4;
	dmeta.snapshot_generation = 1;
	dmeta.slab_thickness_m = 1.0f;
	dmeta.silhouette_jump_m = 0.25f;
	Projection dc;
	dc.set_depth_correction(true);
	Projection proj;
	proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
	const Projection rd_proj = dc * proj;
	SceneRD::fill_depth_snapshot_matrices(Transform3D(), rd_proj, 0.1f, 100.0f, dmeta);
	LocalVector<float> depth_texels;
	depth_texels.resize(16);
	const Vector4 c5 = rd_proj.xform(Vector4(0.0f, 0.0f, -5.0f, 1.0f));
	const float raw_5m = float(c5.z / c5.w);
	const Vector4 c50 = rd_proj.xform(Vector4(0.0f, 0.0f, -50.0f, 1.0f));
	const float raw_50m = float(c50.z / c50.w);
	for (uint32_t i = 0; i < 16; i++) {
		depth_texels[i] = raw_5m;
	}
	// Inject sharp silhouette jump on right neighbor:
	depth_texels[2 * 4 + 3] = raw_50m;
	const RendererRD::CollisionContactResult depth_res = SceneRD::evaluate_depth_snapshot_v2_collision(
			dmeta, depth_texels, Vector3(0.0f, 0.0f, -5.1f), Vector3(0.0f, 0.0f, -3.0f), 0.25f, 0.016f, 0.5f, 0.0f);
	REQUIRE(depth_res.collided);
	CHECK(depth_res.silhouette_fallback_used);
	CHECK(depth_res.world_normal.z > 0.99f);

	// 4. 3D SDF collision: spherical signed distance field in [-4, 4]^3 around origin with radius 2.0.
	RendererRD::SceneSdfMeta256 smeta;
	smeta.dim_x = 16;
	smeta.dim_y = 16;
	smeta.dim_z = 16;
	smeta.bounds_min[0] = -4.0f;
	smeta.bounds_min[1] = -4.0f;
	smeta.bounds_min[2] = -4.0f;
	smeta.bounds_max[0] = 4.0f;
	smeta.bounds_max[1] = 4.0f;
	smeta.bounds_max[2] = 4.0f;
	smeta.restitution = 0.5f;
	smeta.friction = 0.1f;

	LocalVector<float> sdf_voxels;
	sdf_voxels.resize(16 * 16 * 16);
	for (uint32_t z = 0; z < 16; z++) {
		for (uint32_t y = 0; y < 16; y++) {
			for (uint32_t x = 0; x < 16; x++) {
				const Vector3 p(
						-4.0f + 8.0f * (float(x) / 15.0f),
						-4.0f + 8.0f * (float(y) / 15.0f),
						-4.0f + 8.0f * (float(z) / 15.0f));
				sdf_voxels[(z * 16 + y) * 16 + x] = p.length() - 2.0f;
			}
		}
	}

	const RendererRD::CollisionContactResult sdf_hit = SceneRD::evaluate_sdf_3d_collision(
			smeta, sdf_voxels, Transform3D(), Vector3(1.9f, 0.0f, 0.0f), Vector3(-2.0f, 0.0f, 0.0f), 0.25f, 0.016f);
	REQUIRE(sdf_hit.collided);
	CHECK(sdf_hit.world_normal.x > 0.95f);
	CHECK(sdf_hit.post_velocity.x > 0.0f);

	// Out-of-bounds query is a graceful no-op:
	const RendererRD::CollisionContactResult sdf_oob = SceneRD::evaluate_sdf_3d_collision(
			smeta, sdf_voxels, Transform3D(), Vector3(25.0f, 0.0f, 0.0f), Vector3(-2.0f, 0.0f, 0.0f), 0.25f, 0.016f);
	CHECK_FALSE(sdf_oob.collided);
}

TEST_CASE("[particle_scene_k06] D3-P0-2/D3-P1-1/D3-P1-4c LateDepth descriptor, asymmetric principal point & ortho CurrentDepth, off-thread SphereReadLease safety") {
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;

	// 1. D3-P1-4c: Asymmetric perspective principal point & orthographic SceneDepthSnapshotMeta256 (v2) reconstruction:
	{
		Projection dc;
		dc.set_depth_correction(true);

		RendererRD::SceneDepthSnapshotMeta256 dmeta;
		dmeta.width = 8;
		dmeta.height = 8;
		dmeta.snapshot_generation = 1;
		dmeta.slab_thickness_m = 1.0f;
		dmeta.silhouette_jump_m = 2.0f;
		Projection asym_proj;
		asym_proj.set_frustum(-0.05f, 0.15f, -0.05f, 0.15f, 0.1f, 100.0f);
		const Projection rd_asym = dc * asym_proj;
		SceneRD::fill_depth_snapshot_matrices(Transform3D(), rd_asym, 0.1f, 100.0f, dmeta);
		CHECK(Math::abs(dmeta.world_to_clip[8]) > 0.2f);
		CHECK(Math::abs(dmeta.world_to_clip[9]) > 0.2f);

		const Vector4 asym_c5 = rd_asym.xform(Vector4(2.5f, 2.5f, -5.0f, 1.0f));
		const float asym_raw5 = float(asym_c5.z / asym_c5.w);
		LocalVector<float> depth_texels;
		depth_texels.resize(64);
		for (uint32_t i = 0; i < 64; i++) {
			depth_texels[i] = asym_raw5;
		}
		const RendererRD::CollisionContactResult asym_hit = SceneRD::evaluate_depth_snapshot_v2_collision(
				dmeta, depth_texels, Vector3(2.5f, 2.5f, -5.1f), Vector3(0.0f, 0.0f, -4.0f), 0.25f, 0.016f, 0.5f, 0.0f);
		REQUIRE(asym_hit.collided);
		CHECK_FALSE(asym_hit.silhouette_fallback_used);
		CHECK(asym_hit.world_normal.is_equal_approx(Vector3(0.0f, 0.0f, 1.0f)));
		CHECK(asym_hit.post_velocity.z > 0.0f);

		// Orthographic projection SceneDepthSnapshotMeta256 v2 test (z_near = 0.1 > 0):
		RendererRD::SceneDepthSnapshotMeta256 ortho_meta = dmeta;
		Projection ortho_proj;
		ortho_proj.set_orthogonal(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.1f);
		const Projection rd_ortho = dc * ortho_proj;
		SceneRD::fill_depth_snapshot_matrices(Transform3D(), rd_ortho, 0.1f, 100.1f, ortho_meta);
		const Vector4 ortho_c5 = rd_ortho.xform(Vector4(0.0f, 0.0f, -5.0f, 1.0f));
		const float ortho_raw5 = float(ortho_c5.z / ortho_c5.w);
		LocalVector<float> ortho_texels;
		ortho_texels.resize(64);
		for (uint32_t i = 0; i < 64; i++) {
			ortho_texels[i] = ortho_raw5;
		}
		const RendererRD::CollisionContactResult ortho_hit = SceneRD::evaluate_depth_snapshot_v2_collision(
				ortho_meta, ortho_texels, Vector3(3.0f, -2.0f, -5.1f), Vector3(0.0f, 0.0f, -3.0f), 0.25f, 0.016f, 0.5f, 0.0f);
		REQUIRE(ortho_hit.collided);
		CHECK_FALSE(ortho_hit.silhouette_fallback_used);
		CHECK(ortho_hit.world_normal.is_equal_approx(Vector3(0.0f, 0.0f, 1.0f)));
	}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// 2. D3-P0-2 & D3-P1-1: Real LateTransparentDepthDescriptor + LateTransparentDepthContent validation + off-thread SphereReadLease safety:
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	tf.width = 16;
	tf.height = 16;
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	const RID depth_tex = rd.texture_create(tf, RD::TextureView());
	REQUIRE(depth_tex.is_valid());

	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b3036, 9, 8, 7);
	const GDExtensionSceneOpaque32V1 smp_cap = make_opaque32(0x534d50, 1, 2, 3);

	// Null depth_texture must fail LateTransparentDepthContent::validate:
	GDExtensionDeviceExit::LateTransparentDepthDescriptor null_desc;
	null_desc.width = 16;
	null_desc.height = 16;
	Ref<GDExtensionDeviceExit::LateTransparentDepthContent> bad_content;
	bad_content.instantiate(dev_cap, smp_cap, null_desc);
	CHECK(bad_content->validate(dev_cap, smp_cap) == ERR_INVALID_DATA);

	// Populated descriptor with real depth_texture RID passes validate and publishes through dispatch_late_transparent_depth_phase:
	GDExtensionDeviceExit::LateTransparentDepthDescriptor real_desc;
	real_desc.viewport = RID::from_uint64(0x1001ull);
	real_desc.depth_texture = depth_tex;
	real_desc.width = 16;
	real_desc.height = 16;
	real_desc.view_count = 1;
	real_desc.reverse_z = true;
	real_desc.z_near = 0.05f;
	real_desc.z_far = 500.0f;
	real_desc.frame_number = 77;
	Ref<GDExtensionDeviceExit::LateTransparentDepthContent> ok_content;
	ok_content.instantiate(dev_cap, smp_cap, real_desc);
	CHECK(ok_content->validate(dev_cap, smp_cap) == OK);

	CHECK_FALSE(GDExtensionDeviceExit::has_active_late_transparent_depth_phase(&rd, real_desc.viewport));
	GDExtensionDeviceExit::dispatch_late_transparent_depth_phase(rd, real_desc);
	GDExtensionDeviceExit::LateTransparentDepthDescriptor published;
	REQUIRE(GDExtensionDeviceExit::get_last_published_late_transparent_depth_descriptor(real_desc.viewport, published));
	CHECK(published.depth_texture == depth_tex);
	CHECK(published.viewport == real_desc.viewport);
	CHECK(published.width == 16u);
	CHECK(published.height == 16u);
	CHECK(published.frame_number == 77ull);

	// D3-P1-1: Hold a SphereReadLease past shutdown(), then drop the last reference on a worker thread:
	SceneRD scene_collision;
	scene_collision.initialize(&rd, dev_cap);
	LocalVector<RendererRD::SceneSphereRecord32> spheres;
	RendererRD::SceneSphereRecord32 s0;
	s0.radius = 1.0f;
	s0.set_collider_key(777);
	spheres.push_back(s0);
	REQUIRE(scene_collision.update_sphere_colliders(spheres, Vector3(-2, -2, -2), Vector3(2, 2, 2)) == OK);

	Ref<SceneRD::SphereReadLease> held_lease;
	GDExtensionSceneOpaque32V1 lease_sample = {};
	REQUIRE(scene_collision.acquire_sphere_read_lease(held_lease, lease_sample) == OK);
	CHECK(scene_collision.get_live_rid_count() == 2);

	// Shutdown owner on render thread while held_lease is still retained:
	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0);

	struct WorkerDropCtx {
		Ref<SceneRD::SphereReadLease> lease;
	} drop_ctx{ held_lease };
	held_lease.unref();

	Thread worker;
	worker.start([](void *p_userdata) {
		WorkerDropCtx *ctx = static_cast<WorkerDropCtx *>(p_userdata);
		ctx->lease->return_after_reads();
		ctx->lease.unref();
	}, &drop_ctx);
	worker.wait_to_finish();

	rd.free_rid(depth_tex);
#endif
}

TEST_CASE("[particle_scene_k06] SceneDepthSnapshotMeta256 v2 layout and Projection::inverse frustum guards") {
	using Meta = RendererRD::SceneDepthSnapshotMeta256;
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;

	CHECK(sizeof(Meta) == 256);
	CHECK(alignof(Meta) == 16);
	CHECK(offsetof(Meta, magic) == 0);
	CHECK(offsetof(Meta, version) == 4);
	CHECK(offsetof(Meta, width) == 8);
	CHECK(offsetof(Meta, height) == 12);
	CHECK(offsetof(Meta, snapshot_generation) == 16);
	CHECK(offsetof(Meta, view_id) == 24);
	CHECK(offsetof(Meta, world_to_clip) == 32);
	CHECK(offsetof(Meta, clip_to_world) == 96);
	CHECK(offsetof(Meta, view_to_world) == 160);
	CHECK(offsetof(Meta, z_near) == 224);
	CHECK(offsetof(Meta, slab_thickness_m) == 228);
	CHECK(offsetof(Meta, silhouette_jump_m) == 232);
	CHECK(offsetof(Meta, flags) == 236);
	CHECK(offsetof(Meta, reserved) == 240);

	Meta defaults;
	CHECK(defaults.magic == 0x4b303644u);
	CHECK(defaults.version == 2u);
	CHECK(defaults.slab_thickness_m == doctest::Approx(0.50f));
	CHECK(defaults.silhouette_jump_m == doctest::Approx(0.50f));
	CHECK(defaults.flags == Meta::FLAG_REVERSE_Z);
	for (int i = 0; i < 4; ++i) {
		CHECK(defaults.reserved[i] == 0u);
	}

	// Perspective roundtrip with rotated/translated camera using full projective Projection::inverse():
	Transform3D cam_xf(Basis(Vector3(0.0f, 1.0f, 0.0f), 0.35f) * Basis(Vector3(1.0f, 0.0f, 0.0f), -0.20f), Vector3(3.5f, 2.0f, 8.0f));
	Projection dc;
	dc.set_depth_correction(true);
	Projection persp;
	persp.set_perspective(75.0f, 16.0f / 9.0f, 0.1f, 500.0f, false);
	const Projection rd_persp = dc * persp;

	Meta m_persp;
	SceneRD::fill_depth_snapshot_matrices(cam_xf, rd_persp, 0.1f, 500.0f, m_persp);
	CHECK(m_persp.flags == (Meta::FLAG_PRESENT | Meta::FLAG_REVERSE_Z));
	CHECK(m_persp.view_to_world[12] == doctest::Approx(cam_xf.origin.x).epsilon(1e-5));
	CHECK(m_persp.view_to_world[13] == doctest::Approx(cam_xf.origin.y).epsilon(1e-5));
	CHECK(m_persp.view_to_world[14] == doctest::Approx(cam_xf.origin.z).epsilon(1e-5));

	const Vector3 test_world = cam_xf.xform(Vector3(-1.25f, 0.75f, -9.5f));
	const Vector4 clip(
			m_persp.world_to_clip[0] * test_world.x + m_persp.world_to_clip[4] * test_world.y + m_persp.world_to_clip[8] * test_world.z + m_persp.world_to_clip[12],
			m_persp.world_to_clip[1] * test_world.x + m_persp.world_to_clip[5] * test_world.y + m_persp.world_to_clip[9] * test_world.z + m_persp.world_to_clip[13],
			m_persp.world_to_clip[2] * test_world.x + m_persp.world_to_clip[6] * test_world.y + m_persp.world_to_clip[10] * test_world.z + m_persp.world_to_clip[14],
			m_persp.world_to_clip[3] * test_world.x + m_persp.world_to_clip[7] * test_world.y + m_persp.world_to_clip[11] * test_world.z + m_persp.world_to_clip[15]);
	REQUIRE(clip.w > 1e-4f);
	const Vector3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
	const Vector4 unproj(
			m_persp.clip_to_world[0] * ndc.x + m_persp.clip_to_world[4] * ndc.y + m_persp.clip_to_world[8] * ndc.z + m_persp.clip_to_world[12],
			m_persp.clip_to_world[1] * ndc.x + m_persp.clip_to_world[5] * ndc.y + m_persp.clip_to_world[9] * ndc.z + m_persp.clip_to_world[13],
			m_persp.clip_to_world[2] * ndc.x + m_persp.clip_to_world[6] * ndc.y + m_persp.clip_to_world[9 + 1] * ndc.z + m_persp.clip_to_world[14],
			m_persp.clip_to_world[3] * ndc.x + m_persp.clip_to_world[7] * ndc.y + m_persp.clip_to_world[11] * ndc.z + m_persp.clip_to_world[15]);
	REQUIRE(Math::abs(unproj.w) > 1e-6f);
	const Vector3 roundtrip(unproj.x / unproj.w, unproj.y / unproj.w, unproj.z / unproj.w);
	CHECK(roundtrip.distance_to(test_world) < 1e-4f);

	// Orthographic camera with z_near = -10.0f MUST clear FLAG_PRESENT (flags == 2u):
	Projection ortho_neg;
	ortho_neg.set_orthogonal(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 100.0f);
	Meta m_ortho_neg = m_persp;
	SceneRD::fill_depth_snapshot_matrices(cam_xf, dc * ortho_neg, -10.0f, 100.0f, m_ortho_neg);
	CHECK(m_ortho_neg.flags == Meta::FLAG_REVERSE_Z);
	CHECK((m_ortho_neg.flags & Meta::FLAG_PRESENT) == 0u);

	// z_near == 0.0f, z_far <= z_near, and NaN guards:
	Meta m_zero = m_persp;
	SceneRD::fill_depth_snapshot_matrices(cam_xf, rd_persp, 0.0f, 100.0f, m_zero);
	CHECK(m_zero.flags == Meta::FLAG_REVERSE_Z);

	Meta m_inv_far = m_persp;
	SceneRD::fill_depth_snapshot_matrices(cam_xf, rd_persp, 10.0f, 10.0f, m_inv_far);
	CHECK(m_inv_far.flags == Meta::FLAG_REVERSE_Z);

	Projection nan_proj = rd_persp;
	nan_proj.columns[0][0] = std::numeric_limits<real_t>::quiet_NaN();
	Meta m_nan = m_persp;
	SceneRD::fill_depth_snapshot_matrices(cam_xf, nan_proj, 0.1f, 100.0f, m_nan);
	CHECK(m_nan.flags == Meta::FLAG_REVERSE_Z);
}

TEST_CASE("[particle_scene_k06] Frame-0 unpopulated depth snapshot sentinel and samplerless Set-1 compatibility") {
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;
	using Meta = RendererRD::SceneDepthSnapshotMeta256;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 1, 2, 3);
	scene_collision.initialize(&rd, dev_cap);

	const RID vp_a = RID::from_uint64(0x9001ull);
	CHECK_FALSE(scene_collision.has_pending_depth_snapshot_request(vp_a, 0));

	Ref<SceneRD::DepthReadLease> lease0;
	GDExtensionSceneOpaque32V1 sample0 = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_a, lease0, sample0) == OK);
	REQUIRE(lease0.is_valid());
	CHECK(lease0->validate(dev_cap, sample0) == OK);
	CHECK(scene_collision.has_pending_depth_snapshot_request(vp_a, 0));
	CHECK(scene_collision.get_active_lease_count() == 1u);
	CHECK(scene_collision.get_live_rid_count() == 3u);

	Ref<SceneRD::DepthSnapshotGeneration> gen0 = lease0->get_generation();
	REQUIRE(gen0.is_valid());
	const RD::TextureFormat tf0 = rd.texture_get_format(gen0->depth_texture);
	CHECK(tf0.format == RD::DATA_FORMAT_R32_SFLOAT);
	CHECK(tf0.width == 1u);
	CHECK(tf0.height == 1u);

	const Vector<uint8_t> meta_bytes = rd.buffer_get_data(gen0->meta_buffer);
	REQUIRE(meta_bytes.size() == 256);
	Meta read_meta;
	memcpy(&read_meta, meta_bytes.ptr(), sizeof(Meta));
	CHECK(read_meta.magic == Meta::MAGIC);
	CHECK(read_meta.version == Meta::VERSION);
	CHECK(read_meta.width == 0u);
	CHECK(read_meta.height == 0u);
	CHECK(read_meta.snapshot_generation == 0ull);
	CHECK(read_meta.view_id == vp_a.get_id());
	CHECK(read_meta.flags == Meta::FLAG_REVERSE_Z);
	CHECK((read_meta.flags & Meta::FLAG_PRESENT) == 0u);

	// Verify Set-1 RD::UNIFORM_TYPE_TEXTURE + GL_EXT_samplerless_texture_functions texelFetch on Metal:
	static constexpr const char *kSamplerlessGlsl =
			"#version 450\n"
			"#extension GL_EXT_samplerless_texture_functions : enable\n"
			"layout(local_size_x = 1) in;\n"
			"layout(set = 0, binding = 0) uniform texture2D u_depth;\n"
			"layout(set = 0, binding = 1, std430) writeonly buffer Out { float d; } u_out;\n"
			"void main() { u_out.d = texelFetch(u_depth, ivec2(0, 0), 0).r; }\n";
	String shd_err;
	RD::ShaderStageSPIRVData cs_stage;
	cs_stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	cs_stage.spirv = rd.shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, String::utf8(kSamplerlessGlsl), RD::SHADER_LANGUAGE_GLSL, &shd_err, false);
	REQUIRE_MESSAGE(!cs_stage.spirv.is_empty(), shd_err.utf8().get_data());
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(cs_stage);
	const RID c_shd = rd.shader_create_from_spirv(stages);
	const RID c_pipe = rd.compute_pipeline_create(c_shd);
	const RID out_buf = rd.storage_buffer_create(16);
	REQUIRE(c_pipe.is_valid());
	REQUIRE(out_buf.is_valid());

	RD::Uniform u_tex(RD::UNIFORM_TYPE_TEXTURE, 0, gen0->depth_texture);
	RD::Uniform u_out(RD::UNIFORM_TYPE_STORAGE_BUFFER, 1, out_buf);
	Vector<RD::Uniform> uniforms;
	uniforms.push_back(u_tex);
	uniforms.push_back(u_out);
	const RID uset = rd.uniform_set_create(uniforms, c_shd, 0);
	REQUIRE(uset.is_valid());

	const RD::ComputeListID cl = rd.compute_list_begin();
	rd.compute_list_bind_compute_pipeline(cl, c_pipe);
	rd.compute_list_bind_uniform_set(cl, uset, 0);
	rd.compute_list_dispatch(cl, 1, 1, 1);
	rd.compute_list_end();

	const Vector<uint8_t> out_bytes = rd.buffer_get_data(out_buf);
	float sentinel_val = -1.0f;
	memcpy(&sentinel_val, out_bytes.ptr(), sizeof(float));
	CHECK(sentinel_val == 0.0f);

	rd.free_rid(uset);
	rd.free_rid(out_buf);
	rd.free_rid(c_pipe);
	rd.free_rid(c_shd);

	LocalVector<float> one_texel;
	one_texel.push_back(0.0f);
	const Vector3 p0(0.0f, 1.0f, -3.0f);
	const Vector3 v0(1.0f, -2.0f, -4.0f);
	const float dt = 0.016f;
	const RendererRD::CollisionContactResult col = SceneRD::evaluate_depth_snapshot_v2_collision(
			read_meta, one_texel, p0, v0, 0.2f, dt);
	CHECK_FALSE(col.collided);
	CHECK(col.post_position.is_equal_approx(p0 + v0 * dt));

	CHECK(lease0->return_after_reads() == OK);
	lease0.unref();
	CHECK(scene_collision.get_active_lease_count() == 0u);
	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0u);
#endif
}

TEST_CASE("[particle_scene_k06] RefCounted DepthSnapshotGeneration COW resize eviction and rollback") {
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 4, 5, 6);
	scene_collision.initialize(&rd, dev_cap);

	RD::TextureFormat src_tf;
	src_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	src_tf.width = 16;
	src_tf.height = 16;
	src_tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	Vector<uint8_t> src_init;
	src_init.resize(16 * 16 * sizeof(float));
	for (int i = 0; i < 256; ++i) {
		const float d = 0.02f;
		memcpy(src_init.ptrw() + i * 4, &d, 4);
	}
	Vector<Vector<uint8_t>> src_slices;
	src_slices.push_back(src_init);
	const RID src_tex = rd.texture_create(src_tf, RD::TextureView(), src_slices);
	REQUIRE(src_tex.is_valid());

	Projection dc;
	dc.set_depth_correction(true);
	Projection proj;
	proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
	const Projection rd_proj = dc * proj;

	const RID vp_a = RID::from_uint64(0xA001ull);
	REQUIRE(scene_collision.request_viewport_depth_snapshot(vp_a) == OK);
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, src_tex, 64, 64, Transform3D(), rd_proj, 0.1f, 100.0f, 1) == OK);
	CHECK(scene_collision.get_live_rid_count() == 3u);

	Ref<SceneRD::DepthReadLease> lease1;
	GDExtensionSceneOpaque32V1 sample1 = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_a, lease1, sample1) == OK);
	CHECK(scene_collision.get_active_lease_count() == 1u);
	CHECK(lease1->get_generation()->meta.width == 64u);

	// Resize to 128x128 while lease1 is held -> COW retires 64x64 into retired_depth_snapshots!
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, src_tex, 128, 128, Transform3D(), rd_proj, 0.1f, 100.0f, 2) == OK);
	CHECK(scene_collision.get_retired_depth_snapshot_count() == 1u);
	CHECK(scene_collision.get_live_rid_count() == 6u);
	CHECK(lease1->get_generation()->meta.width == 64u);

	Ref<SceneRD::DepthReadLease> lease2;
	GDExtensionSceneOpaque32V1 sample2 = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_a, lease2, sample2) == OK);
	CHECK(lease2->get_generation()->meta.width == 128u);

	// Force multiple retired_depth_snapshots vector growths while leases remain active (0 UAF!):
	LocalVector<Ref<SceneRD::DepthReadLease>> extra_leases;
	for (uint32_t step = 0; step < 6; ++step) {
		const uint32_t dim = 32u + step * 16u;
		REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, src_tex, dim, dim, Transform3D(), rd_proj, 0.1f, 100.0f, 3 + step) == OK);
		Ref<SceneRD::DepthReadLease> lx;
		GDExtensionSceneOpaque32V1 sx = {};
		REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_a, lx, sx) == OK);
		extra_leases.push_back(lx);
	}
	CHECK(scene_collision.get_retired_depth_snapshot_count() >= 6u);
	CHECK(lease1->validate(dev_cap, sample1) == OK);
	CHECK(lease2->validate(dev_cap, sample2) == OK);

	// Simulated framebuffer_create failure MUST transactionally roll back meta_buffer + depth_texture:
	const uint32_t rids_before_fail = scene_collision.get_live_rid_count();
	scene_collision.set_simulate_framebuffer_create_failure_once(true);
	CHECK(scene_collision.capture_viewport_depth_snapshot(vp_a, src_tex, 96, 96, Transform3D(), rd_proj, 0.1f, 100.0f, 20) == ERR_CANT_CREATE);
	CHECK(scene_collision.get_live_rid_count() == rids_before_fail);

	// Evict closed viewport while leases are still held:
	CHECK(scene_collision.has_viewport_depth_snapshot(vp_a));
	scene_collision.invalidate_viewport_depth_snapshot(vp_a);
	CHECK_FALSE(scene_collision.has_viewport_depth_snapshot(vp_a));
	CHECK(lease1->validate(dev_cap, sample1) == OK);
	CHECK(lease2->validate(dev_cap, sample2) == OK);

	// Returning all leases reaps retired_depth_snapshots and frees framebuffer_rid before depth_texture:
	CHECK(lease1->return_after_reads() == OK);
	CHECK(lease2->return_after_reads() == OK);
	for (uint32_t i = 0; i < extra_leases.size(); ++i) {
		CHECK(extra_leases[i]->return_after_reads() == OK);
	}
	extra_leases.clear();
	lease1.unref();
	lease2.unref();

	CHECK(scene_collision.get_fb_first_free_count() >= 8u);
	CHECK(scene_collision.get_retired_depth_snapshot_count() == 0u);
	CHECK(scene_collision.get_active_lease_count() == 0u);
	CHECK(scene_collision.get_live_rid_count() == 0u);

	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0u);
	rd.free_rid(src_tex);
#endif
}

TEST_CASE("[particle_scene_k06] multi-viewport isolation and multiview skip") {
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;
	using Meta = RendererRD::SceneDepthSnapshotMeta256;

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 7, 8, 9);
	scene_collision.initialize(&rd, dev_cap);

	Projection dc;
	dc.set_depth_correction(true);
	Projection proj;
	proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
	const Projection rd_proj = dc * proj;

	auto make_const_depth_tex = [&](float p_world_z) -> RID {
		const Vector4 clip = rd_proj.xform(Vector4(0.0f, 0.0f, p_world_z, 1.0f));
		const float raw_d = float(clip.z / clip.w);
		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;
		tf.width = 8;
		tf.height = 8;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		Vector<uint8_t> bytes;
		bytes.resize(8 * 8 * sizeof(float));
		for (int i = 0; i < 64; ++i) {
			memcpy(bytes.ptrw() + i * 4, &raw_d, 4);
		}
		Vector<Vector<uint8_t>> slices;
		slices.push_back(bytes);
		return rd.texture_create(tf, RD::TextureView(), slices);
	};

	const RID tex_4m = make_const_depth_tex(-4.0f);
	const RID tex_12m = make_const_depth_tex(-12.0f);
	const RID tex_99m = make_const_depth_tex(-99.0f);
	REQUIRE(tex_4m.is_valid());
	REQUIRE(tex_12m.is_valid());
	REQUIRE(tex_99m.is_valid());

	const RID vp_a = RID::from_uint64(0xB001ull);
	const RID vp_b = RID::from_uint64(0xB002ull);
	REQUIRE(scene_collision.request_viewport_depth_snapshot(vp_a) == OK);
	REQUIRE(scene_collision.request_viewport_depth_snapshot(vp_b) == OK);

	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, tex_4m, 8, 8, Transform3D(), rd_proj, 0.1f, 100.0f, 1) == OK);
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_b, tex_12m, 8, 8, Transform3D(), rd_proj, 0.1f, 100.0f, 1) == OK);

	// Multiview (view_count == 2) and reflection probe MUST skip without overwriting vp_a (-4m):
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, tex_99m, 8, 8, Transform3D(), rd_proj, 0.1f, 100.0f, 2, RID(), 2u, false) == OK);
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_a, tex_99m, 8, 8, Transform3D(), rd_proj, 0.1f, 100.0f, 2, RID(), 1u, true) == OK);

	Ref<SceneRD::DepthReadLease> lease_a;
	Ref<SceneRD::DepthReadLease> lease_b;
	GDExtensionSceneOpaque32V1 tok_a = {};
	GDExtensionSceneOpaque32V1 tok_b = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_a, lease_a, tok_a) == OK);
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_b, lease_b, tok_b) == OK);

	CHECK(lease_a->validate(dev_cap, tok_a) == OK);
	CHECK(lease_b->validate(dev_cap, tok_b) == OK);
	CHECK(lease_a->validate(dev_cap, tok_b) == ERR_UNAUTHORIZED);
	CHECK(lease_b->validate(dev_cap, tok_a) == ERR_UNAUTHORIZED);

	const Meta &meta_a = lease_a->get_generation()->meta;
	const Meta &meta_b = lease_b->get_generation()->meta;
	CHECK(meta_a.view_id == vp_a.get_id());
	CHECK(meta_b.view_id == vp_b.get_id());

	const Vector<uint8_t> bytes_a = rd.texture_get_data(lease_a->get_generation()->depth_texture, 0);
	const Vector<uint8_t> bytes_b = rd.texture_get_data(lease_b->get_generation()->depth_texture, 0);
	LocalVector<float> texels_a;
	LocalVector<float> texels_b;
	texels_a.resize(64);
	texels_b.resize(64);
	memcpy(texels_a.ptr(), bytes_a.ptr(), 64 * sizeof(float));
	memcpy(texels_b.ptr(), bytes_b.ptr(), 64 * sizeof(float));

	// Particle near -4.1m hits vp_a (-4m) but does NOT hit vp_b (-12m):
	const RendererRD::CollisionContactResult hit_a = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta_a, texels_a, Vector3(0.0f, 0.0f, -4.1f), Vector3(0.0f, 0.0f, -2.0f), 0.25f, 0.016f);
	const RendererRD::CollisionContactResult miss_b = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta_b, texels_b, Vector3(0.0f, 0.0f, -4.1f), Vector3(0.0f, 0.0f, -2.0f), 0.25f, 0.016f);
	CHECK(hit_a.collided);
	CHECK_FALSE(miss_b.collided);

	// Particle near -12.1m hits vp_b (-12m) but does NOT hit vp_a (-4m, slab = 0.5m):
	const RendererRD::CollisionContactResult hit_b = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta_b, texels_b, Vector3(0.0f, 0.0f, -12.1f), Vector3(0.0f, 0.0f, -2.0f), 0.25f, 0.016f);
	const RendererRD::CollisionContactResult pass_a = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta_a, texels_a, Vector3(0.0f, 0.0f, -12.1f), Vector3(0.0f, 0.0f, -2.0f), 0.25f, 0.016f);
	CHECK(hit_b.collided);
	CHECK_FALSE(pass_a.collided);

	CHECK(lease_a->return_after_reads() == OK);
	CHECK(lease_b->return_after_reads() == OK);
	lease_a.unref();
	lease_b.unref();
	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0u);

	rd.free_rid(tex_4m);
	rd.free_rid(tex_12m);
	rd.free_rid(tex_99m);
#endif
}

TEST_CASE("[particle_scene_k06][SceneTree] C ABI scene_depth_snapshot arm and borrow lifecycle") {
	const auto arm_fn = reinterpret_cast<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotArmV1>(
			GDExtension::get_interface_function("device_session_scene_depth_snapshot_arm_v1"));
	const auto borrow_fn = reinterpret_cast<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotBorrowV1>(
			GDExtension::get_interface_function("device_session_scene_depth_snapshot_borrow_v1"));
	REQUIRE(arm_fn != nullptr);
	REQUIRE(borrow_fn != nullptr);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	REQUIRE(GDExtensionDeviceExit::g_scene_depth_snapshot_c_abi_test_runner != nullptr);

	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice metal_rd;
	REQUIRE(metal_rd.initialize(&context) == OK);

	using SceneRD = RendererRD::ParticlesCollisionSceneRD;
	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 11, 22, 33);
	scene_collision.initialize(&metal_rd, dev_cap);

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	tf.width = 32;
	tf.height = 32;
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	Vector<uint8_t> init_bytes;
	init_bytes.resize(32 * 32 * sizeof(float));
	for (int i = 0; i < 1024; ++i) {
		const float d = 0.025f;
		memcpy(init_bytes.ptrw() + i * 4, &d, 4);
	}
	Vector<Vector<uint8_t>> slices;
	slices.push_back(init_bytes);
	const RID src_tex = metal_rd.texture_create(tf, RD::TextureView(), slices);
	REQUIRE(src_tex.is_valid());

	struct RdCleanupGuard {
		SceneRD *cs = nullptr;
		RenderingDevice *rd = nullptr;
		RID tex;
		~RdCleanupGuard() {
			if (cs) {
				cs->shutdown();
			}
			if (rd && tex.is_valid()) {
				rd->free_rid(tex);
			}
		}
	} cleanup_guard{ &scene_collision, &metal_rd, src_tex };

	const RID vp_a = RID::from_uint64(0xC001ull);
	struct LifecycleCtx {
		SceneRD *cs = nullptr;
		RID vp;
		RID src_tex;
		bool armed_observed = false;
		bool borrowed_observed = false;
		bool held_before_release = false;
		bool released_after_close = false;
	} ctx{ &scene_collision, vp_a, src_tex };

	GDExtensionDeviceExit::SceneDepthSnapshotCAbiTestCallbacks cbs;
	cbs.userdata = &ctx;
	cbs.on_armed = +[](void *p_ud) {
		auto *c = static_cast<LifecycleCtx *>(p_ud);
		c->armed_observed = c->cs->has_pending_depth_snapshot_request(c->vp, 0);
		Projection dc;
		dc.set_depth_correction(true);
		Projection proj;
		proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
		REQUIRE(c->cs->capture_viewport_depth_snapshot(c->vp, c->src_tex, 32, 32, Transform3D(), dc * proj, 0.1f, 100.0f, 1) == OK);
	};
	cbs.on_borrowed_in_phase = +[](void *p_ud, const GDExtensionSceneDepthSnapshotBorrowResultV1 &p_res) {
		auto *c = static_cast<LifecycleCtx *>(p_ud);
		CHECK(p_res.outcome.outcome == 1u);
		CHECK(p_res.meta_ssbo_rid != 0ull);
		CHECK(p_res.depth_texture_rid != 0ull);
		CHECK(p_res.snapshot_generation == 1ull);
		CHECK(p_res.width == 32u);
		CHECK(p_res.height == 32u);
		CHECK(p_res.flags == 3u);
		c->borrowed_observed = (c->cs->get_active_lease_count() == 1u);
	};
	cbs.on_before_release = +[](void *p_ud) {
		auto *c = static_cast<LifecycleCtx *>(p_ud);
		c->held_before_release = (c->cs->get_active_lease_count() == 1u);
	};
	cbs.on_after_release = +[](void *p_ud) {
		auto *c = static_cast<LifecycleCtx *>(p_ud);
		c->released_after_close = (c->cs->get_active_lease_count() == 0u);
	};

	GDExtensionDeviceExit::g_scene_depth_snapshot_c_abi_test_runner(&metal_rd, vp_a.get_id(), cbs);
	CHECK(ctx.armed_observed);
	CHECK(ctx.borrowed_observed);
	CHECK(ctx.held_before_release);
	CHECK(ctx.released_after_close);
	CHECK(scene_collision.get_active_lease_count() == 0u);

	scene_collision.shutdown();
	cleanup_guard.cs = nullptr;
	CHECK(scene_collision.get_live_rid_count() == 0u);
	metal_rd.free_rid(src_tex);
	cleanup_guard.tex = RID();
#endif
}

TEST_CASE("[particle_scene_k06] v2 3-tap collision point-sampled copy and D32_S8 slice") {
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;
	using Meta = RendererRD::SceneDepthSnapshotMeta256;

	Projection dc;
	dc.set_depth_correction(true);
	Projection proj;
	proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
	const Projection rd_proj = dc * proj;

	Meta meta;
	meta.width = 8;
	meta.height = 8;
	meta.snapshot_generation = 1;
	meta.slab_thickness_m = 0.50f;
	meta.silhouette_jump_m = 0.50f;
	SceneRD::fill_depth_snapshot_matrices(Transform3D(), rd_proj, 0.1f, 100.0f, meta);

	auto z_to_raw = [&](float p_world_z) -> float {
		const Vector4 c = rd_proj.xform(Vector4(0.0f, 0.0f, p_world_z, 1.0f));
		return float(c.z / c.w);
	};

	// 1. Reverse-Z sky (d_c = 0.0f) -> collided == false, empty_depth == true:
	LocalVector<float> sky_tex;
	sky_tex.resize(64);
	for (int i = 0; i < 64; ++i) {
		sky_tex[i] = 0.0f;
	}
	const RendererRD::CollisionContactResult sky_res = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta, sky_tex, Vector3(0.0f, 0.0f, -5.0f), Vector3(0.0f, 0.0f, -2.0f), 0.1f, 0.016f);
	CHECK_FALSE(sky_res.collided);
	CHECK(sky_res.empty_depth);

	// 2. Thin pillar at z = -5.0m: particle behind pillar at z = -6.2m (delta = 1.2m > slab = 0.5m) passes through!
	LocalVector<float> pillar_tex;
	pillar_tex.resize(64);
	const float raw_5m = z_to_raw(-5.0f);
	for (int i = 0; i < 64; ++i) {
		pillar_tex[i] = raw_5m;
	}
	const RendererRD::CollisionContactResult behind_pillar = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta, pillar_tex, Vector3(0.0f, 0.0f, -6.2f), Vector3(0.0f, 0.0f, -1.0f), 0.1f, 0.016f);
	CHECK_FALSE(behind_pillar.collided);

	// 3. Silhouette step (|z_c - z_r| = 3.0m > 0.5m) -> eye-facing normal fallback:
	LocalVector<float> sil_tex(pillar_tex);
	const float raw_8m = z_to_raw(-8.0f);
	for (int y = 0; y < 8; ++y) {
		for (int x = 5; x < 8; ++x) {
			sil_tex[y * 8 + x] = raw_8m;
		}
	}
	// Center pixel x=4 has right neighbor x=5 at -8.0m (|5.0 - 8.0| = 3.0m > 0.5m):
	const RendererRD::CollisionContactResult sil_res = SceneRD::evaluate_depth_snapshot_v2_collision(
			meta, sil_tex, Vector3(0.1f, 0.0f, -5.05f), Vector3(0.0f, 0.0f, -3.0f), 0.2f, 0.016f);
	REQUIRE(sil_res.collided);
	CHECK(sil_res.silhouette_fallback_used);
	CHECK(sil_res.world_normal.z > 0.98f);

	// 4. 45-degree ramp (y + z = -5.0m -> normal = normalize(0, 1, 1) = (0, 0.7071, 0.7071)):
	Meta ramp_meta = meta;
	ramp_meta.width = 64;
	ramp_meta.height = 64;
	ramp_meta.slab_thickness_m = 1.0f;
	ramp_meta.silhouette_jump_m = 1.0f;
	SceneRD::fill_depth_snapshot_matrices(Transform3D(), rd_proj, 0.1f, 100.0f, ramp_meta);
	LocalVector<float> ramp_tex;
	ramp_tex.resize(64 * 64);
	// Ray through pixel (sx, sy): x_view = ndc_x * (-z_view), y_view = -ndc_y * (-z_view) under 90deg flip_y.
	// Intersecting y_world + z_world = -5.0 => (-ndc_y * d) - d = -5.0 => d = 5.0 / (1.0 + ndc_y).
	for (int y = 0; y < 64; ++y) {
		const float sv = (float(y) + 0.5f) / 64.0f;
		const float ndc_y = sv * 2.0f - 1.0f;
		const float d = 5.0f / MAX(0.2f, 1.0f + ndc_y);
		const float raw = z_to_raw(-d);
		for (int x = 0; x < 64; ++x) {
			ramp_tex[y * 64 + x] = raw;
		}
	}
	const RendererRD::CollisionContactResult ramp_hit = SceneRD::evaluate_depth_snapshot_v2_collision(
			ramp_meta, ramp_tex, Vector3(0.0f, 0.0f, -5.02f), Vector3(0.0f, -4.0f, 0.0f), 0.15f, 0.016f, 0.8f, 0.0f);
	REQUIRE(ramp_hit.collided);
	CHECK_FALSE(ramp_hit.silhouette_fallback_used);
	CHECK(ramp_hit.world_normal.x == doctest::Approx(0.0f).epsilon(0.02f));
	CHECK(ramp_hit.world_normal.y == doctest::Approx(0.70710678f).epsilon(0.03f));
	CHECK(ramp_hit.world_normal.z == doctest::Approx(0.70710678f).epsilon(0.03f));
	CHECK(ramp_hit.post_velocity.z > 1.5f);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// 5. Point-sampled (FILTER_NEAREST) copy of sharp step edge [d_fg, d_bg] (2x2 -> 4x4 upscale)
	//    MUST preserve exact {d_fg, d_bg} with ZERO mid-depth texels (d_fg + d_bg) * 0.5!
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 44, 55, 66);
	scene_collision.initialize(&rd, dev_cap);

	const float d_fg = 0.80f;
	const float d_bg = 0.10f;
	const float step_2x2[4] = { d_fg, d_bg, d_fg, d_bg };
	RD::TextureFormat step_tf;
	step_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	step_tf.width = 2;
	step_tf.height = 2;
	step_tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	Vector<uint8_t> step_bytes;
	step_bytes.resize(sizeof(step_2x2));
	memcpy(step_bytes.ptrw(), step_2x2, sizeof(step_2x2));
	Vector<Vector<uint8_t>> step_slices;
	step_slices.push_back(step_bytes);
	const RID step_src_tex = rd.texture_create(step_tf, RD::TextureView(), step_slices);
	REQUIRE(step_src_tex.is_valid());

	const RID vp_step = RID::from_uint64(0xD001ull);
	REQUIRE(scene_collision.request_viewport_depth_snapshot(vp_step) == OK);
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_step, step_src_tex, 4, 4, Transform3D(), rd_proj, 0.1f, 100.0f, 1) == OK);

	Ref<SceneRD::DepthReadLease> step_lease;
	GDExtensionSceneOpaque32V1 step_tok = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_step, step_lease, step_tok) == OK);
	const Vector<uint8_t> copied_bytes = rd.texture_get_data(step_lease->get_generation()->depth_texture, 0);
	REQUIRE(copied_bytes.size() == 16 * sizeof(float));
	const float *copied_f = reinterpret_cast<const float *>(copied_bytes.ptr());
	for (int i = 0; i < 16; ++i) {
		const bool exact_fg_or_bg = (Math::abs(copied_f[i] - d_fg) < 1e-5f) || (Math::abs(copied_f[i] - d_bg) < 1e-5f);
		CHECK(exact_fg_or_bg);
	}
	CHECK(step_lease->return_after_reads() == OK);
	step_lease.unref();

	// 6. Combined depth-stencil D32_SFLOAT_S8_UINT source cleared via depth attachment to 0.375f ->
	//    copied via 2D depth slice with 0 aspect errors!
	RD::TextureFormat ds_tf;
	ds_tf.format = RD::DATA_FORMAT_D32_SFLOAT_S8_UINT;
	ds_tf.width = 8;
	ds_tf.height = 8;
	ds_tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
	const RID ds_tex = rd.texture_create(ds_tf, RD::TextureView());
	REQUIRE(ds_tex.is_valid());
	Vector<RID> ds_fb_att;
	ds_fb_att.push_back(ds_tex);
	const RID ds_fb = rd.framebuffer_create(ds_fb_att);
	REQUIRE(ds_fb.is_valid());
	const RD::DrawListID clear_dl = rd.draw_list_begin(ds_fb, RD::DRAW_CLEAR_DEPTH | RD::DRAW_CLEAR_STENCIL, Vector<Color>(), 0.375f, 5u);
	rd.draw_list_end();
	(void)clear_dl;

	const RID vp_ds = RID::from_uint64(0xD002ull);
	REQUIRE(scene_collision.request_viewport_depth_snapshot(vp_ds) == OK);
	REQUIRE(scene_collision.capture_viewport_depth_snapshot(vp_ds, ds_tex, 8, 8, Transform3D(), rd_proj, 0.1f, 100.0f, 1) == OK);

	Ref<SceneRD::DepthReadLease> ds_lease;
	GDExtensionSceneOpaque32V1 ds_tok = {};
	REQUIRE(scene_collision.acquire_depth_snapshot_read_lease(vp_ds, ds_lease, ds_tok) == OK);
	const Vector<uint8_t> ds_out_bytes = rd.texture_get_data(ds_lease->get_generation()->depth_texture, 0);
	REQUIRE(ds_out_bytes.size() == 64 * sizeof(float));
	const float *ds_f = reinterpret_cast<const float *>(ds_out_bytes.ptr());
	for (int i = 0; i < 64; ++i) {
		CHECK(ds_f[i] == doctest::Approx(0.375f).epsilon(1e-5f));
	}
	CHECK(ds_lease->return_after_reads() == OK);
	ds_lease.unref();

	scene_collision.shutdown();
	CHECK(scene_collision.get_live_rid_count() == 0u);
	rd.free_rid(ds_fb);
	rd.free_rid(ds_tex);
	rd.free_rid(step_src_tex);
#endif
}

TEST_CASE("[particle_scene_k06][SceneTree] non-RD and off-thread refusal") {
	using SceneRD = RendererRD::ParticlesCollisionSceneRD;

	// 1. Dummy / non-RD / uninitialized C ABI calls return ERR_UNAVAILABLE with 0 RID allocations:
	const auto arm_fn = reinterpret_cast<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotArmV1>(
			GDExtension::get_interface_function("device_session_scene_depth_snapshot_arm_v1"));
	const auto borrow_fn = reinterpret_cast<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotBorrowV1>(
			GDExtension::get_interface_function("device_session_scene_depth_snapshot_borrow_v1"));
	REQUIRE(arm_fn != nullptr);
	REQUIRE(borrow_fn != nullptr);

	GDExtensionSceneDepthSnapshotArmV1 arm_in = {};
	arm_in.abi = { 1, sizeof(arm_in), 0 };
	arm_in.viewport_rid = 0xE001ull;
	arm_in.slab_thickness_m = 0.50f;
	arm_in.silhouette_jump_m = 0.50f;
	const GDExtensionDeviceSessionResultV1 arm_res = arm_fn({ 1, 1 }, &arm_in);
	CHECK(arm_res.native_error == ERR_UNAVAILABLE);

	const GDExtensionSceneOperationKeyV1 valid_key = make_valid_scene_key();
	GDExtensionSceneDepthSnapshotBorrowResultV1 borrow_out = {};
	const GDExtensionDeviceSessionResultV1 borrow_res = borrow_fn({ 1, 1 }, { 1, 1 }, 0, 0xE001ull, &valid_key, &borrow_out);
	CHECK(borrow_res.native_error == ERR_UNAVAILABLE);
	CHECK(borrow_out.outcome.native_error == ERR_UNAVAILABLE);
	CHECK(borrow_out.meta_ssbo_rid == 0ull);
	CHECK(borrow_out.depth_texture_rid == 0ull);

	// 2. Uninitialized ParticlesCollisionSceneRD refuses cleanly with ERR_UNAVAILABLE:
	SceneRD uninit_scene;
	Ref<SceneRD::DepthReadLease> null_lease;
	GDExtensionSceneOpaque32V1 null_tok = {};
	CHECK(uninit_scene.request_viewport_depth_snapshot(RID::from_uint64(1)) == ERR_UNAVAILABLE);
	CHECK(uninit_scene.acquire_depth_snapshot_read_lease(RID::from_uint64(1), null_lease, null_tok) == ERR_UNAVAILABLE);
	CHECK(uninit_scene.capture_viewport_depth_snapshot(RID::from_uint64(1), RID::from_uint64(2), 8, 8, Transform3D(), Projection(), 0.1f, 100.0f, 1) == ERR_UNAVAILABLE);
	CHECK(uninit_scene.get_live_rid_count() == 0u);

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
	// 3. Off-render-thread capture and lease allocation MUST refuse with ERR_UNAUTHORIZED and 0 RID allocations:
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);

	SceneRD scene_collision;
	const GDExtensionSceneOpaque32V1 dev_cap = make_opaque32(0x4b303644ull, 77, 88, 99);
	scene_collision.initialize(&rd, dev_cap);

	struct OffThreadProbe {
		SceneRD *cs = nullptr;
		Error lease_err = OK;
		Error capture_err = OK;
	} probe{ &scene_collision };

	Thread worker;
	worker.start([](void *p_ud) {
		auto *p = static_cast<OffThreadProbe *>(p_ud);
		Ref<SceneRD::DepthReadLease> l;
		GDExtensionSceneOpaque32V1 tok = {};
		p->lease_err = p->cs->acquire_depth_snapshot_read_lease(RID::from_uint64(0xE002ull), l, tok);
		p->capture_err = p->cs->capture_viewport_depth_snapshot(
				RID::from_uint64(0xE002ull), RID::from_uint64(1), 16, 16, Transform3D(), Projection(), 0.1f, 100.0f, 1);
	}, &probe);
	worker.wait_to_finish();

	CHECK(probe.lease_err == ERR_UNAUTHORIZED);
	CHECK(probe.capture_err == ERR_UNAUTHORIZED);
	CHECK(scene_collision.get_live_rid_count() == 0u);
	CHECK(scene_collision.get_active_lease_count() == 0u);

	scene_collision.shutdown();
#endif
}

} // namespace TestParticleSceneK06
