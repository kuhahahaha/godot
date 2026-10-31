/**************************************************************************/
/*  test_particles_readback.cpp                                           */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/dummy/storage/particles_storage.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_particles_readback)

namespace RendererRD {
class TestParticlesReadbackAccess {
public:
	static bool amount_is_valid(int p_amount) {
		return ParticlesStorage::_particles_amount_is_valid(p_amount);
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
