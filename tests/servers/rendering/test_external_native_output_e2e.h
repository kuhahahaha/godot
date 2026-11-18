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
	GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithHistoryGuardV2 stage_plan_with_history_guard_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryFallbackV2 stage_history_fallback_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryStaticV2 stage_history_static_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackUploadV2 prepare_history_fallback_upload_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2 read_staged_output_input_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2 prepare_native_record_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2 prepare_native_batch_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2 native_batch_read_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2 consumed_prepare_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedReadV2 consumed_read_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2 consumed_borrow_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2 consumed_return_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1 record_schedule = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2 record_consume_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAckSegmentV1 ack_segment = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadRevisionV1 read_revision = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAppendV2 append_v2 = nullptr;

	GDExtensionInterfaceDeviceSessionOutputAcquireV1 output_acquire_v1 = nullptr;
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
	GDExtensionInterfaceDeviceSessionSceneOperationBindV1 scene_operation_bind = nullptr;
	GDExtensionInterfaceDeviceSessionSceneDepthSnapshotArmV1 scene_depth_arm = nullptr;
	GDExtensionInterfaceDeviceSessionSceneDepthSnapshotBorrowV1 scene_depth_borrow = nullptr;

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
		stage_plan_with_history_guard_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithHistoryGuardV2>("device_session_production_stage_output_plan_with_history_guard_v2");
		stage_history_fallback_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryFallbackV2>("device_session_production_stage_output_history_fallback_v2");
		stage_history_static_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryStaticV2>("device_session_production_stage_output_history_static_v2");
		prepare_history_fallback_upload_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackUploadV2>("device_session_production_prepare_history_fallback_upload_v2");
		read_staged_output_input_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2>("device_session_production_read_staged_output_input_v2");
		prepare_native_record_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2>("device_session_production_prepare_native_record_v2");
		prepare_native_batch_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2>("device_session_production_prepare_native_batch_v2");
		native_batch_read_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2>("device_session_production_native_batch_read_v2");

		consumed_prepare_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2>("device_session_output_consumed_prepare_v2");
		consumed_read_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedReadV2>("device_session_output_consumed_read_v2");
		consumed_borrow_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2>("device_session_output_consumed_borrow_v2");
		consumed_return_v2 = get_fn<GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2>("device_session_output_consumed_return_v2");

		record_schedule = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
		record_consume_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		ack_segment = get_fn<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>("device_session_production_ack_segment_v1");
		read_revision = get_fn<GDExtensionInterfaceDeviceSessionProductionReadRevisionV1>("device_session_production_read_revision_v1");
		append_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionAppendV2>("device_session_production_append_v2");

		output_acquire_v1 = get_fn<GDExtensionInterfaceDeviceSessionOutputAcquireV1>("device_session_output_acquire_v1");
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
		scene_operation_bind = get_fn<GDExtensionInterfaceDeviceSessionSceneOperationBindV1>("device_session_scene_operation_bind_v1");
		scene_depth_arm = get_fn<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotArmV1>("device_session_scene_depth_snapshot_arm_v1");
		scene_depth_borrow = get_fn<GDExtensionInterfaceDeviceSessionSceneDepthSnapshotBorrowV1>("device_session_scene_depth_snapshot_borrow_v1");

		return qual_open && qual_storage_create && qual_storage_retire && qual_storage_observe && qual_storage_release &&
				qual_unit_create && qual_unit_retire && qual_unit_observe && qual_unit_release &&
				metadata_query && output_create_v3 && output_withdraw &&
				req_quote_v1 && req_quote_v2 && reserve_v2 && reserve_readbacks && import_storage && import_unit && set_create &&
				prepare_upload_v2 && prepare_dispatch_v2 && prepare_readback_v2 && prepare_host_transform && prepare_output_scene && prepare_host_view && read_camera_ready &&
				prepare_output_v3 && prepare_plan_v1 && prepare_plan_v2 && stage_plan_v2 &&
				stage_plan_with_history_guard_v2 && stage_history_fallback_v2 && stage_history_static_v2 &&
				prepare_history_fallback_upload_v2 && read_staged_output_input_v2 && prepare_native_record_v2 &&
				prepare_native_batch_v2 && native_batch_read_v2 &&
				consumed_prepare_v2 && consumed_read_v2 && consumed_borrow_v2 && consumed_return_v2 &&
				record_schedule && record_consume_v2 && ack_segment && read_revision && append_v2 &&
				output_acquire_v1 && output_acquire_v2 && output_finalize_v1 && output_seal_v1 && output_cancel_v1 && output_cancel_schedule && output_return_consume &&
				observe_v3 && copy_v1 && cancel_v1 && release_v2 && close_v1 &&
				callable_create && camera_relay && camera_bind_relay && camera_bind_request && camera_close &&
				scene_capability && scene_relay && scene_register && scene_unregister &&
				scene_operation_bind && scene_depth_arm && scene_depth_borrow;
	}
};

static inline uint32_t wave_g_f2u(float p_val) {
	uint32_t bits = 0;
	memcpy(&bits, &p_val, sizeof(uint32_t));
	return bits;
}

