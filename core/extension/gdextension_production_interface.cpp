/**************************************************************************/
/*  gdextension_production_interface.cpp                                  */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_external_output_interface.h"
#include "core/extension/gdextension_production_request.h"

#include <cstring>
#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
struct SceneMemoryRange {
	uintptr_t begin = 0, end = 0;
};
bool scene_object_range(const void *data, size_t bytes, size_t alignment, SceneMemoryRange &range) {
	const uintptr_t begin = reinterpret_cast<uintptr_t>(data);
	if (!begin || begin % alignment || bytes > UINTPTR_MAX - begin) {
		return false;
	}
	range = { begin, begin + bytes };
	return true;
}
template <class T>
bool scene_object_range(const T *value, SceneMemoryRange &range) {
	return scene_object_range(value, sizeof(T), alignof(T), range);
}
bool scene_ranges_overlap(const SceneMemoryRange &a, const SceneMemoryRange &b) {
	return a.begin < b.end && b.begin < a.end;
}
} // namespace
GDExtensionDeviceSessionResultV1 device_session_scene_operation_query_v1(GDExtensionProductionRequestRefV1 request, uint64_t ordinal, const GDExtensionSceneOperationKeyV1 *key, GDExtensionSceneReadOutcomeV1 *out) {
	SceneMemoryRange input, output;
	if (!scene_object_range(key, input) || !scene_object_range(out, output) || scene_ranges_overlap(input, output)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	return { Registry::get_singleton().scene_phase_query_operation(request, ordinal, *key, *out), 0, 0, 0 };
}
GDExtensionDeviceSessionResultV1 device_session_scene_operation_bind_v1(GDExtensionProductionRequestRefV1 request, uint64_t ordinal, const GDExtensionSceneOperationKeyV1 *key) {
	SceneMemoryRange input;
	if (!scene_object_range(key, input)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	return { Registry::get_singleton().scene_phase_bind_operation(request, ordinal, *key), 0, 0, 0 };
}
GDExtensionDeviceSessionResultV1 device_session_scene_phase_capability_v1(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionSceneOpaque32V1 *out) {
	SceneMemoryRange output;
	if (!scene_object_range(out, output)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	return { Registry::get_singleton().scene_phase_capability(attachment, *out), 0, 0, 0 };
}
GDExtensionDeviceSessionResultV1 device_session_scene_phase_relay_v1(GDExtensionConstTypePtr callable, GDExtensionScenePhaseReceiverV1 receiver, GDExtensionNormalRelayRefV1 *out) {
	SceneMemoryRange input, output;
	if (!receiver || !scene_object_range(callable, sizeof(Callable), alignof(Callable), input) || !scene_object_range(out, output) || scene_ranges_overlap(input, output)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	const GDExtensionNormalRelayRefV1 empty{};
	if (memcmp(out, &empty, sizeof(empty))) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	NormalRelayToken token;
	const Error error = Registry::get_singleton().scene_phase_install_relay(*reinterpret_cast<const Callable *>(callable), receiver, token);
	if (error == OK) {
		return { Registry::get_singleton().scene_phase_encode_relay(token, *out), 0, 0, 0 };
	}
	return { error, 0, 0, 0 };
}
GDExtensionDeviceSessionResultV1 device_session_scene_phase_register_v1(GDExtensionNormalRelayRefV1 relay, GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionScenePhaseSinkConfigV1 *config, GDExtensionScenePhaseRegistrationRefV1 *out) {
	SceneMemoryRange input, output;
	if (!scene_object_range(config, input) || !scene_object_range(out, output) || scene_ranges_overlap(input, output)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	return { Registry::get_singleton().scene_phase_register(relay, attachment, *config, *out), 0, 0, 0 };
}
GDExtensionDeviceSessionResultV1 device_session_scene_phase_unregister_v1(GDExtensionScenePhaseRegistrationRefV1 registration, GDExtensionScenePhaseCloseV1 *out) {
	SceneMemoryRange output;
	if (!scene_object_range(out, output)) {
		return { ERR_INVALID_PARAMETER, 0, 0, 0 };
	}
	return { Registry::get_singleton().scene_phase_unregister(registration, *out), 0, 0, 0 };
}

static_assert(std::is_same_v<decltype(&device_session_scene_operation_query_v1), GDExtensionInterfaceDeviceSessionSceneOperationQueryV1>);
static_assert(std::is_same_v<decltype(&device_session_scene_operation_bind_v1), GDExtensionInterfaceDeviceSessionSceneOperationBindV1>);
static_assert(std::is_same_v<decltype(&device_session_scene_phase_capability_v1), GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1>);
static_assert(std::is_same_v<decltype(&device_session_scene_phase_relay_v1), GDExtensionInterfaceDeviceSessionScenePhaseRelayV1>);
static_assert(std::is_same_v<decltype(&device_session_scene_phase_register_v1), GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1>);
static_assert(std::is_same_v<decltype(&device_session_scene_phase_unregister_v1), GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1>);

namespace {
static_assert(std::is_standard_layout_v<GDExtensionProductionCapacityV1> && std::is_trivially_copyable_v<GDExtensionProductionCapacityV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionRasterActionV1> && std::is_trivially_copyable_v<GDExtensionProductionRasterActionV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionMetadataQuoteV1> && std::is_trivially_copyable_v<GDExtensionProductionMetadataQuoteV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionInstallationReportV1> && std::is_trivially_copyable_v<GDExtensionProductionInstallationReportV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionOperationV1> && std::is_trivially_copyable_v<GDExtensionProductionOperationV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionSetBindingV1> && std::is_trivially_copyable_v<GDExtensionProductionSetBindingV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionDispatchV1> && std::is_trivially_copyable_v<GDExtensionProductionDispatchV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionCopyV1> && std::is_trivially_copyable_v<GDExtensionProductionCopyV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionUploadV1> && std::is_trivially_copyable_v<GDExtensionProductionUploadV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionAppendV1> && std::is_trivially_copyable_v<GDExtensionProductionAppendV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionReportV1> && std::is_trivially_copyable_v<GDExtensionProductionReportV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionSegmentV1> && std::is_trivially_copyable_v<GDExtensionProductionSegmentV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionServiceBudgetV1> && std::is_trivially_copyable_v<GDExtensionProductionServiceBudgetV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionRecordReportV1> && std::is_trivially_copyable_v<GDExtensionProductionRecordReportV1>);
static_assert(std::is_standard_layout_v<GDExtensionProductionPhysicalContextRefV1> && std::is_trivially_copyable_v<GDExtensionProductionPhysicalContextRefV1>);
GDExtensionDeviceSessionResultV1 device_session_production_quote_v1(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef device_context, GDExtensionOriginalDeviceSessionRef device_session, const GDExtensionProductionCapacityV1 *capacity, GDExtensionProductionMetadataQuoteV1 *out_quote) {
	return Registry::get_singleton().production_quote(abi, scene_ready, producer, device_context, device_session, capacity, out_quote);
}
static_assert(std::is_same_v<decltype(&device_session_production_quote_v1), GDExtensionInterfaceDeviceSessionProductionQuoteV1>);
GDExtensionDeviceSessionResultV1 device_session_production_accept_v1(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef device_context, GDExtensionOriginalDeviceSessionRef device_session, GDExtensionDeviceExitLibraryCodeHoldRef code_hold, const GDExtensionProductionMetadataQuoteV1 *quote, GDExtensionProductionAttachmentRefV1 *out_attachment, GDExtensionProductionInstallationReportV1 *out_report) {
	return Registry::get_singleton().production_accept(abi, scene_ready, producer, device_context, device_session, code_hold, quote, out_attachment, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_accept_v1), GDExtensionInterfaceDeviceSessionProductionAcceptV1>);
int32_t device_session_production_install_record_receiver_v1(GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProductionAttachmentRefV1 attachment, GDExtensionProductionRecordReceiverV1 record_receiver, void *userdata, GDExtensionProductionMetadataReceiverV1 metadata_receiver) {
	if (!metadata_receiver) {
		return ERR_INVALID_PARAMETER; // Both protected RT and late owner return lanes are installed together.
	}
	return int32_t(Registry::get_singleton().production_install_record_receiver(scene_ready, attachment, record_receiver, userdata, metadata_receiver));
}
static_assert(std::is_same_v<decltype(&device_session_production_install_record_receiver_v1), GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1>);
GDExtensionDeviceSessionResultV1 device_session_production_reserve_v1(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV1 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV1 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request) {
	return Registry::get_singleton().production_reserve(attachment, operation, capacity, actions, action_count, resources, resource_count, uses, use_count, readbacks, readback_count, out_request);
}
static_assert(std::is_same_v<decltype(&device_session_production_reserve_v1), GDExtensionInterfaceDeviceSessionProductionReserveV1>);
GDExtensionDeviceSessionResultV1 device_session_production_raster_reserve_v1(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionRasterActionV1 *action, GDExtensionProductionRequestRefV1 *out_request) {
	return Registry::get_singleton().production_raster_reserve(attachment, operation, capacity, action, out_request);
}
static_assert(std::is_same_v<decltype(&device_session_production_raster_reserve_v1), GDExtensionInterfaceDeviceSessionProductionRasterReserveV1>);
GDExtensionDeviceSessionResultV1 device_session_production_raster_prepare_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionRasterPrepareV1 *prepare, const void *shader_binary, uint64_t bytes, GDExtensionProductionRasterPrepareReportV1 *out_report) {
	return Registry::get_singleton().production_raster_prepare(request, prepare, shader_binary, bytes, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_raster_prepare_v1), GDExtensionInterfaceDeviceSessionProductionRasterPrepareV1>);
GDExtensionDeviceSessionResultV1 device_session_production_raster_prepare_report_v1(GDExtensionProductionRequestRefV1 request, GDExtensionProductionRasterPrepareReportV1 *out_report) {
	return Registry::get_singleton().production_raster_prepare_report(request, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_raster_prepare_report_v1), GDExtensionInterfaceDeviceSessionProductionRasterPrepareReportV1>);
GDExtensionDeviceSessionResultV1 device_session_production_raster_begin_v1(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, uint64_t revision) {
	return Registry::get_singleton().production_raster_begin(context, request, revision);
}
static_assert(std::is_same_v<decltype(&device_session_production_raster_begin_v1), GDExtensionInterfaceDeviceSessionProductionRasterBeginV1>);

GDExtensionDeviceSessionResultV1 device_session_production_raster_end_v1(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request) {
	return Registry::get_singleton().production_raster_end(context, request);
}
static_assert(std::is_same_v<decltype(&device_session_production_raster_end_v1), GDExtensionInterfaceDeviceSessionProductionRasterEndV1>);

GDExtensionDeviceSessionResultV1 device_session_production_record_history_v1(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordHistoryV1 *out_history) {
	return Registry::get_singleton().production_record_history(request, control_serial, out_history);
}
static_assert(std::is_same_v<decltype(&device_session_production_record_history_v1), GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1>);

int32_t device_session_production_record_schedule_v1(GDExtensionProductionRequestRefV1 request, uint64_t *out_control_serial) {
	return int32_t(Registry::get_singleton().production_record_schedule(request, out_control_serial));
}
static_assert(std::is_same_v<decltype(&device_session_production_record_schedule_v1), GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>);
GDExtensionDeviceSessionResultV1 device_session_production_record_consume_v1(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordReportV1 *out_report) {
	return Registry::get_singleton().production_record_consume(request, control_serial, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_record_consume_v1), GDExtensionInterfaceDeviceSessionProductionRecordConsumeV1>);
GDExtensionDeviceSessionResultV1 device_session_production_append_v1(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, uint64_t canonical_suboperation, uint64_t revision, GDExtensionProductionAppendV1 *out_append) {
	return Registry::get_singleton().production_append(physical_context, request, canonical_suboperation, revision, out_append);
}
static_assert(std::is_same_v<decltype(&device_session_production_append_v1), GDExtensionInterfaceDeviceSessionProductionAppendV1>);
GDExtensionDeviceSessionResultV1 device_session_production_close_segment_v1(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, GDExtensionProductionSegmentV1 *out_segment) {
	return Registry::get_singleton().production_close_segment(physical_context, request, out_segment);
}
static_assert(std::is_same_v<decltype(&device_session_production_close_segment_v1), GDExtensionInterfaceDeviceSessionProductionCloseSegmentV1>);
GDExtensionDeviceSessionResultV1 device_session_production_ack_segment_v1(GDExtensionProductionRequestRefV1 request, uint64_t segment, uint64_t segment_revision) {
	return Registry::get_singleton().production_ack_segment(request, segment, segment_revision);
}
static_assert(std::is_same_v<decltype(&device_session_production_ack_segment_v1), GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>);
GDExtensionDeviceSessionResultV1 device_session_production_open_v1(GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProductionAttachmentRefV1 attachment) {
	return Registry::get_singleton().production_open(scene_ready, attachment);
}
static_assert(std::is_same_v<decltype(&device_session_production_open_v1), GDExtensionInterfaceDeviceSessionProductionOpenV1>);
GDExtensionDeviceSessionResultV1 device_session_production_set_create_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, uint32_t unit_slot, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t binding_count) {
	return Registry::get_singleton().production_set_create(request, resource_slot, unit_slot, shader_set, bindings, binding_count);
}
static_assert(std::is_same_v<decltype(&device_session_production_set_create_v1), GDExtensionInterfaceDeviceSessionProductionSetCreateV1>);
GDExtensionDeviceSessionResultV1 device_session_production_storage_create_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageCreateRequestV1 *origin, GDExtensionStorageBackingV1 *out_backing) {
	return Registry::get_singleton().production_storage_create(request, resource_slot, origin, out_backing);
}
static_assert(std::is_same_v<decltype(&device_session_production_storage_create_v1), GDExtensionInterfaceDeviceSessionProductionStorageCreateV1>);
GDExtensionDeviceSessionResultV1 device_session_production_storage_retire_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageRetireRequestV1 *origin, GDExtensionStorageRetireRefV1 *out_retire, GDExtensionStorageRetireReportV1 *out_report) {
	return Registry::get_singleton().production_storage_retire(request, resource_slot, origin, out_retire, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_storage_retire_v1), GDExtensionInterfaceDeviceSessionProductionStorageRetireV1>);
GDExtensionDeviceSessionResultV1 device_session_production_storage_observe_v1(GDExtensionProductionRequestRefV1 request, GDExtensionStorageRetireRefV1 retire, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionStorageRetireReportV1 *out_report) {
	return Registry::get_singleton().production_storage_observe(request, retire, budget, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_storage_observe_v1), GDExtensionInterfaceDeviceSessionProductionStorageObserveV1>);
GDExtensionDeviceSessionResultV1 device_session_production_storage_release_v1(GDExtensionProductionRequestRefV1 request, GDExtensionStorageRetireRefV1 retire) {
	return Registry::get_singleton().production_storage_release(request, retire);
}
static_assert(std::is_same_v<decltype(&device_session_production_storage_release_v1), GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1>);
GDExtensionDeviceSessionResultV1 device_session_production_import_unit_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionNativeUnitBackingV1 *backing) {
	return Registry::get_singleton().production_import_unit(request, resource_slot, backing);
}
static_assert(std::is_same_v<decltype(&device_session_production_import_unit_v1), GDExtensionInterfaceDeviceSessionProductionImportUnitV1>);
GDExtensionDeviceSessionResultV1 device_session_production_import_storage_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageBackingV1 *backing) {
	return Registry::get_singleton().production_import_storage(request, resource_slot, backing);
}
static_assert(std::is_same_v<decltype(&device_session_production_import_storage_v1), GDExtensionInterfaceDeviceSessionProductionImportStorageV1>);
GDExtensionDeviceSessionResultV1 device_session_production_import_set_v1(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionQualificationResourceBackingV1 *backing, uint32_t unit_slot, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t binding_count) {
	return Registry::get_singleton().production_import_set(request, resource_slot, backing, unit_slot, shader_set, bindings, binding_count);
}
static_assert(std::is_same_v<decltype(&device_session_production_import_set_v1), GDExtensionInterfaceDeviceSessionProductionImportSetV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_dispatch_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionDispatchV1 *dispatch, const void *push, uint32_t push_bytes) {
	return Registry::get_singleton().production_prepare_dispatch(request, dispatch, push, push_bytes);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_dispatch_v1), GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_copy_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionCopyV1 *copy) {
	return Registry::get_singleton().production_prepare_copy(request, copy);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_copy_v1), GDExtensionInterfaceDeviceSessionProductionPrepareCopyV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_upload_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionUploadV1 *upload, const void *payload, uint64_t payload_bytes) {
	return Registry::get_singleton().production_prepare_upload(request, upload, payload, payload_bytes);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_upload_v1), GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_readback_v1(GDExtensionProductionRequestRefV1 request, uint64_t canonical_suboperation, uint32_t readback_slot) {
	return Registry::get_singleton().production_prepare_readback(request, canonical_suboperation, readback_slot);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_readback_v1), GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV1>);
