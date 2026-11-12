/**************************************************************************/
/*  test_gpu_particles.cpp                                                */
/**************************************************************************/

#include "tests/test_macros.h"

#include <limits>

TEST_FORCE_LINK(test_gpu_particles)

#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/resources/particle_process_material.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

#ifndef _3D_DISABLED
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/skin.h"
#endif

namespace TestGPUParticles {

static int count_signal_target(Object *p_source, const StringName &p_signal, ObjectID p_target) {
	List<Object::Connection> connections;
	p_source->get_signal_connection_list(p_signal, &connections);
	int count = 0;
	for (const Object::Connection &connection : connections) {
		if (connection.callable.get_object_id() == p_target) {
			count++;
		}
	}
	return count;
}

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

TEST_CASE("[CPUParticles] 2D ratio setters reject values outside normalized range") {
	CPUParticles2D *particles = memnew(CPUParticles2D);
	particles->set_explosiveness_ratio(0.25f);
	particles->set_randomness_ratio(0.75f);
	particles->set_lifetime_randomness(0.5);
	ERR_PRINT_OFF;
	particles->set_explosiveness_ratio(-0.1f);
	particles->set_randomness_ratio(1.1f);
	particles->set_lifetime_randomness(2.0);
	ERR_PRINT_ON;
	CHECK(particles->get_explosiveness_ratio() == 0.25f);
	CHECK(particles->get_randomness_ratio() == 0.75f);
	CHECK(particles->get_lifetime_randomness() == 0.5);
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

TEST_CASE("[CPUParticles] 3D ratio setters reject values outside normalized range") {
	CPUParticles3D *particles = memnew(CPUParticles3D);
	particles->set_explosiveness_ratio(0.25f);
	particles->set_randomness_ratio(0.75f);
	particles->set_lifetime_randomness(0.5);
	ERR_PRINT_OFF;
	particles->set_explosiveness_ratio(-0.1f);
	particles->set_randomness_ratio(1.1f);
	particles->set_lifetime_randomness(2.0);
	ERR_PRINT_ON;
	CHECK(particles->get_explosiveness_ratio() == 0.25f);
	CHECK(particles->get_randomness_ratio() == 0.75f);
	CHECK(particles->get_lifetime_randomness() == 0.5);
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

TEST_CASE("[GPUParticles] 3D animation warning checks all draw-pass surfaces") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	BoxMesh::create_mesh_array(arrays, Vector3(1.0, 1.0, 1.0));
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	Ref<StandardMaterial3D> animated_material;
	animated_material.instantiate();
	animated_material->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	Ref<StandardMaterial3D> ordinary_material;
	ordinary_material.instantiate();
	mesh->surface_set_material(0, animated_material);
	mesh->surface_set_material(1, ordinary_material);
	particles->set_draw_pass_mesh(0, mesh);

	Ref<ParticleProcessMaterial> process_material;
	process_material.instantiate();
	process_material->set_param_max(ParticleProcessMaterial::PARAM_ANIM_SPEED, 1.0f);
	particles->set_process_material(process_material);

	auto has_animation_warning = [&]() {
		for (const String &warning : particles->get_configuration_warnings()) {
			if (warning.contains("Particles animation requires")) {
				return true;
			}
		}
		return false;
	};

	CHECK_FALSE(has_animation_warning());
	particles->set_material_override(ordinary_material);
	CHECK(has_animation_warning());
	particles->set_material_override(animated_material);
	CHECK_FALSE(has_animation_warning());
	particles->set_material_override(Ref<Material>());
	mesh->surface_set_material(0, ordinary_material);
	CHECK(has_animation_warning());
	mesh->surface_set_material(1, animated_material);
	CHECK_FALSE(has_animation_warning());
	memdelete(particles);
}

#endif

TEST_CASE("[GPUParticles] 2D collision size rejects texture scaling overflow") {
	GPUParticles2D *particles = memnew(GPUParticles2D);
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	particles->set_texture(texture);
	particles->set_collision_base_size(2.0f);
	CHECK(particles->get_collision_base_size() == 2.0f);
	ERR_PRINT_OFF;
	particles->set_collision_base_size(std::numeric_limits<float>::max());
	ERR_PRINT_ON;
	CHECK(particles->get_collision_base_size() == 2.0f);
	memdelete(particles);

	GPUParticles2D *replacement_particles = memnew(GPUParticles2D);
	replacement_particles->set_collision_base_size(std::numeric_limits<float>::max());
	ERR_PRINT_OFF;
	replacement_particles->set_texture(texture);
	ERR_PRINT_ON;
	CHECK(replacement_particles->get_texture().is_null());
	CHECK(replacement_particles->get_collision_base_size() == std::numeric_limits<float>::max());
	memdelete(replacement_particles);
}

TEST_CASE("[GPUParticles] 2D texture replacement rejection preserves the live connection") {
	Ref<Image> small_image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	Ref<Image> large_image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	Ref<ImageTexture> texture_a = ImageTexture::create_from_image(small_image);
	Ref<ImageTexture> texture_b = ImageTexture::create_from_image(large_image);
	GPUParticles2D *particles = memnew(GPUParticles2D);

	particles->set_texture(texture_a);
	const ObjectID target_id = particles->get_instance_id();
	CHECK(particles->get_texture() == texture_a);
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 1);

	// A 4x4 replacement overflows only after the collision size is raised to
	// FLT_MAX, so the old source must remain attached when the setter refuses it.
	particles->set_collision_base_size(std::numeric_limits<float>::max());
	ERR_PRINT_OFF;
	particles->set_texture(texture_b);
	ERR_PRINT_ON;
	CHECK(particles->get_texture() == texture_a);
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 1);
	CHECK(count_signal_target(texture_b.ptr(), "changed", target_id) == 0);

