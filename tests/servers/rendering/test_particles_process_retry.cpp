/**************************************************************************/
/*  test_particles_process_retry.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif
#include <cstdlib>

TEST_FORCE_LINK(test_particles_process_retry)

namespace RendererRD {
class TestParticlesProcessRetryAccess {
public:
	static uint32_t fault_hits(RenderingDevice &p_rd) { return p_rd.test_get_buffer_update_fault_hits(); }
	static void arm_fault(RenderingDevice &p_rd, RID p_buffer, uint32_t p_count) { p_rd.test_arm_buffer_update_fault(p_buffer, p_count); }
	static void run_rd_upload_retry(RenderingDevice &p_rd, ParticlesStorage &p_storage, RID p_particle_rid, RID p_sub_emitter_rid) {
		ParticlesStorage::Particles *particles = p_storage.particles_owner.get_or_null(p_particle_rid);
		ParticlesStorage::Particles *sub_emitter = p_storage.particles_owner.get_or_null(p_sub_emitter_rid);
		CHECK(particles != nullptr);
		CHECK(sub_emitter != nullptr);
		if (particles == nullptr || sub_emitter == nullptr) {
			return;
		}
		particles->amount = 4;
		particles->particle_buffer = p_rd.storage_buffer_create(sizeof(ParticlesStorage::ParticleData) * particles->amount);
		particles->frame_params_buffer = p_rd.storage_buffer_create(sizeof(ParticlesStorage::ParticlesFrameParams));
		particles->emission_storage_buffer = p_rd.storage_buffer_create(sizeof(ParticlesStorage::ParticleEmissionBuffer));
		particles->emission_buffer_data.resize(sizeof(ParticlesStorage::ParticleEmissionBuffer));
		particles->emission_buffer = reinterpret_cast<ParticlesStorage::ParticleEmissionBuffer *>(particles->emission_buffer_data.ptrw());
		particles->emission_buffer->particle_count = 1;
		particles->emission_buffer->particle_max = 1;
		particles->frame_history.resize(1);
		particles->trail_params.resize(1);
		particles->phase = 0.25;
		particles->cycle_number = 7;
		particles->frame_counter = 11;
		particles->clear = true;
		particles->force_sub_emit = true;
		particles->has_collision_cache = true;
		particles->collision_3d_textures_used = 2;
		particles->collision_heightmap_texture = RID::from_uint64(42);
		particles->collision_3d_textures[0] = RID::from_uint64(43);
		particles->collision_3d_textures[1] = RID::from_uint64(44);
		particles->emitting = true;
		particles->sub_emitter = p_sub_emitter_rid;

		sub_emitter->amount = 4;
		p_storage._particles_allocate_emission_buffer(sub_emitter);
		CHECK(sub_emitter->emission_buffer != nullptr);
		CHECK(sub_emitter->emission_storage_buffer.is_valid());
		if (sub_emitter->emission_buffer == nullptr || !sub_emitter->emission_storage_buffer.is_valid()) {
			return;
		}
		sub_emitter->emitting = true;
		sub_emitter->clear = false;
		sub_emitter->inactive = true;
		sub_emitter->inactive_time = 4.0;

		const RID upload_targets[] = { sub_emitter->emission_storage_buffer, particles->emission_storage_buffer, particles->frame_params_buffer };
		for (RID target : upload_targets) {
			// Each target is exercised from the same pending frame state. A prior
			// successful target consumes the request, so restore that state before
			// moving to the next independent fault boundary.
			particles->frame_history.resize(1);
			particles->trail_params.resize(1);
			particles->phase = 0.25;
			particles->cycle_number = 7;
			particles->frame_counter = 11;
			particles->clear = true;
			particles->force_sub_emit = true;
			particles->emitting = true;
			particles->has_collision_cache = true;
			particles->collision_3d_textures_used = 2;
			particles->collision_heightmap_texture = RID::from_uint64(42);
			particles->collision_3d_textures[0] = RID::from_uint64(43);
			particles->collision_3d_textures[1] = RID::from_uint64(44);
			particles->emission_buffer->particle_count = 1;
			sub_emitter->emitting = true;
			sub_emitter->clear = false;
			sub_emitter->inactive = true;
			sub_emitter->inactive_time = 4.0;
			for (int failure = 0; failure < 2; failure++) {
				arm_fault(p_rd, target, 1);
				CHECK_FALSE(p_storage._particles_process(particles, 1.0 / 60.0));
				CHECK(fault_hits(p_rd) == 1);
				CHECK(particles->phase == 0.25);
				CHECK(particles->cycle_number == 7);
				CHECK(particles->frame_counter == 11);
				CHECK(particles->clear);
				CHECK(particles->force_sub_emit);
				CHECK(particles->emitting);
				CHECK(particles->emission_buffer->particle_count == 1);
				CHECK(particles->collision_3d_textures_used == 2);
				CHECK(particles->collision_heightmap_texture == RID::from_uint64(42));
				CHECK(particles->collision_3d_textures[0] == RID::from_uint64(43));
				CHECK(particles->collision_3d_textures[1] == RID::from_uint64(44));
				CHECK(sub_emitter->emitting);
				CHECK_FALSE(sub_emitter->clear);
				CHECK(sub_emitter->inactive);
				CHECK(sub_emitter->inactive_time == 4.0);
			}
			arm_fault(p_rd, RID(), 0);
			REQUIRE(p_storage._particles_process(particles, 1.0 / 60.0));
			CHECK(particles->frame_counter == 12);
			CHECK_FALSE(particles->clear);
			CHECK_FALSE(particles->force_sub_emit);
			CHECK(particles->emission_buffer->particle_count == 0);
			CHECK_FALSE(particles->has_collision_cache);
			CHECK_FALSE(sub_emitter->emitting);
			CHECK(sub_emitter->clear);
			CHECK_FALSE(sub_emitter->inactive);
			CHECK(sub_emitter->inactive_time == 0.0);
		}
	}
	static void run_real_rd_upload_retry() {
#ifdef VULKAN_ENABLED
		RenderingDevice *rd = RenderingDevice::get_singleton();
		RendererCompositorRD *compositor = RendererCompositorRD::get_singleton();
		if (rd == nullptr || compositor == nullptr) {
			CHECK_MESSAGE(false, "Production particle retry requested, but this test host has no initialized RenderingDevice/RendererCompositorRD.");
			return;
		}
		ParticlesStorage *storage = static_cast<ParticlesStorage *>(compositor->get_particles_storage());
		CHECK(storage != nullptr);
		if (storage == nullptr) {
			return;
		}
		RID particle = storage->particles_allocate();
		storage->particles_initialize(particle);
		RID sub_emitter = storage->particles_allocate();
		storage->particles_initialize(sub_emitter);
		struct RetryCleanup {
			RenderingDevice &rd;
			ParticlesStorage &storage;
			RID particle;
			RID sub_emitter;

			~RetryCleanup() {
				TestParticlesProcessRetryAccess::arm_fault(rd, RID(), 0);
				if (particle.is_valid()) {
					storage.particles_free(particle);
				}
				if (sub_emitter.is_valid()) {
					storage.particles_free(sub_emitter);
				}
			}
		} cleanup = { *rd, *storage, particle, sub_emitter };
		run_rd_upload_retry(*rd, *storage, particle, sub_emitter);
#else
		FAIL("Vulkan support is required for the production upload retry test.");
#endif
	}

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

		particles.speed_scale = -1.0;
		particles.trail_params_speed_scale = 1.0;
		ParticlesStorage::_particles_restore_process_history(&particles, previous_first, previous_last, particles.trail_params_speed_scale);

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

TEST_CASE("[ParticlesProcessRetryGPU] production buffer upload retry") {
	if (std::getenv("GODOT_TEST_PARTICLES_PROCESS_RD") == nullptr) {
		MESSAGE("UNRUN: set GODOT_TEST_PARTICLES_PROCESS_RD=1 to execute the Vulkan production-path test");
		return;
	}
	RendererRD::TestParticlesProcessRetryAccess::run_real_rd_upload_retry();
}
