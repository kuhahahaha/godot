/**************************************************************************/
/*  test_render_data_camera_source.cpp                                    */
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

#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/rendering_server_globals.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_render_data_camera_source)

TEST_CASE("[B4][NativeCameraPropagation] RD retains exact native center and eye source through viewport advance and free") {
	REQUIRE(RSG::viewport != nullptr);
	for (uint32_t eyes = 1; eyes <= 2; eyes++) {
		const RID viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
		RendererViewport::Viewport::ViewPresentationKey key{ 831, 4, 6 };
		REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, key));
		RendererSceneRender::CameraData camera{};
		Projection projections[2];
		projections[0].set_perspective(67, 1.5, 0.2, 700);
		projections[1].set_perspective(68, 1.5, 0.2, 700);
		Transform3D transforms[2] = { Transform3D(Basis(), Vector3(-0.03, 0, 0)), Transform3D(Basis(), Vector3(0.03, 0, 0)) };
		if (eyes == 1) {
			camera.set_camera(transforms[0], projections[0], false, true, Vector2(0.25, -0.5), 8, 0x17);
		} else {
			camera.set_multiview_camera(eyes, transforms, projections, false, true, 0x17);
		}
		camera.source_viewport = viewport; // The real render_camera passes this exact selection.
		const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera);
		REQUIRE(token.is_valid());
		const auto original = camera;
		auto *native_viewport = RSG::viewport->viewport_owner.get_or_null(viewport);
		CHECK(native_viewport->captured_camera.is_null());
		Ref<RendererViewport::Viewport::CameraCapture> pass_source;
		{
			RenderDataRD render_data;
			CHECK(render_data.get_native_camera_source().is_null());
			REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
			if (eyes == 2) {
				auto rewritten_eye = camera;
				rewritten_eye.view_projection[1].columns[0][0] += 1;
				CHECK_FALSE(render_data.retain_native_camera_source(*RSG::viewport, rewritten_eye));
				CHECK(native_viewport->captured_camera.is_null());
			}
			REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
			CHECK_FALSE(render_data.packet_identity.external);
			pass_source = render_data.get_native_camera_source();
			REQUIRE(pass_source.is_valid());
			CHECK(pass_source->get_viewport() == viewport);
			CHECK(pass_source->get_token() == token);
			CHECK(pass_source->get_reference_count() == 3);
			CHECK(pass_source->get_camera_data().view_count == eyes);
			CHECK(pass_source->get_camera_data().main_transform == original.main_transform);
			CHECK(pass_source->get_camera_data().main_projection == original.main_projection);
			for (uint32_t eye = 0; eye < eyes; eye++) {
				CHECK(pass_source->get_camera_data().view_offset[eye] == original.view_offset[eye]);
				CHECK(pass_source->get_camera_data().view_projection[eye] == original.view_projection[eye]);
			}
			camera.main_transform.origin += Vector3(9, 8, 7);
			const auto next = RSG::viewport->viewport_begin_camera_capture(viewport, camera);
			CHECK(next != token);
			CHECK(pass_source->get_reference_count() == 2);
			CHECK(render_data.get_native_camera_source().ptr() == pass_source.ptr());
			CHECK_FALSE(render_data.retain_native_camera_source(*RSG::viewport, camera));
			RSG::viewport->free(viewport);
			CHECK(render_data.get_native_camera_source().ptr() == pass_source.ptr());
			// A repeat refers to the same already retained sample, not a RID lookup.
			CHECK(render_data.retain_native_camera_source(*RSG::viewport, original));
			render_data.retire_packet();
			CHECK(render_data.get_native_camera_source().is_null());
			CHECK_FALSE(render_data.retain_native_camera_source(*RSG::viewport, original));
			CHECK(pass_source->get_camera_data().main_transform == original.main_transform);
		}
		CHECK(pass_source->get_reference_count() == 1);
	}
}

TEST_CASE("[B4][NativeCameraPropagation] scalar camera tuple cannot replace the selected native payload") {
	REQUIRE(RSG::viewport != nullptr);
	const RID viewport = RSG::viewport->viewport_allocate();
	RSG::viewport->viewport_initialize(viewport);
	RendererSceneRender::CameraData camera{};
	Projection projection;
	projection.set_perspective(60, 1.5, 0.1, 100);
	camera.set_camera(Transform3D(), projection, false, false);
	camera.source_viewport = viewport;
	RenderDataRD ordinary;
	CHECK_FALSE(ordinary.retain_native_camera_source(*RSG::viewport, camera));
	CHECK(ordinary.get_native_camera_source().is_null());
	CHECK(ordinary.external_camera_admitted());
	RendererViewport::Viewport::ViewPresentationKey key{ 832, 5, 8 };
	REQUIRE(RSG::viewport->viewport_register_presentation_key(viewport, key));
	const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera);
	REQUIRE(token.is_valid());
	auto *native_viewport = RSG::viewport->viewport_owner.get_or_null(viewport);
	for (uint32_t field = 0; field < 13; field++) {
		auto rewritten = camera;
		switch (field) {
			case 0:
				rewritten.main_transform.origin.x += 1;
				break;
			case 1:
				rewritten.main_projection.columns[0][0] += 1;
				break;
			case 2:
				rewritten.view_offset[0].origin.x += 1;
				break;
			case 3:
				rewritten.view_projection[0].columns[0][0] += 1;
				break;
			case 4:
				rewritten.taa_jitter.x += 1;
				break;
			case 5:
				rewritten.taa_frame_count += 1;
				break;
			case 6:
				rewritten.visible_layers ^= 1;
				break;
			case 7:
				rewritten.is_orthogonal = !rewritten.is_orthogonal;
				break;
			case 8:
				rewritten.vaspect = !rewritten.vaspect;
				break;
			case 9:
				rewritten.view_count = 0;
				break;
			case 10:
				rewritten.view_count = RendererSceneRender::MAX_RENDER_VIEWS + 1;
				break;
			case 11:
				rewritten.source_viewport = RID();
				break;
			case 12:
				rewritten.source_capture_ordinal++;
				break;
		}
		RenderDataRD render_data;
		REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
		CHECK_FALSE(render_data.retain_native_camera_source(*RSG::viewport, rewritten));
		CHECK(render_data.get_native_camera_source().is_null());
		CHECK(native_viewport->captured_camera.is_null()); // refusal did not allocate a retained source.
		CHECK(render_data.packet_identity.camera_source_matches(token.view_id, token.view_epoch, token.capture_ordinal));
	}
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	const auto retained = render_data.get_native_camera_source();
	REQUIRE(retained.is_valid());
	render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal + 1);
	CHECK(render_data.get_native_camera_source().is_null());
	CHECK(retained->get_token() == token);
	RSG::viewport->free(viewport);
}
