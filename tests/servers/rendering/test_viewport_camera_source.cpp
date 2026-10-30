/**************************************************************************/
/*  test_viewport_camera_source.cpp                                      */
/**************************************************************************/

#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "tests/test_macros.h"

#include "servers/rendering/rendering_server_globals.h"

TEST_FORCE_LINK(test_viewport_camera_source)

TEST_CASE("[B4][E15] viewport camera source requires explicit C01 registration") {
	CHECK(RSG::viewport != nullptr);
	if (!RSG::viewport) {
		return;
	}
	RID viewport = RSG::viewport->viewport_allocate();
	RSG::viewport->viewport_initialize(viewport);

	RendererViewport::Viewport::CameraSourceToken absent = RSG::viewport->viewport_begin_camera_capture(viewport);
	CHECK_FALSE(absent.is_valid());

	RendererViewport::Viewport::ViewPresentationKey key;
	key.view_id = 41;
	key.view_epoch = 3;
	key.present_seq = 9;
	CHECK(RSG::viewport->viewport_register_presentation_key(viewport, key));
	CHECK(RSG::viewport->viewport_get_presentation_key(viewport).view_id == 41);

	RendererViewport::Viewport::CameraSourceToken first = RSG::viewport->viewport_begin_camera_capture(viewport);
	CHECK(first.is_valid());
	CHECK(first.view_id == 41);
	CHECK(first.view_epoch == 3);
	CHECK(first.capture_ordinal == 1);

	RendererViewport::Viewport::CameraSourceToken foreign = first;
	foreign.capture_ordinal++;
	CHECK(foreign != first);
	RendererSceneRender::CameraData camera_data{};
	camera_data.source_view_id = first.view_id;
	camera_data.source_view_epoch = first.view_epoch;
	camera_data.source_capture_ordinal = first.capture_ordinal;
	RendererSceneRender::CameraData foreign_data = camera_data;
	foreign_data.source_view_id = 99;
	foreign_data.source_view_epoch = 8;
	foreign_data.source_capture_ordinal = 7;
	RSG::viewport->viewport_set_prev_camera_data(viewport, &foreign_data, foreign);
	const RendererSceneRender::CameraData *previous_camera = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(previous_camera != nullptr);
	CHECK_FALSE(previous_camera->has_source_token());

	RSG::viewport->viewport_set_prev_camera_data(viewport, &camera_data, first);
	CHECK(RSG::viewport->viewport_get_previous_camera_token(viewport) == first);
	previous_camera = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(previous_camera != nullptr);
	CHECK(previous_camera->has_source_token());
	CHECK(previous_camera->source_view_id == first.view_id);
	CHECK(previous_camera->source_view_epoch == first.view_epoch);
	CHECK(previous_camera->source_capture_ordinal == first.capture_ordinal);
	RendererViewport::Viewport::CameraSourceToken next = first;
	next.capture_ordinal++;
	CHECK(RSG::viewport->viewport_begin_camera_capture(viewport) == next);
	RSG::viewport->viewport_set_prev_camera_data(viewport, &camera_data, next);
	previous_camera = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(previous_camera != nullptr);
	CHECK(previous_camera->source_view_id == first.view_id);
	CHECK(previous_camera->source_view_epoch == first.view_epoch);
	CHECK(previous_camera->source_capture_ordinal == first.capture_ordinal);
	RSG::viewport->viewport_set_prev_camera_data(viewport, &foreign_data, foreign);
	previous_camera = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(previous_camera != nullptr);
	CHECK(previous_camera->source_view_id == first.view_id);
	CHECK(previous_camera->source_view_epoch == first.view_epoch);
	CHECK(previous_camera->source_capture_ordinal == first.capture_ordinal);
	CHECK(RSG::viewport->viewport_get_previous_camera_token(viewport) == first);

	RSG::viewport->viewport_set_size(viewport, 17, 19, 1);
	CHECK_FALSE(RSG::viewport->viewport_get_presentation_key(viewport).view_id != 0);
	CHECK_FALSE(RSG::viewport->viewport_begin_camera_capture(viewport).is_valid());
	CHECK(RSG::viewport->viewport_register_presentation_key(viewport, key));
	RSG::viewport->viewport_attach_camera(viewport, RID::from_uint64(1));
	CHECK_FALSE(RSG::viewport->viewport_get_presentation_key(viewport).view_id != 0);

	RSG::viewport->viewport_invalidate_presentation_key(viewport);
	CHECK_FALSE(RSG::viewport->viewport_get_presentation_key(viewport).view_id != 0);
	CHECK_FALSE(RSG::viewport->viewport_get_previous_camera_token(viewport).is_valid());
	const RendererSceneRender::CameraData *invalidated_previous = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(invalidated_previous != nullptr);
	CHECK_FALSE(invalidated_previous->has_source_token());
	CHECK(invalidated_previous->source_view_id == 0);
	CHECK(invalidated_previous->source_view_epoch == 0);
	CHECK(invalidated_previous->source_capture_ordinal == 0);
	RSG::viewport->free(viewport);
}


