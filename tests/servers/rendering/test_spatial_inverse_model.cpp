/**************************************************************************/
/*  test_spatial_inverse_model.cpp                                        */
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

#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_spatial_inverse_model)

namespace TestSpatialInverseModel {

static ShaderLanguage::ShaderCompileInfo compile_info(RSE::ShaderMode p_mode) {
	ShaderLanguage::ShaderCompileInfo info;
	info.functions = ShaderTypes::get_singleton()->get_functions(p_mode);
	info.render_modes = ShaderTypes::get_singleton()->get_modes(p_mode);
	info.stencil_modes = ShaderTypes::get_singleton()->get_stencil_modes(p_mode);
	info.shader_types = ShaderTypes::get_singleton()->get_types();
	return info;
}

TEST_CASE("[ShaderLanguage] spatial inverse model is a readonly matrix in vertex and fragment") {
	REQUIRE(ShaderTypes::get_singleton() != nullptr);
	const auto &functions = ShaderTypes::get_singleton()->get_functions(RSE::SHADER_SPATIAL);
	for (const StringName &stage : { StringName("vertex"), StringName("fragment") }) {
		REQUIRE(functions.has(stage));
		const auto &builtins = functions[stage].built_ins;
		REQUIRE(builtins.has("INV_MODEL_MATRIX"));
		CHECK(builtins["INV_MODEL_MATRIX"].type == ShaderLanguage::TYPE_MAT4);
		CHECK(builtins["INV_MODEL_MATRIX"].constant);
	}
	ShaderLanguage parser;
	CHECK(parser.compile("shader_type spatial; void vertex() { VERTEX = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; } void fragment() { ALBEDO = abs((INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz); }", compile_info(RSE::SHADER_SPATIAL)) == OK);
}

TEST_CASE("[ShaderLanguage] inverse model cannot be assigned or borrowed by canvas shaders") {
	REQUIRE(ShaderTypes::get_singleton() != nullptr);
	ShaderLanguage vertex;
	CHECK(vertex.compile("shader_type spatial; void vertex() { INV_MODEL_MATRIX = mat4(1.0); }", compile_info(RSE::SHADER_SPATIAL)) != OK);
	ShaderLanguage fragment;
	CHECK(fragment.compile("shader_type spatial; void fragment() { INV_MODEL_MATRIX = mat4(1.0); }", compile_info(RSE::SHADER_SPATIAL)) != OK);
	ShaderLanguage canvas;
	CHECK(canvas.compile("shader_type canvas_item; void vertex() { VERTEX = (INV_MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy; }", compile_info(RSE::SHADER_CANVAS_ITEM)) != OK);
}

} // namespace TestSpatialInverseModel