GDExtensionDeviceSessionResultV1 device_session_production_observe_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV1 *out_report) {
	return Registry::get_singleton().production_observe(request, budget, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_observe_v1), GDExtensionInterfaceDeviceSessionProductionObserveV1>);
GDExtensionDeviceSessionResultV1 device_session_production_facts_v1(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t first, GDExtensionProductionFactV1 *out_facts, uint32_t fact_capacity, uint32_t *out_written) {
	return Registry::get_singleton().production_facts(request, revision, first, out_facts, fact_capacity, out_written);
}
static_assert(std::is_same_v<decltype(&device_session_production_facts_v1), GDExtensionInterfaceDeviceSessionProductionFactsV1>);
GDExtensionDeviceSessionResultV1 device_session_production_copy_v1(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t readback_slot, void *destination, uint64_t destination_capacity, uint64_t *out_written) {
	return Registry::get_singleton().production_copy(request, revision, readback_slot, destination, destination_capacity, out_written);
}
static_assert(std::is_same_v<decltype(&device_session_production_copy_v1), GDExtensionInterfaceDeviceSessionProductionCopyV1>);
GDExtensionDeviceSessionResultV1 device_session_production_cancel_v1(GDExtensionProductionRequestRefV1 request) {
	return Registry::get_singleton().production_cancel(request);
}
static_assert(std::is_same_v<decltype(&device_session_production_cancel_v1), GDExtensionInterfaceDeviceSessionProductionCancelV1>);
GDExtensionDeviceSessionResultV1 device_session_production_release_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *out_remaining, GDExtensionProductionReportV1 *out_report) {
	return Registry::get_singleton().production_release(request, input, out_remaining, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_release_v1), GDExtensionInterfaceDeviceSessionProductionReleaseV1>);
