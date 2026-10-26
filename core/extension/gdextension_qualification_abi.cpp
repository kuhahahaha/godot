// Adopted PHS1/QB1/MQS1/U1/NUR1 C ABI shape only. No proc availability or native proof is asserted.
#include "gdextension_interface.gen.h"
#include <cstddef>
#include <type_traits>

static_assert(sizeof(GDExtensionDeviceExitSceneReadyContextRef) == 16);
static_assert(alignof(GDExtensionDeviceExitSceneReadyContextRef) == 8);
static_assert(GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 == 2048);
static_assert(std::is_same_v<GDExtensionDeviceExitSceneReadyCallbackV1,
        int32_t (*)(void*, GDExtensionDeviceExitSceneReadyContextRef)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceExitRegisterSceneReadyV1,
        int32_t (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitInitializationLevelContextRef, GDExtensionDeviceExitSceneReadyCallbackV1, void*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1,
        int32_t (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitSceneReadyContextRef, uint32_t, uint64_t)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionBindSceneReadyV1,
        GDExtensionDeviceSessionResultV1 (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitSceneReadyContextRef, GDExtensionObjectPtr, GDExtensionDeviceRegistryContextRef*, GDExtensionOriginalDeviceSessionRef*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceExitSceneReadyAbortV1,
        int32_t (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitSceneReadyContextRef, int32_t, uint32_t, uint64_t)>);

static_assert(sizeof(GDExtensionQualificationBridgeRefV1) == 16);
static_assert(alignof(GDExtensionQualificationBridgeRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationBridgeRefV1> && std::is_trivially_copyable_v<GDExtensionQualificationBridgeRefV1>);
static_assert(offsetof(GDExtensionQualificationBridgeRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionQualificationBridgeRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionQualificationTicketRefV1) == 16);
static_assert(alignof(GDExtensionQualificationTicketRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationTicketRefV1> && std::is_trivially_copyable_v<GDExtensionQualificationTicketRefV1>);
static_assert(offsetof(GDExtensionQualificationTicketRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionQualificationTicketRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionQualificationRecordV1) == 312);
static_assert(alignof(GDExtensionQualificationRecordV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationRecordV1> && std::is_trivially_copyable_v<GDExtensionQualificationRecordV1>);
static_assert(offsetof(GDExtensionQualificationRecordV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationRecordV1, record_generation) == 16);
static_assert(offsetof(GDExtensionQualificationRecordV1, request_nonce) == 24);
static_assert(offsetof(GDExtensionQualificationRecordV1, library_generation) == 32);
static_assert(offsetof(GDExtensionQualificationRecordV1, qualification_session_serial) == 40);
static_assert(offsetof(GDExtensionQualificationRecordV1, range_begin) == 48);
static_assert(offsetof(GDExtensionQualificationRecordV1, range_end) == 56);
static_assert(offsetof(GDExtensionQualificationRecordV1, vector_ordinal) == 64);
static_assert(offsetof(GDExtensionQualificationRecordV1, attempt) == 68);
static_assert(offsetof(GDExtensionQualificationRecordV1, sample_count) == 72);
static_assert(offsetof(GDExtensionQualificationRecordV1, body_groups_x) == 76);
static_assert(offsetof(GDExtensionQualificationRecordV1, request_digest) == 80);
static_assert(offsetof(GDExtensionQualificationRecordV1, unit_digest) == 112);
static_assert(offsetof(GDExtensionQualificationRecordV1, capture_map_digest) == 144);
static_assert(offsetof(GDExtensionQualificationRecordV1, body_map_digest) == 176);
static_assert(offsetof(GDExtensionQualificationRecordV1, capture_pipeline) == 208);
static_assert(offsetof(GDExtensionQualificationRecordV1, capture_set) == 216);
static_assert(offsetof(GDExtensionQualificationRecordV1, body_pipeline) == 224);
static_assert(offsetof(GDExtensionQualificationRecordV1, body_set) == 232);
static_assert(offsetof(GDExtensionQualificationRecordV1, readback_buffer) == 240);
static_assert(offsetof(GDExtensionQualificationRecordV1, source_allocation_generation) == 248);
static_assert(offsetof(GDExtensionQualificationRecordV1, observed_backing_serial) == 256);
static_assert(offsetof(GDExtensionQualificationRecordV1, readback_offset) == 264);
static_assert(offsetof(GDExtensionQualificationRecordV1, readback_size) == 272);
static_assert(offsetof(GDExtensionQualificationRecordV1, capture_push) == 280);
static_assert(offsetof(GDExtensionQualificationRecordV1, body_push) == 296);

static_assert(sizeof(GDExtensionQualificationBackingV1) == 48);
static_assert(alignof(GDExtensionQualificationBackingV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationBackingV1> && std::is_trivially_copyable_v<GDExtensionQualificationBackingV1>);
static_assert(offsetof(GDExtensionQualificationBackingV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationBackingV1, rid) == 16);
static_assert(offsetof(GDExtensionQualificationBackingV1, backing_serial) == 24);
static_assert(offsetof(GDExtensionQualificationBackingV1, byte_size) == 32);
static_assert(offsetof(GDExtensionQualificationBackingV1, usage_bits) == 40);

static_assert(sizeof(GDExtensionQualificationReportV1) == 184);
static_assert(alignof(GDExtensionQualificationReportV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationReportV1> && std::is_trivially_copyable_v<GDExtensionQualificationReportV1>);
static_assert(offsetof(GDExtensionQualificationReportV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationReportV1, state) == 16);
static_assert(offsetof(GDExtensionQualificationReportV1, phase) == 20);
static_assert(offsetof(GDExtensionQualificationReportV1, native_error) == 24);
static_assert(offsetof(GDExtensionQualificationReportV1, issue_fact) == 28);
static_assert(offsetof(GDExtensionQualificationReportV1, native_code) == 32);
static_assert(offsetof(GDExtensionQualificationReportV1, revision) == 40);
static_assert(offsetof(GDExtensionQualificationReportV1, record_generation) == 48);
static_assert(offsetof(GDExtensionQualificationReportV1, request_nonce) == 56);
static_assert(offsetof(GDExtensionQualificationReportV1, required_count) == 64);
static_assert(offsetof(GDExtensionQualificationReportV1, known_count) == 68);
static_assert(offsetof(GDExtensionQualificationReportV1, possible_count) == 72);
static_assert(offsetof(GDExtensionQualificationReportV1, fact_count) == 76);
static_assert(offsetof(GDExtensionQualificationReportV1, readback_request) == 80);
static_assert(offsetof(GDExtensionQualificationReportV1, source_backing_serial) == 88);
static_assert(offsetof(GDExtensionQualificationReportV1, source_offset) == 96);
static_assert(offsetof(GDExtensionQualificationReportV1, source_size) == 104);
static_assert(offsetof(GDExtensionQualificationReportV1, owned_size) == 112);
static_assert(offsetof(GDExtensionQualificationReportV1, reserved) == 116);
static_assert(offsetof(GDExtensionQualificationReportV1, request_digest) == 120);
static_assert(offsetof(GDExtensionQualificationReportV1, unit_digest) == 152);

static_assert(sizeof(GDExtensionQualificationBudgetV1) == 32);
static_assert(alignof(GDExtensionQualificationBudgetV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationBudgetV1> && std::is_trivially_copyable_v<GDExtensionQualificationBudgetV1>);
static_assert(offsetof(GDExtensionQualificationBudgetV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationBudgetV1, max_passes) == 16);
static_assert(offsetof(GDExtensionQualificationBudgetV1, max_micros) == 20);
static_assert(offsetof(GDExtensionQualificationBudgetV1, reserved) == 24);

static_assert(sizeof(GDExtensionQualificationFactV1) == 112);
static_assert(alignof(GDExtensionQualificationFactV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationFactV1> && std::is_trivially_copyable_v<GDExtensionQualificationFactV1>);
static_assert(offsetof(GDExtensionQualificationFactV1, graph_instance) == 0);
static_assert(offsetof(GDExtensionQualificationFactV1, graph_frame) == 8);
static_assert(offsetof(GDExtensionQualificationFactV1, graph_kind) == 16);
static_assert(offsetof(GDExtensionQualificationFactV1, graph_index) == 20);
static_assert(offsetof(GDExtensionQualificationFactV1, graph_offset) == 24);
static_assert(offsetof(GDExtensionQualificationFactV1, semantic_kind) == 28);
static_assert(offsetof(GDExtensionQualificationFactV1, command_slot) == 32);
static_assert(offsetof(GDExtensionQualificationFactV1, backend_queue) == 36);
static_assert(offsetof(GDExtensionQualificationFactV1, phase) == 40);
static_assert(offsetof(GDExtensionQualificationFactV1, issue) == 44);
static_assert(offsetof(GDExtensionQualificationFactV1, completion) == 48);
static_assert(offsetof(GDExtensionQualificationFactV1, error) == 52);
static_assert(offsetof(GDExtensionQualificationFactV1, native_serial) == 56);
static_assert(offsetof(GDExtensionQualificationFactV1, completion_serial) == 64);
static_assert(offsetof(GDExtensionQualificationFactV1, native_code) == 72);
static_assert(offsetof(GDExtensionQualificationFactV1, parent_fact) == 80);
static_assert(offsetof(GDExtensionQualificationFactV1, reserved) == 84);
static_assert(offsetof(GDExtensionQualificationFactV1, source_backing_serial) == 88);
static_assert(offsetof(GDExtensionQualificationFactV1, source_offset) == 96);
static_assert(offsetof(GDExtensionQualificationFactV1, source_size) == 104);

static_assert(sizeof(GDExtensionStorageBackingRefV1) == 16);
static_assert(alignof(GDExtensionStorageBackingRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageBackingRefV1> && std::is_trivially_copyable_v<GDExtensionStorageBackingRefV1>);
static_assert(offsetof(GDExtensionStorageBackingRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionStorageBackingRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionStorageRetireRefV1) == 16);
static_assert(alignof(GDExtensionStorageRetireRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageRetireRefV1> && std::is_trivially_copyable_v<GDExtensionStorageRetireRefV1>);
static_assert(offsetof(GDExtensionStorageRetireRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionStorageRetireRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionStorageCreateRequestV1) == 56);
static_assert(alignof(GDExtensionStorageCreateRequestV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageCreateRequestV1> && std::is_trivially_copyable_v<GDExtensionStorageCreateRequestV1>);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, abi) == 0);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, bytes) == 16);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, allocation_slot) == 24);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, part) == 28);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, allocation_generation) == 32);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, create_attempt) == 40);
static_assert(offsetof(GDExtensionStorageCreateRequestV1, reserved) == 48);

