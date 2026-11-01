/**************************************************************************/
/*  test_gpu_particles.cpp                                                */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_gpu_particles)

#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/resources/particle_process_material.h"

#ifndef _3D_DISABLED
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
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

TEST_CASE("[GPUParticles] 2D trail lifetime is bounded by renderer limit") {
	GPUParticles2D *particles = memnew(GPUParticles2D);
	particles->set_trail_lifetime(20.0);
	CHECK(particles->get_trail_lifetime() == doctest::Approx(10.0));
	memdelete(particles);
}

TEST_CASE("[GPUParticles] 2D rejected process requests preserve emitting state") {
	GPUParticles2D *particles = memnew(GPUParticles2D);
	particles->set_emitting(false);
	ERR_PRINT_OFF;
	particles->request_particles_process(0.5f, NAN);
	ERR_PRINT_ON;
	CHECK_FALSE(particles->is_emitting());
	particles->set_emitting(true);
	ERR_PRINT_OFF;
	particles->request_particles_process(NAN, 0.5f);
	ERR_PRINT_ON;
	CHECK(particles->is_emitting());
	memdelete(particles);
}

TEST_CASE("[CPUParticles] 2D finite scalar setters preserve state on rejection") {
	CPUParticles2D *particles = memnew(CPUParticles2D);
	particles->set_spread(12.0f);
	particles->set_param_min(CPUParticles2D::PARAM_SCALE, 0.25f);
	particles->set_param_max(CPUParticles2D::PARAM_SCALE, 0.75f);
	ERR_PRINT_OFF;
	particles->set_spread(NAN);
	particles->set_param_min(CPUParticles2D::PARAM_SCALE, INFINITY);
	particles->set_param_max(CPUParticles2D::PARAM_SCALE, -INFINITY);
	ERR_PRINT_ON;
	CHECK(particles->get_spread() == 12.0f);
	CHECK(particles->get_param_min(CPUParticles2D::PARAM_SCALE) == 0.25f);
	CHECK(particles->get_param_max(CPUParticles2D::PARAM_SCALE) == 0.75f);
	memdelete(particles);
}

TEST_CASE("[CPUParticles] 2D ratio setters reject non-finite values") {
	CPUParticles2D *particles = memnew(CPUParticles2D);
	particles->set_explosiveness_ratio(0.25f);
	particles->set_randomness_ratio(0.75f);
	ERR_PRINT_OFF;
	particles->set_explosiveness_ratio(NAN);
	particles->set_randomness_ratio(INFINITY);
	ERR_PRINT_ON;
	CHECK(particles->get_explosiveness_ratio() == 0.25f);
	CHECK(particles->get_randomness_ratio() == 0.75f);
	memdelete(particles);
}

#ifndef _3D_DISABLED
TEST_CASE("[GPUParticles] 3D scene ratio setters reject non-finite values") {
	check_ratios<GPUParticles3D>();
}

TEST_CASE("[GPUParticles] 3D trail lifetime is bounded by renderer limit") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	particles->set_trail_lifetime(20.0);
	CHECK(particles->get_trail_lifetime() == doctest::Approx(10.0));
	memdelete(particles);
}

TEST_CASE("[GPUParticles] 3D rejected process requests preserve emitting state") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	particles->set_emitting(false);
	ERR_PRINT_OFF;
	particles->request_particles_process(0.5f, NAN);
	ERR_PRINT_ON;
	CHECK_FALSE(particles->is_emitting());
	particles->set_emitting(true);
	ERR_PRINT_OFF;
	particles->request_particles_process(NAN, 0.5f);
	ERR_PRINT_ON;
	CHECK(particles->is_emitting());
	memdelete(particles);
}

TEST_CASE("[CPUParticles] 3D finite scalar setters preserve state on rejection") {
	CPUParticles3D *particles = memnew(CPUParticles3D);
	particles->set_spread(12.0f);
	particles->set_flatness(0.25f);
	particles->set_param_min(CPUParticles3D::PARAM_SCALE, 0.25f);
	particles->set_param_max(CPUParticles3D::PARAM_SCALE, 0.75f);
	ERR_PRINT_OFF;
	particles->set_spread(NAN);
	particles->set_flatness(INFINITY);
	particles->set_param_min(CPUParticles3D::PARAM_SCALE, INFINITY);
	particles->set_param_max(CPUParticles3D::PARAM_SCALE, -INFINITY);
	ERR_PRINT_ON;
	CHECK(particles->get_spread() == 12.0f);
	CHECK(particles->get_flatness() == 0.25f);
	CHECK(particles->get_param_min(CPUParticles3D::PARAM_SCALE) == 0.25f);
	CHECK(particles->get_param_max(CPUParticles3D::PARAM_SCALE) == 0.75f);
	memdelete(particles);
}

