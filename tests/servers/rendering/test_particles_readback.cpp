/**************************************************************************/
/*  test_particles_readback.cpp                                           */
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

#include "servers/rendering/dummy/storage/particles_storage.h"
#include "servers/rendering/renderer_rd/renderer_canvas_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_particles_readback)

class TestParticlesCanvasTrailAccess {
public:
	static uint32_t trail_mask() {
		return RendererCanvasRenderRD::BATCH_FLAGS_INSTANCING_MASK;
	}
	static uint32_t colors_flag() {
		return RendererCanvasRenderRD::BATCH_FLAGS_INSTANCING_HAS_COLORS;
	}
	static uint32_t custom_data_flag() {
		return RendererCanvasRenderRD::BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA;
	}
};

TEST_CASE("[ParticlesReadback] Canvas trail count fits the advertised 128 sections") {
	constexpr uint32_t bind_pose_count = 128 + 1;
	CHECK((bind_pose_count & TestParticlesCanvasTrailAccess::trail_mask()) == bind_pose_count);
	CHECK((TestParticlesCanvasTrailAccess::trail_mask() & TestParticlesCanvasTrailAccess::colors_flag()) == 0);
	CHECK((TestParticlesCanvasTrailAccess::trail_mask() & TestParticlesCanvasTrailAccess::custom_data_flag()) == 0);
}

namespace RendererRD {
class TestParticlesReadbackAccess {
public:
	static uint64_t particle_data_size(uint32_t p_userdata_count) {
		return sizeof(ParticlesStorage::ParticleData) + 4 * sizeof(float) * p_userdata_count;
	}
	static bool data_size(uint32_t p_amount, uint32_t p_userdata_count, uint32_t p_trail_count, uint64_t &r_size) {
		ParticlesStorage::Particles particles;
		particles.amount = p_amount;
		particles.userdata_count = p_userdata_count;
		particles.trails_enabled = p_trail_count > 1;
		particles.trail_bind_poses.resize(p_trail_count);
		return ParticlesStorage::_particles_get_data_size(&particles, r_size);
	}
	static bool raw_data_size(uint64_t p_amount, uint64_t p_userdata_count, uint64_t p_trail_count, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_data_size(p_amount, p_userdata_count, p_trail_count, r_size);
	}
	static bool instance_data_size(uint64_t p_total_amount, uint32_t p_xform_size, bool p_motion_vectors, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_instance_data_size(p_total_amount, p_xform_size, p_motion_vectors, r_size);
	}
	static bool sort_data_size(uint64_t p_amount, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_sort_data_size(p_amount, r_size);
	}
	static bool total_amount(uint64_t p_amount, uint64_t p_trail_count, uint32_t &r_total_amount) {
		return ParticlesStorage::_particles_get_total_amount(p_amount, p_trail_count, r_total_amount);
	}
	static bool draw_amount(uint64_t p_amount, bool p_trails_enabled, uint64_t p_trail_count, uint32_t &r_total_amount, uint32_t &r_trail_divisor) {
		return ParticlesStorage::_particles_get_draw_amount(p_amount, p_trails_enabled, p_trail_count, r_total_amount, r_trail_divisor);
	}
	static bool emission_data_size(uint64_t p_amount, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_emission_data_size(p_amount, r_size);
	}
	static uint64_t emission_entry_size() {
		return sizeof(ParticlesStorage::ParticleEmissionBuffer::Data);
	}
	static bool trail_bind_pose_data_size(uint64_t p_count, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_trail_bind_pose_data_size(p_count, r_size);
	}
	static bool frame_params_data_size(uint64_t p_count, uint64_t &r_size) {
		return ParticlesStorage::_particles_get_frame_params_data_size(p_count, r_size);
	}
	static uint64_t frame_params_entry_size() {
		return sizeof(ParticlesStorage::ParticlesFrameParams);
	}
	static bool trail_history_index(uint64_t p_index, uint64_t p_history_size, uint64_t p_trail_size, uint64_t &r_index) {
		return ParticlesStorage::_particles_get_trail_history_index(p_index, p_history_size, p_trail_size, r_index);
	}
	static bool lifetime_split(uint64_t p_amount, double p_phase, uint32_t &r_split) {
		return ParticlesStorage::_particles_get_lifetime_split(p_amount, p_phase, r_split);
	}
	static bool history_size(double p_trail_lifetime, int p_fixed_fps, uint32_t &r_size) {
		return ParticlesStorage::_particles_get_history_size(p_trail_lifetime, p_fixed_fps, r_size);
	}
};
} // namespace RendererRD

