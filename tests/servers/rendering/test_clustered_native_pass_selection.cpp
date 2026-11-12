/**************************************************************************/
/*  test_clustered_native_pass_selection.cpp                              */
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

#include "servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_clustered_native_pass_selection)

namespace RendererSceneRenderImplementation {

class TestClusteredNativePassSelection {
public:
	static void pass_versions() {
		using Renderer = RenderForwardClustered;
		using Shader = SceneShaderForwardClustered;
		struct Row {
			Renderer::PassMode mode;
			Shader::PipelineVersion mono;
			Shader::PipelineVersion multiview;
			bool supports_multiview;
		};
		// Independent expectations for all eight original native pass modes.
		const Row rows[] = {
			{ Renderer::PASS_MODE_COLOR, Shader::PIPELINE_VERSION_COLOR_PASS, Shader::PIPELINE_VERSION_COLOR_PASS, true },
			{ Renderer::PASS_MODE_SHADOW, Shader::PIPELINE_VERSION_DEPTH_PASS, Shader::PIPELINE_VERSION_DEPTH_PASS_MULTIVIEW, true },
			{ Renderer::PASS_MODE_SHADOW_DP, Shader::PIPELINE_VERSION_DEPTH_PASS_DP, Shader::PIPELINE_VERSION_MAX, false },
			{ Renderer::PASS_MODE_DEPTH, Shader::PIPELINE_VERSION_DEPTH_PASS, Shader::PIPELINE_VERSION_DEPTH_PASS_MULTIVIEW, true },
			{ Renderer::PASS_MODE_DEPTH_NORMAL_ROUGHNESS, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS_MULTIVIEW, true },
			{ Renderer::PASS_MODE_DEPTH_NORMAL_ROUGHNESS_VOXEL_GI, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS_AND_VOXEL_GI, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS_AND_VOXEL_GI_MULTIVIEW, true },
			{ Renderer::PASS_MODE_DEPTH_MATERIAL, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_MATERIAL, Shader::PIPELINE_VERSION_MAX, false },
			{ Renderer::PASS_MODE_SDF, Shader::PIPELINE_VERSION_DEPTH_PASS_WITH_SDF, Shader::PIPELINE_VERSION_MAX, false },
		};
		for (const Row &row : rows) {
			Renderer::NativePassSelection selected;
			REQUIRE(Renderer::_select_native_pass(row.mode, 0, 1, 71, selected));
			CHECK(selected.pass_mode == row.mode);
			CHECK(selected.color_pass_flags == 0);
			CHECK(selected.version == row.mono);
			CHECK(selected.base_color_pass_flags == 0);
			CHECK(selected.framebuffer_format == 71);
			if (row.supports_multiview) {
				REQUIRE(Renderer::_select_native_pass(row.mode, 0, 2, 82, selected));
				CHECK(selected.version == row.multiview);
				CHECK(selected.framebuffer_format == 82);
			}
		}
		// Original depth-family branching treats counts <= 1 as mono; color
		// multiview comes from its flag, not a synthesized view-count decision.
		Renderer::NativePassSelection selected;
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_DEPTH, 15, 0, 93, selected));
		CHECK(selected.version == Shader::PIPELINE_VERSION_DEPTH_PASS);
		CHECK(selected.base_color_pass_flags == 0);
	}

	static void color_flags() {
		using Renderer = RenderForwardClustered;
		using Shader = SceneShaderForwardClustered;
		struct Row {
			uint32_t flags;
			uint32_t pipeline_flags;
		};
		// Renderer bits T=1/S=2/MV=4/Motion=8 map to pipeline bits
		// T=1/S=2/MV=8/Motion=16. No pass-level lightmap bit (4).
		const Row rows[] = {
			{ 0, 0 },
			{ 1, 1 },
			{ 5, 9 },
			{ 9, 17 },
			{ 2, 2 },
			{ 6, 10 },
			{ 10, 18 },
			{ 4, 8 },
			{ 12, 24 },
			{ 8, 16 },
			{ 14, 26 },
			{ 13, 25 },
		};
		for (const Row &row : rows) {
			for (uint32_t views : { 1u, 2u }) {
				Renderer::NativePassSelection selected;
				REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, row.flags, views, 107, selected));
				CHECK(selected.version == Shader::PIPELINE_VERSION_COLOR_PASS);
				CHECK(selected.color_pass_flags == row.flags);
				CHECK(selected.base_color_pass_flags == row.pipeline_flags);
				CHECK(selected.framebuffer_format == 107);
			}
		}
	}

	static void surface_inclusion() {
		using Renderer = RenderForwardClustered;
		struct Row {
			uint32_t flags;
			uint32_t mask;
			bool included;
		};
		const Row rows[] = {
			{ 0, 0, true },
			{ 0, 1, false },
			{ 5, 0, true },
			{ 5, 1, true },
			{ 5, 2, false },
			{ 5, 4, true },
			{ 5, 8, false },
			{ 5, 10, false },
			{ 10, 1, false },
			{ 10, 2, true },
			{ 10, 4, false },
			{ 10, 8, true },
			{ 10, 6, true },
			{ 10, 16, false },
		};
		for (const Row &row : rows) {
			Renderer::NativePassSelection selected;
			REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, row.flags, 1, 127, selected));
			CHECK(selected.includes_surface(row.mask) == row.included);
		}
		// Depth-family passes do not apply the color-only surface mask.
		Renderer::NativePassSelection selected;
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_SHADOW, 0, 1, 131, selected));
		CHECK(selected.includes_surface(15));
	}

	static void failure_preserves_selection() {
		using Renderer = RenderForwardClustered;
		Renderer::NativePassSelection selected;
		REQUIRE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, 14, 2, 119, selected));
		const auto original = selected;
		auto unchanged = [&]() {
			CHECK(selected.pass_mode == original.pass_mode);
			CHECK(selected.color_pass_flags == original.color_pass_flags);
			CHECK(selected.version == original.version);
			CHECK(selected.base_color_pass_flags == original.base_color_pass_flags);
			CHECK(selected.framebuffer_format == original.framebuffer_format);
		};
		for (uint32_t flags : { 3u, 7u, 11u, 15u, 16u, 0x80000000u }) {
			CHECK_FALSE(Renderer::_select_native_pass(Renderer::PASS_MODE_COLOR, flags, 1, 999, selected));
			unchanged();
		}
		for (auto mode : { Renderer::PASS_MODE_SHADOW_DP, Renderer::PASS_MODE_DEPTH_MATERIAL, Renderer::PASS_MODE_SDF }) {
			CHECK_FALSE(Renderer::_select_native_pass(mode, 0, 2, 999, selected));
			unchanged();
		}
		CHECK_FALSE(Renderer::_select_native_pass(Renderer::PASS_MODE_MAX, 0, 1, 999, selected));
		unchanged();
	}
};

} // namespace RendererSceneRenderImplementation

TEST_CASE("[B4][ClusteredNativePassSelection] all native pass versions use original multiview rules") {
	RendererSceneRenderImplementation::TestClusteredNativePassSelection::pass_versions();
}

TEST_CASE("[B4][ClusteredNativePassSelection] all twelve color combinations preserve native pipeline flags") {
	RendererSceneRenderImplementation::TestClusteredNativePassSelection::color_flags();
}

TEST_CASE("[B4][ClusteredNativePassSelection] frozen native color flags control actual surface inclusion") {
	RendererSceneRenderImplementation::TestClusteredNativePassSelection::surface_inclusion();
}

TEST_CASE("[B4][ClusteredNativePassSelection] invalid native selection leaves the previous output unchanged") {
	RendererSceneRenderImplementation::TestClusteredNativePassSelection::failure_preserves_selection();
}
