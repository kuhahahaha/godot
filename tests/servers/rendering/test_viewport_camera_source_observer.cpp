/**************************************************************************/
/*  test_viewport_camera_source_observer.cpp                                       */
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
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/rendering_method.h"
#include "servers/rendering/rendering_server_globals.h"
#include "tests/test_macros.h"

namespace {
using Observer = RendererViewport::Viewport::CameraSourceObserver;
using Capture = RendererViewport::Viewport::CameraCapture;
struct ObserverState {
	int retained = 0;
	int closed = 0;
	int destroyed = 0;
	bool detach_on_retained = false;
	bool detached = false;
	bool alive_after_detach = false;
	bool free_on_closed = false;
	bool resize_on_closed = false;
	RID free_other_on_closed;
	bool other_closed_free_result = false;
	bool redraw_on_closed = false;
	bool closed_free_result = false;
	bool free_on_retained = false;
	bool retained_free_result = false;
	bool enqueue_on_retained = false;
	bool enqueued = false;
	int nested_drain_retained = 0;
	Ref<Capture> last;
};
class TestCameraSourceObserver : public Observer {
	ObserverState &state;
	RID viewport;

public:
	TestCameraSourceObserver(ObserverState &p_state, RID p_viewport) : state(p_state), viewport(p_viewport) {}
	~TestCameraSourceObserver() { state.destroyed++; }
	void camera_source_retained(const Ref<Capture> &p_capture) override {
		state.retained++;
		state.last = p_capture;
		if (state.free_on_retained) {
			state.retained_free_result = RSG::viewport->free(viewport);
			state.alive_after_detach = state.destroyed == 0;
		} else if (state.enqueue_on_retained) {
			state.enqueue_on_retained = false;
			CHECK(p_capture.is_valid());
			if (p_capture.is_null()) {
				return;
			}
			auto camera = p_capture->get_camera_data();
			const auto token = RSG::viewport->viewport_begin_camera_capture(viewport, camera);
			RenderDataRD render_data;
			if (render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal) && render_data.retain_native_camera_source(*RSG::viewport, camera)) {
				state.enqueued = RSG::viewport->viewport_notify_camera_source_retained(viewport, render_data.get_native_camera_source());
			}
			RSG::viewport->drain_camera_source_observers();
			state.nested_drain_retained = state.retained;
		}
		if (state.detach_on_retained) {
			state.detached = RSG::viewport->viewport_unbind_camera_source_observer(viewport, Ref<Observer>(this));
			state.alive_after_detach = state.destroyed == 0;
		}
	}
	void camera_source_closed() override {
		state.closed++;
		if (state.free_other_on_closed.is_valid()) {
			state.other_closed_free_result = RSG::viewport->free(state.free_other_on_closed);
		}
		if (state.free_on_closed) {
			state.closed_free_result = RSG::viewport->free(viewport);
		} else if (state.resize_on_closed) {
			RSG::viewport->viewport_set_size(viewport, 47, 39, 1);
		}
		if (state.redraw_on_closed) {
			RSG::viewport->draw_viewports(false);
		}
	}
};
struct ObserverViewport {
	RID viewport;
	RendererViewport::Viewport::ViewPresentationKey key{ 19450, 3, 8 };
	ObserverViewport() {
		viewport = RSG::viewport->viewport_allocate();
		RSG::viewport->viewport_initialize(viewport);
	}
	~ObserverViewport() {
		if (viewport.is_valid() && RSG::viewport->viewport_owner.owns(viewport)) {
			RSG::viewport->free(viewport);
		}
	}
	RendererSceneRender::CameraData camera() const {
		RendererSceneRender::CameraData data{};
		data.set_camera(Transform3D(), Projection(), false, false);
		data.source_viewport = viewport;
		return data;
	}
};
} // namespace

TEST_FORCE_LINK(test_viewport_camera_source_observer)