	particles->set_collision_base_size(1.0f);
	particles->set_texture(Ref<Texture2D>());
	CHECK(particles->get_texture().is_null());
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 0);
	memdelete(particles);
}

TEST_CASE("[CPUParticles] 2D texture replacement transfers the changed connection") {
	Ref<Image> image_a = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	Ref<Image> image_b = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	Ref<ImageTexture> texture_a = ImageTexture::create_from_image(image_a);
	Ref<ImageTexture> texture_b = ImageTexture::create_from_image(image_b);
	CPUParticles2D *particles = memnew(CPUParticles2D);
	const ObjectID target_id = particles->get_instance_id();

	particles->set_texture(texture_a);
	CHECK(particles->get_texture() == texture_a);
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 1);
	particles->set_texture(texture_a);
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 1);

	particles->set_texture(texture_b);
	CHECK(particles->get_texture() == texture_b);
	CHECK(count_signal_target(texture_a.ptr(), "changed", target_id) == 0);
	CHECK(count_signal_target(texture_b.ptr(), "changed", target_id) == 1);

	particles->set_texture(Ref<Texture2D>());
	CHECK(particles->get_texture().is_null());
	CHECK(count_signal_target(texture_b.ptr(), "changed", target_id) == 0);
	memdelete(particles);
}

TEST_CASE("[GPUParticles] process material replacement transfers the 2D changed connection") {
	GPUParticles2D *particles = memnew(GPUParticles2D);
	Ref<ParticleProcessMaterial> material_a;
	Ref<ParticleProcessMaterial> material_b;
	material_a.instantiate();
	material_b.instantiate();
	const ObjectID target_id = particles->get_instance_id();

	particles->set_process_material(material_a);
	CHECK(particles->get_process_material() == material_a);
	CHECK(count_signal_target(material_a.ptr(), "emission_shape_changed", target_id) == 1);
	particles->set_process_material(material_a);
	CHECK(count_signal_target(material_a.ptr(), "emission_shape_changed", target_id) == 1);
	particles->set_process_material(material_b);
	CHECK(particles->get_process_material() == material_b);
	CHECK(count_signal_target(material_a.ptr(), "emission_shape_changed", target_id) == 0);
	CHECK(count_signal_target(material_b.ptr(), "emission_shape_changed", target_id) == 1);
	particles->set_process_material(Ref<Material>());
	CHECK(particles->get_process_material().is_null());
	CHECK(count_signal_target(material_b.ptr(), "emission_shape_changed", target_id) == 0);
	memdelete(particles);
}