GDExtensionDeviceSessionResultV1 device_session_production_close_v1(GDExtensionProductionAttachmentRefV1 attachment) {
	return Registry::get_singleton().production_close(attachment);
}
static_assert(std::is_same_v<decltype(&device_session_production_close_v1), GDExtensionInterfaceDeviceSessionProductionCloseV1>);
GDExtensionDeviceSessionResultV1 device_session_production_installation_observe_v1(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionInstallationReportV1 *out_report) {
	return Registry::get_singleton().production_installation_observe(attachment, budget, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_installation_observe_v1), GDExtensionInterfaceDeviceSessionProductionInstallationObserveV1>);
GDExtensionDeviceSessionResultV1 device_session_production_reserve_v2(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV2 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request) {
	return Registry::get_singleton().production_reserve_v2(attachment, operation, capacity, actions, action_count, resources, resource_count, uses, use_count, readbacks, readback_count, out_request);
}
static_assert(std::is_same_v<decltype(&device_session_production_reserve_v2), GDExtensionInterfaceDeviceSessionProductionReserveV2>);
GDExtensionDeviceSessionResultV1 device_session_production_reserve_readback_sources_v1(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request) {
	return Registry::get_singleton().production_reserve_readback_sources_v1(attachment, operation, capacity, actions, action_count, resources, resource_count, uses, use_count, readbacks, readback_count, out_request);
}
static_assert(std::is_same_v<decltype(&device_session_production_reserve_readback_sources_v1), GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_output_v1(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPrepareV1 *prepare) {
	return Registry::get_singleton().production_prepare_output(request, prepare);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_output_v1), GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_dispatch_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionDispatchV2 *dispatch, const void *push, uint32_t push_bytes) {
	return Registry::get_singleton().production_prepare_dispatch_v2(request, dispatch, push, push_bytes);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_dispatch_v2), GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_copy_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionCopyV2 *copy) {
	return Registry::get_singleton().production_prepare_copy_v2(request, copy);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_copy_v2), GDExtensionInterfaceDeviceSessionProductionPrepareCopyV2>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_upload_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionUploadV2 *upload, const void *payload, uint64_t payload_bytes) {
	return Registry::get_singleton().production_prepare_upload_v2(request, upload, payload, payload_bytes);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_upload_v2), GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>);
