/**************************************************************************/
/*  test_particles_process_retry.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_particles_process_retry)

namespace RendererRD {
class TestParticlesProcessRetryAccess {
public:
	static void restore_shifted_history() {
		ParticlesStorage::Particles particles;
		particles.frame_history.resize(4);
		particles.frame_history[0].frame = 10;
		particles.frame_history[1].frame = 20;
		particles.frame_history[2].frame = 30;
		particles.frame_history[3].frame = 40;
		particles.trail_params.resize(2);

		const ParticlesStorage::ParticlesFrameParams previous_first = particles.frame_history[0];
		const ParticlesStorage::ParticlesFrameParams previous_last = particles.frame_history[3];
		particles.frame_history[3] = particles.frame_history[2];
		particles.frame_history[2] = particles.frame_history[1];
		particles.frame_history[1] = particles.frame_history[0];
		particles.frame_history[0].frame = 99;
		particles.trail_params[0].frame = 99;
		particles.trail_params[1].frame = 98;

		ParticlesStorage::_particles_restore_process_history(&particles, previous_first, previous_last);

		CHECK(particles.frame_history[0].frame == 10);
		CHECK(particles.frame_history[1].frame == 20);
		CHECK(particles.frame_history[2].frame == 30);
		CHECK(particles.frame_history[3].frame == 40);
		CHECK(particles.trail_params[0].frame == 10);
		CHECK(particles.trail_params[1].frame == 30);
	}

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

TEST_CASE("[ParticlesProcessRetry] rollback restores shifted history without a full snapshot") {
	RendererRD::TestParticlesProcessRetryAccess::restore_shifted_history();
}
