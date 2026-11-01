#pragma once

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/templates/local_vector.h"
#include "servers/rendering/rendering_device.h"

namespace GDExtensionDeviceExit {
// Shared POD validation runs on the owner before admission and again on RT.
inline Error validate_qualification_record_v2(const GDExtensionQualificationRecordV2 &q) {
	if (q.abi.abi_version != 2 || q.abi.struct_size != sizeof(GDExtensionDeviceExitAbi) || q.abi.reserved || !q.record_generation || !q.request_nonce || !q.library_generation || !q.qualification_session_serial || !q.attempt || q.sample_count > 512 || q.range_end < q.range_begin || q.range_end - q.range_begin != q.sample_count || q.body_groups_x != MAX(1u, (q.sample_count + 63) / 64) || !q.readback_size || q.readback_size > 16384 || (q.readback_count != 1 && q.readback_count != 3)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!q.sample_count && (q.range_begin || q.range_end)) {
		return ERR_INVALID_PARAMETER;
	}
	if (q.capture_set_count < 1 || q.capture_set_count > 2 || q.body_set_count < 1 || q.body_set_count > 2 || bool(q.capture_set1) != (q.capture_set_count == 2) || bool(q.body_set1) != (q.body_set_count == 2) || (q.readback_count == 1 && (q.capture_set_count != 1 || q.body_set_count != 1))) {
		return ERR_INVALID_PARAMETER;
	}
	if (q.readback_count == 3 && q.readbacks[2].readback_size != sizeof(uint32_t)) {
		return ERR_INVALID_PARAMETER;
	}
	uint64_t total = 0;
	for (uint32_t i = 0; i < q.readback_count; ++i) {
		const auto &r = q.readbacks[i];
		if (!r.readback_buffer || !r.source_allocation_generation || !r.observed_backing_serial || !r.readback_size || r.readback_offset > UINT32_MAX || r.readback_size > UINT32_MAX - r.readback_offset || r.result_offset != total || r.readback_size > q.readback_size - total) {
			return ERR_INVALID_PARAMETER;
		}
		if (q.readback_count == 1 ? r.role != GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1 : i == 0 ? (r.role != GDEXTENSION_QUALIFICATION_READBACK_METHOD_RAW_RESULTS_V1 && r.role != GDEXTENSION_QUALIFICATION_READBACK_METHOD_DESTINATION_V1)
						: i == 1																				? r.role != GDEXTENSION_QUALIFICATION_READBACK_METHOD_FAULT_PLANE_V1
																												: r.role != GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1) {
			return ERR_INVALID_PARAMETER;
		}
		for (uint32_t j = 0; j < i; ++j) {
			const auto &other = q.readbacks[j];
			if (r.readback_buffer == other.readback_buffer && (r.observed_backing_serial != other.observed_backing_serial || r.source_allocation_generation != other.source_allocation_generation || (r.readback_offset < other.readback_offset + other.readback_size && other.readback_offset < r.readback_offset + r.readback_size))) {
				return ERR_INVALID_DATA;
			}
		}
		total += r.readback_size;
	}
	const GDExtensionQualificationReadbackRangeV1 empty = {};
	for (uint32_t i = q.readback_count; i < 3; ++i) {
		if (memcmp(&q.readbacks[i], &empty, sizeof(empty))) {
			return ERR_INVALID_PARAMETER;
		}
	}
	return total == q.readback_size ? OK : ERR_INVALID_PARAMETER;
}
inline GDExtensionQualificationRecordV1 qualification_record_v1(const GDExtensionQualificationRecordV2 &q) {
	GDExtensionQualificationRecordV1 old = {};
	memcpy(&old, &q, offsetof(GDExtensionQualificationRecordV1, readback_buffer));
	old.abi.abi_version = 1;
	const auto &r = q.readbacks[0];
	old.readback_buffer = r.readback_buffer;
	old.source_allocation_generation = r.source_allocation_generation;
	old.observed_backing_serial = r.observed_backing_serial;
	old.readback_offset = r.readback_offset;
	old.readback_size = r.readback_size;
	memcpy(old.capture_push, q.capture_push, sizeof(old.capture_push));
	memcpy(old.body_push, q.body_push, sizeof(old.body_push));
	return old;
}
struct QualificationResource;
// One original bridge child, shared by graph/frame, native source observations,
// the original async result and physical-return reverse indices. C facts are
// snapshots of these original sources, never a separate completion authority.
struct QualificationTicket {
 static constexpr uint32_t FACT_CAPACITY=4096;
 uint64_t serial=0,bridge_serial=0;
 uint32_t library=0;
 GDExtensionQualificationRecordV1 origin={};
 GDExtensionQualificationUploadPrepareV1 preparation={};
 GDExtensionQualificationRecordV2 range_origin = {};
 bool multi_readback = false;
 uint64_t additional_readback_serials[2] = {};
 Ref<RenderingDevice::BufferGetDataResult> additional_results[2];
 uint32_t additional_copies_joined[2] = {};
 GDExtensionQualificationReportV1 report={};
 LocalVector<GDExtensionQualificationFactV1> facts;
 struct GraphMember {RDG::RecordHandle handle;uint32_t fact=0,frame=0,path_length=0;uint64_t epoch=0;bool sealed=false,returned=false;};
 struct NativeMember {RDD::NativeCallFact source;uint32_t fact=0,frame=0,slot=UINT32_MAX;uint64_t epoch=0;bool retained=false,owner_released=false;};
 LocalVector<GraphMember> graphs;
 LocalVector<NativeMember> natives;
 struct Upload {
  GDExtensionQualificationUploadPlanV1 plan={};
  RenderingDevice::QualificationUploadProgress progress;
  GDExtensionQualificationUploadReportV1 report={};
  uint64_t last_request_revision=0;
  bool attempted=false,failed=false;
 };
 LocalVector<Upload> uploads;
 LocalVector<QualificationResource *> texture_uploads;
 Ref<RenderingDevice::BufferGetDataResult> result;
 LocalVector<RID> resource_pins;
 LocalVector<QualificationPhysicalReturn *> physical_uses;
 RDG::RecordHandle capture,body;
 uint32_t frame_refs=0,readback_copies_joined=0,accepted_rt_refs=0;
 bool prepared=false,recorded=false,recording=false,queued=false,acknowledged=false,queue_ref_credit=false;
 std::atomic<bool> admission_closed{false};
 bool stop_requested=false,continuation_pending=false;
 uint32_t ordering_path_length=0;
 bool ordering_closed=false,append_failed=false,mapping_failed=false,observation_dirty=false;
 bool resources_returned=false;
 uint32_t readback_count() const { return multi_readback ? range_origin.readback_count : 1; }
 Ref<RenderingDevice::BufferGetDataResult> &readback_result(uint32_t index) { return index ? additional_results[index - 1] : result; }
 const Ref<RenderingDevice::BufferGetDataResult> &readback_result(uint32_t index) const { return index ? additional_results[index - 1] : result; }
 uint32_t &readback_cursor(uint32_t index) { return index ? additional_copies_joined[index - 1] : readback_copies_joined; }
 GDExtensionQualificationReadbackRangeV1 readback_range(uint32_t index) const {
	 if (multi_readback) {
		 return range_origin.readbacks[index];
	 }
	 return { origin.readback_buffer, origin.source_allocation_generation, origin.observed_backing_serial, origin.readback_offset, origin.readback_size, 0, GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1 };
 }
 bool body_returned_successfully() const {
	 if (mapping_failed) {
		 return false;
	 }
	 for (const auto &graph : graphs) {
		 if (facts[graph.fact].semantic_kind == 2) {
			 const auto &body_fact = facts[graph.fact];
			 if (!graph.sealed || !graph.returned || body_fact.issue != 1 || body_fact.completion != 2 || body_fact.error) {
				 return false;
			 }
			 bool mapped = false;
			 // Later readback frames may acquire their own pending hidden sources.
			 // Only the original Body frame/slot and its hidden sources prove Body.
			 for (const auto &native : natives) {
				 if (native.frame == graph.frame && native.epoch == graph.epoch && (native.slot == UINT32_MAX || native.slot == body_fact.command_slot)) {
					 const auto &fact = facts[native.fact];
					 if (fact.issue != 1 || fact.completion != 2 || fact.error) {
						 return false;
					 }
					 if (native.slot == body_fact.command_slot) {
						 mapped = true;
					 }
				 }
			 }
			 return mapped;
		 }
	 }
	 return false;
 }

