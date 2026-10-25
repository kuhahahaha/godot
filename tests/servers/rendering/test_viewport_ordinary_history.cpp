/**************************************************************************/
/*  test_viewport_ordinary_history.cpp                                    */
/**************************************************************************/

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/rendering_server_globals.h"
#include "tests/test_macros.h"
#include "tests/test_tools.h"

TEST_FORCE_LINK(test_viewport_ordinary_history)

TEST_CASE("[ViewportHistory] ordinary two-frame history preserves registered admission") {
	REQUIRE(RSG::viewport != nullptr);
	REQUIRE(RSG::rasterizer != nullptr);
	RID rid = RSG::viewport->viewport_allocate();
	RSG::viewport->viewport_initialize(rid);
	struct Cleanup {
		RID rid;
		~Cleanup() { RSG::viewport->free(rid); }
	} cleanup{ rid };
	ErrorDetector errors;
	RendererSceneRender::CameraData camera{};
	for (int frame = 1; frame <= 2; frame++) {
		RSG::rasterizer->begin_frame(1.0 / 60.0);
		CHECK_FALSE(RSG::viewport->viewport_begin_camera_capture(rid).is_valid());
		const auto *previous = RSG::viewport->viewport_get_prev_camera_data(rid);
		REQUIRE(previous != nullptr);
		CHECK(previous->main_transform.origin.x == double(frame - 1));
		camera.main_transform.origin.x = frame;
		RSG::viewport->viewport_set_prev_camera_data(rid, &camera);
		CHECK(previous->main_transform.origin.x == double(frame));
		CHECK_FALSE(previous->has_source_token());
		CHECK_FALSE(RSG::viewport->viewport_adopt_camera_data(rid));
		// Preserve the original once-per-frame history write policy.
		camera.main_transform.origin.x = 99;
		RSG::viewport->viewport_set_prev_camera_data(rid, &camera);
		CHECK(previous->main_transform.origin.x == double(frame));
	}
	CHECK_FALSE(errors.has_error);
	RendererViewport::Viewport::ViewPresentationKey key;
	key.view_id = 71;
	key.view_epoch = 1;
	CHECK(RSG::viewport->viewport_register_presentation_key(rid, key));
	auto token = RSG::viewport->viewport_begin_camera_capture(rid);
	CHECK(token.is_valid());
	CHECK_FALSE(RSG::viewport->viewport_stage_camera_data(rid, &camera, {}));
	CHECK_FALSE(RSG::viewport->viewport_adopt_camera_data(rid));
	camera.source_view_id = token.view_id;
	camera.source_view_epoch = token.view_epoch;
	camera.source_capture_ordinal = token.capture_ordinal;
	CHECK(RSG::viewport->viewport_stage_camera_data(rid, &camera, token));
	CHECK(RSG::viewport->viewport_adopt_camera_data(rid));
	CHECK(RSG::viewport->viewport_get_previous_camera_token(rid) == token);
}
