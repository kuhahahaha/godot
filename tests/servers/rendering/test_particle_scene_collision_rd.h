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

#include "core/extension/gdextension_scene_late_depth.h"
#include "core/extension/gdextension_scene_phase.h"
#include "core/extension/gdextension_scene_phase_bind.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_collision_scene_rd.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

#include "core/io/marshalls.h"
#include "core/os/thread.h"

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
	CHECK(sizeof(RendererRD::SceneCurrentDepthMeta256) == 256);
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

	// 3. CurrentDepth collision: silhouette discontinuity falls back to view-facing normal.
	RendererRD::SceneCurrentDepthMeta256 dmeta;
	dmeta.width = 4;
	dmeta.height = 4;
	dmeta.z_near = 0.1f;
	dmeta.z_far = 100.0f;
	dmeta.thickness = 1.0f;
	dmeta.silhouette_threshold = 0.25f;
	dmeta.restitution = 0.5f;
	dmeta.friction = 0.0f;
	dmeta.reverse_z = 1;
	Projection proj;
	proj.set_perspective(90.0f, 1.0f, 0.1f, 100.0f, false);
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			dmeta.projection_rows[r][c] = proj.columns[c][r];
		}
	}
	LocalVector<float> depth_texels;
	depth_texels.resize(16);
	for (uint32_t i = 0; i < 16; i++) {
		depth_texels[i] = 0.02f; // ~5m in Reverse-Z with z_near = 0.1
	}
	// Inject sharp silhouette jump on right neighbor:
	depth_texels[2 * 4 + 3] = 0.002f; // ~50m
	const RendererRD::CollisionContactResult depth_res = SceneRD::evaluate_current_depth_collision(
			dmeta, depth_texels, Transform3D(), Vector3(0.0f, 0.0f, -5.1f), Vector3(0.0f, 0.0f, -3.0f), 0.25f, 0.016f);
	REQUIRE(depth_res.collided);
	CHECK(depth_res.silhouette_fallback_used);
	CHECK(depth_res.world_normal.is_equal_approx(Vector3(0.0f, 0.0f, 1.0f)));

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

	// 1. D3-P1-4c: Asymmetric perspective principal point (m02 != 0, m12 != 0) & orthographic CurrentDepth reconstruction:
	{
		RendererRD::SceneCurrentDepthMeta256 dmeta;
		dmeta.width = 8;
		dmeta.height = 8;
		dmeta.z_near = 0.1f;
		dmeta.z_far = 100.0f;
		dmeta.thickness = 1.0f;
		dmeta.silhouette_threshold = 2.0f;
		dmeta.restitution = 0.5f;
		dmeta.friction = 0.0f;
		dmeta.reverse_z = 1;
		Projection asym_proj;
		asym_proj.set_frustum(-0.05f, 0.15f, -0.05f, 0.15f, 0.1f, 100.0f); // m02 = +0.5, m12 = +0.5!
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				dmeta.projection_rows[r][c] = asym_proj.columns[c][r];
			}
		}
		CHECK(Math::abs(dmeta.projection_rows[0][2]) > 0.2f);
		CHECK(Math::abs(dmeta.projection_rows[1][2]) > 0.2f);

		LocalVector<float> depth_texels;
		depth_texels.resize(64);
		for (uint32_t i = 0; i < 64; i++) {
			depth_texels[i] = 0.02f; // z_view = -5.0m flat wall perpendicular to camera Z
		}
		// Particle shifted into off-center frustum at x = +2.5, y = +2.5, z = -5.1 (maps near NDC center under asymmetric frustum!):
		const RendererRD::CollisionContactResult asym_hit = SceneRD::evaluate_current_depth_collision(
				dmeta, depth_texels, Transform3D(), Vector3(2.5f, 2.5f, -5.1f), Vector3(0.0f, 0.0f, -4.0f), 0.25f, 0.016f);
		REQUIRE(asym_hit.collided);
		// Flat z = -5m wall MUST reconstruct normal (0, 0, +1) even with m02 = 0.5, m12 = 0.5:
		CHECK(asym_hit.world_normal.is_equal_approx(Vector3(0.0f, 0.0f, 1.0f)));
		CHECK(asym_hit.post_velocity.z > 0.0f);

		// Orthographic projection CurrentDepth test (m32 == 0, m33 == 1):
		RendererRD::SceneCurrentDepthMeta256 ortho_meta = dmeta;
		Projection ortho_proj;
		ortho_proj.set_orthogonal(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.1f);
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				ortho_meta.projection_rows[r][c] = ortho_proj.columns[c][r];
			}
		}
		LocalVector<float> ortho_texels;
		ortho_texels.resize(64);
		for (uint32_t i = 0; i < 64; i++) {
			ortho_texels[i] = 0.951f; // Reverse-Z linear distance = 100.1 - 0.951 * 100.0 = 5.0m
		}
		const RendererRD::CollisionContactResult ortho_hit = SceneRD::evaluate_current_depth_collision(
				ortho_meta, ortho_texels, Transform3D(), Vector3(3.0f, -2.0f, -5.1f), Vector3(0.0f, 0.0f, -3.0f), 0.25f, 0.016f);
		REQUIRE(ortho_hit.collided);
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

} // namespace TestParticleSceneK06
