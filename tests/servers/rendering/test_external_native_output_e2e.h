/**************************************************************************/
/*  test_external_native_output_e2e.h                                     */
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

#pragma once

// Included inside TestProductionRequest::Access in tests/core/extension/test_production_request.cpp.
// Exercises Wave G-Godot fixes NATIVE-001, NATIVE-002, and NATIVE-S01 end-to-end
// through the frozen 398-entry public C ABI on both CPU state-machine fixtures
// and live Metal GPU (Mobile & ForwardPlus).

struct WaveGNativeOutputPorts {
	GDExtensionInterfaceDeviceSessionQualificationOpenV1 qual_open = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1 qual_storage_create = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1 qual_storage_retire = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1 qual_storage_observe = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1 qual_storage_release = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1 qual_unit_create = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1 qual_unit_retire = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1 qual_unit_observe = nullptr;
	GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1 qual_unit_release = nullptr;

	GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1 metadata_query = nullptr;
	GDExtensionInterfaceDeviceSessionOutputCreateV3 output_create_v3 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputWithdrawV1 output_withdraw = nullptr;

	GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1 req_quote_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2 req_quote_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReserveV2 reserve_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1 reserve_readbacks = nullptr;
	GDExtensionInterfaceDeviceSessionProductionImportStorageV1 import_storage = nullptr;
	GDExtensionInterfaceDeviceSessionProductionImportUnitV1 import_unit = nullptr;
	GDExtensionInterfaceDeviceSessionProductionSetCreateV1 set_create = nullptr;

	GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2 prepare_upload_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2 prepare_dispatch_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2 prepare_readback_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1 prepare_host_transform = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1 prepare_output_scene = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1 prepare_host_view = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadCameraReadyV1 read_camera_ready = nullptr;

	GDExtensionInterfaceDeviceSessionProductionPrepareOutputV3 prepare_output_v3 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV1 prepare_plan_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV2 prepare_plan_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionStageOutputPlanV2 stage_plan_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2 prepare_native_record_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2 consumed_prepare_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedReadV2 consumed_read_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2 consumed_borrow_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2 consumed_return_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1 record_schedule = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2 record_consume_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAckSegmentV1 ack_segment = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadRevisionV1 read_revision = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAppendV2 append_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionOutputAcquireV2 output_acquire_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionFinalizeOutputV1 output_finalize_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputSealV1 output_seal_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputCancelV1 output_cancel_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1 output_cancel_schedule = nullptr;
	GDExtensionInterfaceDeviceSessionProductionOutputReturnConsumeV1 output_return_consume = nullptr;

	GDExtensionInterfaceDeviceSessionProductionObserveV3 observe_v3 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionCopyV1 copy_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionCancelV1 cancel_v1 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReleaseV2 release_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionCloseV1 close_v1 = nullptr;

	GDExtensionInterfaceCallableCustomCreate2 callable_create = nullptr;
	GDExtensionInterfaceDeviceSessionCameraReadyRelayV1 camera_relay = nullptr;
	GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1 camera_bind_relay = nullptr;
	GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1 camera_bind_request = nullptr;
	GDExtensionInterfaceDeviceSessionCameraReadyCloseV1 camera_close = nullptr;

	GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1 scene_capability = nullptr;
	GDExtensionInterfaceDeviceSessionScenePhaseRelayV1 scene_relay = nullptr;
	GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1 scene_register = nullptr;
	GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1 scene_unregister = nullptr;

	template <typename T>
	static T get_fn(const char *p_name) {
		return reinterpret_cast<T>(GDExtension::get_interface_function(p_name));
	}

	bool load() {
		qual_open = get_fn<GDExtensionInterfaceDeviceSessionQualificationOpenV1>("device_session_qualification_open_v1");
		qual_storage_create = get_fn<GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1>("device_session_qualification_storage_create_v1");
		qual_storage_retire = get_fn<GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1>("device_session_qualification_storage_retire_v1");
		qual_storage_observe = get_fn<GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1>("device_session_qualification_storage_retire_observe_v1");
		qual_storage_release = get_fn<GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1>("device_session_qualification_storage_retire_release_v1");
		qual_unit_create = get_fn<GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1>("device_session_qualification_native_unit_create_v1");
		qual_unit_retire = get_fn<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1>("device_session_qualification_native_unit_retire_v1");
		qual_unit_observe = get_fn<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1>("device_session_qualification_native_unit_retire_observe_v1");
		qual_unit_release = get_fn<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1>("device_session_qualification_native_unit_retire_release_v1");

		metadata_query = get_fn<GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>("device_session_output_metadata_query_v1");
		output_create_v3 = get_fn<GDExtensionInterfaceDeviceSessionOutputCreateV3>("device_session_output_create_v3");
		output_withdraw = get_fn<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>("device_session_output_withdraw_v1");

		req_quote_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1>("device_session_production_request_requirements_v1");
		req_quote_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2>("device_session_production_request_requirements_v2");
		reserve_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		reserve_readbacks = get_fn<GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1>("device_session_production_reserve_readback_sources_v1");
		import_storage = get_fn<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
		import_unit = get_fn<GDExtensionInterfaceDeviceSessionProductionImportUnitV1>("device_session_production_import_unit_v1");
		set_create = get_fn<GDExtensionInterfaceDeviceSessionProductionSetCreateV1>("device_session_production_set_create_v1");

		prepare_upload_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		prepare_dispatch_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2>("device_session_production_prepare_dispatch_v2");
		prepare_readback_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		prepare_host_transform = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1>("device_session_production_prepare_host_transform_v1");
		prepare_output_scene = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1>("device_session_production_prepare_output_scene_v1");
		prepare_host_view = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1>("device_session_production_prepare_host_view_v1");
		read_camera_ready = get_fn<GDExtensionInterfaceDeviceSessionProductionReadCameraReadyV1>("device_session_production_read_camera_ready_v1");

		prepare_output_v3 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareOutputV3>("device_session_production_prepare_output_v3");
		prepare_plan_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV1>("device_session_production_prepare_output_plan_v1");
		prepare_plan_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV2>("device_session_production_prepare_output_plan_v2");
		stage_plan_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionStageOutputPlanV2>("device_session_production_stage_output_plan_v2");
		prepare_native_record_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2>("device_session_production_prepare_native_record_v2");

		consumed_prepare_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2>("device_session_output_consumed_prepare_v2");
		consumed_read_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedReadV2>("device_session_output_consumed_read_v2");
		consumed_borrow_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2>("device_session_output_consumed_borrow_v2");
		consumed_return_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2>("device_session_output_consumed_return_v2");

		record_schedule = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
		record_consume_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		ack_segment = get_fn<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>("device_session_production_ack_segment_v1");
		read_revision = get_fn<GDExtensionInterfaceDeviceSessionProductionReadRevisionV1>("device_session_production_read_revision_v1");
		append_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionAppendV2>("device_session_production_append_v2");

		output_acquire_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputAcquireV2>("device_session_output_acquire_v2");
		output_finalize_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionFinalizeOutputV1>("device_session_production_finalize_output_v1");
		output_seal_v1 = get_fn<GDExtensionInterfaceDeviceSessionOutputSealV1>("device_session_output_seal_v1");
		output_cancel_v1 = get_fn<GDExtensionInterfaceDeviceSessionOutputCancelV1>("device_session_output_cancel_v1");
		output_cancel_schedule = get_fn<GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1>("device_session_production_output_cancel_schedule_v1");
		output_return_consume = get_fn<GDExtensionInterfaceDeviceSessionProductionOutputReturnConsumeV1>("device_session_production_output_return_consume_v1");

		observe_v3 = get_fn<GDExtensionInterfaceDeviceSessionProductionObserveV3>("device_session_production_observe_v3");
		copy_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionCopyV1>("device_session_production_copy_v1");
		cancel_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
		release_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		close_v1 = get_fn<GDExtensionInterfaceDeviceSessionProductionCloseV1>("device_session_production_close_v1");

		callable_create = get_fn<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2");
		camera_relay = get_fn<GDExtensionInterfaceDeviceSessionCameraReadyRelayV1>("device_session_camera_ready_relay_v1");
		camera_bind_relay = get_fn<GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1>("device_session_camera_ready_bind_relay_v1");
		camera_bind_request = get_fn<GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1>("device_session_camera_ready_bind_request_v1");
		camera_close = get_fn<GDExtensionInterfaceDeviceSessionCameraReadyCloseV1>("device_session_camera_ready_close_v1");

		scene_capability = get_fn<GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1>("device_session_scene_phase_capability_v1");
		scene_relay = get_fn<GDExtensionInterfaceDeviceSessionScenePhaseRelayV1>("device_session_scene_phase_relay_v1");
		scene_register = get_fn<GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1>("device_session_scene_phase_register_v1");
		scene_unregister = get_fn<GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1>("device_session_scene_phase_unregister_v1");

		return qual_open && qual_storage_create && qual_storage_retire && qual_storage_observe && qual_storage_release &&
				qual_unit_create && qual_unit_retire && qual_unit_observe && qual_unit_release &&
				metadata_query && output_create_v3 && output_withdraw &&
				req_quote_v1 && req_quote_v2 && reserve_v2 && reserve_readbacks && import_storage && import_unit && set_create &&
				prepare_upload_v2 && prepare_dispatch_v2 && prepare_readback_v2 && prepare_host_transform && prepare_output_scene && prepare_host_view && read_camera_ready &&
				prepare_output_v3 && prepare_plan_v1 && prepare_plan_v2 && stage_plan_v2 && prepare_native_record_v2 &&
				consumed_prepare_v2 && consumed_read_v2 && consumed_borrow_v2 && consumed_return_v2 &&
				record_schedule && record_consume_v2 && ack_segment && read_revision && append_v2 &&
				output_acquire_v2 && output_finalize_v1 && output_seal_v1 && output_cancel_v1 && output_cancel_schedule && output_return_consume &&
				observe_v3 && copy_v1 && cancel_v1 && release_v2 && close_v1 &&
				callable_create && camera_relay && camera_bind_relay && camera_bind_request && camera_close &&
				scene_capability && scene_relay && scene_register && scene_unregister;
	}
};

static inline uint32_t wave_g_f2u(float p_val) {
	uint32_t bits = 0;
	memcpy(&bits, &p_val, sizeof(uint32_t));
	return bits;
}

static inline void wave_g_fill_instance192_record(uint32_t *r_words, float p_tx, float p_ty, float p_tz, uint32_t p_dense_id) {
	memset(r_words, 0, 192);
	// 3x4 affine row-major transform: identity basis + translation (tx, ty, tz)
	r_words[0] = wave_g_f2u(1.0f);
	r_words[3] = wave_g_f2u(p_tx);
	r_words[5] = wave_g_f2u(1.0f);
	r_words[7] = wave_g_f2u(p_ty);
	r_words[10] = wave_g_f2u(1.0f);
	r_words[11] = wave_g_f2u(p_tz);
	// Color / custom finite attributes (pad words 15, 19, 23, 47 must be zero!)
	r_words[12] = wave_g_f2u(0.9f);
	r_words[13] = wave_g_f2u(0.3f);
	r_words[14] = wave_g_f2u(0.1f);
	r_words[44] = p_dense_id;
	r_words[45] = 0u;
	r_words[46] = 1u; // flags bit 0 active, upper bits zero
	r_words[47] = 0u;
}

static inline void wave_g_fill_indexed144_segment(uint32_t *r_seg_words, uint32_t p_seg_index, float p_cx, float p_cy, float p_cz, uint32_t p_key_seed) {
	// 4 Float144 vertices per quad segment = 4 * 36 uint32 words = 576 bytes.
	const float offsets[4][2] = {
		{ -0.15f, -0.15f },
		{ 0.15f, -0.15f },
		{ -0.15f, 0.15f },
		{ 0.15f, 0.15f },
	};
	for (uint32_t v = 0; v < 4; ++v) {
		uint32_t *vw = r_seg_words + v * 36u;
		memset(vw, 0, 144);
		// Words 0..2: position.xyz; Word 3: pad zero
		vw[0] = wave_g_f2u(p_cx + offsets[v][0]);
		vw[1] = wave_g_f2u(p_cy + offsets[v][1]);
		vw[2] = wave_g_f2u(p_cz);
		vw[3] = 0u;
		// Words 4..6: normal.xyz; Word 7: pad zero
		vw[4] = wave_g_f2u(0.0f);
		vw[5] = wave_g_f2u(0.0f);
		vw[6] = wave_g_f2u(1.0f);
		vw[7] = 0u;
		// Words 8..11: tangent (1,0,0,1)
		vw[8] = wave_g_f2u(1.0f);
		vw[11] = wave_g_f2u(1.0f);
		// Words 12..15: RGBA color
		vw[12] = wave_g_f2u(0.8f);
		vw[13] = wave_g_f2u(0.4f);
		vw[14] = wave_g_f2u(0.2f);
		vw[15] = wave_g_f2u(1.0f);
		// Words 28..35: RibbonSegmentEndpointKey32 (8 x uint32)
		const uint32_t global_v = p_seg_index * 4u + v;
		for (uint32_t k = 0; k < 8u; ++k) {
			vw[28u + k] = p_key_seed + global_v * 17u + k;
		}
	}
}

static inline void wave_g_fill_light64_record(uint32_t *r_words, float p_x, float p_y, float p_z, float p_range) {
	memset(r_words, 0, 64);
	r_words[0] = wave_g_f2u(p_x);
	r_words[1] = wave_g_f2u(p_y);
	r_words[2] = wave_g_f2u(p_z);
	r_words[3] = wave_g_f2u(p_range);
	r_words[4] = wave_g_f2u(1.0f);
	r_words[5] = wave_g_f2u(0.8f);
	r_words[6] = wave_g_f2u(0.5f);
	r_words[7] = wave_g_f2u(2.0f);
}