TEST_CASE("[CPUParticles] 3D ratio setters reject non-finite values") {
	CPUParticles3D *particles = memnew(CPUParticles3D);
	particles->set_explosiveness_ratio(0.25f);
	particles->set_randomness_ratio(0.75f);
	ERR_PRINT_OFF;
	particles->set_explosiveness_ratio(-INFINITY);
	particles->set_randomness_ratio(NAN);
	ERR_PRINT_ON;
	CHECK(particles->get_explosiveness_ratio() == 0.25f);
	CHECK(particles->get_randomness_ratio() == 0.75f);
	memdelete(particles);
}

TEST_CASE("[Particles] 3D conversion preserves emission ring axis") {
	CPUParticles3D *cpu_particles = memnew(CPUParticles3D);
	cpu_particles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_RING);
	const Vector3 cpu_axis(1.0, 2.0, 3.0);
	cpu_particles->set_emission_ring_axis(cpu_axis);

	GPUParticles3D *gpu_particles = memnew(GPUParticles3D);
	gpu_particles->convert_from_particles(cpu_particles);
	Ref<ParticleProcessMaterial> material = gpu_particles->get_process_material();
	REQUIRE(material.is_valid());
	CHECK(material->get_emission_ring_axis() == cpu_axis);

	const Vector3 gpu_axis(-2.0, 4.0, 1.0);
	material->set_emission_ring_axis(gpu_axis);
	cpu_particles->convert_from_particles(gpu_particles);
	CHECK(cpu_particles->get_emission_ring_axis() == gpu_axis);

	memdelete(gpu_particles);
	memdelete(cpu_particles);
}

TEST_CASE("[Particles] 3D conversion preserves visibility AABB") {
	CPUParticles3D *cpu_particles = memnew(CPUParticles3D);
	const AABB visibility_aabb(Vector3(-2.0, -3.0, -4.0), Vector3(5.0, 6.0, 7.0));
	cpu_particles->set_visibility_aabb(visibility_aabb);

	GPUParticles3D *gpu_particles = memnew(GPUParticles3D);
	gpu_particles->convert_from_particles(cpu_particles);
	CHECK(gpu_particles->get_visibility_aabb() == visibility_aabb);

	memdelete(gpu_particles);
	memdelete(cpu_particles);
}

TEST_CASE("[Particles] 3D conversion maps draw order enums by meaning") {
	CPUParticles3D *cpu_particles = memnew(CPUParticles3D);
	GPUParticles3D *gpu_particles = memnew(GPUParticles3D);

	cpu_particles->set_draw_order(CPUParticles3D::DRAW_ORDER_VIEW_DEPTH);
	gpu_particles->convert_from_particles(cpu_particles);
	CHECK(gpu_particles->get_draw_order() == GPUParticles3D::DRAW_ORDER_VIEW_DEPTH);

	gpu_particles->set_draw_order(GPUParticles3D::DRAW_ORDER_REVERSE_LIFETIME);
	cpu_particles->convert_from_particles(gpu_particles);
	CHECK(cpu_particles->get_draw_order() == CPUParticles3D::DRAW_ORDER_LIFETIME);

	gpu_particles->set_draw_order(GPUParticles3D::DRAW_ORDER_VIEW_DEPTH);
	cpu_particles->convert_from_particles(gpu_particles);
	CHECK(cpu_particles->get_draw_order() == CPUParticles3D::DRAW_ORDER_VIEW_DEPTH);

	memdelete(gpu_particles);
	memdelete(cpu_particles);
}

TEST_CASE("[GPUParticles] shared draw-pass mesh keeps its changed connection") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	Ref<BoxMesh> mesh;
	mesh.instantiate();
	particles->set_draw_passes(2);
	particles->set_draw_pass_mesh(0, mesh);
	particles->set_draw_pass_mesh(1, mesh);

	List<Object::Connection> connections;
	mesh->get_signal_connection_list("changed", &connections);
	const int connected_count = connections.size();
	CHECK(connected_count > 0);

	particles->set_draw_passes(1);
	connections.clear();
	mesh->get_signal_connection_list("changed", &connections);
	CHECK(connections.size() == connected_count);

	particles->set_draw_pass_mesh(0, Ref<Mesh>());
	connections.clear();
	mesh->get_signal_connection_list("changed", &connections);
	CHECK(connections.is_empty());
	memdelete(particles);
}
#endif

} // namespace TestGPUParticles