static_assert(sizeof(GDExtensionStorageBackingV1) == 72);
static_assert(alignof(GDExtensionStorageBackingV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageBackingV1> && std::is_trivially_copyable_v<GDExtensionStorageBackingV1>);
static_assert(offsetof(GDExtensionStorageBackingV1, abi) == 0);
static_assert(offsetof(GDExtensionStorageBackingV1, backing) == 16);
static_assert(offsetof(GDExtensionStorageBackingV1, rid) == 32);
static_assert(offsetof(GDExtensionStorageBackingV1, backing_serial) == 40);
static_assert(offsetof(GDExtensionStorageBackingV1, byte_size) == 48);
static_assert(offsetof(GDExtensionStorageBackingV1, usage_bits) == 56);
static_assert(offsetof(GDExtensionStorageBackingV1, native_error) == 64);
static_assert(offsetof(GDExtensionStorageBackingV1, state) == 68);

static_assert(sizeof(GDExtensionStorageRetireRequestV1) == 80);
static_assert(alignof(GDExtensionStorageRetireRequestV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageRetireRequestV1> && std::is_trivially_copyable_v<GDExtensionStorageRetireRequestV1>);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, abi) == 0);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, backing) == 16);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, rid) == 32);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, observed_backing_serial) == 40);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, allocation_slot) == 48);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, part) == 52);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, allocation_generation) == 56);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, retire_attempt) == 64);
static_assert(offsetof(GDExtensionStorageRetireRequestV1, reserved) == 72);