static inline void wave_g_fill_instance192_record(uint32_t *r_words, float p_tx, float p_ty, float p_tz, uint32_t p_dense_id) {
	memset(r_words, 0, 192);
	// Rows 0..2: 3x4 affine row-major model transform: identity basis + translation (tx, ty, tz)
	r_words[0] = wave_g_f2u(1.0f);
	r_words[3] = wave_g_f2u(p_tx);
	r_words[5] = wave_g_f2u(1.0f);
	r_words[7] = wave_g_f2u(p_ty);
	r_words[10] = wave_g_f2u(1.0f);
	r_words[11] = wave_g_f2u(p_tz);
	// Rows 3..5: 3x3 normal matrix (pad words 15, 19, 23 must be zero!)
	r_words[12] = wave_g_f2u(1.0f);
	r_words[17] = wave_g_f2u(1.0f);
	r_words[22] = wave_g_f2u(1.0f);
	// Rows 6..8: 3x4 inverse_model transform: identity basis + translation (-tx, -ty, -tz)
	r_words[24] = wave_g_f2u(1.0f);
	r_words[27] = wave_g_f2u(-p_tx);
	r_words[29] = wave_g_f2u(1.0f);
	r_words[31] = wave_g_f2u(-p_ty);
	r_words[34] = wave_g_f2u(1.0f);
	r_words[35] = wave_g_f2u(-p_tz);
	// Row 9: RGBA color (1, 1, 1, 1)
	r_words[36] = wave_g_f2u(1.0f);
	r_words[37] = wave_g_f2u(1.0f);
	r_words[38] = wave_g_f2u(1.0f);
	r_words[39] = wave_g_f2u(1.0f);
	// Row 11: dense ID + flags (pad word 47 must be zero!)
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

		// =========================================================================
		// 3. Role 9 (GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1 = 9):
		//    4-role PrepareV3 ({1,7,9} & {1,2,7,9} at capacity = 4) + malformed rejections
		// =========================================================================
		CHECK(GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1 == 9);
		for (uint32_t aux_case = 0; aux_case < 4; ++aux_case) {
			// 0: {1, 7, 9} no-motion + aux (capacity=4, aux_bytes=128) -> OK
			// 1: {1, 2, 7, 9} motion + aux (capacity=4, aux_bytes=128) -> OK
			// 2: malformed aux_bytes = 96 (!= 4 * 32) -> ERR_INVALID_DATA
			// 3: malformed capacity == 0 with aux_bytes = 32 -> ERR_INVALID_DATA
			const bool with_prev = (aux_case == 1);
			const uint64_t aux_bytes = (aux_case == 2) ? 96u : 128u;
			const bool is_cap0 = (aux_case == 3);
			const uint64_t gen = 700 + aux_case;

			Vector<GDExtensionProductionResourceV1> res_vec;
			res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 });
			if (is_cap0) {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
			} else {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 768, 0 });
				if (with_prev) {
					res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 768, 0 });
				}
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, aux_bytes, 0 });
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

			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 2300 + aux_case, 1, 10, 25 };
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
				zeros.resize_initialized(res_vec[i].physical_bytes);
				const GDExtensionProductionUploadV2 up = {
					production_dto_abi<GDExtensionProductionUploadV2>(),
					uint32_t(i), 10, uint32_t(1 + i), 0,
					uint32_t(i), 0, 0, res_vec[i].physical_bytes
				};
				REQUIRE(ports.prepare_upload_v2(req, &up, zeros.ptr(), zeros.size()).native_error == OK);
			}

			const auto acq_in = make_common_acquire_input(9300 + aux_case, gen);
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
			prep_v3.read_capacity = 2;
			prep_v3.read_graph_capacity = 8;
			prep_v3.source_generation = 1;
			prep_v3.input = acq_in;
			for (uint32_t i = 0; i < 8; ++i) {
				prep_v3.roles[i] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
			}
			if (is_cap0) {
				prep_v3.roles[0] = { 1, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
				prep_v3.roles[1] = { 7, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
				prep_v3.roles[2] = { 9, 1, 1, 0, gen, 32 };
			} else if (with_prev) {
				prep_v3.roles[0] = { 1, 1, 1, 0, gen, 768 };
				prep_v3.roles[1] = { 2, 2, 2, 0, gen, 768 };
				prep_v3.roles[2] = { 7, 3, 3, 0, gen, 32 };
				prep_v3.roles[3] = { 9, 4, 4, 0, gen, aux_bytes };
			} else {
				prep_v3.roles[0] = { 1, 1, 1, 0, gen, 768 };
				prep_v3.roles[1] = { 7, 2, 2, 0, gen, 32 };
				prep_v3.roles[2] = { 9, 3, 3, 0, gen, aux_bytes };
			}

			const Error v3_err = Error(ports.prepare_output_v3(req, &prep_v3).native_error);
			if (aux_case >= 2) {
				CHECK(v3_err == ERR_INVALID_DATA);
			} else {
				REQUIRE(v3_err == OK);
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
				REQUIRE(ports.prepare_plan_v1(req, &plan_v1).native_error == OK);
				REQUIRE(ports.prepare_native_record_v2(req).native_error == OK);
				{
					MutexLock lock(fixture.registry.mutex);
					ProductionRequest *internal_req = nullptr;
					RenderingDevice *actual_rd = nullptr;
					REQUIRE(fixture.registry.production_request_locked(req, false, internal_req, actual_rd) == OK);
					CHECK(internal_req->actions[0].output_role_count == 4);
					CHECK(internal_req->actions[0].output_roles[0].bytes == 768);
					CHECK(internal_req->actions[0].output_roles[1].bytes == (with_prev ? 768u : 0u));
					CHECK(internal_req->actions[0].output_roles[2].bytes == 32);
					CHECK(internal_req->actions[0].output_roles[3].role == 9);
					CHECK(internal_req->actions[0].output_roles[3].bytes == 128);
				}
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

static void run_scene_depth_snapshot_c_abi_lifecycle_with_device(
		RenderingDevice *p_metal_rd,
		uint64_t p_viewport_rid,
		const GDExtensionDeviceExit::SceneDepthSnapshotCAbiTestCallbacks &p_callbacks) {
	using namespace GDExtensionDeviceExit;
	REQUIRE(p_metal_rd != nullptr);
	REQUIRE(p_viewport_rid != 0ull);
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	REQUIRE(server != nullptr);
	server->sync();

	WaveGNativeOutputPorts ports;
	REQUIRE(ports.load());

	struct MainInstanceScope {
		RenderingDevice *rd = nullptr;
		bool prev = false;
		MainInstanceScope(RenderingDevice *p_rd) : rd(p_rd), prev(p_rd->is_main_instance) {
			rd->is_main_instance = true;
		}
		~MainInstanceScope() {
			if (rd) {
				rd->is_main_instance = prev;
			}
		}
	} main_scope(p_metal_rd);

	struct CAbiBorrowProbe {
		WaveGNativeOutputPorts *ports = nullptr;
		uint64_t viewport_rid = 0;
		GDExtensionSceneOperationKeyV1 key = {};
		GDExtensionSceneDepthSnapshotBorrowResultV1 borrow_out = {};
		GDExtensionSceneDepthSnapshotBorrowResultV1 retry_out = {};
		Error borrow_err = ERR_UNCONFIGURED;
		Error retry_err = ERR_UNCONFIGURED;
		Error append_err = ERR_UNCONFIGURED;
		GDExtensionProductionRequestRefV1 target_req = {};
		uint32_t calls = 0;
		SceneDepthSnapshotCAbiTestCallbacks callbacks = {};

		static uint64_t word(const GDExtensionSceneOpaque32V1 &p_op, uint32_t p_idx) {
			uint64_t val = 0;
			for (uint32_t i = 0; i < 8; ++i) {
				val |= uint64_t(p_op.bytes[p_idx * 8 + i]) << (8 * i);
			}
			return val;
		}
		static void on_callable_free(void *) {}
		static void on_callable_call(void *, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr r_ret, GDExtensionCallError *r_err) {
			*reinterpret_cast<Variant *>(r_ret) = Variant();
			*r_err = { GDEXTENSION_CALL_OK, 0, 0 };
		}
		static void on_scene_phase(void *p_userdata, const GDExtensionNativeScenePhaseV1 *p_phase, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *p_op) {
			if (!p_phase) {
				return;
			}
			const ProductionPhysicalContextRef ctx = {
				word(p_phase->physical_context, 0),
				word(p_phase->physical_context, 1)
			};
			on_record(p_userdata, ctx, p_req, p_op);
		}
		static void on_record(void *p_userdata, ProductionPhysicalContextRef p_ctx, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *) {
			auto &self = *static_cast<CAbiBorrowProbe *>(p_userdata);
			if (p_req.handle_serial != self.target_req.handle_serial || p_req.registry_instance != self.target_req.registry_instance) {
				return;
			}
			++self.calls;
			self.borrow_err = Error(self.ports->scene_depth_borrow(
					p_ctx, p_req, 0, self.viewport_rid, &self.key, &self.borrow_out).native_error);
			self.retry_err = Error(self.ports->scene_depth_borrow(
					p_ctx, p_req, 0, self.viewport_rid, &self.key, &self.retry_out).native_error);
			if (self.callbacks.on_borrowed_in_phase) {
				self.callbacks.on_borrowed_in_phase(self.callbacks.userdata, self.borrow_out);
			}
			uint64_t rev = 0;
			Error e = Error(self.ports->read_revision(p_ctx, p_req, &rev).native_error);
			if (e == OK) {
				GDExtensionProductionAppendV2 app = {};
				e = Error(self.ports->append_v2(p_ctx, p_req, 0, rev, &app).native_error);
			}
			self.append_err = e;
		}
	};

	CAbiBorrowProbe probe;
	probe.ports = &ports;
	probe.viewport_rid = p_viewport_rid;
	probe.callbacks = p_callbacks;

	OrdinalPlan pool = {};
	pool.attachment_requests = 4;
	pool.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
	pool.capacity.requests = 4;
	pool.capacity.suboperations = 8;
	pool.capacity.resources = 8;
	pool.capacity.resource_uses = 64;
	pool.capacity.graph_memberships = 32;
	pool.capacity.return_cells = 8;
	pool.capacity.retained_payload_bytes = 4096;

	SingletonEnrollment enrollment;
	{
		MutexLock rd_lock(p_metal_rd->_thread_safe_);
		MutexLock reg_lock(enrollment.registry.mutex);
		REQUIRE(enrollment.registry.production_membership_bound_locked(*p_metal_rd, pool.capacity.graph_memberships, p_metal_rd->transfer_worker_pool_max_size, pool.capacity.native_memberships) == OK);
	}
	enrollment.public_port = false;
	enrollment.server = server;
	REQUIRE(enrollment.initialize_owner(
					1, false, true, &CAbiBorrowProbe::on_record, &probe, false, 1, false,
					+[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, p_metal_rd, true) == OK);

	Callable phase_callable;
	GDExtensionNormalRelayRefV1 scene_relay_ref = {};
	GDExtensionScenePhaseRegistrationRefV1 scene_reg_ref = {};
	{
		MutexLock reg_lock(enrollment.registry.mutex);
		enrollment.registry.records[enrollment.reference.slot].guard.entry_classes |= 1u << 8;
	}
	GDExtensionCallableCustomInfo2 cinfo = {};
	cinfo.callable_userdata = &probe;
	cinfo.call_func = &CAbiBorrowProbe::on_callable_call;
	cinfo.free_func = &CAbiBorrowProbe::on_callable_free;
	cinfo.token = enrollment.registry.records[enrollment.reference.slot].original_owner;
	ports.callable_create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&phase_callable), &cinfo);
	REQUIRE(ports.qual_open(&Enrollment::ABI, enrollment.ready, enrollment.context, enrollment.session,
					{ enrollment.instance, enrollment.holder.handle_serial }, &enrollment.qualification)
					.native_error == OK);
	REQUIRE(ports.scene_relay(reinterpret_cast<GDExtensionConstTypePtr>(&phase_callable), &CAbiBorrowProbe::on_scene_phase, &scene_relay_ref).native_error == OK);

	GDExtensionScenePhaseSinkConfigV1 phase_cfg = {};
	phase_cfg.abi = { 1, sizeof(phase_cfg), 0 };
	phase_cfg.phase = SCENE_PHASE_BEFORE_PARTICLES;
	phase_cfg.max_operations = 8;
	REQUIRE(ports.scene_capability(enrollment.attachment, &phase_cfg.device_capability).native_error == OK);
	REQUIRE(ports.scene_register(scene_relay_ref, enrollment.attachment, &phase_cfg, &scene_reg_ref).native_error == OK);
	enrollment.frame.leave();

	// Arm viewport via C ABI and verify invalid parameter rejection:
	GDExtensionSceneDepthSnapshotArmV1 bad_arm = {};
	CHECK(ports.scene_depth_arm(enrollment.attachment, nullptr).native_error == ERR_INVALID_PARAMETER);
	CHECK(ports.scene_depth_arm(enrollment.attachment, &bad_arm).native_error == ERR_INVALID_PARAMETER);

	GDExtensionSceneDepthSnapshotArmV1 arm_in = {};
	arm_in.abi = Enrollment::ABI;
	arm_in.abi.struct_size = sizeof(arm_in);
	arm_in.viewport_rid = p_viewport_rid;
	arm_in.slab_thickness_m = 0.50f;
	arm_in.silhouette_jump_m = 0.50f;
	REQUIRE(ports.scene_depth_arm(enrollment.attachment, &arm_in).native_error == OK);
	if (p_callbacks.on_armed) {
		p_callbacks.on_armed(p_callbacks.userdata);
	}

	// Create a qualified 32B storage buffer for a 1-action SCENE_PHASE_BEFORE_PARTICLES request:
	GDExtensionStorageCreateRequestV1 src_origin = {};
	src_origin.abi = Enrollment::ABI;
	src_origin.bytes = 32;
	src_origin.allocation_slot = 0;
	src_origin.part = 0;
	src_origin.allocation_generation = 1;
	src_origin.create_attempt = 1;
	GDExtensionStorageBackingV1 src_backing = {};
	REQUIRE(ports.qual_storage_create(enrollment.qualification, &src_origin, &src_backing).native_error == OK);
	p_metal_rd->_submit_transfer_workers();
	p_metal_rd->_wait_for_transfer_workers();
	p_metal_rd->swap_buffers(false);

	const uint32_t upload_words[8] = { 1, 2, 3, 4, 0, 0, 0, 0 };
	const GDExtensionProductionResourceV1 res = {
		GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, sizeof(upload_words), 0
	};
	const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, sizeof(upload_words), 0 };
	const GDExtensionProductionActionV2 act = {
		0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
	};
	const GDExtensionProductionUploadV2 up_dto = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 1, 0, 0, 0, 0, sizeof(upload_words) };
	GDExtensionProductionMetadataQuoteV1 quote = {};
	REQUIRE(ports.req_quote_v1(enrollment.attachment, &act, 1, &res, 1, &use, 1, &up_dto, 1, nullptr, 0, 0, sizeof(upload_words), &quote).native_error == OK);

	const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 9101, 1, 10, 11 };
	GDExtensionProductionRequestRefV1 req = {};
	REQUIRE(ports.reserve_v2(enrollment.attachment, &op, &quote.capacity, &act, 1, &res, 1, &use, 1, nullptr, 0, &req).native_error == OK);
	REQUIRE(ports.import_storage(req, 0, &src_backing).native_error == OK);
	REQUIRE(ports.prepare_upload_v2(req, &up_dto, upload_words, sizeof(upload_words)).native_error == OK);

	probe.key = {};
	probe.key.abi = { 1, sizeof(GDExtensionSceneOperationKeyV1), 0 };
	probe.key.owner_generation = enrollment.holder.generation;
	probe.key.transport_operation = op;
	probe.key.record_request = scene_phase_opaque(101, 102, 103, 104);
	probe.key.operation_ref = scene_phase_opaque(201, 202, 203, 204);
	probe.key.cursor_ref = scene_phase_opaque(301, 302, 303, 304);
	probe.key.schedule_digest = scene_phase_opaque(401, 402, 403, 404);
	{
		CallbackScope owner_entry(enrollment.reference);
		REQUIRE(owner_entry.get_error() == OK);
		REQUIRE(ports.scene_operation_bind(req, 0, &probe.key).native_error == OK);
	}

	probe.target_req = req;
	uint64_t ctrl = 0;
	REQUIRE(ports.record_schedule(req, &ctrl) == OK);
	server->sync();
	Registry::get_singleton().scene_phase_dispatch(*p_metal_rd);

	GDExtensionProductionRecordReportV2 rec = {};
	REQUIRE(ports.record_consume_v2(req, ctrl, &rec).native_error == OK);
	CHECK(rec.result.native_error == OK);
	if (rec.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
		CallbackScope owner_entry(enrollment.reference);
		REQUIRE(owner_entry.get_error() == OK);
		REQUIRE(ports.ack_segment(req, rec.segment.segment, rec.segment.segment_revision).native_error == OK);
	}

	CHECK(probe.calls == 1u);
	CHECK(probe.borrow_err == OK);
	CHECK(probe.retry_err == OK);
	CHECK(probe.append_err == OK);
	CHECK(memcmp(&probe.borrow_out, &probe.retry_out, sizeof(probe.borrow_out)) == 0);

	const GDExtensionProductionServiceBudgetV1 budget = {
		production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
		OS::get_singleton()->get_ticks_usec() + 10000000,
		8, 64, 64, 0
	};
	for (uint32_t turn = 0; turn < 64; ++turn) {
		p_metal_rd->_submit_transfer_workers();
		p_metal_rd->_wait_for_transfer_workers();
		if (!p_metal_rd->native_disposal_active) {
			p_metal_rd->swap_buffers(false);
		}
		GDExtensionProductionObserveResultV3 obs = {};
		const Error o1 = Error(ports.observe_v3(req, &budget, &obs).native_error);
		if (o1 != OK && o1 != ERR_BUSY) {
			break;
		}
		server->sync();
		(void)ports.observe_v3(req, &budget, &obs);
		if (obs.report.known_obligations > 0 && obs.report.complete_obligations == obs.report.known_obligations && obs.report.possible_obligations == 0) {
			break;
		}
	}

	if (p_callbacks.on_before_release) {
		p_callbacks.on_before_release(p_callbacks.userdata);
	}

	ports.cancel_v1(req);
	for (uint32_t turn = 0; turn < 64; ++turn) {
		GDExtensionProductionServiceBudgetV1 rem = {};
		GDExtensionProductionReportV2 rep = {};
		const Error rel_err = Error(ports.release_v2(req, &budget, &rem, &rep).native_error);
		if (rel_err == OK && !rep.service_pending) {
			break;
		}
		server->sync();
		(void)ports.release_v2(req, &budget, &rem, &rep);
		p_metal_rd->_submit_transfer_workers();
		p_metal_rd->_wait_for_transfer_workers();
		if (!p_metal_rd->native_disposal_active) {
			p_metal_rd->swap_buffers(false);
		}
	}

	if (p_callbacks.on_after_release) {
		p_callbacks.on_after_release(p_callbacks.userdata);
	}

	// Retire qualified storage buffer & close enrollment cleanly:
	{
		const GDExtensionStorageRetireRequestV1 ret_in = {
			Enrollment::ABI, src_backing.backing, src_backing.rid, src_backing.backing_serial, 0, 0, 1, 1, 0
		};
		GDExtensionStorageRetireRefV1 ret_ref = {};
		GDExtensionStorageRetireReportV1 ret_rep = {};
		REQUIRE(ports.qual_storage_retire(enrollment.qualification, &ret_in, &ret_ref, &ret_rep).native_error == OK);
		for (uint32_t turn = 0; turn < 64 && ret_rep.state != 1; ++turn) {
			const GDExtensionQualificationBudgetV1 qbudget = { Enrollment::ABI, 4, 5000, 0 };
			REQUIRE(ports.qual_storage_observe(enrollment.qualification, ret_ref, &qbudget, &ret_rep).native_error == OK);
			if (ret_rep.state != 1) {
				p_metal_rd->_submit_transfer_workers();
				p_metal_rd->_wait_for_transfer_workers();
				p_metal_rd->swap_buffers(false);
			}
		}
		REQUIRE(ret_rep.state == 1);
		REQUIRE(ports.qual_storage_release(enrollment.qualification, ret_ref).native_error == OK);
	}

	GDExtensionScenePhaseCloseV1 scene_closed = {};
	Error sp_unreg = Error(ports.scene_unregister(scene_reg_ref, &scene_closed).native_error);
	phase_callable = Callable();
	for (uint32_t turn = 0; turn < 8 && sp_unreg == ERR_BUSY; ++turn) {
		server->sync();
		if (MessageQueue::get_singleton()) {
			MessageQueue::get_singleton()->flush();
		}
		sp_unreg = Error(ports.scene_unregister(scene_reg_ref, &scene_closed).native_error);
	}
	CHECK(sp_unreg == OK);
	CHECK(ports.close_v1(enrollment.attachment).native_error == OK);
	enrollment.attachment = {};
	server->sync();
	enrollment.server = nullptr;
	enrollment.cleanup_owner(true);
	CHECK(enrollment.owner_cleaned);
}