static void run_wave_g_native_output_cpu() {
	using namespace GDExtensionDeviceExit;
	auto *server = ordinal_server();
	REQUIRE(server != nullptr);

	WaveGNativeOutputPorts ports;
	REQUIRE(ports.load());

	const auto baseline_counts = singleton_counts();

	{
		OrdinalPlan pool = {};
		pool.attachment_requests = 8;
		pool.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		pool.capacity.requests = 8;
		pool.capacity.suboperations = 32;
		pool.capacity.resources = 32;
		pool.capacity.resource_uses = 64;
		pool.capacity.graph_memberships = 64;
		pool.capacity.native_memberships = 128;
		pool.capacity.return_cells = 32;
		pool.capacity.retained_payload_bytes = 8192;

		SingletonEnrollment fixture;
		fixture.public_port = true;
		REQUIRE(fixture.prepare_device(server) == OK);
		REQUIRE(fixture.initialize_owner(
						1, false, true,
						+[](void *, ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {},
						nullptr, false, 1, false,
						+[](void *, GDExtensionProductionAttachmentRefV1) {},
						false, false, false, false, &pool, nullptr, true) == OK);
		REQUIRE(ports.qual_open(&Enrollment::ABI, fixture.ready, fixture.context, fixture.session, { fixture.instance, fixture.holder.handle_serial }, &fixture.qualification).native_error == OK);
		fixture.rd.draw_graph.initialize(&fixture.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		fixture.rd.draw_graph.begin();
		fixture.rd.upload_staging_buffers.block_size = 4096;
		fixture.rd.download_staging_buffers.block_size = 4096;
		fixture.rd.frames[0].qualification_epoch = 1;
		fixture.rd.frames[0].fence = RDD::FenceID(93);
		fixture.rd.frames[0].command_buffer = RDD::CommandBufferID(90);
		fixture.rd.frames[0].command_pool = RDD::CommandPoolID(94);
		fixture.rd.main_queue = RDD::CommandQueueID(1);
		fixture.rd.frames_drawn = 3;
		fixture.driver.output_storage_factory = true;

		const GDExtensionStorageCreateRequestV1 src_origin = { Enrollment::ABI, 32, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 src_backing = {};
		REQUIRE(ports.qual_storage_create(fixture.qualification, &src_origin, &src_backing).native_error == OK);
		REQUIRE(src_backing.rid != 0);

		const GDExtensionProductionServiceBudgetV1 budget = {
			production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
			OS::get_singleton()->get_ticks_usec() + 10000000,
			8, 32, 32, 0
		};

		auto cancel_and_reclaim = [&](GDExtensionProductionRequestRefV1 p_req) {
			REQUIRE(ports.cancel_v1(p_req).native_error == OK);
			GDExtensionProductionServiceBudgetV1 rem = {};
			GDExtensionProductionReportV2 rep = {};
			const Error rel_err = Error(ports.release_v2(p_req, &budget, &rem, &rep).native_error);
			CHECK((rel_err == OK || rel_err == ERR_BUSY));
			if (rep.service_pending) {
				server->sync();
				REQUIRE(ports.release_v2(p_req, &budget, &rem, &rep).native_error == OK);
			}
			CHECK(rep.service_pending == 0);
		};

		auto make_common_acquire_input = [](uint64_t p_particles, uint64_t p_gen) {
			GDExtensionOutputAcquireInputV1 in = {};
			in.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
			in.particles = p_particles;
			in.view_epoch = 1;
			in.pass_mask = 1;
			in.common.generation = p_gen;
			in.common.present_seq = 1;
			in.common.output_tag.bytes[0] = 0x11;
			in.common.view_tag.bytes[0] = 0x22;
			in.common.current_host_token.bytes[0] = 0x33;
			for (uint32_t a = 0; a < 3; ++a) {
				in.common.bounds.position[a] = -2.0;
				in.common.bounds.size[a] = 4.0;
			}
			in.common.history_reset = 1;
			in.common.current_camera = { 7, 1, 1 };
			return in;
		};

		// =========================================================================
		// 1. NATIVE-001 + NATIVE-002: INSTANCE_192 (capacity = 2, motion = DISABLED)
		//    Verify BOTH call orders (PrepareV3 -> PlanV1 and PlanV1 -> PrepareV3),
		//    StageOutputPlanV2 <-> PrepareV3 + PrepareNativeRecordV2,
		//    AND fail-closed rejection when PrepareV3 is omitted or mismatched!
		// =========================================================================
		for (uint32_t mode = 0; mode < 5; ++mode) {
			// mode 0: PrepareV3 -> PrepareOutputPlanV1
			// mode 1: PrepareOutputPlanV1 -> PrepareV3
			// mode 2: PrepareV3 -> StageOutputPlanV2 -> PrepareNativeRecordV2
			// mode 3: StageOutputPlanV2 -> PrepareV3 -> PrepareNativeRecordV2
			// mode 4: Negative check — PrepareOutputPlanV1 with zeroed roles & NO PrepareV3 -> record_schedule refuses ERR_UNCONFIGURED!
			const uint64_t gen = 500 + mode;
			const GDExtensionProductionResourceV1 resources[3] = {
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 384, 0 },
				{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 },
			};
			const GDExtensionProductionUseV1 uses[5] = {
				{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
				{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 384, 0 },
				{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
				{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 384, 0 },
				{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 },
			};
			const GDExtensionProductionActionV2 actions[5] = {
				{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 3, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
				{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 3, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
				{ 2, 10, 3, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 4, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
				{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
				{ 4, 10, 5, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			};
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 2000 + mode, 1, 10, 15 };
			GDExtensionProductionCapacityV1 cap = {};
			cap.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
			cap.requests = 1;
			cap.suboperations = 5;
			cap.resources = 3;
			cap.resource_uses = 32;
			cap.graph_memberships = 32;
			cap.native_memberships = 64;
			cap.return_cells = 4;
			cap.retained_payload_bytes = 512;

			GDExtensionProductionRequestRefV1 req = {};
			REQUIRE(ports.reserve_v2(fixture.attachment, &op, &cap, actions, 5, resources, 3, uses, 5, nullptr, 0, &req).native_error == OK);
			REQUIRE(ports.import_storage(req, 0, &src_backing).native_error == OK);

			uint8_t dummy_curr[384] = {};
			uint8_t dummy_meta[32] = {};
			const GDExtensionProductionUploadV2 up1 = { production_dto_abi<GDExtensionProductionUploadV2>(), 1, 10, 2, 0, 1, 0, 0, 384 };
			const GDExtensionProductionUploadV2 up2 = { production_dto_abi<GDExtensionProductionUploadV2>(), 2, 10, 3, 0, 2, 0, 0, 32 };
			REQUIRE(ports.prepare_upload_v2(req, &up1, dummy_curr, sizeof(dummy_curr)).native_error == OK);
			REQUIRE(ports.prepare_upload_v2(req, &up2, dummy_meta, sizeof(dummy_meta)).native_error == OK);

			auto acq_in = make_common_acquire_input(9001, gen);
			const bool is_staged = (mode == 2 || mode == 3);
			if (is_staged) {
				acq_in.view_epoch = 0;
				acq_in.common.present_seq = 0;
				acq_in.common.current_camera = { 0, 0, 0 };
			}
			{
				MutexLock lock(fixture.registry.mutex);
				ProductionRequest *internal_req = nullptr;
				RenderingDevice *actual_rd = nullptr;
				REQUIRE(fixture.registry.production_request_locked(req, false, internal_req, actual_rd) == OK);
				internal_req->actions[0].output_scene_prepared = true;
				internal_req->actions[0].output_scene.particles = acq_in.particles;
				internal_req->actions[0].output_scene.generation = gen;
				if (is_staged) {
					internal_req->actions[0].host_view_prepared = true;
					internal_req->actions[0].preinput_host_prepared = true;
					memcpy(internal_req->actions[0].preinput_host_transform.host_token.bytes, acq_in.common.current_host_token.bytes, 32);
				}
			}

			GDExtensionProductionOutputPrepareV3 prep_v3 = {};
			prep_v3.abi = production_dto_abi<GDExtensionProductionOutputPrepareV3>();
			prep_v3.action_ordinal = 0;
			prep_v3.canonical_suboperation = 10;
			prep_v3.source_recipe_substep = 1;
			prep_v3.source_resource_slot = 0;
			prep_v3.source_use_index = 0;
			prep_v3.first_source_use = 0;
			prep_v3.source_use_count = 1;
			prep_v3.read_capacity = 2;
			prep_v3.read_graph_capacity = 8;
			prep_v3.source_generation = 1;
			prep_v3.input = acq_in;
			for (uint32_t i = 0; i < 8; ++i) {
				prep_v3.roles[i] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			}
			prep_v3.roles[0] = { 1, 1, 1, 0, gen, 384 }; // CURRENT_RECORDS (2 * 192B)
			prep_v3.roles[1] = { 7, 2, 2, 0, gen, 32 }; // OUTPUT_META (32B), NO PREVIOUS_RECORDS!

			GDExtensionProductionOutputPlanV1 plan_v1 = {};
			plan_v1.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
			plan_v1.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
			plan_v1.acquire.action_ordinal = 0;
			plan_v1.acquire.canonical_suboperation = 10;
			plan_v1.acquire.source_recipe_substep = 1;
			plan_v1.acquire.source_resource_slot = 0;
			plan_v1.acquire.source_use_index = 0;
			plan_v1.acquire.first_source_use = 0;
			plan_v1.acquire.source_use_count = 1;
			plan_v1.acquire.read_capacity = 2;
			plan_v1.acquire.read_graph_capacity = 8;
			plan_v1.acquire.source_generation = 1;
			plan_v1.acquire.input = acq_in;
			// Leave plan_v1.acquire.roles zeroed so PrepareV3 supplies the exact v3 role bindings!
			plan_v1.finalize_action_ordinal = 3;
			plan_v1.seal_action_ordinal = 4;
			plan_v1.finalize_canonical_suboperation = 10;
			plan_v1.seal_canonical_suboperation = 10;
			plan_v1.finalize_source_recipe_substep = 4;
			plan_v1.seal_source_recipe_substep = 5;
			plan_v1.source_space = 2;
			for (uint32_t s = 0; s < 5; ++s) {
				plan_v1.reads[s] = { 0, 0, 1, s };
			}
			plan_v1.guard_work_limit = 8;
			plan_v1.guard_dispatch_limit = 4;
			plan_v1.guard_graph_limit = 8;
			plan_v1.guard_native_limit = 16;
			plan_v1.guard_owned_resource_limit = 4;

			if (mode == 0) {
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
				// Mismatched PlanV1 roles[1] (claiming motion=384 when PrepareV3 bound no-motion=0) must fail closed!
				auto bad_plan = plan_v1;
				bad_plan.acquire.roles[0] = { 1, 1, 1, 0, gen, 384 };
				bad_plan.acquire.roles[1] = { 2, 1, 1, 0, gen, 384 };
				bad_plan.acquire.roles[2] = { 7, 2, 2, 0, gen, 32 };
				CHECK(ports.prepare_plan_v1(req, &bad_plan).native_error == ERR_INVALID_DATA);

				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				REQUIRE(ports.prepare_native_record_v2(req).native_error == OK);
			} else if (mode == 1) {
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
				REQUIRE(ports.prepare_native_record_v2(req).native_error == OK);
			} else if (mode == 2) {
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
				REQUIRE(ports.stage_plan_v2(req, &plan_v1, &budget).native_error == OK);
			} else if (mode == 3) {
				REQUIRE(ports.stage_plan_v2(req, &plan_v1, &budget).native_error == OK);
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
			} else {
				// mode 4: PlanV1 alone with zeroed roles and NO PrepareV3 must NOT be schedulable!
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				CHECK(ports.prepare_native_record_v2(req).native_error == ERR_UNCONFIGURED);
				uint64_t refused_ctrl = 0;
				CHECK(ports.record_schedule(req, &refused_ctrl) == ERR_UNCONFIGURED);
				CHECK(refused_ctrl == 0);
			}

			if (mode < 4) {
				ProductionRequest *internal_req = nullptr;
				RenderingDevice *actual_rd = nullptr;
				{
					MutexLock lock(fixture.registry.mutex);
					REQUIRE(fixture.registry.production_request_locked(req, false, internal_req, actual_rd) == OK);
					CHECK(internal_req->actions[0].prepared);
					CHECK(internal_req->actions[0].output_plan_prepared);
					CHECK(internal_req->actions[0].output_prepare_version == 3);
					CHECK_FALSE(internal_req->actions[0].output_roles_pending_v3);
					CHECK(internal_req->actions[0].output_role_count == 3);
					CHECK(internal_req->actions[0].output_roles[0].bytes == 384);
					CHECK(internal_req->actions[0].output_roles[1].bytes == 0);
					CHECK(internal_req->actions[0].output_roles[2].bytes == 32);
					CHECK(internal_req->actions[0].output_camera_pending == is_staged);
					CHECK(internal_req->actions[3].prepared);
					CHECK(internal_req->actions[4].prepared);
				}
			}

			cancel_and_reclaim(req);
		}

		// =========================================================================
		// 2. NATIVE-002: PARTICLE_LIGHT_64 (capacity = 4) & INDEXED_144 (capacity = 2)
		//    Verify BOTH call orders (PrepareV3 <-> PlanV1 / PlanV2 / StageOutputPlanV2)
		// =========================================================================
		for (uint32_t profile_case = 0; profile_case < 4; ++profile_case) {
			// 0: PARTICLE_LIGHT_64 PrepareV3 -> PlanV1
			// 1: PARTICLE_LIGHT_64 PlanV1 -> PrepareV3
			// 2: INDEXED_144 (motion=REQUIRED) PrepareV3 -> PlanV2 & StageOutputPlanWithHistoryGuardV2 -> PrepareV3
			// 3: INDEXED_144 (motion=DISABLED) PlanV1 -> PrepareV3
			const bool is_light = (profile_case < 2);
			const bool is_idx_motion = (profile_case == 2);
			const uint64_t gen = 600 + profile_case;

			Vector<GDExtensionProductionResourceV1> res_vec;
			res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 });
			if (is_light) {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 8, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 256, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
			} else if (is_idx_motion) {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 1152, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 4, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 1152, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 5, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 6, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 48, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
			} else {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 1152, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 6, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 48, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
			}

			Vector<GDExtensionProductionUseV1> use_vec;
			use_vec.push_back({ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 });
			for (int i = 1; i < res_vec.size(); ++i) {
				use_vec.push_back({ uint32_t(i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, res_vec[i].physical_bytes, 0 });
			}
			const uint32_t acq_use_count = uint32_t(use_vec.size());
			Vector<GDExtensionProductionActionV2> act_vec;
			act_vec.push_back({ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, acq_use_count, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });
			for (int i = 1; i < res_vec.size(); ++i) {
				const uint32_t u_idx = uint32_t(use_vec.size());
				use_vec.push_back({ uint32_t(i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, res_vec[i].physical_bytes, 0 });
				act_vec.push_back({ uint32_t(act_vec.size()), 10, uint32_t(1 + i), 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, u_idx, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX });
			}
			const uint32_t fin_ord = uint32_t(act_vec.size());
			act_vec.push_back({ fin_ord, 10, 20, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });
			const uint32_t seal_ord = uint32_t(act_vec.size());
			act_vec.push_back({ seal_ord, 10, 21, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });

			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 2100 + profile_case, 1, 10, 25 };
			GDExtensionProductionCapacityV1 cap = {};
			cap.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
			cap.requests = 1;
			cap.suboperations = uint32_t(act_vec.size());
			cap.resources = uint32_t(res_vec.size());
			cap.resource_uses = uint32_t(use_vec.size()) + 16;
			cap.graph_memberships = 32;
			cap.native_memberships = 64;
			cap.return_cells = 8;
			cap.retained_payload_bytes = 4096;

			GDExtensionProductionRequestRefV1 req = {};
			REQUIRE(ports.reserve_v2(fixture.attachment, &op, &cap, act_vec.ptr(), uint32_t(act_vec.size()), res_vec.ptr(), uint32_t(res_vec.size()), use_vec.ptr(), uint32_t(use_vec.size()), nullptr, 0, &req).native_error == OK);
			REQUIRE(ports.import_storage(req, 0, &src_backing).native_error == OK);
			for (int i = 1; i < res_vec.size(); ++i) {
				Vector<uint8_t> zeros;
				zeros.resize(res_vec[i].physical_bytes);
				memset(zeros.ptrw(), 0, zeros.size());
				const GDExtensionProductionUploadV2 up = {
					production_dto_abi<GDExtensionProductionUploadV2>(),
					uint32_t(i), 10, uint32_t(1 + i), 0,
					uint32_t(i), 0, 0, res_vec[i].physical_bytes
				};
				REQUIRE(ports.prepare_upload_v2(req, &up, zeros.ptr(), zeros.size()).native_error == OK);
			}

			const auto acq_in = make_common_acquire_input(9100 + profile_case, gen);
			{
				MutexLock lock(fixture.registry.mutex);
				ProductionRequest *internal_req = nullptr;
				RenderingDevice *actual_rd = nullptr;
				REQUIRE(fixture.registry.production_request_locked(req, false, internal_req, actual_rd) == OK);
				internal_req->actions[0].output_scene_prepared = true;
				internal_req->actions[0].output_scene.particles = acq_in.particles;
				internal_req->actions[0].output_scene.generation = gen;
			}
			GDExtensionProductionOutputPrepareV3 prep_v3 = {};
			prep_v3.abi = production_dto_abi<GDExtensionProductionOutputPrepareV3>();
			prep_v3.action_ordinal = 0;
			prep_v3.canonical_suboperation = 10;
			prep_v3.source_recipe_substep = 1;
			prep_v3.source_resource_slot = 0;
			prep_v3.source_use_index = 0;
			prep_v3.first_source_use = 0;
			prep_v3.source_use_count = 1;
			prep_v3.read_capacity = is_light ? 0 : 2;
			prep_v3.read_graph_capacity = is_light ? 0 : 8;
			prep_v3.source_generation = 1;
			prep_v3.input = acq_in;
			for (uint32_t i = 0; i < 8; ++i) {
				prep_v3.roles[i] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			}
			if (is_light) {
				prep_v3.input.pass_mask = 0;
				prep_v3.roles[0] = { 8, 1, 1, 0, gen, 256 };
				prep_v3.roles[1] = { 7, 2, 2, 0, gen, 32 };
			} else if (is_idx_motion) {
				prep_v3.roles[0] = { 3, 1, 1, 0, gen, 1152 };
				prep_v3.roles[1] = { 4, 2, 2, 0, gen, 1152 };
				prep_v3.roles[2] = { 5, 3, 3, 0, gen, 32 };
				prep_v3.roles[3] = { 6, 4, 4, 0, gen, 48 };
				prep_v3.roles[4] = { 7, 5, 5, 0, gen, 32 };
			} else {
				prep_v3.roles[0] = { 3, 1, 1, 0, gen, 1152 };
				prep_v3.roles[1] = { 6, 2, 2, 0, gen, 48 };
				prep_v3.roles[2] = { 7, 3, 3, 0, gen, 32 };
			}

			GDExtensionProductionOutputPlanV1 plan_v1 = {};
			plan_v1.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
			plan_v1.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
			plan_v1.acquire.action_ordinal = 0;
			plan_v1.acquire.canonical_suboperation = 10;
			plan_v1.acquire.source_recipe_substep = 1;
			plan_v1.acquire.source_resource_slot = 0;
			plan_v1.acquire.source_use_index = 0;
			plan_v1.acquire.first_source_use = 0;
			plan_v1.acquire.source_use_count = 1;
			plan_v1.acquire.read_capacity = prep_v3.read_capacity;
			plan_v1.acquire.read_graph_capacity = prep_v3.read_graph_capacity;
			plan_v1.acquire.source_generation = 1;
			plan_v1.acquire.input = prep_v3.input;
			plan_v1.finalize_action_ordinal = fin_ord;
			plan_v1.seal_action_ordinal = seal_ord;
			plan_v1.finalize_canonical_suboperation = 10;
			plan_v1.seal_canonical_suboperation = 10;
			plan_v1.finalize_source_recipe_substep = 20;
			plan_v1.seal_source_recipe_substep = 21;
			plan_v1.source_space = 2;
			for (uint32_t s = 0; s < 5; ++s) {
				plan_v1.reads[s] = { 0, 0, 1, 0 };
			}
			plan_v1.guard_work_limit = 8;
			plan_v1.guard_dispatch_limit = 4;
			plan_v1.guard_graph_limit = 8;
			plan_v1.guard_native_limit = 16;
			plan_v1.guard_owned_resource_limit = 4;

			if (profile_case == 0) {
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
			} else if (profile_case == 1) {
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
			} else if (profile_case == 2) {
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
			} else {
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				REQUIRE(ports.prepare_output_v3(req, &prep_v3).native_error == OK);
			}
			REQUIRE(ports.prepare_native_record_v2(req).native_error == OK);

			{
				MutexLock lock(fixture.registry.mutex);
				ProductionRequest *internal_req = nullptr;
				RenderingDevice *actual_rd = nullptr;
				REQUIRE(fixture.registry.production_request_locked(req, false, internal_req, actual_rd) == OK);
				CHECK(internal_req->actions[0].prepared);
				CHECK(internal_req->actions[0].output_plan_prepared);
				CHECK(internal_req->actions[0].output_prepare_version == 3);
				CHECK_FALSE(internal_req->actions[0].output_roles_pending_v3);
				CHECK(internal_req->actions[0].output_role_count == (is_light ? 2u : 5u));
				CHECK(internal_req->actions[fin_ord].prepared);
				CHECK(internal_req->actions[seal_ord].prepared);
			}

			cancel_and_reclaim(req);
		}

		{
			QuoteDriver::QualificationCleanupScope cleanup_scope(fixture.driver);
			const GDExtensionStorageRetireRequestV1 ret_in = { Enrollment::ABI, src_backing.backing, src_backing.rid, src_backing.backing_serial, 0, 0, 1, 1, 0 };
			GDExtensionStorageRetireRefV1 ret_ref = {};
			GDExtensionStorageRetireReportV1 ret_rep = {};
			REQUIRE(ports.qual_storage_retire(fixture.qualification, &ret_in, &ret_ref, &ret_rep).native_error == OK);
			for (uint32_t turn = 0; turn < 32 && ret_rep.state != 1; ++turn) {
				const GDExtensionQualificationBudgetV1 qbudget = { Enrollment::ABI, 4, 5000, 0 };
				REQUIRE(ports.qual_storage_observe(fixture.qualification, ret_ref, &qbudget, &ret_rep).native_error == OK);
				server->sync();
			}
			REQUIRE(ret_rep.state == 1);
			REQUIRE(ports.qual_storage_release(fixture.qualification, ret_ref).native_error == OK);
		}

		fixture.cleanup_owner();
		CHECK(fixture.owner_cleaned);
	}

	const auto after_counts = singleton_counts();
	CHECK(after_counts.records == baseline_counts.records);
	CHECK(after_counts.attachments == baseline_counts.attachments);
}

struct LiveMetalWaveGNativeOutputHarness {
	SingletonEnrollment enrollment;
	WaveGNativeOutputPorts ports;
	RenderingServer *server = nullptr;
	RenderingDevice *device = nullptr;
	Dictionary checks;
	Vector<String> failures;
	Dictionary diagnostics;

	uint64_t next_nonce = 7000;
	uint32_t next_storage_attempt = 1;
	Callable normal_callable;
	GDExtensionNormalRelayRefV1 camera_relay_ref = {};
	uint32_t camera_relay_calls = 0;

	Callable scene_callable;
	GDExtensionNormalRelayRefV1 scene_relay_ref = {};
	GDExtensionScenePhaseRegistrationRefV1 scene_reg_ref = {};

	enum class ActiveRecordKind {
		NONE,
		UPLOAD_SINGLE,
		CREATE_OUTPUT,
		WITHDRAW_OUTPUT,
		CANCEL_SLOT,
		EXECUTE_PACKET,
	};

	struct PacketRecordPlan {
		GDExtensionProductionRequestRefV1 req = {};
		GDExtensionOutputAcquireInputV1 acquire_input = {};
		uint64_t acquire_ordinal = 0;
		Vector<uint64_t> upload_ordinals;
		uint64_t finalize_ordinal = 0;
		uint64_t seal_ordinal = 0;
		Vector<uint64_t> readback_ordinals;

		GDExtensionOutputAcquireResultV2 acquire_out = {};
		GDExtensionProductionOutputFinalizeResultV1 finalize_out = {};
		GDExtensionOutputSealResultV1 seal_out = {};
		Error upload_error = OK;
		Error readback_append_error = OK;
	};

	ActiveRecordKind active_kind = ActiveRecordKind::NONE;
	GDExtensionProductionRequestRefV1 active_req = {};
	GDExtensionOutputCreateInputV3 active_create_in = {};
	GDExtensionOutputCreateResultV1 active_create_out = {};
	uint64_t active_withdraw_particles = 0;
	uint64_t active_withdraw_gen = 0;
	GDExtensionOutputStatusV1 active_withdraw_out = {};
	uint64_t active_cancel_particles = 0;
	GDExtensionOutputTokenV1 active_cancel_lease = {};
	GDExtensionOutputStatusV1 active_cancel_out = {};
	PacketRecordPlan *active_packet = nullptr;

	struct NormalHostPrepContext {
		LiveMetalWaveGNativeOutputHarness *self = nullptr;
		GDExtensionProductionRequestRefV1 req = {};
		uint64_t ordinal = 0;
		GDExtensionProductionHostTransformInputV1 host_in = {};
		GDExtensionProductionOutputSceneInputV1 scene_in = {};
		GDExtensionProductionHostViewInputV1 view_in = {};
		Error err = ERR_UNCONFIGURED;
	};
	NormalHostPrepContext *active_normal_prep = nullptr;

	void record_check(const String &p_name, bool p_ok) {
		checks[p_name] = p_ok;
		if (!p_ok) {
			failures.push_back(p_name);
		}
	}

	static void on_normal_callable_free(void *) {}

	static void on_normal_callable_call(void *p_opaque, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr r_ret, GDExtensionCallError *r_err) {
		auto &self = *static_cast<LiveMetalWaveGNativeOutputHarness *>(p_opaque);
		*reinterpret_cast<Variant *>(r_ret) = Variant();
		*r_err = { GDEXTENSION_CALL_OK, 0, 0 };
		if (!self.active_normal_prep) {
			return;
		}
		auto &ctx = *self.active_normal_prep;
		Error e = Error(self.ports.prepare_host_transform(ctx.req, ctx.ordinal, &ctx.host_in).native_error);
		if (e == OK) {
			e = Error(self.ports.prepare_output_scene(ctx.req, ctx.ordinal, &ctx.scene_in).native_error);
		}
		if (e == OK) {
			e = Error(self.ports.prepare_host_view(ctx.req, ctx.ordinal, &ctx.view_in).native_error);
		}
		ctx.err = e;
	}

	static uint64_t opaque_word(const GDExtensionSceneOpaque32V1 &p_opaque, uint32_t p_index) {
		uint64_t val = 0;
		for (uint32_t i = 0; i < 8; ++i) {
			val |= uint64_t(p_opaque.bytes[p_index * 8 + i]) << (8 * i);
		}
		return val;
	}

	static void on_scene_callable_call(void *, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr r_ret, GDExtensionCallError *r_err) {
		*reinterpret_cast<Variant *>(r_ret) = Variant();
		*r_err = { GDEXTENSION_CALL_OK, 0, 0 };
	}

	static void on_scene_phase_typed(void *p_userdata, const GDExtensionNativeScenePhaseV1 *p_phase, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *p_op) {
		if (!p_phase) {
			return;
		}
		const GDExtensionDeviceExit::ProductionPhysicalContextRef ctx = {
			opaque_word(p_phase->physical_context, 0),
			opaque_word(p_phase->physical_context, 1)
		};
		on_record(p_userdata, ctx, p_req, p_op);
	}

	static void on_camera_relay(void *p_opaque, GDExtensionProductionAttachmentRefV1, const GDExtensionProductionCameraSampleV1 *) {
		auto &self = *static_cast<LiveMetalWaveGNativeOutputHarness *>(p_opaque);
		++self.camera_relay_calls;
	}

	static void on_record(void *p_userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef p_ctx, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *) {
		auto &self = *static_cast<LiveMetalWaveGNativeOutputHarness *>(p_userdata);
		if (p_req.handle_serial != self.active_req.handle_serial || p_req.registry_instance != self.active_req.registry_instance) {
			return;
		}
		if (self.active_kind == ActiveRecordKind::UPLOAD_SINGLE) {
			uint64_t rev = 0;
			if (self.ports.read_revision(p_ctx, p_req, &rev).native_error == OK) {
				GDExtensionProductionAppendV2 app = {};
				self.ports.append_v2(p_ctx, p_req, 0, rev, &app);
			}
			return;
		}
		if (self.active_kind == ActiveRecordKind::CREATE_OUTPUT) {
			self.ports.output_create_v3(p_ctx, p_req, &self.active_create_in, &self.active_create_out);
			return;
		}
		if (self.active_kind == ActiveRecordKind::WITHDRAW_OUTPUT) {
			const GDExtensionOutputGenerationInputV1 win = {
				GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputGenerationInputV1>(),
				self.active_withdraw_particles,
				self.active_withdraw_gen
			};
			self.active_withdraw_out = self.ports.output_withdraw(p_ctx, p_req, &win);
			return;
		}
		if (self.active_kind == ActiveRecordKind::CANCEL_SLOT) {
			const GDExtensionOutputCancelInputV1 cin = {
				GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputCancelInputV1>(),
				self.active_cancel_particles,
				self.active_cancel_lease
			};
			self.active_cancel_out = self.ports.output_cancel_v1(p_ctx, p_req, &cin);
			return;
		}
		if (self.active_kind == ActiveRecordKind::EXECUTE_PACKET && self.active_packet) {
			auto &pkt = *self.active_packet;
			self.ports.output_acquire_v2(p_ctx, p_req, pkt.acquire_ordinal, &pkt.acquire_input, &pkt.acquire_out);
			if (pkt.acquire_out.status.error != OK) {
				return;
			}
			for (int i = 0; i < pkt.upload_ordinals.size(); ++i) {
				uint64_t rev = 0;
				Error e = Error(self.ports.read_revision(p_ctx, p_req, &rev).native_error);
				if (e != OK) {
					pkt.upload_error = e;
					return;
				}
				GDExtensionProductionAppendV2 app = {};
				e = Error(self.ports.append_v2(p_ctx, p_req, pkt.upload_ordinals[i], rev, &app).native_error);
				if (e != OK) {
					pkt.upload_error = e;
					return;
				}
			}
			self.ports.output_finalize_v1(p_ctx, p_req, pkt.finalize_ordinal, &pkt.finalize_out);
			if (pkt.finalize_out.status.error != OK) {
				return;
			}
			GDExtensionOutputSealInputV1 seal_in = {};
			seal_in.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputSealInputV1>();
			seal_in.particles = pkt.acquire_input.particles;
			seal_in.lease_token = pkt.acquire_out.lease_token;
			seal_in.common = pkt.acquire_input.common;
			seal_in.record_token = pkt.finalize_out.record_token;
			self.ports.output_seal_v1(p_ctx, p_req, &seal_in, &pkt.seal_out);
			if (pkt.seal_out.status.error != OK) {
				return;
			}
			for (int i = 0; i < pkt.readback_ordinals.size(); ++i) {
				uint64_t rev = 0;
				Error e = Error(self.ports.read_revision(p_ctx, p_req, &rev).native_error);
				if (e != OK) {
					pkt.readback_append_error = e;
					return;
				}
				GDExtensionProductionAppendV2 app = {};
				e = Error(self.ports.append_v2(p_ctx, p_req, pkt.readback_ordinals[i], rev, &app).native_error);
				if (e != OK) {
					pkt.readback_append_error = e;
					return;
				}
			}
		}
	}

	GDExtensionProductionServiceBudgetV1 make_budget() const {
		return {
			GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
			OS::get_singleton()->get_ticks_usec() + 15000000,
			8, 64, 64, 0
		};
	}

	Error run_record_sync(GDExtensionProductionRequestRefV1 p_req, ActiveRecordKind p_kind, GDExtensionProductionRecordReportV2 &r_receipt) {
		active_req = p_req;
		active_kind = p_kind;
		uint64_t ctrl = 0;
		Error err = Error(ports.record_schedule(p_req, &ctrl));
		if (err != OK) {
			active_kind = ActiveRecordKind::NONE;
			return err;
		}
		server->sync();
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rd_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			if (p_rd) {
				GDExtensionDeviceExit::Registry::get_singleton().scene_phase_dispatch(*p_rd);
			}
		})).bind(uint64_t(uintptr_t(device))));
		server->sync();
		err = Error(ports.record_consume_v2(p_req, ctrl, &r_receipt).native_error);
		active_kind = ActiveRecordKind::NONE;
		if (err != OK) {
			return err;
		}
		if (r_receipt.result.native_error != OK) {
			return Error(r_receipt.result.native_error);
		}
		if (r_receipt.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
			GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
			if (owner_entry.get_error() != OK) {
				return owner_entry.get_error();
			}
			return Error(ports.ack_segment(p_req, r_receipt.segment.segment, r_receipt.segment.segment_revision).native_error);
		}
		return OK;
	}

	void rt_advance_rd_frame() {
		server->call_on_render_thread(callable_mp_static(+[]() {
			if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
				rd->_submit_transfer_workers();
				rd->_wait_for_transfer_workers();
				if (!rd->native_disposal_active) {
					rd->swap_buffers(false);
				}
			}
		}));
		server->sync();
	}

	Error pump_observe_once(GDExtensionProductionRequestRefV1 p_req, GDExtensionProductionObserveResultV3 &r_obs) {
		const auto budget = make_budget();
		Error err = Error(ports.observe_v3(p_req, &budget, &r_obs).native_error);
		if (err != OK && err != ERR_BUSY) {
			return err;
		}
		server->sync();
		const auto consume_budget = make_budget();
		err = Error(ports.observe_v3(p_req, &consume_budget, &r_obs).native_error);
		if (err != OK && err != ERR_BUSY) {
			return err;
		}
		if (err == ERR_BUSY || r_obs.report.service_pending != 0) {
			OS::get_singleton()->delay_usec(400);
		}
		return OK;
	}

	Error release_request_cleanly(GDExtensionProductionRequestRefV1 p_req) {
		ports.cancel_v1(p_req);
		for (uint32_t turn = 0; turn < 256; ++turn) {
			const auto budget = make_budget();
			GDExtensionProductionServiceBudgetV1 rem = {};
			GDExtensionProductionReportV2 rep = {};
			const Error err = Error(ports.release_v2(p_req, &budget, &rem, &rep).native_error);
			if (err != OK && err != ERR_BUSY) {
				return err;
			}
			if (err == OK && !rep.service_pending) {
				return OK;
			}
			if (rep.service_pending) {
				server->sync();
				const auto consume_budget = make_budget();
				const Error c_err = Error(ports.release_v2(p_req, &consume_budget, &rem, &rep).native_error);
				if (c_err != OK && c_err != ERR_BUSY) {
					return c_err;
				}
				if (c_err == OK && !rep.service_pending) {
					return OK;
				}
			}
			GDExtensionProductionObserveResultV3 obs = {};
			const Error obs_err = pump_observe_once(p_req, obs);
			if (obs_err != OK) {
				return obs_err;
			}
			rt_advance_rd_frame();
			OS::get_singleton()->delay_usec(300);
		}
		return ERR_TIMEOUT;
	}

	Error cancel_slot_and_release_request(const String &p_tag, GDExtensionProductionRequestRefV1 p_req, uint64_t p_particles, const GDExtensionOutputTokenV1 &p_lease) {
		active_req = p_req;
		active_kind = ActiveRecordKind::CANCEL_SLOT;
		active_cancel_particles = p_particles;
		active_cancel_lease = p_lease;
		active_cancel_out = { ERR_UNCONFIGURED, 0, nullptr };
		uint64_t ctrl = 0;
		const auto budget = make_budget();
		const Error sched_err = Error(ports.output_cancel_schedule(p_req, &budget, &ctrl));
		Error cons_err = ERR_UNCONFIGURED;
		GDExtensionProductionRecordReportV2 rec = {};
		if (sched_err == OK) {
			server->sync();
			cons_err = Error(ports.record_consume_v2(p_req, ctrl, &rec).native_error);
			if (cons_err == OK && (rec.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1)) {
				GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
				if (owner_entry.get_error() == OK) {
					(void)ports.ack_segment(p_req, rec.segment.segment, rec.segment.segment_revision);
				}
			}
			rt_advance_rd_frame();
		}
		active_kind = ActiveRecordKind::NONE;
		const Error rel_err = release_request_cleanly(p_req);
		if (sched_err != OK || cons_err != OK || rec.result.native_error != OK || active_cancel_out.error != OK || rel_err != OK) {
			Dictionary d;
			d["sched_err"] = int(sched_err);
			d["cons_err"] = int(cons_err);
			d["rec_err"] = int(rec.result.native_error);
			d["cancel_out_err"] = int(active_cancel_out.error);
			d["cancel_out_detail"] = active_cancel_out.detail ? String::utf8(active_cancel_out.detail, active_cancel_out.detail_length) : String();
			d["rel_err"] = int(rel_err);
			Dictionary snap;
			server->call_on_render_thread(callable_mp_static((+[](uint64_t p_part, uint64_t p_snap_addr) {
				if (auto *ps = RendererRD::ParticlesStorage::get_singleton()) {
					RID pipe;
					uint32_t dg = 0, cg = 0;
					(void)RendererRD::ExternalLinkedOwnerTest::staged_color_snapshot(*ps, RID::from_uint64(p_part), 0, Ref<RendererViewport::Viewport::CameraCapture>(), *reinterpret_cast<Dictionary *>(uintptr_t(p_snap_addr)), pipe, dg, cg);
				}
			})).bind(p_particles, uint64_t(uintptr_t(&snap))));
			server->sync();
			d["snap"] = snap;
			diagnostics[p_tag + ".cancel_slot"] = d;
		}
		return rel_err;
	}

	Error create_qualified_source_buffer(uint32_t p_alloc_slot, uint32_t p_active_count, GDExtensionStorageBackingV1 &r_backing, GDExtensionStorageCreateRequestV1 &r_origin) {
		const uint32_t source_words[8] = { p_active_count, 0, 0, 0, 0, 0, 0, 0 };
		r_origin = {};
		r_origin.abi = Enrollment::ABI;
		r_origin.bytes = sizeof(source_words);
		r_origin.allocation_slot = p_alloc_slot;
		r_origin.part = 0;
		r_origin.allocation_generation = 1;
		r_origin.create_attempt = next_storage_attempt++;

		Error create_err = ERR_UNCONFIGURED;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_self_addr, uint64_t p_origin_addr, uint64_t p_back_addr, uint64_t p_err_addr) {
			auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
			auto *orig_ptr = reinterpret_cast<const GDExtensionStorageCreateRequestV1 *>(uintptr_t(p_origin_addr));
			auto *back_ptr = reinterpret_cast<GDExtensionStorageBackingV1 *>(uintptr_t(p_back_addr));
			auto *err_ptr = reinterpret_cast<Error *>(uintptr_t(p_err_addr));
			*err_ptr = Error(self_ptr->ports.qual_storage_create(self_ptr->enrollment.qualification, orig_ptr, back_ptr).native_error);
		})).bind(uint64_t(uintptr_t(this)), uint64_t(uintptr_t(&r_origin)), uint64_t(uintptr_t(&r_backing)), uint64_t(uintptr_t(&create_err))));
		server->sync();
		if (create_err != OK) {
			diagnostics["qual_storage_create_err"] = int(create_err);
			return create_err;
		}
		rt_advance_rd_frame();

		// Upload source_words into r_backing via a tiny 1-action UPLOAD_V1 request on enrollment.attachment.
		const GDExtensionProductionResourceV1 res = {
			GDEXTENSION_PRODUCTION_STORAGE_V1, p_alloc_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, sizeof(source_words), 0
		};
		const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, sizeof(source_words), 0 };
		const GDExtensionProductionActionV2 act = {
			0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		const GDExtensionProductionUploadV2 up_dto = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 1, 0, 0, 0, 0, sizeof(source_words) };
		GDExtensionProductionMetadataQuoteV1 quote = {};
		Error err = Error(ports.req_quote_v1(enrollment.attachment, &act, 1, &res, 1, &use, 1, &up_dto, 1, nullptr, 0, 0, sizeof(source_words), &quote).native_error);
		if (err != OK) {
			diagnostics["src_quote_err"] = int(err);
			retire_qualified_source_buffer(r_backing, r_origin);
			return err;
		}
		const GDExtensionProductionOperationV1 op = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 11 };
		GDExtensionProductionRequestRefV1 up_req = {};
		err = Error(ports.reserve_v2(enrollment.attachment, &op, &quote.capacity, &act, 1, &res, 1, &use, 1, nullptr, 0, &up_req).native_error);
		if (err != OK) {
			diagnostics["src_reserve_err"] = int(err);
			retire_qualified_source_buffer(r_backing, r_origin);
			return err;
		}
		err = Error(ports.import_storage(up_req, 0, &r_backing).native_error);
		if (err != OK) {
			diagnostics["src_import_err"] = int(err);
			release_request_cleanly(up_req);
			retire_qualified_source_buffer(r_backing, r_origin);
			return err;
		}
		err = Error(ports.prepare_upload_v2(up_req, &up_dto, source_words, sizeof(source_words)).native_error);
		if (err != OK) {
			diagnostics["src_prep_upload_err"] = int(err);
			release_request_cleanly(up_req);
			retire_qualified_source_buffer(r_backing, r_origin);
			return err;
		}
		GDExtensionProductionRecordReportV2 rec = {};
		err = run_record_sync(up_req, ActiveRecordKind::UPLOAD_SINGLE, rec);
		if (err != OK) {
			diagnostics["src_record_err"] = int(err);
			release_request_cleanly(up_req);
			retire_qualified_source_buffer(r_backing, r_origin);
			return err;
		}
		// Submit RD frame and pump OBSERVE until GPU finishes source upload!
		rt_advance_rd_frame();
		for (uint32_t turn = 0; turn < 128; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_observe_once(up_req, obs) != OK) {
				break;
			}
			if (obs.report.known_obligations > 0 && obs.report.complete_obligations == obs.report.known_obligations && obs.report.possible_obligations == 0) {
				break;
			}
			rt_advance_rd_frame();
			OS::get_singleton()->delay_usec(300);
		}
		return release_request_cleanly(up_req);
	}

	Error retire_qualified_source_buffer(const GDExtensionStorageBackingV1 &p_backing, const GDExtensionStorageCreateRequestV1 &p_origin) {
		const GDExtensionStorageRetireRequestV1 ret_in = {
			Enrollment::ABI, p_backing.backing, p_backing.rid, p_backing.backing_serial,
			p_origin.allocation_slot, p_origin.part, p_origin.allocation_generation, 1, 0
		};
		GDExtensionStorageRetireRefV1 ret_ref = {};
		GDExtensionStorageRetireReportV1 ret_rep = {};
		Error err = ERR_UNCONFIGURED;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_self_addr, uint64_t p_in_addr, uint64_t p_ref_addr, uint64_t p_rep_addr, uint64_t p_err_addr) {
			auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
			auto *in_ptr = reinterpret_cast<const GDExtensionStorageRetireRequestV1 *>(uintptr_t(p_in_addr));
			auto *ref_ptr = reinterpret_cast<GDExtensionStorageRetireRefV1 *>(uintptr_t(p_ref_addr));
			auto *rep_ptr = reinterpret_cast<GDExtensionStorageRetireReportV1 *>(uintptr_t(p_rep_addr));
			auto *err_ptr = reinterpret_cast<Error *>(uintptr_t(p_err_addr));
			*err_ptr = Error(self_ptr->ports.qual_storage_retire(self_ptr->enrollment.qualification, in_ptr, ref_ptr, rep_ptr).native_error);
		})).bind(uint64_t(uintptr_t(this)), uint64_t(uintptr_t(&ret_in)), uint64_t(uintptr_t(&ret_ref)), uint64_t(uintptr_t(&ret_rep)), uint64_t(uintptr_t(&err))));
		server->sync();
		if (err != OK) {
			return err;
		}
		for (uint32_t turn = 0; turn < 128 && ret_rep.state != 1; ++turn) {
			const GDExtensionQualificationBudgetV1 qbudget = { Enrollment::ABI, 4, 5000, 0 };
			err = Error(ports.qual_storage_observe(enrollment.qualification, ret_ref, &qbudget, &ret_rep).native_error);
			if (err != OK) {
				return err;
			}
			if (ret_rep.state != 1) {
				rt_advance_rd_frame();
				OS::get_singleton()->delay_usec(300);
			}
		}
		if (ret_rep.state != 1) {
			return ERR_TIMEOUT;
		}
		return Error(ports.qual_storage_release(enrollment.qualification, ret_ref).native_error);
	}

	Error create_qualified_history_align_unit(uint32_t p_record_slot, GDExtensionNativeUnitBackingV1 &r_unit) {
		static constexpr const char *kHistoryAlignShader = R"GLSL(#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer PredecessorVertices {
	uint words[];
} pred_verts;