static_assert(sizeof(GDExtensionStorageRetireReportV1) == 120);
static_assert(alignof(GDExtensionStorageRetireReportV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionStorageRetireReportV1> && std::is_trivially_copyable_v<GDExtensionStorageRetireReportV1>);
static_assert(offsetof(GDExtensionStorageRetireReportV1, abi) == 0);
static_assert(offsetof(GDExtensionStorageRetireReportV1, state) == 16);
static_assert(offsetof(GDExtensionStorageRetireReportV1, return_status) == 20);
static_assert(offsetof(GDExtensionStorageRetireReportV1, rid) == 24);
static_assert(offsetof(GDExtensionStorageRetireReportV1, backing_serial) == 32);
static_assert(offsetof(GDExtensionStorageRetireReportV1, retire_cell_serial) == 40);
static_assert(offsetof(GDExtensionStorageRetireReportV1, frame_slot) == 48);
static_assert(offsetof(GDExtensionStorageRetireReportV1, reserved) == 52);
static_assert(offsetof(GDExtensionStorageRetireReportV1, frame_epoch) == 56);
static_assert(offsetof(GDExtensionStorageRetireReportV1, event_seq) == 64);
static_assert(offsetof(GDExtensionStorageRetireReportV1, allocation_generation) == 72);
static_assert(offsetof(GDExtensionStorageRetireReportV1, retire_attempt) == 80);
static_assert(offsetof(GDExtensionStorageRetireReportV1, allocation_slot) == 88);
static_assert(offsetof(GDExtensionStorageRetireReportV1, part) == 92);
static_assert(offsetof(GDExtensionStorageRetireReportV1, dependency_set_serial) == 96);
static_assert(offsetof(GDExtensionStorageRetireReportV1, native_error) == 104);
static_assert(offsetof(GDExtensionStorageRetireReportV1, reason) == 108);
static_assert(offsetof(GDExtensionStorageRetireReportV1, native_code) == 112);

static_assert(sizeof(GDExtensionQualificationUploadPrepareV1) == 152);
static_assert(alignof(GDExtensionQualificationUploadPrepareV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationUploadPrepareV1> && std::is_trivially_copyable_v<GDExtensionQualificationUploadPrepareV1>);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, record_generation) == 16);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, request_nonce) == 24);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, library_generation) == 32);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, qualification_session_serial) == 40);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, parent_serial) == 48);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, reservation_serial) == 56);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, request_digest) == 64);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, unit_digest) == 96);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, vector_ordinal) == 128);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, attempt) == 132);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, upload_count) == 136);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, max_segments) == 140);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, expected_result_bytes) == 144);
static_assert(offsetof(GDExtensionQualificationUploadPrepareV1, reserved) == 148);

