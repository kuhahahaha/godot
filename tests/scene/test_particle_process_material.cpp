/**************************************************************************/
/*  test_particle_process_material.cpp                                   */
/**************************************************************************/

#include "tests/test_macros.h"

#include "scene/2d/gpu_particles_2d.h"
#include "scene/resources/particle_process_material.h"
#include "servers/rendering/dummy/storage/material_storage.h"

TEST_FORCE_LINK(test_particle_process_material)

namespace TestParticleProcessMaterial {

TEST_CASE("[ParticleProcessMaterial] shared 2D shader retires with the final material") {
	RendererDummy::MaterialStorage *storage = RendererDummy::MaterialStorage::get_singleton();
	REQUIRE(storage != nullptr);
	GPUParticles2D *particles = memnew(GPUParticles2D);
	Ref<ParticleProcessMaterial> material_a;
	Ref<ParticleProcessMaterial> material_b;
	material_a.instantiate();
	material_b.instantiate();

	particles->set_process_material(material_a);
	RID shader = material_a->get_shader_rid();
	CHECK(shader.is_valid());
	CHECK(storage->owns_shader(shader));
	particles->set_process_material(material_b);
	CHECK(material_b->get_shader_rid() == shader);
	CHECK(material_a->get_shader_rid() == shader);
	particles->set_process_material(Ref<Material>());
	memdelete(particles);

	material_a = Ref<ParticleProcessMaterial>();
	CHECK(storage->owns_shader(shader));
	material_b = Ref<ParticleProcessMaterial>();
	CHECK_FALSE(storage->owns_shader(shader));
}

TEST_CASE("[ParticleProcessMaterial] changing a shared variant retires each shader with its last user") {
	RendererDummy::MaterialStorage *storage = RendererDummy::MaterialStorage::get_singleton();
	REQUIRE(storage != nullptr);
	Ref<ParticleProcessMaterial> material_a;
	Ref<ParticleProcessMaterial> material_b;
	material_a.instantiate();
	material_b.instantiate();
	material_a->set_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DISABLE_Z, true);
	material_b->set_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DISABLE_Z, true);
	RID shared_shader = material_a->get_shader_rid();
	CHECK(shared_shader.is_valid());
	CHECK(material_b->get_shader_rid() == shared_shader);

	material_b->set_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DAMPING_AS_FRICTION, true);
	RID changed_shader = material_b->get_shader_rid();
	CHECK(changed_shader.is_valid());
	CHECK(changed_shader != shared_shader);
	CHECK(storage->owns_shader(shared_shader));
	CHECK(storage->owns_shader(changed_shader));
	CHECK(material_b->get_shader_rid() == changed_shader);

	material_b = Ref<ParticleProcessMaterial>();
	CHECK_FALSE(storage->owns_shader(changed_shader));
	CHECK(storage->owns_shader(shared_shader));
	material_a = Ref<ParticleProcessMaterial>();
	CHECK_FALSE(storage->owns_shader(shared_shader));
}

} // namespace TestParticleProcessMaterial