TEST_CASE("[B4][CameraSourceObserver] exact owner registration and RenderData notification") {
	ObserverState state, foreign_state;
	ObserverViewport fixture;
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	Ref<Observer> foreign(memnew(TestCameraSourceObserver(foreign_state, fixture.viewport)));
	CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, Ref<Observer>()));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	CHECK(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, foreign));
	CHECK_FALSE(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, foreign));
	CHECK(state.retained == 0);
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	REQUIRE(token.is_valid());
	Ref<Capture> capture;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, capture));
	// Pre-cull begin and lazy source borrow do not publish the first token.
	CHECK(state.retained == 0);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	auto altered = camera;
	altered.main_transform.origin.x += 1;
	CHECK_FALSE(render_data.retain_native_camera_source(*RSG::viewport, altered));
	CHECK(render_data.get_native_camera_source().is_null());
	CHECK(state.retained == 0);
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	CHECK(state.retained == 0);
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
	CHECK(state.retained == 0);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 1);
	CHECK(state.last == capture);
	REQUIRE(state.last.is_valid());
	if (state.last.is_null()) {
		return;
	}
	CHECK(state.last->get_token() == token);
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(state.retained == 1);
	CHECK(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(state.retained == 1);
	CHECK(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, observer));
	CHECK(state.closed == 1);
	CHECK_FALSE(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, observer));
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(foreign_state.retained == 0);
	CHECK(foreign_state.closed == 0);
}

TEST_CASE("[B4][CameraSourceObserver] epoch resource and free close immediately") {
	ObserverState state;
	ObserverViewport fixture;
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	SUBCASE("same epoch presentation progress preserves association but refuses old sequence") {
		auto camera = fixture.camera();
		const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
		Ref<Capture> capture;
		REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, capture));
		fixture.key.present_seq++;
		REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
		CHECK(state.closed == 0);
		CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
		camera = fixture.camera();
		const auto next = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
		REQUIRE(next.is_valid());
		RenderDataRD render_data;
		REQUIRE(render_data.set_camera_source(next.view_id, next.view_epoch, next.capture_ordinal));
		REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
		CHECK(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
		CHECK(state.retained == 0);
		RSG::viewport->drain_camera_source_observers();
		CHECK(state.retained == 1);
		REQUIRE(state.last.is_valid());
		if (state.last.is_null()) {
			return;
		}
		CHECK(state.last->get_presentation_key().present_seq == fixture.key.present_seq);
	}
	SUBCASE("epoch replacement") {
		fixture.key.view_epoch++;
		REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
		CHECK(state.closed == 1);
		CHECK_FALSE(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, observer));
	}
	SUBCASE("resource invalidation") {
		RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport, true);
		CHECK(state.closed == 1);
		CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	}
	SUBCASE("viewport free") {
		const RID old = fixture.viewport;
		REQUIRE(RSG::viewport->free(old));
		fixture.viewport = RID();
		CHECK(state.closed == 1);
		CHECK_FALSE(RSG::viewport->viewport_unbind_camera_source_observer(old, observer));
	}
}

TEST_CASE("[B4][CameraSourceObserver] callback unbind pins exact observer through return") {
	ObserverState state;
	ObserverViewport fixture;
	state.detach_on_retained = true;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	observer.unref();
	CHECK(state.destroyed == 0);
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
	CHECK(state.retained == 0);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 1);
	CHECK(state.closed == 1);
	CHECK(state.detached);
	CHECK(state.alive_after_detach);
	CHECK(state.destroyed == 1);
	REQUIRE(state.last.is_valid());
	CHECK(state.last == render_data.get_native_camera_source());
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, state.last));
}

TEST_CASE("[B4][CameraSourceObserver] exact capture refuses foreign and stale sources and preserves XR eyes") {
	ObserverState state;
	ObserverViewport fixture, foreign;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(foreign.viewport, foreign.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(foreign.viewport, observer));
	auto camera = fixture.camera();
	const Transform3D eyes[] = { Transform3D(Basis(), Vector3(-0.03, 0, 0)), Transform3D(Basis(), Vector3(0.03, 0, 0)) };
	Projection projections[2];
	projections[0].set_perspective(67, 1.5, 0.2, 700);
	projections[1].set_perspective(68, 1.5, 0.2, 700);
	camera.set_multiview_camera(2, eyes, projections, false, false);
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	auto foreign_camera = foreign.camera();
	const auto foreign_token = RSG::viewport->viewport_begin_camera_capture(foreign.viewport, foreign_camera);
	Ref<Capture> foreign_capture;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(foreign.viewport, foreign_token, foreign_capture));
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, foreign_capture));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
	CHECK(state.retained == 0);
	RSG::viewport->drain_camera_source_observers();
	REQUIRE(state.last.is_valid());
	if (state.last.is_null()) {
		return;
	}
	CHECK(state.last->get_camera_data().view_count == 2);
	CHECK(state.last->get_camera_data().view_offset[0] == camera.view_offset[0]);
	CHECK(state.last->get_camera_data().view_offset[1] == camera.view_offset[1]);
	camera = fixture.camera();
	REQUIRE(RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera).is_valid());
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, state.last));
	CHECK(state.retained == 1);
}

