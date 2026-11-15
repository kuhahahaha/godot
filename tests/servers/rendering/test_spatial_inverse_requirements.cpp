/**************************************************************************/
/*  test_spatial_inverse_requirements.cpp                                  */
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

#include "servers/rendering/shader_compiler.h"
#include "servers/rendering/shader_types.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_spatial_inverse_requirements)

// Real spatial compiler usage discovery. This does not issue material/profile
// qualification or test backend GPU readiness or owner/selection retention.
TEST_CASE("[Rendering][ShaderCompiler] spatial inverse model demand is exact") {
	REQUIRE(ShaderTypes::get_singleton() != nullptr);
	ShaderCompiler compiler;
	ShaderCompiler::DefaultIdentifierActions defaults;
	defaults.usage_defines["INV_MODEL_MATRIX"] = "#define INV_MODEL_MATRIX_USED\n";
	defaults.usage_defines["ALPHA_HASH_SCALE"] = "#define ALPHA_HASH_USED\n";
	compiler.initialize(defaults);
	struct Row {
		const char *source;
		bool inverse;
		bool hash;
	};
	const Row rows[] = {
		{ "shader_type spatial; void fragment() { ALBEDO = vec3(1.0); }", false, false },
		{ "shader_type spatial; void vertex() { VERTEX = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }", true, false },
		{ "shader_type spatial; void fragment() { ALBEDO = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }", true, false },
		{ "shader_type spatial; void fragment() { ALPHA_HASH_SCALE = 0.5; }", false, true },
		{ "shader_type spatial; void fragment() { ALPHA_SCISSOR_THRESHOLD = 0.5; }", false, false },
		{ "shader_type spatial; void fragment() { ALBEDO = NORMAL; }", false, false },
		{ "shader_type spatial; void fragment() { ALBEDO = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; ALPHA_HASH_SCALE = 0.5; }", true, true },
	};
	for (const Row &row : rows) {
		bool inverse = false;
		bool hash = false;
		ShaderCompiler::IdentifierActions actions;
		actions.entry_point_stages["vertex"] = ShaderCompiler::STAGE_VERTEX;
		actions.entry_point_stages["fragment"] = ShaderCompiler::STAGE_FRAGMENT;
		actions.usage_flag_pointers["INV_MODEL_MATRIX"] = &inverse;
		actions.usage_flag_pointers["ALPHA_HASH_SCALE"] = &hash;
		ShaderCompiler::GeneratedCode generated;
		const Error error = compiler.compile(RSE::SHADER_SPATIAL, row.source, &actions, "inverse_requirement", generated);
		CHECK(error == OK);
		CHECK(inverse == row.inverse);
		CHECK(hash == row.hash);
	}
}
