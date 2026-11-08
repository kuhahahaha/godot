/**************************************************************************/
/*  test_mobile_native_pass_selection.cpp                                 */
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

#include "servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_mobile_native_pass_selection)

namespace RendererSceneRenderImplementation {

class TestMobileNativePassSelection {
public:
	static void non_color_versions() {
		using Renderer = RenderForwardMobile;
		using Shader = SceneShaderForwardMobile;
		struct Row {
			Renderer::PassMode mode;
			Shader::ShaderVersion mono;
			Shader::ShaderVersion multiview;
			bool supports_multiview;
		};
		const Row rows[] = {
			{ Renderer::PASS_MODE_SHADOW, Shader::SHADER_VERSION_SHADOW_PASS, Shader::SHADER_VERSION_SHADOW_PASS_MULTIVIEW, true },
			{ Renderer::PASS_MODE_SHADOW_DP, Shader::SHADER_VERSION_SHADOW_PASS_DP, Shader::SHADER_VERSION_MAX, false },
			{ Renderer::PASS_MODE_DEPTH_MATERIAL, Shader::SHADER_VERSION_DEPTH_PASS_WITH_MATERIAL, Shader::SHADER_VERSION_MAX, false },
			// The original motion pass always uses this version, including mono.
			{ Renderer::PASS_MODE_MOTION_VECTORS, Shader::SHADER_VERSION_MOTION_VECTORS_MULTIVIEW, Shader::SHADER_VERSION_MOTION_VECTORS_MULTIVIEW, true },
		};
		for (const Row &row : rows) {
			for (uint32_t views : { 0u, 1u, 2u }) {
				for (bool defer_fog : { false, true }) {
					Renderer::NativePassSelection selected;
					const bool supported = views <= 1 || row.supports_multiview;
					CHECK(Renderer::_select_native_pass(row.mode, views, defer_fog, 73, 3, selected) == supported);
					if (!supported) {
						continue;
					}
					CHECK(selected.pass_mode == row.mode);
					const auto expected = views > 1 ? row.multiview : row.mono;
					CHECK(selected.version == expected);
					CHECK(selected.lightmap_version == expected);
					CHECK(selected.version_for_surface(false) == expected);
					CHECK(selected.version_for_surface(true) == expected);
					CHECK(selected.framebuffer_format == 73);
					CHECK(selected.subpass == 3);
				}
			}
		}
	}

	static void color_versions() {
		using Renderer = RenderForwardMobile;
		using Shader = SceneShaderForwardMobile;
		struct Row {
			uint32_t views;
			bool defer_fog;
			Shader::ShaderVersion version;
			Shader::ShaderVersion lightmap;
		};
		const Row rows[] = {
			{ 0, false, Shader::SHADER_VERSION_COLOR_PASS, Shader::SHADER_VERSION_LIGHTMAP_COLOR_PASS },
			{ 1, false, Shader::SHADER_VERSION_COLOR_PASS, Shader::SHADER_VERSION_LIGHTMAP_COLOR_PASS },
			{ 2, false, Shader::SHADER_VERSION_COLOR_PASS_MULTIVIEW, Shader::SHADER_VERSION_LIGHTMAP_COLOR_PASS_MULTIVIEW },
			{ 0, true, Shader::SHADER_VERSION_COLOR_FOG_MASK, Shader::SHADER_VERSION_LIGHTMAP_FOG_MASK },
			{ 1, true, Shader::SHADER_VERSION_COLOR_FOG_MASK, Shader::SHADER_VERSION_LIGHTMAP_FOG_MASK },
			{ 2, true, Shader::SHADER_VERSION_COLOR_FOG_MASK_MULTIVIEW, Shader::SHADER_VERSION_LIGHTMAP_FOG_MASK_MULTIVIEW },
		};
		for (auto mode : { Renderer::PASS_MODE_COLOR, Renderer::PASS_MODE_COLOR_TRANSPARENT }) {
			for (const Row &row : rows) {
				Renderer::NativePassSelection selected;
				REQUIRE(Renderer::_select_native_pass(mode, row.views, row.defer_fog, 101, 0, selected));
				CHECK(selected.pass_mode == mode);
				CHECK(selected.version == row.version);
				CHECK(selected.lightmap_version == row.lightmap);
				CHECK(selected.version_for_surface(false) == row.version);
				CHECK(selected.version_for_surface(true) == row.lightmap);
				CHECK(selected.framebuffer_format == 101);
				CHECK(selected.subpass == 0);
			}
		}
	}

