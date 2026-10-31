/**************************************************************************/
/*  test_particles_process_retry.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_particles_process_retry)

namespace RendererRD {
class TestParticlesProcessRetryAccess {
public:
	static void run() {
		ParticlesStorage::Particles particles;
		ParticlesStorage::Particles sub_emitter;
		particles.clear = true;
		particles.force_sub_emit = true;
		particles.emission_buffer_data.resize(sizeof(ParticlesStorage::ParticleEmissionBuffer));
		particles.emission_buffer = reinterpret_cast<ParticlesStorage::ParticleEmissionBuffer *>(particles.emission_buffer_data.ptrw());
		particles.emission_buffer->particle_count = 1;
		sub_emitter.emission_storage_buffer = RID::from_uint64(1);
		sub_emitter.emitting = true;
		sub_emitter.clear = false;
		sub_emitter.inactive = true;
		sub_emitter.inactive_time = 4.0;

		ParticlesStorage::_particles_commit_process_state(&particles, &sub_emitter, true, false);
		CHECK(particles.clear);
		CHECK(particles.force_sub_emit);
		CHECK(particles.emission_buffer->particle_count == 1);
		CHECK(sub_emitter.emitting);
		CHECK_FALSE(sub_emitter.clear);
		CHECK(sub_emitter.inactive);
		CHECK(sub_emitter.inactive_time == 4.0);
		CHECK_FALSE(sub_emitter.force_sub_emit);

		ParticlesStorage::_particles_commit_process_state(&particles, &sub_emitter, true, true);
		CHECK_FALSE(particles.clear);
		CHECK_FALSE(particles.force_sub_emit);
		CHECK(particles.emission_buffer->particle_count == 0);
		CHECK_FALSE(sub_emitter.emitting);
		CHECK(sub_emitter.clear);
		CHECK_FALSE(sub_emitter.inactive);
		CHECK(sub_emitter.inactive_time == 0.0);
		CHECK(sub_emitter.force_sub_emit);
	}
};
} // namespace RendererRD

TEST_CASE("[ParticlesProcessRetry] failed frame upload keeps emission and sub-emitter state pending") {
	RendererRD::TestParticlesProcessRetryAccess::run();
}
