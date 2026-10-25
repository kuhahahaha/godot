/**************************************************************************/
/*  test_particles_trail_failure.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_device.h"
#include "tests/test_macros.h"
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif
#include <cstdlib>

TEST_FORCE_LINK(test_particles_trail_failure)

namespace RendererRD {
class TestParticlesTrailAccess {
public:
	static void rejected_allocation() {
		ParticlesStorage::Particles particles;
		particles.test_reject_trail_allocation_once = true;
		ERR_PRINT_OFF;
		bool result = ParticlesStorage::_particles_ensure_trail_uniform(&particles, RID());
		ERR_PRINT_ON;
		CHECK_FALSE(result);
		CHECK_FALSE(particles.test_reject_trail_allocation_once);
		CHECK(particles.unused_trail_storage_buffer.is_null());
		CHECK(particles.trail_bind_pose_uniform_set.is_null());
		CHECK(particles.test_trail_uniform_attempts == 0);
	}
	static void retry(RenderingDevice &rd, RID shader) {
		ParticlesStorage::Particles particles;
		struct Cleanup {
			RenderingDevice &rd;
			ParticlesStorage::Particles &particles;
			~Cleanup() {
				if (rd.uniform_set_is_valid(particles.trail_bind_pose_uniform_set)) {
					rd.free_rid(particles.trail_bind_pose_uniform_set);
				}
				if (particles.unused_trail_storage_buffer.is_valid()) {
					rd.free_rid(particles.unused_trail_storage_buffer);
				}
			}
		} cleanup{ rd, particles };
		particles.test_reject_trail_allocation_once = true;
		ERR_PRINT_OFF;
		bool result = ParticlesStorage::_particles_ensure_trail_uniform(&particles, shader);
		ERR_PRINT_ON;
		CHECK_FALSE(result);
		CHECK(particles.test_trail_uniform_attempts == 0);
		CHECK(particles.unused_trail_storage_buffer.is_null());
		CHECK(particles.trail_bind_pose_uniform_set.is_null());
		REQUIRE(ParticlesStorage::_particles_ensure_trail_uniform(&particles, shader));
		CHECK(particles.unused_trail_storage_buffer.is_valid());
		CHECK(rd.uniform_set_is_valid(particles.trail_bind_pose_uniform_set));
		CHECK(particles.test_trail_uniform_attempts == 1);
		RID buffer = particles.unused_trail_storage_buffer;
		RID uniform = particles.trail_bind_pose_uniform_set;
		CHECK(ParticlesStorage::_particles_ensure_trail_uniform(&particles, shader));
		CHECK(particles.test_trail_uniform_attempts == 1);
		CHECK(particles.unused_trail_storage_buffer == buffer);
		CHECK(particles.trail_bind_pose_uniform_set == uniform);
	}
};
}

TEST_CASE("[ParticlesTrail] rejected allocation stops before uniform creation") {
	RendererRD::TestParticlesTrailAccess::rejected_allocation();
}

TEST_CASE("[ParticlesTrailGPU] real Vulkan allocation retry") {
	if (!std::getenv("GODOT_TEST_PARTICLES_TRAIL_GPU")) {
		MESSAGE("UNRUN: set GODOT_TEST_PARTICLES_TRAIL_GPU=1 to require a real Vulkan device.");
		return;
	}
#ifdef VULKAN_ENABLED
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDevice rd;
	REQUIRE(rd.initialize(&context) == OK);
	String error;
	RenderingDevice::ShaderStageSPIRVData stage;
	stage.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
	stage.spirv = rd.shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_COMPUTE,
			"#version 450\nlayout(local_size_x=1) in; layout(set=2,binding=0,std430) buffer Trail { mat4 pose; }; void main() { pose[0][0] = 1.0; }", RenderingDevice::SHADER_LANGUAGE_GLSL, &error, false);
	REQUIRE_MESSAGE(!stage.spirv.is_empty(), error.utf8().get_data());
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	RID shader = rd.shader_create_from_spirv(stages);
	REQUIRE(shader.is_valid());
	RendererRD::TestParticlesTrailAccess::retry(rd, shader);
	rd.free_rid(shader);
#else
	FAIL("Real Vulkan backend is required for this opt-in test.");
#endif
}