TEST_CASE("[B4][E17] external packet identity requires camera tuple and material capability") {
	RendererSceneRender::CameraData camera;
	CHECK_FALSE(camera.has_source_token());
	camera.source_view_id = 41;
	camera.source_view_epoch = 3;
	camera.source_capture_ordinal = 9;
	CHECK(camera.has_source_token());
	RenderDataRD::PacketIdentity packet;
	RenderDataRD render_data;
	render_data.packet_identity = packet;
	CHECK_FALSE(packet.has_camera_source());
	CHECK_FALSE(packet.material_admitted(7, 1));
	CHECK(packet.surface_admitted(0));
	CHECK(packet.external_camera_admitted());
	CHECK(render_data.surface_admitted(0));
	CHECK(render_data.external_camera_admitted());
	packet.external = true;
	packet.view_id = 41;
	packet.view_epoch = 3;
	packet.camera_capture_ordinal = 9;
	packet.material_generation = 12;
	packet.material_capabilities = 0x3;
	render_data.packet_identity = packet;
	CHECK(packet.has_camera_source());
	CHECK(packet.camera_source_matches(41, 3, 9));
	CHECK_FALSE(packet.camera_source_matches(41, 4, 9));
	CHECK(packet.material_admitted(12, 0x1));
	CHECK(packet.material_admitted(12, 0x3));
	CHECK(packet.surface_admitted(12));
	CHECK_FALSE(packet.surface_admitted(11));
	CHECK(packet.external_camera_admitted());
	CHECK(render_data.external_camera_admitted());
	CHECK_FALSE(packet.material_admitted(11, 0x1));
	CHECK_FALSE(packet.material_admitted(12, 0x4));
	packet.invalidate_material_admission();
	CHECK(packet.external);
	CHECK_FALSE(packet.material_admitted(12, 0));
	CHECK(packet.material_generation == 0);
	CHECK(packet.material_capabilities == 0);
	packet.retire();
	packet.retire();
	render_data.packet_identity = packet;
	CHECK(packet.external);
	CHECK(packet.retired);
	CHECK_FALSE(packet.has_camera_source());
	CHECK_FALSE(packet.external_camera_admitted());
	CHECK_FALSE(packet.camera_source_matches(41, 3, 9));
	CHECK_FALSE(packet.material_admitted(12, 0));
	CHECK_FALSE(packet.surface_admitted(12));
	CHECK_FALSE(render_data.surface_admitted(12));
}

TEST_CASE("[B4][D-Next-5] RenderDataRD admission wrappers close all four packet paths") {
	RenderDataRD render_data;

	// Ordinary Mesh compatibility stays open through the shared wrappers.
	CHECK(render_data.external_camera_admitted());
	CHECK(render_data.surface_admitted(0));

	// An external packet requires both a camera tuple and matching material generation.
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	CHECK(render_data.external_camera_admitted());
	CHECK(render_data.surface_admitted(12));

	// A generation mismatch closes the surface path and the shared retire helper
	// closes the packet for all subsequent passes.
	CHECK_FALSE(render_data.surface_admitted(11));
	render_data.retire_packet();
	CHECK_FALSE(render_data.external_camera_admitted());
	CHECK_FALSE(render_data.surface_admitted(12));

	// Retire remains idempotent and cannot be bypassed by restoring tuple fields.
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	render_data.retire_packet();
	CHECK_FALSE(render_data.external_camera_admitted());
	CHECK_FALSE(render_data.surface_admitted(12));
}

