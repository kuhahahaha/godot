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
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/test_macros.h"

#include <cfloat>

TEST_FORCE_LINK(test_particles_readback)

namespace RendererRD {
class TestParticlesReadbackAccess {
public:
	static bool amount_is_valid(int p_amount) {
		return ParticlesStorage::_particles_amount_is_valid(p_amount);
	}
	static bool lifetime_is_valid(double p_lifetime) {
		return ParticlesStorage::_particles_lifetime_is_valid(p_lifetime);
	}
	static bool trail_lifetime_is_valid(double p_lifetime) {
		return ParticlesStorage::_particles_trail_lifetime_is_valid(p_lifetime);
	}
	static bool process_time_is_valid(double p_time) {
		return ParticlesStorage::_particles_process_time_is_valid(p_time);
	}
	static bool speed_scale_is_valid(double p_scale) {
		return ParticlesStorage::_particles_speed_scale_is_valid(p_scale);
	}
	static bool ratio_is_valid(double p_ratio) {
		return ParticlesStorage::_particles_ratio_is_valid(p_ratio);
	}
	static bool buffer_size(uint64_t p_count, uint64_t p_element_size, uint64_t p_header_size, uint32_t &r_size) {
		return ParticlesStorage::_particles_get_buffer_size(p_count, p_element_size, p_header_size, r_size);
	}
	static bool total_amount(uint32_t p_amount, uint32_t p_trail_count, uint32_t &r_total) {
		ParticlesStorage::Particles particles;
		particles.amount = p_amount;
		particles.trails_enabled = p_trail_count > 1;
		particles.trail_bind_poses.resize(p_trail_count);
		return ParticlesStorage::_particles_get_total_amount(&particles, r_total);
	}
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
}

TEST_CASE("[ParticlesReadback] invalid particle RID returns an empty CPU snapshot") {
	// The compatibility storage has no GPU buffer and must fail closed.
	RendererDummy::ParticlesStorage storage;
	CHECK(storage.particles_get_current_data(RID()).is_empty());
}

TEST_CASE("[ParticlesReadback] process amount rejects trail multiplication overflow") {
	uint32_t total = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::total_amount(4, 3, total));
	CHECK(total == 12);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::total_amount(UINT32_MAX, 3, total));
}

TEST_CASE("[ParticlesReadback] particle buffer sizes reject uint32 overflow") {
	uint32_t size = 0;
	CHECK(RendererRD::TestParticlesReadbackAccess::buffer_size(4, 16, 16, size));
	CHECK(size == 80);
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::buffer_size(UINT64_MAX, 16, 16, size));
	CHECK(size == 0);
}

TEST_CASE("[ParticlesReadback] negative particle amounts fail closed") {
	CHECK(RendererRD::TestParticlesReadbackAccess::amount_is_valid(0));
	CHECK(RendererRD::TestParticlesReadbackAccess::amount_is_valid(INT32_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::amount_is_valid(-1));
}

TEST_CASE("[ParticlesReadback] particle lifetimes reject non-finite and non-positive values") {
	CHECK(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(0.01));
	CHECK(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(DBL_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(0.0));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(-1.0));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(NAN));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::lifetime_is_valid(INFINITY));
}

TEST_CASE("[ParticlesReadback] trail lifetimes reject non-finite and too-small values") {
	CHECK(RendererRD::TestParticlesReadbackAccess::trail_lifetime_is_valid(0.01));
	CHECK(RendererRD::TestParticlesReadbackAccess::trail_lifetime_is_valid(10.0));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_lifetime_is_valid(0.0));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_lifetime_is_valid(NAN));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::trail_lifetime_is_valid(INFINITY));
}

TEST_CASE("[ParticlesReadback] process times reject negative and non-representable values") {
	CHECK(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(0.0));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(-1.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(FLT_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(DBL_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(NAN));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(INFINITY));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::process_time_is_valid(-INFINITY));
}

TEST_CASE("[ParticlesReadback] speed scales reject negative and non-finite values") {
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(-1.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(0.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(DBL_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(NAN));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(INFINITY));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::speed_scale_is_valid(-INFINITY));
}

TEST_CASE("[ParticlesReadback] particle ratios reject non-finite values") {
	CHECK(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(-1.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(0.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(1.0));
	CHECK(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(DBL_MAX));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(NAN));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(INFINITY));
	CHECK_FALSE(RendererRD::TestParticlesReadbackAccess::ratio_is_valid(-INFINITY));
}