	static void native_subpass_inputs() {
		using Renderer = RenderForwardMobile;
		using Shader = SceneShaderForwardMobile;
		Renderer::NativePassSelection opaque, merged_alpha, separate_alpha, uv2;
		// Opaque fog-mask output uses pass 0. The same container's merged
		// alpha follows its existing optional fog switch and keeps regular color.
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, 2, true, 107, 0, opaque));
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR_TRANSPARENT, 2, false, 107, 1, merged_alpha));
		CHECK(opaque.version == Shader::SHADER_VERSION_COLOR_FOG_MASK_MULTIVIEW);
		CHECK(opaque.subpass == 0);
		CHECK(merged_alpha.version == Shader::SHADER_VERSION_COLOR_PASS_MULTIVIEW);
		CHECK(merged_alpha.subpass == 1);
		// Separate alpha receives the original pre-begin getter's value; the
		// selection must neither normalize it to 0 nor borrow the opaque FB.
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, 2, false, 109, 5, separate_alpha));
		CHECK(separate_alpha.framebuffer_format == 109);
		CHECK(separate_alpha.subpass == 5);
		CHECK(separate_alpha.version == Shader::SHADER_VERSION_COLOR_PASS_MULTIVIEW);
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_DEPTH_MATERIAL, 1, false, 113, 0, uv2));
		CHECK(uv2.version == Shader::SHADER_VERSION_DEPTH_PASS_WITH_MATERIAL);
		CHECK(uv2.lightmap_version == Shader::SHADER_VERSION_DEPTH_PASS_WITH_MATERIAL);
		CHECK(uv2.framebuffer_format == 113);
		CHECK(uv2.subpass == 0);
	}

	static void native_surface_pipeline() {
		using Renderer = RenderForwardMobile;
		using Shader = SceneShaderForwardMobile;
		// Opaque tokens are only identities for this CPU selection test. No
		// shader method, native allocation or material use is performed here.
		uint64_t identities[5] = {};
		Renderer::GeometryInstanceForwardMobile instance;
		instance.instance_count = 4;
		instance.flags_cache = Renderer::INSTANCE_DATA_FLAG_MULTIMESH | Renderer::INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D |
				Renderer::INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR | Renderer::INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA;
		instance.use_projector = true;
		instance.use_soft_shadow = true;
		instance.omni_light_count = 3;
		instance.spot_light_count = 1;
		instance.area_light_count = 0;
		instance.reflection_probe_count = 2;
		instance.decals_count = 1;
		Renderer::GeometryInstanceSurfaceDataCache surface;
		surface.owner = &instance;
		surface.primitive = RSE::PRIMITIVE_TRIANGLES;
		surface.surface = &identities[0];
		surface.surface_shadow = &identities[1];
		surface.shader = reinterpret_cast<Shader::ShaderData *>(&identities[2]);
		surface.shader_shadow = reinterpret_cast<Shader::ShaderData *>(&identities[3]);
		surface.material = reinterpret_cast<Shader::MaterialData *>(&identities[4]);
		surface.material_uniform_set = RID::from_uint64(211);
		surface.material_uniform_set_shadow = RID::from_uint64(223);
		Renderer::GeometryInstanceSurfaceDataCache *elements[] = { &surface };
		Renderer::RenderElementInfo info{};
		Shader::ShaderSpecialization base = {};
		base.packed_0 = 0x24; // Directional soft shadow and disabled fog survive.
		base.packed_1 = 0x200011; // Directional light count and sample count survive.
		base.packed_2 = 2.5f;
		Renderer::RenderListParameters params(elements, &info, 1, false, Renderer::PASS_MODE_COLOR, RID(), base, true);
		Renderer::NativeSurfacePipelineOverride override;
		Renderer::NativeSurfacePipelineSelection selected;
		for (auto mode : { Renderer::PASS_MODE_COLOR, Renderer::PASS_MODE_COLOR_TRANSPARENT }) {
			params.pass_mode = mode;
			REQUIRE(Renderer::_select_native_pass(mode, 1, false, 307, 2, params.native_pass));
			for (bool lightmap : { false, true }) {
				info.uses_lightmap = lightmap;
				REQUIRE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
				CHECK(selected.shader == surface.shader);
				CHECK(selected.mesh_surface == surface.surface);
				CHECK(selected.used_material == surface.material);
				CHECK(selected.material_uniform_set == RID::from_uint64(211));
				CHECK(selected.base_key.version == (lightmap ? Shader::SHADER_VERSION_LIGHTMAP_COLOR_PASS : Shader::SHADER_VERSION_COLOR_PASS));
				CHECK(selected.base_key.framebuffer_format_id == 307);
				CHECK(selected.base_key.render_pass == 2);
				CHECK(selected.base_key.primitive_type == RSE::PRIMITIVE_TRIANGLES);
				CHECK(selected.base_key.wireframe);
				CHECK_FALSE(selected.base_key.ubershader);
				CHECK(selected.base_key.shader_specialization.packed_0 == 0xF027);
				CHECK(selected.base_key.shader_specialization.packed_1 == 0x686011);
				CHECK(selected.base_key.shader_specialization.packed_2 == 2.5f);
			}
		}
		for (bool mirror : { false, true }) {
			instance.mirror = mirror;
			for (bool reverse : { false, true }) {
				params.reverse_cull = reverse;
				REQUIRE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
				CHECK(selected.cull_variant == (mirror != reverse ? Shader::ShaderData::CULL_VARIANT_REVERSED : Shader::ShaderData::CULL_VARIANT_NORMAL));
			}
		}
		override.shader = surface.shader; // Explicit debug override is ignored by shadow.
		override.material_uniform_set = RID::from_uint64(227);
		for (auto mode : { Renderer::PASS_MODE_SHADOW, Renderer::PASS_MODE_SHADOW_DP }) {
			params.pass_mode = mode;
			REQUIRE(Renderer::_select_native_pass(mode, 1, false, 311, 0, params.native_pass));
			for (bool double_sided : { false, true }) {
				surface.flags = double_sided ? Renderer::GeometryInstanceSurfaceDataCache::FLAG_USES_DOUBLE_SIDED_SHADOWS : 0;
				REQUIRE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
				CHECK(selected.shader == surface.shader_shadow);
				CHECK(selected.mesh_surface == surface.surface_shadow);
				CHECK(selected.used_material == nullptr);
				CHECK(selected.material_uniform_set == RID::from_uint64(223));
				CHECK(selected.base_key.version == (mode == Renderer::PASS_MODE_SHADOW ? Shader::SHADER_VERSION_SHADOW_PASS : Shader::SHADER_VERSION_SHADOW_PASS_DP));
				CHECK(selected.cull_variant == (double_sided ? Shader::ShaderData::CULL_VARIANT_DOUBLE_SIDED : Shader::ShaderData::CULL_VARIANT_NORMAL));
				CHECK(selected.base_key.shader_specialization.packed_0 == 0xF024);
				CHECK(selected.base_key.shader_specialization.packed_1 == 0x200011);
				CHECK(selected.base_key.shader_specialization.packed_2 == 2.5f);
			}
		}
		params.pass_mode = Renderer::PASS_MODE_DEPTH_MATERIAL;
		REQUIRE(Renderer::_select_native_pass(params.pass_mode, 1, false, 313, 0, params.native_pass));
		REQUIRE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
		CHECK(selected.cull_variant == Shader::ShaderData::CULL_VARIANT_DOUBLE_SIDED);
		CHECK(selected.used_material == nullptr);
		CHECK(selected.material_uniform_set == RID::from_uint64(227));
		CHECK(selected.mesh_surface == surface.surface);
		override = {};
		surface.surface = nullptr;
		REQUIRE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
		CHECK(selected.mesh_surface == nullptr);
		CHECK(selected.used_material == surface.material); // Existing draw still marks it used before skipping.
		instance.instance_count = 0;
		CHECK_FALSE(Renderer::_select_native_surface_pipeline(params, 0, override, selected));
		CHECK(selected.base_key.framebuffer_format_id == 313);
		CHECK(selected.used_material == surface.material);
	}

	static void failure_preserves_output() {
		using Renderer = RenderForwardMobile;
		Renderer::NativePassSelection selected;
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, 2, true, 127, 7, selected));
		const auto original = selected;
		auto unchanged = [&]() {
			CHECK(selected.pass_mode == original.pass_mode);
			CHECK(selected.version == original.version);
			CHECK(selected.lightmap_version == original.lightmap_version);
			CHECK(selected.framebuffer_format == original.framebuffer_format);
			CHECK(selected.subpass == original.subpass);
		};
		for (auto mode : { Renderer::PASS_MODE_SHADOW_DP, Renderer::PASS_MODE_DEPTH_MATERIAL }) {
			CHECK_FALSE(Renderer::_select_native_pass(mode, 2, false, 999, 9, selected));
			unchanged();
		}
		CHECK_FALSE(Renderer::_select_native_pass(static_cast<Renderer::PassMode>(6), 1, false, 999, 9, selected));
		unchanged();
	}
};

} // namespace RendererSceneRenderImplementation

TEST_CASE("[B4][MobileNativePassSelection] native depth and motion versions preserve original view rules") {
	RendererSceneRenderImplementation::TestMobileNativePassSelection::non_color_versions();
}

TEST_CASE("[B4][MobileNativePassSelection] color and lightmap versions preserve fog and multiview combinations") {
	RendererSceneRenderImplementation::TestMobileNativePassSelection::color_versions();
}

TEST_CASE("[B4][MobileNativePassSelection] opaque merged alpha separate alpha and UV2 retain pass inputs") {
	RendererSceneRenderImplementation::TestMobileNativePassSelection::native_subpass_inputs();
}

TEST_CASE("[B4][MobileNativePassSelection] unsupported selection preserves the prior output") {
	RendererSceneRenderImplementation::TestMobileNativePassSelection::failure_preserves_output();
}

TEST_CASE("[B4][MobileNativePassSelection][MobileNativeSurfacePipeline] actual surface selector preserves material source cull and specialization") {
	RendererSceneRenderImplementation::TestMobileNativePassSelection::native_surface_pipeline();
}