static inline const bool s_scene_depth_c_abi_registered =
		(GDExtensionDeviceExit::g_scene_depth_snapshot_c_abi_test_runner = &Access::run_scene_depth_snapshot_c_abi_lifecycle_with_device, true);

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
		SCENE_DEPTH_BORROW,
	};

	struct SceneDepthBorrowPlan {
		uint64_t viewport_rid = 0;
		GDExtensionSceneOperationKeyV1 key = {};
		GDExtensionSceneDepthSnapshotBorrowResultV1 out = {};
		Error borrow_error = ERR_UNCONFIGURED;
		Error append_error = ERR_UNCONFIGURED;
	};

	struct PacketRecordPlan {
		GDExtensionProductionRequestRefV1 req = {};
		GDExtensionOutputAcquireInputV1 acquire_input = {};
		uint64_t acquire_ordinal = 0;
		Vector<uint64_t> pre_acquire_upload_ordinals;
		Vector<uint64_t> upload_ordinals;
		uint64_t finalize_ordinal = 0;
		uint64_t seal_ordinal = 0;
		Vector<uint64_t> readback_ordinals;
		bool read_staged_input_before_acquire = false;
		bool probe_v1_acquire_first = false;
		bool probe_premature_seal_before_last_upload = false;

		GDExtensionOutputAcquireResultV1 acquire_v1_probe_out = {};
		GDExtensionOutputSealResultV1 premature_seal_out = {};
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
	SceneDepthBorrowPlan *active_depth_borrow = nullptr;

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
			for (int i = 0; i < pkt.pre_acquire_upload_ordinals.size(); ++i) {
				uint64_t rev = 0;
				Error e = Error(self.ports.read_revision(p_ctx, p_req, &rev).native_error);
				if (e != OK) {
					pkt.upload_error = e;
					return;
				}
				GDExtensionProductionAppendV2 app = {};
				e = Error(self.ports.append_v2(p_ctx, p_req, pkt.pre_acquire_upload_ordinals[i], rev, &app).native_error);
				if (e != OK) {
					pkt.upload_error = e;
					return;
				}
			}
			if (pkt.read_staged_input_before_acquire) {
				GDExtensionProductionOutputPrepareV1 staged_prep = {};
				if (self.ports.read_staged_output_input_v2(p_req, pkt.acquire_ordinal, &staged_prep).native_error == OK) {
					pkt.acquire_input = staged_prep.input;
				}
			}
			if (pkt.probe_v1_acquire_first) {
				self.ports.output_acquire_v1(p_ctx, p_req, pkt.acquire_ordinal, &pkt.acquire_input, &pkt.acquire_v1_probe_out);
			}
			self.ports.output_acquire_v2(p_ctx, p_req, pkt.acquire_ordinal, &pkt.acquire_input, &pkt.acquire_out);
			if (pkt.acquire_out.status.error != OK) {
				return;
			}
			for (int i = 0; i < pkt.upload_ordinals.size(); ++i) {
				if (pkt.probe_premature_seal_before_last_upload && i + 1 == pkt.upload_ordinals.size()) {
					GDExtensionOutputSealInputV1 bad_seal = {};
					bad_seal.abi = GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputSealInputV1>();
					bad_seal.particles = pkt.acquire_input.particles;
					bad_seal.lease_token = pkt.acquire_out.lease_token;
					bad_seal.common = pkt.acquire_input.common;
					self.ports.output_seal_v1(p_ctx, p_req, &bad_seal, &pkt.premature_seal_out);
				}
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
			return;
		}
		if (self.active_kind == ActiveRecordKind::SCENE_DEPTH_BORROW && self.active_depth_borrow) {
			auto &plan = *self.active_depth_borrow;
			plan.borrow_error = Error(self.ports.scene_depth_borrow(
					p_ctx, p_req, 0, plan.viewport_rid, &plan.key, &plan.out).native_error);
			uint64_t rev = 0;
			Error e = Error(self.ports.read_revision(p_ctx, p_req, &rev).native_error);
			if (e == OK) {
				GDExtensionProductionAppendV2 app = {};
				e = Error(self.ports.append_v2(p_ctx, p_req, 0, rev, &app).native_error);
			}
			plan.append_error = e;
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

	Error create_qualified_source_buffer(uint32_t p_alloc_slot, uint32_t p_active_count, GDExtensionStorageBackingV1 &r_backing, GDExtensionStorageCreateRequestV1 &r_origin, uint64_t p_bytes = 32) {
		const uint64_t buf_bytes = MAX(uint64_t(32), p_bytes);
		Vector<uint8_t> init_bytes;
		init_bytes.resize(int(buf_bytes));
		memset(init_bytes.ptrw(), 0, size_t(buf_bytes));
		memcpy(init_bytes.ptrw(), &p_active_count, sizeof(uint32_t));
		r_origin = {};
		r_origin.abi = Enrollment::ABI;
		r_origin.bytes = buf_bytes;
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

		// Upload init_bytes into r_backing via a tiny 1-action UPLOAD_V1 request on enrollment.attachment.
		const GDExtensionProductionResourceV1 res = {
			GDEXTENSION_PRODUCTION_STORAGE_V1, p_alloc_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, buf_bytes, 0
		};
		const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, buf_bytes, 0 };
		const GDExtensionProductionActionV2 act = {
			0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		const GDExtensionProductionUploadV2 up_dto = { GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 1, 0, 0, 0, 0, buf_bytes };
		GDExtensionProductionMetadataQuoteV1 quote = {};
		Error err = Error(ports.req_quote_v1(enrollment.attachment, &act, 1, &res, 1, &use, 1, &up_dto, 1, nullptr, 0, 0, buf_bytes, &quote).native_error);
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
		err = Error(ports.prepare_upload_v2(up_req, &up_dto, init_bytes.ptr(), buf_bytes).native_error);
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
			OutputOwnerSession &r_sess,
			uint32_t p_extra_pass_mask = 0) {
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
		cfg.pass_mask = (p_profile == 4) ? 0u : (uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE) | p_extra_pass_mask);
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
	RID role9_extra_motion_fb;
	RD::FramebufferFormatID role9_extra_motion_fb_format = RD::INVALID_FORMAT_ID;
	bool role9_check_motion_pass = false;
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
		for (uint32_t draw_try = 0; draw_try < 5000; ++draw_try) {
			if (backend == 1) {
				err = TestProductionRequest::wave_g_mobile_draw_external(sel, &render_data, p_material, p_profile, owner_capacity > 0, fb_format, fb, sub_step, p_self.role9_check_motion_pass);
			} else {
				err = OK;
				if (p_self.role9_check_motion_pass && p_self.role9_extra_motion_fb.is_valid()) {
					err = TestProductionRequest::wave_g_clustered_draw_external(sel, &render_data, p_material, p_profile, owner_capacity > 0, p_self.role9_extra_motion_fb_format, p_self.role9_extra_motion_fb, sub_step, true);
				}
				if (err == OK) {
					err = TestProductionRequest::wave_g_clustered_draw_external(sel, &render_data, p_material, p_profile, owner_capacity > 0, fb_format, fb, sub_step, false);
				}
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

	static inline float wave_g_h2f(uint16_t p_bits) {
		return Math::half_to_float(p_bits);
	}

	void run_role9_aux_params_suite(const GDExtensionStorageBackingV1 &p_src_cap4, uint32_t p_src_slot) {
		using namespace GDExtensionDeviceExit;
		static constexpr uint32_t kCap = 4u;
		static constexpr uint32_t kRecBytes = kCap * 192u;
		static constexpr uint32_t kAuxBytes = kCap * 32u;
		static constexpr uint32_t kMetaBytes = 32u;

		// Create static quad mesh [-0.5, 0.5]^2 at z=0 with compressed attributes (matching fixed_instance.compressed_aabb) and WITHOUT ARRAY_FORMAT_CUSTOM0 / ARRAY_FORMAT_CUSTOM1.
		RID quad_mesh = server->mesh_create();
		{
			Array arr;
			arr.resize(RSE::ARRAY_MAX);
			PackedVector3Array vtx;
			vtx.push_back(Vector3(-0.5f, -0.5f, 0.0f));
			vtx.push_back(Vector3(-0.5f, 0.5f, 0.0f));
			vtx.push_back(Vector3(0.5f, 0.5f, 0.0f));
			vtx.push_back(Vector3(0.5f, -0.5f, 0.0f));
			PackedInt32Array idx;
			idx.push_back(0);
			idx.push_back(2);
			idx.push_back(1);
			idx.push_back(0);
			idx.push_back(3);
			idx.push_back(2);
			PackedVector3Array nrm;
			for (int i = 0; i < 4; ++i) {
				nrm.push_back(Vector3(0.0f, 0.0f, 1.0f));
			}
			PackedFloat32Array tan_arr;
			for (int i = 0; i < 4; ++i) {
				tan_arr.push_back(1.0f);
				tan_arr.push_back(0.0f);
				tan_arr.push_back(0.0f);
				tan_arr.push_back(1.0f);
			}
			PackedVector2Array uvs;
			uvs.push_back(Vector2(0.0f, 1.0f));
			uvs.push_back(Vector2(0.0f, 0.0f));
			uvs.push_back(Vector2(1.0f, 0.0f));
			uvs.push_back(Vector2(1.0f, 1.0f));
			arr[RSE::ARRAY_VERTEX] = vtx;
			arr[RSE::ARRAY_INDEX] = idx;
			arr[RSE::ARRAY_NORMAL] = nrm;
			arr[RSE::ARRAY_TANGENT] = tan_arr;
			arr[RSE::ARRAY_TEX_UV] = uvs;
			server->mesh_add_surface_from_arrays(quad_mesh, RSE::PRIMITIVE_TRIANGLES, arr, Array(), Dictionary(), RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES);
		}
		RID aux_shader = server->shader_create_from_code(
				"shader_type spatial;\n"
				"render_mode unshaded, cull_disabled;\n"
				"varying vec4 v_c0;\n"
				"varying vec4 v_c1;\n"
				"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
				"void fragment() { ALBEDO = vec3(0.25) + v_c0.rgb + v_c1.rgb; }\n");
		RID aux_mat = server->material_create();
		server->material_set_shader(aux_mat, aux_shader);

		struct AuxRtTargets {
			RID fp16_color;
			RID fp16_depth;
			RID fp16_fb;
			RD::FramebufferFormatID fp16_fb_format = 0;
			RID mv_spec;
			RID mv_motion;
			RID mv_fb;
			RD::FramebufferFormatID mv_fb_format = 0;
		} rt_targets;

		server->call_on_render_thread(callable_mp_static((+[](RID p_mesh, RID p_mat, uint64_t p_rd_addr, uint64_t p_tgt_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			auto *ms = RendererRD::MeshStorage::get_singleton();
			auto *mats = RendererRD::MaterialStorage::get_singleton();
			auto *t = reinterpret_cast<AuxRtTargets *>(uintptr_t(p_tgt_addr));
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
			if (p_rd && t) {
				RD::TextureFormat c_tf;
				c_tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
				c_tf.width = 32;
				c_tf.height = 32;
				c_tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
				t->fp16_color = p_rd->texture_create(c_tf, RD::TextureView());

				RD::TextureFormat mv_tf;
				mv_tf.format = RD::DATA_FORMAT_R16G16_SFLOAT;
				mv_tf.width = 32;
				mv_tf.height = 32;
				mv_tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
				t->mv_motion = p_rd->texture_create(mv_tf, RD::TextureView());

				RD::TextureFormat d_tf;
				d_tf.format = RD::DATA_FORMAT_D32_SFLOAT;
				d_tf.width = 32;
				d_tf.height = 32;
				d_tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
				t->fp16_depth = p_rd->texture_create(d_tf, RD::TextureView());

				Vector<RID> fb1 = { t->fp16_color, t->fp16_depth };
				t->fp16_fb = p_rd->framebuffer_create(fb1);
				t->fp16_fb_format = p_rd->framebuffer_get_format(t->fp16_fb);

				RD::FramebufferPass mv_pass;
				mv_pass.color_attachments = { 0, RD::ATTACHMENT_UNUSED, 2 };
				mv_pass.depth_attachment = 3;
				Vector<RD::FramebufferPass> mv_passes = { mv_pass };
				Vector<RID> fb_mv_tex = { t->fp16_color, RID(), t->mv_motion, t->fp16_depth };
				t->mv_fb = p_rd->framebuffer_create_multipass(fb_mv_tex, mv_passes);
				t->mv_fb_format = p_rd->framebuffer_get_format(t->mv_fb);

				for (uint32_t f = 0; f < 2; ++f) {
					p_rd->_submit_transfer_workers();
					p_rd->_wait_for_transfer_workers();
					p_rd->swap_buffers(false);
				}
			}
		})).bind(quad_mesh, aux_mat, uint64_t(uintptr_t(device)), uint64_t(uintptr_t(&rt_targets))));
		server->sync();

		auto read_center_fp16 = [&](float &r_r, float &r_g, float &r_b) -> bool {
			Vector<uint8_t> bytes;
			server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rd_addr, RID p_tex, uint64_t p_out_addr) {
				auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
				auto *out_vec = reinterpret_cast<Vector<uint8_t> *>(uintptr_t(p_out_addr));
				if (p_rd && p_tex.is_valid()) {
					p_rd->_submit_transfer_workers();
					p_rd->_wait_for_transfer_workers();
					*out_vec = p_rd->texture_get_data(p_tex, 0);
				}
			})).bind(uint64_t(uintptr_t(device)), rt_targets.fp16_color, uint64_t(uintptr_t(&bytes))));
			server->sync();
			if (bytes.size() != 32 * 32 * 8) {
				return false;
			}
			const uint16_t *px = reinterpret_cast<const uint16_t *>(bytes.ptr() + (16 * 32 + 16) * 8);
			r_r = wave_g_h2f(px[0]);
			r_g = wave_g_h2f(px[1]);
			r_b = wave_g_h2f(px[2]);
			return true;
		};

		// Build canonical 4-instance current records (instance 0 centered at (0,0,-1.5) inside ortho frustum, instances 1..3 off-center) and role 9 aux payload.
		Vector<uint8_t> curr_bytes;
		curr_bytes.resize(kRecBytes);
		for (uint32_t i = 0; i < kCap; ++i) {
			wave_g_fill_instance192_record(reinterpret_cast<uint32_t *>(curr_bytes.ptrw() + i * 192u), i == 0 ? 0.0f : 1.2f, i == 0 ? 0.0f : 1.2f, -1.5f, i + 1u);
		}
		Vector<uint8_t> aux_good;
		aux_good.resize(kAuxBytes);
		for (uint32_t i = 0; i < kCap; ++i) {
			uint32_t *aw = reinterpret_cast<uint32_t *>(aux_good.ptrw() + i * 32u);
			// CUSTOM0 = (0.25, 0.5, 0.75, 1.0), CUSTOM1 = (0.1, 0.2, 0.3, 0.4)
			aw[0] = wave_g_f2u(0.25f);
			aw[1] = wave_g_f2u(0.50f);
			aw[2] = wave_g_f2u(0.75f);
			aw[3] = wave_g_f2u(1.00f);
			aw[4] = wave_g_f2u(0.10f);
			aw[5] = wave_g_f2u(0.20f);
			aw[6] = wave_g_f2u(0.30f);
			aw[7] = wave_g_f2u(0.40f);
		}
		const uint32_t meta_words[8] = { kCap, 0, 0, 0, kCap, 1u, 1u, 0u };
		Vector<uint8_t> meta_bytes;
		meta_bytes.resize(kMetaBytes);
		memcpy(meta_bytes.ptrw(), meta_words, kMetaBytes);

		// Helper for single-frame INSTANCE_192 execution with configurable roles ({1,7}, {1,7,9}, or {1,2,7,9}).
		auto run_aux_case = [&](const String &p_label, bool p_with_prev, bool p_with_aux, const Vector<uint8_t> &p_aux_payload, uint32_t p_expected_fault, bool p_probe_v1_and_seal, bool p_do_draw, bool p_motion_draw) {
			OutputOwnerSession sess;
			Error err = setup_owner_session(1u, kCap, p_with_prev ? 1u : 0u, 1u, quad_mesh, aux_mat, sess);
			record_check(p_label + ".create_v3", err == OK);
			if (err != OK) {
				return;
			}
			const uint64_t gen = sess.created.generation;
			struct UpSpec {
				uint32_t role;
				Vector<uint8_t> bytes;
			};
			Vector<UpSpec> ups;
			ups.push_back({ 1u, curr_bytes });
			if (p_with_prev) {
				ups.push_back({ 2u, curr_bytes });
			}
			ups.push_back({ 7u, meta_bytes });
			if (p_with_aux) {
				ups.push_back({ 9u, p_aux_payload });
			}

			Vector<GDExtensionProductionResourceV1> res_vec;
			res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, p_src_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 });
			for (int i = 0; i < ups.size(); ++i) {
				res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, ups[i].role, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, uint64_t(ups[i].bytes.size()), 0 });
			}
			const uint32_t guard_slot = uint32_t(res_vec.size());
			res_vec.push_back({ GDEXTENSION_PRODUCTION_STORAGE_V1, 1u, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 });

			Vector<GDExtensionProductionUseV1> use_vec;
			use_vec.push_back({ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 });
			for (int i = 0; i < ups.size(); ++i) {
				use_vec.push_back({ uint32_t(1 + i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, uint64_t(ups[i].bytes.size()), 0 });
			}
			const uint32_t acq_use_count = uint32_t(use_vec.size());
			Vector<GDExtensionProductionActionV2> act_vec;
			act_vec.push_back({ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, acq_use_count, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });

			PacketRecordPlan pkt;
			pkt.acquire_ordinal = 0;
			pkt.probe_v1_acquire_first = p_probe_v1_and_seal;
			pkt.probe_premature_seal_before_last_upload = p_probe_v1_and_seal;
			Vector<GDExtensionProductionUploadV2> up_dtos;
			uint64_t payload_total = 0;
			for (int i = 0; i < ups.size(); ++i) {
				const uint32_t u_idx = uint32_t(use_vec.size());
				const uint32_t a_ord = uint32_t(act_vec.size());
				use_vec.push_back({ uint32_t(1 + i), GDEXTENSION_PRODUCTION_WRITE_V1, 0, uint64_t(ups[i].bytes.size()), 0 });
				act_vec.push_back({ a_ord, 10, uint32_t(2 + i), 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, u_idx, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX });
				up_dtos.push_back({ production_dto_abi<GDExtensionProductionUploadV2>(), a_ord, 10, uint32_t(2 + i), 0, uint32_t(1 + i), 0, 0, uint64_t(ups[i].bytes.size()) });
				pkt.upload_ordinals.push_back(a_ord);
				payload_total += uint64_t(ups[i].bytes.size());
			}
			pkt.finalize_ordinal = uint64_t(act_vec.size());
			act_vec.push_back({ uint32_t(pkt.finalize_ordinal), 10, 20, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });
			pkt.seal_ordinal = uint64_t(act_vec.size());
			act_vec.push_back({ uint32_t(pkt.seal_ordinal), 10, 21, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX });

			const uint32_t rb_use = uint32_t(use_vec.size());
			use_vec.push_back({ guard_slot, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 });
			const uint32_t rb_ord = uint32_t(act_vec.size());
			act_vec.push_back({ rb_ord, 10, 22, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, rb_use, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 });
			pkt.readback_ordinals.push_back(rb_ord);
			const GDExtensionProductionReadbackSourceV1 rb_src = { guard_slot, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, rb_ord, 0, 32, { pkt.finalize_ordinal }, {} };

			const uint32_t read_cap = p_do_draw ? 2u : 0u;
			const uint32_t read_graph_cap = p_do_draw ? 112u : 0u;
			const GDExtensionProductionOutputDemandInputV1 demand_in = {
				production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
				sess.create_req, sess.created.particles, gen,
				0, 10, pkt.finalize_ordinal, 10, pkt.seal_ordinal, 10, 1, 20, 21,
				read_cap, read_graph_cap, 2, 0
			};
			GDExtensionProductionOutputGuardDemandV1 guard_demand = {};
			GDExtensionProductionMetadataQuoteV1 req_quote = {};
			err = Error(ports.req_quote_v2(
					enrollment.attachment, act_vec.ptr(), uint32_t(act_vec.size()), res_vec.ptr(), uint32_t(res_vec.size()),
					use_vec.ptr(), uint32_t(use_vec.size()), up_dtos.ptr(), uint32_t(up_dtos.size()), &rb_src, 1,
					6, payload_total, &demand_in, 1, &guard_demand, 1, &req_quote).native_error);
			record_check(p_label + ".req_quote_v2", err == OK);
			if (err != OK) {
				teardown_owner_session(sess);
				return;
			}
			const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 25 };
			err = Error(ports.reserve_readbacks(
					enrollment.attachment, &op, &req_quote.capacity, act_vec.ptr(), uint32_t(act_vec.size()),
					res_vec.ptr(), uint32_t(res_vec.size()), use_vec.ptr(), uint32_t(use_vec.size()), &rb_src, 1, &pkt.req).native_error);
			record_check(p_label + ".reserve", err == OK);
			if (err != OK) {
				teardown_owner_session(sess);
				return;
			}
			err = Error(ports.import_storage(pkt.req, 0, &p_src_cap4).native_error);
			for (int i = 0; i < ups.size() && err == OK; ++i) {
				err = Error(ports.prepare_upload_v2(pkt.req, &up_dtos[i], ups[i].bytes.ptr(), ups[i].bytes.size()).native_error);
			}
			if (err == OK) {
				err = Error(ports.prepare_readback_v2(pkt.req, rb_ord, 0).native_error);
			}
			GDExtensionProductionCameraSampleV1 cam_sample = {};
			if (err == OK) {
				err = prepare_host_and_capture_camera(sess, pkt.req, 0, 0x45, cam_sample);
			}
			record_check(p_label + ".prep_and_cam", err == OK);
			if (err != OK) {
				release_request_cleanly(pkt.req);
				teardown_owner_session(sess);
				return;
			}

			pkt.acquire_input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
			pkt.acquire_input.particles = sess.created.particles;
			pkt.acquire_input.view_epoch = cam_sample.camera.view_epoch;
			pkt.acquire_input.pass_mask = uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
			pkt.acquire_input.layout_digest.bytes[0] = 3;
			pkt.acquire_input.mapping_digest.bytes[0] = 4;
			pkt.acquire_input.common.generation = gen;
			pkt.acquire_input.common.present_seq = cam_sample.present_seq;
			pkt.acquire_input.common.output_tag.bytes[0] = 0x55;
			pkt.acquire_input.common.view_tag.bytes[0] = 0x65;
			memset(pkt.acquire_input.common.current_host_token.bytes, 0x45, 32);
			for (uint32_t a = 0; a < 3; ++a) {
				pkt.acquire_input.common.bounds.position[a] = -3.0;
				pkt.acquire_input.common.bounds.size[a] = 6.0;
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
			for (int i = 0; i < ups.size(); ++i) {
				prep_v3.roles[i] = { ups[i].role, uint32_t(1 + i), uint32_t(1 + i), 0, gen, uint64_t(ups[i].bytes.size()) };
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

			const Error pv3_err = Error(ports.prepare_output_v3(pkt.req, &prep_v3).native_error);
			const Error pl1_err = Error(ports.prepare_plan_v1(pkt.req, &plan_v1).native_error);
			record_check(p_label + ".prepare_v3_and_plan", pv3_err == OK && pl1_err == OK);

			Error fp16_draw_err = OK;
			GDExtensionProductionRecordReportV2 rec = {};
			const RID saved_fb = shared_fb;
			const RD::FramebufferFormatID saved_fb_fmt = shared_fb_format;
			shared_fb = rt_targets.fp16_fb;
			shared_fb_format = rt_targets.fp16_fb_format;
			role9_extra_motion_fb = rt_targets.mv_fb;
			role9_extra_motion_fb_format = rt_targets.mv_fb_format;
			role9_check_motion_pass = p_do_draw && p_motion_draw;
			err = execute_packet_and_color_draw_in_cull_scope(sess, pkt, cam_sample, p_do_draw, 1u, 0u, aux_mat, nullptr, fp16_draw_err, rec);
			shared_fb = saved_fb;
			shared_fb_format = saved_fb_fmt;
			role9_extra_motion_fb = RID();
			role9_extra_motion_fb_format = RD::INVALID_FORMAT_ID;
			role9_check_motion_pass = false;

			const bool rec_ok = err == OK &&
					pkt.acquire_out.status.error == OK &&
					pkt.acquire_out.writable_count == uint32_t(ups.size()) &&
					pkt.finalize_out.status.error == OK &&
					pkt.seal_out.status.error == OK;
			record_check(p_label + ".record_seal_ok", rec_ok);
			if (p_with_aux && rec_ok) {
				const auto &last_w = pkt.acquire_out.writables[pkt.acquire_out.writable_count - 1];
				record_check(p_label + ".role9_writable_meta", last_w.role == 9u && last_w.stride == 32u && last_w.byte_length == kAuxBytes);
			}
			if (p_probe_v1_and_seal) {
				record_check(p_label + ".acquire_v1_rejected_on_4role", pkt.acquire_v1_probe_out.status.error == ERR_INVALID_DATA);
				record_check(p_label + ".seal_before_role9_rejected", pkt.premature_seal_out.status.error == ERR_UNAUTHORIZED);
			}
			if (p_do_draw && rec_ok) {
				record_check(p_label + ".fp16_and_motion_draw", fp16_draw_err == OK);
			}

			rt_advance_rd_frame();
			uint32_t guard_words[8] = { 0xffffffffu };
			bool rb_ok = false;
			for (uint32_t turn = 0; turn < 256 && rec_ok; ++turn) {
				GDExtensionProductionObserveResultV3 obs = {};
				if (pump_observe_once(pkt.req, obs) != OK) {
					break;
				}
				if (obs.report.ready_readbacks >= 1 && obs.report.known_obligations > 0 &&
						obs.report.complete_obligations == obs.report.known_obligations && obs.report.possible_obligations == 0) {
					uint64_t copied = 0;
					if (ports.copy_v1(pkt.req, obs.report.revision, 0, guard_words, 32, &copied).native_error == OK && copied == 32) {
						rb_ok = true;
						break;
					}
				}
				rt_advance_rd_frame();
				OS::get_singleton()->delay_usec(300);
			}
			const uint32_t expected_instances = (p_expected_fault == 0u) ? kCap : 0u;
			record_check(p_label + ".guard_status_exact", rb_ok && guard_words[0] == p_expected_fault && guard_words[1] == expected_instances);

			if (p_do_draw && rec_ok && fp16_draw_err == OK) {
				float pr = -1.0f, pg = -1.0f, pb = -1.0f;
				const bool px_read = read_center_fp16(pr, pg, pb);
				if (p_with_aux) {
					// 0.25 + CUSTOM0(0.25, 0.50, 0.75) + CUSTOM1(0.10, 0.20, 0.30) = (0.60, 0.95, 1.30)
					const bool match = px_read && Math::abs(pr - 0.60f) <= 2e-3f && Math::abs(pg - 0.95f) <= 2e-3f && Math::abs(pb - 1.30f) <= 2e-3f;
					if (!match) {
						Array px_arr;
						px_arr.push_back(pr);
						px_arr.push_back(pg);
						px_arr.push_back(pb);
						diagnostics[p_label + ".px"] = px_arr;
					}
					record_check(p_label + ".pixel_matches_custom0_plus_custom1", match);
				} else {
					// Legacy {1, 7} without role 9 binds settled 32B zero sentinel -> CUSTOM0 == CUSTOM1 == vec4(0.0) -> ALBEDO = 0.25
					const bool match = px_read && Math::abs(pr - 0.25f) <= 2e-3f && Math::abs(pg - 0.25f) <= 2e-3f && Math::abs(pb - 0.25f) <= 2e-3f;
					if (!match) {
						Array px_arr;
						px_arr.push_back(pr);
						px_arr.push_back(pg);
						px_arr.push_back(pb);
						diagnostics[p_label + ".px"] = px_arr;
					}
					record_check(p_label + ".sentinel_pixel_zero_custom01", match);
				}
			}

			const Error rel = cancel_slot_and_release_request(p_label, pkt.req, sess.created.particles, pkt.acquire_out.lease_token);
			const Error tear = teardown_owner_session(sess);
			record_check(p_label + ".clean_retirement", rel == OK && tear == OK);
		};

		// T01 + T02 + T06: {1, 7, 9} no-motion + aux + AcquireV1/premature-Seal probes + FP16 pixel verification!
		run_aux_case("role9_no_motion_aux_draw_and_probes", false, true, aux_good, 0u, true, true, false);

		// T01 + T06: {1, 2, 7, 9} motion + aux + Clustered MOTION_VECTORS draw & Mobile MOTION_VECTORS shader compile + FP16 pixel verification!
		run_aux_case("role9_motion_aux_color_and_motion_draw", true, true, aux_good, 0u, false, true, true);

		// T07: Legacy 2-role {1, 7} (role 9 omitted) drawing CUSTOM0/1-reading shader via settled 32B external_aux_zero_sentinel_buffer!
		run_aux_case("role9_omitted_legacy_2role_sentinel_draw", false, false, Vector<uint8_t>(), 0u, false, true, false);

		// T05: Live Metal compute guard faults NaN (0x7fc00000u) and subnormal (0x00000001u) in role 9 -> NUMERIC_FAULT = 16u, instanceCount = 0!
		{
			Vector<uint8_t> aux_nan = aux_good;
			reinterpret_cast<uint32_t *>(aux_nan.ptrw())[13] = 0x7fc00000u;
			run_aux_case("role9_guard_fault_nan", false, true, aux_nan, 16u, false, false, false);

			Vector<uint8_t> aux_sub = aux_good;
			reinterpret_cast<uint32_t *>(aux_sub.ptrw())[31] = 0x00000001u;
			run_aux_case("role9_guard_fault_subnormal", false, true, aux_sub, 16u, false, false, false);
		}

		// T03: 4-role {1, 2, 7, 9} WithoutPrevious dynamic fallback (stage_output_plan_with_history_guard_v2 + prepare_output_v3 + stage_output_history_fallback_v2 -> {1, 7, 9} NO_MOTION_AUX)!
		{
			const String lbl = "role9_without_previous_fallback";
			OutputOwnerSession sess;
			Error err = setup_owner_session(1u, kCap, 1u, 2u, quad_mesh, aux_mat, sess);
			record_check(lbl + ".create_v3", err == OK);
			if (err == OK) {
				const uint64_t gen = sess.created.generation;
				// Frame 1: seal {1, 2, 7, 9} + color draw + consumed_prepare_v2 / consumed_read_v2 / consumed_borrow_v2!
				const GDExtensionProductionResourceV1 f1_res[6] = {
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, p_src_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 },
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kRecBytes, 0 },
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kRecBytes, 0 },
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kMetaBytes, 0 },
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kAuxBytes, 0 },
					{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 },
				};
				const GDExtensionProductionUseV1 f1_uses[10] = {
					{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
					{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
					{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
					{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
					{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kAuxBytes, 0 },
					{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
					{ 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
					{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
					{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kAuxBytes, 0 },
					{ 5, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
				};
				const GDExtensionProductionActionV2 f1_acts[8] = {
					{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 5, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
					{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 5, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
					{ 2, 10, 3, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 6, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
					{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 7, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
					{ 4, 10, 5, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 8, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
					{ 5, 10, 6, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
					{ 6, 10, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
					{ 7, 10, 8, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 9, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
				};
				const GDExtensionProductionUploadV2 f1_ups[4] = {
					{ production_dto_abi<GDExtensionProductionUploadV2>(), 1, 10, 2, 0, 1, 0, 0, kRecBytes },
					{ production_dto_abi<GDExtensionProductionUploadV2>(), 2, 10, 3, 0, 2, 0, 0, kRecBytes },
					{ production_dto_abi<GDExtensionProductionUploadV2>(), 3, 10, 4, 0, 3, 0, 0, kMetaBytes },
					{ production_dto_abi<GDExtensionProductionUploadV2>(), 4, 10, 5, 0, 4, 0, 0, kAuxBytes },
				};
				const GDExtensionProductionReadbackSourceV1 f1_rb = { 5, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 7, 0, 32, { 5 }, {} };
				const GDExtensionProductionOutputDemandInputV1 f1_dem_in = {
					production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
					sess.create_req, sess.created.particles, gen, 0, 10, 5, 10, 6, 10, 1, 6, 7, 1, 112, 2, 0
				};
				GDExtensionProductionOutputGuardDemandV1 f1_dem = {};
				GDExtensionProductionMetadataQuoteV1 f1_quote = {};
				err = Error(ports.req_quote_v2(enrollment.attachment, f1_acts, 8, f1_res, 6, f1_uses, 10, f1_ups, 4, &f1_rb, 1, 6, 2u * kRecBytes + kMetaBytes + kAuxBytes, &f1_dem_in, 1, &f1_dem, 1, &f1_quote).native_error);
				PacketRecordPlan f1_pkt;
				f1_pkt.acquire_ordinal = 0;
				f1_pkt.upload_ordinals = { 1, 2, 3, 4 };
				f1_pkt.finalize_ordinal = 5;
				f1_pkt.seal_ordinal = 6;
				f1_pkt.readback_ordinals = { 7 };
				const GDExtensionProductionOperationV1 f1_op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 15 };
				if (err == OK) {
					err = Error(ports.reserve_readbacks(enrollment.attachment, &f1_op, &f1_quote.capacity, f1_acts, 8, f1_res, 6, f1_uses, 10, &f1_rb, 1, &f1_pkt.req).native_error);
				}
				if (err == OK) {
					err = Error(ports.import_storage(f1_pkt.req, 0, &p_src_cap4).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_upload_v2(f1_pkt.req, &f1_ups[0], curr_bytes.ptr(), kRecBytes).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_upload_v2(f1_pkt.req, &f1_ups[1], curr_bytes.ptr(), kRecBytes).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_upload_v2(f1_pkt.req, &f1_ups[2], meta_bytes.ptr(), kMetaBytes).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_upload_v2(f1_pkt.req, &f1_ups[3], aux_good.ptr(), kAuxBytes).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_readback_v2(f1_pkt.req, 7, 0).native_error);
				}
				GDExtensionProductionCameraSampleV1 cam1 = {};
				if (err == OK) {
					err = prepare_host_and_capture_camera(sess, f1_pkt.req, 0, 0x75, cam1);
				}
				const GDExtensionOutputConsumedSelectorV2 f1_sel = { production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 0 };
				f1_pkt.acquire_input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
				f1_pkt.acquire_input.particles = sess.created.particles;
				f1_pkt.acquire_input.view_epoch = cam1.camera.view_epoch;
				f1_pkt.acquire_input.pass_mask = uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
				f1_pkt.acquire_input.layout_digest.bytes[0] = 3;
				f1_pkt.acquire_input.mapping_digest.bytes[0] = 4;
				f1_pkt.acquire_input.common.generation = gen;
				f1_pkt.acquire_input.common.present_seq = cam1.present_seq;
				f1_pkt.acquire_input.common.output_tag.bytes[0] = 0x85;
				f1_pkt.acquire_input.common.view_tag.bytes[0] = 0x95;
				memset(f1_pkt.acquire_input.common.current_host_token.bytes, 0x75, 32);
				for (uint32_t a = 0; a < 3; ++a) {
					f1_pkt.acquire_input.common.bounds.position[a] = -3.0;
					f1_pkt.acquire_input.common.bounds.size[a] = 6.0;
				}
				f1_pkt.acquire_input.common.history_reset = 1;
				f1_pkt.acquire_input.common.current_camera = cam1.camera;

				GDExtensionProductionOutputPrepareV3 f1_v3 = {};
				f1_v3.abi = { 3, sizeof(f1_v3), 0 };
				f1_v3.action_ordinal = 0;
				f1_v3.canonical_suboperation = 10;
				f1_v3.source_recipe_substep = 1;
				f1_v3.source_resource_slot = 0;
				f1_v3.source_use_index = 0;
				f1_v3.first_source_use = 0;
				f1_v3.source_use_count = 1;
				f1_v3.read_capacity = 1;
				f1_v3.read_graph_capacity = 112;
				f1_v3.source_generation = 1;
				f1_v3.input = f1_pkt.acquire_input;
				for (uint32_t r = 0; r < 8; ++r) {
					f1_v3.roles[r] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
				}
				f1_v3.roles[0] = { 1, 1, 1, 0, gen, kRecBytes };
				f1_v3.roles[1] = { 2, 2, 2, 0, gen, kRecBytes };
				f1_v3.roles[2] = { 7, 3, 3, 0, gen, kMetaBytes };
				f1_v3.roles[3] = { 9, 4, 4, 0, gen, kAuxBytes };

				GDExtensionProductionOutputPlanV1 f1_plan = {};
				f1_plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
				f1_plan.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
				f1_plan.acquire.action_ordinal = 0;
				f1_plan.acquire.canonical_suboperation = 10;
				f1_plan.acquire.source_recipe_substep = 1;
				f1_plan.acquire.source_resource_slot = 0;
				f1_plan.acquire.source_use_index = 0;
				f1_plan.acquire.first_source_use = 0;
				f1_plan.acquire.source_use_count = 1;
				f1_plan.acquire.read_capacity = 1;
				f1_plan.acquire.read_graph_capacity = 112;
				f1_plan.acquire.source_generation = 1;
				f1_plan.acquire.input = f1_pkt.acquire_input;
				f1_plan.finalize_action_ordinal = 5;
				f1_plan.seal_action_ordinal = 6;
				f1_plan.finalize_canonical_suboperation = 10;
				f1_plan.seal_canonical_suboperation = 10;
				f1_plan.finalize_source_recipe_substep = 6;
				f1_plan.seal_source_recipe_substep = 7;
				f1_plan.source_space = 2;
				for (uint32_t s = 0; s < 5; ++s) {
					f1_plan.reads[s] = { 0, 0, 1, s == 0 ? 0u : 1u };
				}
				f1_plan.guard_work_limit = f1_dem.guard_work;
				f1_plan.guard_dispatch_limit = f1_dem.guard_dispatches;
				f1_plan.guard_graph_limit = f1_dem.guard_graphs;
				f1_plan.guard_native_limit = f1_dem.guard_native_memberships;
				f1_plan.guard_owned_resource_limit = f1_dem.guard_owned_resources;

				if (err == OK) {
					err = Error(ports.prepare_output_v3(f1_pkt.req, &f1_v3).native_error);
				}
				if (err == OK) {
					err = Error(ports.prepare_plan_v1(f1_pkt.req, &f1_plan).native_error);
				}
				if (err == OK) {
					err = Error(ports.consumed_prepare_v2(f1_pkt.req, &f1_sel).native_error);
				}
				Error f1_draw_err = ERR_UNCONFIGURED;
				GDExtensionProductionRecordReportV2 f1_rec = {};
				if (err == OK) {
					err = execute_packet_and_color_draw_in_cull_scope(sess, f1_pkt, cam1, true, 1u, 0u, aux_mat, nullptr, f1_draw_err, f1_rec);
				}
				rt_advance_rd_frame();
				GDExtensionOutputConsumedProofV2 f1_consumed = {};
				bool f1_borrowed = false;
				for (uint32_t turn = 0; turn < 256 && err == OK && f1_draw_err == OK; ++turn) {
					GDExtensionProductionObserveResultV3 obs = {};
					pump_observe_once(f1_pkt.req, obs);
					server->sync();
					const auto b_budget = make_budget();
					GDExtensionOutputConsumedBorrowResultV2 borrow_res = {};
					const Error cb_err = Error(ports.consumed_borrow_v2(f1_pkt.req, &f1_sel, &b_budget, &borrow_res).native_error);
					if (cb_err == OK && borrow_res.result.native_error == OK) {
						if (ports.consumed_read_v2(f1_pkt.req, &borrow_res.proof.ref, &f1_consumed).native_error == OK) {
							f1_borrowed = true;
							break;
						}
					}
					rt_advance_rd_frame();
					OS::get_singleton()->delay_usec(300);
				}
				record_check(lbl + ".f1_consumed_borrowed", f1_borrowed);

				if (f1_borrowed) {
					// Frame 2: dynamic WithoutPrevious fallback ({1,2,7,9} primary -> {1,7,9} fallback)!
					// Create a 32 + kRecBytes scratch buffer for action 0 reset upload (4B) and fallback action 3 upload (kRecBytes, matching exact 32B-chunk staging graph bound).
					static constexpr uint64_t kResetScratchBytes = 32u + kRecBytes;
					GDExtensionStorageBackingV1 reset_buf = {};
					GDExtensionStorageCreateRequestV1 reset_orig = {};
					const Error rb_create = create_qualified_source_buffer(5u, 0u, reset_buf, reset_orig, kResetScratchBytes);
					record_check(lbl + ".f2_reset_buf_created", rb_create == OK);
					if (rb_create == OK) {
						const GDExtensionProductionResourceV1 f2_res[8] = {
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, p_src_slot, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 32, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 5, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, kResetScratchBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, gen, kRecBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kRecBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kRecBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kMetaBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, kAuxBytes, 0 },
							{ GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, gen, 32, 0 },
						};
						// Uses:
						// [0]: reset_buf WRITE 4B (action 0)
						// [1..6]: primary acquire uses {source READ, pred READ, role1 W, role2 W, role7 W, role9 W}
						// [7..10]: primary uploads {role1 W, role2 W, role7 W, role9 W}
						// [11]: guard status readback READ 32B
						// [12..15]: fallback acquire uses {source READ, role1 W, role7 W, role9 W} (omits pred & role2!)
						// [16]: fallback action 3 scratch upload {reset_buf offset 32, WRITE kRecBytes} (exact kRecBytes/32 staging graph bound!)
						const GDExtensionProductionUseV1 f2_uses[17] = {
							{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 4, 0 },
							{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
							{ 2, GDEXTENSION_PRODUCTION_READ_V1, 0, kRecBytes, 0 },
							{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
							{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
							{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
							{ 6, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kAuxBytes, 0 },
							{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
							{ 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
							{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
							{ 6, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kAuxBytes, 0 },
							{ 7, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
							{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 },
							{ 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kRecBytes, 0 },
							{ 5, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kMetaBytes, 0 },
							{ 6, GDEXTENSION_PRODUCTION_WRITE_V1, 0, kAuxBytes, 0 },
							{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 32, kRecBytes, 0 },
						};
						const GDExtensionProductionActionV2 f2_acts[9] = {
							{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 1, 6, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
							{ 2, 10, 3, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 7, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 8, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							{ 4, 10, 5, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 9, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							{ 5, 10, 6, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 10, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							{ 6, 10, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
							{ 7, 10, 8, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
							{ 8, 10, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 11, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 },
						};
						const GDExtensionProductionActionV2 f2_fb_acts[9] = {
							f2_acts[0],
							{ 1, 10, 2, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 12, 4, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX },
							f2_acts[2],
							{ 3, 10, 4, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 16, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
							f2_acts[4],
							f2_acts[5],
							f2_acts[6],
							f2_acts[7],
							f2_acts[8],
						};
						const GDExtensionProductionUploadV2 f2_ups[5] = {
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 1, 0, 1, 0, 0, 4 },
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 2, 10, 3, 0, 3, 0, 0, kRecBytes },
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 3, 10, 4, 0, 4, 0, 0, kRecBytes },
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 4, 10, 5, 0, 5, 0, 0, kMetaBytes },
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 5, 10, 6, 0, 6, 0, 0, kAuxBytes },
						};
						const GDExtensionProductionUploadV2 f2_fb_ups[5] = {
							f2_ups[0],
							f2_ups[1],
							{ production_dto_abi<GDExtensionProductionUploadV2>(), 3, 10, 4, 0, 1, 0, 32, kRecBytes },
							f2_ups[3],
							f2_ups[4],
						};
						const GDExtensionProductionReadbackSourceV1 f2_rb = { 7, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 8, 0, 32, { 6 }, {} };
						const GDExtensionProductionOutputDemandInputV1 f2_dem_in = {
							production_dto_abi<GDExtensionProductionOutputDemandInputV1>(),
							sess.create_req, sess.created.particles, gen, 1, 10, 6, 10, 7, 10, 2, 7, 8, 0, 0, 2, 0
						};
						GDExtensionProductionOutputGuardDemandV1 f2_dem = {};
						GDExtensionProductionMetadataQuoteV1 f2_quote = {};
						const uint64_t f2_payload_bytes = 2u * (4u + 2u * kRecBytes + kMetaBytes + kAuxBytes);
						err = Error(ports.req_quote_v2(enrollment.attachment, f2_acts, 9, f2_res, 8, f2_uses, 17, f2_ups, 5, &f2_rb, 1, 6, f2_payload_bytes, &f2_dem_in, 1, &f2_dem, 1, &f2_quote).native_error);
						f2_quote.capacity.suboperations = 18; // 2 * action_count for WithoutPrevious fallback bank!
						f2_quote.capacity.resource_uses += 16;
						f2_quote.capacity.graph_memberships += 4;
						{
							MutexLock reg_lock(enrollment.registry.mutex);
							MutexLock rd_lock(device->_thread_safe_);
							enrollment.registry.production_membership_bound_locked(*device, f2_quote.capacity.graph_memberships, device->transfer_worker_pool_max_size, f2_quote.capacity.native_memberships);
						}

						PacketRecordPlan f2_pkt;
						f2_pkt.pre_acquire_upload_ordinals = { 0 };
						f2_pkt.acquire_ordinal = 1;
						f2_pkt.upload_ordinals = { 2, 3, 4, 5 };
						f2_pkt.finalize_ordinal = 6;
						f2_pkt.seal_ordinal = 7;
						f2_pkt.readback_ordinals = { 8 };
						f2_pkt.read_staged_input_before_acquire = true;

						const GDExtensionProductionOperationV1 f2_op = { production_dto_abi<GDExtensionProductionOperationV1>(), ++next_nonce, 1, 10, 20 };
						if (err == OK) {
							err = Error(ports.reserve_readbacks(enrollment.attachment, &f2_op, &f2_quote.capacity, f2_acts, 9, f2_res, 8, f2_uses, 17, &f2_rb, 1, &f2_pkt.req).native_error);
						}
						const uint32_t zero_u32 = 0;
						const uint32_t f2_meta_words[8] = { kCap, 0, 0, 0, kCap, 1u, 2u, 0u };
						if (err == OK) {
							err = Error(ports.import_storage(f2_pkt.req, 0, &p_src_cap4).native_error);
						}
						if (err == OK) {
							err = Error(ports.import_storage(f2_pkt.req, 1, &reset_buf).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_upload_v2(f2_pkt.req, &f2_ups[0], &zero_u32, 4).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_upload_v2(f2_pkt.req, &f2_ups[1], curr_bytes.ptr(), kRecBytes).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_upload_v2(f2_pkt.req, &f2_ups[2], curr_bytes.ptr(), kRecBytes).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_upload_v2(f2_pkt.req, &f2_ups[3], f2_meta_words, kMetaBytes).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_upload_v2(f2_pkt.req, &f2_ups[4], aux_good.ptr(), kAuxBytes).native_error);
						}
						if (err == OK) {
							err = Error(ports.prepare_readback_v2(f2_pkt.req, 8, 0).native_error);
						}
						record_check(lbl + ".f2_primary_uploads_prepared", err == OK);

						// Prepare host/scene/view on action 1 BEFORE camera capture (staged mode).
						NormalHostPrepContext prep_ctx;
						prep_ctx.self = this;
						prep_ctx.req = f2_pkt.req;
						prep_ctx.ordinal = 1;
						prep_ctx.host_in.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
						prep_ctx.host_in.affine[0] = prep_ctx.host_in.affine[5] = prep_ctx.host_in.affine[10] = 1.0f;
						memset(prep_ctx.host_in.host_token.bytes, 0x76, 32);
						prep_ctx.scene_in = { production_dto_abi<GDExtensionProductionOutputSceneInputV1>(), sess.instance.get_id(), sess.created.particles, gen, sess.scenario.get_id(), prep_ctx.host_in };
						prep_ctx.view_in = { production_dto_abi<GDExtensionProductionHostViewInputV1>(), sess.viewport.get_id(), sess.camera.get_id() };
						active_normal_prep = &prep_ctx;
						Variant ret_v;
						Callable::CallError ce;
						normal_callable.callp(nullptr, 0, ret_v, ce);
						active_normal_prep = nullptr;
						server->call_on_render_thread(callable_mp_static(+[]() {
							if (auto *scene = static_cast<RendererSceneCull *>(RSG::scene)) {
								scene->update_dirty_instances();
							}
						}));
						server->sync();
						record_check(lbl + ".f2_host_view_prepared", prep_ctx.err == OK);

						// Staged WithPrevious input (zeroed camera/present/view_epoch per output_decode staged contract).
						GDExtensionOutputAcquireInputV1 staged_in = {};
						staged_in.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
						staged_in.particles = sess.created.particles;
						staged_in.pass_mask = uint32_t(RendererRD::ExternalOutputConfig::PASS_OPAQUE);
						staged_in.layout_digest.bytes[0] = 3;
						staged_in.mapping_digest.bytes[0] = 4;
						staged_in.common.generation = gen;
						staged_in.common.output_tag.bytes[0] = 0x86;
						staged_in.common.view_tag.bytes[0] = 0x95;
						memset(staged_in.common.current_host_token.bytes, 0x76, 32);
						for (uint32_t a = 0; a < 3; ++a) {
							staged_in.common.bounds.position[a] = -3.0;
							staged_in.common.bounds.size[a] = 6.0;
						}
						staged_in.common.history_reset = 0;
						staged_in.common.has_previous_consumed_tuple = 1;
						staged_in.common.previous_consumed_tuple_token = f1_consumed.ref.original_color_token;

						GDExtensionProductionOutputPlanV2 f2_plan_v2 = {};
						f2_plan_v2.abi = { 2, sizeof(f2_plan_v2), 0 };
						f2_plan_v2.plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
						f2_plan_v2.plan.acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
						f2_plan_v2.plan.acquire.action_ordinal = 1;
						f2_plan_v2.plan.acquire.canonical_suboperation = 10;
						f2_plan_v2.plan.acquire.source_recipe_substep = 2;
						f2_plan_v2.plan.acquire.source_resource_slot = 0;
						f2_plan_v2.plan.acquire.source_use_index = 1;
						f2_plan_v2.plan.acquire.first_source_use = 1;
						f2_plan_v2.plan.acquire.source_use_count = 2; // source + predecessor!
						f2_plan_v2.plan.acquire.source_generation = 1;
						f2_plan_v2.plan.acquire.input = staged_in;
						f2_plan_v2.plan.finalize_action_ordinal = 6;
						f2_plan_v2.plan.seal_action_ordinal = 7;
						f2_plan_v2.plan.finalize_canonical_suboperation = 10;
						f2_plan_v2.plan.seal_canonical_suboperation = 10;
						f2_plan_v2.plan.finalize_source_recipe_substep = 7;
						f2_plan_v2.plan.seal_source_recipe_substep = 8;
						f2_plan_v2.plan.source_space = 2;
						for (uint32_t s = 0; s < 5; ++s) {
							f2_plan_v2.plan.reads[s] = { 0, 1, 1, s == 0 ? 0u : 1u };
						}
						f2_plan_v2.plan.guard_work_limit = f2_dem.guard_work;
						f2_plan_v2.plan.guard_dispatch_limit = f2_dem.guard_dispatches;
						f2_plan_v2.plan.guard_graph_limit = f2_dem.guard_graphs;
						f2_plan_v2.plan.guard_native_limit = f2_quote.capacity.native_memberships;
						f2_plan_v2.plan.guard_owned_resource_limit = f2_dem.guard_owned_resources;
						f2_plan_v2.predecessor = { production_dto_abi<GDExtensionProductionOutputPredecessorV2>(), f1_consumed.ref, 2, 2 };

						const GDExtensionProductionServiceBudgetV1 fixed_stage_budget = make_budget();
						Error st_err = ERR_TIMEOUT;
						for (uint32_t turn = 0; turn < 256; ++turn) {
							server->sync();
							const auto turn_budget = make_budget();
							st_err = Error(ports.stage_plan_with_history_guard_v2(f2_pkt.req, &f2_plan_v2, UINT32_MAX, &fixed_stage_budget, &turn_budget).native_error);
							if (st_err != ERR_BUSY) {
								break;
							}
							OS::get_singleton()->delay_usec(400);
						}
						record_check(lbl + ".f2_stage_plan_with_history_guard_v2", st_err == OK);

						GDExtensionProductionOutputPrepareV3 f2_v3 = {};
						f2_v3.abi = { 3, sizeof(f2_v3), 0 };
						f2_v3.action_ordinal = 1;
						f2_v3.canonical_suboperation = 10;
						f2_v3.source_recipe_substep = 2;
						f2_v3.source_resource_slot = 0;
						f2_v3.source_use_index = 1;
						f2_v3.first_source_use = 1;
						f2_v3.source_use_count = 2;
						f2_v3.source_generation = 1;
						f2_v3.input = staged_in;
						for (uint32_t r = 0; r < 8; ++r) {
							f2_v3.roles[r] = { 0, UINT32_MAX, UINT32_MAX, 0, 0, 0 };
						}
						f2_v3.roles[0] = { 1, 3, 3, 0, gen, kRecBytes };
						f2_v3.roles[1] = { 2, 4, 4, 0, gen, kRecBytes };
						f2_v3.roles[2] = { 7, 5, 5, 0, gen, kMetaBytes };
						f2_v3.roles[3] = { 9, 6, 6, 0, gen, kAuxBytes };
						const Error v3_e = Error(ports.prepare_output_v3(f2_pkt.req, &f2_v3).native_error);
						record_check(lbl + ".f2_prepare_output_v3_4role", v3_e == OK);

						// Stage WithoutPrevious fallback plan (roles = {}, has_previous_consumed_tuple = 0, history_reset = 7, source_use_count = 1 at use 12).
						GDExtensionProductionOutputPlanV1 fb_plan = f2_plan_v2.plan;
						fb_plan.acquire.source_use_index = 12;
						fb_plan.acquire.first_source_use = 12;
						fb_plan.acquire.source_use_count = 1;
						fb_plan.acquire.input.common.has_previous_consumed_tuple = 0;
						memset(fb_plan.acquire.input.common.previous_consumed_tuple_token.bytes, 0, 32);
						fb_plan.acquire.input.common.history_reset = 7;
						for (uint32_t s = 0; s < 5; ++s) {
							fb_plan.reads[s] = { 0, 12, 1, s == 0 ? 0u : 1u };
						}
						const Error fb_stage_e = Error(ports.stage_history_fallback_v2(f2_pkt.req, &fb_plan, f2_fb_acts, 9).native_error);
						record_check(lbl + ".f2_stage_history_fallback_v2", fb_stage_e == OK);

						const bool fb_ups_ok =
								ports.prepare_history_fallback_upload_v2(f2_pkt.req, &f2_fb_ups[0], &zero_u32, 4).native_error == OK &&
								ports.prepare_history_fallback_upload_v2(f2_pkt.req, &f2_fb_ups[1], curr_bytes.ptr(), kRecBytes).native_error == OK &&
								ports.prepare_history_fallback_upload_v2(f2_pkt.req, &f2_fb_ups[2], curr_bytes.ptr(), kRecBytes).native_error == OK &&
								ports.prepare_history_fallback_upload_v2(f2_pkt.req, &f2_fb_ups[3], f2_meta_words, kMetaBytes).native_error == OK &&
								ports.prepare_history_fallback_upload_v2(f2_pkt.req, &f2_fb_ups[4], aux_good.ptr(), kAuxBytes).native_error == OK;
						record_check(lbl + ".f2_fallback_uploads_prepared", fb_ups_ok);

						GDExtensionProductionOutputHistoryStaticV2 hist_static = {};
						hist_static.abi = production_dto_abi<GDExtensionProductionOutputHistoryStaticV2>();
						hist_static.kind = 2; // Dynamic WithoutPrevious fallback!
						hist_static.static_mask = 0;
						hist_static.previous_color_token = f1_consumed.ref.original_color_token;
						hist_static.reset_upload_action = 0;
						hist_static.fallback_reset_upload_action = 0;
						GDExtensionProductionOutputHistoryStateV2 hist_state = {};
						const Error hs_e = Error(ports.stage_history_static_v2(f2_pkt.req, 1, &hist_static, &hist_state).native_error);
						record_check(lbl + ".f2_stage_history_static_v2", hs_e == OK);

						// Bind camera observer & seal static staged executor + batch (validates 4-role WithoutPrevious fallback!).
						const Error cb_e = Error(ports.camera_bind_request(f2_pkt.req, 1, camera_relay_ref, sess.viewport.get_id()).native_error);
						Error pnr_e = ERR_UNCONFIGURED;
						for (uint32_t turn = 0; turn < 32 && cb_e == OK; ++turn) {
							GDExtensionProductionObserveResultV3 tmp_obs = {};
							(void)pump_observe_once(f2_pkt.req, tmp_obs);
							server->sync();
							pnr_e = Error(ports.prepare_native_record_v2(f2_pkt.req).native_error);
							if (pnr_e == OK || (pnr_e != ERR_BUSY && pnr_e != ERR_UNCONFIGURED)) {
								break;
							}
							OS::get_singleton()->delay_usec(300);
						}
						const Error pnb_e = (pnr_e == OK) ? Error(ports.prepare_native_batch_v2(f2_pkt.req).native_error) : pnr_e;
						record_check(lbl + ".f2_prepare_native_record_v2_validated", cb_e == OK && pnr_e == OK && pnb_e == OK);
						if (pnr_e != OK || pnb_e != OK) {
							diagnostics[lbl + ".pnr_err"] = int(pnr_e);
							diagnostics[lbl + ".pnb_err"] = int(pnb_e);
						}

						uint64_t f2_ctrl = 0;
						const Error f2_sched = (cb_e == OK && pnb_e == OK) ? Error(ports.record_schedule(f2_pkt.req, &f2_ctrl)) : ERR_UNCONFIGURED;
						if (f2_sched == OK) {
							server->sync();
							// Capture camera WITHOUT previous camera & deliver native pre-cull + native invocation -> triggers WithoutPrevious fallback!
							struct StagedExecRt {
								LiveMetalWaveGNativeOutputHarness *self = nullptr;
								OutputOwnerSession *sess = nullptr;
								GDExtensionProductionRequestRefV1 req = {};
								uint64_t ctrl = 0;
								GDExtensionProductionRecordReportV2 rec = {};
								Error consume_err = ERR_UNCONFIGURED;
							} srt = { this, &sess, f2_pkt.req, f2_ctrl, {}, ERR_UNCONFIGURED };
							server->call_on_render_thread(callable_mp_static((+[](uint64_t p_addr) {
								auto *s = reinterpret_cast<StagedExecRt *>(uintptr_t(p_addr));
								auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
								if (!scene || !RSG::viewport || !s->self->device) {
									s->consume_err = ERR_UNAVAILABLE;
									return;
								}
								RendererViewport::Viewport::CameraSourceSubscriber *sub = nullptr;
								{
									MutexLock reg_lock(s->self->enrollment.registry.mutex);
									GDExtensionDeviceExit::ProductionRequest *internal_req = nullptr;
									RenderingDevice *actual_rd = nullptr;
									if (s->self->enrollment.registry.production_request_locked(s->req, false, internal_req, actual_rd) == OK && internal_req && internal_req->action_count > 1) {
										sub = &internal_req->actions[1].camera_ready_subscriber;
									}
								}
								if (!sub) {
									s->consume_err = ERR_UNAVAILABLE;
									return;
								}
								RendererSceneRender::CameraData cam_data;
								Projection proj;
								proj.set_orthogonal(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 20.0f);
								cam_data.set_camera(Transform3D(Basis(), Vector3(0, 0, 5)), proj, true, false, Vector2(), 0, 1);
								cam_data.source_viewport = s->sess->viewport;
								const auto tok = RSG::viewport->viewport_begin_camera_capture(s->sess->viewport, cam_data, s->sess->camera, s->sess->camera_buffers);
								s->sess->view_id = tok.view_id;
								s->sess->view_epoch = tok.view_epoch;

								Ref<RendererViewport::Viewport::CameraCapture> capture;
								if (!RSG::viewport->viewport_get_camera_capture_ref(s->sess->viewport, tok, capture) || capture.is_null()) {
									s->consume_err = ERR_UNAVAILABLE;
									return;
								}
								RendererSceneCull::NativeOutputCullScope cull_scope(*scene, capture);
								TestProductionRequest::wave_g_deliver_native_invocation(*RSG::viewport, capture, sub);
								s->consume_err = OK;
							})).bind(uint64_t(uintptr_t(&srt))));
							server->sync();
							if (srt.consume_err == OK) {
								srt.consume_err = Error(ports.record_consume_v2(f2_pkt.req, f2_ctrl, &srt.rec).native_error);
							}

							// Drain all 9 prepared batch action results (acknowledging segments) and read bound staged output input!
							GDExtensionOutputAcquireResultV1 f2_acq = {};
							GDExtensionProductionOutputFinalizeResultV1 f2_fin = {};
							GDExtensionOutputSealResultV1 f2_seal = {};
							uint32_t f2_batch_items = 0;
							uint64_t f2_cursor = 0;
							Error f2_batch_err = srt.consume_err == OK ? Error(srt.rec.result.native_error) : srt.consume_err;
							while (f2_batch_err == OK && f2_batch_items < 16) {
								GDExtensionProductionNativeBatchItemV1 item = {};
								f2_batch_err = Error(ports.native_batch_read_v2(f2_pkt.req, f2_ctrl, f2_cursor, &item).native_error);
								if (f2_batch_err != OK || !(item.flags & 1u)) {
									if (f2_batch_err == OK) {
										f2_batch_err = ERR_INVALID_DATA;
									}
									break;
								}
								if (item.result.output_kind == 1) {
									f2_acq = item.result.acquire;
								} else if (item.result.output_kind == 2) {
									f2_fin = item.result.finalize;
								} else if (item.result.output_kind == 3) {
									f2_seal = item.result.seal;
								}
								if (item.flags & 2u) {
									GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
									if (owner_entry.get_error() == OK) {
										f2_batch_err = Error(ports.ack_segment(f2_pkt.req, item.segment.segment, item.segment.segment_revision).native_error);
									} else {
										f2_batch_err = owner_entry.get_error();
									}
								}
								++f2_batch_items;
								f2_cursor = item.next_cursor;
								if (f2_cursor == 0) {
									break;
								}
							}
							GDExtensionProductionOutputPrepareV1 f2_bound_in = {};
							const Error f2_rsi = Error(ports.read_staged_output_input_v2(f2_pkt.req, 1, &f2_bound_in).native_error);

							const bool f2_fallback_ok = f2_batch_err == OK &&
									f2_batch_items == 9u &&
									f2_rsi == OK &&
									f2_bound_in.input.common.history_reset == 7u &&
									f2_bound_in.input.common.has_previous_consumed_tuple == 0u &&
									f2_acq.status.error == OK &&
									f2_acq.writable_count == 3u &&
									f2_acq.writables[0].role == 1u &&
									f2_acq.writables[1].role == 7u &&
									f2_acq.writables[2].role == 9u &&
									f2_acq.writables[2].byte_length == kAuxBytes &&
									f2_fin.status.error == OK &&
									f2_seal.status.error == OK;
							record_check(lbl + ".f2_fallback_acquired_3writables_and_sealed", f2_fallback_ok);
							if (!f2_fallback_ok) {
								Dictionary d;
								d["batch_err"] = int(f2_batch_err);
								d["batch_items"] = f2_batch_items;
								d["rsi_err"] = int(f2_rsi);
								d["reset"] = f2_bound_in.input.common.history_reset;
								d["acq_err"] = f2_acq.status.error;
								d["writables"] = f2_acq.writable_count;
								d["fin_err"] = f2_fin.status.error;
								d["seal_err"] = f2_seal.status.error;
								diagnostics[lbl + ".f2_fail"] = d;
							}

							uint32_t f2_gw[8] = { 0xffffffffu };
							bool f2_rb_ok = false;
							if (f2_fallback_ok) {
								rt_advance_rd_frame();
								for (uint32_t turn = 0; turn < 256; ++turn) {
									GDExtensionProductionObserveResultV3 obs = {};
									if (pump_observe_once(f2_pkt.req, obs) != OK) {
										break;
									}
									if (obs.report.ready_readbacks >= 1 && obs.report.known_obligations > 0 &&
											obs.report.complete_obligations == obs.report.known_obligations && obs.report.possible_obligations == 0) {
										uint64_t copied = 0;
										if (ports.copy_v1(f2_pkt.req, obs.report.revision, 0, f2_gw, 32, &copied).native_error == OK && copied == 32) {
											f2_rb_ok = true;
											break;
										}
									}
									rt_advance_rd_frame();
									OS::get_singleton()->delay_usec(300);
								}
							}
							record_check(lbl + ".f2_no_motion_aux_guard_zero_faults", f2_rb_ok && f2_gw[0] == 0u && f2_gw[1] == kCap);
							const Error r2 = cancel_slot_and_release_request(lbl + ".f2", f2_pkt.req, sess.created.particles, f2_acq.lease_token);
							record_check(lbl + ".f2_released", r2 == OK);
						} else {
							release_request_cleanly(f2_pkt.req);
						}
						record_check(lbl + ".retire_reset_buf", retire_qualified_source_buffer(reset_buf, reset_orig) == OK);
					}
					for (uint32_t turn = 0; turn < 256; ++turn) {
						server->sync();
						const auto rb_b = make_budget();
						GDExtensionProductionReportV2 rb_r = {};
						if (ports.consumed_return_v2(f1_pkt.req, &f1_consumed.ref, &rb_b, &rb_r).native_error != ERR_BUSY) {
							break;
						}
						OS::get_singleton()->delay_usec(300);
					}
				}
				const Error r1 = cancel_slot_and_release_request(lbl + ".f1", f1_pkt.req, sess.created.particles, f1_pkt.acquire_out.lease_token);
				const Error tear = teardown_owner_session(sess);
				record_check(lbl + ".clean_retirement", r1 == OK && tear == OK);
			}
		}

		// Clean up FP16/motion targets and quad mesh/material.
		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rd_addr, uint64_t p_tgt_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			auto *t = reinterpret_cast<AuxRtTargets *>(uintptr_t(p_tgt_addr));
			if (p_rd && t) {
				for (RID *fb : { &t->mv_fb, &t->fp16_fb }) {
					if (fb->is_valid()) {
						p_rd->free_rid(*fb);
						*fb = RID();
					}
				}
				for (RID *tx : { &t->mv_motion, &t->mv_spec, &t->fp16_depth, &t->fp16_color }) {
					if (tx->is_valid()) {
						p_rd->free_rid(*tx);
						*tx = RID();
					}
				}
			}
		})).bind(uint64_t(uintptr_t(device)), uint64_t(uintptr_t(&rt_targets))));
		server->free_rid(aux_mat);
		server->free_rid(aux_shader);
		server->free_rid(quad_mesh);
		server->sync();
	}

	bool borrow_and_verify_scene_depth_v2(
			const String &p_tag,
			RID p_viewport,
			const GDExtensionStorageBackingV1 &p_src_backing,
			float p_expected_world_z,
			bool p_test_samplerless_compute,
			uint64_t &r_snapshot_gen) {
		using namespace GDExtensionDeviceExit;
		r_snapshot_gen = 0ull;

		const uint32_t dummy_words[8] = { 7, 0, 0, 0, 0, 0, 0, 0 };
		const GDExtensionProductionResourceV1 res = {
			GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, sizeof(dummy_words), 0
		};
		const GDExtensionProductionUseV1 use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, sizeof(dummy_words), 0 };
		const GDExtensionProductionActionV2 act = {
			0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		const GDExtensionProductionUploadV2 up_dto = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 10, 1, 0, 0, 0, 0, sizeof(dummy_words) };
		GDExtensionProductionMetadataQuoteV1 quote = {};
		Error err = Error(ports.req_quote_v1(enrollment.attachment, &act, 1, &res, 1, &use, 1, &up_dto, 1, nullptr, 0, 0, sizeof(dummy_words), &quote).native_error);
		if (err != OK) {
			record_check(p_tag + ".quote", false);
			return false;
		}
		const uint64_t nonce = ++next_nonce;
		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), nonce, 1, 10, 11 };
		GDExtensionProductionRequestRefV1 req = {};
		err = Error(ports.reserve_v2(enrollment.attachment, &op, &quote.capacity, &act, 1, &res, 1, &use, 1, nullptr, 0, &req).native_error);
		if (err != OK) {
			record_check(p_tag + ".reserve", false);
			return false;
		}
		if (ports.import_storage(req, 0, &p_src_backing).native_error != OK ||
				ports.prepare_upload_v2(req, &up_dto, dummy_words, sizeof(dummy_words)).native_error != OK) {
			release_request_cleanly(req);
			record_check(p_tag + ".prep", false);
			return false;
		}

		SceneDepthBorrowPlan plan;
		plan.viewport_rid = p_viewport.get_id();
		plan.key.abi = { 1, sizeof(GDExtensionSceneOperationKeyV1), 0 };
		plan.key.owner_generation = enrollment.holder.generation;
		plan.key.transport_operation = op;
		plan.key.record_request = scene_phase_opaque(nonce, 102, 103, 104);
		plan.key.operation_ref = scene_phase_opaque(nonce, 202, 203, 204);
		plan.key.cursor_ref = scene_phase_opaque(nonce, 302, 303, 304);
		plan.key.schedule_digest = scene_phase_opaque(nonce, 402, 403, 404);
		{
			CallbackScope owner_entry(enrollment.reference);
			err = owner_entry.get_error();
			if (err == OK) {
				err = Error(ports.scene_operation_bind(req, 0, &plan.key).native_error);
			}
		}
		if (err != OK) {
			release_request_cleanly(req);
			record_check(p_tag + ".bind_key", false);
			return false;
		}

		active_depth_borrow = &plan;
		GDExtensionProductionRecordReportV2 rec = {};
		err = run_record_sync(req, ActiveRecordKind::SCENE_DEPTH_BORROW, rec);
		active_depth_borrow = nullptr;

		const bool borrow_ok = (err == OK &&
				plan.borrow_error == OK &&
				plan.append_error == OK &&
				plan.out.outcome.outcome == 1u &&
				plan.out.meta_ssbo_rid != 0ull &&
				plan.out.depth_texture_rid != 0ull &&
				plan.out.snapshot_generation >= 1ull &&
				plan.out.width == 64u &&
				plan.out.height == 64u &&
				plan.out.flags == 3u);
		record_check(p_tag + ".borrow_permit_and_flags", borrow_ok);
		if (!borrow_ok) {
			Dictionary d;
			d["run_err"] = int(err);
			d["borrow_err"] = int(plan.borrow_error);
			d["append_err"] = int(plan.append_error);
			d["outcome"] = plan.out.outcome.outcome;
			d["gen"] = plan.out.snapshot_generation;
			d["w"] = plan.out.width;
			d["h"] = plan.out.height;
			d["flags"] = plan.out.flags;
			diagnostics[p_tag + ".borrow_diag"] = d;
			release_request_cleanly(req);
			return false;
		}
		r_snapshot_gen = plan.out.snapshot_generation;

		struct ReadbackVerifyState {
			uint64_t meta_rid = 0;
			uint64_t tex_rid = 0;
			bool test_compute = false;
			bool meta_ok = false;
			bool tex_ok = false;
			bool compute_ok = true;
			float raw_center = -1.0f;
			float world_z = 0.0f;
		} verify;
		verify.meta_rid = plan.out.meta_ssbo_rid;
		verify.tex_rid = plan.out.depth_texture_rid;
		verify.test_compute = p_test_samplerless_compute;

		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rd_addr, uint64_t p_v_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			auto *st = reinterpret_cast<ReadbackVerifyState *>(uintptr_t(p_v_addr));
			if (!p_rd || !st) {
				return;
			}
			const RID meta_buf = RID::from_uint64(st->meta_rid);
			const RID depth_tex = RID::from_uint64(st->tex_rid);
			const Vector<uint8_t> meta_bytes = p_rd->buffer_get_data(meta_buf);
			const Vector<uint8_t> tex_bytes = p_rd->texture_get_data(depth_tex, 0);
			if (meta_bytes.size() == 256 && tex_bytes.size() == 64 * 64 * 4) {
				uint32_t magic = 0;
				uint32_t ver = 0;
				uint32_t w = 0;
				uint32_t h = 0;
				uint32_t flags = 0;
				memcpy(&magic, meta_bytes.ptr() + 0, 4);
				memcpy(&ver, meta_bytes.ptr() + 4, 4);
				memcpy(&w, meta_bytes.ptr() + 8, 4);
				memcpy(&h, meta_bytes.ptr() + 12, 4);
				memcpy(&flags, meta_bytes.ptr() + 236, 4);
				st->meta_ok = (magic == 0x4b303644u && ver == 2u && w == 64u && h == 64u && flags == 3u);

				const float *texels = reinterpret_cast<const float *>(tex_bytes.ptr());
				const float d_c = texels[32 * 64 + 32];
				st->raw_center = d_c;
				st->tex_ok = (Math::is_finite(d_c) && d_c > 0.005f && d_c < 0.99f);

				float clip_to_world[16] = {};
				memcpy(clip_to_world, meta_bytes.ptr() + 96, 64);
				const float nx = ((32.0f + 0.5f) / 64.0f) * 2.0f - 1.0f;
				const float ny = ((32.0f + 0.5f) / 64.0f) * 2.0f - 1.0f;
				const float wx = clip_to_world[0] * nx + clip_to_world[4] * ny + clip_to_world[8] * d_c + clip_to_world[12];
				const float wy = clip_to_world[1] * nx + clip_to_world[5] * ny + clip_to_world[9] * d_c + clip_to_world[13];
				const float wz = clip_to_world[2] * nx + clip_to_world[6] * ny + clip_to_world[10] * d_c + clip_to_world[14];
				const float ww = clip_to_world[3] * nx + clip_to_world[7] * ny + clip_to_world[11] * d_c + clip_to_world[15];
				(void)wx;
				(void)wy;
				if (Math::is_finite(ww) && Math::abs(ww) > 1e-6f) {
					st->world_z = wz / ww;
				}
			}

			if (st->test_compute) {
				st->compute_ok = false;
				static constexpr const char *kCompGlsl =
						"#version 450\n"
						"#extension GL_EXT_samplerless_texture_functions : enable\n"
						"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
						"layout(set = 0, binding = 0) uniform texture2D u_depth_tex;\n"
						"layout(set = 0, binding = 1, std430) readonly buffer MetaBuf { uint meta_words[64]; } u_meta;\n"
						"layout(set = 0, binding = 2, std430) writeonly buffer OutBuf { float fetched_depth; uint magic; } u_out;\n"
						"void main() {\n"
						"	u_out.fetched_depth = texelFetch(u_depth_tex, ivec2(32, 32), 0).r;\n"
						"	u_out.magic = u_meta.meta_words[0];\n"
						"}\n";
				String comp_err;
				RD::ShaderStageSPIRVData cs_stage;
				cs_stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
				cs_stage.spirv = p_rd->shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, String::utf8(kCompGlsl), RD::SHADER_LANGUAGE_GLSL, &comp_err, false);
				if (!cs_stage.spirv.is_empty()) {
					Vector<RD::ShaderStageSPIRVData> stages;
					stages.push_back(cs_stage);
					const RID c_shader = p_rd->shader_create_from_spirv(stages);
					const RID c_pipe = c_shader.is_valid() ? p_rd->compute_pipeline_create(c_shader) : RID();
					const RID out_ssbo = p_rd->storage_buffer_create(16);
					if (c_pipe.is_valid() && out_ssbo.is_valid()) {
						RD::Uniform u0(RD::UNIFORM_TYPE_TEXTURE, 0, depth_tex);
						RD::Uniform u1(RD::UNIFORM_TYPE_STORAGE_BUFFER, 1, meta_buf);
						RD::Uniform u2(RD::UNIFORM_TYPE_STORAGE_BUFFER, 2, out_ssbo);
						Vector<RD::Uniform> uniforms;
						uniforms.push_back(u0);
						uniforms.push_back(u1);
						uniforms.push_back(u2);
						const RID uset = p_rd->uniform_set_create(uniforms, c_shader, 0);
						if (uset.is_valid()) {
							const RD::ComputeListID cl = p_rd->compute_list_begin();
							p_rd->compute_list_bind_compute_pipeline(cl, c_pipe);
							p_rd->compute_list_bind_uniform_set(cl, uset, 0);
							p_rd->compute_list_dispatch(cl, 1, 1, 1);
							p_rd->compute_list_end();
							const Vector<uint8_t> out_bytes = p_rd->buffer_get_data(out_ssbo);
							if (out_bytes.size() >= 8) {
								float fetched = 0.0f;
								uint32_t magic_word = 0;
								memcpy(&fetched, out_bytes.ptr() + 0, 4);
								memcpy(&magic_word, out_bytes.ptr() + 4, 4);
								st->compute_ok = (magic_word == 0x4b303644u && Math::abs(fetched - st->raw_center) <= 1e-6f);
							}
							p_rd->free_rid(uset);
						}
					}
					if (out_ssbo.is_valid()) {
						p_rd->free_rid(out_ssbo);
					}
					if (c_pipe.is_valid()) {
						p_rd->free_rid(c_pipe);
					}
					if (c_shader.is_valid()) {
						p_rd->free_rid(c_shader);
					}
				}
			}
		})).bind(uint64_t(uintptr_t(device)), uint64_t(uintptr_t(&verify))));
		server->sync();

		rt_advance_rd_frame();
		for (uint32_t turn = 0; turn < 64; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_observe_once(req, obs) != OK) {
				break;
			}
			if (obs.report.known_obligations > 0 && obs.report.complete_obligations == obs.report.known_obligations && obs.report.possible_obligations == 0) {
				break;
			}
			rt_advance_rd_frame();
		}

		const Error rel_err = release_request_cleanly(req);
		const bool depth_match = verify.meta_ok && verify.tex_ok && verify.compute_ok && Math::abs(verify.world_z - p_expected_world_z) < 0.08f;
		record_check(p_tag + ".meta_and_depth_reconstruct", depth_match);
		record_check(p_tag + ".request_released", rel_err == OK);
		if (!depth_match) {
			Dictionary d;
			d["meta_ok"] = verify.meta_ok;
			d["tex_ok"] = verify.tex_ok;
			d["compute_ok"] = verify.compute_ok;
			d["raw_center"] = verify.raw_center;
			d["world_z"] = verify.world_z;
			d["expected_z"] = p_expected_world_z;
			diagnostics[p_tag + ".depth_diag"] = d;
		}
		return depth_match && rel_err == OK;
	}

	bool run_scene_depth_v2_viewport_e2e(
			const String &p_label,
			bool p_use_msaa_4x,
			const GDExtensionStorageBackingV1 &p_src_backing) {
		RID scen = server->scenario_create();
		RID cam = server->camera_create();
		server->camera_set_perspective(cam, 90.0f, 0.1f, 100.0f);
		server->camera_set_transform(cam, Transform3D());

		RID vp = server->viewport_create();
		server->viewport_set_size(vp, 64, 64);
		server->viewport_set_active(vp, true);
		server->viewport_set_update_mode(vp, RSE::VIEWPORT_UPDATE_ALWAYS);
		server->viewport_set_scenario(vp, scen);
		server->viewport_attach_camera(vp, cam);
		server->viewport_set_msaa_3d(vp, p_use_msaa_4x ? RSE::VIEWPORT_MSAA_4X : RSE::VIEWPORT_MSAA_DISABLED);

		RID quad_mesh = server->mesh_create();
		{
			Array arr;
			arr.resize(RSE::ARRAY_MAX);
			PackedVector3Array vtx;
			vtx.push_back(Vector3(-15.0f, -15.0f, 0.0f));
			vtx.push_back(Vector3(15.0f, -15.0f, 0.0f));
			vtx.push_back(Vector3(-15.0f, 15.0f, 0.0f));
			vtx.push_back(Vector3(15.0f, 15.0f, 0.0f));
			PackedInt32Array idx;
			idx.push_back(0);
			idx.push_back(1);
			idx.push_back(2);
			idx.push_back(2);
			idx.push_back(1);
			idx.push_back(3);
			arr[RSE::ARRAY_VERTEX] = vtx;
			arr[RSE::ARRAY_INDEX] = idx;
			server->mesh_add_surface_from_arrays(quad_mesh, RSE::PRIMITIVE_TRIANGLES, arr);
		}
		RID opaque_shd = server->shader_create_from_code(
				"shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(0.2, 0.7, 0.3); }");
		RID opaque_mat = server->material_create();
		server->material_set_shader(opaque_mat, opaque_shd);

		RID opaque_inst = server->instance_create();
		server->instance_set_base(opaque_inst, quad_mesh);
		server->instance_set_scenario(opaque_inst, scen);
		server->instance_set_transform(opaque_inst, Transform3D(Basis(), Vector3(0.0f, 0.0f, -5.0f)));
		server->instance_set_surface_override_material(opaque_inst, 0, opaque_mat);

		server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rd_addr) {
			auto *p_rd = reinterpret_cast<RenderingDevice *>(uintptr_t(p_rd_addr));
			if (auto *mats = RendererRD::MaterialStorage::get_singleton()) {
				mats->_update_queued_materials();
			}
			if (p_rd) {
				for (uint32_t f = 0; f < 2; ++f) {
					p_rd->_submit_transfer_workers();
					p_rd->_wait_for_transfer_workers();
					p_rd->swap_buffers(false);
				}
			}
		})).bind(uint64_t(uintptr_t(device))));
		server->sync();

		GDExtensionSceneDepthSnapshotArmV1 arm_in = {};
		arm_in.abi = Enrollment::ABI;
		arm_in.abi.struct_size = sizeof(arm_in);
		arm_in.viewport_rid = vp.get_id();
		arm_in.slab_thickness_m = 0.50f;
		arm_in.silhouette_jump_m = 0.50f;
		const Error arm_err = Error(ports.scene_depth_arm(enrollment.attachment, &arm_in).native_error);
		record_check(p_label + ".arm_v1", arm_err == OK);

		// Frame 0: Opaque-only (-5.0m floor, 0 transparent elements)
		server->draw(false, 1.0 / 60.0);
		server->sync();

		uint64_t f0_gen = 0;
		const bool f0_ok = borrow_and_verify_scene_depth_v2(
				p_label + ".f0_opaque", vp, p_src_backing, -5.0f, !p_use_msaa_4x, f0_gen);

		bool f1_ok = true;
		if (!p_use_msaa_4x) {
			// Frame 1 (T08): Add transparent quad at z = -2.0m with depth_draw_always so post-transparent
			// capture would erroneously read -2.0m; pre-transparent capture MUST still read -5.0m!
			RID alpha_shd = server->shader_create_from_code(
					"shader_type spatial; render_mode unshaded, cull_disabled, blend_mix, depth_draw_always; "
					"void fragment() { ALBEDO = vec3(0.9, 0.1, 0.1); ALPHA = 0.5; }");
			RID alpha_mat = server->material_create();
			server->material_set_shader(alpha_mat, alpha_shd);
			RID alpha_inst = server->instance_create();
			server->instance_set_base(alpha_inst, quad_mesh);
			server->instance_set_scenario(alpha_inst, scen);
			server->instance_set_transform(alpha_inst, Transform3D(Basis(), Vector3(0.0f, 0.0f, -2.0f)));
			server->instance_set_surface_override_material(alpha_inst, 0, alpha_mat);

			server->call_on_render_thread(callable_mp_static(+[]() {
				if (auto *mats = RendererRD::MaterialStorage::get_singleton()) {
					mats->_update_queued_materials();
				}
			}));
			server->sync();

			server->draw(false, 1.0 / 60.0);
			server->sync();

			uint64_t f1_gen = 0;
			f1_ok = borrow_and_verify_scene_depth_v2(
					p_label + ".f1_pre_transparent", vp, p_src_backing, -5.0f, false, f1_gen);
			record_check(p_label + ".f1_gen_advanced", f1_gen > f0_gen);

			server->free_rid(alpha_inst);
			server->free_rid(alpha_mat);
			server->free_rid(alpha_shd);
		}

		server->free_rid(opaque_inst);
		server->free_rid(opaque_mat);
		server->free_rid(opaque_shd);
		server->free_rid(quad_mesh);
		server->free_rid(vp);
		server->free_rid(cam);
		server->free_rid(scen);
		server->sync();
		rt_advance_rd_frame();
		return arm_err == OK && f0_ok && f1_ok;
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

				// 6. Role 9 (GDEXTENSION_OUTPUT_ROLE_AUX_PARAMS_V1 = 9): 4-role PrepareV3/AcquireV2, WithoutPrevious fallback, Guard NaN/subnormal, Color+Motion draw, & 32B zero sentinel!
				run_role9_aux_params_suite(src_cap4, 1u);

				// 7. SceneDepthSnapshotMeta256 v2 pre-transparent capture (MSAA_DISABLED + opaque-only Frame 0 + Frame 1 alpha exclusion) & VIEWPORT_MSAA_4X depth resolve!
				run_scene_depth_v2_viewport_e2e("scene_depth_v2_pre_transparent", false, src_cap2);
				run_scene_depth_v2_viewport_e2e("scene_depth_v2_msaa_4x", true, src_cap2);

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
