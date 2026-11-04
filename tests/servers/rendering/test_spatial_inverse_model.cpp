/**************************************************************************/
/*  test_spatial_inverse_model.cpp                                         */
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
	for (const StringName stage : { StringName("vertex"), StringName("fragment") }) {
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