static_assert(sizeof(GDExtensionQualificationUploadPlanV1) == 112);
static_assert(alignof(GDExtensionQualificationUploadPlanV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationUploadPlanV1> && std::is_trivially_copyable_v<GDExtensionQualificationUploadPlanV1>);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, upload_index) == 16);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, role) == 20);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, owner_kind) == 24);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, reserved) == 28);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, rid) == 32);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, backing_serial) == 40);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, byte_offset) == 48);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, byte_size) == 56);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, e05_backing) == 64);
static_assert(offsetof(GDExtensionQualificationUploadPlanV1, payload_digest) == 80);

static_assert(sizeof(GDExtensionQualificationUploadRequestV1) == 136);
static_assert(alignof(GDExtensionQualificationUploadRequestV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationUploadRequestV1> && std::is_trivially_copyable_v<GDExtensionQualificationUploadRequestV1>);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, record_generation) == 16);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, parent_serial) == 24);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, rid) == 32);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, backing_serial) == 40);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, byte_offset) == 48);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, byte_size) == 56);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, upload_index) == 64);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, role) == 68);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, owner_kind) == 72);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, reserved) == 76);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, e05_backing) == 80);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, payload_digest) == 96);
static_assert(offsetof(GDExtensionQualificationUploadRequestV1, expected_revision) == 128);