 std::atomic<bool> physically_retired{false};
};

// Common storage of the existing bridge children. No new registry or authority.
struct QualificationResource {
 uint64_t serial=0,rid=0,object_serial=0,retire_serial=0,dependency_serial=0;
 uint32_t kind=0,accepted_rt_refs=0;
 GDExtensionQualificationSourceV1 source={};
 GDExtensionQualificationResourceBackingV1 backing={};
 GDExtensionQualificationResourceRetireV1 retirement={};
 GDExtensionQualificationResourceRetireReportV1 report={};
 QualificationPhysicalReturn physical;
 Error maintenance_error=OK;
 bool creating=true,retiring=false,queued=false,acknowledged=false,cleanup_serial_reserved=false,queue_ref_credit=false;
 uint64_t upload_ticket=0,last_upload_revision=0;
 GDExtensionQualificationTextureUploadV1 upload_origin={};
 RenderingDevice::QualificationUploadProgress upload;
 GDExtensionQualificationUploadReportV1 upload_report={};
 bool upload_attempted=false,upload_failed=false;
};

// Fixed transport storage owned by this same bridge. It carries API calls,
// never GPU completion truth, DSO pointers, Callables, or another ticket.
struct QualificationRtCell {
 GDExtensionQualificationRtConfigV1 config={};
 GDExtensionQualificationRtRequestV1 header={};
 Vector<uint8_t> request,payload,result;
 Vector<GDExtensionQualificationUploadSegmentV1> segments;
 uint64_t serial=0,retired_operation=0,cleanup_child_serial=0;
 QualificationTicket *ticket=nullptr;
 LocalVector<QualificationResource *> resource_refs;
 GDExtensionDeviceSessionResultV1 call_result={};
 uint32_t state=0,result_bytes=0,segment_count=0;
 bool ever_accepted=false,occupied=false,active=false,queued=false,executing=false,observed=false,admission_consumed=false,slot_ref_credit=false,queue_ref_credit=false,queue_ref_reserved=false;
};

// Storage belongs to the original Registry. Public numeric observations never
// create one of these children. The parent CodeHolder cannot return while live.
struct Registry::QualificationBridge {
 uint64_t serial = 0;
 uint32_t library = 0;
 uint32_t device = 0;
 uint64_t epoch = 0;
 HolderRetirement *holder = nullptr;
 QualificationRtCell *rt=nullptr;
 bool closing = false;
 bool cleanup_refs_reserved = false, cleanup_slot_in_use = false, cleanup_queue_in_use = false;
 uint32_t children = 0;
 // Registry mutex is held by the caller. The credit remains a real retained
 // library reference while it is transferred to the original queue work.
 bool claim_cleanup_queue(bool &queued,bool &credit) {
  if(!cleanup_refs_reserved || cleanup_queue_in_use || queued || credit)return false;
  queued=true;credit=true;cleanup_queue_in_use=true;++children;return true;
 }
 void release_cleanup_queue(bool &queued,bool &credit) {
  CRASH_COND(!queued || !credit || !cleanup_queue_in_use || !children);
  queued=false;credit=false;cleanup_queue_in_use=false;--children;
 }
 static constexpr uint32_t CHILD_CAPACITY = 128;
 struct Storage {
  uint64_t serial = 0, retire_serial = 0, dependency_serial = 0;
  GDExtensionStorageCreateRequestV1 origin = {};
  GDExtensionStorageBackingV1 backing = {};
  GDExtensionStorageRetireRequestV1 retirement = {};
  GDExtensionStorageRetireReportV1 report = {};
  QualificationPhysicalReturn physical;
  Error maintenance_error = OK;
  bool creating = true, retiring = false, queued = false, acknowledged = false, cleanup_serial_reserved = false, queue_ref_credit = false;
 };
 // Original bridge child for a method texture. The producer and typed
 // retire transaction must bind this cell before it can be admitted by wire.
 struct Texture:QualificationResource {GDExtensionQualificationTextureCreateV1 origin={};};
 struct Sampler:QualificationResource {GDExtensionQualificationSamplerCreateV1 origin={};};
 struct UniformSet:QualificationResource {GDExtensionQualificationSetCreateV1 origin={};LocalVector<GDExtensionQualificationUniformBindingV1> bindings;};
 struct NativeUnit {
  uint64_t serial = 0, retire_serial = 0, dependency_serial = 0;
  GDExtensionNativeUnitCreateRequestV1 origin = {};
  GDExtensionNativeUnitBackingV1 backing = {};
  GDExtensionNativeUnitRetireReportV1 report = {};
  QualificationPhysicalReturn shader, pipeline;
  Error maintenance_error = OK;
  bool creating = true, retiring = false, queued = false, acknowledged = false, cleanup_serial_reserved = false, queue_ref_credit = false;
 };
 QualificationTicket *tickets[CHILD_CAPACITY] = {};
 Storage *storage[CHILD_CAPACITY] = {};
 Texture *textures[CHILD_CAPACITY] = {};
 Sampler *samplers[CHILD_CAPACITY] = {};
 UniformSet *sets[CHILD_CAPACITY] = {};
 NativeUnit *units[CHILD_CAPACITY] = {};
};
}