layout(set = 0, binding = 1, std430) readonly buffer CurrentVertices {
	uint words[];
} curr_verts;

layout(set = 0, binding = 2, std430) buffer AlignedPrevVertices {
	uint words[];
} prev_verts;

layout(set = 0, binding = 3, std430) buffer HistoryValidity {
	uint words[];
} hist_valid;

void main() {
	uint seg = gl_GlobalInvocationID.x;
	uint seg_count = uint(curr_verts.words.length()) / 144u;
	if (seg >= seg_count) {
		return;
	}
	uint pred_seg_count = uint(pred_verts.words.length()) / 144u;
	for (uint k = 0u; k < 4u; ++k) {
		uint cur_v = seg * 4u + k;
		uint cur_base = cur_v * 36u;
		uint found_base = 0xffffffffu;
		for (uint pv = 0u; pv < pred_seg_count * 4u; ++pv) {
			uint p_base = pv * 36u;
			bool match = true;
			for (uint key_w = 28u; key_w < 36u; ++key_w) {
				if (pred_verts.words[p_base + key_w] != curr_verts.words[cur_base + key_w]) {
					match = false;
					break;
				}
			}
			if (match) {
				found_base = p_base;
				break;
			}
		}
		if (found_base != 0xffffffffu) {
			for (uint w = 0u; w < 36u; ++w) {
				prev_verts.words[cur_base + w] = pred_verts.words[found_base + w];
			}
			hist_valid.words[cur_v] = 1u;
		} else {
			for (uint w = 0u; w < 36u; ++w) {
				prev_verts.words[cur_base + w] = curr_verts.words[cur_base + w];
			}
			hist_valid.words[cur_v] = 0u;
		}
	}
}
)GLSL";
		String diagnostic;
		const Vector<uint8_t> spirv = device->shader_compile_spirv_from_source(
				RenderingDevice::SHADER_STAGE_COMPUTE,
				String::utf8(kHistoryAlignShader),
				RenderingDevice::SHADER_LANGUAGE_GLSL,
				&diagnostic,
				false);
		if (spirv.is_empty() || !diagnostic.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		Vector<RenderingDevice::ShaderStageSPIRVData> stages;
		RenderingDevice::ShaderStageSPIRVData code;
		code.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
		code.spirv = spirv;
		stages.push_back(code);
		const Vector<uint8_t> container = device->shader_compile_binary_from_spirv_with_options(
				stages, "indexed144_history_align_writer", RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH);
		if (container.is_empty()) {
			return ERR_COMPILATION_FAILED;
		}
		GDExtensionNativeUnitCreateRequestV1 unit_origin = {};
		unit_origin.abi = Enrollment::ABI;
		unit_origin.library_generation = enrollment.holder.generation;
		unit_origin.preparation_generation = 1;
		unit_origin.operation_serial = ++next_nonce;
		unit_origin.record_slot = p_record_slot;
		unit_origin.physical_unit = 0;
		unit_origin.expected_flags = RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH;
		unit_origin.container_byte_count = container.size();
		if (CryptoCore::sha256(container.ptr(), container.size(), unit_origin.container_digest) != OK) {
			return ERR_INVALID_DATA;
		}
		r_unit = {};
		Error err = ERR_UNCONFIGURED;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_self_addr, uint64_t p_orig_addr, uint64_t p_cont_addr, uint64_t p_out_addr, uint64_t p_err_addr) {
			auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
			auto *orig_ptr = reinterpret_cast<const GDExtensionNativeUnitCreateRequestV1 *>(uintptr_t(p_orig_addr));
			auto *cont_ptr = reinterpret_cast<const Vector<uint8_t> *>(uintptr_t(p_cont_addr));
			auto *out_ptr = reinterpret_cast<GDExtensionNativeUnitBackingV1 *>(uintptr_t(p_out_addr));
			auto *err_ptr = reinterpret_cast<Error *>(uintptr_t(p_err_addr));
			*err_ptr = Error(self_ptr->ports.qual_unit_create(
					self_ptr->enrollment.qualification, orig_ptr, cont_ptr->ptr(), cont_ptr->size(), out_ptr).native_error);
		})).bind(uint64_t(uintptr_t(this)), uint64_t(uintptr_t(&unit_origin)), uint64_t(uintptr_t(&container)), uint64_t(uintptr_t(&r_unit)), uint64_t(uintptr_t(&err))));
		server->sync();
		return (err == OK && r_unit.state == 1 && r_unit.native_error == OK) ? OK : (err != OK ? err : ERR_CANT_CREATE);
	}

	Error retire_qualified_history_align_unit(const GDExtensionNativeUnitBackingV1 &p_unit) {
		const GDExtensionNativeUnitRetireRequestV1 ret_in = { Enrollment::ABI, p_unit.unit, 1, 0 };
		GDExtensionNativeUnitRetireRefV1 ret_ref = {};
		GDExtensionNativeUnitRetireReportV1 ret_rep = {};
		Error err = ERR_UNCONFIGURED;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_self_addr, uint64_t p_in_addr, uint64_t p_ref_addr, uint64_t p_rep_addr, uint64_t p_err_addr) {
			auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
			auto *in_ptr = reinterpret_cast<const GDExtensionNativeUnitRetireRequestV1 *>(uintptr_t(p_in_addr));
			auto *ref_ptr = reinterpret_cast<GDExtensionNativeUnitRetireRefV1 *>(uintptr_t(p_ref_addr));
			auto *rep_ptr = reinterpret_cast<GDExtensionNativeUnitRetireReportV1 *>(uintptr_t(p_rep_addr));
			auto *err_ptr = reinterpret_cast<Error *>(uintptr_t(p_err_addr));
			*err_ptr = Error(self_ptr->ports.qual_unit_retire(self_ptr->enrollment.qualification, in_ptr, ref_ptr, rep_ptr).native_error);
		})).bind(uint64_t(uintptr_t(this)), uint64_t(uintptr_t(&ret_in)), uint64_t(uintptr_t(&ret_ref)), uint64_t(uintptr_t(&ret_rep)), uint64_t(uintptr_t(&err))));
		server->sync();
		if (err != OK) {
			return err;
		}
		for (uint32_t turn = 0; turn < 128 && ret_rep.state != 1; ++turn) {
			const GDExtensionQualificationBudgetV1 qbudget = { Enrollment::ABI, 4, 5000, 0 };
			err = Error(ports.qual_unit_observe(enrollment.qualification, ret_ref, &qbudget, &ret_rep).native_error);
			if (err != OK) {
				return err;
			}
			if (ret_rep.state != 1) {
				rt_advance_rd_frame();
				OS::get_singleton()->delay_usec(300);
			}
		}
		if (ret_rep.state != 1) {
			return ERR_TIMEOUT;
		}
		return Error(ports.qual_unit_release(enrollment.qualification, ret_ref).native_error);
	}

	struct OutputOwnerSession {
		GDExtensionProductionRequestRefV1 create_req = {};
		GDExtensionOutputCreateResultV1 created = {};
		RID scenario;
		RID instance;
		RID viewport;
		RID camera;
		Ref<RenderSceneBuffers> camera_buffers;
		uint64_t view_id = 0;
		uint64_t view_epoch = 1;
	};

	struct CameraTestBuffers : public RenderSceneBuffers {
	public:
		void configure(const RenderSceneBuffersConfiguration *) override {}
		void set_fsr_sharpness(float) override {}
		void set_texture_mipmap_bias(float) override {}
		void set_anisotropic_filtering_level(RSE::ViewportAnisotropicFiltering) override {}
		void set_use_debanding(bool) override {}
	};

	Error setup_owner_session(
			uint32_t p_profile,
			uint32_t p_capacity,
			uint32_t p_motion,
			uint32_t p_max_inflight,
			RID p_mesh,
			RID p_material,
			OutputOwnerSession &r_sess) {
		using namespace GDExtensionDeviceExit;
		uint64_t mesh_ids[1] = { p_mesh.get_id() };
		uint64_t mat_ids[1] = { p_material.get_id() };

		GDExtensionOutputCreateInputV3 create_in = {};
		create_in.abi = { 3, sizeof(create_in), 0 };
		auto &cfg = create_in.config;
		cfg.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
		cfg.profile = p_profile;
		cfg.capacity = p_capacity;
		cfg.max_inflight = p_max_inflight;
		cfg.max_views = 2;
		cfg.space = 2; // WORLD
		cfg.motion = p_motion;
		cfg.pass_mask = (p_profile == 4) ? 0u : uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
		if (p_profile == 1) {
			cfg.surfaces = { mesh_ids, 1, 0 };
			cfg.surface_materials = { mat_ids, 1, 0 };
		} else if (p_profile == 2) {
			cfg.surfaces = { nullptr, 0, 0 };
			cfg.surface_materials = { mat_ids, 1, 0 };
			for (uint32_t a = 0; a < 3; ++a) {
				cfg.geometry_bounds.position[a] = -2.0;
				cfg.geometry_bounds.size[a] = 4.0;
			}
		} else {
			cfg.surfaces = { nullptr, 0, 0 };
			cfg.surface_materials = { nullptr, 0, 0 };
		}
		for (uint32_t a = 0; a < 3; ++a) {
			cfg.bounds.position[a] = -2.0;
			cfg.bounds.size[a] = 4.0;
		}
		cfg.producer_tag.bytes[0] = uint8_t(p_profile);
		cfg.device_tag.bytes[0] = 2;
		cfg.layout_digest.bytes[0] = 3;
		cfg.mapping_digest.bytes[0] = 4;
		cfg.numeric_profile_digest.bytes[0] = 5;

		GDExtensionOutputMetadataQueryInputV1 query = {};
		query.abi = production_dto_abi<GDExtensionOutputMetadataQueryInputV1>();
		query.config = cfg;
		query.limits = {
			production_dto_abi<GDExtensionOutputMetadataLimitsV1>(),
			4,
			(p_profile == 4) ? 0u : 4u,
			(p_profile == 4) ? 0u : 2u,
			0
		};
		GDExtensionOutputMetadataQueryResultV1 mquote = {};
		ports.metadata_query(&query, &mquote);
		if (mquote.status.error != OK) {
			return Error(mquote.status.error);
		}
		create_in.requirements = mquote.requirements;
		create_in.metadata_resource_slot = 0;

		const GDExtensionProductionResourceV1 meta_res = {
			GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, mquote.requirements.required_metadata_bytes, 0
		};
		const GDExtensionProductionActionV2 dummy_act = {
			0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX
		};
		// Reserve a clean 0-action request owning the OUTPUT_METADATA_V1 slot so CreateV3 executes in its RECORD.
		GDExtensionProductionCapacityV1 cap = {};
		cap.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		cap.requests = 1;
		cap.suboperations = 0;
		cap.resources = 1;
		cap.resource_uses = 64;
		cap.graph_memberships = 16;
		cap.return_cells = 4;
		{
			MutexLock reg_lock(enrollment.registry.mutex);
			MutexLock rd_lock(device->_thread_safe_);
			const Error be = enrollment.registry.production_membership_bound_locked(*device, cap.graph_memberships, device->transfer_worker_pool_max_size, cap.native_memberships);
			if (be != OK) {
				return be;
			}
		}
		(void)dummy_act;
		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 11 };
		Error err = Error(ports.reserve_v2(enrollment.attachment, &op, &cap, nullptr, 0, &meta_res, 1, nullptr, 0, nullptr, 0, &r_sess.create_req).native_error);
		if (err != OK) {
			return err;
		}
		active_create_in = create_in;
		active_create_out = {};
		GDExtensionProductionRecordReportV2 rec = {};
		err = run_record_sync(r_sess.create_req, ActiveRecordKind::CREATE_OUTPUT, rec);
		if (err != OK) {
			release_request_cleanly(r_sess.create_req);
			return err;
		}
		if (active_create_out.status.error != OK) {
			diagnostics["last_create_detail"] = active_create_out.status.detail
					? String::utf8(active_create_out.status.detail, active_create_out.status.detail_length)
					: String();
			release_request_cleanly(r_sess.create_req);
			return Error(active_create_out.status.error);
		}
		r_sess.created = active_create_out;

		// Set up real Scenario + Instance + Viewport + Camera on render thread.
		static Error s_rt_scene_err = OK;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_sess_addr) {
			auto *p_sess = reinterpret_cast<OutputOwnerSession *>(uintptr_t(p_sess_addr));
			auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
			p_sess->scenario = scene->scenario_allocate();
			scene->scenario_initialize(p_sess->scenario);
			p_sess->instance = scene->instance_allocate();
			scene->instance_initialize(p_sess->instance);
			scene->instance_set_base(p_sess->instance, RID::from_uint64(p_sess->created.particles));
			scene->instance_set_scenario(p_sess->instance, p_sess->scenario);
			scene->instance_set_transform(p_sess->instance, Transform3D());
			scene->instance_set_custom_aabb(p_sess->instance, AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8)));
			scene->update_dirty_instances();

			p_sess->viewport = RSG::viewport->viewport_allocate();
			RSG::viewport->viewport_initialize(p_sess->viewport);
			RSG::viewport->viewport_set_size(p_sess->viewport, 32, 32);
			RSG::viewport->viewport_set_scenario(p_sess->viewport, p_sess->scenario);
			p_sess->camera = scene->camera_allocate();
			scene->camera_initialize(p_sess->camera);
			scene->camera_set_orthogonal(p_sess->camera, 4.0f, 0.1f, 20.0f);
			scene->camera_set_transform(p_sess->camera, Transform3D(Basis(), Vector3(0, 0, 5)));
			RSG::viewport->viewport_attach_camera(p_sess->viewport, p_sess->camera);
			p_sess->camera_buffers = Ref<RenderSceneBuffers>(memnew(CameraTestBuffers));
			if (auto *vp = RSG::viewport->viewport_owner.get_or_null(p_sess->viewport)) {
				vp->render_buffers = p_sess->camera_buffers;
			}
			s_rt_scene_err = OK;
		})).bind(uint64_t(uintptr_t(&r_sess))));
		server->sync();
		return s_rt_scene_err;
	}

	Error withdraw_owner_session(OutputOwnerSession &p_sess) {
		using namespace GDExtensionDeviceExit;
		active_withdraw_particles = p_sess.created.particles;
		active_withdraw_gen = p_sess.created.generation;
		active_withdraw_out = { ERR_UNCONFIGURED, 0, nullptr };
		GDExtensionProductionRecordReportV2 rec = {};
		Error err = run_record_sync(p_sess.create_req, ActiveRecordKind::WITHDRAW_OUTPUT, rec);
		if (err != OK || active_withdraw_out.error != OK) {
			return err != OK ? err : Error(active_withdraw_out.error);
		}
		rt_advance_rd_frame();
		return OK;
	}

	Error finish_teardown_owner_session(OutputOwnerSession &p_sess) {
		using namespace GDExtensionDeviceExit;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_s_addr) {
			auto *p_s = reinterpret_cast<OutputOwnerSession *>(uintptr_t(p_s_addr));
			if (p_s->instance.is_valid()) {
				RSG::scene->free(p_s->instance);
				p_s->instance = RID();
			}
			if (p_s->camera.is_valid()) {
				RSG::scene->free(p_s->camera);
				p_s->camera = RID();
			}
			if (p_s->viewport.is_valid()) {
				if (auto *vp = RSG::viewport->viewport_owner.get_or_null(p_s->viewport)) {
					vp->render_buffers.unref();
				}
				RSG::viewport->free(p_s->viewport);
				p_s->viewport = RID();
			}
			if (p_s->scenario.is_valid()) {
				RSG::scene->free(p_s->scenario);
				p_s->scenario = RID();
			}
			p_s->camera_buffers.unref();
		})).bind(uint64_t(uintptr_t(&p_sess))));
		server->sync();

		Error err = OK;
		bool gen_consumed = false;
		for (uint32_t turn = 0; turn < 128; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			err = pump_observe_once(p_sess.create_req, obs);
			if (err != OK) {
				return err;
			}
			if (obs.output_generation.native_released) {
				const auto &term = obs.output_generation;
				const GDExtensionProductionOutputGenerationRefV1 expected = {
					production_dto_abi<GDExtensionProductionOutputGenerationRefV1>(),
					term.registry_instance, term.request_serial, term.control_serial,
					term.root_serial, term.particles, term.generation
				};
				GDExtensionProductionOutputGenerationResultV1 out_gen = {};
				err = Error(ports.output_return_consume(p_sess.create_req, &expected, &out_gen).native_error);
				if (err != OK || !out_gen.native_released) {
					return err != OK ? err : ERR_INVALID_DATA;
				}
				gen_consumed = true;
				break;
			}
			rt_advance_rd_frame();
			OS::get_singleton()->delay_usec(300);
		}
		if (!gen_consumed) {
			return ERR_TIMEOUT;
		}

		server->free_rid(RID::from_uint64(p_sess.created.particles));
		server->sync();
		return release_request_cleanly(p_sess.create_req);
	}

	Error teardown_owner_session(OutputOwnerSession &p_sess) {
		const Error w_err = withdraw_owner_session(p_sess);
		if (w_err != OK) {
			return w_err;
		}
		return finish_teardown_owner_session(p_sess);
	}

	Error prepare_host_and_capture_camera(
			OutputOwnerSession &p_sess,
			GDExtensionProductionRequestRefV1 p_req,
			uint64_t p_acq_ordinal,
			uint8_t p_host_token_byte,
			GDExtensionProductionCameraSampleV1 &r_sample) {
		using namespace GDExtensionDeviceExit;
		NormalHostPrepContext prep_ctx;
		prep_ctx.self = this;
		prep_ctx.req = p_req;
		prep_ctx.ordinal = p_acq_ordinal;
		prep_ctx.host_in.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
		prep_ctx.host_in.affine[0] = 1.0f;
		prep_ctx.host_in.affine[5] = 1.0f;
		prep_ctx.host_in.affine[10] = 1.0f;
		memset(prep_ctx.host_in.host_token.bytes, p_host_token_byte, 32);
		prep_ctx.scene_in = {
			production_dto_abi<GDExtensionProductionOutputSceneInputV1>(),
			p_sess.instance.get_id(),
			p_sess.created.particles,
			p_sess.created.generation,
			p_sess.scenario.get_id(),
			prep_ctx.host_in
		};
		prep_ctx.view_in = {
			production_dto_abi<GDExtensionProductionHostViewInputV1>(),
			p_sess.viewport.get_id(),
			p_sess.camera.get_id()
		};

		active_normal_prep = &prep_ctx;
		Variant ret;
		Callable::CallError ce;
		normal_callable.callp(nullptr, 0, ret, ce);
		active_normal_prep = nullptr;
		if (ce.error != Callable::CallError::CALL_OK || prep_ctx.err != OK) {
			return prep_ctx.err != OK ? prep_ctx.err : ERR_UNAUTHORIZED;
		}

		Error err = Error(ports.camera_bind_request(p_req, p_acq_ordinal, camera_relay_ref, p_sess.viewport.get_id()).native_error);
		if (err != OK) {
			return err;
		}
		// Pump OBSERVE so RT installs the camera observer on the viewport!
		GDExtensionProductionObserveResultV3 obs = {};
		err = pump_observe_once(p_req, obs);
		if (err != OK) {
			return err;
		}
		server->sync();
		err = pump_observe_once(p_req, obs);
		if (err != OK) {
			return err;
		}

		// Capture camera on RT, stage for post-frame adoption, and notify retained source!
		static Error s_cam_err = OK;
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_s_addr) {
			auto *p_s = reinterpret_cast<OutputOwnerSession *>(uintptr_t(p_s_addr));
			RendererSceneRender::CameraData cam_data;
			Projection proj;
			proj.set_orthogonal(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 20.0f);
			cam_data.set_camera(Transform3D(Basis(), Vector3(0, 0, 5)), proj, true, false, Vector2(), 0, 1);
			cam_data.source_viewport = p_s->viewport;
			const auto tok = RSG::viewport->viewport_begin_camera_capture(p_s->viewport, cam_data, p_s->camera, p_s->camera_buffers);
			if (!tok.is_valid()) {
				s_cam_err = ERR_UNAVAILABLE;
				return;
			}
			p_s->view_id = tok.view_id;
			p_s->view_epoch = tok.view_epoch;
			(void)RSG::viewport->viewport_stage_camera_data(p_s->viewport, &cam_data, tok);
			RenderDataRD rd_data;
			if (!rd_data.set_camera_source(tok.view_id, tok.view_epoch, tok.capture_ordinal) ||
					!rd_data.retain_native_camera_source(*RSG::viewport, cam_data, p_s->camera_buffers) ||
					!RSG::viewport->viewport_notify_camera_source_retained(p_s->viewport, rd_data.get_native_camera_source())) {
				s_cam_err = ERR_INVALID_DATA;
				return;
			}
			RSG::viewport->drain_camera_source_observers();
			s_cam_err = OK;
		})).bind(uint64_t(uintptr_t(&p_sess))));
		server->sync();
		if (s_cam_err != OK) {
			return s_cam_err;
		}
		return Error(ports.read_camera_ready(p_req, p_acq_ordinal, &r_sample).native_error);
	}

	// Genuine native color raster pass on Live Metal (Mobile & ForwardPlus) for sealed INSTANCE_192 or INDEXED_144 packet!
	struct Indexed144ExternalSurfaceHold {
		RID ext_mesh;
		RID curr_vb;
		RID prev_vb;
		RID idx_buf;
		RID idx_arr;
		uint32_t surf_idx = 0;
		uint32_t slot_index = UINT32_MAX;
		RID particles_rid;

		void release_untransferred_slot_hold(RID p_particles) {
			auto *ps = RendererRD::ParticlesStorage::get_singleton();
			auto *ms = RendererRD::MeshStorage::get_singleton();
			if (ps && ms && p_particles.is_valid() && slot_index != UINT32_MAX) {
				RendererRD::ExternalLinkedOwnerTest::wave_g_release_indexed144_surface_hold(*ps, *ms, p_particles, slot_index);
			}
		}

		void cleanup(RenderingDevice *p_rd, RID p_particles) {
			release_untransferred_slot_hold(p_particles);
			slot_index = UINT32_MAX;
			auto *ms = RendererRD::MeshStorage::get_singleton();
			if (ms && ext_mesh.is_valid()) {
				ms->mesh_clear(ext_mesh);
				ms->mesh_free(ext_mesh);
				ext_mesh = RID();
			}
			if (p_rd) {
				if (idx_arr.is_valid()) {
					p_rd->free_rid(idx_arr);
					idx_arr = RID();
				}
				if (idx_buf.is_valid()) {
					p_rd->free_rid(idx_buf);
					idx_buf = RID();
				}
				if (prev_vb.is_valid()) {
					p_rd->free_rid(prev_vb);
					prev_vb = RID();
				}
				if (curr_vb.is_valid()) {
					p_rd->free_rid(curr_vb);
					curr_vb = RID();
				}
			}
		}
	};

	RID shared_color_tex;
	RID shared_depth_tex;
	RID shared_fb;
	RD::FramebufferFormatID shared_fb_format = RD::INVALID_FORMAT_ID;
	Vector<Indexed144ExternalSurfaceHold> deferred_idx144_holds;

	void defer_idx144_cleanup(Indexed144ExternalSurfaceHold &p_hold) {
		p_hold.slot_index = UINT32_MAX;
		p_hold.particles_rid = RID();
		if (p_hold.ext_mesh.is_valid() || p_hold.curr_vb.is_valid() || p_hold.prev_vb.is_valid() || p_hold.idx_buf.is_valid() || p_hold.idx_arr.is_valid()) {
			deferred_idx144_holds.push_back(p_hold);
			p_hold = Indexed144ExternalSurfaceHold();
		}
	}

	Error prepare_indexed144_surface_hold(uint32_t p_capacity, RID p_material, Indexed144ExternalSurfaceHold &r_hold) {
		Error prep_err = ERR_UNCONFIGURED;
		server->call_on_render_thread(callable_mp_static((+[](uint32_t p_cap, RID p_mat, uint64_t p_rd_addr, uint64_t p_h_addr, uint64_t p_err_addr) {
			using namespace RendererRD;
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			auto *ms = MeshStorage::get_singleton();
			auto *h = reinterpret_cast<Indexed144ExternalSurfaceHold *>(uintptr_t(p_h_addr));
			auto *r_err = reinterpret_cast<Error *>(uintptr_t(p_err_addr));
			if (!p_rd || !ms || p_cap == 0) {
				*r_err = ERR_UNAVAILABLE;
				return;
			}
			const uint32_t vtx_count = p_cap * 4u;
			const uint32_t idx_count = p_cap * 6u;
			const uint32_t vtx_bytes = vtx_count * 144u;
			const uint32_t idx_bytes_size = idx_count * 4u;
			h->curr_vb = p_rd->vertex_buffer_create(vtx_bytes, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
			h->prev_vb = p_rd->vertex_buffer_create(vtx_bytes, {}, RD::BUFFER_CREATION_AS_STORAGE_BIT);
			h->idx_buf = p_rd->index_buffer_create(idx_count, RD::INDEX_BUFFER_FORMAT_UINT32, {}, false);
			h->idx_arr = p_rd->index_array_create(h->idx_buf, 0, idx_count);

			h->ext_mesh = ms->mesh_allocate();
			ms->mesh_initialize(h->ext_mesh);
			MeshStorage::ExternalSurfaceDescriptor desc;
			desc.primitive = RSE::PRIMITIVE_TRIANGLES;
			desc.vertex_count = vtx_count;
			desc.index_count = idx_count;
			desc.current_vertex_buffer = h->curr_vb;
			desc.previous_vertex_buffer = h->prev_vb;
			desc.vertex_buffer_size = vtx_bytes;
			desc.previous_vertex_buffer_size = vtx_bytes;
			desc.index_buffer = h->idx_buf;
			desc.index_array = h->idx_arr;
			desc.index_buffer_size = idx_bytes_size;
			desc.material = p_mat;
			desc.aabb = AABB(Vector3(-2, -2, -2), Vector3(4, 4, 4));
			desc.generation = 1;
			desc.current_allocation_generation = 11;
			desc.previous_allocation_generation = 12;
			uint64_t surf_gen = 0;
			*r_err = ms->mesh_add_external_surface(h->ext_mesh, desc, h->surf_idx, surf_gen);
			if (*r_err != OK) {
				return;
			}
			for (uint32_t f = 0; f < 4; ++f) {
				p_rd->_submit_transfer_workers();
				p_rd->_wait_for_transfer_workers();
				p_rd->swap_buffers(false);
			}
			if (auto *vbo = p_rd->vertex_buffer_owner.get_or_null(h->curr_vb)) {
				p_rd->_check_transfer_worker_buffer(vbo);
			}
			if (auto *pvbo = p_rd->vertex_buffer_owner.get_or_null(h->prev_vb)) {
				p_rd->_check_transfer_worker_buffer(pvbo);
			}
			if (auto *ibo = p_rd->index_buffer_owner.get_or_null(h->idx_buf)) {
				p_rd->_check_transfer_worker_buffer(ibo);
			}
			if (auto *iao = p_rd->index_array_owner.get_or_null(h->idx_arr)) {
				p_rd->_check_transfer_worker_index_array(iao);
			}
		})).bind(p_capacity, p_material, uint64_t(uintptr_t(device)), uint64_t(uintptr_t(&r_hold)), uint64_t(uintptr_t(&prep_err))));
		server->sync();
		return prep_err;
	}

	static Error execute_native_color_draw_in_scope_rt(
			LiveMetalWaveGNativeOutputHarness &p_self,
			OutputOwnerSession &p_sess,
			RenderingDevice *p_rd,
			uint32_t p_profile,
			uint32_t p_slot_index,
			RID p_material,
			const GDExtensionProductionCameraSampleV1 &p_cam_sample,
			Indexed144ExternalSurfaceHold *p_idx144_hold,
			const GDExtensionOutputCameraV1 *p_prev_camera,
			String &r_step) {
		using namespace RendererRD;
		auto *ps = ParticlesStorage::get_singleton();
		auto *ms = MeshStorage::get_singleton();
		auto *mats = MaterialStorage::get_singleton();
		auto *scene_render = RendererSceneRenderRD::get_singleton();
		const RID particles_rid = RID::from_uint64(p_sess.created.particles);
		uint32_t owner_capacity = 0;
		bool prev_omitted_unused = false;
		if (!p_rd || !ps || !ms || !mats || !scene_render || !p_self.shared_fb.is_valid() ||
				ExternalLinkedOwnerTest::wave_g_owner_capacity_and_prev_omitted(*ps, particles_rid, owner_capacity, prev_omitted_unused) != OK) {
			r_step = "singletons";
			return ERR_UNAVAILABLE;
		}
		const uint32_t backend = scene_render->get_native_material_recipe_backend();
		const RID fb = p_self.shared_fb;
		const RD::FramebufferFormatID fb_format = p_self.shared_fb_format;

		// For INDEXED_144 with capacity > 0, install the pre-created external Float144 surface hold on slot.surfaces[0].geometry.
		if (p_profile == 2 && owner_capacity > 0) {
			if (!p_idx144_hold || !p_idx144_hold->ext_mesh.is_valid()) {
				r_step = "idx_hold_null";
				return ERR_INVALID_PARAMETER;
			}
			const Error e = ExternalLinkedOwnerTest::wave_g_install_indexed144_surface_hold(*ps, *ms, particles_rid, p_slot_index, p_idx144_hold->ext_mesh, p_idx144_hold->surf_idx);
			if (e != OK) {
				r_step = "mesh_surface_hold_acquire";
				return e;
			}
			p_idx144_hold->slot_index = p_slot_index;
			p_idx144_hold->particles_rid = particles_rid;
		}

		// Build admitted RenderDataRD for this exact captured camera token.
		RendererSceneRender::CameraData cam_data;
		const RendererViewport::Viewport::CameraSourceToken tok = {
			p_cam_sample.camera.view_id,
			p_cam_sample.camera.view_epoch,
			p_cam_sample.camera.capture_ordinal
		};
		if (!RSG::viewport->viewport_get_camera_capture(p_sess.viewport, tok, cam_data)) {
			r_step = "viewport_get_camera_capture";
			return ERR_UNAVAILABLE;
		}
		RenderDataRD render_data;
		render_data.set_camera_source(tok.view_id, tok.view_epoch, tok.capture_ordinal);
		if (p_prev_camera) {
			render_data.set_previous_camera_source(p_prev_camera->view_id, p_prev_camera->view_epoch, p_prev_camera->capture_ordinal);
		} else {
			render_data.set_previous_camera_source(tok.view_id, tok.view_epoch, tok.capture_ordinal);
		}
		if (!render_data.retain_native_camera_source(*RSG::viewport, cam_data, p_sess.camera_buffers)) {
			r_step = "retain_native_camera_source";
			return ERR_UNAUTHORIZED;
		}

		ExternalSurfaceSelection sel;
		Error err = (p_profile == 2)
				? ps->particles_external_slot_surface_acquire(particles_rid, p_slot_index, 0, sel)
				: ps->particles_external_surface_acquire(particles_rid, 0, sel);
		if (err != OK) {
			r_step = "surface_acquire";
			return err;
		}

		String sub_step;
		for (uint32_t draw_try = 0; draw_try < 64; ++draw_try) {
			if (backend == 1) {
				err = TestProductionRequest::wave_g_mobile_draw_external(sel, &render_data, p_material, p_profile, owner_capacity > 0, fb_format, fb, sub_step);
			} else {
				err = TestProductionRequest::wave_g_clustered_draw_external(sel, &render_data, p_material, p_profile, owner_capacity > 0, fb_format, fb, sub_step);
			}
			if (err != ERR_BUSY) {
				break;
			}
			OS::get_singleton()->delay_usec(1000);
		}

		const Error rel_err = ps->particles_external_surface_release(sel);

		if (err != OK || rel_err != OK) {
			r_step = err != OK ? sub_step : String("surface_release");
			return err != OK ? err : rel_err;
		}
		r_step = "ok";
		return OK;
	}

	Error execute_packet_and_color_draw_in_cull_scope(
			OutputOwnerSession &p_sess,
			PacketRecordPlan &p_pkt,
			const GDExtensionProductionCameraSampleV1 &p_cam_sample,
			bool p_do_color_draw,
			uint32_t p_profile,
			uint32_t p_slot_index,
			RID p_material,
			Indexed144ExternalSurfaceHold *p_idx144_hold,
			Error &r_draw_err,
			GDExtensionProductionRecordReportV2 &r_receipt,
			const GDExtensionOutputCameraV1 *p_prev_camera = nullptr) {
		r_draw_err = p_do_color_draw ? ERR_UNCONFIGURED : OK;
		active_req = p_pkt.req;
		active_kind = ActiveRecordKind::EXECUTE_PACKET;
		active_packet = &p_pkt;

		uint64_t ctrl = 0;
		Error err = Error(ports.record_schedule(p_pkt.req, &ctrl));
		if (err != OK) {
			active_packet = nullptr;
			active_kind = ActiveRecordKind::NONE;
			return err;
		}
		server->sync(); // Admits p_pkt.req into QUEUED + scene_waiting == true.

		struct RtScopeArgs {
			LiveMetalWaveGNativeOutputHarness *self = nullptr;
			OutputOwnerSession *sess = nullptr;
			PacketRecordPlan *pkt = nullptr;
			uint64_t ctrl = 0;
			GDExtensionProductionRecordReportV2 *receipt = nullptr;
			GDExtensionProductionCameraSampleV1 sample = {};
			bool do_draw = false;
			uint32_t profile = 0;
			uint32_t slot_index = 0;
			RID material;
			Indexed144ExternalSurfaceHold *idx_hold = nullptr;
			const GDExtensionOutputCameraV1 *prev_cam = nullptr;
			Error scope_err = OK;
			Error consume_err = OK;
			Error draw_err = ERR_UNCONFIGURED;
			String draw_step;
		} rt_args;
		rt_args.self = this;
		rt_args.sess = &p_sess;
		rt_args.pkt = &p_pkt;
		rt_args.ctrl = ctrl;
		rt_args.receipt = &r_receipt;
		rt_args.sample = p_cam_sample;
		rt_args.do_draw = p_do_color_draw;
		rt_args.profile = p_profile;
		rt_args.slot_index = p_slot_index;
		rt_args.material = p_material;
		rt_args.idx_hold = p_idx144_hold;
		rt_args.prev_cam = p_prev_camera;

		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_addr) {
			auto *a = reinterpret_cast<RtScopeArgs *>(uintptr_t(p_addr));
			auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
			if (!scene || !RSG::viewport || !a->self->device) {
				a->scope_err = ERR_UNAVAILABLE;
				return;
			}
			const RendererViewport::Viewport::CameraSourceToken tok = {
				a->sample.camera.view_id,
				a->sample.camera.view_epoch,
				a->sample.camera.capture_ordinal
			};
			Ref<RendererViewport::Viewport::CameraCapture> capture;
			if (!RSG::viewport->viewport_get_camera_capture_ref(a->sess->viewport, tok, capture) || capture.is_null()) {
				a->scope_err = ERR_UNAVAILABLE;
				return;
			}

			RendererSceneCull::NativeOutputCullScope cull_scope(*scene, capture);
			GDExtensionDeviceExit::Registry::get_singleton().scene_phase_dispatch_for_phase(
					*a->self->device,
					GDExtensionDeviceExit::SCENE_PHASE_BEFORE_PARTICLES,
					a->sess->viewport);

			a->self->active_packet = nullptr;
			a->self->active_kind = ActiveRecordKind::NONE;

			Error cerr = Error(a->self->ports.record_consume_v2(a->pkt->req, a->ctrl, a->receipt).native_error);
			if (cerr != OK) {
				a->consume_err = cerr;
				return;
			}
			if (a->receipt->result.native_error != OK) {
				a->consume_err = Error(a->receipt->result.native_error);
				return;
			}
			if (a->receipt->flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
				GDExtensionDeviceExit::CallbackScope owner_entry(a->self->enrollment.reference);
				if (owner_entry.get_error() != OK) {
					a->consume_err = owner_entry.get_error();
					return;
				}
				cerr = Error(a->self->ports.ack_segment(a->pkt->req, a->receipt->segment.segment, a->receipt->segment.segment_revision).native_error);
				if (cerr != OK) {
					a->consume_err = cerr;
					return;
				}
			}

			if (a->do_draw &&
					a->pkt->acquire_out.status.error == OK &&
					a->pkt->upload_error == OK &&
					a->pkt->finalize_out.status.error == OK &&
					a->pkt->seal_out.status.error == OK) {
				a->draw_err = execute_native_color_draw_in_scope_rt(
						*a->self, *a->sess, a->self->device, a->profile, a->slot_index,
						a->material, a->sample, a->idx_hold, a->prev_cam, a->draw_step);
			}
		})).bind(uint64_t(uintptr_t(&rt_args))));
		server->sync();

		active_packet = nullptr;
		active_kind = ActiveRecordKind::NONE;

		if (rt_args.scope_err != OK) {
			diagnostics["last_rt_scope_err"] = int(rt_args.scope_err);
			return rt_args.scope_err;
		}
		if (rt_args.consume_err != OK) {
			return rt_args.consume_err;
		}

		if (p_do_color_draw) {
			r_draw_err = rt_args.draw_err;
			if (rt_args.draw_err != OK) {
				diagnostics["last_rt_draw_step"] = rt_args.draw_step;
				diagnostics["last_rt_draw_err"] = int(rt_args.draw_err);
			}
		}
		return OK;
	}

	// Single-frame end-to-end runner for INSTANCE_192 (no-motion), PARTICLE_LIGHT_64, and INDEXED_144 (no-motion).
	bool run_single_frame_profile(
			const String &p_label,
			uint32_t p_profile,
			uint32_t p_capacity,
			uint32_t p_motion,
			bool p_plan_before_v3,
			bool p_do_color_draw,
			RID p_mesh,
			RID p_material,
			const GDExtensionStorageBackingV1 &p_source,
			uint32_t p_source_slot) {
		using namespace GDExtensionDeviceExit;
		OutputOwnerSession sess;
		Error err = setup_owner_session(p_profile, p_capacity, p_motion, 1, p_mesh, p_material, sess);
		record_check(p_label + ".create_v3", err == OK && sess.created.particles != 0 && sess.created.generation != 0);
		if (err != OK) {
			diagnostics[p_label + ".create_err"] = int(err);
			return false;
		}
		const uint64_t gen = sess.created.generation;

		// Build resources, uses, actions, and readback for GUARD_STATUS.
		Vector<GDExtensionProductionResourceV1> res_vec;
		res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, p_source_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 });

		struct RoleUploadSpec {
			uint32_t role;
			Vector<uint8_t> payload;
		};
		Vector<RoleUploadSpec> uploads;

		const uint32_t meta_words[8] = { p_capacity, 0, 0, 0, p_capacity, p_profile, 1u, 0u };
		Vector<uint8_t> meta_bytes;
		meta_bytes.resize(32);
		memcpy(meta_bytes.ptrw(), meta_words, 32);

		if (p_profile == 1) {
			// INSTANCE_192 (motion == DISABLED, capacity = 2 -> 384B current + 32B meta)
			Vector<uint8_t> curr_bytes;
			curr_bytes.resize(p_capacity * 192u);
			for (uint32_t i = 0; i < p_capacity; ++i) {
				wave_g_fill_instance192_record(reinterpret_cast<uint32_t *>(curr_bytes.ptrw() + i * 192u), float(i) * 0.25f, 0.0f, 0.0f, i + 1u);
			}
			uploads.push_back({ 1u, curr_bytes });
			uploads.push_back({ 7u, meta_bytes });
		} else if (p_profile == 4) {
			// PARTICLE_LIGHT_64 (capacity = 4 -> 256B lights + 32B meta)
			Vector<uint8_t> light_bytes;
			light_bytes.resize(p_capacity * 64u);
			for (uint32_t i = 0; i < p_capacity; ++i) {
				wave_g_fill_light64_record(reinterpret_cast<uint32_t *>(light_bytes.ptrw() + i * 64u), float(i) * 0.2f, 0.1f, 0.0f, 2.5f);
			}
			uploads.push_back({ 8u, light_bytes });
			uploads.push_back({ 7u, meta_bytes });
		} else {
			// INDEXED_144 (motion == DISABLED, capacity = 2 -> 1152B curr + 48B idx + 32B meta)
			Vector<uint8_t> curr_bytes;
			curr_bytes.resize(p_capacity * 576u);
			for (uint32_t s = 0; s < p_capacity; ++s) {
				wave_g_fill_indexed144_segment(reinterpret_cast<uint32_t *>(curr_bytes.ptrw() + s * 576u), s, float(s) * 0.4f - 0.2f, 0.0f, 0.0f, 0xA0510000u);
			}
			Vector<uint8_t> idx_bytes;
			idx_bytes.resize(p_capacity * 24u);
			uint32_t *iw = reinterpret_cast<uint32_t *>(idx_bytes.ptrw());
			for (uint32_t s = 0; s < p_capacity; ++s) {
				iw[s * 6 + 0] = s * 4 + 0;
				iw[s * 6 + 1] = s * 4 + 1;
				iw[s * 6 + 2] = s * 4 + 2;
				iw[s * 6 + 3] = s * 4 + 2;
				iw[s * 6 + 4] = s * 4 + 1;
				iw[s * 6 + 5] = s * 4 + 3;
			}
			uploads.push_back({ 3u, curr_bytes });
			uploads.push_back({ 6u, idx_bytes });
			uploads.push_back({ 7u, meta_bytes });
		}

		for (int i = 0; i < uploads.size(); ++i) {
			res_vec.push_back({
				GDEXTENSION_PRODUCTION_STORAGE_V1,
				uploads[i].role,
				0,
				GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1,
				gen,
				uint64_t(uploads[i].payload.size()),
				0
			});
		}
		// Guard status readback alias resource (original_slot matches primary current role: 1, 8, or 3).
		const uint32_t guard_alias_res_slot = uint32_t(res_vec.size());
		res_vec.push_back({
			GDEXTENSION_PRODUCTION_STORAGE_V1,
			uploads[0].role,
			0,
			GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1,
			gen,
			32,
			0
		});

		Vector<GDExtensionProductionUseV1> use_vec;
		use_vec.push_back({ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 });
		for (int i = 0; i < uploads.size(); ++i) {
			use_vec.push_back({ uint32_t(1 + i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, uint64_t(uploads[i].payload.size()), 0 });
		}
		const uint32_t acq_use_count = uint32_t(use_vec.size());

		Vector<GDExtensionProductionActionV2> act_vec;
		act_vec.push_back({ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, acq_use_count, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });

		Vector<GDExtensionProductionUploadV2> upload_dtos;
		uint64_t total_payload_bytes = 0;
		PacketRecordPlan pkt;
		pkt.acquire_ordinal = 0;

		for (int i = 0; i < uploads.size(); ++i) {
			const uint32_t u_idx = uint32_t(use_vec.size());
			const uint32_t a_ord = uint32_t(act_vec.size());
			use_vec.push_back({ uint32_t(1 + i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, uint64_t(uploads[i].payload.size()), 0 });
			act_vec.push_back({ a_ord, 10, uint32_t(2 + i), 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, u_idx, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX });
			upload_dtos.push_back({ production_dto_abi<GDExtensionProductionUploadV2>(), a_ord, 10, uint32_t(2 + i), 0, uint32_t(1 + i), 0, 0, uint64_t(uploads[i].payload.size()) });
			pkt.upload_ordinals.push_back(a_ord);
			total_payload_bytes += uint64_t(uploads[i].payload.size());
		}

		pkt.finalize_ordinal = uint64_t(act_vec.size());
		act_vec.push_back({ uint32_t(pkt.finalize_ordinal), 10, 20, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });
		pkt.seal_ordinal = uint64_t(act_vec.size());
		act_vec.push_back({ uint32_t(pkt.seal_ordinal), 10, 21, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });

		const uint32_t rb_use_idx = uint32_t(use_vec.size());
		use_vec.push_back({ guard_alias_res_slot, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 });
		const uint32_t rb_ord = uint32_t(act_vec.size());
		act_vec.push_back({ rb_ord, 10, 22, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, rb_use_idx, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 });
		pkt.readback_ordinals.push_back(rb_ord);

		const GDExtensionProductionReadbackSourceV1 rb_src = {
			guard_alias_res_slot,
			GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1,
			rb_ord,
			0,
			32,
			{ pkt.finalize_ordinal },
			{}
		};

		const uint32_t read_cap = p_do_color_draw ? 1u : 0u;
		const uint32_t read_graph_cap = p_do_color_draw ? 112u : 0u;
		const GDExtensionProductionOutputDemandInputV1 demand_in = {
			production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
			sess.create_req,
			sess.created.particles,
			gen,
			0, 10,
			pkt.finalize_ordinal, 10,
			pkt.seal_ordinal, 10,
			1, 20, 21,
			read_cap, read_graph_cap,
			2, 0
		};
		GDExtensionProductionOutputGuardDemandV1 guard_demand = {};
		GDExtensionProductionMetadataQuoteV1 req_quote = {};
		err = Error(ports.req_quote_v2(
				enrollment.attachment,
				act_vec.ptr(), uint32_t(act_vec.size()),
				res_vec.ptr(), uint32_t(res_vec.size()),
				use_vec.ptr(), uint32_t(use_vec.size()),
				upload_dtos.ptr(), uint32_t(upload_dtos.size()),
				&rb_src, 1,
				5, total_payload_bytes,
				&demand_in, 1, &guard_demand, 1, &req_quote).native_error);
		record_check(p_label + ".req_quote_v2", err == OK);
		if (err != OK) {
			diagnostics[p_label + ".quote_err"] = int(err);
			teardown_owner_session(sess);
			return false;
		}

		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 25 };
		err = Error(ports.reserve_readbacks(
				enrollment.attachment, &op, &req_quote.capacity,
				act_vec.ptr(), uint32_t(act_vec.size()),
				res_vec.ptr(), uint32_t(res_vec.size()),
				use_vec.ptr(), uint32_t(use_vec.size()),
				&rb_src, 1, &pkt.req).native_error);
		record_check(p_label + ".reserve_readbacks", err == OK);
		if (err != OK) {
			teardown_owner_session(sess);
			return false;
		}

		err = Error(ports.import_storage(pkt.req, 0, &p_source).native_error);
		for (int i = 0; i < uploads.size() && err == OK; ++i) {
			err = Error(ports.prepare_upload_v2(pkt.req, &upload_dtos[i], uploads[i].payload.ptr(), uploads[i].payload.size()).native_error);
		}
		if (err == OK) {
			err = Error(ports.prepare_readback_v2(pkt.req, rb_ord, 0).native_error);
		}
		record_check(p_label + ".prepare_uploads_and_readback", err == OK);

		GDExtensionProductionCameraSampleV1 cam_sample = {};
		err = prepare_host_and_capture_camera(sess, pkt.req, 0, 0x41, cam_sample);
		record_check(p_label + ".camera_capture", err == OK && cam_sample.present_seq == 1);
		if (err != OK) {
			release_request_cleanly(pkt.req);
			teardown_owner_session(sess);
			return false;
		}

		pkt.acquire_input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		pkt.acquire_input.particles = sess.created.particles;
		pkt.acquire_input.view_epoch = cam_sample.camera.view_epoch;
		pkt.acquire_input.pass_mask = (p_profile == 4) ? 0u : uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
		pkt.acquire_input.layout_digest.bytes[0] = 3;
		pkt.acquire_input.mapping_digest.bytes[0] = 4;
		pkt.acquire_input.common.generation = gen;
		pkt.acquire_input.common.present_seq = cam_sample.present_seq;
		pkt.acquire_input.common.output_tag.bytes[0] = 0x51;
		pkt.acquire_input.common.view_tag.bytes[0] = 0x61;
		memset(pkt.acquire_input.common.current_host_token.bytes, 0x41, 32);
		for (uint32_t a = 0; a < 3; ++a) {
			pkt.acquire_input.common.bounds.position[a] = -2.0;
			pkt.acquire_input.common.bounds.size[a] = 4.0;
		}
		pkt.acquire_input.common.history_reset = 1;
		pkt.acquire_input.common.current_camera = cam_sample.camera;

		GDExtensionProductionOutputPrepareV3 prep_v3 = {};
		prep_v3.abi = { 3, sizeof(prep_v3), 0 };
		prep_v3.action_ordinal = 0;
		prep_v3.canonical_suboperation = 10;
		prep_v3.source_recipe_substep = 1;
		prep_v3.source_resource_slot = 0;
		prep_v3.source_use_index = 0;
		prep_v3.first_source_use = 0;
		prep_v3.source_use_count = 1;
		prep_v3.read_capacity = read_cap;
		prep_v3.read_graph_capacity = read_graph_cap;
		prep_v3.source_generation = 1;
		prep_v3.input = pkt.acquire_input;
		for (uint32_t r = 0; r < 8; ++r) {
			prep_v3.roles[r] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		}
		for (int i = 0; i < uploads.size(); ++i) {
			prep_v3.roles[i] = { uploads[i].role, uint32_t(1 + i), uint32_t(1 + i), 0, gen, uint64_t(uploads[i].payload.size()) };
		}

		GDExtensionProductionOutputPlanV1 plan_v1 = {};
		plan_v1.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
		plan_v1.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		plan_v1.acquire.action_ordinal = 0;
		plan_v1.acquire.canonical_suboperation = 10;
		plan_v1.acquire.source_recipe_substep = 1;
		plan_v1.acquire.source_resource_slot = 0;
		plan_v1.acquire.source_use_index = 0;
		plan_v1.acquire.first_source_use = 0;
		plan_v1.acquire.source_use_count = 1;
		plan_v1.acquire.read_capacity = read_cap;
		plan_v1.acquire.read_graph_capacity = read_graph_cap;
		plan_v1.acquire.source_generation = 1;
		plan_v1.acquire.input = pkt.acquire_input;
		plan_v1.finalize_action_ordinal = pkt.finalize_ordinal;
		plan_v1.seal_action_ordinal = pkt.seal_ordinal;
		plan_v1.finalize_canonical_suboperation = 10;
		plan_v1.seal_canonical_suboperation = 10;
		plan_v1.finalize_source_recipe_substep = 20;
		plan_v1.seal_source_recipe_substep = 21;
		plan_v1.source_space = 2;
		for (uint32_t s = 0; s < 5; ++s) {
			plan_v1.reads[s] = { 0, 0, 1, s == 0 ? 0u : 1u };
		}
		plan_v1.guard_work_limit = guard_demand.guard_work;
		plan_v1.guard_dispatch_limit = guard_demand.guard_dispatches;
		plan_v1.guard_graph_limit = guard_demand.guard_graphs;
		plan_v1.guard_native_limit = guard_demand.guard_native_memberships;
		plan_v1.guard_owned_resource_limit = guard_demand.guard_owned_resources;

		Error order_err_1 = OK;
		Error order_err_2 = OK;
		if (p_plan_before_v3) {
			order_err_1 = Error(ports.prepare_plan_v1(pkt.req, &plan_v1).native_error);
			order_err_2 = Error(ports.prepare_output_v3(pkt.req, &prep_v3).native_error);
		} else {
			order_err_1 = Error(ports.prepare_output_v3(pkt.req, &prep_v3).native_error);
			order_err_2 = Error(ports.prepare_plan_v1(pkt.req, &plan_v1).native_error);
		}
		record_check(p_label + ".prepare_v3_and_plan_v1", order_err_1 == OK && order_err_2 == OK);
		if (order_err_1 != OK || order_err_2 != OK) {
			diagnostics[p_label + ".prep_err_1"] = int(order_err_1);
			diagnostics[p_label + ".prep_err_2"] = int(order_err_2);
			release_request_cleanly(pkt.req);
			teardown_owner_session(sess);
			return false;
		}

		Indexed144ExternalSurfaceHold idx_hold;
		if (p_do_color_draw && p_profile == 2 && p_capacity > 0) {
			(void)prepare_indexed144_surface_hold(p_capacity, p_material, idx_hold);
		}
		Error draw_err = OK;
		GDExtensionProductionRecordReportV2 rec = {};
		err = execute_packet_and_color_draw_in_cull_scope(
				sess, pkt, cam_sample, p_do_color_draw, p_profile, 0u, p_material, &idx_hold, draw_err, rec);

		const bool record_ok = err == OK &&
				pkt.acquire_out.status.error == OK &&
				pkt.acquire_out.writable_count == uint32_t(uploads.size()) &&
				pkt.upload_error == OK &&
				pkt.finalize_out.status.error == OK &&
				pkt.seal_out.status.error == OK &&
				pkt.readback_append_error == OK;
		record_check(p_label + ".acquire_upload_finalize_seal", record_ok);
		if (!record_ok) {
			Dictionary d;
			d["run_err"] = int(err);
			d["acq_err"] = pkt.acquire_out.status.error;
			d["acq_detail"] = pkt.acquire_out.status.detail ? String::utf8(pkt.acquire_out.status.detail, pkt.acquire_out.status.detail_length) : String();
			d["acq_writables"] = pkt.acquire_out.writable_count;
			d["up_err"] = int(pkt.upload_error);
			d["fin_err"] = pkt.finalize_out.status.error;
			d["fin_detail"] = pkt.finalize_out.status.detail ? String::utf8(pkt.finalize_out.status.detail, pkt.finalize_out.status.detail_length) : String();
			d["seal_err"] = pkt.seal_out.status.error;
			d["rb_err"] = int(pkt.readback_append_error);
			diagnostics[p_label + ".record_fail"] = d;
			if (p_do_color_draw && p_profile == 2) {
				server->call_on_render_thread(callable_mp_static((+[](uint64_t p_h_addr, uint64_t p_part) {
					reinterpret_cast<Indexed144ExternalSurfaceHold *>(uintptr_t(p_h_addr))->release_untransferred_slot_hold(RID::from_uint64(p_part));
				})).bind(uint64_t(uintptr_t(&idx_hold)), sess.created.particles));
				server->sync();
			}
			cancel_slot_and_release_request(p_label + ".abort", pkt.req, sess.created.particles, pkt.acquire_out.lease_token);
			if (p_do_color_draw && p_profile == 2) {
				defer_idx144_cleanup(idx_hold);
			}
			teardown_owner_session(sess);
			return false;
		}

		// Verify no PREVIOUS_RECORDS buffer was allocated on the slot when motion == DISABLED!
		if (p_profile == 1 && p_motion == 0) {
			bool prev_omitted = false;
			server->call_on_render_thread(callable_mp_static((+[](uint64_t p_particles, uint64_t p_out_addr) {
				auto *r_ok = reinterpret_cast<bool *>(uintptr_t(p_out_addr));
				auto *ps = RendererRD::ParticlesStorage::get_singleton();
				uint32_t cap = 0;
				bool omitted = false;
				if (ps && RendererRD::ExternalLinkedOwnerTest::wave_g_owner_capacity_and_prev_omitted(*ps, RID::from_uint64(p_particles), cap, omitted) == OK) {
					*r_ok = omitted;
				}
			})).bind(sess.created.particles, uint64_t(uintptr_t(&prev_omitted))));
			server->sync();
			record_check(p_label + ".previous_records_omitted", prev_omitted);
		}

		if (p_do_color_draw) {
			record_check(p_label + ".native_color_draw", draw_err == OK);
		}

		// Submit RD frame to Metal, pump paid OBSERVE, and read back GPU GUARD_STATUS (32 bytes = 8 uint32 words)!
		rt_advance_rd_frame();
		uint32_t guard_words[8] = { 0xffffffffu, 0, 0, 0, 0, 0, 0, 0 };
		bool rb_complete = false;
		for (uint32_t turn = 0; turn < 256; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_observe_once(pkt.req, obs) != OK) {
				break;
			}
			if (obs.report.ready_readbacks >= 1 &&
					obs.report.known_obligations > 0 &&
					obs.report.complete_obligations == obs.report.known_obligations &&
					obs.report.possible_obligations == 0) {
				uint64_t copied = 0;
				if (ports.copy_v1(pkt.req, obs.report.revision, 0, guard_words, sizeof(guard_words), &copied).native_error == OK && copied == 32) {
					rb_complete = true;
					break;
				}
			}
			rt_advance_rd_frame();
			OS::get_singleton()->delay_usec(300);
		}
		record_check(p_label + ".guard_readback_complete", rb_complete);
		if (rb_complete) {
			bool status_valid = false;
			if (p_profile == 4) {
				// PARTICLE_LIGHT_64 status layout: [0, 0, 0, 1, capacity, 4, present_seq_low, present_seq_high]
				status_valid = (guard_words[0] == 0u && guard_words[3] == 1u && guard_words[4] == p_capacity && guard_words[5] == 4u && guard_words[6] == 1u);
			} else if (p_profile == 2) {
				// INDEXED_144 status layout: [fault_bits=0, valid_segments=cap, 0, 0, seq_low=1, seq_high=0, profile=2, 0]
				status_valid = (guard_words[0] == 0u && guard_words[1] == p_capacity && guard_words[4] == 1u && guard_words[6] == 2u);
			} else {
				// INSTANCE_192 status layout: [fault_bits=0, valid_records=cap, 0, 0, seq_low=1, ...]
				status_valid = (guard_words[0] == 0u && guard_words[1] == p_capacity && guard_words[4] == 1u);
			}
			record_check(p_label + ".guard_status_zero_faults", status_valid);
			if (!status_valid) {
				Array w;
				for (uint32_t i = 0; i < 8; ++i) {
					w.push_back(guard_words[i]);
				}
				diagnostics[p_label + ".guard_words"] = w;
			}
		}

		if (p_do_color_draw && p_profile == 2) {
			server->call_on_render_thread(callable_mp_static((+[](uint64_t p_h_addr, uint64_t p_part) {
				reinterpret_cast<Indexed144ExternalSurfaceHold *>(uintptr_t(p_h_addr))->release_untransferred_slot_hold(RID::from_uint64(p_part));
			})).bind(uint64_t(uintptr_t(&idx_hold)), sess.created.particles));
			server->sync();
		}
		const Error rel_req_err = cancel_slot_and_release_request(p_label, pkt.req, sess.created.particles, pkt.acquire_out.lease_token);
		if (p_do_color_draw && p_profile == 2) {
			defer_idx144_cleanup(idx_hold);
		}
		const Error tear_err = teardown_owner_session(sess);
		if (tear_err != OK) {
			diagnostics[p_label + ".tear_err"] = int(tear_err);
		}
		record_check(p_label + ".clean_retirement", rel_req_err == OK && tear_err == OK);
		return record_ok && rb_complete && rel_req_err == OK && tear_err == OK;
	}

	// Two-frame non-reset INDEXED_144 motion history end-to-end on Live Metal (NATIVE-002 + NATIVE-S01)!
	bool run_indexed144_two_frame_motion_history(
			const String &p_label,
			RID p_material,
			const GDExtensionStorageBackingV1 &p_source) {
		using namespace GDExtensionDeviceExit;
		constexpr uint32_t kSegments = 2u;
		constexpr uint32_t kVertBytes = kSegments * 4u * 144u; // 1152B
		constexpr uint32_t kValidBytes = kSegments * 4u * 4u; // 32B
		constexpr uint32_t kIdxBytes = kSegments * 6u * 4u; // 48B
		constexpr uint32_t kMetaBytes = 32u;

		OutputOwnerSession sess;
		Error err = setup_owner_session(2u, kSegments, 1u, 2u, RID(), p_material, sess);
		record_check(p_label + ".create_v3", err == OK && sess.created.particles != 0);
		if (err != OK) {
			return false;
		}
		const uint64_t gen = sess.created.generation;

		// Prepare Frame 1 payloads (2 Float144 ribbon quad segments = 1152B).
		Vector<uint8_t> f1_curr;
		f1_curr.resize(kVertBytes);
		for (uint32_t s = 0; s < kSegments; ++s) {
			wave_g_fill_indexed144_segment(reinterpret_cast<uint32_t *>(f1_curr.ptrw() + s * 576u), s, float(s) * 0.4f - 0.25f, -0.1f, 0.0f, 0xA05F0000u);
		}
		Vector<uint8_t> f1_prev;
		f1_prev.resize(kVertBytes);
		memset(f1_prev.ptrw(), 0, kVertBytes);
		Vector<uint8_t> f1_valid;
		f1_valid.resize(kValidBytes);
		memset(f1_valid.ptrw(), 0, kValidBytes);
		Vector<uint8_t> idx_bytes;
		idx_bytes.resize(kIdxBytes);
		{
			uint32_t *iw = reinterpret_cast<uint32_t *>(idx_bytes.ptrw());
			for (uint32_t s = 0; s < kSegments; ++s) {
				iw[s * 6 + 0] = s * 4 + 0;
				iw[s * 6 + 1] = s * 4 + 1;
				iw[s * 6 + 2] = s * 4 + 2;
				iw[s * 6 + 3] = s * 4 + 2;
				iw[s * 6 + 4] = s * 4 + 1;
				iw[s * 6 + 5] = s * 4 + 3;
			}
		}
		const uint32_t f1_meta_words[8] = { kSegments, 0, 0, 0, kSegments, 2u, 1u, 0u };
		Vector<uint8_t> f1_meta;
		f1_meta.resize(kMetaBytes);
		memcpy(f1_meta.ptrw(), f1_meta_words, kMetaBytes);

		// =========================================================================
		// FRAME 1: history_reset = 1, PrepareV3 -> PlanV1 + OutputConsumedPrepareV2
		// =========================================================================
		const GDExtensionProductionResourceV1 f1_res[7] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kVertBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 4, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kVertBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 5, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kValidBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 6, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kIdxBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kMetaBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 },
		};
		const GDExtensionProductionUseV1 f1_uses[12] = {
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kValidBytes, 0 },
			{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kIdxBytes, 0 },
			{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
			{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kValidBytes, 0 },
			{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kIdxBytes, 0 },
			{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
			{ 6, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
		};
		const GDExtensionProductionActionV2 f1_acts[9] = {
			{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 6, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 6, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 2, 10, 3, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 7, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 8, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 4, 10, 5, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 9, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 5, 10, 6, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 10, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 6, 10, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 7, 10, 8, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 8, 10, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 11, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
		};
		const GDExtensionProductionUploadV2 f1_ups[5] = {
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 1, 10, 2, 0, 1, 0, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 2, 10, 3, 0, 2, 0, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 3, 10, 4, 0, 3, 0, 0, kValidBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 4, 10, 5, 0, 4, 0, 0, kIdxBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 5, 10, 6, 0, 5, 0, 0, kMetaBytes },
		};
		const GDExtensionProductionReadbackSourceV1 f1_rb = {
			6, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 8, 0, 32, { 6 }, {}
		};
		const uint64_t f1_payload_total = uint64_t(kVertBytes) * 2 + kValidBytes + kIdxBytes + kMetaBytes;
		const GDExtensionProductionOutputDemandInputV1 f1_demand_in = {
			production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
			sess.create_req, sess.created.particles, gen,
			0, 10, 6, 10, 7, 10, 1, 7, 8,
			1, 112, 2, 0
		};
		GDExtensionProductionOutputGuardDemandV1 f1_demand = {};
		GDExtensionProductionMetadataQuoteV1 f1_quote = {};
		err = Error(ports.req_quote_v2(
				enrollment.attachment, f1_acts, 9, f1_res, 7, f1_uses, 12,
				f1_ups, 5, &f1_rb, 1, 5, f1_payload_total,
				&f1_demand_in, 1, &f1_demand, 1, &f1_quote).native_error);
		record_check(p_label + ".f1_quote", err == OK);
		if (err != OK) {
			teardown_owner_session(sess);
			return false;
		}

		PacketRecordPlan f1_pkt;
		f1_pkt.acquire_ordinal = 0;
		for (uint64_t u = 1; u <= 5; ++u) {
			f1_pkt.upload_ordinals.push_back(u);
		}
		f1_pkt.finalize_ordinal = 6;
		f1_pkt.seal_ordinal = 7;
		f1_pkt.readback_ordinals.push_back(8);

		const GDExtensionProductionOperationV1 f1_op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 15 };
		err = Error(ports.reserve_readbacks(enrollment.attachment, &f1_op, &f1_quote.capacity, f1_acts, 9, f1_res, 7, f1_uses, 12, &f1_rb, 1, &f1_pkt.req).native_error);
		record_check(p_label + ".f1_reserve", err == OK);
		if (err != OK) {
			teardown_owner_session(sess);
			return false;
		}

		const bool f1_prep_uploads_ok =
				ports.import_storage(f1_pkt.req, 0, &p_source).native_error == OK &&
				ports.prepare_upload_v2(f1_pkt.req, &f1_ups[0], f1_curr.ptr(), kVertBytes).native_error == OK &&
				ports.prepare_upload_v2(f1_pkt.req, &f1_ups[1], f1_prev.ptr(), kVertBytes).native_error == OK &&
				ports.prepare_upload_v2(f1_pkt.req, &f1_ups[2], f1_valid.ptr(), kValidBytes).native_error == OK &&
				ports.prepare_upload_v2(f1_pkt.req, &f1_ups[3], idx_bytes.ptr(), kIdxBytes).native_error == OK &&
				ports.prepare_upload_v2(f1_pkt.req, &f1_ups[4], f1_meta.ptr(), kMetaBytes).native_error == OK &&
				ports.prepare_readback_v2(f1_pkt.req, 8, 0).native_error == OK;
		record_check(p_label + ".f1_prepare_uploads_and_readbacks", f1_prep_uploads_ok);

		GDExtensionProductionCameraSampleV1 cam_sample_1 = {};
		err = prepare_host_and_capture_camera(sess, f1_pkt.req, 0, 0x71, cam_sample_1);
		record_check(p_label + ".f1_camera_capture", err == OK && cam_sample_1.present_seq == 1);
		if (err != OK) {
			release_request_cleanly(f1_pkt.req);
			teardown_owner_session(sess);
			return false;
		}

		f1_pkt.acquire_input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		f1_pkt.acquire_input.particles = sess.created.particles;
		f1_pkt.acquire_input.view_epoch = cam_sample_1.camera.view_epoch;
		f1_pkt.acquire_input.pass_mask = uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
		f1_pkt.acquire_input.layout_digest.bytes[0] = 3;
		f1_pkt.acquire_input.mapping_digest.bytes[0] = 4;
		f1_pkt.acquire_input.common.generation = gen;
		f1_pkt.acquire_input.common.present_seq = cam_sample_1.present_seq;
		f1_pkt.acquire_input.common.output_tag.bytes[0] = 0x81;
		f1_pkt.acquire_input.common.view_tag.bytes[0] = 0x91;
		memset(f1_pkt.acquire_input.common.current_host_token.bytes, 0x71, 32);
		for (uint32_t a = 0; a < 3; ++a) {
			f1_pkt.acquire_input.common.bounds.position[a] = -2.0;
			f1_pkt.acquire_input.common.bounds.size[a] = 4.0;
		}
		f1_pkt.acquire_input.common.history_reset = 1;
		f1_pkt.acquire_input.common.current_camera = cam_sample_1.camera;

		GDExtensionProductionOutputPrepareV3 f1_prep_v3 = {};
		f1_prep_v3.abi = { 3, sizeof(f1_prep_v3), 0 };
		f1_prep_v3.action_ordinal = 0;
		f1_prep_v3.canonical_suboperation = 10;
		f1_prep_v3.source_recipe_substep = 1;
		f1_prep_v3.source_resource_slot = 0;
		f1_prep_v3.source_use_index = 0;
		f1_prep_v3.first_source_use = 0;
		f1_prep_v3.source_use_count = 1;
		f1_prep_v3.read_capacity = 1;
		f1_prep_v3.read_graph_capacity = 112;
		f1_prep_v3.source_generation = 1;
		f1_prep_v3.input = f1_pkt.acquire_input;
		for (uint32_t r = 0; r < 8; ++r) {
			f1_prep_v3.roles[r] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		}
		f1_prep_v3.roles[0] = { 3, 1, 1, 0, gen, kVertBytes };
		f1_prep_v3.roles[1] = { 4, 2, 2, 0, gen, kVertBytes };
		f1_prep_v3.roles[2] = { 5, 3, 3, 0, gen, kValidBytes };
		f1_prep_v3.roles[3] = { 6, 4, 4, 0, gen, kIdxBytes };
		f1_prep_v3.roles[4] = { 7, 5, 5, 0, gen, kMetaBytes };

		GDExtensionProductionOutputPlanV1 f1_plan_v1 = {};
		f1_plan_v1.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
		f1_plan_v1.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
		f1_plan_v1.acquire.action_ordinal = 0;
		f1_plan_v1.acquire.canonical_suboperation = 10;
		f1_plan_v1.acquire.source_recipe_substep = 1;
		f1_plan_v1.acquire.source_resource_slot = 0;
		f1_plan_v1.acquire.source_use_index = 0;
		f1_plan_v1.acquire.first_source_use = 0;
		f1_plan_v1.acquire.source_use_count = 1;
		f1_plan_v1.acquire.read_capacity = 1;
		f1_plan_v1.acquire.read_graph_capacity = 112;
		f1_plan_v1.acquire.source_generation = 1;
		f1_plan_v1.acquire.input = f1_pkt.acquire_input;
		f1_plan_v1.finalize_action_ordinal = 6;
		f1_plan_v1.seal_action_ordinal = 7;
		f1_plan_v1.finalize_canonical_suboperation = 10;
		f1_plan_v1.seal_canonical_suboperation = 10;
		f1_plan_v1.finalize_source_recipe_substep = 7;
		f1_plan_v1.seal_source_recipe_substep = 8;
		f1_plan_v1.source_space = 2;
		for (uint32_t s = 0; s < 5; ++s) {
			f1_plan_v1.reads[s] = { 0, 0, 1, s == 0 ? 0u : 1u };
		}
		f1_plan_v1.guard_work_limit = f1_demand.guard_work;
		f1_plan_v1.guard_dispatch_limit = f1_demand.guard_dispatches;
		f1_plan_v1.guard_graph_limit = f1_demand.guard_graphs;
		f1_plan_v1.guard_native_limit = f1_demand.guard_native_memberships;
		f1_plan_v1.guard_owned_resource_limit = f1_demand.guard_owned_resources;

		const GDExtensionOutputConsumedSelectorV2 f1_sel = { production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 0 };
		const Error f1_p1 = Error(ports.prepare_output_v3(f1_pkt.req, &f1_prep_v3).native_error);
		const Error f1_p2 = Error(ports.prepare_plan_v1(f1_pkt.req, &f1_plan_v1).native_error);
		const Error f1_cp = Error(ports.consumed_prepare_v2(f1_pkt.req, &f1_sel).native_error);
		record_check(p_label + ".f1_prepare_v3_plan_consumed", f1_p1 == OK && f1_p2 == OK && f1_cp == OK);
		if (f1_p1 != OK || f1_p2 != OK || f1_cp != OK) {
			release_request_cleanly(f1_pkt.req);
			teardown_owner_session(sess);
			return false;
		}

		Indexed144ExternalSurfaceHold f1_idx_hold;
		(void)prepare_indexed144_surface_hold(kSegments, p_material, f1_idx_hold);
		Error f1_draw_err = ERR_UNCONFIGURED;
		GDExtensionProductionRecordReportV2 f1_rec = {};
		err = execute_packet_and_color_draw_in_cull_scope(
				sess, f1_pkt, cam_sample_1, true, 2u, 0u, p_material, &f1_idx_hold, f1_draw_err, f1_rec);
		const bool f1_rec_ok = err == OK &&
				f1_pkt.acquire_out.status.error == OK &&
				f1_pkt.acquire_out.writable_count == 5 &&
				f1_pkt.finalize_out.status.error == OK &&
				f1_pkt.seal_out.status.error == OK;
		record_check(p_label + ".f1_acquire_finalize_seal", f1_rec_ok);
		auto abort_f1_session = [&]() {
			server->call_on_render_thread(callable_mp_static((+[](uint64_t p_h_addr, uint64_t p_part) {
				reinterpret_cast<Indexed144ExternalSurfaceHold *>(uintptr_t(p_h_addr))->release_untransferred_slot_hold(RID::from_uint64(p_part));
			})).bind(uint64_t(uintptr_t(&f1_idx_hold)), sess.created.particles));
			server->sync();
			cancel_slot_and_release_request(p_label + ".abort_f1", f1_pkt.req, sess.created.particles, f1_pkt.acquire_out.lease_token);
			defer_idx144_cleanup(f1_idx_hold);
			teardown_owner_session(sess);
		};

		if (!f1_rec_ok) {
			diagnostics[p_label + ".f1_fin_err"] = f1_pkt.finalize_out.status.error;
			abort_f1_session();
			return false;
		}

		record_check(p_label + ".f1_native_color_draw", f1_draw_err == OK);
		if (f1_draw_err != OK) {
			abort_f1_session();
			return false;
		}

		// Submit RD frame to Metal and pump paid OBSERVE on Frame 1 until GUARD_STATUS readback AND OutputConsumedBorrowV2 + OutputConsumedReadV2 are complete!
		rt_advance_rd_frame();
		uint32_t f1_guard_words[8] = { 0xffffffffu };
		GDExtensionOutputConsumedProofV2 f1_consumed = {};
		bool f1_rb_ok = false;
		for (uint32_t turn = 0; turn < 256; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_observe_once(f1_pkt.req, obs) != OK) {
				break;
			}
			uint64_t copied = 0;
			if (obs.report.ready_readbacks >= 1 &&
					obs.report.known_obligations > 0 &&
					obs.report.complete_obligations == obs.report.known_obligations &&
					obs.report.possible_obligations == 0 &&
					ports.copy_v1(f1_pkt.req, obs.report.revision, 0, f1_guard_words, 32, &copied).native_error == OK &&
					copied == 32) {
				f1_rb_ok = true;
				break;
			}
			rt_advance_rd_frame();
			OS::get_singleton()->delay_usec(300);
		}
		bool f1_done = false;
		if (f1_rb_ok) {
			for (uint32_t turn = 0; turn < 256; ++turn) {
				server->sync();
				const GDExtensionProductionServiceBudgetV1 b_budget = make_budget();
				GDExtensionOutputConsumedBorrowResultV2 borrow_res = {};
				const Error cb_err = Error(ports.consumed_borrow_v2(f1_pkt.req, &f1_sel, &b_budget, &borrow_res).native_error);
				if (cb_err == ERR_BUSY || (cb_err == OK && borrow_res.result.native_error == ERR_BUSY)) {
					rt_advance_rd_frame();
					OS::get_singleton()->delay_usec(300);
					continue;
				}
				if (cb_err == OK && borrow_res.result.native_error == OK) {
					const Error cr_err = Error(ports.consumed_read_v2(f1_pkt.req, &borrow_res.proof.ref, &f1_consumed).native_error);
					record_check(p_label + ".f1_consumed_borrow_and_read_v2", cr_err == OK && f1_consumed.ref.view_cell_serial != 0);
					f1_done = (cr_err == OK);
				} else {
					diagnostics[p_label + ".f1_borrow_err"] = int(cb_err != OK ? cb_err : Error(borrow_res.result.native_error));
				}
				break;
			}
		}
		record_check(p_label + ".f1_gpu_and_consumed_proof_ready", f1_done && f1_guard_words[0] == 0u && f1_guard_words[1] == kSegments);
		if (!f1_done) {
			abort_f1_session();
			return false;
		}

		// Adopt Frame 1 staged camera on viewport so Frame 2 advances present_seq = 2 and captures actual_previous_camera!
		server->call_on_render_thread(callable_mp_static((+[](RID p_vp) {
			RSG::viewport->viewport_adopt_camera_data(p_vp);
		})).bind(sess.viewport));
		server->sync();

		// =========================================================================
		// FRAME 2: history_reset = 0, has_previous_consumed_tuple = 1,
		//          PlanV2 -> PrepareV3 (reverse call order!),
		//          ImportOutputPredecessorV1 (1152B),
		//          POISONED ALIGNED_PREVIOUS_VERTICES overwritten by GPU history copy,
		//          GPU GUARD_STATUS [fault_bits == 0, valid_segments == 2] +
		//          1152B ALIGNED_PREVIOUS_VERTICES byte-exact match with Frame 1 CURRENT_VERTICES!
		// =========================================================================
		Vector<uint8_t> f2_curr;
		f2_curr.resize(kVertBytes);
		for (uint32_t s = 0; s < kSegments; ++s) {
			// Shift ribbon segments by (+0.12, +0.08, 0.0) while preserving exact RibbonSegmentEndpointKey32 (0xA05F0000u)!
			wave_g_fill_indexed144_segment(reinterpret_cast<uint32_t *>(f2_curr.ptrw() + s * 576u), s, float(s) * 0.4f - 0.13f, -0.02f, 0.0f, 0xA05F0000u);
		}
		// Upload POISONED previous vertices (corrupt endpoint keys 0xBAD0BAD0u that would trigger HISTORY_FAULT=32u if not overwritten!)
		Vector<uint8_t> f2_poison_prev;
		f2_poison_prev.resize(kVertBytes);
		{
			uint32_t *pw = reinterpret_cast<uint32_t *>(f2_poison_prev.ptrw());
			for (uint32_t i = 0; i < kVertBytes / 4u; ++i) {
				pw[i] = 0xBAD0BAD0u;
			}
		}
		// Set all 8 vertex history validity flags to 1!
		Vector<uint8_t> f2_valid;
		f2_valid.resize(kValidBytes);
		{
			uint32_t *vw = reinterpret_cast<uint32_t *>(f2_valid.ptrw());
			for (uint32_t v = 0; v < kSegments * 4u; ++v) {
				vw[v] = 1u;
			}
		}
		const uint32_t f2_meta_words[8] = { kSegments, 0, 0, 0, kSegments, 2u, 2u, 0u };
		Vector<uint8_t> f2_meta;
		f2_meta.resize(kMetaBytes);
		memcpy(f2_meta.ptrw(), f2_meta_words, kMetaBytes);

		GDExtensionNativeUnitBackingV1 f2_align_unit = {};
		const Error f2_unit_err = create_qualified_history_align_unit(77, f2_align_unit);
		record_check(p_label + ".f2_create_history_align_unit", f2_unit_err == OK);

		const GDExtensionProductionResourceV1 f2_res[10] = {
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, gen, kVertBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kVertBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 4, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kVertBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 5, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kValidBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 6, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kIdxBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kMetaBytes, 0 },
			{ GDEXTENSION_PRODUCTION_STORAGE_V1, 3, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 },
			{ GDEXTENSION_PRODUCTION_UNIT_V1, 77, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 0, 0 },
			{ GDEXTENSION_PRODUCTION_SET_V1, 78, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 },
		};
		const GDExtensionProductionUseV1 f2_uses[18] = {
			{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, kVertBytes, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kValidBytes, 0 },
			{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kIdxBytes, 0 },
			{ 6, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
			{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kValidBytes, 0 },
			{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kIdxBytes, 0 },
			{ 6, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
			{ 1, GDEXTENSION_PRODUCTION_READ_V1, 0, kVertBytes, 0 },
			{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, kVertBytes, 0 },
			{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kVertBytes, 0 },
			{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kValidBytes, 0 },
			{ 7, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
			{ 3, GDEXTENSION_PRODUCTION_READ_V1, 0, kVertBytes, 0 },
		};
		const GDExtensionProductionActionV2 f2_acts[11] = {
			{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 7, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 7, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 2, 10, 3, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 8, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 9, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 4, 10, 5, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 10, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 5, 10, 6, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 11, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 6, 10, 7, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 8, 12, 4, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			{ 7, 10, 8, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 8, 10, 9, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
			{ 9, 10, 10, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 16, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
			{ 10, 10, 11, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 17, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 1 },
		};
		const GDExtensionProductionUploadV2 f2_ups[5] = {
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 1, 10, 2, 0, 2, 0, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 2, 10, 3, 0, 3, 0, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 3, 10, 4, 0, 4, 0, 0, kValidBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 4, 10, 5, 0, 5, 0, 0, kIdxBytes },
			{ production_dto_abi<GDExtensionProductionUploadV2>(), 5, 10, 6, 0, 6, 0, 0, kMetaBytes },
		};
		const GDExtensionProductionReadbackSourceV1 f2_rbs[2] = {
			{ 7, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 9, 0, 32, { 7 }, {} },
			{ 3, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, 10, 0, kVertBytes, { 0 }, {} },
		};
		const GDExtensionProductionOutputDemandInputV1 f2_demand_in = {
			production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
			sess.create_req, sess.created.particles, gen,
			0, 10, 7, 10, 8, 10, 1, 8, 9,
			0, 0, 2, 0
		};
		GDExtensionProductionOutputGuardDemandV1 f2_demand = {};
		GDExtensionProductionMetadataQuoteV1 f2_quote = {};
		err = Error(ports.req_quote_v2(
				enrollment.attachment, f2_acts, 11, f2_res, 10, f2_uses, 18,
				f2_ups, 5, f2_rbs, 2, 9, f1_payload_total,
				&f2_demand_in, 1, &f2_demand, 1, &f2_quote).native_error);
		f2_quote.capacity.return_cells = MAX(f2_quote.capacity.return_cells, 10u);
		f2_quote.capacity.resource_uses += 8u;
		f2_quote.capacity.graph_memberships += 4;
		{
			MutexLock reg_lock(enrollment.registry.mutex);
			MutexLock rd_lock(device->_thread_safe_);
			enrollment.registry.production_membership_bound_locked(*device, f2_quote.capacity.graph_memberships, device->transfer_worker_pool_max_size, f2_quote.capacity.native_memberships);
		}
		record_check(p_label + ".f2_quote", err == OK);

		PacketRecordPlan f2_pkt;
		f2_pkt.acquire_ordinal = 0;
		for (uint64_t u = 1; u <= 6; ++u) {
			f2_pkt.upload_ordinals.push_back(u);
		}
		f2_pkt.finalize_ordinal = 7;
		f2_pkt.seal_ordinal = 8;
		f2_pkt.readback_ordinals.push_back(9);
		f2_pkt.readback_ordinals.push_back(10);

		auto return_f1_borrow = [&]() -> Error {
			for (uint32_t turn = 0; turn < 256; ++turn) {
				server->sync();
				const GDExtensionProductionServiceBudgetV1 r_budget = make_budget();
				GDExtensionProductionReportV2 r_rep = {};
				const Error re = Error(ports.consumed_return_v2(f1_pkt.req, &f1_consumed.ref, &r_budget, &r_rep).native_error);
				if (re == ERR_BUSY) {
					OS::get_singleton()->delay_usec(400);
					continue;
				}
				return re;
			}
			return ERR_TIMEOUT;
		};

		const GDExtensionProductionOperationV1 f2_op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 16 };
		err = Error(ports.reserve_readbacks(enrollment.attachment, &f2_op, &f2_quote.capacity, f2_acts, 11, f2_res, 10, f2_uses, 18, f2_rbs, 2, &f2_pkt.req).native_error);
		record_check(p_label + ".f2_reserve", err == OK);
		if (err != OK) {
			(void)return_f1_borrow();
			abort_f1_session();
			if (f2_unit_err == OK) {
				(void)retire_qualified_history_align_unit(f2_align_unit);
			}
			return false;
		}

		const Error imp_src_err = Error(ports.import_storage(f2_pkt.req, 0, &p_source).native_error);
		const Error imp_unit_err = (imp_src_err == OK) ? Error(ports.import_unit(f2_pkt.req, 8, &f2_align_unit).native_error) : imp_src_err;
		record_check(p_label + ".f2_import_source", imp_src_err == OK && imp_unit_err == OK);
		if (imp_src_err != OK || imp_unit_err != OK) {
			release_request_cleanly(f2_pkt.req);
			(void)return_f1_borrow();
			abort_f1_session();
			if (f2_unit_err == OK) {
				(void)retire_qualified_history_align_unit(f2_align_unit);
			}
			return false;
		}

		const bool f2_prep_uploads_ok =
				ports.prepare_upload_v2(f2_pkt.req, &f2_ups[0], f2_curr.ptr(), kVertBytes).native_error == OK &&
				ports.prepare_upload_v2(f2_pkt.req, &f2_ups[1], f2_poison_prev.ptr(), kVertBytes).native_error == OK &&
				ports.prepare_upload_v2(f2_pkt.req, &f2_ups[2], f2_valid.ptr(), kValidBytes).native_error == OK &&
				ports.prepare_upload_v2(f2_pkt.req, &f2_ups[3], idx_bytes.ptr(), kIdxBytes).native_error == OK &&
				ports.prepare_upload_v2(f2_pkt.req, &f2_ups[4], f2_meta.ptr(), kMetaBytes).native_error == OK &&
				ports.prepare_readback_v2(f2_pkt.req, 9, 0).native_error == OK &&
				ports.prepare_readback_v2(f2_pkt.req, 10, 1).native_error == OK;
		record_check(p_label + ".f2_prepare_uploads_and_readbacks", f2_prep_uploads_ok);

		GDExtensionProductionCameraSampleV1 cam_sample_2 = {};
		err = prepare_host_and_capture_camera(sess, f2_pkt.req, 0, 0x72, cam_sample_2);
		record_check(p_label + ".f2_camera_capture", err == OK && cam_sample_2.present_seq == 2);

		f2_pkt.acquire_input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
		f2_pkt.acquire_input.particles = sess.created.particles;
		f2_pkt.acquire_input.view_epoch = cam_sample_2.camera.view_epoch;
		f2_pkt.acquire_input.pass_mask = uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
		f2_pkt.acquire_input.layout_digest.bytes[0] = 3;
		f2_pkt.acquire_input.mapping_digest.bytes[0] = 4;
		f2_pkt.acquire_input.common.generation = gen;
		f2_pkt.acquire_input.common.present_seq = cam_sample_2.present_seq;
		f2_pkt.acquire_input.common.output_tag.bytes[0] = 0x82; // Distinct from Frame 1 (0x81)!
		f2_pkt.acquire_input.common.view_tag.bytes[0] = 0x91; // Matches Frame 1 view_tag!
		memset(f2_pkt.acquire_input.common.current_host_token.bytes, 0x72, 32);
		for (uint32_t a = 0; a < 3; ++a) {
			f2_pkt.acquire_input.common.bounds.position[a] = -2.0;
			f2_pkt.acquire_input.common.bounds.size[a] = 4.0;
		}
		f2_pkt.acquire_input.common.history_reset = 0;
		f2_pkt.acquire_input.common.current_camera = cam_sample_2.camera;
		f2_pkt.acquire_input.common.has_actual_previous_camera = 1;
		f2_pkt.acquire_input.common.actual_previous_camera = cam_sample_1.camera;
		f2_pkt.acquire_input.common.has_previous_consumed_tuple = 1;
		f2_pkt.acquire_input.common.previous_consumed_tuple_token = f1_consumed.ref.original_color_token;

		GDExtensionProductionOutputPrepareV3 f2_prep_v3 = {};
		f2_prep_v3.abi = { 3, sizeof(f2_prep_v3), 0 };
		f2_prep_v3.action_ordinal = 0;
		f2_prep_v3.canonical_suboperation = 10;
		f2_prep_v3.source_recipe_substep = 1;
		f2_prep_v3.source_resource_slot = 0;
		f2_prep_v3.source_use_index = 0;
		f2_prep_v3.first_source_use = 0;
		f2_prep_v3.source_use_count = 2; // source + predecessor!
		f2_prep_v3.read_capacity = 0;
		f2_prep_v3.read_graph_capacity = 0;
		f2_prep_v3.source_generation = 1;
		f2_prep_v3.input = f2_pkt.acquire_input;
		for (uint32_t r = 0; r < 8; ++r) {
			f2_prep_v3.roles[r] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
		}
		f2_prep_v3.roles[0] = { 3, 2, 2, 0, gen, kVertBytes };
		f2_prep_v3.roles[1] = { 4, 3, 3, 0, gen, kVertBytes };
		f2_prep_v3.roles[2] = { 5, 4, 4, 0, gen, kValidBytes };
		f2_prep_v3.roles[3] = { 6, 5, 5, 0, gen, kIdxBytes };
		f2_prep_v3.roles[4] = { 7, 6, 6, 0, gen, kMetaBytes };

		GDExtensionProductionOutputPlanV2 f2_plan_v2 = {};
		f2_plan_v2.abi = { 2, sizeof(f2_plan_v2), 0 };
		f2_plan_v2.plan = f1_plan_v1;
		f2_plan_v2.plan.finalize_action_ordinal = 7;
		f2_plan_v2.plan.seal_action_ordinal = 8;
		f2_plan_v2.plan.finalize_source_recipe_substep = 8;
		f2_plan_v2.plan.seal_source_recipe_substep = 9;
		f2_plan_v2.plan.acquire.source_use_count = 2;
		f2_plan_v2.plan.acquire.read_capacity = 0;
		f2_plan_v2.plan.acquire.read_graph_capacity = 0;
		f2_plan_v2.plan.acquire.input = f2_pkt.acquire_input;
		f2_plan_v2.plan.guard_work_limit = f2_demand.guard_work;
		f2_plan_v2.plan.guard_dispatch_limit = f2_demand.guard_dispatches;
		f2_plan_v2.plan.guard_graph_limit = f2_demand.guard_graphs + 2;
		f2_plan_v2.plan.guard_native_limit = f2_quote.capacity.native_memberships;
		f2_plan_v2.plan.guard_owned_resource_limit = f2_demand.guard_owned_resources;
		f2_plan_v2.predecessor = {
			production_dto_abi<GDExtensionProductionOutputPredecessorV2>(),
			f1_consumed.ref,
			1,
			1
		};

		// Test REVERSE call order on Frame 2: PrepareOutputPlanV2 (imports INDEXED_144 predecessor on RT!) -> PrepareOutputV3!
		Error f2_p1 = ERR_TIMEOUT;
		for (uint32_t turn = 0; turn < 256; ++turn) {
			server->sync();
			const GDExtensionProductionServiceBudgetV1 p_budget = make_budget();
			f2_p1 = Error(ports.prepare_plan_v2(f2_pkt.req, &f2_plan_v2, &p_budget).native_error);
			if (f2_p1 == ERR_BUSY) {
				OS::get_singleton()->delay_usec(400);
				continue;
			}
			break;
		}
		const Error f2_p2 = (f2_p1 == OK) ? Error(ports.prepare_output_v3(f2_pkt.req, &f2_prep_v3).native_error) : ERR_UNCONFIGURED;
		record_check(p_label + ".f2_plan_v2_then_prepare_v3", f2_p1 == OK && f2_p2 == OK);
		if (f2_p1 != OK || f2_p2 != OK) {
			diagnostics[p_label + ".f2_p1_err"] = int(f2_p1);
			diagnostics[p_label + ".f2_p2_err"] = int(f2_p2);
		}

		// Create deferred set binding imported Frame 1 predecessor (slot 1) + Frame 2 aliases (slots 2, 3, 4) and prepare GPU alignment dispatch!
		const GDExtensionProductionSetBindingV1 f2_set_bindings[4] = {
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 0, 0, 1, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 1, 0, 2, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 2, 0, 3, 0, kVertBytes },
			{ production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, 3, 0, 4, 0, kValidBytes },
		};
		const Error f2_set_err = Error(ports.set_create(f2_pkt.req, 9, 8, 0, f2_set_bindings, 4).native_error);
		const GDExtensionProductionDispatchV2 f2_dispatch = {
			production_dto_abi<GDExtensionProductionDispatchV2>(),
			6, 10, 7, 0, 8, 9, 1, kSegments, 1, 1, 0, 0
		};
		const Error f2_disp_err = (f2_set_err == OK) ? Error(ports.prepare_dispatch_v2(f2_pkt.req, &f2_dispatch, nullptr, 0).native_error) : f2_set_err;
		record_check(p_label + ".f2_prepare_history_align_dispatch", f2_set_err == OK && f2_disp_err == OK);
		if (f2_set_err != OK || f2_disp_err != OK) {
			diagnostics[p_label + ".f2_set_err"] = int(f2_set_err);
			diagnostics[p_label + ".f2_disp_err"] = int(f2_disp_err);
		}

		Error f2_unused_draw_err = OK;
		GDExtensionProductionRecordReportV2 f2_rec = {};
		err = execute_packet_and_color_draw_in_cull_scope(
				sess, f2_pkt, cam_sample_2, false, 2u, 1u, p_material, nullptr, f2_unused_draw_err, f2_rec, &cam_sample_1.camera);

		const bool f2_rec_ok = err == OK &&
				f2_pkt.acquire_out.status.error == OK &&
				f2_pkt.acquire_out.writable_count == 5 &&
				f2_pkt.upload_error == OK &&
				f2_pkt.finalize_out.status.error == OK &&
				f2_pkt.seal_out.status.error == OK &&
				f2_pkt.readback_append_error == OK;
		record_check(p_label + ".f2_acquire_finalize_seal_with_history", f2_rec_ok);
		if (!f2_rec_ok) {
			Dictionary d;
			d["run_err"] = int(err);
			d["acq_err"] = f2_pkt.acquire_out.status.error;
			d["fin_err"] = f2_pkt.finalize_out.status.error;
			d["fin_detail"] = f2_pkt.finalize_out.status.detail ? String::utf8(f2_pkt.finalize_out.status.detail, f2_pkt.finalize_out.status.detail_length) : String();
			d["seal_err"] = f2_pkt.seal_out.status.error;
			diagnostics[p_label + ".f2_record_fail"] = d;
		}

		// Pump paid OBSERVE on Frame 2 and verify BOTH readbacks:
		//   [0] GUARD_STATUS: fault_bits == 0, valid_segments == 2, present_seq == 2, profile == 2
		//   [1] ALIGNED_PREVIOUS_VERTICES (1152B): byte-exact match with Frame 1 CURRENT_VERTICES!
		uint32_t f2_guard_words[8] = { 0xffffffffu };
		Vector<uint8_t> f2_prev_readback;
		f2_prev_readback.resize(kVertBytes);
		memset(f2_prev_readback.ptrw(), 0xEE, kVertBytes);
		bool f2_rb_done = false;
		if (f2_rec_ok) {
			rt_advance_rd_frame();
			for (uint32_t turn = 0; turn < 256; ++turn) {
				GDExtensionProductionObserveResultV3 obs = {};
				if (pump_observe_once(f2_pkt.req, obs) != OK) {
					break;
				}
				if (obs.report.ready_readbacks >= 2 &&
						obs.report.known_obligations > 0 &&
						obs.report.complete_obligations == obs.report.known_obligations &&
						obs.report.possible_obligations == 0) {
					uint64_t c0 = 0, c1 = 0;
					if (ports.copy_v1(f2_pkt.req, obs.report.revision, 0, f2_guard_words, 32, &c0).native_error == OK && c0 == 32 &&
							ports.copy_v1(f2_pkt.req, obs.report.revision, 1, f2_prev_readback.ptrw(), kVertBytes, &c1).native_error == OK && c1 == kVertBytes) {
						f2_rb_done = true;
						break;
					}
				}
				rt_advance_rd_frame();
				OS::get_singleton()->delay_usec(300);
			}
		}
		record_check(p_label + ".f2_readbacks_complete", f2_rb_done);
		const bool f2_guard_ok = f2_rb_done && f2_guard_words[0] == 0u && f2_guard_words[1] == kSegments && f2_guard_words[4] == 2u && f2_guard_words[6] == 2u;
		const bool f2_history_exact = f2_rb_done && memcmp(f2_prev_readback.ptr(), f1_curr.ptr(), kVertBytes) == 0;
		record_check(p_label + ".f2_guard_zero_faults", f2_guard_ok);
		record_check(p_label + ".f2_aligned_previous_matches_f1_current_exact_1152b", f2_history_exact);
		if (f2_rb_done && !f2_guard_ok) {
			Array gw;
			for (uint32_t i = 0; i < 8; ++i) {
				gw.push_back(f2_guard_words[i]);
			}
			diagnostics[p_label + ".f2_guard_words"] = gw;
		}

		// Cancel & retire Frame 2 slot first, return Frame 1 consumed borrow, cancel & retire Frame 1 slot, then withdraw & tear down generation!
		const Error rel2 = cancel_slot_and_release_request(p_label + ".f2", f2_pkt.req, sess.created.particles, f2_pkt.acquire_out.lease_token);
		const Error ret_borrow_err = return_f1_borrow();
		record_check(p_label + ".consumed_return_v2", ret_borrow_err == OK);

		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_h_addr, uint64_t p_part) {
			reinterpret_cast<Indexed144ExternalSurfaceHold *>(uintptr_t(p_h_addr))->release_untransferred_slot_hold(RID::from_uint64(p_part));
		})).bind(uint64_t(uintptr_t(&f1_idx_hold)), sess.created.particles));
		server->sync();

		const Error rel1 = cancel_slot_and_release_request(p_label + ".f1", f1_pkt.req, sess.created.particles, f1_pkt.acquire_out.lease_token);
		defer_idx144_cleanup(f1_idx_hold);

		const Error tear = teardown_owner_session(sess);
		if (tear != OK) {
			diagnostics[p_label + ".tear_err"] = int(tear);
		}
		const Error unit_ret = (f2_unit_err == OK) ? retire_qualified_history_align_unit(f2_align_unit) : f2_unit_err;
		if (unit_ret != OK) {
			diagnostics[p_label + ".unit_ret_err"] = int(unit_ret);
		}
		record_check(p_label + ".clean_retirement", rel2 == OK && rel1 == OK && tear == OK && unit_ret == OK);
		return f2_rec_ok && f2_guard_ok && f2_history_exact && ret_borrow_err == OK && rel2 == OK && rel1 == OK && tear == OK && unit_ret == OK;
	}

	Dictionary run_all() {
		using namespace GDExtensionDeviceExit;
		Dictionary out;

		server = RenderingServer::get_singleton();
		device = server ? server->get_rendering_device() : nullptr;
		const String driver_name = OS::get_singleton()->get_current_rendering_driver_name();
		const String render_method = OS::get_singleton()->get_current_rendering_method();
		out["driver"] = driver_name;
		out["rendering_method"] = render_method;

		server->set_render_loop_enabled(false);
		record_check("live_metal_driver", driver_name == "metal" && device != nullptr && device == RenderingDevice::get_singleton() && device->is_main_instance);
		record_check("ports_loaded", ports.load());

		auto *scene_render = RendererSceneRenderRD::get_singleton();
		const uint32_t expected_backend = render_method == "mobile" ? 1u : (render_method == "forward_plus" ? 2u : 0u);
		record_check("renderer_backend_matches_method", scene_render != nullptr && expected_backend != 0 && scene_render->get_native_material_recipe_backend() == expected_backend);

		if (!failures.is_empty()) {
			out["pass"] = false;
			out["checks"] = checks;
			out["failures"] = failures;
			return out;
		}

		const auto baseline_counts = singleton_counts();

		// Prepare a clean triangle mesh + unshaded cull_disabled spatial material on RT and settle transfer workers.
		RID mesh_rid = server->mesh_create();
		{
			Array arr;
			arr.resize(RSE::ARRAY_MAX);
			PackedVector3Array vtx;
			vtx.push_back(Vector3(-0.25f, -0.25f, 0.0f));
			vtx.push_back(Vector3(0.25f, -0.25f, 0.0f));
			vtx.push_back(Vector3(0.0f, 0.25f, 0.0f));
			PackedInt32Array idx;
			idx.push_back(0);
			idx.push_back(1);
			idx.push_back(2);
			arr[RSE::ARRAY_VERTEX] = vtx;
			arr[RSE::ARRAY_INDEX] = idx;
			server->mesh_add_surface_from_arrays(mesh_rid, RSE::PRIMITIVE_TRIANGLES, arr);
		}
		RID shader_rid = server->shader_create_from_code("shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(0.9, 0.3, 0.1); }");
		RID material_rid = server->material_create();
		server->material_set_shader(material_rid, shader_rid);

		server->call_on_render_thread(callable_mp_static((+[](RID p_mesh, RID p_mat, uint64_t p_rd_addr, uint64_t p_self_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			auto *ms = RendererRD::MeshStorage::get_singleton();
			auto *mats = RendererRD::MaterialStorage::get_singleton();
			if (p_rd) {
				for (uint32_t f = 0; f < 4; ++f) {
					p_rd->_submit_transfer_workers();
					p_rd->_wait_for_transfer_workers();
					p_rd->swap_buffers(false);
				}
			}
			if (ms && p_rd) {
				const auto *const_ms = ms;
				if (auto *m = ms->mesh_owner.get_or_null(p_mesh)) {
					if (m->surface_count > 0 && m->surfaces[0]) {
						if (auto *vbo = p_rd->vertex_buffer_owner.get_or_null(m->surfaces[0]->vertex_buffer)) {
							p_rd->_check_transfer_worker_buffer(vbo);
						}
						if (auto *ibo = p_rd->index_buffer_owner.get_or_null(m->surfaces[0]->index_buffer)) {
							p_rd->_check_transfer_worker_buffer(ibo);
						}
						if (auto *iao = p_rd->index_array_owner.get_or_null(m->surfaces[0]->index_array)) {
							p_rd->_check_transfer_worker_index_array(iao);
						}
					}
				}
				(void)const_ms->mesh_get_surface(p_mesh, 0);
				(void)const_ms->mesh_get_surface(p_mesh, 0);
				if (auto *m = ms->mesh_owner.get_or_null(p_mesh)) {
					if (m->surface_count > 0 && m->surfaces[0]) {
						if (auto *vbo = p_rd->vertex_buffer_owner.get_or_null(m->surfaces[0]->vertex_buffer)) {
							p_rd->_check_transfer_worker_buffer(vbo);
						}
						if (auto *ibo = p_rd->index_buffer_owner.get_or_null(m->surfaces[0]->index_buffer)) {
							p_rd->_check_transfer_worker_buffer(ibo);
						}
						if (auto *iao = p_rd->index_array_owner.get_or_null(m->surfaces[0]->index_array)) {
							p_rd->_check_transfer_worker_index_array(iao);
						}
					}
				}
				RendererRD::MeshStorage::NativeSelectionLease sel;
				RendererRD::MeshStorage::NativeSurfaceHold hold;
				if (ms->mesh_selection_acquire(p_mesh, sel) == OK) {
					if (ms->mesh_surface_source_hold_acquire(p_mesh, 0, hold) == OK) {
						(void)ms->mesh_surface_hold_release(hold);
					}
					(void)ms->mesh_selection_release(sel);
				}
			}
			if (mats) {
				mats->_update_queued_materials();
				mats->material_prepare_native_qualification(p_mat);
			}
			if (p_rd) {
				auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
				RD::TextureFormat color_tf;
				color_tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
				color_tf.width = 32;
				color_tf.height = 32;
				color_tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
				self_ptr->shared_color_tex = p_rd->texture_create(color_tf, RD::TextureView());

				RD::TextureFormat depth_tf;
				depth_tf.format = p_rd->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D32_SFLOAT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
						? RD::DATA_FORMAT_D32_SFLOAT
						: RD::DATA_FORMAT_D24_UNORM_S8_UINT;
				depth_tf.width = 32;
				depth_tf.height = 32;
				depth_tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
				self_ptr->shared_depth_tex = p_rd->texture_create(depth_tf, RD::TextureView());

				Vector<RID> fb_tex;
				fb_tex.push_back(self_ptr->shared_color_tex);
				fb_tex.push_back(self_ptr->shared_depth_tex);
				self_ptr->shared_fb = p_rd->framebuffer_create(fb_tex);
				self_ptr->shared_fb_format = p_rd->framebuffer_get_format(self_ptr->shared_fb);
				for (uint32_t f = 0; f < 4; ++f) {
					p_rd->_submit_transfer_workers();
					p_rd->_wait_for_transfer_workers();
					p_rd->swap_buffers(false);
				}
			}
		})).bind(mesh_rid, material_rid, uint64_t(uintptr_t(device)), uint64_t(uintptr_t(this))));
		server->sync();

		// Initialize owner enrollment + large attachment pool + QualificationOpen + Normal Callable + Camera Relay.
		OrdinalPlan pool = {};
		pool.attachment_requests = 16;
		pool.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		pool.capacity.requests = 16;
		pool.capacity.suboperations = 64;
		pool.capacity.resources = 64;
		pool.capacity.resource_uses = 2048;
		pool.capacity.graph_memberships = 1024;
		pool.capacity.return_cells = 64;
		pool.capacity.retained_payload_bytes = 65536;
		pool.capacity.readbacks = 16;
		pool.capacity.readback_bytes = 16384;
		{
			MutexLock rd_lock(device->_thread_safe_);
			MutexLock reg_lock(enrollment.registry.mutex);
			const Error bound_err = enrollment.registry.production_membership_bound_locked(*device, pool.capacity.graph_memberships, device->transfer_worker_pool_max_size, pool.capacity.native_memberships);
			record_check("pool_membership_bound", bound_err == OK);
		}
		enrollment.public_port = false;
		enrollment.server = Object::cast_to<RenderingServerDefault>(server);
		const Error init_err = enrollment.initialize_owner(
				1, false, true, &LiveMetalWaveGNativeOutputHarness::on_record, this, false, 1, false,
				+[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, device, true);
		if (init_err == OK) {
			// Adopt normal callable + camera relay while SCENE_READY registration frame is active!
			{
				MutexLock reg_lock(enrollment.registry.mutex);
				enrollment.registry.records[enrollment.reference.slot].guard.entry_classes |= 1u << 8;
			}
			GDExtensionCallableCustomInfo2 cinfo = {};
			cinfo.callable_userdata = this;
			cinfo.call_func = &LiveMetalWaveGNativeOutputHarness::on_normal_callable_call;
			cinfo.free_func = &LiveMetalWaveGNativeOutputHarness::on_normal_callable_free;
			cinfo.token = enrollment.registry.records[enrollment.reference.slot].original_owner;
			ports.callable_create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&normal_callable), &cinfo);

			const Error q_err = Error(ports.qual_open(&Enrollment::ABI, enrollment.ready, enrollment.context, enrollment.session,
					{ enrollment.instance, enrollment.holder.handle_serial }, &enrollment.qualification).native_error);
			const Error r_err = Error(ports.camera_relay(reinterpret_cast<GDExtensionConstTypePtr>(&normal_callable), &LiveMetalWaveGNativeOutputHarness::on_camera_relay, &camera_relay_ref).native_error);
			const Error b_err = (r_err == OK) ? Error(ports.camera_bind_relay(enrollment.attachment, camera_relay_ref).native_error) : r_err;

			const Error sr_err = Error(ports.scene_relay(reinterpret_cast<GDExtensionConstTypePtr>(&normal_callable), &LiveMetalWaveGNativeOutputHarness::on_scene_phase_typed, &scene_relay_ref).native_error);
			GDExtensionScenePhaseSinkConfigV1 phase_cfg = {};
			phase_cfg.abi = { 1, sizeof(phase_cfg), 0 };
			phase_cfg.phase = 1;
			phase_cfg.max_operations = 16;
			const Error sc_err = (sr_err == OK) ? Error(ports.scene_capability(enrollment.attachment, &phase_cfg.device_capability).native_error) : sr_err;
			const Error sp_err = (sc_err == OK) ? Error(ports.scene_register(scene_relay_ref, enrollment.attachment, &phase_cfg, &scene_reg_ref).native_error) : sc_err;
			record_check("qualification_and_relay_open", q_err == OK && b_err == OK && sp_err == OK);
		}
		enrollment.frame.leave();
		record_check("enrollment_initialized", init_err == OK);

		if (init_err == OK && failures.is_empty()) {
			GDExtensionStorageBackingV1 src_cap2 = {};
			GDExtensionStorageCreateRequestV1 src_cap2_origin = {};
			GDExtensionStorageBackingV1 src_cap4 = {};
			GDExtensionStorageCreateRequestV1 src_cap4_origin = {};
			const Error s2_err = create_qualified_source_buffer(0u, 2u, src_cap2, src_cap2_origin);
			const Error s4_err = create_qualified_source_buffer(1u, 4u, src_cap4, src_cap4_origin);
			record_check("qualified_sources_created", s2_err == OK && s4_err == OK);

			if (s2_err == OK && s4_err != OK) {
				retire_qualified_source_buffer(src_cap2, src_cap2_origin);
			}
			if (s2_err == OK && s4_err == OK) {
				// 1. NATIVE-001 + NATIVE-002: INSTANCE_192 (capacity=2, motion=DISABLED, previous=0) — Order A (PrepareV3 -> PlanV1) + Live Metal Color Draw!
				run_single_frame_profile("instance192_no_motion_v3_then_plan", 1u, 2u, 0u, false, true, mesh_rid, material_rid, src_cap2, 0u);

				// 2. NATIVE-001 + NATIVE-002: INSTANCE_192 (capacity=2, motion=DISABLED, previous=0) — Order B (PlanV1 -> PrepareV3) + Live Metal Color Draw!
				run_single_frame_profile("instance192_no_motion_plan_then_v3", 1u, 2u, 0u, true, true, mesh_rid, material_rid, src_cap2, 0u);

				// 3. NATIVE-002: PARTICLE_LIGHT_64 (capacity=4) — Both call orders!
				run_single_frame_profile("light64_v3_then_plan", 4u, 4u, 0u, false, false, RID(), RID(), src_cap4, 1u);
				run_single_frame_profile("light64_plan_then_v3", 4u, 4u, 0u, true, false, RID(), RID(), src_cap4, 1u);

				// 4. NATIVE-002: INDEXED_144 (capacity=2, motion=DISABLED) — End-to-End + Live Metal Color Draw!
				run_single_frame_profile("indexed144_no_motion_end_to_end", 2u, 2u, 0u, false, true, RID(), material_rid, src_cap2, 0u);

				// 5. NATIVE-002 + NATIVE-S01: INDEXED_144 Two-Frame Non-Reset Motion History End-to-End on Live Metal!
				run_indexed144_two_frame_motion_history("indexed144_two_frame_motion_history", material_rid, src_cap2);

				record_check("retire_src_cap4", retire_qualified_source_buffer(src_cap4, src_cap4_origin) == OK);
				record_check("retire_src_cap2", retire_qualified_source_buffer(src_cap2, src_cap2_origin) == OK);
			}

			if (enrollment.request && enrollment.request->serial != 0) {
				const GDExtensionProductionRequestRefV1 dummy_ref = { enrollment.instance, enrollment.request->serial };
				const Error dummy_rel = release_request_cleanly(dummy_ref);
				record_check("initial_request_released", dummy_rel == OK);
				if (dummy_rel == OK) {
					enrollment.request = nullptr;
				}
			}
			if (enrollment.attachment.handle_serial != 0) {
				GDExtensionScenePhaseCloseV1 scene_closed = {};
				Error sp_unreg = Error(ports.scene_unregister(scene_reg_ref, &scene_closed).native_error);
				normal_callable = Callable();
				scene_callable = Callable();
				for (uint32_t turn = 0; turn < 8 && sp_unreg == ERR_BUSY; ++turn) {
					server->sync();
					if (MessageQueue::get_singleton()) {
						MessageQueue::get_singleton()->flush();
					}
					sp_unreg = Error(ports.scene_unregister(scene_reg_ref, &scene_closed).native_error);
				}
				record_check("scene_and_shared_camera_relay_unregistered", sp_unreg == OK && scene_closed.relay_holder_closed == 1);

				const Error close_err = Error(ports.close_v1(enrollment.attachment).native_error);
				record_check("attachment_closed", close_err == OK);
				if (close_err == OK) {
					enrollment.attachment = {};
				}
			}
			normal_callable = Callable();
			if (!failures.is_empty()) {
				print_line("WAVE_G_PRE_CLEANUP_FAILURES=" + Variant(failures).to_json_string() + " DIAG=" + Variant(diagnostics).to_json_string());
			}
			server->sync();
			enrollment.server = nullptr;
			enrollment.cleanup_owner(true);
			record_check("owner_cleaned", enrollment.owner_cleaned && enrollment.holder.production_children == 0 && enrollment.holder.qualification_children == 0);
		}

		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_self_addr, uint64_t p_rd_addr) {
			auto *self_ptr = reinterpret_cast<LiveMetalWaveGNativeOutputHarness *>(uintptr_t(p_self_addr));
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			for (int i = 0; i < self_ptr->deferred_idx144_holds.size(); ++i) {
				self_ptr->deferred_idx144_holds.write[i].cleanup(p_rd, RID());
			}
			self_ptr->deferred_idx144_holds.clear();
			if (p_rd) {
				if (self_ptr->shared_fb.is_valid()) {
					p_rd->free_rid(self_ptr->shared_fb);
					self_ptr->shared_fb = RID();
				}
				if (self_ptr->shared_depth_tex.is_valid()) {
					p_rd->free_rid(self_ptr->shared_depth_tex);
					self_ptr->shared_depth_tex = RID();
				}
				if (self_ptr->shared_color_tex.is_valid()) {
					p_rd->free_rid(self_ptr->shared_color_tex);
					self_ptr->shared_color_tex = RID();
				}
			}
		})).bind(uint64_t(uintptr_t(this)), uint64_t(uintptr_t(device))));
		server->free_rid(material_rid);
		server->free_rid(shader_rid);
		server->free_rid(mesh_rid);
		server->sync();

		const auto after_counts = singleton_counts();
		const bool counts_restored =
				after_counts.records == baseline_counts.records &&
				after_counts.views == baseline_counts.views &&
				after_counts.devices == baseline_counts.devices &&
				after_counts.device_refs == baseline_counts.device_refs &&
				after_counts.attachments == baseline_counts.attachments &&
				after_counts.private_refs == baseline_counts.private_refs &&
				after_counts.external_refs == baseline_counts.external_refs;
		record_check("singleton_counts_restored", counts_restored);

		out["pass"] = failures.is_empty();
		out["checks"] = checks;
		out["failures"] = failures;
		out["diagnostics"] = diagnostics;
		return out;
	}
};
