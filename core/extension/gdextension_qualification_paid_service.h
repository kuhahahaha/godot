#pragma once

// Private additive DTO candidate. Typed procs are registered in the private
// qualification implementation; frozen V1 DTOs and kind values are unchanged.
#include "core/extension/gdextension_interface.gen.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(sizeof(GDExtensionQualificationServiceGrantV2)==48 && alignof(GDExtensionQualificationServiceGrantV2)==8);
static_assert(offsetof(GDExtensionQualificationServiceGrantV2,grant_serial)==16 && offsetof(GDExtensionQualificationServiceGrantV2,deadline_usec)==24 && offsetof(GDExtensionQualificationServiceGrantV2,controls)==32 && offsetof(GDExtensionQualificationServiceGrantV2,receipts)==36 && offsetof(GDExtensionQualificationServiceGrantV2,passes)==40 && offsetof(GDExtensionQualificationServiceGrantV2,reserved)==44);
static_assert(sizeof(GDExtensionSceneDepthServiceRequestV2)==72 && alignof(GDExtensionSceneDepthServiceRequestV2)==8);
static_assert(offsetof(GDExtensionSceneDepthServiceRequestV2,facet)==16 && offsetof(GDExtensionSceneDepthServiceRequestV2,operation_serial)==32 && offsetof(GDExtensionSceneDepthServiceRequestV2,allocation_generation)==40 && offsetof(GDExtensionSceneDepthServiceRequestV2,expected_frame_version)==48 && offsetof(GDExtensionSceneDepthServiceRequestV2,allocation_slot)==56 && offsetof(GDExtensionSceneDepthServiceRequestV2,part)==60 && offsetof(GDExtensionSceneDepthServiceRequestV2,mode)==64 && offsetof(GDExtensionSceneDepthServiceRequestV2,reserved)==68);
static_assert(sizeof(GDExtensionQualificationServiceUsageV2)==32 && alignof(GDExtensionQualificationServiceUsageV2)==4);
static_assert(sizeof(GDExtensionQualificationServiceObservationV2)==176 && alignof(GDExtensionQualificationServiceObservationV2)==8);
static_assert(offsetof(GDExtensionQualificationServiceObservationV2,mode)==16 && offsetof(GDExtensionQualificationServiceObservationV2,operation_serial)==24 && offsetof(GDExtensionQualificationServiceObservationV2,slot_generation)==32 && offsetof(GDExtensionQualificationServiceObservationV2,grant_serial)==40 && offsetof(GDExtensionQualificationServiceObservationV2,execution_serial)==48 && offsetof(GDExtensionQualificationServiceObservationV2,result_bytes)==56 && offsetof(GDExtensionQualificationServiceObservationV2,call_result)==64 && offsetof(GDExtensionQualificationServiceObservationV2,used)==88 && offsetof(GDExtensionQualificationServiceObservationV2,remaining)==120 && offsetof(GDExtensionQualificationServiceObservationV2,service_flags)==168);
static_assert(std::is_standard_layout_v<GDExtensionQualificationServiceObservationV2> && std::is_trivially_copyable_v<GDExtensionQualificationServiceObservationV2>);
static_assert(sizeof(GDExtensionQualificationBudgetV1)==32 && sizeof(GDExtensionStorageSceneDepthRetireRequestV1)==64 && sizeof(GDExtensionQualificationRtRequestV1)==56 && sizeof(GDExtensionQualificationRtObservationV1)==72);

namespace GDExtensionDeviceExit {
struct QualificationHeightPaidLedger {
 enum : uint32_t { ROW_ADMITTED=1, DEADLINE_EXHAUSTED=2, QUOTA_EXHAUSTED=4, CONTINUATION_PENDING=8 };
 GDExtensionQualificationServiceGrantV2 grant{};
 GDExtensionQualificationServiceUsageV2 used{};
 uint64_t execution_serial=0;
 int32_t last_error=0;
 uint32_t flags=0;
 bool accepted=false, executing=false, returned=false, observed=false;
 uint64_t receipts_used() const { return uint64_t(used.polls)+used.frees+used.issues+used.retains+used.releases+used.recycles; }
 bool valid_usage() const { return used.controls<=grant.controls && used.passes<=grant.passes && receipts_used()<=grant.receipts; }
 bool active(uint64_t now) const { return accepted && valid_usage() && used.controls<grant.controls && used.passes<grant.passes && receipts_used()<grant.receipts && grant.deadline_usec && now<grant.deadline_usec; }
 bool accept(const GDExtensionQualificationServiceGrantV2 &input,uint64_t now) {
  if(accepted || input.abi.abi_version!=2 || input.abi.struct_size!=sizeof(input) || input.abi.reserved || input.reserved || !input.grant_serial || !input.controls || !input.receipts || !input.passes || !input.deadline_usec || now>=input.deadline_usec)return false;
  grant=input;accepted=true;return true;
 }
 bool begin(uint64_t now) {
  if(executing || returned || !active(now) || execution_serial==UINT64_MAX)return false;
  ++used.controls;++execution_serial;executing=true;observed=false;flags&=CONTINUATION_PENDING;return true;
 }
 void publish(int32_t error,bool row_admitted,uint64_t now) {
  last_error=error;executing=false;returned=true;observed=false;
  flags=(flags&CONTINUATION_PENDING) | (row_admitted?ROW_ADMITTED:0u) | (now>=grant.deadline_usec?DEADLINE_EXHAUSTED:0u) | ((!valid_usage() || used.controls>=grant.controls || used.passes>=grant.passes || receipts_used()>=grant.receipts)?QUOTA_EXHAUSTED:0u);
 }
 // Only a new admitted same-selector snapshot can replace an earlier fact.
 // Retry, refused publication, expiry and unadmitted preflight are not evidence
 // that the original native continuation completed or disappeared.
 void project_continuation(bool row_admitted,bool pending) {
  if(row_admitted)flags=(flags&~CONTINUATION_PENDING) | (pending?CONTINUATION_PENDING:0u);
 }
 void observe() { if(returned && !executing)observed=true; }
 bool retry(int32_t busy_error,uint64_t now,uint64_t expected_grant,uint64_t expected_execution) {
  if(!returned || !observed || executing || last_error!=busy_error || expected_grant!=grant.grant_serial || expected_execution!=execution_serial || !active(now))return false;
  returned=false;observed=false;flags&=CONTINUATION_PENDING;return true;
 }
 GDExtensionQualificationServiceGrantV2 remaining() const {
  auto out=grant;
  out.controls=used.controls<=grant.controls?grant.controls-used.controls:0;
  out.receipts=receipts_used()<=grant.receipts?grant.receipts-uint32_t(receipts_used()):0;
  out.passes=used.passes<=grant.passes?grant.passes-used.passes:0;
  return out;
 }
};
} // namespace GDExtensionDeviceExit