TEST_CASE("[B4][D-Next-6] packet admission atomically retires camera and surface failures") {
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;

	CHECK(render_data.admit_camera());
	CHECK(render_data.admit_surface(12));

	// Either stale camera or stale material admission seals the whole packet.
	render_data.packet_identity.camera_capture_ordinal = 0;
	CHECK_FALSE(render_data.admit_camera());
	CHECK(render_data.packet_identity.retired);
	CHECK_FALSE(render_data.admit_surface(12));

	render_data.packet_identity.retired = false;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	CHECK(render_data.admit_camera());
	CHECK_FALSE(render_data.admit_surface(11));
	CHECK(render_data.packet_identity.retired);
	CHECK_FALSE(render_data.admit_camera());
}

TEST_CASE("[B4][D-Next-7] packet admission propagates typed diagnostics") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;

	CHECK(render_data.admit_camera_status() == AdmissionStatus::ORDINARY_MESH);
	CHECK(render_data.admit_surface_status(0) == AdmissionStatus::ORDINARY_MESH);

	render_data.packet_identity.external = true;
	render_data.packet_identity.material_generation = 12;
	CHECK(render_data.admit_camera_status() == AdmissionStatus::CAMERA_SOURCE_MISSING);
	CHECK(render_data.packet_identity.retired);

	render_data.packet_identity.retired = false;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	CHECK(render_data.admit_camera_status() == AdmissionStatus::ADMITTED);
	CHECK(render_data.admit_surface_status(11) == AdmissionStatus::MATERIAL_GENERATION_MISMATCH);
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.admit_camera_status() == AdmissionStatus::RETIRED);
}

TEST_CASE("[B4] exhausted material generation cannot admit an external packet") {
	using Status = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD data;
	CHECK(data.admit_surface_status(UINT64_MAX) == Status::ORDINARY_MESH);
	data.packet_identity.external = true;
	data.packet_identity.material_generation = UINT64_MAX - 1;
	CHECK(data.admit_surface_status(UINT64_MAX - 1) == Status::ADMITTED);
	data.packet_identity.material_generation = UINT64_MAX;
	CHECK(data.admit_surface_status(UINT64_MAX) == Status::MATERIAL_GENERATION_MISMATCH);
	CHECK(data.packet_identity.retired);
}

TEST_CASE("[B4][D-Next-8] retired status wins over ordinary Mesh compatibility") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;

	CHECK(render_data.external_camera_admitted());
	CHECK(render_data.surface_admitted(0));
	CHECK(render_data.admit_camera_status() == AdmissionStatus::ORDINARY_MESH);
	CHECK(render_data.admit_surface_status(0) == AdmissionStatus::ORDINARY_MESH);

	render_data.retire_packet();
	CHECK_FALSE(render_data.external_camera_admitted());
	CHECK_FALSE(render_data.surface_admitted(0));
	CHECK(render_data.admit_camera_status() == AdmissionStatus::RETIRED);
	CHECK(render_data.admit_surface_status(0) == AdmissionStatus::RETIRED);
	CHECK_FALSE(render_data.admit_camera());
	CHECK_FALSE(render_data.admit_surface(0));
}

TEST_CASE("[B4][D-Next-9] status, bool helpers, and predicates stay retired-first") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;

	// Ordinary Mesh: all three layers agree on admission.
	CHECK(render_data.admit_camera_status() == AdmissionStatus::ORDINARY_MESH);
	CHECK(RenderDataRD::admission_succeeded(render_data.admit_camera_status()));
	CHECK(render_data.external_camera_admitted());
	CHECK(render_data.surface_admitted(0));

	// Valid external packet: typed and bool paths agree.
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	CHECK(render_data.admit_camera_status() == AdmissionStatus::ADMITTED);
	CHECK(render_data.admit_camera());
	CHECK(render_data.admit_surface_status(12) == AdmissionStatus::ADMITTED);
	CHECK(render_data.admit_surface(12));

	// Camera and material failures each retire, then all layers report closed.
	render_data.packet_identity.camera_capture_ordinal = 0;
	CHECK(render_data.admit_camera_status() == AdmissionStatus::CAMERA_SOURCE_MISSING);
	CHECK_FALSE(render_data.external_camera_admitted());
	CHECK_FALSE(render_data.admit_camera());
	CHECK(render_data.packet_identity.retired);

	render_data.packet_identity.retired = false;
	render_data.packet_identity.camera_capture_ordinal = 9;
	CHECK(render_data.admit_surface_status(11) == AdmissionStatus::MATERIAL_GENERATION_MISMATCH);
	CHECK_FALSE(render_data.surface_admitted(11));
	CHECK_FALSE(render_data.admit_surface(11));
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.admit_camera_status() == AdmissionStatus::RETIRED);
}