TEST_CASE("[B4][CameraSourceObserver] closed callbacks may free the original viewport") {
	ObserverState state;
	ObserverViewport fixture;
	state.free_on_closed = true;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	SUBCASE("direct invalidation") {
		RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport, true);
		CHECK(state.closed_free_result);
	}
	SUBCASE("render buffer reconfiguration") {
		RSG::viewport->viewport_set_scaling_3d_scale(fixture.viewport, 0.75);
		CHECK(state.closed_free_result);
	}
	SUBCASE("viewport size") {
		RSG::viewport->viewport_set_size(fixture.viewport, 64, 48, 1);
		CHECK(state.closed_free_result);
	}
	SUBCASE("camera attachment") {
		const RID camera = RSG::scene->camera_allocate();
		RSG::scene->camera_initialize(camera);
		RSG::viewport->viewport_attach_camera(fixture.viewport, camera);
		RSG::scene->free(camera);
		CHECK(state.closed_free_result);
	}
#ifndef XR_DISABLED
	SUBCASE("XR resource change") {
		RSG::viewport->viewport_set_use_xr(fixture.viewport, true);
		CHECK(state.closed_free_result);
	}
#endif
	SUBCASE("epoch replacement") {
		fixture.key.view_epoch++;
		REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
		CHECK(state.closed_free_result);
	}
	SUBCASE("free closes after native teardown and rejects recursive free") {
		CHECK(RSG::viewport->free(fixture.viewport));
		CHECK_FALSE(state.closed_free_result);
	}
	CHECK(state.closed == 1);
	CHECK(RSG::viewport->viewport_owner.get_or_null(fixture.viewport) == nullptr);
}

TEST_CASE("[B4][CameraSourceObserver] closed callback resize runs after original mutation") {
	ObserverState state;
	ObserverViewport fixture;
	state.resize_on_closed = true;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	SUBCASE("viewport size") {
		RSG::viewport->viewport_set_size(fixture.viewport, 64, 48, 1);
	}
	SUBCASE("render buffer reconfiguration") {
		RSG::viewport->viewport_set_scaling_3d_scale(fixture.viewport, 0.75);
	}
	CHECK(state.closed == 1);
	const auto *viewport = RSG::viewport->viewport_owner.get_or_null(fixture.viewport);
	REQUIRE(viewport != nullptr);
	if (!viewport) {
		return;
	}
	CHECK(viewport->size == Size2i(47, 39));
	CHECK_FALSE(viewport->presentation_registered);
}

TEST_CASE("[B4][CameraSourceObserver] first queued source survives same epoch progression") {
	ObserverState state;
	ObserverViewport fixture;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	auto camera = fixture.camera();
	const auto first_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> first;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, first_token, first));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, first));
	fixture.key.present_seq++;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	const auto next_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> next;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, next_token, next));
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, next));
	CHECK(state.retained == 0);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 1);
	CHECK(state.last == first);
	REQUIRE(state.last.is_valid());
	if (state.last.is_null()) {
		return;
	}
	CHECK(state.last->get_presentation_key().present_seq + 1 == fixture.key.present_seq);
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, next));
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 2);
	CHECK(state.last == next);
}

TEST_CASE("[B4][CameraSourceObserver] invalidation removes pending source before safe drain") {
	ObserverState state;
	ObserverViewport fixture;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	const Ref<Capture> capture = render_data.get_native_camera_source();
	REQUIRE(capture.is_valid());
	if (capture.is_null()) {
		return;
	}
	const int before_queue = capture->get_reference_count();
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(capture->get_reference_count() == before_queue + 1);
	CHECK(state.retained == 0);
	SUBCASE("unbind") {
		CHECK(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, observer));
		CHECK(capture->get_reference_count() == before_queue);
	}
	SUBCASE("epoch change") {
		fixture.key.view_epoch++;
		CHECK(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
		CHECK(capture->get_reference_count() == before_queue - 1);
	}
	SUBCASE("resource invalidation") {
		RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport, true);
		CHECK(capture->get_reference_count() == before_queue - 1);
	}
	SUBCASE("free") {
		CHECK(RSG::viewport->free(fixture.viewport));
		CHECK(capture->get_reference_count() == before_queue - 1);
	}
	CHECK(state.closed == 1);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 0);
	CHECK(state.last.is_null());
}

