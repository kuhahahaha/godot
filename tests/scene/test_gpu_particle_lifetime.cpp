/**************************************************************************/
/*  test_gpu_particle_lifetime.cpp                                         */
/**************************************************************************/

#include "tests/test_macros.h"

#include "scene/2d/gpu_particles_2d.h"
#ifndef _3D_DISABLED
#include "scene/3d/gpu_particles_3d.h"
#endif

#include <limits>

TEST_FORCE_LINK(test_gpu_particle_lifetime)

namespace TestGPUParticleLifetime {

template <typename T>
void check_shader_lifetime_range() {
	T *particles = memnew(T);
	particles->set_emitting(false);
	const double smallest_float = std::numeric_limits<float>::denorm_min();
	const double largest_float = std::numeric_limits<float>::max();
	// The rendering storage accepts a double when its float conversion is
	// positive and finite; exact float representability is not required.
	for (double accepted : { 0.5, smallest_float, double(std::numeric_limits<float>::min()),
			 largest_float, smallest_float * 0.75, largest_float * 1.00000001 }) {
		particles->set_lifetime(accepted);
		CHECK(particles->get_lifetime() == accepted);
	}
	for (double rejected : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
			 std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
			 std::numeric_limits<double>::denorm_min(), smallest_float * 0.5,
			 largest_float * 2.0, std::numeric_limits<double>::max() }) {
		particles->set_lifetime(0.75);
		ERR_PRINT_OFF;
		particles->set_lifetime(rejected);
		ERR_PRINT_ON;
		CHECK(particles->get_lifetime() == 0.75);
	}
	memdelete(particles);
}

TEST_CASE("[GPUParticlesLifetime] 2D scene lifetime matches shader float range") {
	check_shader_lifetime_range<GPUParticles2D>();
}

#ifndef _3D_DISABLED
TEST_CASE("[GPUParticlesLifetime] 3D scene lifetime matches shader float range") {
	check_shader_lifetime_range<GPUParticles3D>();
}
#endif

} // namespace TestGPUParticleLifetime