static_assert(sizeof(GDExtensionQualificationUploadSegmentV1) == 88);
static_assert(alignof(GDExtensionQualificationUploadSegmentV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationUploadSegmentV1> && std::is_trivially_copyable_v<GDExtensionQualificationUploadSegmentV1>);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, upload_index) == 16);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, segment_index) == 20);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, state) == 24);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, reserved) == 28);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, byte_begin) == 32);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, byte_end) == 40);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, graph_instance) == 48);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, graph_frame) == 56);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, graph_kind) == 64);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, graph_index) == 68);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, graph_offset) == 72);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, reserved2) == 76);
static_assert(offsetof(GDExtensionQualificationUploadSegmentV1, backing_serial) == 80);

static_assert(sizeof(GDExtensionQualificationUploadReportV1) == 120);
static_assert(alignof(GDExtensionQualificationUploadReportV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationUploadReportV1> && std::is_trivially_copyable_v<GDExtensionQualificationUploadReportV1>);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, abi) == 0);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, state) == 16);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, native_error) == 20);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, revision) == 24);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, record_generation) == 32);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, parent_serial) == 40);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, upload_index) == 48);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, known_count) == 52);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, possible_count) == 56);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, out_written) == 60);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, appended_begin) == 64);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, appended_end) == 72);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, possible_begin) == 80);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, possible_end) == 88);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, unappended_begin) == 96);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, unappended_end) == 104);
static_assert(offsetof(GDExtensionQualificationUploadReportV1, native_code) == 112);

static_assert(sizeof(GDExtensionNativeUnitRefV1) == 16);
static_assert(alignof(GDExtensionNativeUnitRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitRefV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitRefV1>);
static_assert(offsetof(GDExtensionNativeUnitRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionNativeUnitRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionNativeUnitRetireRefV1) == 16);
static_assert(alignof(GDExtensionNativeUnitRetireRefV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitRetireRefV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitRetireRefV1>);
static_assert(offsetof(GDExtensionNativeUnitRetireRefV1, registry_instance) == 0);
static_assert(offsetof(GDExtensionNativeUnitRetireRefV1, handle_serial) == 8);

static_assert(sizeof(GDExtensionNativeUnitCreateRequestV1) == 104);
static_assert(alignof(GDExtensionNativeUnitCreateRequestV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitCreateRequestV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitCreateRequestV1>);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, abi) == 0);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, library_generation) == 16);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, preparation_generation) == 24);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, operation_serial) == 32);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, container_byte_count) == 40);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, record_slot) == 48);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, physical_unit) == 52);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, expected_flags) == 56);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, reserved32) == 60);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, container_digest) == 64);
static_assert(offsetof(GDExtensionNativeUnitCreateRequestV1, reserved64) == 96);

static_assert(sizeof(GDExtensionNativeUnitBackingV1) == 96);
static_assert(alignof(GDExtensionNativeUnitBackingV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitBackingV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitBackingV1>);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, abi) == 0);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, unit) == 16);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, shader_rid) == 32);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, pipeline_rid) == 40);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, shader_serial) == 48);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, pipeline_serial) == 56);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, known_mask) == 64);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, possible_mask) == 68);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, state) == 72);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, effective_flags) == 76);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, native_error) == 80);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, phase) == 84);
static_assert(offsetof(GDExtensionNativeUnitBackingV1, event_seq) == 88);

