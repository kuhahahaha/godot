/**************************************************************************/
/*  test_gpu_particles.cpp                                                */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_gpu_particles)

#include "scene/2d/gpu_particles_2d.h"

#ifndef _3D_DISABLED
#include "scene/3d/gpu_particles_3d.h"
#endif

namespace TestGPUParticles {

template <typename T>
void check_ratios() {
	T *particles = memnew(T);
	for (float ratio : { -1.0f, 0.0f, 0.5f, 1.0f, 2.0f }) {
		particles->set_amount_ratio(ratio);
		particles->set_explosiveness_ratio(ratio);
		particles->set_randomness_ratio(ratio);
		CHECK(particles->get_amount_ratio() == ratio);
		CHECK(particles->get_explosiveness_ratio() == ratio);
		CHECK(particles->get_randomness_ratio() == ratio);
	}
	for (float invalid : { NAN, INFINITY, -INFINITY }) {
		particles->set_amount_ratio(0.5f);
		particles->set_explosiveness_ratio(0.25f);
		particles->set_randomness_ratio(0.75f);
		ERR_PRINT_OFF;
		particles->set_amount_ratio(invalid);
		particles->set_explosiveness_ratio(invalid);
		particles->set_randomness_ratio(invalid);
		ERR_PRINT_ON;
		CHECK(particles->get_amount_ratio() == 0.5f);
		CHECK(particles->get_explosiveness_ratio() == 0.25f);
		CHECK(particles->get_randomness_ratio() == 0.75f);
	}
	memdelete(particles);
}

TEST_CASE("[GPUParticles] 2D scene ratio setters reject non-finite values") {
	check_ratios<GPUParticles2D>();
}

#ifndef _3D_DISABLED
TEST_CASE("[GPUParticles] 3D scene ratio setters reject non-finite values") {
	check_ratios<GPUParticles3D>();
}
#endif

} // namespace TestGPUParticles
