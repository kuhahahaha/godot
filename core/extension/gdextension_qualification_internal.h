#pragma once

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/templates/local_vector.h"
#include "servers/rendering/rendering_device.h"

namespace GDExtensionDeviceExit {
// One original bridge child, shared by graph/frame, native source observations,
// the original async result and physical-return reverse indices. C facts are
// snapshots of these original sources, never a separate completion authority.
struct QualificationTicket {
 static constexpr uint32_t FACT_CAPACITY=4096;
 uint64_t serial=0,bridge_serial=0;
 uint32_t library=0;
 GDExtensionQualificationRecordV1 origin={};
 GDExtensionQualificationUploadPrepareV1 preparation={};
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
 Ref<RenderingDevice::BufferGetDataResult> result;
 LocalVector<RID> resource_pins;
 LocalVector<QualificationPhysicalReturn *> physical_uses;
 RDG::RecordHandle capture,body;
 uint32_t frame_refs=0,readback_copies_joined=0,accepted_rt_refs=0;
 bool prepared=false,recorded=false,recording=false,queued=false,acknowledged=false;
 std::atomic<bool> admission_closed{false};
 bool stop_requested=false,continuation_pending=false;
 uint32_t ordering_path_length=0;
 bool ordering_closed=false,append_failed=false,mapping_failed=false,observation_dirty=false;
 bool resources_returned=false;
 std::atomic<bool> physically_retired{false};
};

// Fixed transport storage owned by this same bridge. It carries API calls,
// never GPU completion truth, DSO pointers, Callables, or another ticket.
struct QualificationRtCell {
 GDExtensionQualificationRtConfigV1 config={};
 GDExtensionQualificationRtRequestV1 header={};
 Vector<uint8_t> request,payload,result;
 Vector<GDExtensionQualificationUploadSegmentV1> segments;
 uint64_t serial=0,retired_operation=0;
 QualificationTicket *ticket=nullptr;
 GDExtensionDeviceSessionResultV1 call_result={};
 uint32_t state=0,result_bytes=0,segment_count=0;
 bool ever_accepted=false,occupied=false,active=false,queued=false,executing=false,observed=false,admission_consumed=false;
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
 uint32_t children = 0;
 static constexpr uint32_t CHILD_CAPACITY = 128;
 struct Storage {
  uint64_t serial = 0, retire_serial = 0, dependency_serial = 0;
  GDExtensionStorageCreateRequestV1 origin = {};
  GDExtensionStorageBackingV1 backing = {};
  GDExtensionStorageRetireRequestV1 retirement = {};
  GDExtensionStorageRetireReportV1 report = {};
  QualificationPhysicalReturn physical;
  bool creating = true, retiring = false, queued = false, acknowledged = false;
 };
 struct NativeUnit {
  uint64_t serial = 0, retire_serial = 0, dependency_serial = 0;
  GDExtensionNativeUnitCreateRequestV1 origin = {};
  GDExtensionNativeUnitBackingV1 backing = {};
  GDExtensionNativeUnitRetireReportV1 report = {};
  QualificationPhysicalReturn shader, pipeline;
  bool creating = true, retiring = false, queued = false, acknowledged = false;
 };
 QualificationTicket *tickets[CHILD_CAPACITY] = {};
 Storage *storage[CHILD_CAPACITY] = {};
 NativeUnit *units[CHILD_CAPACITY] = {};
};
}