TEST_CASE("[B4][CameraSourceObserver] safe drain may free original viewport and keeps callback alive") {
	ObserverState state;
	ObserverViewport fixture;
	state.free_on_retained = true;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	observer.unref();
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
	CHECK(state.retained == 0);
	CHECK(RSG::viewport->viewport_owner.get_or_null(fixture.viewport) != nullptr);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 1);
	CHECK(state.retained_free_result);
	CHECK(state.closed == 1);
	CHECK(state.alive_after_detach);
	CHECK(state.destroyed == 1);
	CHECK(RSG::viewport->viewport_owner.get_or_null(fixture.viewport) == nullptr);
	CHECK(state.last == render_data.get_native_camera_source());
}

TEST_CASE("[B4][CameraSourceObserver] safe drain uses its starting count and refuses recursive drain") {
	ObserverState state;
	ObserverViewport fixture;
	state.enqueue_on_retained = true;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, observer));
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	RenderDataRD render_data;
	REQUIRE(render_data.set_camera_source(token.view_id, token.view_epoch, token.capture_ordinal));
	REQUIRE(render_data.retain_native_camera_source(*RSG::viewport, camera));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, render_data.get_native_camera_source()));
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 1);
	CHECK(state.enqueued);
	CHECK(state.nested_drain_retained == 1);
	RSG::viewport->drain_camera_source_observers();
	CHECK(state.retained == 2);
	REQUIRE(state.last.is_valid());
	if (state.last.is_null()) {
		return;
	}
	CHECK(state.last->get_token().capture_ordinal > token.capture_ordinal);
}


TEST_CASE("[B4][CameraSourceSubscriber] original native source fans out to independently retained associations") {
	ObserverState primary_state, a_state, b_state;
	ObserverViewport fixture, foreign;
	Ref<Observer> primary(memnew(TestCameraSourceObserver(primary_state, fixture.viewport)));
	Ref<Observer> a(memnew(TestCameraSourceObserver(a_state, fixture.viewport)));
	Ref<Observer> b(memnew(TestCameraSourceObserver(b_state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(foreign.viewport, foreign.key));
	CHECK(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, primary));
	CHECK(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, a));
	CHECK(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, b));
	CHECK(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, a));
	CHECK_FALSE(RSG::viewport->viewport_subscribe_camera_source(foreign.viewport, a));
	CHECK_FALSE(RSG::viewport->viewport_bind_camera_source_observer(fixture.viewport, a));
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> capture;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, capture));
	CHECK(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(primary_state.retained == 0);
	CHECK(a_state.retained == 0);
	CHECK(b_state.retained == 0);
	// Closing exactly one association cancels only its own pending source.
	CHECK(RSG::viewport->viewport_unsubscribe_camera_source(fixture.viewport, a));
	CHECK(a_state.closed == 1);
	RSG::viewport->drain_camera_source_observers();
	CHECK(primary_state.retained == 1);
	CHECK(b_state.retained == 1);
	CHECK(a_state.retained == 0);
	CHECK(primary_state.last == capture);
	CHECK(b_state.last == capture);
	CHECK_FALSE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	CHECK(RSG::viewport->viewport_unbind_camera_source_observer(fixture.viewport, primary));
	CHECK(primary_state.closed == 1);
	CHECK(b_state.closed == 0);
	RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport);
	CHECK(b_state.closed == 1);
	CHECK_FALSE(RSG::viewport->viewport_unsubscribe_camera_source(fixture.viewport, b));
}

TEST_CASE("[B4][CameraSourceSubscriber] invalidation detaches all before close may free original viewport") {
	ObserverState a_state, b_state;
	ObserverViewport fixture;
	Ref<Observer> a(memnew(TestCameraSourceObserver(a_state, fixture.viewport)));
	Ref<Observer> b(memnew(TestCameraSourceObserver(b_state, fixture.viewport)));
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	REQUIRE(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, a));
	REQUIRE(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, b));
	auto camera = fixture.camera();
	const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> capture;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, capture));
	REQUIRE(RSG::viewport->viewport_notify_camera_source_retained(fixture.viewport, capture));
	b_state.free_on_closed = true;
	RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport);
	CHECK(b_state.closed_free_result);
	CHECK(a_state.closed == 1);
	CHECK(b_state.closed == 1);
	RSG::viewport->drain_camera_source_observers();
	CHECK(a_state.retained == 0);
	CHECK(b_state.retained == 0);
}

