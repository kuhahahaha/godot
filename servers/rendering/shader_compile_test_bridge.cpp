/**************************************************************************/
/*  rendering_shader_container.h                                          */
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

#include "shader_compile_test_bridge.h"

#if defined(TESTS_ENABLED) && defined(METAL_ENABLED)
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "drivers/metal/rendering_shader_container_metal.h"

static bool valid_policy_name(const String &p_name) {
	const CharString bytes = p_name.utf8();
	if (bytes.length() > 256) {
		return false;
	}
	Vector<String> parts = p_name.split(":");
	if (parts.size() != 4 || parts[0] != "e06" || parts[1].length() != 36 || parts[2].is_empty() || parts[2].length() > 64 || parts[3].is_empty()) {
		return false;
	}
	for (int i = 0; i < 36; i++) {
		const char32_t c = parts[1][i];
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			if (c != '-') {
				return false;
			}
		} else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
			return false;
		}
	}
	for (int i = 0; i < parts[2].length(); i++) {
		const char32_t c = parts[2][i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
			return false;
		}
	}
	uint64_t attempt = 0;
	for (int i = 0; i < parts[3].length(); i++) {
		const char32_t c = parts[3][i];
		if (c < '0' || c > '9') {
			return false;
		}
		attempt = attempt * 10 + (c - '0');
		if (attempt > UINT32_MAX) {
			return false;
		}
	}
	return attempt > 0;
}

Dictionary RenderingShaderCompileTestBridge::bake_metal_compute(const PackedByteArray &p_spirv, const String &p_shader_name, int64_t p_flags) {
	const MetalDeviceProfile *profile = MetalDeviceProfile::get_profile(MetalDeviceProfile::Platform::macOS, MetalDeviceProfile::GPU::Apple7, MinOsVersion(14, 0, 0));
	Dictionary profile_dict;
	profile_dict["platform"] = uint32_t(profile->platform);
	profile_dict["gpu"] = uint32_t(profile->gpu);
	profile_dict["min_os_version"] = uint32_t(profile->min_os_version);
	Dictionary result;
	result["schema_version"] = 1;
	result["status"] = "Failed";
	result["shader_name"] = p_shader_name;
	result["flags"] = p_flags;
	result["profile"] = profile_dict;
	result["library_size"] = 0;
	result["container_bytes"] = PackedByteArray();
	result["cause"] = "InvalidInput";
	if (!Thread::is_main_thread() || p_spirv.is_empty() || p_spirv.size() > 1048576 || p_spirv.size() % 4 != 0 || !valid_policy_name(p_shader_name) || (p_flags != 0 && p_flags != 1)) {
		return result;
	}
	RenderingShaderContainerFormatMetal format(profile, true);
	Ref<RenderingShaderContainerMetal> container = format.create_container();
	Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stages;
	RenderingDeviceCommons::ShaderStageSPIRVData stage;
	stage.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
	stage.spirv = p_spirv;
	stages.push_back(stage);
	if (!container->validate_test_compute_spirv(p_shader_name, stages)) {
		result["cause"] = "InvalidComputeModule";
		return result;
	}
	for (const String &tool : { String("metal"), String("metal-dsymutil") }) {
		List<String> args{ "-sdk", "macosx", "--find", tool };
		String output;
		int exit_code = -1;
		Error err = OS::get_singleton()->execute("/usr/bin/xcrun", args, &output, &exit_code, true);
		if (err != OK || exit_code != 0 || !FileAccess::exists(output.strip_edges())) {
			result["status"] = "Unavailable";
			result["cause"] = "MissingTool:" + tool;
			return result;
		}
	}
	if (!ShaderCompilePolicy::flags_valid(p_flags, format.supported_compile_flags()) || !container->set_code_from_spirv(p_shader_name, stages, p_flags) || container->is_invalid() || container->mtl_shaders.size() != 1 || container->mtl_shaders[0].library_size == 0) {
		result["cause"] = "OfflineCompileFailed";
		return result;
	}
	const auto &tools = container->get_offline_test_result();
	for (const auto *tool : { &tools.property_probe, &tools.metal, &tools.strip }) {
		if (!tool->succeeded()) {
			result["cause"] = "ToolResultFailed";
			return result;
		}
	}
	PackedByteArray bytes = container->to_bytes();
	if (bytes.is_empty() || bytes.size() > 4194304) {
		result["cause"] = "ContainerSize";
		return result;
	}
	result["status"] = "Baked";
	result["library_size"] = container->mtl_shaders[0].library_size;
	result["container_bytes"] = bytes;
	result["cause"] = "";
	return result;
}

void RenderingShaderCompileTestBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bake_metal_compute", "spirv", "shader_name", "flags"), &RenderingShaderCompileTestBridge::bake_metal_compute);
}
#endif