GDExtensionDeviceSessionResultV1 device_session_production_prepare_readback_v2(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint32_t readback_slot) {
	return Registry::get_singleton().production_prepare_readback_v2(request, action_ordinal, readback_slot);
}
static_assert(std::is_same_v<decltype(&device_session_production_prepare_readback_v2), GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>);
GDExtensionDeviceSessionResultV1 device_session_production_append_v2(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint64_t revision, GDExtensionProductionAppendV2 *out_append) {
	return Registry::get_singleton().production_append_v2(physical_context, request, action_ordinal, revision, out_append);
}
static_assert(std::is_same_v<decltype(&device_session_production_append_v2), GDExtensionInterfaceDeviceSessionProductionAppendV2>);
GDExtensionDeviceSessionResultV1 device_session_production_close_segment_v2(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, GDExtensionProductionSegmentV2 *out_segment) {
	return Registry::get_singleton().production_close_segment_v2(physical_context, request, out_segment);
}
static_assert(std::is_same_v<decltype(&device_session_production_close_segment_v2), GDExtensionInterfaceDeviceSessionProductionCloseSegmentV2>);
GDExtensionDeviceSessionResultV1 device_session_production_record_consume_v2(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordReportV2 *out_report) {
	return Registry::get_singleton().production_record_consume_v2(request, control_serial, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_record_consume_v2), GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>);
