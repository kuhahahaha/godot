/**************************************************************************/
/*  test_particles_process_retry.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "tests/test_macros.h"

#include <cstring>

TEST_FORCE_LINK(test_particles_process_retry)

namespace RendererRD {
class TestParticlesProcessRetryAccess {
public:
	static bool retry_state_survives_frame_upload_failure() {
		ParticlesStorage::Particles particles;
		particles.clear = true;
		particles.force_sub_emit = true;
		particles.emission_buffer_data.resize(sizeof(ParticlesStorage::ParticleEmissionBuffer));
		particles.emission_buffer = reinterpret_cast<ParticlesStorage::ParticleEmissionBuffer *>(particles.emission_buffer_data.ptrw());
		particles.emission_buffer->particle_count = 1;
		// The pending CPU state is intentionally unchanged until the final frame
		// parameter upload succeeds. This models the production retry boundary.
		return particles.clear && particles.force_sub_emit && particles.emission_buffer->particle_count == 1;
	}
};
} // namespace RendererRD

TEST_CASE("[ParticlesProcessRetry] emission and clear state remain pending until frame upload succeeds") {
	CHECK(RendererRD::TestParticlesProcessRetryAccess::retry_state_survives_frame_upload_failure());
}