TEST_CASE("[B4][D-Next-10] retire clears camera and material packet state") {
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	render_data.packet_identity.material_capabilities = 0x3;

	render_data.retire_packet();
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK(render_data.packet_identity.material_generation == 0);
	CHECK(render_data.packet_identity.material_capabilities == 0);

	// Repeated retire keeps the fully invalidated state.
	render_data.retire_packet();
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK_FALSE(render_data.admit_camera());
	CHECK_FALSE(render_data.admit_surface(12));
}

TEST_CASE("[B4][D-Next-11] implicit admission failures preserve cleared retired state") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 0;
	render_data.packet_identity.material_generation = 12;
	render_data.packet_identity.material_capabilities = 0x3;

	CHECK(render_data.admit_camera_status() == AdmissionStatus::CAMERA_SOURCE_MISSING);
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK(render_data.packet_identity.material_generation == 0);
	CHECK(render_data.packet_identity.material_capabilities == 0);
	CHECK(render_data.admit_camera_status() == AdmissionStatus::RETIRED);

	render_data.packet_identity.retired = false;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	CHECK(render_data.admit_surface_status(11) == AdmissionStatus::MATERIAL_GENERATION_MISMATCH);
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK(render_data.packet_identity.material_generation == 0);
	CHECK(render_data.admit_surface_status(12) == AdmissionStatus::RETIRED);
}

TEST_CASE("[B4][D-Next-12] material invalidation defers retirement until surface use") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	render_data.packet_identity.view_id = 41;
	render_data.packet_identity.view_epoch = 3;
	render_data.packet_identity.camera_capture_ordinal = 9;
	render_data.packet_identity.material_generation = 12;
	render_data.packet_identity.material_capabilities = 0x3;

	render_data.packet_identity.invalidate_material_admission();
	CHECK(render_data.admit_camera_status() == AdmissionStatus::ADMITTED);
	CHECK(render_data.external_camera_admitted());
	CHECK(render_data.admit_surface_status(12) == AdmissionStatus::MATERIAL_GENERATION_MISMATCH);
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK(render_data.packet_identity.material_generation == 0);
	CHECK_FALSE(render_data.admit_camera());
	CHECK_FALSE(render_data.admit_surface(12));

	RenderDataRD ordinary;
	CHECK(ordinary.admit_camera());
	CHECK(ordinary.admit_surface(0));
}

TEST_CASE("[B4][D-Next-13] camera tuple enters RenderDataRD through one typed seam") {
	RenderDataRD render_data;
	CHECK_FALSE(render_data.packet_identity.external);
	CHECK(render_data.external_camera_admitted());

	render_data.set_camera_source(41, 3, 9);
	CHECK(render_data.packet_identity.view_id == 41);
	CHECK(render_data.packet_identity.view_epoch == 3);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 9);
	CHECK(render_data.external_camera_admitted());
	CHECK_FALSE(render_data.packet_identity.external);

	render_data.set_camera_source(52, 4, 10);
	CHECK(render_data.packet_identity.view_id == 52);
	CHECK(render_data.packet_identity.view_epoch == 4);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 10);
	render_data.retire_packet();
	CHECK_FALSE(render_data.external_camera_admitted());
}