GDExtensionDeviceSessionResultV1 device_session_production_observe_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV2 *out_report) {
	return Registry::get_singleton().production_observe_v2(request, budget, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_observe_v2), GDExtensionInterfaceDeviceSessionProductionObserveV2>);
GDExtensionDeviceSessionResultV1 device_session_production_facts_v2(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t first, GDExtensionProductionFactV2 *out_facts, uint32_t fact_capacity, uint32_t *out_written) {
	return Registry::get_singleton().production_facts_v2(request, revision, first, out_facts, fact_capacity, out_written);
}
static_assert(std::is_same_v<decltype(&device_session_production_facts_v2), GDExtensionInterfaceDeviceSessionProductionFactsV2>);
GDExtensionDeviceSessionResultV1 device_session_production_release_v2(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *out_remaining, GDExtensionProductionReportV2 *out_report) {
	return Registry::get_singleton().production_release_v2(request, input, out_remaining, out_report);
}
static_assert(std::is_same_v<decltype(&device_session_production_release_v2), GDExtensionInterfaceDeviceSessionProductionReleaseV2>);
} // namespace

// Installed by the original device-session aggregator after fixed B source review.
// This publishes checked physical mechanics, not Runtime logical authority.
void register_production_interfaces() {
	static_assert(std::is_same_v<decltype(&device_session_output_create_v1), GDExtensionInterfaceDeviceSessionOutputCreateV1>);
	GDExtension::register_interface_function("device_session_output_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_create_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_metadata_query_v1), GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>);
	GDExtension::register_interface_function("device_session_output_metadata_query_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_metadata_query_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_create_v3), GDExtensionInterfaceDeviceSessionOutputCreateV3>);
	GDExtension::register_interface_function("device_session_output_create_v3", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_create_v3));
	static_assert(std::is_same_v<decltype(&device_session_output_create_v2), GDExtensionInterfaceDeviceSessionOutputCreateV2>);
	GDExtension::register_interface_function("device_session_output_create_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_create_v2));
	static_assert(std::is_same_v<decltype(&device_session_output_acquire_v1), GDExtensionInterfaceDeviceSessionOutputAcquireV1>);
	GDExtension::register_interface_function("device_session_output_acquire_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_acquire_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_poll_v1), GDExtensionInterfaceDeviceSessionOutputPollV1>);
	GDExtension::register_interface_function("device_session_output_poll_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_poll_v1));
	GDExtension::register_interface_function("device_session_production_prepare_output_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_output_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_seal_v1), GDExtensionInterfaceDeviceSessionOutputSealV1>);
	GDExtension::register_interface_function("device_session_output_seal_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_seal_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_cancel_v1), GDExtensionInterfaceDeviceSessionOutputCancelV1>);
	GDExtension::register_interface_function("device_session_output_cancel_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_cancel_v1));
	static_assert(std::is_same_v<decltype(&device_session_output_withdraw_v1), GDExtensionInterfaceDeviceSessionOutputWithdrawV1>);
	GDExtension::register_interface_function("device_session_output_withdraw_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_output_withdraw_v1));

	GDExtension::register_interface_function("device_session_scene_operation_query_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_operation_query_v1));
	GDExtension::register_interface_function("device_session_scene_operation_bind_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_operation_bind_v1));
	GDExtension::register_interface_function("device_session_scene_phase_capability_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_phase_capability_v1));
	GDExtension::register_interface_function("device_session_scene_phase_relay_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_phase_relay_v1));
	GDExtension::register_interface_function("device_session_scene_phase_register_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_phase_register_v1));
	GDExtension::register_interface_function("device_session_scene_phase_unregister_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_scene_phase_unregister_v1));
	GDExtension::register_interface_function("device_session_production_reserve_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_reserve_v2));
	GDExtension::register_interface_function("device_session_production_reserve_readback_sources_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_reserve_readback_sources_v1));
	GDExtension::register_interface_function("device_session_production_prepare_dispatch_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_dispatch_v2));
	GDExtension::register_interface_function("device_session_production_prepare_copy_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_copy_v2));
	GDExtension::register_interface_function("device_session_production_prepare_upload_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_upload_v2));
	GDExtension::register_interface_function("device_session_production_prepare_readback_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_readback_v2));
	GDExtension::register_interface_function("device_session_production_append_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_append_v2));
	GDExtension::register_interface_function("device_session_production_close_segment_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_close_segment_v2));
	GDExtension::register_interface_function("device_session_production_record_consume_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_record_consume_v2));
	GDExtension::register_interface_function("device_session_production_observe_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_observe_v2));
	GDExtension::register_interface_function("device_session_production_facts_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_facts_v2));
	GDExtension::register_interface_function("device_session_production_release_v2", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_release_v2));
	GDExtension::register_interface_function("device_session_production_quote_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_quote_v1));
	GDExtension::register_interface_function("device_session_production_accept_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_accept_v1));
	GDExtension::register_interface_function("device_session_production_install_record_receiver_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_install_record_receiver_v1));
	GDExtension::register_interface_function("device_session_production_reserve_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_reserve_v1));
	GDExtension::register_interface_function("device_session_production_raster_prepare_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_raster_prepare_v1));
	GDExtension::register_interface_function("device_session_production_raster_prepare_report_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_raster_prepare_report_v1));
	GDExtension::register_interface_function("device_session_production_raster_begin_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_raster_begin_v1));
	GDExtension::register_interface_function("device_session_production_raster_end_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_raster_end_v1));
	GDExtension::register_interface_function("device_session_production_raster_reserve_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_raster_reserve_v1));
	GDExtension::register_interface_function("device_session_production_record_history_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_record_history_v1));
	GDExtension::register_interface_function("device_session_production_record_schedule_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_record_schedule_v1));
	GDExtension::register_interface_function("device_session_production_record_consume_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_record_consume_v1));
	GDExtension::register_interface_function("device_session_production_append_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_append_v1));
	GDExtension::register_interface_function("device_session_production_close_segment_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_close_segment_v1));
	GDExtension::register_interface_function("device_session_production_ack_segment_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_ack_segment_v1));
	GDExtension::register_interface_function("device_session_production_open_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_open_v1));
	GDExtension::register_interface_function("device_session_production_set_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_set_create_v1));
	GDExtension::register_interface_function("device_session_production_storage_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_storage_create_v1));
	GDExtension::register_interface_function("device_session_production_storage_retire_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_storage_retire_v1));
	GDExtension::register_interface_function("device_session_production_storage_observe_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_storage_observe_v1));
	GDExtension::register_interface_function("device_session_production_storage_release_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_storage_release_v1));
	GDExtension::register_interface_function("device_session_production_import_unit_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_import_unit_v1));
	GDExtension::register_interface_function("device_session_production_import_storage_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_import_storage_v1));
	GDExtension::register_interface_function("device_session_production_import_set_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_import_set_v1));
	GDExtension::register_interface_function("device_session_production_prepare_dispatch_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_dispatch_v1));
	GDExtension::register_interface_function("device_session_production_prepare_copy_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_copy_v1));
	GDExtension::register_interface_function("device_session_production_prepare_upload_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_upload_v1));
	GDExtension::register_interface_function("device_session_production_prepare_readback_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_prepare_readback_v1));
	GDExtension::register_interface_function("device_session_production_observe_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_observe_v1));
	GDExtension::register_interface_function("device_session_production_facts_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_facts_v1));
	GDExtension::register_interface_function("device_session_production_copy_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_copy_v1));
	GDExtension::register_interface_function("device_session_production_cancel_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_cancel_v1));
	GDExtension::register_interface_function("device_session_production_release_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_release_v1));
	GDExtension::register_interface_function("device_session_production_close_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_close_v1));
	GDExtension::register_interface_function("device_session_production_installation_observe_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&device_session_production_installation_observe_v1));
}
} // namespace GDExtensionDeviceExit