TEST_CASE("[GPUParticles] 2D texture connection is removed when the node is destroyed") {
	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	ObjectID target_id;
	{
		GPUParticles2D *particles = memnew(GPUParticles2D);
		particles->set_texture(texture);
		target_id = particles->get_instance_id();
		CHECK(count_signal_target(texture.ptr(), "changed", target_id) == 1);
		memdelete(particles);
	}
	CHECK(count_signal_target(texture.ptr(), "changed", target_id) == 0);
}

#ifndef _3D_DISABLED
TEST_CASE("[GPUParticles] 3D process material and skin replacement transfer connections") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	Ref<ParticleProcessMaterial> material_a;
	Ref<ParticleProcessMaterial> material_b;
	Ref<Skin> skin_a;
	Ref<Skin> skin_b;
	material_a.instantiate();
	material_b.instantiate();
	skin_a.instantiate();
	skin_b.instantiate();
	const ObjectID target_id = particles->get_instance_id();

	particles->set_process_material(material_a);
	CHECK(particles->get_process_material() == material_a);
	particles->set_process_material(material_a);
	#ifdef TOOLS_ENABLED
	CHECK(count_signal_target(material_a.ptr(), "emission_shape_changed", target_id) == 1);
	#endif
	particles->set_process_material(material_b);
	CHECK(particles->get_process_material() == material_b);
	#ifdef TOOLS_ENABLED
	CHECK(count_signal_target(material_a.ptr(), "emission_shape_changed", target_id) == 0);
	CHECK(count_signal_target(material_b.ptr(), "emission_shape_changed", target_id) == 1);
	#endif
	particles->set_process_material(Ref<Material>());
	CHECK(particles->get_process_material().is_null());
	#ifdef TOOLS_ENABLED
	CHECK(count_signal_target(material_b.ptr(), "emission_shape_changed", target_id) == 0);
	#endif

	particles->set_skin(skin_a);
	CHECK(particles->get_skin() == skin_a);
	CHECK(count_signal_target(skin_a.ptr(), "changed", target_id) == 1);
	particles->set_skin(skin_b);
	CHECK(particles->get_skin() == skin_b);
	CHECK(count_signal_target(skin_a.ptr(), "changed", target_id) == 0);
	CHECK(count_signal_target(skin_b.ptr(), "changed", target_id) == 1);
	particles->set_skin(Ref<Skin>());
	CHECK(particles->get_skin().is_null());
	CHECK(count_signal_target(skin_b.ptr(), "changed", target_id) == 0);
	memdelete(particles);
}

TEST_CASE("[GPUParticles] 3D resource connections are removed when the node is destroyed") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	Ref<ParticleProcessMaterial> process_material;
	Ref<Skin> skin;
	process_material.instantiate();
	skin.instantiate();
	const ObjectID target_id = particles->get_instance_id();

	particles->set_process_material(process_material);
	particles->set_skin(skin);
	CHECK(count_signal_target(skin.ptr(), "changed", target_id) == 1);
#ifdef TOOLS_ENABLED
	CHECK(count_signal_target(process_material.ptr(), "emission_shape_changed", target_id) == 1);
#endif

	memdelete(particles);
	CHECK(count_signal_target(skin.ptr(), "changed", target_id) == 0);
#ifdef TOOLS_ENABLED
	CHECK(count_signal_target(process_material.ptr(), "emission_shape_changed", target_id) == 0);
#endif
}

TEST_CASE("[GPUParticles] shared draw-pass mesh keeps its changed connection") {
	GPUParticles3D *particles = memnew(GPUParticles3D);
	Ref<BoxMesh> mesh;
	mesh.instantiate();
	particles->set_draw_passes(2);
	particles->set_draw_pass_mesh(0, mesh);
	particles->set_draw_pass_mesh(1, mesh);

	const ObjectID target_id = particles->get_instance_id();
	const int connected_count = count_signal_target(mesh.ptr(), "changed", target_id);
	CHECK(connected_count > 0);

	particles->set_draw_passes(1);
	CHECK(count_signal_target(mesh.ptr(), "changed", target_id) == connected_count);

	particles->set_draw_pass_mesh(0, Ref<Mesh>());
	CHECK(count_signal_target(mesh.ptr(), "changed", target_id) == 0);
	memdelete(particles);
}
#endif

} // namespace TestGPUParticles