TEST_CASE("[B4][D-Next-14] retired packet rejects camera tuple restoration") {
	RenderDataRD render_data;
	CHECK(render_data.set_camera_source(41, 3, 9));
	CHECK(render_data.packet_identity.view_id == 41);

	render_data.retire_packet();
	CHECK_FALSE(render_data.set_camera_source(52, 4, 10));
	CHECK(render_data.packet_identity.view_id == 0);
	CHECK(render_data.packet_identity.view_epoch == 0);
	CHECK(render_data.packet_identity.camera_capture_ordinal == 0);
	CHECK_FALSE(render_data.external_camera_admitted());
	CHECK(render_data.admit_camera_status() == RenderDataRD::PacketAdmissionStatus::RETIRED);
}

TEST_CASE("[B4][D-Next-16] external packet capability boundary fails closed") {
	using AdmissionStatus = RenderDataRD::PacketAdmissionStatus;
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	render_data.set_camera_source(41, 3, 9);
	render_data.packet_identity.material_generation = 12;

	CHECK(render_data.admit_camera_status() == AdmissionStatus::ADMITTED);
	CHECK(render_data.admit_surface_status(12, 0) == AdmissionStatus::ADMITTED);
	CHECK(render_data.admit_surface_status(12, 0x1) == AdmissionStatus::MATERIAL_GENERATION_MISMATCH);
	CHECK(render_data.packet_identity.retired);
	CHECK(render_data.packet_identity.material_generation == 0);
	CHECK(render_data.packet_identity.material_capabilities == 0);
	CHECK_FALSE(render_data.admit_surface(12, 0));
}

TEST_CASE("[B4][E15][E16] render packet carries previous camera source identity") {
	RenderDataRD render_data;
	CHECK(render_data.set_camera_source(41, 3, 9));
	render_data.packet_identity.external = true;
	CHECK(render_data.set_previous_camera_source(41, 3, 8));
	CHECK(render_data.previous_camera_source_admitted());
	CHECK(render_data.previous_camera_source_matches(41, 3, 8));
	CHECK_FALSE(render_data.previous_camera_source_matches(40, 2, 8));

	// A previous camera from another presentation lineage must fail closed.
	CHECK(render_data.set_previous_camera_source(40, 2, 7));
	CHECK_FALSE(render_data.previous_camera_source_admitted());
	CHECK(render_data.admit_camera_status() == RenderDataRD::PacketAdmissionStatus::ADMITTED);
	render_data.retire_packet();
	CHECK(render_data.admit_camera_status() == RenderDataRD::PacketAdmissionStatus::RETIRED);

	CHECK_FALSE(render_data.previous_camera_source_admitted());
	CHECK_FALSE(render_data.previous_camera_source_matches(40, 2, 8));
	CHECK(render_data.packet_identity.previous_view_id == 0);
	CHECK(render_data.packet_identity.previous_view_epoch == 0);
	CHECK(render_data.packet_identity.previous_camera_capture_ordinal == 0);
}