TEST_CASE("[B4][CameraSourceSubscriber] cached RID ordering survives cross-viewport close and recursive draw") {
	ObserverState a_state;
	ObserverViewport a, b;
	RSG::viewport->viewport_set_size(a.viewport, 64, 48, 1);
	RSG::viewport->viewport_set_size(b.viewport, 64, 48, 1);
	RSG::viewport->viewport_set_update_mode(a.viewport, RSE::VIEWPORT_UPDATE_ALWAYS);
	RSG::viewport->viewport_set_update_mode(b.viewport, RSE::VIEWPORT_UPDATE_ALWAYS);
	RSG::viewport->viewport_set_active(a.viewport, true);
	RSG::viewport->viewport_set_active(b.viewport, true);
	// Dummy texture storage has no render target. Exercise real sorting and
	// close/reentry here; this does not claim callback-during-draw coverage.
	RSG::viewport->draw_viewports(false);
	const Vector<RID> frame_viewports = RSG::viewport->sorted_active_viewports;
	REQUIRE(frame_viewports.has(a.viewport));
	REQUIRE(frame_viewports.has(b.viewport));
	const int frame_count = frame_viewports.size();
	REQUIRE(RSG::viewport->viewport_register_presentation_key(a.viewport, a.key));
	Ref<Observer> observer(memnew(TestCameraSourceObserver(a_state, a.viewport)));
	REQUIRE(RSG::viewport->viewport_subscribe_camera_source(a.viewport, observer));
	a_state.free_other_on_closed = b.viewport;
	a_state.free_on_closed = true;
	SUBCASE("The later cached viewport was freed") {}
	SUBCASE("Close also recursively replaces the sorted viewport cache") {
		a_state.redraw_on_closed = true;
	}
	const uint64_t previous_pass = RSG::viewport->draw_viewports_pass;
	RSG::viewport->viewport_set_size(a.viewport, 65, 49, 1);
	CHECK(a_state.closed == 1);
	CHECK(a_state.other_closed_free_result);
	CHECK(a_state.closed_free_result);
	CHECK(frame_viewports.size() == frame_count);
	for (RID rid : frame_viewports) {
		if (rid == a.viewport || rid == b.viewport) {
			CHECK(RSG::viewport->viewport_owner.get_or_null(rid) == nullptr);
		}
	}
	CHECK(RSG::viewport->draw_viewports_pass == previous_pass + (a_state.redraw_on_closed ? 1 : 0));
	RSG::viewport->draw_viewports(false);
	CHECK(a_state.closed == 1);
}