static_assert(sizeof(GDExtensionNativeUnitRetireRequestV1) == 48);
static_assert(alignof(GDExtensionNativeUnitRetireRequestV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitRetireRequestV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitRetireRequestV1>);
static_assert(offsetof(GDExtensionNativeUnitRetireRequestV1, abi) == 0);
static_assert(offsetof(GDExtensionNativeUnitRetireRequestV1, unit) == 16);
static_assert(offsetof(GDExtensionNativeUnitRetireRequestV1, retire_operation) == 32);
static_assert(offsetof(GDExtensionNativeUnitRetireRequestV1, reserved) == 40);

static_assert(sizeof(GDExtensionNativeUnitRetireReportV1) == 168);
static_assert(alignof(GDExtensionNativeUnitRetireReportV1) == 8);
static_assert(std::is_standard_layout_v<GDExtensionNativeUnitRetireReportV1> && std::is_trivially_copyable_v<GDExtensionNativeUnitRetireReportV1>);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, abi) == 0);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, unit) == 16);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, state) == 32);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, return_status) == 36);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, enqueued_mask) == 40);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, freed_mask) == 44);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, absent_mask) == 48);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, unresolved_mask) == 52);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, shader_frame_slot) == 56);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, pipeline_frame_slot) == 60);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, shader_rid) == 64);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, pipeline_rid) == 72);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, shader_serial) == 80);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, pipeline_serial) == 88);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, shader_frame_epoch) == 96);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, pipeline_frame_epoch) == 104);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, shader_free_event_seq) == 112);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, pipeline_free_event_seq) == 120);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, dependency_set_serial) == 128);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, report_revision) == 136);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, native_error) == 144);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, reason) == 148);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, native_code) == 152);
static_assert(offsetof(GDExtensionNativeUnitRetireReportV1, retire_operation) == 160);

static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionBindMainSceneReadyV1,
        GDExtensionDeviceSessionResultV1 (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitSceneReadyContextRef, GDExtensionDeviceRegistryContextRef*, GDExtensionOriginalDeviceSessionRef*, uint64_t*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationOpenV1,
        GDExtensionDeviceSessionResultV1 (*)(const GDExtensionDeviceExitAbi*, GDExtensionDeviceExitSceneReadyContextRef, GDExtensionDeviceRegistryContextRef, GDExtensionOriginalDeviceSessionRef, GDExtensionDeviceExitLibraryCodeHoldRef, GDExtensionQualificationBridgeRefV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationBackingV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, uint64_t, GDExtensionQualificationBackingV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRecordV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationRecordV1*, GDExtensionQualificationTicketRefV1*, GDExtensionQualificationReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationObserveV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1, const GDExtensionQualificationBudgetV1*, GDExtensionQualificationReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationFactsV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1, uint64_t, uint32_t, uint32_t, GDExtensionQualificationFactV1*, uint32_t*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationCopyV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1, uint64_t, void*, uint32_t, uint32_t*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationReleaseV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationCloseV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionStorageCreateRequestV1*, GDExtensionStorageBackingV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionStorageRetireRequestV1*, GDExtensionStorageRetireRefV1*, GDExtensionStorageRetireReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionStorageRetireRefV1, const GDExtensionQualificationBudgetV1*, GDExtensionStorageRetireReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionStorageRetireRefV1)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationUploadPrepareV1*, const GDExtensionQualificationUploadPlanV1*, uint32_t, GDExtensionQualificationTicketRefV1*, GDExtensionQualificationReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationUploadV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1, const GDExtensionQualificationUploadRequestV1*, const void*, uint32_t, GDExtensionQualificationUploadSegmentV1*, uint32_t, GDExtensionQualificationUploadReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1, const GDExtensionQualificationRecordV1*, GDExtensionQualificationReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionNativeUnitCreateRequestV1*, const void*, uint64_t, GDExtensionNativeUnitBackingV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, const GDExtensionNativeUnitRetireRequestV1*, GDExtensionNativeUnitRetireRefV1*, GDExtensionNativeUnitRetireReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionNativeUnitRetireRefV1, const GDExtensionQualificationBudgetV1*, GDExtensionNativeUnitRetireReportV1*)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1,
        GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1, GDExtensionNativeUnitRetireRefV1)>);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_WRONG_PHASE_V1 == 14);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_WRONG_THREAD_V1 == 15);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_NOT_ADMITTED_V1 == 16);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_STALE_TICKET_V1 == 17);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1 == 18);
