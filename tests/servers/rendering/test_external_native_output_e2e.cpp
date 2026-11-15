/**************************************************************************/
/*  test_external_native_output_e2e.cpp                                   */
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

TEST_FORCE_LINK(test_external_native_output_e2e)

namespace TestProductionRequest {
void run_wave_g_native_output_cpu();
} // namespace TestProductionRequest

namespace TestWaveGNativeOutput {

#if defined(MACOS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
static void run_live_metal_subprocess(const char *p_rendering_method) {
	if (OS::get_singleton()->get_environment("GODOT_SKIP_LIVE_METAL_SUBPROCESS") == "1") {
		return;
	}
	const String exe = OS::get_singleton()->get_executable_path();
	REQUIRE(!exe.is_empty());
	const String repo_root = exe.get_base_dir().path_join("..").simplify_path();
	const String fixture_dir = repo_root.path_join("tests/servers/rendering/fixtures/external_native_output_e2e");
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

	String pipe_out;
	int exit_code = -1;
	const Error err = OS::get_singleton()->execute(exe, args, &pipe_out, &exit_code, true);
	INFO(pipe_out);
	CHECK(err == OK);
	CHECK(exit_code == 0);
	CHECK(pipe_out.contains("WAVE_G_NATIVE_OUTPUT_PASS"));
	CHECK(pipe_out.contains("frames_drawn=0"));
}
#endif

TEST_CASE("[SceneTree][WaveGGodot][CPU] PrepareV3 + PlanV1/PlanV2 call-order symmetry, INSTANCE_192 no-motion, PARTICLE_LIGHT_64, and INDEXED_144 validation") {
	TestProductionRequest::run_wave_g_native_output_cpu();
}

#if defined(MACOS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
TEST_CASE("[WaveGGodot][LiveMetal][Mobile] INSTANCE_192 no-motion, PARTICLE_LIGHT_64, and INDEXED_144 two-frame non-reset motion history end-to-end") {
	run_live_metal_subprocess("mobile");
}

TEST_CASE("[WaveGGodot][LiveMetal][ForwardPlus] INSTANCE_192 no-motion, PARTICLE_LIGHT_64, and INDEXED_144 two-frame non-reset motion history end-to-end") {
	run_live_metal_subprocess("forward_plus");
}
#endif

} // namespace TestWaveGNativeOutput
