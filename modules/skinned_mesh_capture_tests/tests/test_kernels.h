/**************************************************************************/
/*  test_kernels.h                                                        */
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

#pragma once

#include "servers/rendering/rendering_device_commons.h"
#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_kernels.h"

#ifdef MODULE_GLSLANG_ENABLED
Vector<uint8_t> compile_glslang_shader(RenderingDeviceCommons::ShaderStage, const String &, RenderingDeviceCommons::ShaderLanguageVersion, RenderingDeviceCommons::ShaderSpirvVersion, String *);

namespace TestSkinnedMeshCaptureTestKernels {
TEST_CASE("[P01] Fixed compute kernels compile to SPIRV") {
	for (const char *source : { skinned_mesh_capture_test_public_deformation_shader_source(), skinned_mesh_capture_test_transport_shader_source() }) {
		String error;
		const Vector<uint8_t> spirv = compile_glslang_shader(RenderingDeviceCommons::SHADER_STAGE_COMPUTE, source, RenderingDeviceCommons::SHADER_LANGUAGE_VULKAN_VERSION_1_1, RenderingDeviceCommons::SHADER_SPIRV_VERSION_1_3, &error);
		CAPTURE(error);
		CHECK_FALSE(spirv.is_empty());
		CHECK(error.is_empty());
	}
}
} // namespace TestSkinnedMeshCaptureTestKernels
#endif