static_assert(GDEXTENSION_DEVICE_SESSION_REASON_CAPACITY_V1 == 19);

// U1-OC1 adds one proc and symbolic domains; original uint32_t POD members stay.
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_DOMAIN_COUNT_V1==0);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_FAULT_PLANE_V1==1);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_INPUT_GATE_V1==2);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_INSTANCE_GATE_V1==3);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_MAPPING_V1==4);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_OUTPUT_GATE_V1==5);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_INPUT_V1==6);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_OUTPUT_V1==7);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_COUNT_V1==8);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_INVALID_V1==0);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1==1);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1==2);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E22_V1==3);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E23_V1==4);
static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E24_V1==5);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationCancelV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1)>);

// QRT1 fixed carrier; original PODs and ticket payload prefix remain unchanged.
static_assert(sizeof(GDExtensionQualificationRtConfigV1)==40 && alignof(GDExtensionQualificationRtConfigV1)==8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationRtConfigV1> && std::is_trivially_copyable_v<GDExtensionQualificationRtConfigV1>);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,abi)==0);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,slot_count)==16);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,max_request_bytes)==20);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,max_result_bytes)==24);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,max_segment_count)==28);
static_assert(offsetof(GDExtensionQualificationRtConfigV1,max_payload_bytes)==32);
static_assert(sizeof(GDExtensionQualificationRtRequestV1)==56 && alignof(GDExtensionQualificationRtRequestV1)==8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationRtRequestV1> && std::is_trivially_copyable_v<GDExtensionQualificationRtRequestV1>);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,abi)==0);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,kind)==16);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,flags)==20);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,operation_serial)==24);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,payload_bytes)==32);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,request_bytes)==40);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,result_capacity)==44);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,segment_capacity)==48);
static_assert(offsetof(GDExtensionQualificationRtRequestV1,reserved)==52);
static_assert(sizeof(GDExtensionQualificationRtRefV1)==16 && alignof(GDExtensionQualificationRtRefV1)==8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationRtRefV1> && std::is_trivially_copyable_v<GDExtensionQualificationRtRefV1>);
static_assert(offsetof(GDExtensionQualificationRtRefV1,registry_instance)==0);
static_assert(offsetof(GDExtensionQualificationRtRefV1,handle_serial)==8);
static_assert(sizeof(GDExtensionQualificationRtObservationV1)==72 && alignof(GDExtensionQualificationRtObservationV1)==8);
static_assert(std::is_standard_layout_v<GDExtensionQualificationRtObservationV1> && std::is_trivially_copyable_v<GDExtensionQualificationRtObservationV1>);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,abi)==0);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,kind)==16);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,state)==20);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,operation_serial)==24);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,slot_generation)==32);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,result_bytes)==40);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,segment_count)==44);
static_assert(offsetof(GDExtensionQualificationRtObservationV1,call_result)==48);
static_assert(sizeof(GDExtensionStorageRetireRefV1)+sizeof(GDExtensionStorageRetireReportV1)==136);
static_assert(sizeof(GDExtensionNativeUnitRetireRefV1)+sizeof(GDExtensionNativeUnitRetireReportV1)==184);
static_assert(sizeof(GDExtensionQualificationTicketRefV1)+sizeof(GDExtensionQualificationReportV1)==200);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationRtConfigV1 *)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRtReserveV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationRtRequestV1 *,const void *,const void *,GDExtensionQualificationRtRefV1 *)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRtActivateV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRtObserveV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1,GDExtensionQualificationRtObservationV1 *,void *,uint32_t,GDExtensionQualificationUploadSegmentV1 *,uint32_t)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1,GDExtensionDeviceSessionResultV1 (*)(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1)>);

