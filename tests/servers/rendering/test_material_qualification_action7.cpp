/**************************************************************************/
/*  test_material_qualification_action7.cpp                               */
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

#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_material_qualification_action7)

namespace TestProductionRequest {
void run_material_qualification_action7_cpu();
} // namespace TestProductionRequest

namespace TestMaterialQualificationAction7 {

#if defined(MACOS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
static void run_live_metal_subprocess(const char *p_rendering_method, bool p_prime_ordinary_frame = false, const char *p_transparent = nullptr) {
	if (OS::get_singleton()->get_environment("GODOT_SKIP_LIVE_METAL_SUBPROCESS") == "1") {
		return;
	}
	const String exe = OS::get_singleton()->get_executable_path();
	REQUIRE(!exe.is_empty());
	const String repo_root = exe.get_base_dir().path_join("..").simplify_path();
	const String fixture_dir = repo_root.path_join("tests/servers/rendering/fixtures/material_qualification_action7");
	REQUIRE(DirAccess::dir_exists_absolute(fixture_dir));

	OS::get_singleton()->set_environment("ANGE_MACOS_BACKGROUND_RENDER", "1");

	List<String> args;
	args.push_back("--display-driver");
	args.push_back("macos");
	args.push_back("--rendering-driver");
	args.push_back("metal");
	args.push_back("--rendering-method");
	args.push_back(p_rendering_method);
	args.push_back("--render-thread");
	args.push_back("safe");
	args.push_back("--audio-driver");
	args.push_back("Dummy");
	args.push_back("--path");
	args.push_back(fixture_dir);
	args.push_back("--script");
	args.push_back("res://run.gd");
	if (p_prime_ordinary_frame || p_transparent) {
		args.push_back("--");
		args.push_back(p_transparent ? p_transparent : "--prime-ordinary-frame");
	}

	String pipe_out;
	int exit_code = -1;
	const Error err = OS::get_singleton()->execute(exe, args, &pipe_out, &exit_code, true);
	INFO(pipe_out);
	CHECK(err == OK);
	CHECK(exit_code == 0);
	CHECK(pipe_out.contains("MATERIAL_QUALIFICATION_ACTION7_PASS"));
	CHECK(pipe_out.contains("frames_drawn=0"));
	if (p_transparent) {
		const bool additive = String(p_transparent) == "--transparent-add";
		CHECK(pipe_out.contains(additive ? "transparent_add.rgba_center_blend" : "transparent_mix.rgba_center_blend"));
	}
}
#endif

TEST_CASE("[SceneTree][MaterialQualificationAction7][CPU] ABI layouts, negative prepare/recipe/record/read validations, and QuoteDriver release") {
	TestProductionRequest::run_material_qualification_action7_cpu();
}

#if defined(MACOS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
TEST_CASE("[MaterialQualificationAction7][LiveMetal][Mobile] pre-first-draw end-to-end qualification, 5 source gates, wrong generation, and 4-stage K03 release") {
	run_live_metal_subprocess("mobile");
}

TEST_CASE("[MaterialQualificationAction7][LiveMetal][ForwardPlus] pre-first-draw end-to-end qualification, 5 source gates, wrong generation, and 4-stage K03 release") {
	run_live_metal_subprocess("forward_plus");
}

TEST_CASE("[MaterialQualificationAction7][LiveMetal][Mobile] qualifies after one ordinary frame updates scene atlases") {
	run_live_metal_subprocess("mobile", true);
}

TEST_CASE("[MaterialQualificationAction7][LiveMetal][ForwardPlus] qualifies after one ordinary frame updates scene atlases") {
	run_live_metal_subprocess("forward_plus", true);
}
TEST_CASE("[MaterialQualificationAction7][LiveMetal][Mobile] transparent MIX and ADD preserve depth and blend qualified color") {
	run_live_metal_subprocess("mobile", false, "--transparent-mix");
	run_live_metal_subprocess("mobile", false, "--transparent-add");
}
TEST_CASE("[MaterialQualificationAction7][LiveMetal][ForwardPlus] transparent MIX and ADD preserve depth and blend qualified color") {
	run_live_metal_subprocess("forward_plus", false, "--transparent-mix");
	run_live_metal_subprocess("forward_plus", false, "--transparent-add");
}
#endif

} // namespace TestMaterialQualificationAction7
