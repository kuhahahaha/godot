/**************************************************************************/
/*  test_particles_process_retry.cpp                                      */
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

#include "core/os/os.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif

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

	static void restore_history_matrix() {
		const int history_sizes[] = { 0, 1, 2, 4 };
		const int trail_sizes[] = { 0, 1, 2, 3 };
		const double previous_speeds[] = { -1.0, 0.0, 1.0 };
		const double current_speeds[] = { -1.0, 0.0, 1.0 };
		// Expected samples are independent of the helper's proportional indexing.
		const uint32_t expected_samples[4][4][3] = {
			{ {}, { 900 }, { 900, 901 }, { 900, 901, 902 } },
			{ {}, { 10 }, { 10, 10 }, { 10, 10, 10 } },
			{ {}, { 10 }, { 10, 20 }, { 10, 10, 20 } },
			{ {}, { 10 }, { 10, 30 }, { 10, 20, 30 } },
		};

		for (int history_size : history_sizes) {
			for (int trail_size : trail_sizes) {
				for (double previous_speed : previous_speeds) {
					for (double current_speed : current_speeds) {
						ParticlesStorage::Particles particles;
						particles.frame_history.resize(history_size);
						particles.trail_params.resize(trail_size);
						for (int i = 0; i < trail_size; i++) {
							particles.trail_params[i].frame = uint32_t(900 + i);
						}
						for (int i = 0; i < history_size; i++) {
							particles.frame_history[i].frame = uint32_t((i + 1) * 10);
						}
						const ParticlesStorage::ParticlesFrameParams previous_first = history_size > 0 ? particles.frame_history[0] : ParticlesStorage::ParticlesFrameParams();
						const ParticlesStorage::ParticlesFrameParams previous_last = history_size > 1 ? particles.frame_history[history_size - 1] : previous_first;

						if (history_size > 0) {
							for (int i = history_size - 1; i > 0; i--) {
								particles.frame_history[i] = particles.frame_history[i - 1];
							}
							particles.frame_history[0].frame = 99;
						}
						particles.speed_scale = current_speed;
						particles.trail_params_speed_scale = previous_speed;
						ParticlesStorage::_particles_restore_process_history(&particles, previous_first, previous_last, previous_speed);

						CHECK(particles.frame_history.size() == uint32_t(history_size));
						for (int i = 0; i < history_size; i++) {
							CHECK(particles.frame_history[i].frame == uint32_t((i + 1) * 10));
						}
						CHECK(particles.trail_params.size() == uint32_t(trail_size));
						const int history_row = history_size == 4 ? 3 : history_size;
						for (int i = 0; i < trail_size; i++) {
							const uint32_t expected = history_size > 0 && previous_speed <= 0.0 ? 10 : expected_samples[history_row][trail_size][i];
							CHECK(particles.trail_params[i].frame == expected);
						}
					}
				}
			}
		}
	}

	static void commit_state_boundary_matrix() {
		struct SubEmitterState {
			bool emitting;
			bool clear;
			bool inactive;
			double inactive_time;
			bool force_sub_emit;
		};
		const SubEmitterState pending[] = {
			{ false, false, true, 4.0, false },
			{ true, false, true, 4.0, false },
		};
		const SubEmitterState committed[] = {
			{ false, false, false, 0.0, true }, // Was not emitting: leave emitting/clear unchanged.
			{ false, true, false, 0.0, true },
		};
		for (bool success : { false, true }) {
			for (bool was_emitting : { false, true }) {
				const SubEmitterState &previous = pending[was_emitting ? 1 : 0];
				for (int sub_emitter_kind = 0; sub_emitter_kind < 3; sub_emitter_kind++) {
					for (int queue_kind = 0; queue_kind < 3; queue_kind++) {
						CAPTURE(success);
						CAPTURE(was_emitting);
						CAPTURE(sub_emitter_kind);
						CAPTURE(queue_kind);
						ParticlesStorage::Particles particles;
						ParticlesStorage::Particles sub_emitter;
						particles.clear = true;
						particles.force_sub_emit = true;
						if (queue_kind != 0) {
							particles.emission_buffer_data.resize(sizeof(ParticlesStorage::ParticleEmissionBuffer));
							particles.emission_buffer = reinterpret_cast<ParticlesStorage::ParticleEmissionBuffer *>(particles.emission_buffer_data.ptrw());
							particles.emission_buffer->particle_count = queue_kind == 1 ? 0 : 3;
							particles.emission_buffer->particle_max = 7;
						}
						ParticlesStorage::ParticleEmissionBuffer *const previous_queue = particles.emission_buffer;
						// 0: no sub-emitter; 1: present without storage; 2: valid storage.
						if (sub_emitter_kind == 2) {
							sub_emitter.emission_storage_buffer = RID::from_uint64(1);
						}
						sub_emitter.emitting = previous.emitting;
						sub_emitter.clear = previous.clear;
						sub_emitter.inactive = previous.inactive;
						sub_emitter.inactive_time = previous.inactive_time;
						sub_emitter.force_sub_emit = previous.force_sub_emit;

						ParticlesStorage::_particles_commit_process_state(&particles, sub_emitter_kind == 0 ? nullptr : &sub_emitter, was_emitting, success);

						CHECK(particles.clear == !success);
						CHECK(particles.force_sub_emit == !success);
						REQUIRE(particles.emission_buffer == previous_queue);
						if (queue_kind != 0) {
							CHECK(particles.emission_buffer->particle_count == (!success && queue_kind == 2 ? 3 : 0));
							CHECK(particles.emission_buffer->particle_max == 7);
						}
						const SubEmitterState &expected = success && sub_emitter_kind == 2 ? committed[was_emitting ? 1 : 0] : previous;
						CHECK(sub_emitter.emitting == expected.emitting);
						CHECK(sub_emitter.clear == expected.clear);
						CHECK(sub_emitter.inactive == expected.inactive);
						CHECK(sub_emitter.inactive_time == expected.inactive_time);
						CHECK(sub_emitter.force_sub_emit == expected.force_sub_emit);
					}
				}
			}
		}
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

TEST_CASE("[ParticlesProcessRetry] commit covers success and optional emission boundaries") {
	RendererRD::TestParticlesProcessRetryAccess::commit_state_boundary_matrix();
}

TEST_CASE("[ParticlesProcessRetry] rollback restores shifted history without a full snapshot") {
	RendererRD::TestParticlesProcessRetryAccess::restore_shifted_history();
}

TEST_CASE("[ParticlesProcessRetry] history rollback covers empty and trail sampling boundaries") {
	RendererRD::TestParticlesProcessRetryAccess::restore_history_matrix();
}

TEST_CASE("[ParticlesProcessRetryGPU] production buffer upload retry") {
	if (!OS::get_singleton()->has_environment("GODOT_TEST_PARTICLES_PROCESS_RD")) {
		MESSAGE("UNRUN: set GODOT_TEST_PARTICLES_PROCESS_RD=1 to execute the Vulkan production-path test");
		return;
	}
	RendererRD::TestParticlesProcessRetryAccess::run_real_rd_upload_retry();
}