// Scene installation V2 preserves the original platform-native Abi record.
static_assert(sizeof(GDExtensionSceneInstallRequirementsV2) == 32, "GDExtensionSceneInstallRequirementsV2 size");
static_assert(alignof(GDExtensionSceneInstallRequirementsV2) == alignof(GDExtensionDeviceExitAbi), "record ABI alignment");
static_assert(offsetof(GDExtensionSceneInstallRequirementsV2, abi) == 0, "GDExtensionSceneInstallRequirementsV2.abi");
static_assert(offsetof(GDExtensionSceneInstallRequirementsV2, required_features) == 16, "GDExtensionSceneInstallRequirementsV2.required_features");
static_assert(offsetof(GDExtensionSceneInstallRequirementsV2, optional_features) == 20, "GDExtensionSceneInstallRequirementsV2.optional_features");
static_assert(offsetof(GDExtensionSceneInstallRequirementsV2, reserved) == 24, "GDExtensionSceneInstallRequirementsV2.reserved");
static_assert(sizeof(GDExtensionSceneInstallPlanV2) == 96, "GDExtensionSceneInstallPlanV2 size");
static_assert(alignof(GDExtensionSceneInstallPlanV2) == alignof(GDExtensionDeviceExitAbi), "record ABI alignment");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, abi) == 0, "GDExtensionSceneInstallPlanV2.abi");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, library_generation) == 16, "GDExtensionSceneInstallPlanV2.library_generation");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, required_features) == 24, "GDExtensionSceneInstallPlanV2.required_features");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, optional_features) == 28, "GDExtensionSceneInstallPlanV2.optional_features");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, selected_features) == 32, "GDExtensionSceneInstallPlanV2.selected_features");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, omitted_optional_features) == 36, "GDExtensionSceneInstallPlanV2.omitted_optional_features");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, omission_reason) == 40, "GDExtensionSceneInstallPlanV2.omission_reason");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, reserved) == 44, "GDExtensionSceneInstallPlanV2.reserved");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, tree_id) == 48, "GDExtensionSceneInstallPlanV2.tree_id");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, window_id) == 56, "GDExtensionSceneInstallPlanV2.window_id");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, world_id) == 64, "GDExtensionSceneInstallPlanV2.world_id");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, rendering_server_id) == 72, "GDExtensionSceneInstallPlanV2.rendering_server_id");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, main_rd_id) == 80, "GDExtensionSceneInstallPlanV2.main_rd_id");
static_assert(offsetof(GDExtensionSceneInstallPlanV2, main_rd_epoch) == 88, "GDExtensionSceneInstallPlanV2.main_rd_epoch");
static_assert(std::is_same_v<GDExtensionDeviceExitSceneReadyCallbackV2, int32_t (*)(void *, GDExtensionDeviceExitSceneReadyContextRef, const GDExtensionSceneInstallPlanV2 *)>);
static_assert(std::is_same_v<GDExtensionInterfaceDeviceExitRegisterSceneReadyV2, int32_t (*)(const GDExtensionSceneInstallRequirementsV2 *, GDExtensionDeviceExitInitializationLevelContextRef, GDExtensionDeviceExitSceneReadyCallbackV2, void *)>);