TEST_CASE("[B4][CameraSourceObserver][PreviousCameraCapture] delayed source borrow retains actual previous camera before current adoption") {
	ObserverViewport fixture;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	auto camera = fixture.camera();
	camera.main_transform.origin.x = 3;
	camera.taa_jitter = Vector2(0.125, -0.25);
	camera.taa_frame_count = 7;
	camera.view_offset[0].origin.x = 0.5;
	const auto first_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> first;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, first_token, first));
	CHECK_FALSE(first->get_previous_camera_token().is_valid());
	CHECK(first->get_previous_camera_data() == nullptr);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, first_token));
	REQUIRE(RSG::viewport->viewport_adopt_camera_data(fixture.viewport));

	// A presentation may skip this view entirely. No capture/adoption occurs,
	// so its next invocation still consumes the actual first camera record.
	fixture.key.present_seq += 2;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	camera.main_transform.origin.x = 7;
	camera.taa_jitter = Vector2(0.375, 0.25);
	camera.taa_frame_count = 8;
	camera.view_offset[0].origin.x = 1;
	camera.main_projection = Projection::create_orthogonal(-4, 4, -4, 4, 0.1, 100);
	const auto second_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	REQUIRE(second_token.is_valid());
	CHECK(second_token.capture_ordinal == first_token.capture_ordinal + 1);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, second_token));
	REQUIRE(RSG::viewport->viewport_adopt_camera_data(fixture.viewport));
	CHECK(RSG::viewport->viewport_get_previous_camera_token(fixture.viewport) == second_token);

	// First lazy borrow is deliberately after the native post-render adoption.
	// Reading mutable viewport history here would incorrectly return camera2.
	Ref<Capture> second;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, second_token, second));
	CHECK(second->get_previous_camera_token() == first_token);
	const auto *previous = second->get_previous_camera_data();
	REQUIRE(previous != nullptr);
	CHECK(previous->source_viewport == fixture.viewport);
	CHECK(previous->source_view_id == first_token.view_id);
	CHECK(previous->source_view_epoch == first_token.view_epoch);
	CHECK(previous->source_capture_ordinal == first_token.capture_ordinal);
	CHECK(previous->main_transform == first->get_camera_data().main_transform);
	CHECK(previous->main_projection == first->get_camera_data().main_projection);
	CHECK(previous->view_count == first->get_camera_data().view_count);
	CHECK(previous->is_orthogonal == first->get_camera_data().is_orthogonal);
	CHECK(previous->visible_layers == first->get_camera_data().visible_layers);
	CHECK(previous->vaspect == first->get_camera_data().vaspect);
	CHECK(previous->taa_jitter == Vector2(0.125, -0.25));
	CHECK(previous->taa_frame_count == 7);
	for (uint32_t eye = 0; eye < previous->view_count; ++eye) {
		CHECK(previous->view_offset[eye] == first->get_camera_data().view_offset[eye]);
		CHECK(previous->view_projection[eye] == first->get_camera_data().view_projection[eye]);
	}
	CHECK(previous->main_transform.origin.x == 3);
	CHECK(second->get_camera_data().main_transform.origin.x == 7);
	CHECK(second->get_presentation_key().present_seq == fixture.key.present_seq);

	fixture.key.present_seq++;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	camera.main_transform.origin.x = 11;
	const auto third_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, third_token));
	REQUIRE(RSG::viewport->viewport_adopt_camera_data(fixture.viewport));
	Ref<Capture> third;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, third_token, third));
	CHECK(third->get_previous_camera_token() == second_token);
	REQUIRE(third->get_previous_camera_data() != nullptr);
	CHECK(third->get_previous_camera_data()->main_transform.origin.x == 7);
	CHECK(second->get_previous_camera_token() == first_token);
	CHECK(previous->main_transform.origin.x == 3);

	fixture.key.view_epoch++;
	fixture.key.present_seq++;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	const auto reset_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> reset;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, reset_token, reset));
	CHECK_FALSE(reset->get_previous_camera_token().is_valid());
	CHECK(reset->get_previous_camera_data() == nullptr);
	CHECK(second->get_previous_camera_token() == first_token);
	CHECK(previous->main_transform.origin.x == 3);
	REQUIRE(RSG::viewport->free(fixture.viewport));
	CHECK(second->get_previous_camera_token() == first_token);
	CHECK(previous->main_transform.origin.x == 3);
}

TEST_CASE("[B4][CameraSourceObserver][PreviousCameraCapture] equal camera payload does not replace original previous identity") {
	ObserverViewport fixture;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	auto camera = fixture.camera();
	const auto first_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, first_token));
	REQUIRE(RSG::viewport->viewport_adopt_camera_data(fixture.viewport));
	fixture.key.present_seq++;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	const auto next_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	Ref<Capture> next;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, next_token, next));
	CHECK(next_token != first_token);
	CHECK(next->get_previous_camera_token() == first_token);
	REQUIRE(next->get_previous_camera_data() != nullptr);
	CHECK(next->get_previous_camera_data()->main_transform == next->get_camera_data().main_transform);
	CHECK(next->get_previous_camera_data()->source_capture_ordinal == first_token.capture_ordinal);
	CHECK(next->get_camera_data().source_capture_ordinal == next_token.capture_ordinal);
}