TEST_CASE("[B4][E18] native retirement receipt binds the exact packet identity") {
	RenderDataRD render_data;
	render_data.packet_identity.external = true;
	CHECK(render_data.set_camera_source(41, 3, 9));
	CHECK(render_data.set_previous_camera_source(41, 3, 8));
	render_data.packet_identity.material_generation = 12;

	// RED: the typed packet must issue a receipt before identity is retired.
	const RenderDataRD::NativeRetirementReceipt receipt = render_data.issue_native_retirement_receipt();
	CHECK(receipt.is_valid());
	CHECK(receipt.view_id == 41);
	CHECK(receipt.view_epoch == 3);
	CHECK(receipt.capture_ordinal == 9);
	CHECK(receipt.previous_capture_ordinal == 8);
	CHECK(receipt.material_generation == 12);
	CHECK_FALSE(render_data.native_retirement_acknowledged());
	CHECK_FALSE(render_data.set_camera_source(41, 3, 10));
	CHECK_FALSE(render_data.set_previous_camera_source(41, 3, 9));

	CHECK_FALSE(render_data.acknowledge_native_retirement(receipt.view_id, receipt.view_epoch, receipt.capture_ordinal + 1, receipt.previous_capture_ordinal, receipt.material_generation));
	CHECK_FALSE(render_data.acknowledge_native_retirement(receipt.view_id, receipt.view_epoch, receipt.capture_ordinal, receipt.previous_capture_ordinal + 1, receipt.material_generation));
	CHECK(render_data.acknowledge_native_retirement(receipt.view_id, receipt.view_epoch, receipt.capture_ordinal, receipt.previous_capture_ordinal, receipt.material_generation));
	CHECK(render_data.native_retirement_acknowledged());
	render_data.retire_packet();
	CHECK(render_data.native_retirement_acknowledged());
	CHECK_FALSE(render_data.acknowledge_native_retirement(receipt.view_id, receipt.view_epoch, receipt.capture_ordinal, receipt.previous_capture_ordinal, receipt.material_generation));
	CHECK_FALSE(render_data.issue_native_retirement_receipt().is_valid());

	RenderDataRD retired_before_ack;
	retired_before_ack.packet_identity.external = true;
	CHECK(retired_before_ack.set_camera_source(51, 4, 11));
	CHECK(retired_before_ack.set_previous_camera_source(51, 4, 10));
	retired_before_ack.packet_identity.material_generation = 13;
	const RenderDataRD::NativeRetirementReceipt delayed = retired_before_ack.issue_native_retirement_receipt();
	CHECK(delayed.is_valid());
	retired_before_ack.retire_packet();
	CHECK(retired_before_ack.acknowledge_native_retirement(delayed.view_id, delayed.view_epoch, delayed.capture_ordinal, delayed.previous_capture_ordinal, delayed.material_generation));
	CHECK(retired_before_ack.native_retirement_acknowledged());
	retired_before_ack.retire_packet();
	CHECK(retired_before_ack.native_retirement_acknowledged());
}

TEST_CASE("[B4][E19] camera history advances only after color adoption") {
	CHECK(RSG::viewport != nullptr);
	if (!RSG::viewport) {
		return;
	}
	RID viewport = RSG::viewport->viewport_allocate();
	RSG::viewport->viewport_initialize(viewport);
	RendererViewport::Viewport::ViewPresentationKey key;
	key.view_id = 71;
	key.view_epoch = 5;
	key.present_seq = 1;
	CHECK(RSG::viewport->viewport_register_presentation_key(viewport, key));
	const auto token = RSG::viewport->viewport_begin_camera_capture(viewport);
	CHECK(token.is_valid());
	RendererSceneRender::CameraData camera_data{};
	camera_data.source_view_id = token.view_id;
	camera_data.source_view_epoch = token.view_epoch;
	camera_data.source_capture_ordinal = token.capture_ordinal;

	CHECK(RSG::viewport->viewport_stage_camera_data(viewport, &camera_data, token));
	const auto *before_adoption = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(before_adoption != nullptr);
	CHECK_FALSE(before_adoption->has_source_token());
	CHECK_FALSE(RSG::viewport->viewport_get_previous_camera_token(viewport).is_valid());

	CHECK(RSG::viewport->viewport_adopt_camera_data(viewport));
	const auto *after_adoption = RSG::viewport->viewport_get_prev_camera_data(viewport);
	REQUIRE(after_adoption != nullptr);
	CHECK(after_adoption->source_view_id == token.view_id);
	CHECK(RSG::viewport->viewport_get_previous_camera_token(viewport) == token);
	CHECK_FALSE(RSG::viewport->viewport_adopt_camera_data(viewport));
	RendererSceneRender::CameraData mismatched = camera_data;
	const auto token_two = RSG::viewport->viewport_begin_camera_capture(viewport);
	CHECK(token_two.is_valid());
	mismatched.source_capture_ordinal = token_two.capture_ordinal + 1;
	CHECK_FALSE(RSG::viewport->viewport_stage_camera_data(viewport, &mismatched, token_two));
	RendererSceneRender::CameraData matching_two = mismatched;
	matching_two.source_capture_ordinal = token_two.capture_ordinal;
	CHECK(RSG::viewport->viewport_stage_camera_data(viewport, &matching_two, token_two));
	RSG::viewport->viewport_invalidate_presentation_key(viewport);
	CHECK_FALSE(RSG::viewport->viewport_adopt_camera_data(viewport));
	RSG::viewport->free(viewport);
}