TEST_CASE("[ParticlesReadback] data size includes trails and user data with a bounded result") {
	uint64_t size = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::data_size(2, 3, 0, size));
	CHECK(size == 2 * RendererRD::TestParticlesReadbackAccess::particle_data_size(3));

	CHECK(RendererRD::TestParticlesReadbackAccess::data_size(2, 3, 4, size));
	CHECK(size == 8 * RendererRD::TestParticlesReadbackAccess::particle_data_size(3));

	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::data_size(UINT32_MAX, UINT32_MAX, 0, size));
	CHECK(size == 0);

	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::raw_data_size(UINT32_MAX, 0, UINT32_MAX, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::raw_data_size(1U << 26, 0, 1, size));
	CHECK(size == 0);

	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::instance_data_size(UINT64_MAX, 3, false, size));
	CHECK(size == 0);

	CHECK(RendererRD::TestParticlesReadbackAccess::instance_data_size(2, 3, false, size));
	CHECK(size == 2 * 5 * 4 * sizeof(float));
	CHECK(RendererRD::TestParticlesReadbackAccess::instance_data_size(2, 3, true, size));
	CHECK(size == 2 * 2 * 5 * 4 * sizeof(float));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::instance_data_size(1U << 26, 2, false, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::instance_data_size(1U << 25, 2, true, size));
	CHECK(size == 0);

	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::sort_data_size(0, size));
	CHECK(size == 0);
	CHECK(RendererRD::TestParticlesReadbackAccess::sort_data_size(2, size));
	CHECK(size == 2 * 2 * sizeof(float));
	CHECK(RendererRD::TestParticlesReadbackAccess::sort_data_size(3, size));
	CHECK(size == 4 * 2 * sizeof(float));
	const uint64_t max_sort_amount = (UINT32_MAX / (sizeof(float) * 2)) & ~uint64_t(1);
	CHECK(RendererRD::TestParticlesReadbackAccess::sort_data_size(max_sort_amount, size));
	CHECK(size == max_sort_amount * 2 * sizeof(float));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::sort_data_size(max_sort_amount + 1, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::sort_data_size(UINT32_MAX, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::sort_data_size(UINT64_MAX, size));
	CHECK(size == 0);

	uint32_t total_amount = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::total_amount(2, 4, total_amount));
	CHECK(total_amount == 8);
	CHECK(RendererRD::TestParticlesReadbackAccess::total_amount(2, 0, total_amount));
	CHECK(total_amount == 2);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::total_amount(0, 4, total_amount));
	CHECK(total_amount == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::total_amount(UINT32_MAX, 2, total_amount));
	CHECK(total_amount == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::total_amount(UINT64_MAX, 2, total_amount));
	CHECK(total_amount == 0);

	uint32_t trail_divisor = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::draw_amount(2, true, 4, total_amount, trail_divisor));
	CHECK(total_amount == 8);
	CHECK(trail_divisor == 4);
	CHECK(RendererRD::TestParticlesReadbackAccess::draw_amount(2, false, 4, total_amount, trail_divisor));
	CHECK(total_amount == 2);
	CHECK(trail_divisor == 1);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::draw_amount(UINT32_MAX, true, 2, total_amount, trail_divisor));
	CHECK(total_amount == 0);
	CHECK(trail_divisor == 1);

	CHECK(RendererRD::TestParticlesReadbackAccess::emission_data_size(2, size));
	CHECK(size == RendererRD::TestParticlesReadbackAccess::emission_entry_size() * 2 + sizeof(uint32_t) * 4);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::emission_data_size(UINT64_MAX, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::emission_data_size(1U << 26, size));
	CHECK(size == 0);

	CHECK(RendererRD::TestParticlesReadbackAccess::trail_bind_pose_data_size(2, size));
	CHECK(size == 2 * 16 * sizeof(float));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_bind_pose_data_size(0, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_bind_pose_data_size(UINT64_MAX, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_bind_pose_data_size(1U << 26, size));
	CHECK(size == 0);

	CHECK(RendererRD::TestParticlesReadbackAccess::frame_params_data_size(2, size));
	CHECK(size == 2 * RendererRD::TestParticlesReadbackAccess::frame_params_entry_size());
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::frame_params_data_size(0, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::frame_params_data_size(UINT64_MAX, size));
	CHECK(size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::frame_params_data_size(1U << 26, size));
	CHECK(size == 0);

	uint64_t history_index = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::trail_history_index(1, 4, 2, history_index));
	CHECK(history_index == 2);
	CHECK(RendererRD::TestParticlesReadbackAccess::trail_history_index(1, UINT64_MAX, 2, history_index));
	CHECK(history_index == UINT64_MAX / 2);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_history_index(2, 4, 2, history_index));
	CHECK(history_index == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_history_index(0, 0, 2, history_index));
	CHECK(history_index == 0);
	CHECK(RendererRD::TestParticlesReadbackAccess::trail_history_index(0, UINT64_MAX, 2, history_index));
	CHECK(history_index == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_history_index(0, 4, 0, history_index));
	CHECK(history_index == 0);

	uint32_t lifetime_split = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::lifetime_split(10, 0.2, lifetime_split));
	CHECK(lifetime_split == 3);
	CHECK(RendererRD::TestParticlesReadbackAccess::lifetime_split(4, 1.0, lifetime_split));
	CHECK(lifetime_split == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_split(0, 0.5, lifetime_split));
	CHECK(lifetime_split == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_split(4, -0.1, lifetime_split));
	CHECK(lifetime_split == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_split(4, Math::NaN, lifetime_split));
	CHECK(lifetime_split == 0);
	CHECK(RendererRD::TestParticlesReadbackAccess::lifetime_split(4, 1e300, lifetime_split));
	CHECK(lifetime_split == 0);

	uint32_t history_size = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::history_size(0.3, 60, history_size));
	CHECK(history_size == 18);
	CHECK(RendererRD::TestParticlesReadbackAccess::history_size(double(INT32_MAX), 1, history_size));
	CHECK(history_size == INT32_MAX);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::history_size(double(INT32_MAX) + 1.0, 1, history_size));
	CHECK(history_size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::history_size(10.0, -1, history_size));
	CHECK(history_size == 0);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::history_size(Math::NaN, 60, history_size));
	CHECK(history_size == 0);
}

TEST_CASE("[ParticlesReadback] invalid particle RID returns an empty CPU snapshot") {
	// The compatibility storage has no GPU buffer and must fail closed.
	RendererDummy::ParticlesStorage storage;
	CHECK(storage.particles_get_current_data(RID()).is_empty());
}