TEST_CASE("[B4][CameraSourceObserver][PreviousCameraCapture] sequence refusal preserves frozen previous and unadopted current stage") {
	ObserverViewport fixture;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	auto camera = fixture.camera();
	camera.main_transform.origin.x = 3;
	const auto first_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, first_token));
	REQUIRE(RSG::viewport->viewport_adopt_camera_data(fixture.viewport));
	auto *viewport = RSG::viewport->viewport_owner.get_or_null(fixture.viewport);
	REQUIRE(viewport != nullptr);
	// Pure counter boundary input, before the next original capture invocation.
	viewport->camera_capture_ordinal = UINT64_MAX - 1;
	fixture.key.present_seq++;
	REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
	camera.main_transform.origin.x = 7;
	const auto last_token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	CHECK(last_token.capture_ordinal == UINT64_MAX);
	REQUIRE(RSG::viewport->viewport_stage_camera_data(fixture.viewport, &camera, last_token));
	Ref<Capture> last;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, last_token, last));
	ERR_PRINT_OFF;
	const auto refused = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
	ERR_PRINT_ON;
	CHECK_FALSE(refused.is_valid());
	CHECK(viewport->current_camera_token == last_token);
	CHECK(viewport->captured_previous_camera_token == first_token);
	CHECK(viewport->captured_previous_camera_data.main_transform.origin.x == 3);
	CHECK(viewport->staged_camera_data_valid);
	CHECK(viewport->staged_camera_token == last_token);
	Ref<Capture> replay;
	REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, last_token, replay));
	CHECK(replay == last);
	CHECK(last->get_previous_camera_token() == first_token);
	REQUIRE(last->get_previous_camera_data() != nullptr);
	CHECK(last->get_previous_camera_data()->main_transform.origin.x == 3);
}

// Invocation ownership and recordability are deliberately separate: Dummy can
// exercise native retention/close, but can never grant a GPU recording window.
class TestRendererInvocationAccess {
public:
 static void reject_stale(RendererViewport &owner, const Ref<Capture> &capture, Observer *selected) {
  RendererViewport::CameraInvocationScope scope(owner, capture);
  CHECK_FALSE(scope.select(selected));
  CHECK_FALSE(scope.is_current(selected, capture));
  CHECK_FALSE(scope.is_recordable(selected, capture));
 }

 template <typename F>
 static void retained_scope(RendererViewport &owner, const Ref<Capture> &capture, Observer *selected, Observer *foreign, F body) {
  RendererViewport::CameraInvocationScope scope(owner, capture);
  CHECK_FALSE(scope.select(foreign));
  CHECK(scope.select(selected));
  CHECK(scope.is_current(selected, capture));
  CHECK_FALSE(scope.is_current(foreign, capture));
  CHECK_FALSE(scope.is_current(selected, Ref<Capture>()));
  CHECK_FALSE(scope.is_recordable(selected, capture));
  body();
  CHECK_FALSE(scope.is_current(selected, capture));
 }
};

TEST_CASE("[B4][RendererInvocation] selected registration closes after scope and never grants Dummy") {
 ObserverState state, foreign_state;
 ObserverViewport fixture;
 REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
 Ref<Observer> selected(memnew(TestCameraSourceObserver(state, fixture.viewport)));
 Ref<Observer> foreign(memnew(TestCameraSourceObserver(foreign_state, fixture.viewport)));
 REQUIRE(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, selected));
 auto camera = fixture.camera();
 const auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
 Ref<Capture> capture;
 REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, capture));
 TestRendererInvocationAccess::retained_scope(*RSG::viewport, capture, selected.ptr(), foreign.ptr(), [&]() {
  RSG::viewport->viewport_invalidate_presentation_key(fixture.viewport, true);
  selected.unref();
  CHECK(state.destroyed == 0);
  CHECK(state.closed == 0);
  RSG::viewport->drain_camera_source_observers();
  CHECK(state.closed == 0);
 });
 CHECK(state.closed == 0);
 RSG::viewport->drain_camera_source_observers();
 CHECK(state.closed == 1);
 CHECK(state.destroyed == 1);
 CHECK(foreign_state.closed == 0);
 CHECK(capture->get_token() == token);
}

TEST_CASE("[B4][RendererInvocation] superseded exact capture cannot recover from equal camera payload") {
 ObserverState state;
 ObserverViewport fixture;
 REQUIRE(RSG::viewport->viewport_register_presentation_key(fixture.viewport, fixture.key));
 Ref<Observer> selected(memnew(TestCameraSourceObserver(state, fixture.viewport)));
 REQUIRE(RSG::viewport->viewport_subscribe_camera_source(fixture.viewport, selected));
 auto camera = fixture.camera();
 auto token = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
 Ref<Capture> old;
 REQUIRE(RSG::viewport->viewport_get_camera_capture_ref(fixture.viewport, token, old));
 const auto next = RSG::viewport->viewport_begin_camera_capture(fixture.viewport, camera);
 REQUIRE(next != token);
 TestRendererInvocationAccess::reject_stale(*RSG::viewport, old, selected.ptr());
 CHECK(old->get_camera_data().main_transform == camera.main_transform);
 CHECK(state.closed == 0);
}
