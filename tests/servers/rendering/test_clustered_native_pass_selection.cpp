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
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_clustered_native_pass_selection)

namespace TestProductionRequest {
void wave_g_settle_transfer_workers(RenderingDevice *p_rd);
} // namespace TestProductionRequest

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

	static Error wave_g_draw_external(
			const RendererRD::ExternalSurfaceSelection &p_sel,
			const RenderDataRD *p_render_data,
			RID p_material,
			uint32_t p_profile,
			bool p_has_capacity,
			RD::FramebufferFormatID p_fb_format,
			RID p_fb,
			String &r_step,
			bool p_motion_vectors = false) {
		using namespace RendererRD;
		auto *clu = RenderForwardClustered::get_singleton();
		auto *ps = ParticlesStorage::get_singleton();
		auto *mats = MaterialStorage::get_singleton();
		auto *rd = RD::get_singleton();
		if (!clu || !ps || !mats || !rd) {
			r_step = "clustered_singletons";
			return ERR_UNAVAILABLE;
		}
		Error err = clu->ensure_native_material_recipe_sources_ready();
		if (err != OK) {
			r_step = "clustered_ensure_recipe_sources";
			return err;
		}
		if (p_motion_vectors) {
			clu->scene_shader.enable_advanced_shader_group(false);
		}
		mats->_update_queued_materials();
		auto *mat_data = static_cast<SceneShaderForwardClustered::MaterialData *>(mats->material_get_data(p_material, MaterialStorage::SHADER_TYPE_3D));
		if (!mat_data || !mat_data->shader_data) {
			r_step = "clustered_material_data";
			return ERR_UNAVAILABLE;
		}
		const uint32_t color_flags = p_motion_vectors ? uint32_t(SceneShaderForwardClustered::PIPELINE_COLOR_PASS_FLAG_MOTION_VECTORS) : 0u;
		const RID shader_var = mat_data->shader_data->get_shader_variant(SceneShaderForwardClustered::PIPELINE_VERSION_COLOR_PASS, color_flags, false, p_profile == 2, p_profile == 1);
		if (shader_var.is_null() || !mat_data->shader_data->is_valid()) {
			r_step = "clustered_get_shader_variant";
			return ERR_UNAVAILABLE;
		}
		err = mats->material_settle_native_uploads(p_material);
		if (err != OK) {
			r_step = "clustered_settle_material_uploads";
			return err;
		}

		ExternalPacketPipelineRequest pipe_req;
		pipe_req.backend = ExternalPacketPipelineRequest::Backend::CLUSTERED;
		pipe_req.pass = p_motion_vectors ? ExternalPacketPass::MOTION : ExternalPacketPass::OPAQUE;
		pipe_req.color_flags = color_flags;
		pipe_req.version = SceneShaderForwardClustered::PIPELINE_VERSION_COLOR_PASS;
		pipe_req.framebuffer_format = p_fb_format;
		pipe_req.view_count = 1;

		Error set_err = OK;
		clu->_update_render_base_uniform_set(&set_err, nullptr);
		if (set_err != OK) {
			r_step = "clustered_base_uniform_set";
			return set_err;
		}
		const RID base_set = clu->render_base_uniform_set;
		const auto &samplers = mats->samplers_rd_get_default();
		Vector<RD::Uniform> pass_uniforms;
		(void)clu->_setup_render_pass_uniform_set(RenderForwardClustered::RENDER_LIST_OPAQUE, nullptr, RID(), samplers, 0u, false, &set_err, &pass_uniforms);
		if (set_err != OK) {
			r_step = "clustered_pass_uniform_sources";
			return set_err;
		}
		{
			static uint64_t s_uploaded_frame[3] = { UINT64_MAX, UINT64_MAX, UINT64_MAX };
			static RID s_uploaded_rid[3];
			const uint64_t cur_frame = rd->get_frames_drawn();
			Vector<uint8_t> fixed_scene, fixed_instance, fixed_impl, fixed_cluster, fixed_push;
			if (RenderSceneDataRD::build_native_material_fixed_scene_data(fixed_scene) == OK &&
					clu->build_native_material_fixed_pass_data(fixed_instance, fixed_impl, fixed_cluster, fixed_push) == OK) {
				for (const RD::Uniform &u : pass_uniforms) {
					if (u.get_id_count() == 1 && u.get_id(0).is_valid() && u.binding < 3) {
						const RID buf_rid = u.get_id(0);
						if (s_uploaded_frame[u.binding] == cur_frame && s_uploaded_rid[u.binding] == buf_rid) {
							continue;
						}
						if (u.binding == 0 && !fixed_scene.is_empty()) {
							(void)rd->buffer_update(buf_rid, 0, fixed_scene.size(), fixed_scene.ptr());
							s_uploaded_frame[0] = cur_frame;
							s_uploaded_rid[0] = buf_rid;
						} else if (u.binding == 1 && !fixed_impl.is_empty()) {
							(void)rd->buffer_update(buf_rid, 0, fixed_impl.size(), fixed_impl.ptr());
							s_uploaded_frame[1] = cur_frame;
							s_uploaded_rid[1] = buf_rid;
						} else if (u.binding == 2 && !fixed_instance.is_empty()) {
							(void)rd->buffer_update(buf_rid, 0, fixed_instance.size(), fixed_instance.ptr());
							s_uploaded_frame[2] = cur_frame;
							s_uploaded_rid[2] = buf_rid;
						}
					}
				}
			}
		}
		const RID pass_set = clu->_setup_render_pass_uniform_set(RenderForwardClustered::RENDER_LIST_OPAQUE, nullptr, RID(), samplers, 0u, false, &set_err, nullptr);
		if (set_err != OK) {
			r_step = "clustered_pass_uniform_set";
			return set_err;
		}

		if (p_has_capacity) {
			ExternalPacketViewLease view;
			err = ps->particles_external_view_acquire(p_sel, p_render_data, view);
			if (err != OK) {
				r_step = "clustered_pre_list_view_acquire";
				return err;
			}
			const ExternalPipelinePreparation *prep_row = nullptr;
			err = clu->external_pipeline_prepare_for_view(p_sel, view, pipe_req, prep_row, true);
			const Error vr = view.release_checked();
			if (err != OK || vr != OK) {
				r_step = "clustered_pre_list_pipeline_prepare";
				return err != OK ? err : vr;
			}
		}

		TestProductionRequest::wave_g_settle_transfer_workers(rd);

		const uint32_t push_words[4] = {};
		Vector<Color> clear_colors;
		const uint32_t color_count = p_motion_vectors ? 3u : 1u;
		for (uint32_t c = 0; c < color_count; ++c) {
			clear_colors.push_back(Color(0, 0, 0, 1));
		}
		RD::DrawListID draw_list = rd->draw_list_begin(p_fb, RD::DRAW_CLEAR_ALL, clear_colors, 0.0f, 0);
		if (base_set.is_valid()) {
			rd->draw_list_bind_uniform_set(draw_list, base_set, 0);
		}
		if (pass_set.is_valid()) {
			rd->draw_list_bind_uniform_set(draw_list, pass_set, 1);
		}
		err = ps->particles_external_draw(
				p_sel,
				p_render_data,
				draw_list,
				pipe_req,
				+[](void *r, const ExternalSurfaceSelection &s, const ExternalPacketViewLease &v, const ExternalPacketPipelineRequest &req, const ExternalPipelinePreparation *&o) {
					return static_cast<RenderForwardClustered *>(r)->external_pipeline_prepare_for_view(s, v, req, o);
				},
				clu,
				+[](void *ctx, const ExternalPacketBinding &, const void *&out_push, uint32_t &out_bytes) {
					out_push = ctx;
					out_bytes = sizeof(uint32_t) * 4u;
					return OK;
				},
				const_cast<uint32_t *>(push_words),
				ParticlesStorage::ExternalDrawPreparation::PREPARED_ONLY);
		rd->draw_list_end();
		if (err != OK) {
			r_step = "clustered_particles_external_draw";
			return err;
		}
		r_step = "ok";
		return OK;
	}

	static void external_indexed144_mirror_reverse_cull() {
		using Renderer = RenderForwardClustered;
		Renderer::GeometryInstanceForwardClustered owner;
		Renderer::GeometryInstanceSurfaceDataCache surface;
		surface.owner = &owner;
		Renderer::RenderListParameters params(nullptr, nullptr, 0, false, Renderer::PASS_MODE_COLOR, 0, true, false, RID());
		params.native_pass.pass_mode = Renderer::PASS_MODE_COLOR;
		params.view_count = 1;
		Renderer::NativeSurfacePipelineSelection selected;

		// INDEXED_144 (ribbons) must XOR host owner->mirror with params.reverse_cull:
		surface.external_surface.profile = RendererRD::ExternalOutputConfig::INDEXED_144;
		for (bool mirror : { false, true }) {
			for (bool reverse_cull : { false, true }) {
				owner.mirror = mirror;
				params.reverse_cull = reverse_cull;
				const auto req = Renderer::_external_pipeline_request(params, surface, selected);
				CHECK(req.reverse_cull == (mirror != reverse_cull));
			}
		}

		// INSTANCE_192 keeps params.reverse_cull unchanged regardless of owner->mirror:
		surface.external_surface.profile = RendererRD::ExternalOutputConfig::INSTANCE_192;
		for (bool mirror : { false, true }) {
			for (bool reverse_cull : { false, true }) {
				owner.mirror = mirror;
				params.reverse_cull = reverse_cull;
				const auto req = Renderer::_external_pipeline_request(params, surface, selected);
				CHECK(req.reverse_cull == reverse_cull);
			}
		}
	}
};

} // namespace RendererSceneRenderImplementation

namespace TestProductionRequest {
Error wave_g_clustered_draw_external(
		const RendererRD::ExternalSurfaceSelection &p_sel,
		const RenderDataRD *p_render_data,
		RID p_material,
		uint32_t p_profile,
		bool p_has_capacity,
		RD::FramebufferFormatID p_fb_format,
		RID p_fb,
		String &r_step,
		bool p_motion_vectors) {
	return RendererSceneRenderImplementation::TestClusteredNativePassSelection::wave_g_draw_external(
			p_sel, p_render_data, p_material, p_profile, p_has_capacity, p_fb_format, p_fb, r_step, p_motion_vectors);
}
} // namespace TestProductionRequest

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

TEST_CASE("[ExternalOutput][ClusteredNativePassSelection] BUG-47 INDEXED_144 reverses hardware face culling on mirrored host scale") {
	RendererSceneRenderImplementation::TestClusteredNativePassSelection::external_indexed144_mirror_reverse_cull();
}
