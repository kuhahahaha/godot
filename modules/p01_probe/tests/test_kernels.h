#pragma once

#include "modules/modules_enabled.gen.h"
#include "modules/p01_probe/p01_kernels.h"
#include "servers/rendering/rendering_device_commons.h"
#include "tests/test_macros.h"

#ifdef MODULE_GLSLANG_ENABLED
Vector<uint8_t> compile_glslang_shader(RenderingDeviceCommons::ShaderStage, const String &, RenderingDeviceCommons::ShaderLanguageVersion, RenderingDeviceCommons::ShaderSpirvVersion, String *);

namespace TestP01Kernels {
TEST_CASE("[P01] Fixed compute kernels compile to SPIRV") {
	for (const char *source : { p01_public_deformation_shader_source(), p01_transport_shader_source() }) {
		String error;
		const Vector<uint8_t> spirv = compile_glslang_shader(RenderingDeviceCommons::SHADER_STAGE_COMPUTE, source, RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1, RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3, &error);
		CAPTURE(error);
		CHECK_FALSE(spirv.is_empty());
		CHECK(error.is_empty());
	}
}
} // namespace TestP01Kernels
#endif
